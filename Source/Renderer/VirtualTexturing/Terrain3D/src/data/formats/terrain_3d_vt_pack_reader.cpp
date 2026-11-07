// Shared frozen VT V1 package reader and validators; no authoring dependencies.
#include "data/formats/terrain_3d_vt_pack_format.h"
#include "core/utility/terrain_3d_hash_bytes.h"
#include "core/utility/terrain_3d_checked_math.h"
#include <stddef.h>
#include <string.h>

uint64_t t3d_vt_pack_hash64( const void *pBytes, const uint64_t nBytes )
{
	return t3dHashStableBytes( pBytes, nBytes );
}

uint32_t t3d_vt_pack_checksum32( const void *pBytes, const uint64_t nBytes )
{
	const uint8_t *pData = ( const uint8_t * )pBytes;
	if ( pData == NULL && nBytes != 0u ) {
		return 0u;
	}
	uint32_t nCrc = 0xffffffffu;
	for ( uint64_t iByte = 0u; iByte < nBytes; ++iByte ) {
		nCrc ^= pData[iByte];
		for ( uint32_t iBit = 0u; iBit < 8u; ++iBit ) {
			const uint32_t nMask = 0u - ( nCrc & 1u );
			nCrc = ( nCrc >> 1u ) ^ ( 0xedb88320u & nMask );
		}
	}
	return ~nCrc;
}

uint32_t t3d_vt_pack_expected_tile_bytes( const uint32_t nFormat )
{
	const uint32_t nBlocks = ( T3D_VT_PHYSICAL_TILE + 3u ) / 4u;
	switch ( nFormat ) {
	case T3D_VT_FORMAT_ALBEDO_BC1_SRGB:
		return nBlocks * nBlocks * 8u;
	case T3D_VT_FORMAT_NORMAL_BC5_UNORM:
	case T3D_VT_FORMAT_SURFACE_BC7_UNORM:
		return nBlocks * nBlocks * 16u;
	case T3D_VT_FORMAT_HEIGHT_R8_UNORM:
		return T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE;
	default:
		return 0u;
	}
}

// -----------------------------------------------------------------------------
// Sparse V1 package primitives.

uint32_t t3d_vt_pack_expected_physical_tile_bytes( const uint32_t nPhysicalFamily )
{
	const uint32_t nBlocks = ( T3D_VT_PHYSICAL_TILE + 3u ) / 4u;
	switch ( nPhysicalFamily ) {
	case T3D_VT_PHYSICAL_FORMAT_BC1_UNORM:
		return nBlocks * nBlocks * 8u;
	case T3D_VT_PHYSICAL_FORMAT_BC7_UNORM:
	case T3D_VT_PHYSICAL_FORMAT_BC5_UNORM:
		return nBlocks * nBlocks * 16u;
	case T3D_VT_PHYSICAL_FORMAT_R8_UNORM:
		return T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE;
	case T3D_VT_PHYSICAL_FORMAT_R16_UNORM:
		return T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE * 2u;
	default:
		return 0u;
	}
}

bool t3d_vt_pack_init_format_family_records( t3dVTPackFormatFamilyRecord_t *pRecords, const uint32_t nRecordCount )
{
	if ( pRecords == NULL || nRecordCount != T3D_VT_LOGICAL_CHANNEL_COUNT ) {
		return false;
	}
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		const t3dVTLogicalChannel_t Channel = ( t3dVTLogicalChannel_t )iChannel;
		memset( &pRecords[iChannel], 0, sizeof( pRecords[iChannel] ) );
		pRecords[iChannel].m_nLogicalChannel = ( uint8_t )iChannel;
		pRecords[iChannel].m_nPhysicalFamily = ( uint8_t )t3d_vt_logical_channel_physical_family( Channel );
		pRecords[iChannel].m_nFlags = Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO ? T3D_VT_FORMAT_MAP_ALBEDO_RGBA : 0u;
		pRecords[iChannel].m_nSemantic = ( uint8_t )t3d_vt_logical_channel_semantic( Channel );
	}
	return true;
}

static uint32_t t3d_vt_pack_expected_profile_mask( const uint32_t nHighestProfile )
{
	if ( nHighestProfile >= T3D_VT_QUALITY_PROFILE_COUNT ) {
		return 0u;
	}
	return ( 1u << ( nHighestProfile + 1u ) ) - 1u;
}

