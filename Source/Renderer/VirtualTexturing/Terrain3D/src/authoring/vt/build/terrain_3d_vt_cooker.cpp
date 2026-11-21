// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "core/utility/terrain_3d_object_bytes.h"
#include "Terrain3DTypes.h"

#include "authoring/vt/build/terrain_3d_vt_cooker.h"
#include "core/utility/terrain_3d_hash_bytes.h"
#include "rendering/rd/vt/runtime/terrain_3d_vt_fixed_profile.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define T3D_VT_COOK_ALIGNMENT 16u
#define T3D_VT_COOK_MAX_RAW_TILE_BYTES ( T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE * 4u )
#define T3D_VT_COOK_MAX_ENCODED_TILE_BYTES ( T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE * 2u )

static bool t3d_vt_cooker_validate_quality_views( const t3dVTCookedSparsePackage_t& rPackage )
{
	if ( rPackage.m_pAllocations == NULL || rPackage.m_nAllocationCount == 0u || rPackage.m_pChannelAllocations == NULL ||
	     rPackage.m_pQualityViews == NULL || rPackage.m_nQualityViewCount == 0u || rPackage.m_nFullySupportedProfileMask == 0u ) {
		return false;
	}
	uint32_t nExpectedViewCount = 0u;
	uint32_t nExpectedFullySupportedMask = ( 1u << T3D_VT_FIXED_PROFILE_COUNT ) - 1u;
	for ( uint32_t iAllocation = 0u; iAllocation < rPackage.m_nAllocationCount; ++iAllocation ) {
		const t3dVTPackAllocationRecord_t& rAllocation = rPackage.m_pAllocations[iAllocation];
		if ( rAllocation.m_nHighestAvailableProfile >= T3D_VT_FIXED_PROFILE_COUNT ) {
			return false;
		}
		nExpectedViewCount += rAllocation.m_nHighestAvailableProfile + 1u;
		nExpectedFullySupportedMask &= rAllocation.m_nAvailableProfileMask;
	}
	if ( nExpectedViewCount != rPackage.m_nQualityViewCount || nExpectedFullySupportedMask != rPackage.m_nFullySupportedProfileMask ) {
		return false;
	}
	uint32_t nViewCursor = 0u;
	for ( uint32_t iAllocation = 0u; iAllocation < rPackage.m_nAllocationCount; ++iAllocation ) {
		const t3dVTPackAllocationRecord_t& rAllocation = rPackage.m_pAllocations[iAllocation];
		const t3dVTFixedProfile_t HighestProfile = ( t3dVTFixedProfile_t )rAllocation.m_nHighestAvailableProfile;
		for ( uint32_t iProfile = 0u; iProfile <= rAllocation.m_nHighestAvailableProfile; ++iProfile ) {
			const t3dVTFixedProfile_t ViewProfile = ( t3dVTFixedProfile_t )iProfile;
			const t3dVTPackQualityView_t& rView = rPackage.m_pQualityViews[nViewCursor++];
			if ( rView.m_nAllocationId != iAllocation || rView.m_nProfile != iProfile ||
			     ( rAllocation.m_nAvailableProfileMask & ( 1u << iProfile ) ) == 0u ) {
				return false;
			}
			uint32_t nExpectedChannelMask = 0u;
			for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
				const uint32_t nBit = T3D_VT_LOGICAL_CHANNEL_BIT( iChannel );
				const uint32_t nTargetExtent = t3d_vt_fixed_profile_channel_extent( ViewProfile, ( t3dVTLogicalChannel_t )iChannel );
				if ( ( rAllocation.m_nChannelMask & nBit ) != 0u && nTargetExtent != 0u ) {
					nExpectedChannelMask |= nBit;
					const t3dVTChannelAllocation_t& rChannelAllocation =
					    rPackage.m_pChannelAllocations[iAllocation * T3D_VT_LOGICAL_CHANNEL_COUNT + iChannel];
					uint8_t nExpectedBaseMip = T3D_VT_PACK_INVALID_MIP;
					if ( !t3d_vt_fixed_profile_quality_base_mip( HighestProfile, ViewProfile, ( t3dVTLogicalChannel_t )iChannel,
					                                             rChannelAllocation.m_nMipCount, nExpectedBaseMip ) ||
					     rView.m_nBaseMip[iChannel] != nExpectedBaseMip ||
					     rView.m_nMaxMip[iChannel] != rChannelAllocation.m_nMipCount - 1u ) {
						return false;
					}
				} else if ( rView.m_nBaseMip[iChannel] != T3D_VT_PACK_INVALID_MIP ||
				            rView.m_nMaxMip[iChannel] != T3D_VT_PACK_INVALID_MIP ) {
					return false;
				}
			}
			if ( rView.m_nChannelMask != nExpectedChannelMask ) {
				return false;
			}
		}
	}
	return nViewCursor == rPackage.m_nQualityViewCount;
}

// t3dVTCookStagedPayload_s centralized in Terrain3DTypes.h.

static uint32_t t3d_vt_cooker_next_power_of_two( uint32_t nValue )
{
	if ( nValue <= 1u ) {
		return 1u;
	}
	--nValue;
	nValue |= nValue >> 1u;
	nValue |= nValue >> 2u;
	nValue |= nValue >> 4u;
	nValue |= nValue >> 8u;
	nValue |= nValue >> 16u;
	return nValue == UINT32_MAX ? 0u : nValue + 1u;
}

static uint32_t t3d_vt_cooker_fold_signature( const uint64_t nSignature )
{
	const uint32_t nFolded = t3dHashAvalanche32( ( uint32_t )nSignature ^ ( uint32_t )( nSignature >> 32u ) );
	return nFolded == T3D_VT_INVALID_PAGE ? nFolded - 1u : nFolded;
}

static void *t3d_vt_cooker_allocate( const t3dVTCookAllocator_t& rAllocator, const uint64_t nBytes )
{
	if ( rAllocator.m_pAllocate == NULL || nBytes == 0u || nBytes > SIZE_MAX ) {
		return NULL;
	}
	return rAllocator.m_pAllocate( rAllocator.m_pUser, nBytes, T3D_VT_COOK_ALIGNMENT );
}

static void *t3d_vt_cooker_allocate_zero( const t3dVTCookAllocator_t& rAllocator, const uint64_t nBytes )
{
	void *pMemory = t3d_vt_cooker_allocate( rAllocator, nBytes );
	if ( pMemory != NULL ) {
		memset( pMemory, 0, ( size_t )nBytes );
	}
	return pMemory;
}

static void t3d_vt_cooker_free( const t3dVTCookAllocator_t& rAllocator, void *pMemory )
{
	if ( pMemory != NULL && rAllocator.m_pFree != NULL ) {
		rAllocator.m_pFree( rAllocator.m_pUser, pMemory );
	}
}

bool t3d_vt_cooker_normalize_channel_to_profile( const t3dVTChannelNormalization_t& rRequest, const t3dVTCookAllocator_t& rAllocator,
                                                 uint8_t *pDestination, const uint64_t nDestinationBytes )
{
	const t3dVTCookImageView_t& rSource = rRequest.m_source;
	const t3dVTLogicalChannel_t Channel = rRequest.m_channel;
	const uint32_t nSourceLayout = rRequest.m_nSourceLayout;
	const uint32_t nTargetExtent = rRequest.m_nTargetExtent;
	const bool bAlphaScissor = rRequest.m_bAlphaScissor;
	const uint8_t nAlphaThreshold = rRequest.m_nAlphaThreshold;
	const uint32_t nComponents = t3d_vt_page_builder_logical_channel_components( Channel );
	if ( nComponents == 0u || nTargetExtent == 0u || nTargetExtent > T3D_VT_MAX_AUTHORED_EXTENT || pDestination == NULL ||
	     rAllocator.m_pAllocate == NULL || rAllocator.m_pFree == NULL || rSource.m_pPixels == NULL || rSource.m_nWidth < nTargetExtent ||
	     rSource.m_nHeight < nTargetExtent ) {
		return false;
	}
	const uint64_t nTargetBytes = ( uint64_t )nTargetExtent * nTargetExtent * nComponents;
	if ( nDestinationBytes < nTargetBytes ) {
		return false;
	}

	const uint32_t nCappedWidth = rSource.m_nWidth > T3D_VT_MAX_AUTHORED_EXTENT ? T3D_VT_MAX_AUTHORED_EXTENT : rSource.m_nWidth;
	const uint32_t nCappedHeight = rSource.m_nHeight > T3D_VT_MAX_AUTHORED_EXTENT ? T3D_VT_MAX_AUTHORED_EXTENT : rSource.m_nHeight;
	if ( nCappedWidth < nTargetExtent || nCappedHeight < nTargetExtent ) {
		return false;
	}

	if ( rSource.m_nWidth <= T3D_VT_MAX_AUTHORED_EXTENT && rSource.m_nHeight <= T3D_VT_MAX_AUTHORED_EXTENT ) {
		return t3d_vt_page_builder_resample_logical_channel_u8( Channel, nSourceLayout, bAlphaScissor, nAlphaThreshold, rSource,
		                                                        nTargetExtent, nTargetExtent, pDestination, nDestinationBytes );
	}

	const uint64_t nCappedBytes = ( uint64_t )nCappedWidth * nCappedHeight * nComponents;
	uint8_t *const pCapped = ( uint8_t * )t3d_vt_cooker_allocate( rAllocator, nCappedBytes );
	if ( pCapped == NULL ) {
		return false;
	}
	if ( !t3d_vt_page_builder_resample_logical_channel_u8( Channel, nSourceLayout, bAlphaScissor, nAlphaThreshold, rSource, nCappedWidth,
	                                                       nCappedHeight, pCapped, nCappedBytes ) ) {
		t3d_vt_cooker_free( rAllocator, pCapped );
		return false;
	}
	if ( nCappedWidth == nTargetExtent && nCappedHeight == nTargetExtent ) {
		memcpy( pDestination, pCapped, ( size_t )nTargetBytes );
		t3d_vt_cooker_free( rAllocator, pCapped );
		return true;
	}

	t3dVTCookImageView_t CappedView;
	CappedView.m_pPixels = pCapped;
	CappedView.m_nWidth = nCappedWidth;
	CappedView.m_nHeight = nCappedHeight;
	CappedView.m_nComponents = nComponents;
	CappedView.m_nRowBytes = nCappedWidth * nComponents;
	const bool bResult =
	    t3d_vt_page_builder_resample_logical_channel_u8( Channel, T3D_VT_SURFACE_SOURCE_ORM, bAlphaScissor, nAlphaThreshold, CappedView,
		                                                 nTargetExtent, nTargetExtent, pDestination, nDestinationBytes );
	t3d_vt_cooker_free( rAllocator, pCapped );
	return bResult;
}
static uint64_t t3d_vt_cooker_payload_signature( const uint32_t nFormat, const uint8_t *pBytes, const uint32_t nBytes )
{
	uint64_t nHash = t3d_vt_pack_hash64( pBytes, nBytes );
	nHash = t3dHashFnv1a64LittleU64( nHash, nFormat );
	nHash = t3dHashFnv1a64LittleU64( nHash, nBytes );
	return nHash;
}

