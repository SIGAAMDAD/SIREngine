// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "rendering/rd/vt/io/terrain_3d_vt_pack_runtime.h"
#include "rendering/rd/vt/io/terrain_3d_vt_compact_tail_layout.h"
#include "core/utility/terrain_3d_checked_math.h"
#include "rendering/rd/vt/io/terrain_3d_vt_file_source.h"
#include <new>

#include <limits.h>
#include <string.h>

static bool t3d_vt_pack_runtime_tail_block_layout( const uint32_t nChannel, uint32_t& rBlockWidth, uint32_t& rBlockHeight,
                                                   uint32_t& rBlockBytes )
{
	rBlockWidth = 0u;
	rBlockHeight = 0u;
	rBlockBytes = 0u;
	switch ( nChannel ) {
	case T3D_VT_CHANNEL_ALBEDO:
		rBlockWidth = 4u;
		rBlockHeight = 4u;
		rBlockBytes = 8u;
		return true;
	case T3D_VT_CHANNEL_NORMAL:
	case T3D_VT_CHANNEL_SURFACE:
		rBlockWidth = 4u;
		rBlockHeight = 4u;
		rBlockBytes = 16u;
		return true;
	case T3D_VT_CHANNEL_HEIGHT:
		rBlockWidth = 1u;
		rBlockHeight = 1u;
		rBlockBytes = 1u;
		return true;
	default:
		return false;
	}
}

void t3d_vt_pack_runtime_clear( t3dVTPackRuntimeStorage_t& rStorage )
{
	rStorage = t3dVTPackRuntimeStorage_t{};
}

bool t3d_vt_pack_runtime_transfer( t3dVTPackRuntimeStorage_t& rDestination, t3dVTPackRuntimeStorage_t& rSource )
{
	if ( &rDestination == &rSource || rDestination.pOwnedBlock != NULL || rSource.pOwnedBlock == NULL ) {
		return false;
	}
	rDestination = rSource;
	t3d_vt_pack_runtime_clear( rSource );
	return true;
}

static bool t3d_vt_pack_runtime_raw_clusters_only( const t3dVTPackBaselineView_t& rView )
{
	if ( rView.m_pHeader == NULL || rView.m_pClusters == NULL ) {
		return false;
	}
	for ( uint32_t iCluster = 0u; iCluster < rView.m_pHeader->m_nClusterCount; ++iCluster ) {
		if ( rView.m_pClusters[iCluster].m_nCodec != T3D_VT_PACK_CODEC_NONE ) {
			return false;
		}
	}
	return true;
}

#define T3D_VT_RUNTIME_ADD( count, type )                                                                                                  \
	do {                                                                                                                                   \
		if ( ( uint64_t )( count ) > ( UINT64_MAX - nBytes ) / sizeof( type ) ) {                                                          \
			return false;                                                                                                                  \
		}                                                                                                                                  \
		nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )( count ) * sizeof( type );                                       \
	} while ( 0 )

#define T3D_VT_RUNTIME_TAKE( field, count, type )                                                                                          \
	do {                                                                                                                                   \
		Candidate.field = ( type * )( pBase + nOffset );                                                                                   \
		nOffset = t3dAlignUpOrZero<uint64_t>( nOffset + ( uint64_t )( count ) * sizeof( type ), 64u );                                     \
	} while ( 0 )

