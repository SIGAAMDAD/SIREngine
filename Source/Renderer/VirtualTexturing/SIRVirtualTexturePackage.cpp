/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackage.h>

uint64_t sirVTPackHash64( const void *pBytes, uint64_t nBytes ) noexcept
{
	const uint64_t nSeed = UINT64_C( 1469598103934665603 );
	const uint8_t *pInput = ( const uint8_t * )pBytes;
	if ( pInput == nullptr ) {
		return nBytes == 0 ? nSeed : 0;
	}
	uint64_t nHash = nSeed;
	for ( uint64_t i = 0; i < nBytes; ++i ) {
		nHash = ( nHash ^ pInput[i] ) * UINT64_C( 1099511628211 );
	}
	return nHash;
}

uint32_t sirVTPackChecksum32( const void *pBytes, uint64_t nBytes ) noexcept
{
	const uint8_t *pInput = ( const uint8_t * )pBytes;
	if ( pInput == nullptr && nBytes != 0 ) {
		return 0;
	}
	uint32_t nCrc = UINT32_MAX;
	for ( uint64_t i = 0; i < nBytes; ++i ) {
		nCrc ^= pInput[i];
		for ( uint32_t iBit = 0; iBit < 8; ++iBit ) {
			const uint32_t nMask = 0U - ( nCrc & 1U );
			nCrc = ( nCrc >> 1U ) ^ ( 0xEDB88320U & nMask );
		}
	}
	return ~nCrc;
}

uint32_t sirVTPackPhysicalTileBytes( sirVirtualTexturePhysicalFamily_t eFamily ) noexcept
{
	const uint32_t nBlocks = ( SIR_VT_PHYSICAL_TILE + 3U ) / 4U;
	switch ( eFamily ) {
	case SIR_VT_PHYSICAL_BC1:
		return nBlocks * nBlocks * 8U;
	case SIR_VT_PHYSICAL_BC7:
	case SIR_VT_PHYSICAL_BC5:
		return nBlocks * nBlocks * 16U;
	case SIR_VT_PHYSICAL_R8:
		return SIR_VT_PHYSICAL_TILE * SIR_VT_PHYSICAL_TILE;
	case SIR_VT_PHYSICAL_R16:
		return SIR_VT_PHYSICAL_TILE * SIR_VT_PHYSICAL_TILE * 2U;
	default:
		return 0;
	}
}

static bool sirVTPackRangeValid( uint64_t nStart, uint64_t nSize, uint64_t nTotal ) noexcept
{
	return nStart <= nTotal && nSize <= nTotal - nStart;
}

