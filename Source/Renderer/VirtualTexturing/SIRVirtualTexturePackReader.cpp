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

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackReader.h>

// Source: Terrain3D data/formats/terrain_3d_vt_pack_reader.cpp.
// memcpy preserves the frozen on-disk POD layouts when a caller supplies an
// unaligned file view; do not reinterpret unaligned byte spans as record arrays.
template <typename T>
static T sirVTReadPackRecord( const sirVirtualTexturePackView_t& view, sirVirtualTexturePackDirectory_t eDirectory,
                              uint32_t nIndex ) noexcept
{
	T record = {};
	memcpy( &record, view.szDirectories[eDirectory].pData + ( uint64_t )nIndex * sizeof( T ), sizeof( T ) );
	return record;
}

static constexpr uint32_t SIR_VT_PACK_INVALID_INDEX = UINT32_MAX;
static constexpr uint8_t SIR_VT_PACK_INVALID_MIP = UINT8_MAX;
static constexpr uint16_t SIR_VT_PACK_ALBEDO_RGBA_FLAG = 1U << 14U;
static constexpr uint32_t SIR_VT_PACK_PAYLOAD_TAIL_FLAG = 1U;

static uint32_t sirVTAllowedChannelMask( uint8_t nConsumer ) noexcept
{
	if ( nConsumer == 0 ) {
		return ( 1U << 0 ) | ( 1U << 1 ) | ( 1U << 2 );
	}
	if ( nConsumer == 1 ) {
		return ( 1U << 0 ) | ( 1U << 3 ) | ( 1U << 4 );
	}
	if ( nConsumer == 2 ) {
		return ( 1U << 0 ) | ( 1U << 3 ) | ( 1U << 1 );
	}
	return 0;
}

static uint32_t sirVTRequiredChannelMask( uint8_t nConsumer ) noexcept
{
	if ( nConsumer == 0 ) {
		return ( 1U << 0 ) | ( 1U << 1 );
	}
	if ( nConsumer == 1 ) {
		return 1U << 0;
	}
	if ( nConsumer == 2 ) {
		return ( 1U << 0 ) | ( 1U << 3 ) | ( 1U << 1 );
	}
	return 0;
}

static uint32_t sirVTChannelPhysicalFamily( uint32_t nChannel, uint16_t nFlags ) noexcept
{
	static constexpr uint8_t s_szFamilies[SIR_VT_PACK_CHANNEL_COUNT] = { 0, 1, 3, 2, 0, 3 };
	if ( nChannel == 0 && ( nFlags & SIR_VT_PACK_ALBEDO_RGBA_FLAG ) != 0 ) {
		return SIR_VT_PHYSICAL_BC7;
	}
	return s_szFamilies[nChannel];
}

static bool sirVTValidatePackFormats( const sirVirtualTexturePackView_t& view ) noexcept
{
	static constexpr uint8_t s_szSemantics[SIR_VT_PACK_CHANNEL_COUNT] = { 0, 1, 2, 3, 4, 4 };
	for ( uint32_t iChannel = 0; iChannel < SIR_VT_PACK_CHANNEL_COUNT; ++iChannel ) {
		const sirVirtualTexturePackFamilyRecord_t record =
		    sirVTReadPackRecord<sirVirtualTexturePackFamilyRecord_t>( view, SIR_VT_PACK_FAMILIES, iChannel );
		if ( record.nChannel != iChannel || record.nFamily != sirVTChannelPhysicalFamily( iChannel, 0 ) ||
		     record.nSemantic != s_szSemantics[iChannel] || ( record.nFlags & ~( iChannel == 0 ? 1U : 0U ) ) != 0 ||
		     record.nReserved != 0 ) {
			return false;
		}
	}
	return true;
}