static bool t3d_vt_pack_runtime_open_cold( const void *const pPackage, const uint64_t nPackageBytes,
                                           const t3dVTPackRuntimeAllocator_t& rAllocator, t3dVTPackRuntimeStorage_t& rStorage,
                                           const bool bMetadataOnly, t3dVTFileSource_s *const pFileSource )
{
	if ( pPackage == NULL || nPackageBytes == 0u || rAllocator.pAllocate == NULL || rAllocator.pFree == NULL ||
	     rStorage.pOwnedBlock != NULL ) {
		return false;
	}
	t3dVTPackBaselineView_t SourceView;
	if ( !( bMetadataOnly ? t3d_vt_pack_open_metadata_view( pPackage, nPackageBytes, pFileSource->nFileBytes, SourceView )
	                      : t3d_vt_pack_open_baseline_view( pPackage, nPackageBytes, SourceView ) ) ||
	     !t3d_vt_pack_runtime_raw_clusters_only( SourceView ) ) {
		return false;
	}
	const t3dVTPackHeader_t& Header = *SourceView.m_pHeader;
	if ( Header.m_nAllocationCount == 0u || Header.m_nPayloadCount == 0u || Header.m_nQualityViewCount == 0u ||
	     Header.m_nFormatFamilyCount != T3D_VT_LOGICAL_CHANNEL_COUNT ||
	     Header.m_nAllocationCount > UINT32_MAX / T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION ) {
		return false;
	}
	const uint32_t nRuntimeAllocationCount = Header.m_nAllocationCount * T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION;

	uint64_t nBytes = t3dAlignUpOrZero<uint64_t>( nPackageBytes, 64u );
	T3D_VT_RUNTIME_ADD( 1u, t3dVTFileHeader_t );
	T3D_VT_RUNTIME_ADD( nRuntimeAllocationCount, t3dVTAllocationRecord_t );
	T3D_VT_RUNTIME_ADD( Header.m_nPageCount, t3dVTPageFileRecord_t );
	T3D_VT_RUNTIME_ADD( Header.m_nPayloadCount, t3dVTPayloadRecord_t );
	T3D_VT_RUNTIME_ADD( nRuntimeAllocationCount, t3dVTAllocation_t );
	T3D_VT_RUNTIME_ADD( Header.m_nPayloadCount, t3dVTRuntimePayloadRecord_t );
	T3D_VT_RUNTIME_ADD( nRuntimeAllocationCount, t3dVTTailMetadata_t );
	T3D_VT_RUNTIME_ADD( nRuntimeAllocationCount, t3dVTRuntimeTailSourceRecord_t );
#undef T3D_VT_RUNTIME_ADD
	if ( nBytes == 0u || nBytes > SIZE_MAX ) {
		return false;
	}

	void *const pBlock = rAllocator.pAllocate( rAllocator.pUser, ( size_t )nBytes, 64u );
	if ( pBlock == NULL ) {
		return false;
	}
	memset( pBlock, 0, ( size_t )nBytes );
	t3dVTPackRuntimeStorage_t Candidate;
	t3d_vt_pack_runtime_clear( Candidate );
	Candidate.pOwnedBlock = pBlock;
	Candidate.nOwnedBytes = nBytes;
	uint8_t *const pBase = ( uint8_t * )pBlock;
	uint64_t nOffset = 0u;
	Candidate.pPackageBytes = pBase;
	Candidate.nPackageBytes = bMetadataOnly ? Header.m_nPackageBytes : nPackageBytes;
	Candidate.pFileSource = pFileSource;
	if ( pFileSource != NULL ) {
		pFileSource->nGeneration = Header.m_nPackageGeneration;
	}
	memcpy( Candidate.pPackageBytes, pPackage, ( size_t )nPackageBytes );
	nOffset = t3dAlignUpOrZero<uint64_t>( nPackageBytes, 64u );
	if ( !( bMetadataOnly
	            ? t3d_vt_pack_open_metadata_view( Candidate.pPackageBytes, nPackageBytes, Header.m_nPackageBytes, Candidate.PackViewV1 )
				: t3d_vt_pack_open_baseline_view( Candidate.pPackageBytes, nPackageBytes, Candidate.PackViewV1 ) ) ||
	     !t3d_vt_pack_runtime_raw_clusters_only( Candidate.PackViewV1 ) ) {
		rAllocator.pFree( rAllocator.pUser, pBlock );
		return false;
	}

	T3D_VT_RUNTIME_TAKE( pRuntimeHeader, 1u, t3dVTFileHeader_t );
	T3D_VT_RUNTIME_TAKE( pLegacyAllocations, nRuntimeAllocationCount, t3dVTAllocationRecord_t );
	T3D_VT_RUNTIME_TAKE( pLegacyPages, Header.m_nPageCount, t3dVTPageFileRecord_t );
	T3D_VT_RUNTIME_TAKE( pLegacyPayloads, Header.m_nPayloadCount, t3dVTPayloadRecord_t );
	T3D_VT_RUNTIME_TAKE( pRuntimeAllocations, nRuntimeAllocationCount, t3dVTAllocation_t );
	T3D_VT_RUNTIME_TAKE( pRuntimePayloads, Header.m_nPayloadCount, t3dVTRuntimePayloadRecord_t );
	T3D_VT_RUNTIME_TAKE( pRuntimeTails, nRuntimeAllocationCount, t3dVTTailMetadata_t );
	T3D_VT_RUNTIME_TAKE( pRuntimeTailSources, nRuntimeAllocationCount, t3dVTRuntimeTailSourceRecord_t );
#undef T3D_VT_RUNTIME_TAKE

	t3dVTFileHeader_t& RuntimeHeader = *Candidate.pRuntimeHeader;
	RuntimeHeader.nMagic = T3D_VT_FILE_MAGIC;
	RuntimeHeader.nVersion = T3D_VT_FILE_FORMAT_VERSION;
	RuntimeHeader.nHeaderBytes = sizeof( t3dVTFileHeader_t );
	RuntimeHeader.nAllocationCount = nRuntimeAllocationCount;
	RuntimeHeader.nPageCount = Header.m_nPageCount;
	RuntimeHeader.nPayloadCount = Header.m_nPayloadCount;
	RuntimeHeader.nChannelCount = T3D_VT_PHYSICAL_FORMAT_COUNT;
	RuntimeHeader.nPayloadDataBytes = Header.m_nPayloadDataBytes;
	RuntimeHeader.nContentSignature = Header.m_nContentSignature;
	RuntimeHeader.nInteriorTexels = T3D_VT_PAGE_INTERIOR;
	RuntimeHeader.nGutterTexels = T3D_VT_PAGE_GUTTER;
	RuntimeHeader.nPhysicalTexels = T3D_VT_PHYSICAL_TILE;

	// Two 64x32 COMPOSED roots share one V1 128x64 tail slot. Retain the
	// original 64-row alignment so resource planning, shader coordinates and
	// the existing RD tail uploader need no ABI or descriptor changes.
	uint32_t nFamilyTailHalfSlots[T3D_VT_PHYSICAL_FORMAT_COUNT] = {};
	for ( uint32_t iAllocation = 0u; iAllocation < Header.m_nAllocationCount; ++iAllocation ) {
		const t3dVTPackAllocationRecord_t& SourceAllocation = Candidate.PackViewV1.m_pAllocations[iAllocation];
		const t3dVTConsumer_t Consumer = ( t3dVTConsumer_t )SourceAllocation.m_nConsumer;
		const uint32_t nRuntimeBase = t3d_vt_runtime_allocation_base( iAllocation );
		for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
			uint32_t nSlot = T3D_VT_INVALID_SLOT;
			if ( !t3d_vt_runtime_slot_for_channel( Consumer, ( t3dVTLogicalChannel_t )iChannel, nSlot ) ) {
				continue;
			}
			const uint32_t nRuntimeId = nRuntimeBase + nSlot;
			const t3dVTChannelAllocation_t& SourceChannel =
			    Candidate.PackViewV1.m_pChannelAllocations[iAllocation * T3D_VT_LOGICAL_CHANNEL_COUNT + iChannel];
			t3dVTAllocationRecord_t& Legacy = Candidate.pLegacyAllocations[nRuntimeId];
			Legacy.nReserved0 = T3D_VT_RESIDENCY_TERRAIN;
			if ( SourceChannel.m_nPageCount == 0u ) {
				Candidate.pRuntimeAllocations[nRuntimeId].m_nTailIndex = T3D_VT_INVALID_TAIL;
				for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c )
					Candidate.pRuntimeTailSources[nRuntimeId].nPayloadId[c] = T3D_VT_INVALID_PAGE;
				continue;
			}
			if ( SourceChannel.m_nTailIndex >= Header.m_nTailCount ) {
				rAllocator.pFree( rAllocator.pUser, pBlock );
				return false;
			}
			const t3dVTPackTailRecord_t& SourceTail = Candidate.PackViewV1.m_pTails[SourceChannel.m_nTailIndex];
			if ( SourceTail.m_nChannel != iChannel || SourceTail.m_nPayloadId >= Header.m_nPayloadCount ||
			     SourceTail.m_nFirstMipWidth == 0u || SourceTail.m_nFirstMipHeight == 0u ||
			     SourceTail.m_nFirstMipWidth > T3D_VT_PACK_TAIL_SLOT_HEIGHT ||
			     SourceTail.m_nFirstMipHeight > T3D_VT_PACK_TAIL_SLOT_HEIGHT || SourceTail.m_nAtlasInteriorWidth == 0u ||
			     SourceTail.m_nAtlasInteriorHeight == 0u || SourceTail.m_nAtlasInteriorWidth > T3D_VT_PACK_TAIL_SLOT_WIDTH ||
			     SourceTail.m_nAtlasInteriorHeight > T3D_VT_PACK_TAIL_SLOT_HEIGHT ||
			     SourceTail.m_nLayout != T3D_VT_PACK_TAIL_LAYOUT_HORIZONTAL ) {
				rAllocator.pFree( rAllocator.pUser, pBlock );
				return false;
			}
			t3dVTAllocation_t& RuntimeAllocation = Candidate.pRuntimeAllocations[nRuntimeId];
			RuntimeAllocation.m_nBaseVirtualPage = SourceChannel.m_nBaseVirtualPage;
			RuntimeAllocation.m_nPageCount = SourceChannel.m_nPageCount;
			RuntimeAllocation.m_nMip0WidthPages = SourceChannel.m_nMip0WidthPages;
			RuntimeAllocation.m_nMip0HeightPages = SourceChannel.m_nMip0HeightPages;
			RuntimeAllocation.m_nMipCount = SourceChannel.m_nMipCount;
			RuntimeAllocation.m_nFlags = SourceChannel.m_nFlags;
			RuntimeAllocation.m_nTailIndex = nRuntimeId;
			RuntimeAllocation.m_nGeneration = SourceAllocation.m_nGeneration;
			RuntimeAllocation.m_nContentId = SourceAllocation.m_nContentId;
			RuntimeAllocation.m_nReserved = T3D_VT_RESIDENCY_TERRAIN;

			Legacy.nBaseVirtualPage = SourceChannel.m_nBaseVirtualPage;
			Legacy.nPageCount = SourceChannel.m_nPageCount;
			Legacy.nMip0WidthPages = SourceChannel.m_nMip0WidthPages;
			Legacy.nMip0HeightPages = SourceChannel.m_nMip0HeightPages;
			Legacy.nMipCount = SourceChannel.m_nMipCount;
			Legacy.nChannel =
			    ( uint16_t )t3d_vt_logical_channel_physical_family( ( t3dVTLogicalChannel_t )iChannel, SourceChannel.m_nFlags );
			Legacy.nFlags = SourceChannel.m_nFlags;
			Legacy.nRootPage = SourceChannel.m_nPageCount != 0u ? SourceChannel.m_nBaseVirtualPage + SourceChannel.m_nPageCount - 1u : 0u;
			Legacy.nReserved1 = SourceAllocation.m_nContentId;

			t3dVTTailMetadata_t& RuntimeTail = Candidate.pRuntimeTails[nRuntimeId];
			const uint32_t nFamily =
			    ( uint32_t )t3d_vt_logical_channel_physical_family( ( t3dVTLogicalChannel_t )iChannel, SourceChannel.m_nFlags );
			// Pack only COMPOSED chains which fit the left/right half. Other
			// consumers retain their full 128x64 layout and a half-slot gap
			// is deliberately skipped before them to prevent any overlap.
			const bool bCompact = Consumer == T3D_VT_CONSUMER_COMPOSED &&
			                      SourceTail.m_nAtlasInteriorWidth <= T3D_VT_PACK_TAIL_SLOT_WIDTH / 2u &&
			                      SourceTail.m_nAtlasInteriorHeight <= T3D_VT_PACK_TAIL_SLOT_HEIGHT;
			if ( !t3dVtPlaceCompactTailCold( nFamilyTailHalfSlots[nFamily], bCompact, T3D_VT_PACK_TAILS_PER_ROW,
			                                 T3D_VT_PACK_TAIL_SLOT_WIDTH, T3D_VT_PACK_TAIL_SLOT_HEIGHT, RuntimeTail.m_nAtlasX,
			                                 RuntimeTail.m_nAtlasY ) ) {
				rAllocator.pFree( rAllocator.pUser, pBlock );
				return false;
			}
			RuntimeTail.m_nWidthTexels = SourceTail.m_nFirstMipWidth;
			RuntimeTail.m_nHeightTexels = SourceTail.m_nFirstMipHeight;
			RuntimeTail.m_nFirstMip = SourceTail.m_nFirstMip;
			RuntimeTail.m_nMipCount = SourceTail.m_nMipCount;
			RuntimeTail.m_nGeneration = SourceAllocation.m_nGeneration;
			RuntimeTail.m_nChannelMask = 1u << nFamily;
			RuntimeTail.m_nContentId = SourceTail.m_nContentId;
			for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c )
				Candidate.pRuntimeTailSources[nRuntimeId].nPayloadId[c] = T3D_VT_INVALID_PAGE;
			Candidate.pRuntimeTailSources[nRuntimeId].nPayloadId[nFamily] = SourceTail.m_nPayloadId;
			Candidate.pRuntimeTailSources[nRuntimeId].nAtlasInteriorWidth = SourceTail.m_nAtlasInteriorWidth;
			Candidate.pRuntimeTailSources[nRuntimeId].nAtlasInteriorHeight = SourceTail.m_nAtlasInteriorHeight;
		}
	}

	for ( uint32_t iPage = 0u; iPage < Header.m_nPageCount; ++iPage ) {
		const t3dVTPackPageRecord_t& SourcePage = Candidate.PackViewV1.m_pPages[iPage];
		if ( SourcePage.m_nAllocationId >= Header.m_nAllocationCount || SourcePage.m_nChannel >= T3D_VT_LOGICAL_CHANNEL_COUNT ) {
			rAllocator.pFree( rAllocator.pUser, pBlock );
			return false;
		}
		const t3dVTConsumer_t Consumer = ( t3dVTConsumer_t )Candidate.PackViewV1.m_pAllocations[SourcePage.m_nAllocationId].m_nConsumer;
		uint32_t nSlot = T3D_VT_INVALID_SLOT;
		if ( !t3d_vt_runtime_slot_for_channel( Consumer, ( t3dVTLogicalChannel_t )SourcePage.m_nChannel, nSlot ) ) {
			rAllocator.pFree( rAllocator.pUser, pBlock );
			return false;
		}
		t3dVTPageFileRecord_t& RuntimePage = Candidate.pLegacyPages[iPage];
		RuntimePage.nVirtualPage = SourcePage.m_nVirtualPage;
		RuntimePage.nParentPage = SourcePage.m_nParentPage;
		RuntimePage.nAllocationId = t3d_vt_runtime_allocation_base( SourcePage.m_nAllocationId ) + nSlot;
		RuntimePage.nPayloadId = SourcePage.m_nPayloadId;
		RuntimePage.nMip = SourcePage.m_nMip;
		RuntimePage.nChannel = ( uint16_t )Candidate.PackViewV1.m_pPayloads[SourcePage.m_nPayloadId].m_nFormat;
		RuntimePage.nFlags = SourcePage.m_nFlags;
		RuntimePage.nGeneration = SourcePage.m_nGeneration;
		RuntimePage.nReserved1 = SourcePage.m_nContentId;
	}

	for ( uint32_t iPayload = 0u; iPayload < Header.m_nPayloadCount; ++iPayload ) {
		const t3dVTPackPayloadRecord_t& Payload = Candidate.PackViewV1.m_pPayloads[iPayload];
		const t3dVTPackClusterRecord_t& Cluster = Candidate.PackViewV1.m_pClusters[Payload.m_nClusterId];
		if ( Cluster.m_nCodec != T3D_VT_PACK_CODEC_NONE || Payload.m_nClusterOffset > Cluster.m_nDecodedBytes ||
		     Payload.m_nDecodedBytes > Cluster.m_nDecodedBytes - Payload.m_nClusterOffset ) {
			rAllocator.pFree( rAllocator.pUser, pBlock );
			return false;
		}
		const uint64_t nPayloadOffset = Cluster.m_nPayloadDataOffset + Payload.m_nClusterOffset;
		t3dVTRuntimePayloadRecord_t& RuntimePayload = Candidate.pRuntimePayloads[iPayload];
		RuntimePayload.nOffset = nPayloadOffset;
		RuntimePayload.nBytes = Payload.m_nDecodedBytes;
		RuntimePayload.nChannel = Payload.m_nFormat;
		RuntimePayload.nContentId = ( uint32_t )( Payload.m_nContentSignature ^ ( Payload.m_nContentSignature >> 32u ) );
		RuntimePayload.nChecksum = Payload.m_nChecksum;
		t3dVTPayloadRecord_t& LegacyPayload = Candidate.pLegacyPayloads[iPayload];
		LegacyPayload.nOffset = nPayloadOffset;
		LegacyPayload.nCompressedBytes = Payload.m_nDecodedBytes;
		LegacyPayload.nDecodedBytes = Payload.m_nDecodedBytes;
		LegacyPayload.nCodec = T3D_VT_PACK_CODEC_NONE;
		LegacyPayload.nChecksum = Payload.m_nChecksum;
	}

	Candidate.Database.pHeader = Candidate.pRuntimeHeader;
	Candidate.Database.pAllocations = Candidate.pLegacyAllocations;
	Candidate.Database.pPages = Candidate.pLegacyPages;
	Candidate.Database.pPayloads = Candidate.pLegacyPayloads;
	Candidate.Database.pPayloadData = Candidate.PackViewV1.m_pPayloadData;
	Candidate.Database.pFileSource = pFileSource;
	Candidate.Database.nFileBytes = Candidate.nPackageBytes;
	Candidate.Database.pRuntimeAllocations = Candidate.pRuntimeAllocations;
	Candidate.Database.pRuntimeTailMetadata = Candidate.pRuntimeTails;
	Candidate.Database.pRuntimeTailSources = Candidate.pRuntimeTailSources;
	Candidate.Database.nRuntimeTailCount = nRuntimeAllocationCount;
	Candidate.Database.pRuntimePayloads = Candidate.pRuntimePayloads;
	Candidate.Database.nRuntimePayloadCount = Header.m_nPayloadCount;
	Candidate.Database.pV1Allocations = Candidate.PackViewV1.m_pAllocations;
	Candidate.Database.pV1ChannelAllocations = Candidate.PackViewV1.m_pChannelAllocations;
	Candidate.Database.pV1Pages = Candidate.PackViewV1.m_pPages;
	Candidate.Database.pV1Tails = Candidate.PackViewV1.m_pTails;
	Candidate.Database.pV1QualityViews = Candidate.PackViewV1.m_pQualityViews;
	Candidate.Database.pV1FormatFamilies = Candidate.PackViewV1.m_pFormatFamilies;
	Candidate.Database.nV1ChannelAllocationCount = Header.m_nChannelAllocationCount;
	Candidate.Database.nV1PageCount = Header.m_nPageCount;
	Candidate.Database.nV1TailCount = Header.m_nTailCount;
	Candidate.Database.nV1QualityViewCount = Header.m_nQualityViewCount;
	Candidate.Database.nV1FormatFamilyCount = Header.m_nFormatFamilyCount;
	Candidate.Database.nV1FullySupportedProfileMask = Header.m_nFullySupportedProfileMask;
	Candidate.Database.nPackageGeneration = Header.m_nPackageGeneration;
	Candidate.Database.nPackageContentSignature = Header.m_nContentSignature;
	Candidate.Database.nRuntimeKind = T3D_VT_RUNTIME_DATABASE_PACK;

	return t3d_vt_pack_runtime_transfer( rStorage, Candidate );
}