bool sirVTPackOpenDirectories( const sirVirtualTexturePackInput_t& input, sirVirtualTexturePackView_t *pView ) noexcept
{
	if ( pView == nullptr || input.pBytes == nullptr || input.nFileBytes < SIR_VT_PACK_HEADER_BYTES || input.nFileBytes > SIZE_MAX ||
	     input.nAvailableBytes < SIR_VT_PACK_HEADER_BYTES || input.nAvailableBytes > input.nFileBytes ||
	     ( !input.bMetadataOnly && input.nAvailableBytes != input.nFileBytes ) ) {
		return false;
	}
	// Terrain3D V1 writes native little-endian PODs. Do not reinterpret data on
	// a host of unknown endianness or with unvalidated directory alignment.
	const uint16_t nEndian = 1;
	if ( *( const uint8_t * )&nEndian != 1 ) {
		return false;
	}
	const uint8_t *pBytes = ( const uint8_t * )input.pBytes;
	sirVirtualTexturePackHeader_t header = {};
	memcpy( &header, pBytes, sizeof( header ) );
	if ( header.nMagic != SIR_VT_PACK_MAGIC || header.nVersion != SIR_VT_PACK_VERSION || header.nHeaderBytes != SIR_VT_PACK_HEADER_BYTES ||
	     header.nPackageBytes != input.nFileBytes || header.nAllocationCount == 0 ||
	     header.nAllocationCount > UINT32_MAX / SIR_VT_PACK_CHANNEL_COUNT ||
	     header.nChannelAllocationCount != header.nAllocationCount * SIR_VT_PACK_CHANNEL_COUNT ||
	     header.nChannelCount != SIR_VT_PACK_CHANNEL_COUNT || header.nInteriorTexels != SIR_VT_PAGE_INTERIOR ||
	     header.nGutterTexels != SIR_VT_PAGE_GUTTER || header.nPhysicalTexels != SIR_VT_PHYSICAL_TILE ||
	     header.nFormatFamilyCount != SIR_VT_PACK_CHANNEL_COUNT || header.nPayloadCount == 0 || header.nClusterCount == 0 ||
	     header.nTailCount == 0 || header.nQualityViewCount == 0 || header.nFullySupportedProfileMask == 0 ||
	     ( header.nFullySupportedProfileMask & ~63U ) != 0 ||
	     ( header.nFlags & ( SIR_VT_PACK_FLAG_QUALITY_VIEWS | SIR_VT_PACK_FLAG_FAMILY_MAP ) ) !=
	         ( SIR_VT_PACK_FLAG_QUALITY_VIEWS | SIR_VT_PACK_FLAG_FAMILY_MAP ) ) {
		return false;
	}
	const uint32_t nSavedChecksum = header.nHeaderChecksum;
	header.nHeaderChecksum = 0;
	if ( sirVTPackChecksum32( &header, sizeof( header ) ) != nSavedChecksum ) {
		return false;
	}
	header.nHeaderChecksum = nSavedChecksum;
	if ( !input.bMetadataOnly &&
	     sirVTPackHash64( pBytes + SIR_VT_PACK_HEADER_BYTES, input.nFileBytes - SIR_VT_PACK_HEADER_BYTES ) != header.nContentSignature ) {
		return false;
	}

	const uint64_t szOffsets[SIR_VT_PACK_DIRECTORY_COUNT] = {
	    header.nAllocationOffset,       header.nAllocationKeyOffset,    header.nChannelAllocationOffset, header.nPageOffset,
	    header.nPayloadDirectoryOffset, header.nClusterDirectoryOffset, header.nTailMetadataOffset,      header.nQualityViewOffset,
	    header.nFormatFamilyOffset,     header.nPayloadDataOffset };
	const uint64_t szBytes[SIR_VT_PACK_DIRECTORY_COUNT] = {
	    ( uint64_t )header.nAllocationCount * sizeof( sirVirtualTexturePackAllocationRecord_t ),
	    ( uint64_t )header.nAllocationCount * sizeof( sirVirtualTexturePackAllocationKey_t ),
	    ( uint64_t )header.nChannelAllocationCount * sizeof( sirVirtualTexturePackChannelAllocation_t ),
	    ( uint64_t )header.nPageCount * sizeof( sirVirtualTexturePackPageRecord_t ),
	    ( uint64_t )header.nPayloadCount * sizeof( sirVirtualTexturePackPayloadRecord_t ),
	    ( uint64_t )header.nClusterCount * sizeof( sirVirtualTexturePackClusterRecord_t ),
	    ( uint64_t )header.nTailCount * sizeof( sirVirtualTexturePackTailRecord_t ),
	    ( uint64_t )header.nQualityViewCount * sizeof( sirVirtualTexturePackQualityView_t ),
	    ( uint64_t )header.nFormatFamilyCount * sizeof( sirVirtualTexturePackFamilyRecord_t ),
	    header.nPayloadDataBytes };

	sirVirtualTexturePackView_t candidate = {};
	candidate.header = header;
	candidate.bMetadataOnly = input.bMetadataOnly;
	uint64_t nPreviousEnd = SIR_VT_PACK_HEADER_BYTES;
	for ( uint32_t iDirectory = 0; iDirectory < SIR_VT_PACK_DIRECTORY_COUNT; ++iDirectory ) {
		const uint64_t nOffset = szOffsets[iDirectory];
		const uint64_t nSize = szBytes[iDirectory];
		if ( ( nOffset & ( SIR_VT_PACK_ALIGNMENT - 1U ) ) != 0 || nOffset < nPreviousEnd ||
		     !sirVTPackRangeValid( nOffset, nSize, input.nFileBytes ) ||
		     ( iDirectory == SIR_VT_PACK_ALLOCATIONS && nOffset != SIR_VT_PACK_HEADER_BYTES ) ) {
			return false;
		}
		candidate.szDirectories[iDirectory] = { input.bMetadataOnly && iDirectory == SIR_VT_PACK_PAYLOAD_DATA ? nullptr : pBytes + nOffset,
		                                        nSize, nOffset };
		nPreviousEnd = nOffset + nSize;
	}
	if ( nPreviousEnd != input.nFileBytes || ( input.bMetadataOnly && input.nAvailableBytes != header.nPayloadDataOffset ) ||
	     ( !input.bMetadataOnly && header.nPayloadDataOffset > input.nAvailableBytes ) ) {
		return false;
	}
	*pView = candidate;
	return true;
}

bool sirVTPlaceCompactTail( const sirVirtualTextureTailRequest_t& request, sirVirtualTextureTailPlacement_t *pPlacement ) noexcept
{
	if ( pPlacement == nullptr || request.nSlotsPerRow == 0 || request.nSlotWidth == 0 || ( request.nSlotWidth & 1U ) != 0 ||
	     request.nSlotHeight == 0 || request.nSlotWidth > UINT32_MAX / request.nSlotsPerRow ) {
		return false;
	}
	uint32_t nNext = request.nNextHalf;
	if ( !request.bCompact && ( nNext & 1U ) != 0 ) {
		if ( nNext == UINT32_MAX ) {
			return false;
		}
		++nNext;
	}
	const uint32_t nCount = request.bCompact ? 1U : 2U;
	if ( nNext > UINT32_MAX - nCount ) {
		return false;
	}
	const uint32_t nHalfIndex = nNext;
	const uint32_t nFullSlot = nHalfIndex >> 1U;
	const uint64_t nY = ( uint64_t )( nFullSlot / request.nSlotsPerRow ) * request.nSlotHeight;
	if ( nY > ( uint64_t )UINT32_MAX - request.nSlotHeight ) {
		return false;
	}
	*pPlacement = { nNext + nCount,
	                ( nFullSlot % request.nSlotsPerRow ) * request.nSlotWidth +
	                    ( request.bCompact ? ( nHalfIndex & 1U ) * ( request.nSlotWidth >> 1U ) : 0U ),
	                ( uint32_t )nY };
	return true;
}