static bool sirVTValidatePackAllocations( const sirVirtualTexturePackView_t& view, uint32_t *pFullySupportedMask ) noexcept
{
	uint64_t nPreviousKey = 0;
	uint64_t nExpectedPageBase = 0;
	uint64_t nExpectedViews = 0;
	uint32_t nProfileIntersection = ( 1U << SIR_VT_PACK_QUALITY_PROFILES ) - 1U;
	for ( uint32_t iAllocation = 0; iAllocation < view.header.nAllocationCount; ++iAllocation ) {
		const sirVirtualTexturePackAllocationRecord_t allocation =
		    sirVTReadPackRecord<sirVirtualTexturePackAllocationRecord_t>( view, SIR_VT_PACK_ALLOCATIONS, iAllocation );
		const sirVirtualTexturePackAllocationKey_t key =
		    sirVTReadPackRecord<sirVirtualTexturePackAllocationKey_t>( view, SIR_VT_PACK_ALLOCATION_KEYS, iAllocation );
		const uint32_t nAllowed = sirVTAllowedChannelMask( allocation.nConsumer );
		const uint32_t nRequired = sirVTRequiredChannelMask( allocation.nConsumer );
		const uint32_t nExpectedMask =
		    allocation.nHighestProfile < SIR_VT_PACK_QUALITY_PROFILES ? ( 1U << ( allocation.nHighestProfile + 1U ) ) - 1U : 0;
		if ( allocation.nConsumer > 2 || ( allocation.nChannelMask & nRequired ) != nRequired ||
		     ( allocation.nChannelMask & ~nAllowed ) != 0 || nExpectedMask == 0 || allocation.nAvailableProfiles != nExpectedMask ||
		     allocation.nReserved != 0 || key.nStableKey == 0 || key.nAllocationId != iAllocation ||
		     ( iAllocation > 0 && key.nStableKey <= nPreviousKey ) || key.nContentId != allocation.nContentId ) {
			return false;
		}
		nProfileIntersection &= allocation.nAvailableProfiles;
		nExpectedViews += allocation.nHighestProfile + 1U;
		nPreviousKey = key.nStableKey;
		for ( uint32_t iChannel = 0; iChannel < SIR_VT_PACK_CHANNEL_COUNT; ++iChannel ) {
			const sirVirtualTexturePackChannelAllocation_t channel = sirVTReadPackRecord<sirVirtualTexturePackChannelAllocation_t>(
			    view, SIR_VT_PACK_CHANNEL_ALLOCATIONS, iAllocation * SIR_VT_PACK_CHANNEL_COUNT + iChannel );
			if ( ( allocation.nChannelMask & ( 1U << iChannel ) ) == 0 ) {
				if ( channel.nBasePage != 0 || channel.nPageCount != 0 || channel.nTailIndex != SIR_VT_PACK_INVALID_INDEX ||
				     channel.nMip0WidthPages != 0 || channel.nMip0HeightPages != 0 || channel.nMipCount != 0 || channel.nFlags != 0 ) {
					return false;
				}
				continue;
			}
			if ( channel.nMipCount == 0 || channel.nMipCount > 16 || channel.nMip0WidthPages == 0 || channel.nMip0HeightPages == 0 ||
			     channel.nTailIndex >= view.header.nTailCount || channel.nBasePage != nExpectedPageBase ||
			     channel.nBasePage > view.header.nPageCount || channel.nPageCount > view.header.nPageCount - channel.nBasePage ) {
				return false;
			}
			const sirVirtualTexturePackTailRecord_t tail =
			    sirVTReadPackRecord<sirVirtualTexturePackTailRecord_t>( view, SIR_VT_PACK_TAILS, channel.nTailIndex );
			const sirVirtualTexturePackFamilyRecord_t mapping =
			    sirVTReadPackRecord<sirVirtualTexturePackFamilyRecord_t>( view, SIR_VT_PACK_FAMILIES, iChannel );
			if ( ( iChannel == 0 && ( channel.nFlags & SIR_VT_PACK_ALBEDO_RGBA_FLAG ) != 0 && ( mapping.nFlags & 1U ) == 0 ) ||
			     tail.nChannel != iChannel || tail.nFirstMip >= channel.nMipCount ||
			     tail.nMipCount != channel.nMipCount - tail.nFirstMip ) {
				return false;
			}
			nExpectedPageBase += channel.nPageCount;
		}
	}
	if ( nExpectedPageBase != view.header.nPageCount || nExpectedViews != view.header.nQualityViewCount || nProfileIntersection == 0 ) {
		return false;
	}
	*pFullySupportedMask = nProfileIntersection;
	return true;
}