static bool t3d_vt_pack_runtime_find_quality_view( const t3dVTDatabaseView_t& rDatabase, const uint32_t nAllocationId,
                                                   const uint32_t nProfile, t3dVTPackQualityView_t& rResult )
{
	uint32_t nLow = 0u;
	uint32_t nHigh = rDatabase.nV1QualityViewCount;
	while ( nLow < nHigh ) {
		const uint32_t nMid = nLow + ( nHigh - nLow ) / 2u;
		const t3dVTPackQualityView_t& View = rDatabase.pV1QualityViews[nMid];
		const uint64_t nKey = ( ( uint64_t )View.m_nAllocationId << 8u ) | View.m_nProfile;
		const uint64_t nWanted = ( ( uint64_t )nAllocationId << 8u ) | nProfile;
		if ( nKey < nWanted )
			nLow = nMid + 1u;
		else
			nHigh = nMid;
	}
	if ( nLow >= rDatabase.nV1QualityViewCount ) {
		return false;
	}
	const t3dVTPackQualityView_t& View = rDatabase.pV1QualityViews[nLow];
	rResult = View;
	return View.m_nAllocationId == nAllocationId && View.m_nProfile == nProfile;
}

bool t3d_vt_pack_runtime_apply_quality_profile( const t3dVTDatabaseView_t& rDatabase, t3dVTAllocation_t *const pRuntimeAllocations,
                                                const uint32_t nRuntimeAllocationCount, const uint32_t nRequestedProfile,
                                                t3dVTQualityRuntimeStats_t& rStats )
{
	memset( &rStats, 0, sizeof( rStats ) );
	rStats.nRequestedProfile = nRequestedProfile;
	rStats.nPackageFullySupportedProfileMask = rDatabase.nV1FullySupportedProfileMask;
	if ( rDatabase.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK || rDatabase.pHeader == NULL || rDatabase.pV1Allocations == NULL ||
	     rDatabase.pV1QualityViews == NULL || pRuntimeAllocations == NULL || nRequestedProfile >= T3D_VT_QUALITY_PROFILE_COUNT )
		return false;
	const uint32_t nLogicalAllocationCount = rDatabase.pHeader->nAllocationCount / T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION;
	if ( nLogicalAllocationCount == 0u || nRuntimeAllocationCount != nLogicalAllocationCount * T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION ) {
		return false;
	}
	for ( uint32_t iAllocation = 0u; iAllocation < nLogicalAllocationCount; ++iAllocation ) {
		const t3dVTPackAllocationRecord_t& Source = rDatabase.pV1Allocations[iAllocation];
		if ( Source.m_nHighestAvailableProfile >= T3D_VT_QUALITY_PROFILE_COUNT ) {
			return false;
		}
		const uint32_t nResolvedProfile =
		    nRequestedProfile < Source.m_nHighestAvailableProfile ? nRequestedProfile : Source.m_nHighestAvailableProfile;
		if ( ( Source.m_nAvailableProfileMask & ( 1u << nResolvedProfile ) ) == 0u ) {
			return false;
		}
		t3dVTPackQualityView_t View{};
		if ( !t3d_vt_pack_runtime_find_quality_view( rDatabase, iAllocation, nResolvedProfile, View ) ) {
			return false;
		}
		++rStats.nAllocationsByHighestProfile[Source.m_nHighestAvailableProfile];
		++rStats.nAllocationsByResolvedProfile[nResolvedProfile];
		if ( nResolvedProfile != nRequestedProfile )
			++rStats.nFallbackAllocationCount;
		const t3dVTConsumer_t Consumer = ( t3dVTConsumer_t )Source.m_nConsumer;
		for ( uint32_t nSlot = 0u; nSlot < T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION; ++nSlot ) {
			t3dVTLogicalChannel_t Channel = T3D_VT_LOGICAL_CHANNEL_ALBEDO;
			if ( !t3d_vt_logical_channel_for_runtime_slot( Consumer, nSlot, Channel ) ) {
				return false;
			}
			t3dVTAllocation_t& Runtime = pRuntimeAllocations[t3d_vt_runtime_allocation_base( iAllocation ) + nSlot];
			Runtime.m_nFlags &= ( uint16_t )~T3D_VT_ALLOCATION_RUNTIME_QUALITY_MASK;
			if ( ( View.m_nChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( Channel ) ) == 0u ) {
				Runtime.m_nFlags |= T3D_VT_ALLOCATION_QUALITY_DISABLED;
				continue;
			}
			const uint32_t nBaseMip = View.m_nBaseMip[Channel];
			const uint32_t nMaxMip = View.m_nMaxMip[Channel];
			if ( nBaseMip == T3D_VT_PACK_INVALID_MIP || nBaseMip > nMaxMip || nMaxMip >= Runtime.m_nMipCount || nBaseMip > 0x0fu ) {
				return false;
			}
			Runtime.m_nFlags |= ( uint16_t )( nBaseMip << T3D_VT_ALLOCATION_QUALITY_BASE_MIP_SHIFT );
		}
	}
	return true;
}