template <typename TSource>
static bool t3d_vt_pack_format_map_valid( const TSource& rSource )
{
	if ( rSource.m_pFormatFamilies == NULL || rSource.m_nFormatFamilyCount != T3D_VT_LOGICAL_CHANNEL_COUNT ) {
		return false;
	}
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		const t3dVTPackFormatFamilyRecord_t& rRecord = rSource.m_pFormatFamilies[iChannel];
		const t3dVTLogicalChannel_t Channel = ( t3dVTLogicalChannel_t )iChannel;
		if ( rRecord.m_nLogicalChannel != iChannel ||
		     rRecord.m_nPhysicalFamily != ( uint8_t )t3d_vt_logical_channel_physical_family( Channel ) ||
		     rRecord.m_nSemantic != ( uint8_t )t3d_vt_logical_channel_semantic( Channel ) ||
		     ( rRecord.m_nFlags & ~( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO ? T3D_VT_FORMAT_MAP_ALBEDO_RGBA : 0u ) ) != 0u ||
		     rRecord.m_nReserved != 0u ) {
			return false;
		}
	}
	return true;
}

template <typename TSource>
static uint32_t t3d_vt_pack_fully_supported_mask_impl( const TSource& rSource )
{
	if ( rSource.m_pAllocations == NULL || rSource.m_nAllocationCount == 0u ) {
		return 0u;
	}
	uint32_t nMask = ( 1u << T3D_VT_QUALITY_PROFILE_COUNT ) - 1u;
	for ( uint32_t iAllocation = 0u; iAllocation < rSource.m_nAllocationCount; ++iAllocation ) {
		nMask &= rSource.m_pAllocations[iAllocation].m_nAvailableProfileMask;
	}
	return nMask;
}

template <typename TSource>
static bool t3d_vt_pack_validate_allocations( const TSource& rSource )
{
	uint64_t nPreviousStableKey = 0u;
	uint64_t nExpectedPageBase = 0u;
	uint64_t nExpectedQualityViews = 0u;
	for ( uint32_t iAllocation = 0u; iAllocation < rSource.m_nAllocationCount; ++iAllocation ) {
		const t3dVTPackAllocationRecord_t& rAllocation = rSource.m_pAllocations[iAllocation];
		const t3dVTPackAllocationKeyRecord_t& rKey = rSource.m_pAllocationKeys[iAllocation];
		const uint32_t nAllowedMask = t3d_vt_consumer_allowed_mask( ( t3dVTConsumer_t )rAllocation.m_nConsumer );
		const uint32_t nRequiredMask = t3d_vt_consumer_required_mask( ( t3dVTConsumer_t )rAllocation.m_nConsumer );
		const uint32_t nExpectedProfileMask = t3d_vt_pack_expected_profile_mask( rAllocation.m_nHighestAvailableProfile );
		// V1 contains the coarse regional (FAR) consumer too. The cooker and
		// residency reader must agree on the supported consumer range.
		if ( rAllocation.m_nConsumer > T3D_VT_CONSUMER_COMPOSED || ( rAllocation.m_nChannelMask & nRequiredMask ) != nRequiredMask ||
		     ( rAllocation.m_nChannelMask & ~nAllowedMask ) != 0u ||
		     rAllocation.m_nHighestAvailableProfile >= T3D_VT_QUALITY_PROFILE_COUNT || nExpectedProfileMask == 0u ||
		     rAllocation.m_nAvailableProfileMask != nExpectedProfileMask || rAllocation.m_nReserved0 != 0u || rKey.m_nStableKey == 0u ||
		     rKey.m_nAllocationId != iAllocation || ( iAllocation > 0u && rKey.m_nStableKey <= nPreviousStableKey ) ||
		     rKey.m_nContentId != rAllocation.m_nContentId ) {
			return false;
		}
		nExpectedQualityViews += rAllocation.m_nHighestAvailableProfile + 1u;
		nPreviousStableKey = rKey.m_nStableKey;

		for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
			const uint32_t nChannelAllocationIndex = iAllocation * T3D_VT_LOGICAL_CHANNEL_COUNT + iChannel;
			const t3dVTChannelAllocation_t& rChannelAllocation = rSource.m_pChannelAllocations[nChannelAllocationIndex];
			const bool bPresent = ( rAllocation.m_nChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( iChannel ) ) != 0u;
			if ( !bPresent ) {
				if ( rChannelAllocation.m_nBaseVirtualPage != 0u || rChannelAllocation.m_nPageCount != 0u ||
				     rChannelAllocation.m_nTailIndex != T3D_VT_INVALID_TAIL || rChannelAllocation.m_nMip0WidthPages != 0u ||
				     rChannelAllocation.m_nMip0HeightPages != 0u || rChannelAllocation.m_nMipCount != 0u ||
				     rChannelAllocation.m_nFlags != 0u ) {
					return false;
				}
				continue;
			}
			if ( rChannelAllocation.m_nMipCount == 0u || rChannelAllocation.m_nMipCount > T3D_VT_MAX_MIPS ||
			     rChannelAllocation.m_nMip0WidthPages == 0u || rChannelAllocation.m_nMip0HeightPages == 0u ||
			     rChannelAllocation.m_nTailIndex >= rSource.m_nTailCount || rChannelAllocation.m_nBaseVirtualPage != nExpectedPageBase ||
			     rChannelAllocation.m_nBaseVirtualPage > rSource.m_nPageCount ||
			     rChannelAllocation.m_nPageCount > rSource.m_nPageCount - rChannelAllocation.m_nBaseVirtualPage ) {
				return false;
			}
			if ( iChannel == T3D_VT_LOGICAL_CHANNEL_ALBEDO && ( rChannelAllocation.m_nFlags & T3D_VT_ALLOCATION_ALBEDO_RGBA ) != 0u &&
			     ( rSource.m_pFormatFamilies[iChannel].m_nFlags & T3D_VT_FORMAT_MAP_ALBEDO_RGBA ) == 0u ) {
				return false;
			}
			const t3dVTPackTailRecord_t& rTail = rSource.m_pTails[rChannelAllocation.m_nTailIndex];
			if ( rTail.m_nChannel != iChannel || rTail.m_nFirstMip >= rChannelAllocation.m_nMipCount ||
			     rTail.m_nMipCount != rChannelAllocation.m_nMipCount - rTail.m_nFirstMip ) {
				return false;
			}
			nExpectedPageBase += rChannelAllocation.m_nPageCount;
		}
	}
	if ( nExpectedPageBase != rSource.m_nPageCount || nExpectedQualityViews != rSource.m_nQualityViewCount ||
	     t3d_vt_pack_fully_supported_mask_impl( rSource ) == 0u ) {
		return false;
	}

	return true;
}