// Reject cyclic or inconsistent parent chains even when all package hashes
// are recomputed. The donor cooker stores coarser (x/2, y/2) parents.
static bool sirVTValidatePackPageParent( const sirVirtualTexturePackView_t& view, const sirVirtualTexturePackPageRecord_t& page ) noexcept
{
	if ( page.nParentPage == SIR_VT_PACK_INVALID_INDEX ) {
		return true;
	}
	const sirVirtualTexturePackPageRecord_t parent =
	    sirVTReadPackRecord<sirVirtualTexturePackPageRecord_t>( view, SIR_VT_PACK_PAGES, page.nParentPage );
	if ( parent.nAllocationId != page.nAllocationId || parent.nChannel != page.nChannel || parent.nGeneration != page.nGeneration ) {
		return false;
	}
	if ( parent.nContentId != page.nContentId || parent.nMip != page.nMip + 1U || parent.nPageX != ( page.nPageX >> 1U ) ) {
		return false;
	}
	return parent.nPageY == ( page.nPageY >> 1U );
}

static bool sirVTValidatePackPages( const sirVirtualTexturePackView_t& view ) noexcept
{
	for ( uint32_t iPage = 0; iPage < view.header.nPageCount; ++iPage ) {
		const sirVirtualTexturePackPageRecord_t page =
		    sirVTReadPackRecord<sirVirtualTexturePackPageRecord_t>( view, SIR_VT_PACK_PAGES, iPage );
		if ( page.nVirtualPage != iPage || page.nAllocationId >= view.header.nAllocationCount ||
		     page.nPayloadId >= view.header.nPayloadCount || page.nChannel >= SIR_VT_PACK_CHANNEL_COUNT ) {
			return false;
		}
		const sirVirtualTexturePackAllocationRecord_t allocation =
		    sirVTReadPackRecord<sirVirtualTexturePackAllocationRecord_t>( view, SIR_VT_PACK_ALLOCATIONS, page.nAllocationId );
		if ( ( allocation.nChannelMask & ( 1U << page.nChannel ) ) == 0 || page.nGeneration != allocation.nGeneration ||
		     page.nContentId != allocation.nContentId ) {
			return false;
		}
		const sirVirtualTexturePackChannelAllocation_t channel = sirVTReadPackRecord<sirVirtualTexturePackChannelAllocation_t>(
		    view, SIR_VT_PACK_CHANNEL_ALLOCATIONS, page.nAllocationId * SIR_VT_PACK_CHANNEL_COUNT + page.nChannel );
		if ( iPage < channel.nBasePage || iPage >= ( uint64_t )channel.nBasePage + channel.nPageCount || page.nMip >= channel.nMipCount ||
		     ( page.nParentPage != SIR_VT_PACK_INVALID_INDEX &&
		       ( page.nParentPage < channel.nBasePage || page.nParentPage >= ( uint64_t )channel.nBasePage + channel.nPageCount ) ) ) {
			return false;
		}
		if ( !sirVTValidatePackPageParent( view, page ) ) {
			return false;
		}
		const sirVirtualTexturePackPayloadRecord_t payload =
		    sirVTReadPackRecord<sirVirtualTexturePackPayloadRecord_t>( view, SIR_VT_PACK_PAYLOADS, page.nPayloadId );
		if ( payload.nFormat != sirVTChannelPhysicalFamily( page.nChannel, channel.nFlags ) ||
		     payload.nDecodedBytes != sirVTPackPhysicalTileBytes( ( sirVirtualTexturePhysicalFamily_t )payload.nFormat ) ) {
			return false;
		}
	}
	return true;
}