bool t3d_vt_pack_runtime_open_cold( const void *const pPackage, const uint64_t nPackageBytes, const t3dVTPackRuntimeAllocator_t& rAllocator,
                                    t3dVTPackRuntimeStorage_t& rStorage )
{
	if ( pPackage == NULL || nPackageBytes < sizeof( uint32_t ) * 2u || rStorage.pOwnedBlock != NULL ) {
		return false;
	}
	uint32_t nPrefix[2];
	memcpy( nPrefix, pPackage, sizeof( nPrefix ) );
	if ( nPrefix[0] != T3D_VT_FILE_MAGIC ) {
		return false;
	}
	if ( nPrefix[1] != T3D_VT_FILE_FORMAT_VERSION ) {
		return false;
	}
	return t3d_vt_pack_runtime_open_cold( pPackage, nPackageBytes, rAllocator, rStorage, false, NULL );
}

bool t3d_vt_pack_runtime_open_file_cold( const godot::String& rPath, const t3dVTPackRuntimeAllocator_t& rAllocator,
                                         t3dVTPackRuntimeStorage_t& rStorage )
{
	if ( rStorage.pOwnedBlock != NULL ) {
		return false;
	}
	t3dVTFileSource_s *const pSource = new ( std::nothrow ) t3dVTFileSource_s();
	if ( pSource == NULL ) {
		return false;
	}
	godot::PackedByteArray metadata;
	if ( !t3d_vt_file_source_open_cold( rPath, *pSource, metadata, false ) ||
	     !t3d_vt_pack_runtime_open_cold( metadata.ptr(), ( uint64_t )metadata.size(), rAllocator, rStorage, true, pSource ) ) {
		t3d_vt_file_source_close( *pSource );
		delete pSource;
		return false;
	}
	return true;
}