template <typename TSource>
static bool t3d_vt_pack_validate_pages( const TSource& rSource )
{
	for ( uint32_t iPage = 0u; iPage < rSource.m_nPageCount; ++iPage ) {
		const t3dVTPackPageRecord_t& rPage = rSource.m_pPages[iPage];
		if ( rPage.m_nVirtualPage != iPage || rPage.m_nAllocationId >= rSource.m_nAllocationCount ||
		     rPage.m_nPayloadId >= rSource.m_nPayloadCount || rPage.m_nChannel >= T3D_VT_LOGICAL_CHANNEL_COUNT ) {
			return false;
		}
		const t3dVTPackAllocationRecord_t& rAllocation = rSource.m_pAllocations[rPage.m_nAllocationId];
		if ( ( rAllocation.m_nChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( rPage.m_nChannel ) ) == 0u ||
		     rPage.m_nGeneration != rAllocation.m_nGeneration || rPage.m_nContentId != rAllocation.m_nContentId ) {
			return false;
		}
		const t3dVTChannelAllocation_t& rChannelAllocation =
		    rSource.m_pChannelAllocations[rPage.m_nAllocationId * T3D_VT_LOGICAL_CHANNEL_COUNT + rPage.m_nChannel];
		if ( iPage < rChannelAllocation.m_nBaseVirtualPage ||
		     iPage >= rChannelAllocation.m_nBaseVirtualPage + rChannelAllocation.m_nPageCount ||
		     rPage.m_nMip >= rChannelAllocation.m_nMipCount ||
		     ( rPage.m_nParentPage != T3D_VT_INVALID_PAGE &&
		       ( rPage.m_nParentPage < rChannelAllocation.m_nBaseVirtualPage ||
		         rPage.m_nParentPage >= rChannelAllocation.m_nBaseVirtualPage + rChannelAllocation.m_nPageCount ) ) ) {
			return false;
		}
		const uint32_t nFamily =
		    ( uint32_t )t3d_vt_logical_channel_physical_family( ( t3dVTLogicalChannel_t )rPage.m_nChannel, rChannelAllocation.m_nFlags );
		const t3dVTPackPayloadRecord_t& rPayload = rSource.m_pPayloads[rPage.m_nPayloadId];
		if ( rPayload.m_nFormat != nFamily || rPayload.m_nDecodedBytes != t3d_vt_pack_expected_physical_tile_bytes( nFamily ) ) {
			return false;
		}
	}

	return true;
}

