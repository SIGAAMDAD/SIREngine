/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Scripting/Core/SIRScriptOptimizerInputStore.h>
#include <Scripting/Core/SIRScriptSHA256.h>
#include <Scripting/Core/SIRScriptOptimizationRecipe.h>

#include <Core/Platform/SIRAtomic.h>
#include <stdio.h>
#include <string.h>
#if defined( _WIN32 )
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static constexpr uint8_t s_szInputMagic[8] = { 'S', 'I', 'R', 'I', 'N', 'P', '0', '1' };
static SIRAtomic<uint64_t> s_nInputSerial( 1 );

static void inputWrite32( uint8_t *pData, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pData[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static void inputWrite64( uint8_t *pData, uint64_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 8; ++i ) {
		pData[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static uint32_t inputRead32( const uint8_t *pData ) noexcept
{
	uint32_t nValue = 0;
	for ( uint32_t i = 0; i < 4; ++i ) {
		nValue |= ( uint32_t )pData[i] << ( i * 8 );
	}
	return nValue;
}

static uint64_t inputRead64( const uint8_t *pData ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t i = 0; i < 8; ++i ) {
		nValue |= ( uint64_t )pData[i] << ( i * 8 );
	}
	return nValue;
}

static bool inputNonzeroKey( const sirScriptCacheKey_t& key ) noexcept
{
	uint8_t nMask = 0;
	for ( uint32_t i = 0; i < sizeof( key.szDigest ); ++i ) {
		nMask |= key.szDigest[i];
	}
	return nMask != 0;
}

static bool inputDurableWrite( FILE *pFile, const void *pBytes, size_t nBytes ) noexcept
{
	if ( fwrite( pBytes, 1, nBytes, pFile ) != nBytes ) {
		return false;
	}
	return true;
}

static bool inputSync( FILE *pFile ) noexcept
{
	if ( fflush( pFile ) != 0 ) {
		return false;
	}
#if defined( _WIN32 )
	return _commit( _fileno( pFile ) ) == 0;
#else
	return fsync( fileno( pFile ) ) == 0;
#endif
}

static FILE *inputCreateExclusive( const char *pszPath ) noexcept
{
#if defined( _WIN32 )
	const int nFd = _open( pszPath, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE );
	if ( nFd < 0 ) {
		return nullptr;
	}
	FILE *pFile = _fdopen( nFd, "wb" );
	if ( pFile == nullptr ) {
		( void )_close( nFd );
	}
#else
	const int nFd = open( pszPath, O_WRONLY | O_CREAT | O_EXCL, 0600 );
	if ( nFd < 0 ) {
		return nullptr;
	}
	FILE *pFile = fdopen( nFd, "wb" );
	if ( pFile == nullptr ) {
		( void )close( nFd );
	}
#endif
	return pFile;
}

static bool inputCommit( const char *pszTemporary, const char *pszFinal, const char *pszRoot ) noexcept
{
#if defined( _WIN32 )
	( void )pszRoot;
	return MoveFileExA( pszTemporary, pszFinal, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) != 0;
#else
	if ( rename( pszTemporary, pszFinal ) != 0 ) {
		return false;
	}
	const int nFd = open( pszRoot, O_RDONLY | O_DIRECTORY );
	if ( nFd < 0 ) {
		return false;
	}
	const bool bSynced = fsync( nFd ) == 0;
	( void )close( nFd );
	return bSynced;
#endif
}

SIRScriptOptimizerInputStore::SIRScriptOptimizerInputStore( void ) noexcept
    : m_szRoot{}, m_nMaximumInputBytes( 0 ), m_nMaximumRecipeBytes( 0 ), m_OwnerThread( std::this_thread::get_id() )
{
}

bool SIRScriptOptimizerInputStore::isOwnerThread( void ) const noexcept
{
	return std::this_thread::get_id() == m_OwnerThread;
}

sirScriptResult_t SIRScriptOptimizerInputStore::configure( const char *pszRoot, uint64_t nMaximumInputBytes,
                                                           uint64_t nMaximumRecipeBytes ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_nMaximumInputBytes != 0 ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pszRoot == nullptr || nMaximumInputBytes == 0 || nMaximumInputBytes > UINT32_MAX || nMaximumRecipeBytes == 0 ||
	     nMaximumRecipeBytes > 65536 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	const size_t nLength = strlen( pszRoot );
	if ( nLength == 0 || nLength >= MAX_PATH_BYTES - 150 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	memcpy( m_szRoot, pszRoot, nLength + 1 );
	m_nMaximumInputBytes = nMaximumInputBytes;
	m_nMaximumRecipeBytes = nMaximumRecipeBytes;
	return SIR_SCRIPT_OK;
}

bool SIRScriptOptimizerInputStore::formatPath( const sirScriptCacheKey_t& key, char szPath[MAX_PATH_BYTES] ) const noexcept
{
	if ( m_nMaximumInputBytes == 0 || !inputNonzeroKey( key ) ) {
		return false;
	}
	static constexpr char szHex[] = "0123456789abcdef";
	const size_t nRoot = strlen( m_szRoot );
	memcpy( szPath, m_szRoot, nRoot );
	size_t nOffset = nRoot;
	if ( szPath[nOffset - 1] != '/' && szPath[nOffset - 1] != '\\' ) {
		szPath[nOffset++] = '/';
	}
	for ( uint32_t i = 0; i < sizeof( key.szDigest ); ++i ) {
		szPath[nOffset++] = szHex[key.szDigest[i] >> 4];
		szPath[nOffset++] = szHex[key.szDigest[i] & 15];
	}
	static constexpr char szSuffix[] = ".sirinput";
	memcpy( szPath + nOffset, szSuffix, sizeof( szSuffix ) );
	return true;
}

void SIRScriptOptimizerInputStore::encodeHeader( const sirScriptOptimizationRequest_t& request, uint64_t nRecipeBytes,
                                                 const uint8_t szInputDigest[32], uint8_t szHeader[HEADER_BYTES] ) noexcept
{
	memset( szHeader, 0, HEADER_BYTES );
	memcpy( szHeader, s_szInputMagic, 8 );
	inputWrite32( szHeader + 8, 1 );
	inputWrite32( szHeader + 12, HEADER_BYTES );
	const sirScriptCodeArtifact_t& artifact = request.artifact;
	const uint64_t szFields[7] = { artifact.nBackendId,         artifact.nProgramId, artifact.nRevision,        artifact.nFormatId,
	                               artifact.nSchemaFingerprint, artifact.nTargetId,  artifact.nBuildFingerprint };
	for ( uint32_t i = 0; i < 7; ++i ) {
		inputWrite64( szHeader + 16 + 8 * i, szFields[i] );
	}
	memcpy( szHeader + 72, request.key.szDigest, 32 );
	inputWrite32( szHeader + 104, ( uint32_t )request.eTier );
	inputWrite32( szHeader + 108, request.nFlags );
	inputWrite64( szHeader + 112, request.nInputBytes );
	inputWrite64( szHeader + 120, nRecipeBytes );
	memcpy( szHeader + 128, request.key.szDigest, 32 );
	memcpy( szHeader + 160, szInputDigest, 32 );
	SIRScriptSHA256::digest( szHeader, 192, szHeader + 192 );
}

bool SIRScriptOptimizerInputStore::validHeader( const sirScriptOptimizationRequest_t& request, const uint8_t szHeader[HEADER_BYTES],
                                                uint64_t nMaximumRecipeBytes, uint64_t *pRecipeBytes ) noexcept
{
	if ( memcmp( szHeader, s_szInputMagic, 8 ) != 0 || inputRead32( szHeader + 8 ) != 1 || inputRead32( szHeader + 12 ) != HEADER_BYTES ) {
		return false;
	}
	uint8_t szDigest[32] = {};
	SIRScriptSHA256::digest( szHeader, 192, szDigest );
	if ( memcmp( szDigest, szHeader + 192, 32 ) != 0 ) {
		return false;
	}
	const sirScriptCodeArtifact_t& artifact = request.artifact;
	const uint64_t szFields[7] = { artifact.nBackendId,         artifact.nProgramId, artifact.nRevision,        artifact.nFormatId,
	                               artifact.nSchemaFingerprint, artifact.nTargetId,  artifact.nBuildFingerprint };
	for ( uint32_t i = 0; i < 7; ++i ) {
		if ( inputRead64( szHeader + 16 + 8 * i ) != szFields[i] ) {
			return false;
		}
	}
	const uint64_t nRecipeBytes = inputRead64( szHeader + 120 );
	if ( nRecipeBytes == 0 || nRecipeBytes > nMaximumRecipeBytes || memcmp( szHeader + 72, request.key.szDigest, 32 ) != 0 ||
	     memcmp( szHeader + 128, request.key.szDigest, 32 ) != 0 || inputRead32( szHeader + 104 ) != ( uint32_t )request.eTier ||
	     inputRead32( szHeader + 108 ) != request.nFlags || inputRead64( szHeader + 112 ) != request.nInputBytes ) {
		return false;
	}
	*pRecipeBytes = nRecipeBytes;
	return true;
}

sirScriptResult_t SIRScriptOptimizerInputStore::readHeader( const sirScriptOptimizationRequest_t& request, uint8_t szHeader[HEADER_BYTES],
                                                            uint64_t *pRecipeBytes ) const noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	if ( m_nMaximumInputBytes == 0 || request.nInputBytes > m_nMaximumInputBytes ||
	     SIRScriptOptimizationProtocol::encodeRequest( request, szWire ) != SIR_SCRIPT_OK ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	char szPath[MAX_PATH_BYTES] = {};
	if ( !formatPath( request.key, szPath ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	FILE *pFile = fopen( szPath, "rb" );
	if ( pFile == nullptr ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	const bool bRead = fread( szHeader, 1, HEADER_BYTES, pFile ) == HEADER_BYTES;
	( void )fclose( pFile );
	return bRead && validHeader( request, szHeader, m_nMaximumRecipeBytes, pRecipeBytes ) ? SIR_SCRIPT_OK : SIR_SCRIPT_TYPE_MISMATCH;
}

sirScriptResult_t SIRScriptOptimizerInputStore::inspect( const sirScriptOptimizationRequest_t& request,
                                                         uint64_t *pRecipeBytes ) const noexcept
{
	if ( pRecipeBytes == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	uint8_t szHeader[HEADER_BYTES] = {};
	return readHeader( request, szHeader, pRecipeBytes );
}

sirScriptResult_t SIRScriptOptimizerInputStore::fetch( const sirScriptOptimizationRequest_t& request,
                                                       sirScriptOptimizerInputBuffers_t *pBuffers ) const noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pBuffers == nullptr || pBuffers->pRecipe == nullptr || pBuffers->pInput == nullptr ||
	     pBuffers->nInputCapacity < request.nInputBytes ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	uint8_t szHeader[HEADER_BYTES] = {};
	uint64_t nRecipeBytes = 0;
	const sirScriptResult_t eHeader = readHeader( request, szHeader, &nRecipeBytes );
	if ( eHeader != SIR_SCRIPT_OK ) {
		return eHeader;
	}
	if ( nRecipeBytes > pBuffers->nRecipeCapacity ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	char szPath[MAX_PATH_BYTES] = {};
	if ( !formatPath( request.key, szPath ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	FILE *pFile = fopen( szPath, "rb" );
	if ( pFile == nullptr ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	uint8_t szAgain[HEADER_BYTES] = {};
	uint64_t nAgain = 0;
	const bool bHeader = fread( szAgain, 1, HEADER_BYTES, pFile ) == HEADER_BYTES &&
	                     validHeader( request, szAgain, m_nMaximumRecipeBytes, &nAgain ) && nAgain == nRecipeBytes &&
	                     memcmp( szHeader, szAgain, HEADER_BYTES ) == 0;
	const bool bRead = bHeader && fread( pBuffers->pRecipe, 1, ( size_t )nRecipeBytes, pFile ) == nRecipeBytes &&
	                   fread( pBuffers->pInput, 1, ( size_t )request.nInputBytes, pFile ) == request.nInputBytes;
	const bool bEOF = bRead && fgetc( pFile ) == EOF && !ferror( pFile );
	( void )fclose( pFile );
	if ( !bEOF ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	uint8_t szRecipeDigest[32] = {};
	uint8_t szInputDigest[32] = {};
	SIRScriptSHA256::digest( pBuffers->pRecipe, nRecipeBytes, szRecipeDigest );
	SIRScriptSHA256::digest( pBuffers->pInput, request.nInputBytes, szInputDigest );
	if ( memcmp( szRecipeDigest, szHeader + 128, 32 ) != 0 || memcmp( szInputDigest, szHeader + 160, 32 ) != 0 ||
	     SIRScriptOptimizationRecipe::verify( request, pBuffers->pInput, pBuffers->pRecipe, nRecipeBytes ) != SIR_SCRIPT_OK ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	pBuffers->nRecipeBytes = nRecipeBytes;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizerInputStore::stage( const sirScriptOptimizationRequest_t& request, const void *pRecipe,
                                                       uint64_t nRecipeBytes, const void *pInput ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	if ( m_nMaximumInputBytes == 0 || pRecipe == nullptr || pInput == nullptr || nRecipeBytes == 0 ||
	     nRecipeBytes > m_nMaximumRecipeBytes || request.nInputBytes > m_nMaximumInputBytes ||
	     SIRScriptOptimizationProtocol::encodeRequest( request, szWire ) != SIR_SCRIPT_OK ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	sirScriptCacheKey_t digest = {};
	if ( SIRScriptArtifactCache::makeKey( pRecipe, nRecipeBytes, &digest ) != SIR_SCRIPT_OK ||
	     memcmp( digest.szDigest, request.key.szDigest, 32 ) != 0 ||
	     SIRScriptOptimizationRecipe::verify( request, pInput, pRecipe, nRecipeBytes ) != SIR_SCRIPT_OK ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	uint8_t szInputDigest[32] = {};
	SIRScriptSHA256::digest( pInput, request.nInputBytes, szInputDigest );
	uint8_t szHeader[HEADER_BYTES] = {};
	encodeHeader( request, nRecipeBytes, szInputDigest, szHeader );
	char szFinal[MAX_PATH_BYTES] = {};
	if ( !formatPath( request.key, szFinal ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	FILE *pExisting = fopen( szFinal, "rb" );
	if ( pExisting != nullptr ) {
		uint8_t szOld[HEADER_BYTES] = {};
		const bool bOldHeader = fread( szOld, 1, HEADER_BYTES, pExisting ) == HEADER_BYTES;
		bool bEqual = bOldHeader && memcmp( szOld, szHeader, HEADER_BYTES ) == 0;
		uint8_t szScratch[4096] = {};
		const uint8_t *pRecipeBytes = ( const uint8_t * )pRecipe;
		const uint8_t *pInputBytes = ( const uint8_t * )pInput;
		for ( uint64_t nOffset = 0; bEqual && nOffset < nRecipeBytes; ) {
			const size_t nCount = ( size_t )( nRecipeBytes - nOffset < sizeof( szScratch ) ? nRecipeBytes - nOffset : sizeof( szScratch ) );
			bEqual = fread( szScratch, 1, nCount, pExisting ) == nCount && memcmp( szScratch, pRecipeBytes + nOffset, nCount ) == 0;
			nOffset += nCount;
		}
		for ( uint64_t nOffset = 0; bEqual && nOffset < request.nInputBytes; ) {
			const size_t nCount =
			    ( size_t )( request.nInputBytes - nOffset < sizeof( szScratch ) ? request.nInputBytes - nOffset : sizeof( szScratch ) );
			bEqual = fread( szScratch, 1, nCount, pExisting ) == nCount && memcmp( szScratch, pInputBytes + nOffset, nCount ) == 0;
			nOffset += nCount;
		}
		if ( bEqual ) {
			bEqual = fgetc( pExisting ) == EOF && !ferror( pExisting );
		}
		( void )fclose( pExisting );
		if ( bEqual ) {
			return SIR_SCRIPT_DUPLICATE;
		}
		if ( bOldHeader && memcmp( szOld, szHeader, HEADER_BYTES ) == 0 ) {
			// A well-formed descriptor with damaged contents is recoverable.
		} else if ( bOldHeader ) {
			uint64_t nRecipeOld = 0;
			if ( validHeader( request, szOld, m_nMaximumRecipeBytes, &nRecipeOld ) ) {
				return SIR_SCRIPT_TYPE_MISMATCH;
			}
			// Different artifact identity under the same canonical recipe key is a conflict.
			if ( memcmp( szOld, s_szInputMagic, 8 ) == 0 && memcmp( szOld + 72, request.key.szDigest, 32 ) == 0 ) {
				return SIR_SCRIPT_TYPE_MISMATCH;
			}
		}
	}
	char szTemporary[MAX_PATH_BYTES] = {};
#if defined( _WIN32 )
	const uint64_t nPID = ( uint64_t )_getpid();
#else
	const uint64_t nPID = ( uint64_t )getpid();
#endif
	const uint64_t nSerial = s_nInputSerial.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
	if ( snprintf( szTemporary, sizeof( szTemporary ), "%s.tmp.%llu.%llu", szFinal, ( unsigned long long )nPID,
	               ( unsigned long long )nSerial ) >= ( int )sizeof( szTemporary ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	FILE *pFile = inputCreateExclusive( szTemporary );
	if ( pFile == nullptr ) {
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	const bool bWritten = inputDurableWrite( pFile, szHeader, HEADER_BYTES ) &&
	                      inputDurableWrite( pFile, pRecipe, ( size_t )nRecipeBytes ) &&
	                      inputDurableWrite( pFile, pInput, ( size_t )request.nInputBytes ) && inputSync( pFile );
	const bool bClosed = fclose( pFile ) == 0;
	if ( !bWritten || !bClosed || !inputCommit( szTemporary, szFinal, m_szRoot ) ) {
		( void )remove( szTemporary );
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizerInputStore::erase( sirScriptCacheKey_t key ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	char szPath[MAX_PATH_BYTES] = {};
	if ( !formatPath( key, szPath ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	return remove( szPath ) == 0 ? SIR_SCRIPT_OK : SIR_SCRIPT_NOT_FOUND;
}