typedef struct t3dVTCookPayloadTable_s
{
	const t3dVTCookAllocator_t& m_allocator;
	t3dVTCookStagedPayload_t *m_pPayloads;
	uint32_t m_nCapacity;
	uint32_t& m_nCount;
	uint32_t *m_pHashTable;
	uint32_t m_nHashCapacity;
} t3dVTCookPayloadTable_t;

typedef struct t3dVTCookPayloadBytes_s
{
	uint32_t m_nFormat;
	uint32_t m_nFlags;
	const uint8_t *m_pBytes;
	uint32_t m_nBytes;
} t3dVTCookPayloadBytes_t;

static bool t3d_vt_cooker_add_payload( t3dVTCookPayloadTable_t& rTable, const t3dVTCookPayloadBytes_t& rRequest, uint32_t& rPayloadId )
{
	const t3dVTCookAllocator_t& rAllocator = rTable.m_allocator;
	t3dVTCookStagedPayload_t *const pPayloads = rTable.m_pPayloads;
	const uint32_t nPayloadCapacity = rTable.m_nCapacity;
	uint32_t& rPayloadCount = rTable.m_nCount;
	uint32_t *const pHashTable = rTable.m_pHashTable;
	const uint32_t nHashCapacity = rTable.m_nHashCapacity;
	const uint32_t nFormat = rRequest.m_nFormat;
	const uint32_t nFlags = rRequest.m_nFlags;
	const uint8_t *const pBytes = rRequest.m_pBytes;
	const uint32_t nBytes = rRequest.m_nBytes;
	if ( pPayloads == NULL || pHashTable == NULL || nHashCapacity == 0u || pBytes == NULL || nBytes == 0u ) {
		return false;
	}
	const uint64_t nSignature = t3d_vt_cooker_payload_signature( nFormat, pBytes, nBytes );
	uint32_t nSlot = ( uint32_t )( nSignature ^ ( nSignature >> 32u ) ) & ( nHashCapacity - 1u );
	for ( uint32_t iProbe = 0u; iProbe < nHashCapacity; ++iProbe ) {
		const uint32_t nEntry = pHashTable[nSlot];
		if ( nEntry == 0u ) {
			if ( rPayloadCount >= nPayloadCapacity ) {
				return false;
			}
			uint8_t *pOwnedBytes = ( uint8_t * )t3d_vt_cooker_allocate( rAllocator, nBytes );
			if ( pOwnedBytes == NULL ) {
				return false;
			}
			memcpy( pOwnedBytes, pBytes, nBytes );
			t3dVTCookStagedPayload_t& rPayload = pPayloads[rPayloadCount];
			memset( &rPayload, 0, sizeof( rPayload ) );
			rPayload.m_nContentSignature = nSignature;
			rPayload.m_pBytes = pOwnedBytes;
			rPayload.m_nBytes = nBytes;
			rPayload.m_nFormat = nFormat;
			rPayload.m_nFlags = nFlags;
			rPayload.m_nChecksum = t3d_vt_pack_checksum32( pBytes, nBytes );
			rPayloadId = rPayloadCount;
			pHashTable[nSlot] = rPayloadCount + 1u;
			++rPayloadCount;
			return true;
		}
		const uint32_t nPayloadId = nEntry - 1u;
		const t3dVTCookStagedPayload_t& rPayload = pPayloads[nPayloadId];
		if ( rPayload.m_nContentSignature == nSignature && rPayload.m_nFormat == nFormat && rPayload.m_nBytes == nBytes &&
		     memcmp( rPayload.m_pBytes, pBytes, nBytes ) == 0 ) {
			pPayloads[nPayloadId].m_nFlags |= nFlags;
			rPayloadId = nPayloadId;
			return true;
		}
		nSlot = ( nSlot + 1u ) & ( nHashCapacity - 1u );
	}
	return false;
}

// COMPOSED roots begin at 32 texels. Their horizontal mip chain occupies
// at most 64x32, allowing two roots per existing 128x64 atlas slot. Fine
// 64x64 pages stay in the normal streamed V1 page hierarchy; the root is
// always resident via the shared tail atlas. Other consumers retain V1 layout.
static uint32_t t3d_vt_cooker_tail_first_mip( uint32_t nWidth, uint32_t nHeight, const uint32_t nMipCount, const t3dVTConsumer_t eConsumer )
{
	const uint32_t nRootExtent = eConsumer == T3D_VT_CONSUMER_COMPOSED ? 32u : 64u;
	for ( uint32_t iMip = 0u; iMip < nMipCount; ++iMip ) {
		if ( nWidth <= nRootExtent && nHeight <= nRootExtent ) {
			return iMip;
		}
		nWidth = nWidth > 1u ? nWidth >> 1u : 1u;
		nHeight = nHeight > 1u ? nHeight >> 1u : 1u;
	}
	return nMipCount - 1u;
}
void t3d_vt_cooker_default_settings( t3dVTCookerSettings_t& rSettings )
{
	memset( &rSettings, 0, sizeof( rSettings ) );
	rSettings.m_nPageInterior = T3D_VT_PAGE_INTERIOR;
	rSettings.m_nPageGutter = T3D_VT_PAGE_GUTTER;
	rSettings.m_nClusterTargetPages = 64u;
	rSettings.m_nClusterTargetBytes = 4u * 1024u * 1024u;
	rSettings.m_nOuterCompression = T3D_VT_OUTER_COMPRESSION_NONE;
	rSettings.m_nMipQuality = 1u;
	rSettings.m_nCompressionQuality = 2u;
}

