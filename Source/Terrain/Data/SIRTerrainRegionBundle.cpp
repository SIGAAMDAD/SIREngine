/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Terrain/Data/SIRTerrainRegionBundle.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

static constexpr uint8_t SIR_TERRAIN_BUNDLE_MAGIC[4] = { 'S', 'T', 'R', 'B' };
static constexpr uint16_t SIR_TERRAIN_BUNDLE_VERSION = 1;
static constexpr uint32_t SIR_TERRAIN_BUNDLE_INDEX_OFFSET = 64;
static constexpr uint32_t SIR_TERRAIN_BUNDLE_CHECKSUM_OFFSET = 40;

static uint16_t sirBundleRead16( const uint8_t *pData ) noexcept
{
	return ( uint16_t )( static_cast<uint16_t>(pData[0]) | ( static_cast<uint16_t>(pData[1]) << 8u ) );
}

static uint32_t sirBundleRead32( const uint8_t *pData ) noexcept
{
	return static_cast<uint32_t>(pData[0]) | ( static_cast<uint32_t>(pData[1]) << 8u ) | ( static_cast<uint32_t>(pData[2]) << 16u ) | ( static_cast<uint32_t>(pData[3]) << 24u );
}

static uint64_t sirBundleRead64( const uint8_t *pData ) noexcept
{
	return static_cast<uint64_t>( sirBundleRead32(  pData  ) ) | ( static_cast<uint64_t>( sirBundleRead32(  pData + 4  ) ) << 32u );
}

static void sirBundleWrite16( uint8_t *pData, uint16_t nValue ) noexcept
{
	pData[0] = static_cast<uint8_t>(nValue);
	pData[1] = static_cast<uint8_t>( nValue >> 8u );
}

static void sirBundleWrite32( uint8_t *pData, uint32_t nValue ) noexcept
{
	for ( uint32_t nByte = 0; nByte < 4; ++nByte ) {
		pData[nByte] = static_cast<uint8_t>(  nValue >> ( nByte * 8u )  );
	}
}

static void sirBundleWrite64( uint8_t *pData, uint64_t nValue ) noexcept
{
	sirBundleWrite32( pData, static_cast<uint32_t>(nValue) );
	sirBundleWrite32( pData + 4, static_cast<uint32_t>( nValue >> 32u ) );
}

static uint64_t sirBundleHash( const uint8_t *pData, size_t nCount ) noexcept
{
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( size_t nIndex = 0; nIndex < nCount; ++nIndex ) {
		nHash = ( nHash ^ pData[nIndex] ) * UINT64_C( 1099511628211 );
	}
	return nHash;
}

static int sirBundleCompareKey( int32_t iLeftX, int32_t iLeftZ, int32_t iRightX, int32_t iRightZ ) noexcept
{
	if ( iLeftX != iRightX ) {
		return iLeftX < iRightX ? -1 : 1;
	}
	if ( iLeftZ != iRightZ ) {
		return iLeftZ < iRightZ ? -1 : 1;
	}
	return 0;
}

static bool sirBundleOverlaps( const uint8_t *pLeft, size_t nLeft, const uint8_t *pRight, size_t nRight ) noexcept
{
	const uintptr_t nL = reinterpret_cast<uintptr_t>(pLeft);
	const uintptr_t nR = reinterpret_cast<uintptr_t>(pRight);
	return nL <= nR ? nR - nL < nLeft : nL - nR < nRight;
}

static const uint8_t *sirBundleEntry( const uint8_t *pBytes, uint32_t nIndex ) noexcept
{
	return pBytes + SIR_TERRAIN_BUNDLE_INDEX_OFFSET + static_cast<size_t>(nIndex) * SIRTerrainRegionBundle::ENTRY_BYTES;
}

SIRTerrainBundleResult SIRTerrainRegionBundle::measure( const sirTerrainConstByteSpan_t *pRegions, uint32_t nCount,
                                                          size_t& nBytes ) noexcept
{
	if ( !pRegions || nCount == 0 || nCount > MAX_REGIONS ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT;
	}
	size_t nRequired = HEADER_BYTES + static_cast<size_t>(nCount) * ENTRY_BYTES;
	int32_t iPreviousX = 0;
	int32_t iPreviousZ = 0;
	for ( uint32_t nIndex = 0; nIndex < nCount; ++nIndex ) {
		sirTerrainRegionDescriptor_t descriptor {};
		if ( SIRTerrainHeightfieldCodec::inspect( pRegions[nIndex], descriptor ) != SIRTerrainCodecResult::SIR_TERRAIN_CODEC_SUCCESS ) {
			return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_FORMAT;
		}
		if ( nIndex != 0 && sirBundleCompareKey( iPreviousX, iPreviousZ, descriptor.iRegionX, descriptor.iRegionZ ) >= 0 ) {
			return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_DUPLICATE_OR_UNSORTED;
		}
		if ( pRegions[nIndex].nCapacity > SIZE_MAX - nRequired ) {
			return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT;
		}
		nRequired += pRegions[nIndex].nCapacity;
		iPreviousX = descriptor.iRegionX;
		iPreviousZ = descriptor.iRegionZ;
	}
	nBytes = nRequired;
	return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_SUCCESS;
}