void t3d_vt_pack_runtime_release_cold( const t3dVTPackRuntimeAllocator_t& rAllocator, t3dVTPackRuntimeStorage_t& rStorage )
{
	if ( rStorage.pOwnedBlock != NULL && rAllocator.pFree != NULL ) {
		rAllocator.pFree( rAllocator.pUser, rStorage.pOwnedBlock );
	}
	if ( rStorage.pFileSource != NULL ) {
		t3d_vt_file_source_close( *rStorage.pFileSource );
		delete rStorage.pFileSource;
	}
	t3d_vt_pack_runtime_clear( rStorage );
}

bool t3d_vt_pack_runtime_find_allocation_by_stable_key( const t3dVTPackRuntimeStorage_t& rStorage, const uint64_t nStableKey,
                                                        uint32_t& rAllocationId )
{
	rAllocationId = T3D_VT_INVALID_ALLOCATION;
	if ( nStableKey == 0u ) {
		return false;
	}
	const auto& pKeys = rStorage.PackViewV1.m_pAllocationKeys;
	const uint32_t nAllocationCount = rStorage.PackViewV1.m_pHeader != NULL ? rStorage.PackViewV1.m_pHeader->m_nAllocationCount : 0u;
	if ( rStorage.Database.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK || pKeys == NULL || nAllocationCount == 0u ) {
		return false;
	}
	uint32_t nLow = 0u;
	uint32_t nHigh = nAllocationCount;
	while ( nLow < nHigh ) {
		const uint32_t nMiddle = nLow + ( ( nHigh - nLow ) >> 1u );
		const uint64_t nKey = pKeys[nMiddle].m_nStableKey;
		if ( nKey < nStableKey ) {
			nLow = nMiddle + 1u;
		} else {
			nHigh = nMiddle;
		}
	}
	if ( nLow >= nAllocationCount || pKeys[nLow].m_nStableKey != nStableKey ) {
		return false;
	}
	rAllocationId = pKeys[nLow].m_nAllocationId;
	return rAllocationId < nAllocationCount;
}