template <typename TSource>
static bool t3d_vt_pack_validate_payloads( const TSource& rSource )
{
	for ( uint32_t iPayload = 0u; iPayload < rSource.m_nPayloadCount; ++iPayload ) {
		const t3dVTPackPayloadRecord_t& rPayload = rSource.m_pPayloads[iPayload];
		if ( rPayload.m_nClusterId >= rSource.m_nClusterCount || rPayload.m_nStoredBytes == 0u || rPayload.m_nDecodedBytes == 0u ||
		     rPayload.m_nFormat >= T3D_VT_PHYSICAL_FORMAT_COUNT || rPayload.m_nCodec != T3D_VT_PACK_CODEC_NONE ||
		     rPayload.m_nStoredBytes != rPayload.m_nDecodedBytes ||
		     rPayload.m_nDecodedBytes != t3d_vt_pack_expected_physical_tile_bytes( rPayload.m_nFormat ) ) {
			return false;
		}
		const t3dVTPackClusterRecord_t& rCluster = rSource.m_pClusters[rPayload.m_nClusterId];
		if ( rPayload.m_nClusterOffset > rCluster.m_nDecodedBytes ||
		     rPayload.m_nDecodedBytes > rCluster.m_nDecodedBytes - rPayload.m_nClusterOffset ) {
			return false;
		}
	}

	return true;
}

template <typename TSource>
static bool t3d_vt_pack_validate_clusters( const TSource& rSource )
{
	uint32_t nExpectedFirstPayload = 0u;
	for ( uint32_t iCluster = 0u; iCluster < rSource.m_nClusterCount; ++iCluster ) {
		const t3dVTPackClusterRecord_t& rCluster = rSource.m_pClusters[iCluster];
		if ( rCluster.m_nPayloadDataOffset > rSource.m_nPayloadDataBytes ||
		     rCluster.m_nStoredBytes > rSource.m_nPayloadDataBytes - rCluster.m_nPayloadDataOffset || rCluster.m_nStoredBytes == 0u ||
		     rCluster.m_nDecodedBytes == 0u || rCluster.m_nFirstPayload != nExpectedFirstPayload || rCluster.m_nPayloadCount == 0u ||
		     rCluster.m_nPayloadCount > rSource.m_nPayloadCount - rCluster.m_nFirstPayload ||
		     ( rCluster.m_nCodec != T3D_VT_PACK_CODEC_NONE && rCluster.m_nCodec != T3D_VT_PACK_CODEC_ZSTD ) ||
		     ( rCluster.m_nCodec == T3D_VT_PACK_CODEC_NONE && rCluster.m_nStoredBytes != rCluster.m_nDecodedBytes ) ) {
			return false;
		}
		for ( uint32_t iPayload = 0u; iPayload < rCluster.m_nPayloadCount; ++iPayload ) {
			if ( rSource.m_pPayloads[rCluster.m_nFirstPayload + iPayload].m_nClusterId != iCluster ) {
				return false;
			}
		}
		nExpectedFirstPayload += rCluster.m_nPayloadCount;
	}
	if ( nExpectedFirstPayload != rSource.m_nPayloadCount ) {
		return false;
	}

	return true;
}

template <typename TSource>
static bool t3d_vt_pack_validate_tails( const TSource& rSource )
{
	for ( uint32_t iTail = 0u; iTail < rSource.m_nTailCount; ++iTail ) {
		const t3dVTPackTailRecord_t& rTail = rSource.m_pTails[iTail];
		if ( rTail.m_nCanonicalAllocationId >= rSource.m_nAllocationCount || rTail.m_nPayloadId >= rSource.m_nPayloadCount ||
		     rTail.m_nChannel >= T3D_VT_LOGICAL_CHANNEL_COUNT || rTail.m_nMipCount == 0u || rTail.m_nFirstMipWidth == 0u ||
		     rTail.m_nFirstMipHeight == 0u || rTail.m_nAtlasInteriorWidth == 0u || rTail.m_nAtlasInteriorWidth > T3D_VT_PAGE_INTERIOR ||
		     rTail.m_nAtlasInteriorHeight == 0u || rTail.m_nAtlasInteriorHeight > T3D_VT_PAGE_INTERIOR ||
		     rTail.m_nLayout != T3D_VT_PACK_TAIL_LAYOUT_HORIZONTAL ) {
			return false;
		}
		const t3dVTPackAllocationRecord_t& rAllocation = rSource.m_pAllocations[rTail.m_nCanonicalAllocationId];
		if ( ( rAllocation.m_nChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( rTail.m_nChannel ) ) == 0u ) {
			return false;
		}
		const t3dVTPackPayloadRecord_t& rPayload = rSource.m_pPayloads[rTail.m_nPayloadId];
		const uint32_t nFamily = ( uint32_t )t3d_vt_logical_channel_physical_family(
		    ( t3dVTLogicalChannel_t )rTail.m_nChannel,
		    rSource.m_pChannelAllocations[rTail.m_nCanonicalAllocationId * T3D_VT_LOGICAL_CHANNEL_COUNT + rTail.m_nChannel].m_nFlags );
		if ( rPayload.m_nFormat != nFamily || ( rPayload.m_nFlags & T3D_VT_PACK_PAYLOAD_FLAG_TAIL ) == 0u ) {
			return false;
		}
	}

	return true;
}