static bool sirVTValidatePackPayloads( const sirVirtualTexturePackView_t& view ) noexcept
{
	for ( uint32_t iPayload = 0; iPayload < view.header.nPayloadCount; ++iPayload ) {
		const sirVirtualTexturePackPayloadRecord_t payload =
		    sirVTReadPackRecord<sirVirtualTexturePackPayloadRecord_t>( view, SIR_VT_PACK_PAYLOADS, iPayload );
		if ( payload.nClusterId >= view.header.nClusterCount || payload.nStoredBytes == 0 || payload.nDecodedBytes == 0 ||
		     payload.nFormat >= SIR_VT_PHYSICAL_COUNT || payload.nCodec != 0 || payload.nStoredBytes != payload.nDecodedBytes ||
		     payload.nDecodedBytes != sirVTPackPhysicalTileBytes( ( sirVirtualTexturePhysicalFamily_t )payload.nFormat ) ) {
			return false;
		}
		const sirVirtualTexturePackClusterRecord_t cluster =
		    sirVTReadPackRecord<sirVirtualTexturePackClusterRecord_t>( view, SIR_VT_PACK_CLUSTERS, payload.nClusterId );
		if ( payload.nClusterOffset > cluster.nDecodedBytes || payload.nDecodedBytes > cluster.nDecodedBytes - payload.nClusterOffset ) {
			return false;
		}
	}
	return true;
}

static bool sirVTValidatePackClusters( const sirVirtualTexturePackView_t& view ) noexcept
{
	uint32_t nExpectedFirst = 0;
	for ( uint32_t iCluster = 0; iCluster < view.header.nClusterCount; ++iCluster ) {
		const sirVirtualTexturePackClusterRecord_t cluster =
		    sirVTReadPackRecord<sirVirtualTexturePackClusterRecord_t>( view, SIR_VT_PACK_CLUSTERS, iCluster );
		if ( cluster.nPayloadDataOffset > view.header.nPayloadDataBytes ||
		     cluster.nStoredBytes > view.header.nPayloadDataBytes - cluster.nPayloadDataOffset || cluster.nStoredBytes == 0 ||
		     cluster.nDecodedBytes == 0 || cluster.nFirstPayload != nExpectedFirst || cluster.nPayloadCount == 0 ||
		     cluster.nFirstPayload > view.header.nPayloadCount ||
		     cluster.nPayloadCount > view.header.nPayloadCount - cluster.nFirstPayload || cluster.nCodec > 1 ||
		     ( cluster.nCodec == 0 && cluster.nStoredBytes != cluster.nDecodedBytes ) ) {
			return false;
		}
		for ( uint32_t i = 0; i < cluster.nPayloadCount; ++i ) {
			const sirVirtualTexturePackPayloadRecord_t payload =
			    sirVTReadPackRecord<sirVirtualTexturePackPayloadRecord_t>( view, SIR_VT_PACK_PAYLOADS, cluster.nFirstPayload + i );
			if ( payload.nClusterId != iCluster ) {
				return false;
			}
		}
		nExpectedFirst += cluster.nPayloadCount;
	}
	return nExpectedFirst == view.header.nPayloadCount;
}

static bool sirVTValidatePackTails( const sirVirtualTexturePackView_t& view ) noexcept
{
	for ( uint32_t iTail = 0; iTail < view.header.nTailCount; ++iTail ) {
		const sirVirtualTexturePackTailRecord_t tail =
		    sirVTReadPackRecord<sirVirtualTexturePackTailRecord_t>( view, SIR_VT_PACK_TAILS, iTail );
		if ( tail.nCanonicalAllocationId >= view.header.nAllocationCount || tail.nPayloadId >= view.header.nPayloadCount ||
		     tail.nChannel >= SIR_VT_PACK_CHANNEL_COUNT || tail.nMipCount == 0 || tail.nFirstMipWidth == 0 || tail.nFirstMipHeight == 0 ||
		     tail.nAtlasInteriorWidth == 0 || tail.nAtlasInteriorWidth > SIR_VT_PAGE_INTERIOR || tail.nAtlasInteriorHeight == 0 ||
		     tail.nAtlasInteriorHeight > SIR_VT_PAGE_INTERIOR || tail.nLayout != SIR_VT_TAIL_LAYOUT_HORIZONTAL ) {
			return false;
		}
		const sirVirtualTexturePackAllocationRecord_t allocation =
		    sirVTReadPackRecord<sirVirtualTexturePackAllocationRecord_t>( view, SIR_VT_PACK_ALLOCATIONS, tail.nCanonicalAllocationId );
		if ( ( allocation.nChannelMask & ( 1U << tail.nChannel ) ) == 0 ) {
			return false;
		}
		const sirVirtualTexturePackChannelAllocation_t channel = sirVTReadPackRecord<sirVirtualTexturePackChannelAllocation_t>(
		    view, SIR_VT_PACK_CHANNEL_ALLOCATIONS, tail.nCanonicalAllocationId * SIR_VT_PACK_CHANNEL_COUNT + tail.nChannel );
		const sirVirtualTexturePackPayloadRecord_t payload =
		    sirVTReadPackRecord<sirVirtualTexturePackPayloadRecord_t>( view, SIR_VT_PACK_PAYLOADS, tail.nPayloadId );
		if ( payload.nFormat != sirVTChannelPhysicalFamily( tail.nChannel, channel.nFlags ) ||
		     ( payload.nFlags & SIR_VT_PACK_PAYLOAD_TAIL_FLAG ) == 0 ) {
			return false;
		}
	}
	return true;
}