SIRTerrainBundleResult SIRTerrainRegionBundle::write( const sirTerrainConstByteSpan_t *pRegions, uint32_t nCount,
                                                        sirTerrainMutableByteSpan_t output, size_t& nWritten ) noexcept
{
	size_t nRequired = 0;
	const SIRTerrainBundleResult eMeasured = measure( pRegions, nCount, nRequired );
	if ( eMeasured != SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_SUCCESS ) {
		return eMeasured;
	}
	if ( !output.pData ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT;
	}
	if ( output.nCapacity < nRequired ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_BUFFER_TOO_SMALL;
	}
	for ( uint32_t nIndex = 0; nIndex < nCount; ++nIndex ) {
		if ( sirBundleOverlaps( pRegions[nIndex].pData, pRegions[nIndex].nCapacity, output.pData, nRequired ) ) {
			return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_OVERLAPPING_BUFFERS;
		}
	}
	memset( output.pData, 0, HEADER_BYTES + static_cast<size_t>(nCount) * ENTRY_BYTES );
	memcpy( output.pData, SIR_TERRAIN_BUNDLE_MAGIC, 4 );
	sirBundleWrite16( output.pData + 4, SIR_TERRAIN_BUNDLE_VERSION );
	sirBundleWrite16( output.pData + 6, HEADER_BYTES );
	sirBundleWrite32( output.pData + 8, nCount );
	sirBundleWrite32( output.pData + 12, ENTRY_BYTES );
	sirBundleWrite64( output.pData + 16, nRequired );
	size_t nOffset = HEADER_BYTES + static_cast<size_t>(nCount) * ENTRY_BYTES;
	for ( uint32_t nIndex = 0; nIndex < nCount; ++nIndex ) {
		sirTerrainRegionDescriptor_t descriptor {};
		static_cast<void>(SIRTerrainHeightfieldCodec::inspect( pRegions[nIndex], descriptor ));
		uint8_t *pEntry = output.pData + HEADER_BYTES + static_cast<size_t>(nIndex) * ENTRY_BYTES;
		sirBundleWrite32( pEntry, static_cast<uint32_t>(descriptor.iRegionX) );
		sirBundleWrite32( pEntry + 4, static_cast<uint32_t>(descriptor.iRegionZ) );
		sirBundleWrite64( pEntry + 8, descriptor.nRevision );
		sirBundleWrite64( pEntry + 16, nOffset );
		sirBundleWrite64( pEntry + 24, pRegions[nIndex].nCapacity );
		sirBundleWrite32( pEntry + 32, descriptor.nResolution );
		memcpy( output.pData + nOffset, pRegions[nIndex].pData, pRegions[nIndex].nCapacity );
		nOffset += pRegions[nIndex].nCapacity;
	}
	const size_t nIndexBytes = static_cast<size_t>(nCount) * ENTRY_BYTES;
	sirBundleWrite64( output.pData + 24, sirBundleHash( output.pData + HEADER_BYTES, nIndexBytes ) );
	sirBundleWrite64( output.pData + 32,
	                  sirBundleHash( output.pData + HEADER_BYTES + nIndexBytes, nRequired - HEADER_BYTES - nIndexBytes ) );
	sirBundleWrite64( output.pData + SIR_TERRAIN_BUNDLE_CHECKSUM_OFFSET,
	                  sirBundleHash( output.pData, SIR_TERRAIN_BUNDLE_CHECKSUM_OFFSET ) );
	nWritten = nRequired;
	return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_SUCCESS;
}