bool t3d_vt_cooker_validate_settings( const t3dVTCookerSettings_t& rSettings )
{
	return rSettings.m_nPageInterior == T3D_VT_PAGE_INTERIOR && rSettings.m_nPageGutter == T3D_VT_PAGE_GUTTER &&
	       rSettings.m_nClusterTargetPages > 0u && rSettings.m_nClusterTargetBytes > 0u &&
	       rSettings.m_nOuterCompression <= T3D_VT_OUTER_COMPRESSION_ZSTD && rSettings.m_nMipQuality <= 3u &&
	       rSettings.m_nCompressionQuality <= 3u;
}
bool t3d_vt_cooker_encode_tile( const t3dVTEncoderInterface_t& rEncoder, const t3dVTEncodeTileRequest_t& rRequest, uint32_t& rWrittenBytes )
{
	rWrittenBytes = 0u;
	if ( rRequest.m_nChannel >= T3D_VT_LOGICAL_CHANNEL_COUNT ) {
		return false;
	}
	const t3dVTLogicalChannel_t Channel = ( t3dVTLogicalChannel_t )rRequest.m_nChannel;
	const uint32_t nExpectedFamily = rRequest.m_nFormat;
	const uint32_t nExpectedBytes = t3d_vt_pack_expected_physical_tile_bytes( nExpectedFamily );
	const uint32_t nExpectedComponents = t3d_vt_page_builder_logical_channel_components( Channel );
	if ( rEncoder.m_pEncodeTile == NULL || rRequest.m_pSourcePixels == NULL || rRequest.m_pDestination == NULL ||
	     rRequest.m_nContractVersion != T3D_VT_FILE_FORMAT_VERSION ||
	     ( nExpectedFamily != ( uint32_t )t3d_vt_logical_channel_physical_family( Channel ) &&
	       !( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO && nExpectedFamily == T3D_VT_PHYSICAL_FORMAT_BC7_UNORM ) ) ||
	     rRequest.m_nWidth != T3D_VT_PHYSICAL_TILE || rRequest.m_nHeight != T3D_VT_PHYSICAL_TILE ||
	     rRequest.m_nComponents != nExpectedComponents || nExpectedComponents == 0u ||
	     rRequest.m_nSourceBytes != T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE * nExpectedComponents ||
	     rRequest.m_nDestinationCapacity < nExpectedBytes || nExpectedBytes == 0u ) {
		return false;
	}
	uint32_t nWrittenBytes = 0u;
	if ( !rEncoder.m_pEncodeTile( rEncoder.m_pUser, rRequest, nWrittenBytes ) || nWrittenBytes != nExpectedBytes ) {
		return false;
	}
	rWrittenBytes = nWrittenBytes;
	return true;
}
bool t3d_vt_cooker_build_pack_source( const t3dVTCookedSparsePackage_t& rPackage, t3dVTSerializedPackSource_t& rSource )
{
	memset( &rSource, 0, sizeof( rSource ) );
	if ( rPackage.m_pAllocations == NULL || rPackage.m_nAllocationCount == 0u || rPackage.m_pAllocationKeys == NULL ||
	     rPackage.m_pChannelAllocations == NULL || rPackage.m_nChannelAllocationCount == 0u || rPackage.m_pPayloads == NULL ||
	     rPackage.m_nPayloadCount == 0u || rPackage.m_pClusters == NULL || rPackage.m_nClusterCount == 0u || rPackage.m_pTails == NULL ||
	     rPackage.m_nTailCount == 0u || rPackage.m_pQualityViews == NULL || rPackage.m_nQualityViewCount == 0u ||
	     rPackage.m_pPayloadData == NULL || rPackage.m_nPayloadDataBytes == 0u || rPackage.m_nFullySupportedProfileMask == 0u ) {
		return false;
	}
	rSource.m_pAllocations = rPackage.m_pAllocations;
	rSource.m_nAllocationCount = rPackage.m_nAllocationCount;
	rSource.m_pAllocationKeys = rPackage.m_pAllocationKeys;
	rSource.m_pChannelAllocations = rPackage.m_pChannelAllocations;
	rSource.m_nChannelAllocationCount = rPackage.m_nChannelAllocationCount;
	rSource.m_pPages = rPackage.m_pPages;
	rSource.m_nPageCount = rPackage.m_nPageCount;
	rSource.m_pPayloads = rPackage.m_pPayloads;
	rSource.m_nPayloadCount = rPackage.m_nPayloadCount;
	rSource.m_pClusters = rPackage.m_pClusters;
	rSource.m_nClusterCount = rPackage.m_nClusterCount;
	rSource.m_pTails = rPackage.m_pTails;
	rSource.m_nTailCount = rPackage.m_nTailCount;
	rSource.m_pQualityViews = rPackage.m_pQualityViews;
	rSource.m_nQualityViewCount = rPackage.m_nQualityViewCount;
	rSource.m_pFormatFamilies = rPackage.m_FormatFamilies;
	rSource.m_nFormatFamilyCount = T3D_VT_LOGICAL_CHANNEL_COUNT;
	rSource.m_pPayloadData = rPackage.m_pPayloadData;
	rSource.m_nPayloadDataBytes = rPackage.m_nPayloadDataBytes;
	rSource.m_nPackageGeneration = rPackage.m_nPackageGeneration;
	return t3d_vt_cooker_validate_quality_views( rPackage ) && t3d_vt_pack_validate_source( rSource );
}

// -----------------------------------------------------------------------------
// VT-P04 sparse V1 page build. This is a cold authoring path only. Shipping
// VT-P09 publishes this V1 source for shipping/export; P05 owns the quality-view directory.

// t3dVTCookRawTileCacheEntry_s centralized in Terrain3DTypes.h.

static uint64_t t3d_vt_cooker_raw_hash( const uint8_t *pBytes, const uint32_t nBytes, const uint32_t nFamily, const uint32_t nChannel,
                                        const uint64_t nSeed )
{
	uint64_t nHash = t3dHashFnv1a64( pBytes, nBytes, nSeed );
	nHash = t3dHashFnv1a64LittleU64( nHash, nFamily );
	nHash = t3dHashFnv1a64LittleU64( nHash, nChannel );
	nHash = t3dHashFnv1a64LittleU64( nHash, nBytes );
	return nHash;
}

static bool t3d_vt_cooker_find_raw_tile( const t3dVTCookRawTileCacheEntry_t *pEntries, const uint32_t nCount, const uint64_t nHashA,
                                         const uint64_t nHashB, const uint32_t nRawBytes, const uint32_t nFamily, const uint32_t nChannel,
                                         uint32_t& rPayloadId )
{
	for ( uint32_t i = 0u; i < nCount; ++i ) {
		const t3dVTCookRawTileCacheEntry_t& rEntry = pEntries[i];
		if ( rEntry.m_nHashA == nHashA && rEntry.m_nHashB == nHashB && rEntry.m_nRawBytes == nRawBytes && rEntry.m_nFamily == nFamily &&
		     rEntry.m_nChannel == nChannel ) {
			rPayloadId = rEntry.m_nPayloadId;
			return true;
		}
	}
	return false;
}

typedef struct t3dVTCookTileTable_s
{
	t3dVTCookPayloadTable_t m_payloads;
	const t3dVTEncoderInterface_t& m_encoder;
	t3dVTCookRawTileCacheEntry_t *m_pRawCache;
	uint32_t m_nRawCacheCapacity;
	uint32_t& m_nRawCacheCount;
	uint8_t *m_pEncodedBytes;
	t3dVTCookSparseStats_t& m_stats;
} t3dVTCookTileTable_t;

typedef struct t3dVTCookTileRequest_s
{
	t3dVTLogicalChannel_t m_channel;
	uint32_t m_nPayloadFlags;
	uint16_t m_nAllocationFlags;
	uint8_t m_nAlphaThreshold;
	uint32_t m_nCompressionQuality;
	const uint8_t *m_pTileBytes;
} t3dVTCookTileRequest_t;

static bool t3d_vt_cooker_encode_and_add_payload( t3dVTCookTileTable_t& rTable, const t3dVTCookTileRequest_t& rRequest,
                                                  uint32_t& rPayloadId )
{
	const t3dVTEncoderInterface_t& rEncoder = rTable.m_encoder;
	t3dVTCookStagedPayload_t *const pStagedPayloads = rTable.m_payloads.m_pPayloads;
	t3dVTCookRawTileCacheEntry_t *const pRawCache = rTable.m_pRawCache;
	const uint32_t nRawCacheCapacity = rTable.m_nRawCacheCapacity;
	uint32_t& rRawCacheCount = rTable.m_nRawCacheCount;
	uint8_t *const pEncodedBytes = rTable.m_pEncodedBytes;
	t3dVTCookSparseStats_t& rStats = rTable.m_stats;
	const t3dVTLogicalChannel_t Channel = rRequest.m_channel;
	const uint32_t nPayloadFlags = rRequest.m_nPayloadFlags;
	const uint32_t nCompressionQuality = rRequest.m_nCompressionQuality;
	const uint8_t *const pTileBytes = rRequest.m_pTileBytes;
	const uint32_t nComponents = t3d_vt_page_builder_logical_channel_components( Channel );
	const uint32_t nRawBytes = T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE * nComponents;
	const uint32_t nFamily = ( uint32_t )t3d_vt_logical_channel_physical_family( Channel, rRequest.m_nAllocationFlags );
	const uint32_t nChannel = ( uint32_t )Channel;
	// Compressed output also depends on alpha cutoff and encoder quality.
	const uint64_t nEncoderIdentity = ( ( uint64_t )rRequest.m_nAlphaThreshold << 32u ) | nCompressionQuality;
	const uint64_t nHashA = t3d_vt_cooker_raw_hash( pTileBytes, nRawBytes, nFamily, nChannel,
	                                                t3dHashFnv1a64LittleU64( 1469598103934665603ull, nEncoderIdentity ) );
	const uint64_t nHashB = t3d_vt_cooker_raw_hash( pTileBytes, nRawBytes, nFamily, nChannel,
	                                                t3dHashFnv1a64LittleU64( 7809847782465536322ull, nEncoderIdentity ) );
	if ( t3d_vt_cooker_find_raw_tile( pRawCache, rRawCacheCount, nHashA, nHashB, nRawBytes, nFamily, nChannel, rPayloadId ) ) {
		pStagedPayloads[rPayloadId].m_nFlags |= nPayloadFlags;
		++rStats.m_nRawDedupHits;
		return true;
	}
	if ( rRawCacheCount >= nRawCacheCapacity ) {
		return false;
	}

	t3dVTEncodeTileRequest_t Request;
	t3dClearTrivialObjectBytes( Request );
	Request.m_pSourcePixels = pTileBytes;
	Request.m_nSourceBytes = nRawBytes;
	Request.m_nChannel = ( uint32_t )Channel;
	Request.m_nFormat = nFamily;
	Request.m_nWidth = T3D_VT_PHYSICAL_TILE;
	Request.m_nHeight = T3D_VT_PHYSICAL_TILE;
	Request.m_nComponents = nComponents;
	Request.m_nQuality = nCompressionQuality;
	Request.m_nContractVersion = T3D_VT_FILE_FORMAT_VERSION;
	Request.m_nAlphaThreshold = rRequest.m_nAlphaThreshold;
	Request.m_pDestination = pEncodedBytes;
	Request.m_nDestinationCapacity = T3D_VT_COOK_MAX_ENCODED_TILE_BYTES;
	uint32_t nWrittenBytes = 0u;
	if ( !t3d_vt_cooker_encode_tile( rEncoder, Request, nWrittenBytes ) ) {
		return false;
	}
	++rStats.m_nEncodeCalls;
	t3dVTCookPayloadBytes_t Payload;
	Payload.m_nFormat = nFamily;
	Payload.m_nFlags = nPayloadFlags;
	Payload.m_pBytes = pEncodedBytes;
	Payload.m_nBytes = nWrittenBytes;
	if ( !t3d_vt_cooker_add_payload( rTable.m_payloads, Payload, rPayloadId ) ) {
		return false;
	}
	t3dVTCookRawTileCacheEntry_t& rEntry = pRawCache[rRawCacheCount++];
	rEntry.m_nHashA = nHashA;
	rEntry.m_nHashB = nHashB;
	rEntry.m_nRawBytes = nRawBytes;
	rEntry.m_nFamily = nFamily;
	rEntry.m_nChannel = nChannel;
	rEntry.m_nPayloadId = rPayloadId;
	rEntry.m_nFlags = nPayloadFlags;
	return true;
}