static bool sirVTValidatePackQualityViews( const sirVirtualTexturePackView_t& view ) noexcept
{
	uint32_t nPreviousAllocation = 0;
	uint32_t nPreviousProfile = 0;
	uint32_t nSeenMask = 0;
	for ( uint32_t iView = 0; iView < view.header.nQualityViewCount; ++iView ) {
		const sirVirtualTexturePackQualityView_t quality =
		    sirVTReadPackRecord<sirVirtualTexturePackQualityView_t>( view, SIR_VT_PACK_QUALITY_VIEWS, iView );
		if ( quality.nAllocationId >= view.header.nAllocationCount || quality.nProfile >= SIR_VT_PACK_QUALITY_PROFILES ||
		     ( iView == 0 && quality.nAllocationId != 0 ) ) {
			return false;
		}
		if ( iView == 0 || quality.nAllocationId != nPreviousAllocation ) {
			if ( iView != 0 ) {
				const sirVirtualTexturePackAllocationRecord_t previous =
				    sirVTReadPackRecord<sirVirtualTexturePackAllocationRecord_t>( view, SIR_VT_PACK_ALLOCATIONS, nPreviousAllocation );
				if ( nSeenMask != previous.nAvailableProfiles || quality.nAllocationId != nPreviousAllocation + 1U ) {
					return false;
				}
			}
			nSeenMask = 0;
			nPreviousProfile = 0;
		} else if ( quality.nProfile <= nPreviousProfile ) {
			return false;
		}
		const sirVirtualTexturePackAllocationRecord_t allocation =
		    sirVTReadPackRecord<sirVirtualTexturePackAllocationRecord_t>( view, SIR_VT_PACK_ALLOCATIONS, quality.nAllocationId );
		const uint32_t nBit = 1U << quality.nProfile;
		const uint32_t nRequired = sirVTRequiredChannelMask( allocation.nConsumer );
		if ( ( allocation.nAvailableProfiles & nBit ) == 0 || ( nSeenMask & nBit ) != 0 || quality.nChannelMask == 0 ||
		     ( quality.nChannelMask & nRequired ) != nRequired || ( quality.nChannelMask & ~allocation.nChannelMask ) != 0 ) {
			return false;
		}
		for ( uint32_t iChannel = 0; iChannel < SIR_VT_PACK_CHANNEL_COUNT; ++iChannel ) {
			const bool bPresent = ( quality.nChannelMask & ( 1U << iChannel ) ) != 0;
			if ( !bPresent ) {
				if ( quality.szBaseMip[iChannel] != SIR_VT_PACK_INVALID_MIP || quality.szMaxMip[iChannel] != SIR_VT_PACK_INVALID_MIP ) {
					return false;
				}
				continue;
			}
			const sirVirtualTexturePackChannelAllocation_t channel = sirVTReadPackRecord<sirVirtualTexturePackChannelAllocation_t>(
			    view, SIR_VT_PACK_CHANNEL_ALLOCATIONS, quality.nAllocationId * SIR_VT_PACK_CHANNEL_COUNT + iChannel );
			if ( quality.szBaseMip[iChannel] == SIR_VT_PACK_INVALID_MIP || quality.szMaxMip[iChannel] == SIR_VT_PACK_INVALID_MIP ||
			     quality.szBaseMip[iChannel] > quality.szMaxMip[iChannel] || quality.szMaxMip[iChannel] >= channel.nMipCount ) {
				return false;
			}
		}
		nSeenMask |= nBit;
		nPreviousAllocation = quality.nAllocationId;
		nPreviousProfile = quality.nProfile;
	}
	const sirVirtualTexturePackAllocationRecord_t last =
	    sirVTReadPackRecord<sirVirtualTexturePackAllocationRecord_t>( view, SIR_VT_PACK_ALLOCATIONS, nPreviousAllocation );
	return nSeenMask == last.nAvailableProfiles;
}

