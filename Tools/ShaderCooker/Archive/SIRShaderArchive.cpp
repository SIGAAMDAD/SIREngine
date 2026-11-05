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

#include <Tools/ShaderCooker/Archive/SIRShaderArchive.h>

#include <string.h>

static uint16_t sirArchiveRead16( const uint8_t *pBytes ) noexcept
{
	return ( uint16_t )( pBytes[0] | ( ( uint16_t )pBytes[1] << 8 ) );
}

static uint32_t sirArchiveRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

static uint64_t sirArchiveRead64( const uint8_t *pBytes ) noexcept
{
	return ( uint64_t )sirArchiveRead32( pBytes ) | ( ( uint64_t )sirArchiveRead32( pBytes + 4 ) << 32 );
}

static void sirArchiveWrite16( uint8_t *pBytes, uint16_t nValue ) noexcept
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8 );
}

static void sirArchiveWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( iByte * 8 ) );
	}
}

static void sirArchiveWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	sirArchiveWrite32( pBytes, ( uint32_t )nValue );
	sirArchiveWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

static uint64_t sirArchiveHashBytes( const uint8_t *pBytes, uint64_t nCount ) noexcept
{
	uint64_t nHash = 14695981039346656037ULL;
	for ( uint64_t iByte = 0; iByte < nCount; ++iByte ) {
		nHash ^= pBytes[iByte];
		nHash *= 1099511628211ULL;
	}
	return nHash;
}

static uint64_t sirArchiveManifestHash( const uint8_t *pBytes, uint64_t nCount ) noexcept
{
	uint64_t nHash = 14695981039346656037ULL;
	for ( uint64_t iByte = 0; iByte < nCount; ++iByte ) {
		// Manifest checksum bytes 44..51 are treated as zero.
		const uint8_t nByte = iByte >= 44 && iByte < 52 ? 0 : pBytes[iByte];
		nHash ^= nByte;
		nHash *= 1099511628211ULL;
	}
	return nHash;
}

static bool sirArchiveStageValid( uint32_t nMask ) noexcept
{
	const uint32_t nAllowed = ( SIR_GRAPHICS_STAGE_TASK << 1 ) - 1U;
	return nMask != 0 && ( nMask & ( nMask - 1U ) ) == 0 && ( nMask & ~nAllowed ) == 0;
}

static bool sirArchiveFormatValid( uint32_t nFormat ) noexcept
{
	return nFormat >= SIR_GRAPHICS_SHADER_DXIL && nFormat <= SIR_GRAPHICS_SHADER_DXBC;
}

static bool sirArchiveHeaderValid( const uint8_t *pBytes, uint64_t nCount ) noexcept
{
	if ( pBytes == nullptr || nCount < SIR_SHADER_ARCHIVE_HEADER_BYTES || nCount > SIR_SHADER_ARCHIVE_MAX_BYTES ) {
		return false;
	}
	const uint32_t nEntries = sirArchiveRead32( pBytes + 12 );
	const uint32_t nUnique = sirArchiveRead32( pBytes + 16 );
	if ( sirArchiveRead32( pBytes ) != SIR_SHADER_ARCHIVE_MAGIC || sirArchiveRead16( pBytes + 4 ) != SIR_SHADER_ARCHIVE_SCHEMA ||
	     sirArchiveRead16( pBytes + 6 ) != SIR_SHADER_ARCHIVE_HEADER_BYTES || sirArchiveRead32( pBytes + 8 ) != nCount ) {
		return false;
	}
	if ( nEntries == 0 || nEntries > SIR_SHADER_ARCHIVE_MAX_ENTRIES || nUnique == 0 || nUnique > nEntries ||
	     sirArchiveRead64( pBytes + 36 ) == 0 || sirArchiveRead32( pBytes + 52 ) != SIR_SHADER_ARCHIVE_TARGET_ABI_V1 ||
	     sirArchiveRead32( pBytes + 56 ) != 0 || sirArchiveRead32( pBytes + 60 ) != 0 ) {
		return false;
	}
	const uint64_t nRefEnd = ( uint64_t )SIR_SHADER_ARCHIVE_HEADER_BYTES + ( uint64_t )nEntries * SIR_SHADER_ARCHIVE_ENTRY_BYTES;
	const uint64_t nUniqueEnd = nRefEnd + ( uint64_t )nUnique * SIR_SHADER_ARCHIVE_UNIQUE_BYTES;
	const uint32_t nPayloadOffset = sirArchiveRead32( pBytes + 28 );
	const uint32_t nPayloadBytes = sirArchiveRead32( pBytes + 32 );
	return sirArchiveRead32( pBytes + 20 ) == SIR_SHADER_ARCHIVE_HEADER_BYTES && sirArchiveRead32( pBytes + 24 ) == nRefEnd &&
	       nPayloadOffset == nUniqueEnd && ( uint64_t )nPayloadOffset + nPayloadBytes == nCount;
}