static uint32_t t3d_vt_cooker_source_extent( const t3dVTCookImageView_t& rView )
{
	uint32_t nExtent = rView.m_nWidth < rView.m_nHeight ? rView.m_nWidth : rView.m_nHeight;
	return nExtent > T3D_VT_MAX_AUTHORED_EXTENT ? T3D_VT_MAX_AUTHORED_EXTENT : nExtent;
}

static uint32_t t3d_vt_cooker_sparse_enabled_mask( const t3dVTCookSparseSource_t& rSource, const t3dVTFixedProfile_t Profile )
{
	uint32_t nMask = 0u;
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		if ( ( rSource.m_nAuthoredChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( iChannel ) ) != 0u &&
		     t3d_vt_fixed_profile_channel_extent( Profile, ( t3dVTLogicalChannel_t )iChannel ) != 0u ) {
			nMask |= T3D_VT_LOGICAL_CHANNEL_BIT( iChannel );
		}
	}
	return nMask;
}

static uint32_t t3d_vt_cooker_regular_page_count( const uint32_t nExtent, const uint32_t nTailFirstMip )
{
	uint32_t nWidth = nExtent;
	uint32_t nHeight = nExtent;
	uint64_t nPages = 0u;
	for ( uint32_t iMip = 0u; iMip < nTailFirstMip; ++iMip ) {
		nPages += ( uint64_t )t3d_vt_page_builder_pages_wide( nWidth ) * t3d_vt_page_builder_pages_high( nHeight );
		nWidth = nWidth > 1u ? nWidth >> 1u : 1u;
		nHeight = nHeight > 1u ? nHeight >> 1u : 1u;
	}
	return nPages > UINT32_MAX ? 0u : ( uint32_t )nPages;
}

static bool t3d_vt_cooker_validate_sparse_source( const t3dVTCookSparseSource_t& rSource, t3dVTFixedProfile_t& rProfile,
                                                  uint32_t& rEnabledMask )
{
	if ( rSource.m_nStableKey == 0u || rSource.m_nConsumer > T3D_VT_CONSUMER_COMPOSED || rSource.m_nWrapMode > T3D_VT_COOK_WRAP_CLAMP ||
	     rSource.m_nOrmSourceLayout > T3D_VT_SURFACE_SOURCE_ROUGHNESS_A ) {
		return false;
	}
	uint32_t SourceExtent[T3D_VT_LOGICAL_CHANNEL_COUNT];
	memset( SourceExtent, 0, sizeof( SourceExtent ) );
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		if ( ( rSource.m_nAuthoredChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( iChannel ) ) == 0u ) {
			continue;
		}
		const t3dVTCookImageView_t& rView = rSource.m_Channels[iChannel];
		if ( rView.m_pPixels == NULL || rView.m_nWidth == 0u || rView.m_nHeight == 0u || rView.m_nComponents == 0u ||
		     rView.m_nRowBytes < rView.m_nWidth * rView.m_nComponents ) {
			return false;
		}
		SourceExtent[iChannel] = t3d_vt_cooker_source_extent( rView );
	}
	rProfile =
	    t3d_vt_fixed_profile_highest_supported( ( t3dVTConsumer_t )rSource.m_nConsumer, rSource.m_nAuthoredChannelMask, SourceExtent );
	if ( rProfile == T3D_VT_FIXED_PROFILE_INVALID ) {
		return false;
	}
	rEnabledMask = t3d_vt_cooker_sparse_enabled_mask( rSource, rProfile );
	const uint32_t nRequiredMask = t3d_vt_consumer_required_mask( ( t3dVTConsumer_t )rSource.m_nConsumer );
	return ( rEnabledMask & nRequiredMask ) == nRequiredMask;
}

bool t3d_vt_cooker_build_channel_pyramid( const t3dVTCookAllocator_t& rAllocator, const t3dVTCookSparseSource_t& rSource,
                                          const t3dVTFixedProfile_t Profile, const t3dVTLogicalChannel_t Channel, uint8_t *& rpMipBytes,
                                          t3dVTMipChainLayout_t& rLayout, const uint32_t nMipQuality )
{
	rpMipBytes = NULL;
	memset( &rLayout, 0, sizeof( rLayout ) );
	const uint32_t nExtent = t3d_vt_fixed_profile_channel_extent( Profile, Channel );
	const uint32_t nComponents = t3d_vt_page_builder_logical_channel_components( Channel );
	if ( nExtent == 0u || nComponents == 0u || !t3d_vt_page_builder_layout_mips( nExtent, nExtent, nComponents, rLayout ) ) {
		return false;
	}
	const t3dVTCookPreparedChannel_t& rPrepared = rSource.m_PreparedChannels[( uint32_t )Channel];
	if ( rPrepared.m_pMipBytes != NULL && rPrepared.m_nTargetExtent == nExtent && rPrepared.m_Layout.m_nMipCount == rLayout.m_nMipCount &&
	     rPrepared.m_Layout.m_nComponents == rLayout.m_nComponents && rPrepared.m_Layout.m_nTotalBytes == rLayout.m_nTotalBytes ) {
		for ( uint32_t iMip = 0u; iMip < rLayout.m_nMipCount; ++iMip ) {
			const t3dVTMipLevelLayout_t& a = rPrepared.m_Layout.m_Levels[iMip];
			const t3dVTMipLevelLayout_t& b = rLayout.m_Levels[iMip];
			if ( a.m_nWidth != b.m_nWidth || a.m_nHeight != b.m_nHeight || a.m_nOffset != b.m_nOffset || a.m_nBytes != b.m_nBytes ) {
				return false;
			}
		}
		rpMipBytes = ( uint8_t * )t3d_vt_cooker_allocate( rAllocator, rLayout.m_nTotalBytes );
		if ( rpMipBytes == NULL ) {
			return false;
		}
		memcpy( rpMipBytes, rPrepared.m_pMipBytes, ( size_t )rLayout.m_nTotalBytes );
		return true;
	}
	rpMipBytes = ( uint8_t * )t3d_vt_cooker_allocate( rAllocator, rLayout.m_nTotalBytes );
	if ( rpMipBytes == NULL ) {
		return false;
	}
	const bool bAlphaScissor = Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO && rSource.m_bAlphaScissor != 0u;
	t3dVTChannelNormalization_t Normalization;
	Normalization.m_source = rSource.m_Channels[( uint32_t )Channel];
	Normalization.m_channel = Channel;
	Normalization.m_nSourceLayout = rSource.m_nOrmSourceLayout;
	Normalization.m_nTargetExtent = nExtent;
	Normalization.m_bAlphaScissor = bAlphaScissor;
	Normalization.m_nAlphaThreshold = rSource.m_nAlphaThreshold;
	if ( !t3d_vt_cooker_normalize_channel_to_profile( Normalization, rAllocator, rpMipBytes + rLayout.m_Levels[0].m_nOffset,
	                                                  rLayout.m_Levels[0].m_nBytes ) ||
	     !t3d_vt_page_builder_build_logical_channel_mips_u8( Channel, bAlphaScissor, rSource.m_nAlphaThreshold, rpMipBytes,
	                                                         rLayout.m_nTotalBytes, rLayout, nMipQuality ) ) {
		t3d_vt_cooker_free( rAllocator, rpMipBytes );
		rpMipBytes = NULL;
		return false;
	}
	return true;
}