bool sirVTPackOpenValidated( const sirVirtualTexturePackInput_t& input, sirVirtualTexturePackView_t *pView ) noexcept
{
	if ( pView == nullptr ) {
		return false;
	}
	*pView = {};
	sirVirtualTexturePackView_t candidate = {};
	if ( !sirVTPackOpenDirectories( input, &candidate ) || !sirVTValidatePackFormats( candidate ) ) {
		return false;
	}
	uint32_t nProfileMask = 0;
	if ( !sirVTValidatePackAllocations( candidate, &nProfileMask ) || nProfileMask != candidate.header.nFullySupportedProfileMask ||
	     !sirVTValidatePackPages( candidate ) || !sirVTValidatePackPayloads( candidate ) || !sirVTValidatePackClusters( candidate ) ||
	     !sirVTValidatePackTails( candidate ) || !sirVTValidatePackQualityViews( candidate ) ) {
		return false;
	}
	uint32_t szPageCounts[SIR_VT_PACK_CHANNEL_COUNT] = {};
	uint32_t szTailCounts[SIR_VT_PACK_CHANNEL_COUNT] = {};
	for ( uint32_t iPage = 0; iPage < candidate.header.nPageCount; ++iPage ) {
		const sirVirtualTexturePackPageRecord_t page =
		    sirVTReadPackRecord<sirVirtualTexturePackPageRecord_t>( candidate, SIR_VT_PACK_PAGES, iPage );
		++szPageCounts[page.nChannel];
	}
	for ( uint32_t iTail = 0; iTail < candidate.header.nTailCount; ++iTail ) {
		const sirVirtualTexturePackTailRecord_t tail =
		    sirVTReadPackRecord<sirVirtualTexturePackTailRecord_t>( candidate, SIR_VT_PACK_TAILS, iTail );
		++szTailCounts[tail.nChannel];
	}
	for ( uint32_t iChannel = 0; iChannel < SIR_VT_PACK_CHANNEL_COUNT; ++iChannel ) {
		if ( szPageCounts[iChannel] != candidate.header.szChannelPageCount[iChannel] ||
		     szTailCounts[iChannel] != candidate.header.szChannelTailCount[iChannel] ) {
			return false;
		}
	}
	if ( !candidate.bMetadataOnly ) {
		for ( uint32_t iCluster = 0; iCluster < candidate.header.nClusterCount; ++iCluster ) {
			const sirVirtualTexturePackClusterRecord_t cluster =
			    sirVTReadPackRecord<sirVirtualTexturePackClusterRecord_t>( candidate, SIR_VT_PACK_CLUSTERS, iCluster );
			if ( cluster.nCodec == 0 ) {
				const uint8_t *pBytes = candidate.szDirectories[SIR_VT_PACK_PAYLOAD_DATA].pData + cluster.nPayloadDataOffset;
				if ( sirVTPackChecksum32( pBytes, cluster.nStoredBytes ) != cluster.nChecksum ||
				     sirVTPackHash64( pBytes, cluster.nStoredBytes ) != cluster.nContentSignature ) {
					return false;
				}
			}
		}
	}
	*pView = candidate;
	return true;
}

bool sirVTPackGetPage( const sirVirtualTexturePackView_t& view, uint32_t nIndex, sirVirtualTexturePackPageRecord_t *pPage ) noexcept
{
	if ( pPage == nullptr || nIndex >= view.header.nPageCount || view.szDirectories[SIR_VT_PACK_PAGES].pData == nullptr ) {
		return false;
	}
	*pPage = sirVTReadPackRecord<sirVirtualTexturePackPageRecord_t>( view, SIR_VT_PACK_PAGES, nIndex );
	return true;
}