template <typename TSource>
static bool t3d_vt_pack_validate_quality_views( const TSource& rSource )
{
	uint32_t nPreviousAllocation = 0u;
	uint32_t nPreviousProfile = 0u;
	uint32_t nSeenProfileMask = 0u;
	for ( uint32_t iView = 0u; iView < rSource.m_nQualityViewCount; ++iView ) {
		const t3dVTPackQualityView_t& rView = rSource.m_pQualityViews[iView];
		if ( rView.m_nAllocationId >= rSource.m_nAllocationCount || rView.m_nProfile >= T3D_VT_QUALITY_PROFILE_COUNT ||
		     ( iView == 0u && rView.m_nAllocationId != 0u ) ) {
			return false;
		}
		if ( iView == 0u || rView.m_nAllocationId != nPreviousAllocation ) {
			if ( iView != 0u && nSeenProfileMask != rSource.m_pAllocations[nPreviousAllocation].m_nAvailableProfileMask ) {
				return false;
			}
			if ( iView != 0u && rView.m_nAllocationId != nPreviousAllocation + 1u ) {
				return false;
			}
			nSeenProfileMask = 0u;
			nPreviousProfile = 0u;
		} else if ( rView.m_nProfile <= nPreviousProfile ) {
			return false;
		}
		const t3dVTPackAllocationRecord_t& rAllocation = rSource.m_pAllocations[rView.m_nAllocationId];
		const uint32_t nProfileBit = 1u << rView.m_nProfile;
		const uint32_t nRequiredMask = t3d_vt_consumer_required_mask( ( t3dVTConsumer_t )rAllocation.m_nConsumer );
		if ( ( rAllocation.m_nAvailableProfileMask & nProfileBit ) == 0u || ( nSeenProfileMask & nProfileBit ) != 0u ||
		     rView.m_nChannelMask == 0u || ( rView.m_nChannelMask & nRequiredMask ) != nRequiredMask ||
		     ( rView.m_nChannelMask & ~rAllocation.m_nChannelMask ) != 0u ) {
			return false;
		}
		for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
			const bool bInView = ( rView.m_nChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( iChannel ) ) != 0u;
			const uint8_t nBaseMip = rView.m_nBaseMip[iChannel];
			const uint8_t nMaxMip = rView.m_nMaxMip[iChannel];
			if ( !bInView ) {
				if ( nBaseMip != T3D_VT_PACK_INVALID_MIP || nMaxMip != T3D_VT_PACK_INVALID_MIP ) {
					return false;
				}
				continue;
			}
			const t3dVTChannelAllocation_t& rChannelAllocation =
			    rSource.m_pChannelAllocations[rView.m_nAllocationId * T3D_VT_LOGICAL_CHANNEL_COUNT + iChannel];
			if ( nBaseMip == T3D_VT_PACK_INVALID_MIP || nMaxMip == T3D_VT_PACK_INVALID_MIP || nBaseMip > nMaxMip ||
			     nMaxMip >= rChannelAllocation.m_nMipCount ) {
				return false;
			}
		}
		nSeenProfileMask |= nProfileBit;
		nPreviousAllocation = rView.m_nAllocationId;
		nPreviousProfile = rView.m_nProfile;
	}
	if ( nSeenProfileMask != rSource.m_pAllocations[nPreviousAllocation].m_nAvailableProfileMask ) {
		return false;
	}
	return true;
}

template <typename TSource>
static bool t3d_vt_pack_validate_records( const TSource& rSource )
{
	if ( rSource.m_pAllocations == NULL || rSource.m_nAllocationCount == 0u || rSource.m_pAllocationKeys == NULL ||
	     rSource.m_pChannelAllocations == NULL || rSource.m_nAllocationCount > UINT32_MAX / T3D_VT_LOGICAL_CHANNEL_COUNT ||
	     rSource.m_nChannelAllocationCount != rSource.m_nAllocationCount * T3D_VT_LOGICAL_CHANNEL_COUNT ||
	     ( rSource.m_nPageCount != 0u && rSource.m_pPages == NULL ) || rSource.m_pPayloads == NULL || rSource.m_nPayloadCount == 0u ||
	     rSource.m_pClusters == NULL || rSource.m_nClusterCount == 0u || rSource.m_pTails == NULL || rSource.m_nTailCount == 0u ||
	     rSource.m_pQualityViews == NULL || rSource.m_nQualityViewCount == 0u || rSource.m_pPayloadData == NULL ||
	     rSource.m_nPayloadDataBytes == 0u || !t3d_vt_pack_format_map_valid( rSource ) ) {
		return false;
	}

	return t3d_vt_pack_validate_allocations( rSource ) && t3d_vt_pack_validate_pages( rSource ) &&
	       t3d_vt_pack_validate_payloads( rSource ) && t3d_vt_pack_validate_clusters( rSource ) && t3d_vt_pack_validate_tails( rSource ) &&
	       t3d_vt_pack_validate_quality_views( rSource );
}