static bool t3d_vt_cooker_finalize_sparse_payloads( const t3dVTCookerSettings_t& rSettings, const t3dVTCookAllocator_t& rAllocator,
                                                    const t3dVTCookClusterCodec_t *pClusterCodec, t3dVTCookStagedPayload_t *pStaged,
                                                    const uint32_t nPayloadCount, t3dVTCookedSparsePackage_t& rPackage )
{
	if ( pStaged == NULL || nPayloadCount == 0u ) {
		return false;
	}
	uint64_t nDecodedBytes64 = 0u;
	for ( uint32_t iPayload = 0u; iPayload < nPayloadCount; ++iPayload ) {
		nDecodedBytes64 += pStaged[iPayload].m_nBytes;
		if ( nDecodedBytes64 > UINT32_MAX ) {
			return false;
		}
	}
	const uint32_t nDecodedBytes = ( uint32_t )nDecodedBytes64;
	rPackage.m_pPayloads = ( t3dVTPackPayloadRecord_t * )t3d_vt_cooker_allocate_zero( rAllocator, ( uint64_t )nPayloadCount *
	                                                                                                  sizeof( t3dVTPackPayloadRecord_t ) );
	rPackage.m_pClusters = ( t3dVTPackClusterRecord_t * )t3d_vt_cooker_allocate_zero( rAllocator, sizeof( t3dVTPackClusterRecord_t ) );
	uint8_t *pDecoded = ( uint8_t * )t3d_vt_cooker_allocate( rAllocator, nDecodedBytes );
	if ( rPackage.m_pPayloads == NULL || rPackage.m_pClusters == NULL || pDecoded == NULL ) {
		t3d_vt_cooker_free( rAllocator, pDecoded );
		return false;
	}
	uint32_t nOffset = 0u;
	for ( uint32_t iPayload = 0u; iPayload < nPayloadCount; ++iPayload ) {
		const t3dVTCookStagedPayload_t& rStaged = pStaged[iPayload];
		t3dVTPackPayloadRecord_t& rPayload = rPackage.m_pPayloads[iPayload];
		memset( &rPayload, 0, sizeof( rPayload ) );
		rPayload.m_nContentSignature = rStaged.m_nContentSignature;
		rPayload.m_nFormat = rStaged.m_nFormat;
		rPayload.m_nClusterId = 0u;
		rPayload.m_nClusterOffset = nOffset;
		rPayload.m_nStoredBytes = rStaged.m_nBytes;
		rPayload.m_nDecodedBytes = rStaged.m_nBytes;
		rPayload.m_nCodec = T3D_VT_PACK_CODEC_NONE;
		rPayload.m_nChecksum = rStaged.m_nChecksum;
		rPayload.m_nFlags = rStaged.m_nFlags;
		memcpy( pDecoded + nOffset, rStaged.m_pBytes, rStaged.m_nBytes );
		nOffset += rStaged.m_nBytes;
		if ( rStaged.m_nFormat < T3D_VT_PHYSICAL_FORMAT_COUNT ) {
			++rPackage.m_Stats.m_nPhysicalFamilyPayloadCount[rStaged.m_nFormat];
		}
	}

	uint64_t nCapacity = nDecodedBytes;
	if ( rSettings.m_nOuterCompression == T3D_VT_OUTER_COMPRESSION_ZSTD ) {
		if ( pClusterCodec == NULL || pClusterCodec->m_pBound == NULL || pClusterCodec->m_pEncode == NULL ) {
			t3d_vt_cooker_free( rAllocator, pDecoded );
			return false;
		}
		nCapacity = pClusterCodec->m_pBound( pClusterCodec->m_pUser, T3D_VT_PACK_CODEC_ZSTD, nDecodedBytes );
		if ( nCapacity == 0u || nCapacity > UINT32_MAX ) {
			t3d_vt_cooker_free( rAllocator, pDecoded );
			return false;
		}
	}
	rPackage.m_pPayloadData = ( uint8_t * )t3d_vt_cooker_allocate( rAllocator, nCapacity );
	if ( rPackage.m_pPayloadData == NULL ) {
		t3d_vt_cooker_free( rAllocator, pDecoded );
		return false;
	}
	uint32_t nStoredBytes = nDecodedBytes;
	uint32_t nCodec = T3D_VT_PACK_CODEC_NONE;
	if ( rSettings.m_nOuterCompression == T3D_VT_OUTER_COMPRESSION_ZSTD ) {
		nCodec = T3D_VT_PACK_CODEC_ZSTD;
		if ( !pClusterCodec->m_pEncode( pClusterCodec->m_pUser, nCodec, pDecoded, nDecodedBytes, rPackage.m_pPayloadData,
		                                ( uint32_t )nCapacity, nStoredBytes ) ||
		     nStoredBytes == 0u || nStoredBytes > nCapacity ) {
			t3d_vt_cooker_free( rAllocator, pDecoded );
			return false;
		}
	} else {
		memcpy( rPackage.m_pPayloadData, pDecoded, nDecodedBytes );
	}
	t3d_vt_cooker_free( rAllocator, pDecoded );

	t3dVTPackClusterRecord_t& rCluster = rPackage.m_pClusters[0];
	memset( &rCluster, 0, sizeof( rCluster ) );
	rCluster.m_nPayloadDataOffset = 0u;
	rCluster.m_nContentSignature = t3d_vt_pack_hash64( rPackage.m_pPayloadData, nStoredBytes );
	rCluster.m_nFirstPayload = 0u;
	rCluster.m_nPayloadCount = nPayloadCount;
	rCluster.m_nStoredBytes = nStoredBytes;
	rCluster.m_nDecodedBytes = nDecodedBytes;
	rCluster.m_nCodec = nCodec;
	rCluster.m_nChecksum = t3d_vt_pack_checksum32( rPackage.m_pPayloadData, nStoredBytes );
	rPackage.m_nPayloadCount = nPayloadCount;
	rPackage.m_nClusterCount = 1u;
	rPackage.m_nPayloadDataBytes = nStoredBytes;
	rPackage.m_Stats.m_nUniquePayloadCount = nPayloadCount;
	return true;
}

static bool t3d_vt_cooker_build_quality_views( const t3dVTCookSparseSource_t *pSources, const uint32_t *pOrder,
                                               const t3dVTFixedProfile_t *pProfiles, const uint32_t nSourceCount,
                                               t3dVTCookedSparsePackage_t& rPackage )
{
	if ( pSources == NULL || pOrder == NULL || pProfiles == NULL || nSourceCount == 0u || rPackage.m_pAllocations == NULL ||
	     rPackage.m_pChannelAllocations == NULL || rPackage.m_pQualityViews == NULL ) {
		return false;
	}
	uint32_t nViewCursor = 0u;
	uint32_t nFullySupportedMask = ( 1u << T3D_VT_FIXED_PROFILE_COUNT ) - 1u;
	for ( uint32_t iAllocation = 0u; iAllocation < nSourceCount; ++iAllocation ) {
		const uint32_t iSource = pOrder[iAllocation];
		const t3dVTCookSparseSource_t& rSource = pSources[iSource];
		const t3dVTFixedProfile_t HighestProfile = pProfiles[iSource];
		const t3dVTPackAllocationRecord_t& rAllocation = rPackage.m_pAllocations[iAllocation];
		nFullySupportedMask &= rAllocation.m_nAvailableProfileMask;
		for ( uint32_t iProfile = 0u; iProfile <= ( uint32_t )HighestProfile; ++iProfile ) {
			if ( nViewCursor >= rPackage.m_nQualityViewCount ) {
				return false;
			}
			const t3dVTFixedProfile_t ViewProfile = ( t3dVTFixedProfile_t )iProfile;
			uint32_t SourceExtent[T3D_VT_LOGICAL_CHANNEL_COUNT];
			memset( SourceExtent, 0, sizeof( SourceExtent ) );
			for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
				if ( ( rSource.m_nAuthoredChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( iChannel ) ) != 0u ) {
					SourceExtent[iChannel] = t3d_vt_cooker_source_extent( rSource.m_Channels[iChannel] );
				}
			}
			if ( !t3d_vt_fixed_profile_source_supports( ViewProfile, ( t3dVTConsumer_t )rSource.m_nConsumer, rSource.m_nAuthoredChannelMask,
			                                            SourceExtent ) ) {
				return false;
			}
			t3dVTPackQualityView_t& rView = rPackage.m_pQualityViews[nViewCursor++];
			memset( &rView, 0xff, sizeof( rView ) );
			rView.m_nAllocationId = iAllocation;
			rView.m_nProfile = ( uint8_t )ViewProfile;
			rView.m_nChannelMask = ( uint8_t )t3d_vt_cooker_sparse_enabled_mask( rSource, ViewProfile );
			if ( ( rView.m_nChannelMask & ~rAllocation.m_nChannelMask ) != 0u ) {
				return false;
			}
			for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
				if ( ( rView.m_nChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( iChannel ) ) == 0u ) {
					rView.m_nBaseMip[iChannel] = T3D_VT_PACK_INVALID_MIP;
					rView.m_nMaxMip[iChannel] = T3D_VT_PACK_INVALID_MIP;
					continue;
				}
				const t3dVTChannelAllocation_t& rChannelAllocation =
				    rPackage.m_pChannelAllocations[iAllocation * T3D_VT_LOGICAL_CHANNEL_COUNT + iChannel];
				uint8_t nBaseMip = T3D_VT_PACK_INVALID_MIP;
				if ( !t3d_vt_fixed_profile_quality_base_mip( HighestProfile, ViewProfile, ( t3dVTLogicalChannel_t )iChannel,
				                                             rChannelAllocation.m_nMipCount, nBaseMip ) ) {
					return false;
				}
				rView.m_nBaseMip[iChannel] = nBaseMip;
				rView.m_nMaxMip[iChannel] = ( uint8_t )( rChannelAllocation.m_nMipCount - 1u );
			}
		}
	}
	if ( nViewCursor != rPackage.m_nQualityViewCount || nFullySupportedMask == 0u ) {
		return false;
	}
	rPackage.m_nFullySupportedProfileMask = nFullySupportedMask;
	return true;
}