SIRTerrainBundleResult SIRTerrainRegionBundle::open( sirTerrainConstByteSpan_t input, sirTerrainRegionBundleView_t& view ) noexcept
{
	if ( !input.pData ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT;
	}
	if ( input.nCapacity < HEADER_BYTES ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_BUFFER_TOO_SMALL;
	}
	if ( memcmp( input.pData, SIR_TERRAIN_BUNDLE_MAGIC, 4 ) != 0 ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_FORMAT;
	}
	if ( sirBundleRead16( input.pData + 4 ) != SIR_TERRAIN_BUNDLE_VERSION ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_UNSUPPORTED_VERSION;
	}
	const uint32_t nCount = sirBundleRead32( input.pData + 8 );
	if ( sirBundleRead16( input.pData + 6 ) != HEADER_BYTES || sirBundleRead32( input.pData + 12 ) != ENTRY_BYTES || nCount == 0 ||
	     nCount > MAX_REGIONS || sirBundleRead64( input.pData + 16 ) != input.nCapacity ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_FORMAT;
	}
	for ( size_t nIndex = 48; nIndex < HEADER_BYTES; ++nIndex ) {
		if ( input.pData[nIndex] != 0 ) {
			return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_FORMAT;
		}
	}
	if ( sirBundleRead64( input.pData + SIR_TERRAIN_BUNDLE_CHECKSUM_OFFSET ) !=
	     sirBundleHash( input.pData, SIR_TERRAIN_BUNDLE_CHECKSUM_OFFSET ) ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_BAD_CHECKSUM;
	}
	const size_t nIndexBytes = static_cast<size_t>(nCount) * ENTRY_BYTES;
	if ( nIndexBytes > input.nCapacity - HEADER_BYTES ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_BUFFER_TOO_SMALL;
	}
	const size_t nPayloadOffset = HEADER_BYTES + nIndexBytes;
	if ( sirBundleRead64( input.pData + 24 ) != sirBundleHash( input.pData + HEADER_BYTES, nIndexBytes ) ||
	     sirBundleRead64( input.pData + 32 ) != sirBundleHash( input.pData + nPayloadOffset, input.nCapacity - nPayloadOffset ) ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_BAD_CHECKSUM;
	}
	size_t nExpectedOffset = nPayloadOffset;
	int32_t iPreviousX = 0;
	int32_t iPreviousZ = 0;
	for ( uint32_t nIndex = 0; nIndex < nCount; ++nIndex ) {
		const uint8_t *pEntry = sirBundleEntry( input.pData, nIndex );
		const int32_t iX = static_cast<int32_t>( sirBundleRead32(  pEntry  ) );
		const int32_t iZ = static_cast<int32_t>( sirBundleRead32(  pEntry + 4  ) );
		const uint64_t nRevision = sirBundleRead64( pEntry + 8 );
		const uint64_t nOffset = sirBundleRead64( pEntry + 16 );
		const uint64_t nLength = sirBundleRead64( pEntry + 24 );
		const uint32_t nResolution = sirBundleRead32( pEntry + 32 );
		if ( nIndex && sirBundleCompareKey( iPreviousX, iPreviousZ, iX, iZ ) >= 0 ) {
			return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_DUPLICATE_OR_UNSORTED;
		}
		if ( sirBundleRead32( pEntry + 36 ) != 0 || nOffset != nExpectedOffset || nLength > input.nCapacity - nExpectedOffset ) {
			return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_FORMAT;
		}
		sirTerrainRegionDescriptor_t descriptor {};
		if ( SIRTerrainHeightfieldCodec::inspect( { input.pData + nExpectedOffset, static_cast<size_t>(nLength) }, descriptor ) !=
		         SIRTerrainCodecResult::SIR_TERRAIN_CODEC_SUCCESS ||
		     descriptor.iRegionX != iX || descriptor.iRegionZ != iZ || descriptor.nRevision != nRevision ||
		     descriptor.nResolution != nResolution ) {
			return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_FORMAT;
		}
		nExpectedOffset += static_cast<size_t>(nLength);
		iPreviousX = iX;
		iPreviousZ = iZ;
	}
	if ( nExpectedOffset != input.nCapacity ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_FORMAT;
	}
	view = { input.pData, input.nCapacity, nCount };
	return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_SUCCESS;
}

SIRTerrainBundleResult SIRTerrainRegionBundle::find( const sirTerrainRegionBundleView_t& view, int32_t iRegionX, int32_t iRegionZ,
                                                       sirTerrainConstByteSpan_t& region ) noexcept
{
	if ( !view.pBytes || view.nRegionCount == 0 || view.nRegionCount > MAX_REGIONS || view.nBytes < HEADER_BYTES ||
	     static_cast<size_t>(view.nRegionCount) * ENTRY_BYTES > view.nBytes - HEADER_BYTES ) {
		return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT;
	}
	uint32_t nLow = 0;
	uint32_t nHigh = view.nRegionCount;
	while ( nLow < nHigh ) {
		const uint32_t nMiddle = nLow + ( nHigh - nLow ) / 2;
		const uint8_t *pEntry = sirBundleEntry( view.pBytes, nMiddle );
		const int nCompare =
		    sirBundleCompareKey( iRegionX, iRegionZ, static_cast<int32_t>( sirBundleRead32(  pEntry  ) ), static_cast<int32_t>( sirBundleRead32(  pEntry + 4  ) ) );
		if ( nCompare == 0 ) {
			const uint64_t nOffset = sirBundleRead64( pEntry + 16 );
			const uint64_t nLength = sirBundleRead64( pEntry + 24 );
			if ( nOffset > view.nBytes || nLength > view.nBytes - static_cast<size_t>(nOffset) ) {
				return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_INVALID_FORMAT;
			}
			region = { view.pBytes + static_cast<size_t>(nOffset), static_cast<size_t>(nLength) };
			return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_SUCCESS;
		}
		if ( nCompare < 0 ) {
			nHigh = nMiddle;
		} else {
			nLow = nMiddle + 1;
		}
	}
	return SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_NOT_FOUND;
}