bool t3d_vt_pack_validate_source( const t3dVTSerializedPackSource_t& rSource )
{
	return t3d_vt_pack_validate_records( rSource );
}

static bool t3d_vt_pack_open_view_impl( const void *pPackage, const uint64_t nAvailableBytes, const uint64_t nPackageBytes,
                                        const bool bMetadataOnly, t3dVTPackBaselineView_t& rView )
{
	rView = t3dVTPackBaselineView_t{};
	if ( pPackage == NULL || nAvailableBytes < T3D_VT_PACK_HEADER_BYTES || nPackageBytes < nAvailableBytes ) {
		return false;
	}
	const uint8_t *const pBytes = ( const uint8_t * )pPackage;
	t3dVTPackHeader_t Header{};
	memcpy( &Header, pBytes, sizeof( Header ) );
	const t3dVTPackHeader_t *const pHeader = &Header;
	if ( pHeader->m_nMagic != T3D_VT_FILE_MAGIC || pHeader->m_nVersion != T3D_VT_FILE_FORMAT_VERSION ||
	     pHeader->m_nHeaderBytes != T3D_VT_PACK_HEADER_BYTES || pHeader->m_nPackageBytes != nPackageBytes ||
	     pHeader->m_nAllocationCount > UINT32_MAX / T3D_VT_LOGICAL_CHANNEL_COUNT ||
	     pHeader->m_nChannelCount != T3D_VT_LOGICAL_CHANNEL_COUNT || pHeader->m_nInteriorTexels != T3D_VT_PAGE_INTERIOR ||
	     pHeader->m_nGutterTexels != T3D_VT_PAGE_GUTTER || pHeader->m_nPhysicalTexels != T3D_VT_PHYSICAL_TILE ||
	     pHeader->m_nChannelAllocationCount != pHeader->m_nAllocationCount * T3D_VT_LOGICAL_CHANNEL_COUNT ||
	     pHeader->m_nFormatFamilyCount != T3D_VT_LOGICAL_CHANNEL_COUNT || pHeader->m_nFullySupportedProfileMask == 0u ||
	     ( pHeader->m_nFullySupportedProfileMask & ~( ( 1u << T3D_VT_QUALITY_PROFILE_COUNT ) - 1u ) ) != 0u ||
	     ( pHeader->m_nFlags & ( T3D_VT_PACK_FLAG_QUALITY_VIEWS | T3D_VT_PACK_FLAG_FORMAT_FAMILY_MAP ) ) !=
	         ( T3D_VT_PACK_FLAG_QUALITY_VIEWS | T3D_VT_PACK_FLAG_FORMAT_FAMILY_MAP ) ) {
		return false;
	}

	t3dVTPackHeader_t HeaderCopy = *pHeader;
	const uint32_t nStoredHeaderChecksum = HeaderCopy.m_nHeaderChecksum;
	HeaderCopy.m_nHeaderChecksum = 0u;
	if ( t3d_vt_pack_checksum32( &HeaderCopy, sizeof( HeaderCopy ) ) != nStoredHeaderChecksum ||
	     ( !bMetadataOnly && t3d_vt_pack_hash64( pBytes + T3D_VT_PACK_HEADER_BYTES, nPackageBytes - T3D_VT_PACK_HEADER_BYTES ) !=
	                             pHeader->m_nContentSignature ) ) {
		return false;
	}

	const uint64_t nAllocationBytes = ( uint64_t )pHeader->m_nAllocationCount * sizeof( t3dVTPackAllocationRecord_t );
	const uint64_t nAllocationKeyBytes = ( uint64_t )pHeader->m_nAllocationCount * sizeof( t3dVTPackAllocationKeyRecord_t );
	const uint64_t nChannelAllocationBytes = ( uint64_t )pHeader->m_nChannelAllocationCount * sizeof( t3dVTChannelAllocation_t );
	const uint64_t nPageBytes = ( uint64_t )pHeader->m_nPageCount * sizeof( t3dVTPackPageRecord_t );
	const uint64_t nPayloadBytes = ( uint64_t )pHeader->m_nPayloadCount * sizeof( t3dVTPackPayloadRecord_t );
	const uint64_t nClusterBytes = ( uint64_t )pHeader->m_nClusterCount * sizeof( t3dVTPackClusterRecord_t );
	const uint64_t nTailBytes = ( uint64_t )pHeader->m_nTailCount * sizeof( t3dVTPackTailRecord_t );
	const uint64_t nQualityViewBytes = ( uint64_t )pHeader->m_nQualityViewCount * sizeof( t3dVTPackQualityView_t );
	const uint64_t nFormatFamilyBytes = ( uint64_t )pHeader->m_nFormatFamilyCount * sizeof( t3dVTPackFormatFamilyRecord_t );
	if ( !t3dByteRangeFits<uint64_t>( pHeader->m_nAllocationOffset, nAllocationBytes, nPackageBytes ) ||
	     !t3dByteRangeFits<uint64_t>( pHeader->m_nAllocationKeyOffset, nAllocationKeyBytes, nPackageBytes ) ||
	     !t3dByteRangeFits<uint64_t>( pHeader->m_nChannelAllocationOffset, nChannelAllocationBytes, nPackageBytes ) ||
	     !t3dByteRangeFits<uint64_t>( pHeader->m_nPageOffset, nPageBytes, nPackageBytes ) ||
	     !t3dByteRangeFits<uint64_t>( pHeader->m_nPayloadDirectoryOffset, nPayloadBytes, nPackageBytes ) ||
	     !t3dByteRangeFits<uint64_t>( pHeader->m_nClusterDirectoryOffset, nClusterBytes, nPackageBytes ) ||
	     !t3dByteRangeFits<uint64_t>( pHeader->m_nTailMetadataOffset, nTailBytes, nPackageBytes ) ||
	     !t3dByteRangeFits<uint64_t>( pHeader->m_nQualityViewOffset, nQualityViewBytes, nPackageBytes ) ||
	     !t3dByteRangeFits<uint64_t>( pHeader->m_nFormatFamilyOffset, nFormatFamilyBytes, nPackageBytes ) ||
	     !t3dByteRangeFits<uint64_t>( pHeader->m_nPayloadDataOffset, pHeader->m_nPayloadDataBytes, nPackageBytes ) ) {
		return false;
	}
	if ( bMetadataOnly &&
	     ( nAvailableBytes != pHeader->m_nPayloadDataOffset || pHeader->m_nFormatFamilyOffset + nFormatFamilyBytes > nAvailableBytes ) ) {
		return false;
	}
	if ( ( pHeader->m_nAllocationOffset % T3D_VT_PACK_ALIGNMENT ) != 0u ||
	     ( pHeader->m_nAllocationKeyOffset % T3D_VT_PACK_ALIGNMENT ) != 0u ||
	     ( pHeader->m_nChannelAllocationOffset % T3D_VT_PACK_ALIGNMENT ) != 0u ||
	     ( pHeader->m_nPageOffset % T3D_VT_PACK_ALIGNMENT ) != 0u || ( pHeader->m_nPayloadDirectoryOffset % T3D_VT_PACK_ALIGNMENT ) != 0u ||
	     ( pHeader->m_nClusterDirectoryOffset % T3D_VT_PACK_ALIGNMENT ) != 0u ||
	     ( pHeader->m_nTailMetadataOffset % T3D_VT_PACK_ALIGNMENT ) != 0u ||
	     ( pHeader->m_nQualityViewOffset % T3D_VT_PACK_ALIGNMENT ) != 0u ||
	     ( pHeader->m_nFormatFamilyOffset % T3D_VT_PACK_ALIGNMENT ) != 0u ||
	     ( pHeader->m_nPayloadDataOffset % T3D_VT_PACK_ALIGNMENT ) != 0u || pHeader->m_nAllocationOffset != T3D_VT_PACK_HEADER_BYTES ||
	     pHeader->m_nAllocationKeyOffset < pHeader->m_nAllocationOffset + nAllocationBytes ||
	     pHeader->m_nChannelAllocationOffset < pHeader->m_nAllocationKeyOffset + nAllocationKeyBytes ||
	     pHeader->m_nPageOffset < pHeader->m_nChannelAllocationOffset + nChannelAllocationBytes ||
	     pHeader->m_nPayloadDirectoryOffset < pHeader->m_nPageOffset + nPageBytes ||
	     pHeader->m_nClusterDirectoryOffset < pHeader->m_nPayloadDirectoryOffset + nPayloadBytes ||
	     pHeader->m_nTailMetadataOffset < pHeader->m_nClusterDirectoryOffset + nClusterBytes ||
	     pHeader->m_nQualityViewOffset < pHeader->m_nTailMetadataOffset + nTailBytes ||
	     pHeader->m_nFormatFamilyOffset < pHeader->m_nQualityViewOffset + nQualityViewBytes ||
	     pHeader->m_nPayloadDataOffset < pHeader->m_nFormatFamilyOffset + nFormatFamilyBytes ||
	     pHeader->m_nPayloadDataOffset + pHeader->m_nPayloadDataBytes != nPackageBytes ) {
		return false;
	}

	rView.m_pHeader.m_Value = Header;
	rView.m_pHeader.m_bValid = true;
	rView.m_pAllocations.m_pBytes = pBytes + Header.m_nAllocationOffset;
	rView.m_pAllocationKeys.m_pBytes = pBytes + Header.m_nAllocationKeyOffset;
	rView.m_pChannelAllocations.m_pBytes = pBytes + Header.m_nChannelAllocationOffset;
	rView.m_pPages.m_pBytes = pBytes + Header.m_nPageOffset;
	rView.m_pPayloads.m_pBytes = pBytes + Header.m_nPayloadDirectoryOffset;
	rView.m_pClusters.m_pBytes = pBytes + Header.m_nClusterDirectoryOffset;
	rView.m_pTails.m_pBytes = pBytes + Header.m_nTailMetadataOffset;
	rView.m_pQualityViews.m_pBytes = pBytes + Header.m_nQualityViewOffset;
	rView.m_pFormatFamilies.m_pBytes = pBytes + Header.m_nFormatFamilyOffset;
	rView.m_pPayloadData = bMetadataOnly ? NULL : pBytes + Header.m_nPayloadDataOffset;
	rView.m_nAllocationCount = Header.m_nAllocationCount;
	rView.m_nChannelAllocationCount = Header.m_nChannelAllocationCount;
	rView.m_nPageCount = Header.m_nPageCount;
	rView.m_nPayloadCount = Header.m_nPayloadCount;
	rView.m_nClusterCount = Header.m_nClusterCount;
	rView.m_nTailCount = Header.m_nTailCount;
	rView.m_nQualityViewCount = Header.m_nQualityViewCount;
	rView.m_nFormatFamilyCount = Header.m_nFormatFamilyCount;
	rView.m_nPayloadDataBytes = Header.m_nPayloadDataBytes;

	if ( !t3d_vt_pack_validate_records( rView ) ||
	     pHeader->m_nFullySupportedProfileMask != t3d_vt_pack_fully_supported_mask_impl( rView ) ) {
		rView = t3dVTPackBaselineView_t{};
		return false;
	}

	uint32_t nChannelPageCount[T3D_VT_LOGICAL_CHANNEL_COUNT];
	uint32_t nChannelTailCount[T3D_VT_LOGICAL_CHANNEL_COUNT];
	memset( nChannelPageCount, 0, sizeof( nChannelPageCount ) );
	memset( nChannelTailCount, 0, sizeof( nChannelTailCount ) );
	for ( uint32_t iPage = 0u; iPage < pHeader->m_nPageCount; ++iPage ) {
		++nChannelPageCount[rView.m_pPages[iPage].m_nChannel];
	}
	for ( uint32_t iTail = 0u; iTail < pHeader->m_nTailCount; ++iTail ) {
		++nChannelTailCount[rView.m_pTails[iTail].m_nChannel];
	}
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		if ( nChannelPageCount[iChannel] != pHeader->m_nChannelPageCount[iChannel] ||
		     nChannelTailCount[iChannel] != pHeader->m_nChannelTailCount[iChannel] ) {
			rView = t3dVTPackBaselineView_t{};
			return false;
		}
	}
	for ( uint32_t iCluster = 0u; !bMetadataOnly && iCluster < pHeader->m_nClusterCount; ++iCluster ) {
		const t3dVTPackClusterRecord_t& rCluster = rView.m_pClusters[iCluster];
		if ( rCluster.m_nCodec == T3D_VT_PACK_CODEC_NONE ) {
			const uint8_t *const pClusterBytes = rView.m_pPayloadData + rCluster.m_nPayloadDataOffset;
			if ( t3d_vt_pack_checksum32( pClusterBytes, rCluster.m_nStoredBytes ) != rCluster.m_nChecksum ||
			     t3d_vt_pack_hash64( pClusterBytes, rCluster.m_nStoredBytes ) != rCluster.m_nContentSignature ) {
				rView = t3dVTPackBaselineView_t{};
				return false;
			}
		}
	}
	return true;
}

bool t3d_vt_pack_open_baseline_view( const void *pPackage, const uint64_t nPackageBytes, t3dVTPackBaselineView_t& rView )
{
	return t3d_vt_pack_open_view_impl( pPackage, nPackageBytes, nPackageBytes, false, rView );
}

bool t3d_vt_pack_open_metadata_view( const void *pPrefix, const uint64_t nPrefixBytes, const uint64_t nFileBytes,
                                     t3dVTPackBaselineView_t& rView )
{
	return t3d_vt_pack_open_view_impl( pPrefix, nPrefixBytes, nFileBytes, true, rView );
}

uint32_t t3d_vt_pack_fully_supported_profile_mask( const t3dVTSerializedPackSource_t& rSource )
{
	return t3d_vt_pack_fully_supported_mask_impl( rSource );
}
