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

#include <Scripting/Core/SIRScriptArtifactCache.h>
#include <Core/Platform/SIRFileLock.h>
#include <Scripting/Core/SIRScriptSHA256.h>

#include <atomic>
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

static constexpr char s_szCacheMagic[8] = { 'S', 'I', 'R', 'C', 'A', 'S', '0', '1' };
static constexpr uint32_t s_nCacheVersion = 1;
static std::atomic<uint64_t> s_nCacheTempSerial( 1 );

static void scriptWriteLE32( uint8_t *pData, uint32_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		pData[iByte] = static_cast<uint8_t>(  nValue >> ( iByte * 8 )  );
	}
}

static void scriptWriteLE64( uint8_t *pData, uint64_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		pData[iByte] = static_cast<uint8_t>(  nValue >> ( iByte * 8 )  );
	}
}

static uint32_t scriptReadLE32( const uint8_t *pData ) noexcept
{
	uint32_t nValue = 0;
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		nValue |= static_cast<uint32_t>(pData[iByte]) << ( iByte * 8 );
	}
	return nValue;
}

static uint64_t scriptReadLE64( const uint8_t *pData ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nValue |= static_cast<uint64_t>(pData[iByte]) << ( iByte * 8 );
	}
	return nValue;
}

static bool scriptIsPowerOfTwo( uint32_t nValue ) noexcept
{
	return nValue != 0 && ( nValue & ( nValue - 1 ) ) == 0;
}

static bool scriptWriteDurable( FILE *pFile, const uint8_t *pBytes, size_t nBytes ) noexcept
{
	if ( fwrite( pBytes, 1, nBytes, pFile ) != nBytes || fflush( pFile ) != 0 ) {
		return false;
	}
#if defined( _WIN32 )
	return _commit( _fileno( pFile ) ) == 0;
#else
	return fsync( fileno( pFile ) ) == 0;
#endif
}

static bool scriptCommitFile( const char *szTemp, const char *szFinal, const char *szRoot ) noexcept
{
#if defined( _WIN32 )
	static_cast<void>(szRoot);
	return MoveFileExA( szTemp, szFinal, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) != 0;
#else
	if ( rename( szTemp, szFinal ) != 0 ) {
		return false;
	}
	const int nDir = open( szRoot, O_RDONLY | O_DIRECTORY );
	if ( nDir < 0 ) {
		return false;
	}
	const bool bDurable = fsync( nDir ) == 0;
	static_cast<void>(close( nDir ));
	return bDurable;
#endif
}

static FILE *scriptCreateExclusive( const char *szTemp ) noexcept
{
#if defined( _WIN32 )
	const int nFd = _open( szTemp, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE );
	if ( nFd < 0 ) {
		return nullptr;
	}
	FILE *pFile = _fdopen( nFd, "wb" );
	if ( pFile == nullptr ) {
		static_cast<void>(_close( nFd ));
	}
#else
	const int nFd = open( szTemp, O_WRONLY | O_CREAT | O_EXCL, 0600 );
	if ( nFd < 0 ) {
		return nullptr;
	}
	FILE *pFile = fdopen( nFd, "wb" );
	if ( pFile == nullptr ) {
		static_cast<void>(close( nFd ));
	}
#endif
	return pFile;
}

// Cache policy: the sidecar lock is stable across artifact replacement and
// remains on disk while independently owned cache processes may be active.
static bool scriptMakeCacheLockPath( const char *pszArtifact, char *pszOutput, size_t nCapacity ) noexcept
{
	if ( pszArtifact == nullptr || pszOutput == nullptr || nCapacity == 0 ) {
		return false;
	}
	const int nLength = snprintf( pszOutput, nCapacity, "%s.lock", pszArtifact );
	return nLength > 0 && static_cast<size_t>(nLength) < nCapacity;
}

SIRScriptArtifactCache::SIRScriptArtifactCache( void ) noexcept
    : m_szRoot{}, m_nMaximumArtifactBytes( 0 ), m_OwnerThread( SIRThread::currentThreadId() )
{
}

bool SIRScriptArtifactCache::isOwnerThread( void ) const noexcept
{
	return SIRThread::isCurrentThread( m_OwnerThread );
}

bool SIRScriptArtifactCache::validKey( const sirScriptCacheKey_t& key ) const noexcept
{
	uint8_t nCombined = 0;
	for ( uint32_t iByte = 0; iByte < sizeof( key.szDigest ); ++iByte ) {
		nCombined |= key.szDigest[iByte];
	}
	return nCombined != 0;
}