void t3d_vt_cooker_release_sparse_package( t3dVTCookedSparsePackage_t& rPackage )
{
	const t3dVTCookAllocator_t Allocator = rPackage.m_Allocator;
	t3d_vt_cooker_free( Allocator, rPackage.m_pAllocations );
	t3d_vt_cooker_free( Allocator, rPackage.m_pAllocationKeys );
	t3d_vt_cooker_free( Allocator, rPackage.m_pChannelAllocations );
	t3d_vt_cooker_free( Allocator, rPackage.m_pPages );
	t3d_vt_cooker_free( Allocator, rPackage.m_pPayloads );
	t3d_vt_cooker_free( Allocator, rPackage.m_pClusters );
	t3d_vt_cooker_free( Allocator, rPackage.m_pTails );
	t3d_vt_cooker_free( Allocator, rPackage.m_pQualityViews );
	t3d_vt_cooker_free( Allocator, rPackage.m_pPayloadData );
	memset( &rPackage, 0, sizeof( rPackage ) );
}

// Cold source planning owns sorting/profile scratch independently of package
// construction. Failed preflight cannot strand one of these allocations.
class Terrain3DVTCookSourcePlan
{
public:
	explicit Terrain3DVTCookSourcePlan( const t3dVTCookAllocator_t& rAllocator ) : m_allocator( rAllocator )
	{
	}
	~Terrain3DVTCookSourcePlan( void )
	{
		t3d_vt_cooker_free( m_allocator, pOrder );
		t3d_vt_cooker_free( m_allocator, pProfiles );
		t3d_vt_cooker_free( m_allocator, pEnabledMask );
	}
	Terrain3DVTCookSourcePlan( const Terrain3DVTCookSourcePlan& ) = delete;
	Terrain3DVTCookSourcePlan& operator=( const Terrain3DVTCookSourcePlan& ) = delete;
	uint32_t *pOrder = NULL;
	t3dVTFixedProfile_t *pProfiles = NULL;
	uint32_t *pEnabledMask = NULL;
	uint32_t nTotalPages = 0u;
	uint32_t nTotalTails = 0u;
	uint32_t nTotalQualityViews = 0u;
	uint32_t nPayloadCapacity = 0u;
	uint32_t nHashCapacity = 0u;
	bool prepare( const t3dVTCookSparseSource_t *pSources, const uint32_t nSourceCount )
	{
		const t3dVTCookAllocator_t& rAllocator = m_allocator;
		pOrder = ( uint32_t * )t3d_vt_cooker_allocate( rAllocator, ( uint64_t )nSourceCount * sizeof( uint32_t ) );
		pProfiles = ( t3dVTFixedProfile_t * )t3d_vt_cooker_allocate( rAllocator, ( uint64_t )nSourceCount * sizeof( t3dVTFixedProfile_t ) );
		pEnabledMask = ( uint32_t * )t3d_vt_cooker_allocate( rAllocator, ( uint64_t )nSourceCount * sizeof( uint32_t ) );
		if ( pOrder == NULL || pProfiles == NULL || pEnabledMask == NULL ) {
			return false;
		}
		for ( uint32_t i = 0u; i < nSourceCount; ++i ) {
			pOrder[i] = i;
			if ( !t3d_vt_cooker_validate_sparse_source( pSources[i], pProfiles[i], pEnabledMask[i] ) ) {
				return false;
			}
		}
		for ( uint32_t i = 1u; i < nSourceCount; ++i ) {
			const uint32_t nValue = pOrder[i];
			uint32_t j = i;
			while ( j > 0u && pSources[pOrder[j - 1u]].m_nStableKey > pSources[nValue].m_nStableKey ) {
				pOrder[j] = pOrder[j - 1u];
				--j;
			}
			pOrder[j] = nValue;
		}
		for ( uint32_t i = 1u; i < nSourceCount; ++i ) {
			if ( pSources[pOrder[i - 1u]].m_nStableKey == pSources[pOrder[i]].m_nStableKey ) {
				return false;
			}
		}

		uint64_t nTotalPages64 = 0u;
		uint64_t nTotalTails64 = 0u;
		uint64_t nTotalQualityViews64 = 0u;
		for ( uint32_t iOutput = 0u; iOutput < nSourceCount; ++iOutput ) {
			const uint32_t iSource = pOrder[iOutput];
			nTotalQualityViews64 += ( uint32_t )pProfiles[iSource] + 1u;
			if ( nTotalQualityViews64 > UINT32_MAX ) {
				return false;
			}
			for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
				if ( ( pEnabledMask[iSource] & T3D_VT_LOGICAL_CHANNEL_BIT( iChannel ) ) == 0u ) {
					continue;
				}
				const uint32_t nExtent = t3d_vt_fixed_profile_channel_extent( pProfiles[iSource], ( t3dVTLogicalChannel_t )iChannel );
				const uint32_t nMipCount = t3d_vt_page_builder_mip_count( nExtent, nExtent );
				const uint32_t nTailFirstMip =
				    t3d_vt_cooker_tail_first_mip( nExtent, nExtent, nMipCount, ( t3dVTConsumer_t )pSources[iSource].m_nConsumer );
				const uint32_t nPages = t3d_vt_cooker_regular_page_count( nExtent, nTailFirstMip );
				nTotalPages64 += nPages;
				++nTotalTails64;
				if ( nTotalPages64 > UINT32_MAX || nTotalTails64 > UINT32_MAX ) {
					return false;
				}
			}
		}
		nTotalPages = ( uint32_t )nTotalPages64;
		nTotalTails = ( uint32_t )nTotalTails64;
		nTotalQualityViews = ( uint32_t )nTotalQualityViews64;
		if ( nTotalPages64 + nTotalTails64 > UINT32_MAX ) {
			return false;
		}
		nPayloadCapacity = nTotalPages + nTotalTails;
		if ( nPayloadCapacity == 0u || nSourceCount > UINT32_MAX / T3D_VT_LOGICAL_CHANNEL_COUNT ) {
			return false;
		}
		nHashCapacity = t3d_vt_cooker_next_power_of_two( nPayloadCapacity > UINT32_MAX / 2u ? 0u : nPayloadCapacity * 2u );
		if ( nHashCapacity == 0u ) {
			return false;
		}
		return true;
	}

private:
	const t3dVTCookAllocator_t& m_allocator;
};