sirShaderCookResult_t sirShaderArchiveBuild( const sirShaderArchiveBuildDesc_t& desc, void *pDestination, uint64_t nCapacity,
                                             sirShaderArchiveBuildResult_t *pOut ) noexcept
{
	if ( pOut == nullptr || desc.nArchiveId == 0 || desc.nTargetAbiVersion != SIR_SHADER_ARCHIVE_TARGET_ABI_V1 ||
	     desc.pEntries == nullptr || desc.nEntryCount == 0 || desc.nEntryCount > SIR_SHADER_ARCHIVE_MAX_ENTRIES ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const uint32_t nCount = desc.nEntryCount;
	uint32_t szRepresentatives[SIR_SHADER_ARCHIVE_MAX_ENTRIES] = {};
	uint32_t szUniqueIndices[SIR_SHADER_ARCHIVE_MAX_ENTRIES] = {};
	uint64_t szByteHashes[SIR_SHADER_ARCHIVE_MAX_ENTRIES] = {};
	uint64_t nPayloadBytes = 0;
	uint32_t nUnique = 0;
	for ( uint32_t iStage = 0; iStage < nCount; ++iStage ) {
		const sirShaderArchiveStageInput_t& stage = desc.pEntries[iStage];
		if ( !sirArchiveFormatValid( stage.eFormat ) || !sirArchiveStageValid( stage.nStageMask ) || stage.nEntryPointHash == 0 ||
		     stage.pBytes == nullptr || stage.nByteCount == 0 ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		const uint64_t nHash = sirArchiveHashBytes( stage.pBytes, stage.nByteCount );
		uint32_t iUnique = 0;
		for ( ; iUnique < nUnique; ++iUnique ) {
			const sirShaderArchiveStageInput_t& previous = desc.pEntries[szRepresentatives[iUnique]];
			if ( stage.eFormat == previous.eFormat && stage.nByteCount == previous.nByteCount && nHash == szByteHashes[iUnique] &&
			     memcmp( stage.pBytes, previous.pBytes, stage.nByteCount ) == 0 ) {
				break;
			}
		}
		if ( iUnique == nUnique ) {
			szRepresentatives[nUnique] = iStage;
			szByteHashes[nUnique] = nHash;
			nPayloadBytes += stage.nByteCount;
			++nUnique;
		}
		szUniqueIndices[iStage] = iUnique;
	}
	const uint64_t nRequired = SIR_SHADER_ARCHIVE_HEADER_BYTES + ( uint64_t )nCount * SIR_SHADER_ARCHIVE_ENTRY_BYTES +
	                           ( uint64_t )nUnique * SIR_SHADER_ARCHIVE_UNIQUE_BYTES + nPayloadBytes;
	pOut->nRequiredBytes = nRequired;
	pOut->nUniqueCount = nUnique;
	pOut->nManifestHash = 0;
	if ( nRequired > SIR_SHADER_ARCHIVE_MAX_BYTES ) {
		return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
	}
	if ( pDestination == nullptr || nCapacity < nRequired ) {
		return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
	}
	uint8_t *pData = ( uint8_t * )pDestination;
	memset( pData, 0, ( size_t )nRequired );
	const uint32_t nUniqueOffset = SIR_SHADER_ARCHIVE_HEADER_BYTES + nCount * SIR_SHADER_ARCHIVE_ENTRY_BYTES;
	const uint32_t nPayloadOffset = nUniqueOffset + nUnique * SIR_SHADER_ARCHIVE_UNIQUE_BYTES;
	sirArchiveWrite32( pData, SIR_SHADER_ARCHIVE_MAGIC );
	sirArchiveWrite16( pData + 4, ( uint16_t )SIR_SHADER_ARCHIVE_SCHEMA );
	sirArchiveWrite16( pData + 6, ( uint16_t )SIR_SHADER_ARCHIVE_HEADER_BYTES );
	sirArchiveWrite32( pData + 8, ( uint32_t )nRequired );
	sirArchiveWrite32( pData + 12, nCount );
	sirArchiveWrite32( pData + 16, nUnique );
	sirArchiveWrite32( pData + 20, SIR_SHADER_ARCHIVE_HEADER_BYTES );
	sirArchiveWrite32( pData + 24, nUniqueOffset );
	sirArchiveWrite32( pData + 28, nPayloadOffset );
	sirArchiveWrite32( pData + 32, ( uint32_t )nPayloadBytes );
	sirArchiveWrite64( pData + 36, desc.nArchiveId );
	sirArchiveWrite32( pData + 52, desc.nTargetAbiVersion );
	for ( uint32_t iEntry = 0; iEntry < nCount; ++iEntry ) {
		const sirShaderArchiveStageInput_t& stage = desc.pEntries[iEntry];
		const uint32_t iUnique = szUniqueIndices[iEntry];
		uint8_t *pEntry = pData + SIR_SHADER_ARCHIVE_HEADER_BYTES + iEntry * SIR_SHADER_ARCHIVE_ENTRY_BYTES;
		sirArchiveWrite32( pEntry, stage.eFormat );
		sirArchiveWrite32( pEntry + 4, stage.nStageMask );
		sirArchiveWrite64( pEntry + 8, stage.nEntryPointHash );
		sirArchiveWrite32( pEntry + 16, iUnique );
		sirArchiveWrite64( pEntry + 24, szByteHashes[iUnique] );
	}
	uint32_t nCursor = nPayloadOffset;
	for ( uint32_t iUnique = 0; iUnique < nUnique; ++iUnique ) {
		const sirShaderArchiveStageInput_t& stage = desc.pEntries[szRepresentatives[iUnique]];
		uint8_t *pRecord = pData + nUniqueOffset + iUnique * SIR_SHADER_ARCHIVE_UNIQUE_BYTES;
		sirArchiveWrite32( pRecord, stage.eFormat );
		sirArchiveWrite32( pRecord + 4, nCursor );
		sirArchiveWrite32( pRecord + 8, stage.nByteCount );
		sirArchiveWrite64( pRecord + 16, szByteHashes[iUnique] );
		memcpy( pData + nCursor, stage.pBytes, stage.nByteCount );
		nCursor += stage.nByteCount;
	}
	const uint64_t nManifest = sirArchiveManifestHash( pData, nRequired );
	sirArchiveWrite64( pData + 44, nManifest );
	pOut->nManifestHash = nManifest;
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirShaderArchiveValidate( const void *pArchive, uint64_t nBytes ) noexcept
{
	const uint8_t *pData = ( const uint8_t * )pArchive;
	if ( !sirArchiveHeaderValid( pData, nBytes ) || sirArchiveRead64( pData + 44 ) != sirArchiveManifestHash( pData, nBytes ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const uint32_t nEntries = sirArchiveRead32( pData + 12 );
	const uint32_t nUnique = sirArchiveRead32( pData + 16 );
	const uint32_t nUniqueStart = sirArchiveRead32( pData + 24 );
	uint32_t nCursor = sirArchiveRead32( pData + 28 );
	for ( uint32_t iUnique = 0; iUnique < nUnique; ++iUnique ) {
		const uint8_t *pRecord = pData + nUniqueStart + iUnique * SIR_SHADER_ARCHIVE_UNIQUE_BYTES;
		const uint32_t nOffset = sirArchiveRead32( pRecord + 4 );
		const uint32_t nLength = sirArchiveRead32( pRecord + 8 );
		if ( !sirArchiveFormatValid( sirArchiveRead32( pRecord ) ) || nLength == 0 || sirArchiveRead32( pRecord + 12 ) != 0 ||
		     nOffset != nCursor || nLength > nBytes - nCursor ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		if ( sirArchiveRead64( pRecord + 16 ) != sirArchiveHashBytes( pData + nOffset, nLength ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		nCursor += nLength;
	}
	if ( nCursor != nBytes ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	for ( uint32_t iEntry = 0; iEntry < nEntries; ++iEntry ) {
		const uint8_t *pEntry = pData + SIR_SHADER_ARCHIVE_HEADER_BYTES + iEntry * SIR_SHADER_ARCHIVE_ENTRY_BYTES;
		const uint32_t iUnique = sirArchiveRead32( pEntry + 16 );
		if ( !sirArchiveStageValid( sirArchiveRead32( pEntry + 4 ) ) || sirArchiveRead64( pEntry + 8 ) == 0 ||
		     sirArchiveRead32( pEntry + 20 ) != 0 || iUnique >= nUnique ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		const uint8_t *pUnique = pData + nUniqueStart + iUnique * SIR_SHADER_ARCHIVE_UNIQUE_BYTES;
		if ( sirArchiveRead32( pEntry ) != sirArchiveRead32( pUnique ) ||
		     sirArchiveRead64( pEntry + 24 ) != sirArchiveRead64( pUnique + 16 ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
	}
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirShaderArchiveResolve( const void *pArchive, uint64_t nBytes, uint32_t iEntry,
                                               sirShaderArchiveStageView_t *pOut ) noexcept
{
	if ( pOut == nullptr || sirShaderArchiveValidate( pArchive, nBytes ) != SIR_SHADER_COOK_OK ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const uint8_t *pData = ( const uint8_t * )pArchive;
	if ( iEntry >= sirArchiveRead32( pData + 12 ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const uint8_t *pEntry = pData + SIR_SHADER_ARCHIVE_HEADER_BYTES + iEntry * SIR_SHADER_ARCHIVE_ENTRY_BYTES;
	const uint32_t iUnique = sirArchiveRead32( pEntry + 16 );
	const uint8_t *pUnique = pData + sirArchiveRead32( pData + 24 ) + iUnique * SIR_SHADER_ARCHIVE_UNIQUE_BYTES;
	pOut->eFormat = ( sirGraphicsShaderFormat_t )sirArchiveRead32( pEntry );
	pOut->nStageMask = sirArchiveRead32( pEntry + 4 );
	pOut->nEntryPointHash = sirArchiveRead64( pEntry + 8 );
	pOut->nBinaryHash = sirArchiveRead64( pUnique + 16 );
	pOut->pBytes = pData + sirArchiveRead32( pUnique + 4 );
	pOut->nByteCount = sirArchiveRead32( pUnique + 8 );
	return SIR_SHADER_COOK_OK;
}