SIRScriptResult SIRScriptArtifactCache::configure( const char *szRoot, uint64_t nMaximumArtifactBytes ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( m_nMaximumArtifactBytes != 0 ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	if ( szRoot == nullptr || nMaximumArtifactBytes == 0 || nMaximumArtifactBytes > UINT32_MAX ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	const size_t nLength = strlen( szRoot );
	if ( nLength == 0 || nLength >= MAX_PATH_BYTES - 140 ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	memcpy( m_szRoot, szRoot, nLength + 1 );
	m_nMaximumArtifactBytes = nMaximumArtifactBytes;
	return SIRScriptResult::SIR_SCRIPT_OK;
}

SIRScriptResult SIRScriptArtifactCache::makeKey( const void *pCanonicalRecipe, uint64_t nRecipeBytes, sirScriptCacheKey_t *pKey ) noexcept
{
	if ( pCanonicalRecipe == nullptr || nRecipeBytes == 0 || pKey == nullptr || nRecipeBytes > UINT32_MAX ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	SIRScriptSHA256::digest( pCanonicalRecipe, nRecipeBytes, pKey->szDigest );
	return SIRScriptResult::SIR_SCRIPT_OK;
}

bool SIRScriptArtifactCache::formatPath( const sirScriptCacheKey_t& key, char szPath[MAX_PATH_BYTES] ) const noexcept
{
	if ( !validKey( key ) || m_nMaximumArtifactBytes == 0 ) {
		return false;
	}
	static constexpr char szHex[] = "0123456789abcdef";
	const size_t nRoot = strlen( m_szRoot );
	memcpy( szPath, m_szRoot, nRoot );
	size_t iCharacter = nRoot;
	if ( szPath[iCharacter - 1] != '/' && szPath[iCharacter - 1] != '\\' ) {
		szPath[iCharacter++] = '/';
	}
	for ( uint32_t iByte = 0; iByte < sizeof( key.szDigest ); ++iByte ) {
		szPath[iCharacter++] = szHex[key.szDigest[iByte] >> 4];
		szPath[iCharacter++] = szHex[key.szDigest[iByte] & 15];
	}
	static constexpr char szExtension[] = ".sirbc";
	memcpy( szPath + iCharacter, szExtension, sizeof( szExtension ) );
	return true;
}

bool SIRScriptArtifactCache::validDescriptor( const sirScriptCachedArtifact_t& descriptor ) const noexcept
{
	const sirScriptCodeArtifact_t& artifact = descriptor.artifact;
	return validKey( descriptor.key ) && artifact.nBackendId != 0 && artifact.nProgramId != 0 && artifact.nRevision != 0 &&
	       artifact.nFormatId != 0 && artifact.nSchemaFingerprint != 0 && artifact.nTargetId != 0 && artifact.nBuildFingerprint != 0 &&
	       descriptor.nBytes != 0 && descriptor.nBytes <= m_nMaximumArtifactBytes && descriptor.nAlignment <= 4096 &&
	       scriptIsPowerOfTwo( descriptor.nAlignment );
}

void SIRScriptArtifactCache::encodeHeader( const sirScriptCachedArtifact_t& descriptor, uint8_t szHeader[HEADER_BYTES] ) noexcept
{
	memset( szHeader, 0, HEADER_BYTES );
	memcpy( szHeader, s_szCacheMagic, sizeof( s_szCacheMagic ) );
	scriptWriteLE32( szHeader + 8, s_nCacheVersion );
	const sirScriptCodeArtifact_t& a = descriptor.artifact;
	const uint64_t szFields[7] = { a.nBackendId,         a.nProgramId, a.nRevision,        a.nFormatId,
	                               a.nSchemaFingerprint, a.nTargetId,  a.nBuildFingerprint };
	for ( uint32_t iField = 0; iField < 7; ++iField ) {
		scriptWriteLE64( szHeader + 16 + iField * 8, szFields[iField] );
	}
	memcpy( szHeader + 72, descriptor.key.szDigest, 32 );
	memcpy( szHeader + 104, descriptor.szPayloadDigest, 32 );
	scriptWriteLE64( szHeader + 136, descriptor.nBytes );
	scriptWriteLE32( szHeader + 144, descriptor.nAlignment );
}

bool SIRScriptArtifactCache::decodeHeader( const uint8_t szHeader[HEADER_BYTES], sirScriptCachedArtifact_t *pDescriptor ) noexcept
{
	if ( memcmp( szHeader, s_szCacheMagic, sizeof( s_szCacheMagic ) ) != 0 || scriptReadLE32( szHeader + 8 ) != s_nCacheVersion ||
	     scriptReadLE32( szHeader + 12 ) != 0 || scriptReadLE32( szHeader + 148 ) != 0 ) {
		return false;
	}
	sirScriptCachedArtifact_t& entry = *pDescriptor;
	uint64_t *pszFields[7] = { &entry.artifact.nBackendId,       &entry.artifact.nProgramId,         &entry.artifact.nRevision,
	                           &entry.artifact.nFormatId,        &entry.artifact.nSchemaFingerprint, &entry.artifact.nTargetId,
	                           &entry.artifact.nBuildFingerprint };
	for ( uint32_t iField = 0; iField < 7; ++iField ) {
		*pszFields[iField] = scriptReadLE64( szHeader + 16 + iField * 8 );
	}
	memcpy( entry.key.szDigest, szHeader + 72, 32 );
	memcpy( entry.szPayloadDigest, szHeader + 104, 32 );
	entry.nBytes = scriptReadLE64( szHeader + 136 );
	entry.nAlignment = scriptReadLE32( szHeader + 144 );
	return true;
}

bool SIRScriptArtifactCache::sameDescriptor( const sirScriptCachedArtifact_t& a, const sirScriptCachedArtifact_t& b ) noexcept
{
	return a.artifact.nBackendId == b.artifact.nBackendId && a.artifact.nProgramId == b.artifact.nProgramId &&
	       a.artifact.nRevision == b.artifact.nRevision && a.artifact.nFormatId == b.artifact.nFormatId &&
	       a.artifact.nSchemaFingerprint == b.artifact.nSchemaFingerprint && a.artifact.nTargetId == b.artifact.nTargetId &&
	       a.artifact.nBuildFingerprint == b.artifact.nBuildFingerprint && a.nBytes == b.nBytes && a.nAlignment == b.nAlignment &&
	       memcmp( a.key.szDigest, b.key.szDigest, 32 ) == 0 && memcmp( a.szPayloadDigest, b.szPayloadDigest, 32 ) == 0;
}

SIRScriptResult SIRScriptArtifactCache::store( const sirScriptCodeArtifact_t& artifact, sirScriptCacheKey_t key, const void *pBytes,
                                                 uint64_t nBytes, uint32_t nAlignment, sirScriptCachedArtifact_t *pDescriptor ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( pBytes == nullptr || pDescriptor == nullptr || m_nMaximumArtifactBytes == 0 ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	sirScriptCachedArtifact_t next {};
	next.artifact = artifact;
	next.key = key;
	next.nBytes = nBytes;
	next.nAlignment = nAlignment;
	if ( !validDescriptor( next ) ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	SIRScriptSHA256::digest( pBytes, nBytes, next.szPayloadDigest );
	char szFinal[MAX_PATH_BYTES];
	memset( szFinal, 0, sizeof( szFinal ) );
	if ( !formatPath( key, szFinal ) ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}

	// A build-recipe key cannot silently change its output or target metadata.
	// Keep the entire validation-and-replacement transaction under one OS lock.
	char szLockPath[MAX_PATH_BYTES];
	memset( szLockPath, 0, sizeof( szLockPath ) );
	if ( !scriptMakeCacheLockPath( szFinal, szLockPath, sizeof( szLockPath ) ) ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	SIRFileLock lock( szLockPath );
	if ( !lock.isLocked() ) {
		return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
	}
	FILE *pExisting = fopen( szFinal, "rb" );
	if ( pExisting != nullptr ) {
		uint8_t szHeader[HEADER_BYTES];
		memset( szHeader, 0, sizeof( szHeader ) );
		sirScriptCachedArtifact_t old {};
		if ( fread( szHeader, 1, HEADER_BYTES, pExisting ) == HEADER_BYTES && decodeHeader( szHeader, &old ) && validDescriptor( old ) ) {
			const bool bSame = sameDescriptor( old, next );
			bool bBytesMatch = bSame;
			uint8_t szRead[4096];
			memset( szRead, 0, sizeof( szRead ) );
			uint64_t nExamined = 0;
			while ( bBytesMatch && nExamined < nBytes ) {
				const size_t nNext = nBytes - nExamined < sizeof( szRead ) ? static_cast<size_t>( nBytes - nExamined ) : sizeof( szRead );
				const size_t nRead = fread( szRead, 1, nNext, pExisting );
				bBytesMatch = nRead == nNext && memcmp( szRead, static_cast<const uint8_t *>(pBytes) + nExamined, nNext ) == 0;
				nExamined += nNext;
			}
			if ( bBytesMatch ) {
				bBytesMatch = fgetc( pExisting ) == EOF && !ferror( pExisting );
			}
			static_cast<void>(fclose( pExisting ));
			if ( bBytesMatch ) {
				*pDescriptor = next;
				return SIRScriptResult::SIR_SCRIPT_DUPLICATE;
			}
			return SIRScriptResult::SIR_SCRIPT_TYPE_MISMATCH;
		}
		static_cast<void>(fclose( pExisting ));
		// An incomplete or malformed prior write can be replaced atomically.
	}

	char szTemp[MAX_PATH_BYTES];
	memset( szTemp, 0, sizeof( szTemp ) );
#if defined( _WIN32 )
	const uint64_t nPID = static_cast<uint64_t>(_getpid());
#else
	const uint64_t nPID = static_cast<uint64_t>(getpid());
#endif
	const uint64_t nSerial = s_nCacheTempSerial.fetch_add( 1, std::memory_order_relaxed );
	if ( snprintf( szTemp, sizeof( szTemp ), "%s.tmp.%llu.%llu", szFinal, ( unsigned long long )nPID, ( unsigned long long )nSerial ) >=
	     static_cast<int>(sizeof( szTemp ) )) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	FILE *pFile = scriptCreateExclusive( szTemp );
	if ( pFile == nullptr ) {
		return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
	}
	uint8_t szHeader[HEADER_BYTES];
	memset( szHeader, 0, sizeof( szHeader ) );
	encodeHeader( next, szHeader );
	const bool bHeader = fwrite( szHeader, 1, HEADER_BYTES, pFile ) == HEADER_BYTES;
	const bool bWritten = bHeader && scriptWriteDurable( pFile, static_cast<const uint8_t *>(pBytes), static_cast<size_t>(nBytes) );
	const bool bClosed = fclose( pFile ) == 0;
	if ( !bWritten || !bClosed ) {
		static_cast<void>(remove( szTemp ));
		return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( !scriptCommitFile( szTemp, szFinal, m_szRoot ) ) {
		static_cast<void>(remove( szTemp ));
		return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
	}
	*pDescriptor = next;
	return SIRScriptResult::SIR_SCRIPT_OK;
}

SIRScriptResult SIRScriptArtifactCache::fetch( const sirScriptCachedArtifact_t& descriptor, void *pOutput,
                                                 uint64_t nCapacity ) const noexcept
{
	if ( !isOwnerThread() ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( !validDescriptor( descriptor ) || pOutput == nullptr || nCapacity < descriptor.nBytes ||
	     ( reinterpret_cast<uintptr_t>(pOutput) & ( descriptor.nAlignment - 1 ) ) != 0 ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	char szPath[MAX_PATH_BYTES];
	memset( szPath, 0, sizeof( szPath ) );
	if ( !formatPath( descriptor.key, szPath ) ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	FILE *pFile = fopen( szPath, "rb" );
	if ( pFile == nullptr ) {
		return SIRScriptResult::SIR_SCRIPT_NOT_FOUND;
	}
	uint8_t szHeader[HEADER_BYTES];
	memset( szHeader, 0, sizeof( szHeader ) );
	sirScriptCachedArtifact_t found {};
	const bool bHeader = fread( szHeader, 1, HEADER_BYTES, pFile ) == HEADER_BYTES && decodeHeader( szHeader, &found );
	if ( !bHeader || !sameDescriptor( descriptor, found ) ) {
		static_cast<void>(fclose( pFile ));
		return SIRScriptResult::SIR_SCRIPT_TYPE_MISMATCH;
	}
	const bool bRead = fread( pOutput, 1, static_cast<size_t>(found.nBytes), pFile ) == found.nBytes;
	const int nExtra = fgetc( pFile );
	const bool bClean = !ferror( pFile ) && nExtra == EOF;
	static_cast<void>(fclose( pFile ));
	if ( !bRead || !bClean ) {
		return SIRScriptResult::SIR_SCRIPT_TYPE_MISMATCH;
	}
	uint8_t szDigest[32];
	memset( szDigest, 0, sizeof( szDigest ) );
	SIRScriptSHA256::digest( pOutput, found.nBytes, szDigest );
	return memcmp( szDigest, found.szPayloadDigest, sizeof( szDigest ) ) == 0 ? SIRScriptResult::SIR_SCRIPT_OK : SIRScriptResult::SIR_SCRIPT_TYPE_MISMATCH;
}

SIRScriptResult SIRScriptArtifactCache::erase( sirScriptCacheKey_t key ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	char szPath[MAX_PATH_BYTES];
	memset( szPath, 0, sizeof( szPath ) );
	if ( !formatPath( key, szPath ) ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	char szLockPath[MAX_PATH_BYTES];
	memset( szLockPath, 0, sizeof( szLockPath ) );
	if ( !scriptMakeCacheLockPath( szPath, szLockPath, sizeof( szLockPath ) ) ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	SIRFileLock lock( szLockPath );
	if ( !lock.isLocked() ) {
		return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
	}
	return remove( szPath ) == 0 ? SIRScriptResult::SIR_SCRIPT_OK : SIRScriptResult::SIR_SCRIPT_NOT_FOUND;
}