bool t3d_vt_cooker_cook_sparse_pages( const t3dVTCookerSettings_t& rSettings, const t3dVTCookSparseSource_t *pSources,
                                      const uint32_t nSourceCount, const t3dVTEncoderInterface_t& rEncoder,
                                      const t3dVTCookAllocator_t& rAllocator, const t3dVTCookClusterCodec_t *pClusterCodec,
                                      t3dVTCookedSparsePackage_t& rPackage )
{
	memset( &rPackage, 0, sizeof( rPackage ) );
	if ( !t3d_vt_cooker_validate_settings( rSettings ) || pSources == NULL || nSourceCount == 0u || rEncoder.m_pEncodeTile == NULL ||
	     rAllocator.m_pAllocate == NULL || rAllocator.m_pFree == NULL ) {
		return false;
	}
	rPackage.m_Allocator = rAllocator;

	Terrain3DVTCookSourcePlan Plan( rAllocator );
	if ( !Plan.prepare( pSources, nSourceCount ) ) {
		return false;
	}
	const uint32_t *const pOrder = Plan.pOrder;
	const t3dVTFixedProfile_t *const pProfiles = Plan.pProfiles;
	const uint32_t *const pEnabledMask = Plan.pEnabledMask;
	const uint32_t nTotalPages = Plan.nTotalPages;
	const uint32_t nTotalTails = Plan.nTotalTails;
	const uint32_t nTotalQualityViews = Plan.nTotalQualityViews;
	const uint32_t nPayloadCapacity = Plan.nPayloadCapacity;
	const uint32_t nHashCapacity = Plan.nHashCapacity;

	rPackage.m_pAllocations = ( t3dVTPackAllocationRecord_t * )t3d_vt_cooker_allocate_zero(
	    rAllocator, ( uint64_t )nSourceCount * sizeof( t3dVTPackAllocationRecord_t ) );
	rPackage.m_pAllocationKeys = ( t3dVTPackAllocationKeyRecord_t * )t3d_vt_cooker_allocate_zero(
	    rAllocator, ( uint64_t )nSourceCount * sizeof( t3dVTPackAllocationKeyRecord_t ) );
	rPackage.m_pChannelAllocations = ( t3dVTChannelAllocation_t * )t3d_vt_cooker_allocate_zero(
	    rAllocator, ( uint64_t )nSourceCount * T3D_VT_LOGICAL_CHANNEL_COUNT * sizeof( t3dVTChannelAllocation_t ) );
	rPackage.m_pPages = nTotalPages == 0u ? NULL
	                                      : ( t3dVTPackPageRecord_t * )t3d_vt_cooker_allocate_zero(
	                                            rAllocator, ( uint64_t )nTotalPages * sizeof( t3dVTPackPageRecord_t ) );
	rPackage.m_pTails =
	    ( t3dVTPackTailRecord_t * )t3d_vt_cooker_allocate_zero( rAllocator, ( uint64_t )nTotalTails * sizeof( t3dVTPackTailRecord_t ) );
	rPackage.m_pQualityViews = ( t3dVTPackQualityView_t * )t3d_vt_cooker_allocate_zero( rAllocator, ( uint64_t )nTotalQualityViews *
	                                                                                                    sizeof( t3dVTPackQualityView_t ) );
	t3dVTCookStagedPayload_t *pStaged = ( t3dVTCookStagedPayload_t * )t3d_vt_cooker_allocate_zero(
	    rAllocator, ( uint64_t )nPayloadCapacity * sizeof( t3dVTCookStagedPayload_t ) );
	uint32_t *pEncodedHash = ( uint32_t * )t3d_vt_cooker_allocate_zero( rAllocator, ( uint64_t )nHashCapacity * sizeof( uint32_t ) );
	t3dVTCookRawTileCacheEntry_t *pRawCache = ( t3dVTCookRawTileCacheEntry_t * )t3d_vt_cooker_allocate_zero(
	    rAllocator, ( uint64_t )nPayloadCapacity * sizeof( t3dVTCookRawTileCacheEntry_t ) );
	uint8_t *pTileBytes = ( uint8_t * )t3d_vt_cooker_allocate( rAllocator, T3D_VT_COOK_MAX_RAW_TILE_BYTES );
	uint8_t *pEncodedBytes = ( uint8_t * )t3d_vt_cooker_allocate( rAllocator, T3D_VT_COOK_MAX_ENCODED_TILE_BYTES );
	if ( rPackage.m_pAllocations == NULL || rPackage.m_pAllocationKeys == NULL || rPackage.m_pChannelAllocations == NULL ||
	     ( nTotalPages != 0u && rPackage.m_pPages == NULL ) || rPackage.m_pTails == NULL || rPackage.m_pQualityViews == NULL ||
	     pStaged == NULL || pEncodedHash == NULL || pRawCache == NULL || pTileBytes == NULL || pEncodedBytes == NULL ||
	     !t3d_vt_pack_init_format_family_records( rPackage.m_FormatFamilies, T3D_VT_LOGICAL_CHANNEL_COUNT ) ) {
		t3d_vt_cooker_free( rAllocator, pStaged );
		t3d_vt_cooker_free( rAllocator, pEncodedHash );
		t3d_vt_cooker_free( rAllocator, pRawCache );
		t3d_vt_cooker_free( rAllocator, pTileBytes );
		t3d_vt_cooker_free( rAllocator, pEncodedBytes );

		t3d_vt_cooker_release_sparse_package( rPackage );
		return false;
	}

	rPackage.m_nAllocationCount = nSourceCount;
	rPackage.m_nChannelAllocationCount = nSourceCount * T3D_VT_LOGICAL_CHANNEL_COUNT;
	rPackage.m_nPageCount = nTotalPages;
	rPackage.m_nTailCount = nTotalTails;
	rPackage.m_nQualityViewCount = nTotalQualityViews;
	rPackage.m_Stats.m_nAllocationCount = nSourceCount;
	rPackage.m_Stats.m_nPageCount = nTotalPages;
	rPackage.m_Stats.m_nTailCount = nTotalTails;
	uint32_t nPageCursor = 0u;
	uint32_t nTailCursor = 0u;
	uint32_t nStagedCount = 0u;
	uint32_t nRawCacheCount = 0u;
	t3dVTCookTileTable_t TileTable{ { rAllocator, pStaged, nPayloadCapacity, nStagedCount, pEncodedHash, nHashCapacity },
	                                rEncoder,
	                                pRawCache,
	                                nPayloadCapacity,
	                                nRawCacheCount,
	                                pEncodedBytes,
	                                rPackage.m_Stats };
	uint64_t nPackageSignature = 1469598103934665603ull;
	bool bOk = true;

	for ( uint32_t iAllocation = 0u; iAllocation < nSourceCount && bOk; ++iAllocation ) {
		const uint32_t iSource = pOrder[iAllocation];
		const t3dVTCookSparseSource_t& rSource = pSources[iSource];
		const t3dVTFixedProfile_t Profile = pProfiles[iSource];
		const uint32_t nChannelMask = pEnabledMask[iSource];
		t3dVTPackAllocationRecord_t& rAllocation = rPackage.m_pAllocations[iAllocation];
		memset( &rAllocation, 0, sizeof( rAllocation ) );
		rAllocation.m_nFlags = rSource.m_nAllocationFlags;
		rAllocation.m_nChannelMask = ( uint16_t )nChannelMask;
		rAllocation.m_nHighestAvailableProfile = ( uint8_t )Profile;
		rAllocation.m_nAvailableProfileMask = ( uint8_t )( ( 1u << ( ( uint32_t )Profile + 1u ) ) - 1u );
		rAllocation.m_nConsumer = ( uint8_t )rSource.m_nConsumer;
		rPackage.m_pAllocationKeys[iAllocation].m_nStableKey = rSource.m_nStableKey;
		rPackage.m_pAllocationKeys[iAllocation].m_nAllocationId = iAllocation;
		uint64_t nAllocationSignature = t3dHashFnv1a64LittleU64( 1469598103934665603ull, rSource.m_nStableKey );

		for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT && bOk; ++iChannel ) {
			t3dVTChannelAllocation_t& rChannelAllocation =
			    rPackage.m_pChannelAllocations[iAllocation * T3D_VT_LOGICAL_CHANNEL_COUNT + iChannel];
			memset( &rChannelAllocation, 0, sizeof( rChannelAllocation ) );
			rChannelAllocation.m_nTailIndex = T3D_VT_INVALID_TAIL;
			if ( ( nChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( iChannel ) ) == 0u ) {
				continue;
			}
			const t3dVTLogicalChannel_t Channel = ( t3dVTLogicalChannel_t )iChannel;
			const uint32_t nExtent = t3d_vt_fixed_profile_channel_extent( Profile, Channel );
			uint8_t *pMipBytes = NULL;
			t3dVTMipChainLayout_t Layout;
			if ( !t3d_vt_cooker_build_channel_pyramid( rAllocator, rSource, Profile, Channel, pMipBytes, Layout,
			                                           rSettings.m_nMipQuality ) ) {
				bOk = false;
				break;
			}
			const uint32_t nTailFirstMip =
			    t3d_vt_cooker_tail_first_mip( nExtent, nExtent, Layout.m_nMipCount, ( t3dVTConsumer_t )rSource.m_nConsumer );
			const uint32_t nRegularPages = t3d_vt_cooker_regular_page_count( nExtent, nTailFirstMip );
			rChannelAllocation.m_nBaseVirtualPage = nPageCursor;
			rChannelAllocation.m_nPageCount = nRegularPages;
			rChannelAllocation.m_nTailIndex = nTailCursor;
			rChannelAllocation.m_nMip0WidthPages = ( uint16_t )t3d_vt_page_builder_pages_wide( nExtent );
			rChannelAllocation.m_nMip0HeightPages = ( uint16_t )t3d_vt_page_builder_pages_high( nExtent );
			rChannelAllocation.m_nMipCount = ( uint16_t )Layout.m_nMipCount;
			rChannelAllocation.m_nFlags = rSource.m_nAllocationFlags;

			uint32_t MipBase[T3D_VT_MAX_MIPS];
			memset( MipBase, 0, sizeof( MipBase ) );
			uint32_t nLocalPage = 0u;
			for ( uint32_t iMip = 0u; iMip < nTailFirstMip; ++iMip ) {
				MipBase[iMip] = nLocalPage;
				const t3dVTMipLevelLayout_t& rMip = Layout.m_Levels[iMip];
				nLocalPage += t3d_vt_page_builder_pages_wide( rMip.m_nWidth ) * t3d_vt_page_builder_pages_high( rMip.m_nHeight );
			}
			if ( nLocalPage != nRegularPages ) {
				t3d_vt_cooker_free( rAllocator, pMipBytes );
				bOk = false;
				break;
			}

			for ( uint32_t iMip = 0u; iMip < nTailFirstMip && bOk; ++iMip ) {
				const t3dVTMipLevelLayout_t& rMip = Layout.m_Levels[iMip];
				const uint32_t nPagesWide = t3d_vt_page_builder_pages_wide( rMip.m_nWidth );
				const uint32_t nPagesHigh = t3d_vt_page_builder_pages_high( rMip.m_nHeight );
				const uint32_t nNextPagesWide =
				    iMip + 1u < nTailFirstMip ? t3d_vt_page_builder_pages_wide( Layout.m_Levels[iMip + 1u].m_nWidth ) : 0u;
				for ( uint32_t y = 0u; y < nPagesHigh && bOk; ++y ) {
					for ( uint32_t x = 0u; x < nPagesWide; ++x ) {
						if ( !t3d_vt_page_builder_build_tile_u8( pMipBytes + rMip.m_nOffset, rMip.m_nWidth, rMip.m_nHeight,
						                                         Layout.m_nComponents, x, y, ( t3dVTCookWrapMode_t )rSource.m_nWrapMode,
						                                         pTileBytes, T3D_VT_COOK_MAX_RAW_TILE_BYTES ) ) {
							bOk = false;
							break;
						}
						uint32_t nPayloadId = 0u;
						if ( !t3d_vt_cooker_encode_and_add_payload(
						         TileTable,
						         t3dVTCookTileRequest_t{ Channel, 0u, rSource.m_nAllocationFlags,
								                         ( uint8_t )( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO && rSource.m_bAlphaScissor
						                                                  ? rSource.m_nAlphaThreshold
																		  : 0u ),
								                         rSettings.m_nCompressionQuality, pTileBytes },
						         nPayloadId ) ) {
							bOk = false;
							break;
						}
						const uint32_t nPage = nPageCursor + MipBase[iMip] + y * nPagesWide + x;
						t3dVTPackPageRecord_t& rPage = rPackage.m_pPages[nPage];
						memset( &rPage, 0, sizeof( rPage ) );
						rPage.m_nVirtualPage = nPage;
						rPage.m_nAllocationId = iAllocation;
						rPage.m_nPayloadId = nPayloadId;
						rPage.m_nMip = ( uint16_t )iMip;
						rPage.m_nFlags = T3D_VT_PAGE_VALID;
						rPage.m_nPageX = ( uint16_t )x;
						rPage.m_nPageY = ( uint16_t )y;
						rPage.m_nChannel = ( uint16_t )iChannel;
						if ( iMip + 1u < nTailFirstMip ) {
							rPage.m_nParentPage = nPageCursor + MipBase[iMip + 1u] + ( y >> 1u ) * nNextPagesWide + ( x >> 1u );
						} else {
							rPage.m_nParentPage = T3D_VT_INVALID_PAGE;
							rPage.m_nFlags |= T3D_VT_PAGE_ROOT | T3D_VT_PAGE_TAIL_FALLBACK;
						}
						++rPackage.m_Stats.m_nChannelPageCount[iChannel];
						nAllocationSignature = t3dHashFnv1a64LittleU64( nAllocationSignature, pStaged[nPayloadId].m_nContentSignature );
					}
				}
			}

			if ( bOk ) {
				if ( !t3d_vt_page_builder_fill_logical_neutral( Channel, T3D_VT_PHYSICAL_TILE, T3D_VT_PHYSICAL_TILE, pTileBytes,
				                                                T3D_VT_COOK_MAX_RAW_TILE_BYTES ) ) {
					bOk = false;
				} else {
					uint32_t nAtlasX = T3D_VT_PAGE_GUTTER;
					uint32_t nAtlasWidth = 0u;
					uint32_t nAtlasHeight = 0u;
					for ( uint32_t iMip = nTailFirstMip; iMip < Layout.m_nMipCount && bOk; ++iMip ) {
						const t3dVTMipLevelLayout_t& rMip = Layout.m_Levels[iMip];
						if ( nAtlasX + rMip.m_nWidth > T3D_VT_PAGE_GUTTER + T3D_VT_PAGE_INTERIOR ||
						     T3D_VT_PAGE_GUTTER + rMip.m_nHeight > T3D_VT_PAGE_GUTTER + T3D_VT_PAGE_INTERIOR ) {
							bOk = false;
							break;
						}
						for ( uint32_t y = 0u; y < rMip.m_nHeight; ++y ) {
							uint8_t *pDestination =
							    pTileBytes +
							    ( ( uint64_t )( T3D_VT_PAGE_GUTTER + y ) * T3D_VT_PHYSICAL_TILE + nAtlasX ) * Layout.m_nComponents;
							const uint8_t *pSource = pMipBytes + rMip.m_nOffset + ( uint64_t )y * rMip.m_nWidth * Layout.m_nComponents;
							memcpy( pDestination, pSource, ( size_t )rMip.m_nWidth * Layout.m_nComponents );
						}
						nAtlasX += rMip.m_nWidth;
						nAtlasWidth += rMip.m_nWidth;
						nAtlasHeight = rMip.m_nHeight > nAtlasHeight ? rMip.m_nHeight : nAtlasHeight;
					}
					if ( bOk ) {
						uint32_t nTailPayload = 0u;
						if ( !t3d_vt_cooker_encode_and_add_payload(
						         TileTable,
						         t3dVTCookTileRequest_t{ Channel, T3D_VT_PACK_PAYLOAD_FLAG_TAIL, rSource.m_nAllocationFlags,
								                         ( uint8_t )( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO && rSource.m_bAlphaScissor
						                                                  ? rSource.m_nAlphaThreshold
																		  : 0u ),
								                         rSettings.m_nCompressionQuality, pTileBytes },
						         nTailPayload ) ) {
							bOk = false;
						} else {
							t3dVTPackTailRecord_t& rTail = rPackage.m_pTails[nTailCursor];
							memset( &rTail, 0, sizeof( rTail ) );
							rTail.m_nCanonicalAllocationId = iAllocation;
							rTail.m_nPayloadId = nTailPayload;
							rTail.m_nFirstMip = ( uint16_t )nTailFirstMip;
							rTail.m_nMipCount = ( uint16_t )( Layout.m_nMipCount - nTailFirstMip );
							rTail.m_nFirstMipWidth = ( uint16_t )Layout.m_Levels[nTailFirstMip].m_nWidth;
							rTail.m_nFirstMipHeight = ( uint16_t )Layout.m_Levels[nTailFirstMip].m_nHeight;
							rTail.m_nAtlasInteriorWidth = ( uint16_t )nAtlasWidth;
							rTail.m_nAtlasInteriorHeight = ( uint16_t )nAtlasHeight;
							rTail.m_nChannel = ( uint16_t )iChannel;
							rTail.m_nLayout = T3D_VT_PACK_TAIL_LAYOUT_HORIZONTAL;
							rTail.m_nContentSignature =
							    t3dHashFnv1a64LittleU64( pStaged[nTailPayload].m_nContentSignature, ( uint64_t )iChannel );
							nAllocationSignature = t3dHashFnv1a64LittleU64( nAllocationSignature, rTail.m_nContentSignature );
							++nTailCursor;
						}
					}
				}
			}
			t3d_vt_cooker_free( rAllocator, pMipBytes );
			if ( bOk ) {
				nPageCursor += nRegularPages;
			}
		}
		if ( !bOk ) {
			break;
		}
		const uint32_t nContentId = t3d_vt_cooker_fold_signature( nAllocationSignature );
		const uint32_t nGeneration = rSource.m_nGenerationSeed != 0u
		                                 ? rSource.m_nGenerationSeed
		                                 : t3d_vt_cooker_fold_signature( rSource.m_nStableKey ^ nAllocationSignature );
		rAllocation.m_nContentId = nContentId;
		rAllocation.m_nGeneration = nGeneration;
		rPackage.m_pAllocationKeys[iAllocation].m_nContentId = nContentId;
		for ( uint32_t iPage = 0u; iPage < nPageCursor; ++iPage ) {
			if ( rPackage.m_pPages[iPage].m_nAllocationId == iAllocation ) {
				rPackage.m_pPages[iPage].m_nContentId = nContentId;
				rPackage.m_pPages[iPage].m_nGeneration = nGeneration;
			}
		}
		for ( uint32_t iTail = 0u; iTail < nTailCursor; ++iTail ) {
			if ( rPackage.m_pTails[iTail].m_nCanonicalAllocationId == iAllocation ) {
				rPackage.m_pTails[iTail].m_nContentId = nContentId;
			}
		}
		nPackageSignature = t3dHashFnv1a64LittleU64( nPackageSignature, rSource.m_nStableKey );
		nPackageSignature = t3dHashFnv1a64LittleU64( nPackageSignature, nAllocationSignature );
	}

	if ( bOk && ( nPageCursor != nTotalPages || nTailCursor != nTotalTails ) ) {
		bOk = false;
	}
	if ( bOk ) {
		bOk = t3d_vt_cooker_build_quality_views( pSources, pOrder, pProfiles, nSourceCount, rPackage ) &&
		      t3d_vt_cooker_validate_quality_views( rPackage );
	}
	if ( bOk ) {
		bOk = t3d_vt_cooker_finalize_sparse_payloads( rSettings, rAllocator, pClusterCodec, pStaged, nStagedCount, rPackage );
	}
	if ( bOk ) {
		rPackage.m_nPackageGeneration = t3d_vt_cooker_fold_signature( nPackageSignature );
		rPackage.m_Stats.m_nContentSignature = nPackageSignature;
	}
	for ( uint32_t iPayload = 0u; iPayload < nStagedCount; ++iPayload ) {
		t3d_vt_cooker_free( rAllocator, pStaged[iPayload].m_pBytes );
	}
	t3d_vt_cooker_free( rAllocator, pStaged );
	t3d_vt_cooker_free( rAllocator, pEncodedHash );
	t3d_vt_cooker_free( rAllocator, pRawCache );
	t3d_vt_cooker_free( rAllocator, pTileBytes );
	t3d_vt_cooker_free( rAllocator, pEncodedBytes );

	if ( !bOk ) {
		t3d_vt_cooker_release_sparse_package( rPackage );
		return false;
	}
	return true;
}