bool t3d_vt_pack_runtime_copy_tail_mip_chain( const t3dVTDatabaseView_t& rDatabase, const uint32_t nRuntimeTailIndex,
                                              const uint32_t nChannel, uint8_t *const pDestination, const uint64_t nDestinationBytes,
                                              const uint32_t nDestinationWidth, const uint32_t nDestinationHeight )
{
	if ( rDatabase.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK || rDatabase.pHeader == NULL || rDatabase.pRuntimeTailMetadata == NULL ||
	     rDatabase.pRuntimeTailSources == NULL || rDatabase.pRuntimePayloads == NULL ||
	     ( rDatabase.pPayloadData == NULL && rDatabase.pFileSource == NULL ) || nRuntimeTailIndex >= rDatabase.nRuntimeTailCount ||
	     nChannel >= T3D_VT_CHANNEL_COUNT || pDestination == NULL || nDestinationWidth == 0u || nDestinationHeight == 0u ) {
		return false;
	}
	const t3dVTTailMetadata_t& Tail = rDatabase.pRuntimeTailMetadata[nRuntimeTailIndex];
	const t3dVTRuntimeTailSourceRecord_t& TailSource = rDatabase.pRuntimeTailSources[nRuntimeTailIndex];
	const uint32_t nPayloadId = TailSource.nPayloadId[nChannel];
	if ( nPayloadId == T3D_VT_INVALID_PAGE ) {
		return true;
	}
	const uint32_t nChainWidth = TailSource.nAtlasInteriorWidth;
	const uint32_t nChainHeight = TailSource.nAtlasInteriorHeight;
	if ( nPayloadId >= rDatabase.nRuntimePayloadCount || Tail.m_nWidthTexels == 0u || Tail.m_nHeightTexels == 0u || nChainWidth == 0u ||
	     nChainHeight == 0u || nChainWidth > T3D_VT_PACK_TAIL_SLOT_WIDTH || nChainHeight > T3D_VT_PACK_TAIL_SLOT_HEIGHT ||
	     Tail.m_nAtlasX >= nDestinationWidth || nChainWidth > nDestinationWidth - Tail.m_nAtlasX || Tail.m_nAtlasY >= nDestinationHeight ||
	     nChainHeight > nDestinationHeight - Tail.m_nAtlasY ) {
		return false;
	}
	const t3dVTRuntimePayloadRecord_t& Payload = rDatabase.pRuntimePayloads[nPayloadId];
	if ( Payload.nChannel != nChannel || Payload.nOffset > rDatabase.pHeader->nPayloadDataBytes ||
	     Payload.nBytes > rDatabase.pHeader->nPayloadDataBytes - Payload.nOffset ) {
		return false;
	}

	uint32_t nBlockWidth = 0u, nBlockHeight = 0u, nBlockBytes = 0u;
	if ( !t3d_vt_pack_runtime_tail_block_layout( nChannel, nBlockWidth, nBlockHeight, nBlockBytes ) || Tail.m_nAtlasX % nBlockWidth != 0u ||
	     Tail.m_nAtlasY % nBlockHeight != 0u || T3D_VT_PAGE_GUTTER % nBlockWidth != 0u || T3D_VT_PAGE_GUTTER % nBlockHeight != 0u ) {
		return false;
	}
	const uint32_t nSourceBlocksWide = ( T3D_VT_PHYSICAL_TILE + nBlockWidth - 1u ) / nBlockWidth;
	const uint32_t nSourceBlocksHigh = ( T3D_VT_PHYSICAL_TILE + nBlockHeight - 1u ) / nBlockHeight;
	const uint32_t nDestinationBlocksWide = ( nDestinationWidth + nBlockWidth - 1u ) / nBlockWidth;
	const uint32_t nDestinationBlocksHigh = ( nDestinationHeight + nBlockHeight - 1u ) / nBlockHeight;
	// Copy the complete horizontal compact-tail chain. For BC formats this also
	// intentionally copies the final partially occupied compression block so the
	// 2x2/1x1 tail texels retain their original in-block coordinates.
	const uint32_t nCopyBlocksWide = ( nChainWidth + nBlockWidth - 1u ) / nBlockWidth;
	const uint32_t nCopyBlocksHigh = ( nChainHeight + nBlockHeight - 1u ) / nBlockHeight;
	const uint32_t nSourceBlockX = T3D_VT_PAGE_GUTTER / nBlockWidth;
	const uint32_t nSourceBlockY = T3D_VT_PAGE_GUTTER / nBlockHeight;
	const uint32_t nDestinationBlockX = Tail.m_nAtlasX / nBlockWidth;
	const uint32_t nDestinationBlockY = Tail.m_nAtlasY / nBlockHeight;
	const uint64_t nRequiredSourceBytes = ( uint64_t )nSourceBlocksWide * nSourceBlocksHigh * nBlockBytes;
	const uint64_t nRequiredDestinationBytes = ( uint64_t )nDestinationBlocksWide * nDestinationBlocksHigh * nBlockBytes;
	if ( nRequiredSourceBytes != Payload.nBytes || nRequiredDestinationBytes != nDestinationBytes ||
	     nSourceBlockX + nCopyBlocksWide > nSourceBlocksWide || nSourceBlockY + nCopyBlocksHigh > nSourceBlocksHigh ||
	     nDestinationBlockX + nCopyBlocksWide > nDestinationBlocksWide || nDestinationBlockY + nCopyBlocksHigh > nDestinationBlocksHigh ) {
		return false;
	}
	uint8_t scratch[T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE];
	const uint8_t *pSource = rDatabase.pPayloadData != NULL ? rDatabase.pPayloadData + Payload.nOffset : scratch;
	if ( rDatabase.pPayloadData == NULL ) {
		if ( Payload.nBytes > sizeof( scratch ) ||
		     !t3d_vt_file_source_read_payload( *const_cast<t3dVTFileSource_s *>( rDatabase.pFileSource ), Payload.nOffset, scratch,
		                                       ( uint32_t )Payload.nBytes ) ||
		     t3d_vt_pack_checksum32( scratch, Payload.nBytes ) != Payload.nChecksum ) {
			return false;
		}
	}
	const uint64_t nRowBytes = ( uint64_t )nCopyBlocksWide * nBlockBytes;
	for ( uint32_t iRow = 0u; iRow < nCopyBlocksHigh; ++iRow ) {
		const uint64_t nSourceOffset = ( ( uint64_t )( nSourceBlockY + iRow ) * nSourceBlocksWide + nSourceBlockX ) * nBlockBytes;
		const uint64_t nDestinationOffset =
		    ( ( uint64_t )( nDestinationBlockY + iRow ) * nDestinationBlocksWide + nDestinationBlockX ) * nBlockBytes;
		memcpy( pDestination + nDestinationOffset, pSource + nSourceOffset, ( size_t )nRowBytes );
	}
	return true;
}
