// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "authoring/vt/io/terrain_3d_vt_pack_writer.h"

#include "core/utility/terrain_3d_hash_bytes.h"
#include "core/utility/terrain_3d_checked_math.h"

#include <stddef.h>
#include <string.h>

static bool t3d_vt_pack_add_bytes( uint64_t& rOffset, const uint64_t nBytes )
{
	uint64_t nEnd = 0u;
	if ( !t3dCheckedAdd( rOffset, nBytes, nEnd ) ) {
		return false;
	}
	const uint64_t nAligned = t3dAlignUpOrZero<uint64_t>( nEnd, T3D_VT_PACK_ALIGNMENT );
	if ( nAligned == 0u ) {
		return false;
	}
	rOffset = nAligned;
	return true;
}

uint64_t t3d_vt_pack_compute_metadata_bytes( const t3dVTSerializedPackSource_t& rSource )
{
	uint64_t nOffset = T3D_VT_PACK_HEADER_BYTES;
	if ( !t3d_vt_pack_add_bytes( nOffset, ( uint64_t )rSource.m_nAllocationCount * sizeof( t3dVTPackAllocationRecord_t ) ) ||
	     !t3d_vt_pack_add_bytes( nOffset, ( uint64_t )rSource.m_nAllocationCount * sizeof( t3dVTPackAllocationKeyRecord_t ) ) ||
	     !t3d_vt_pack_add_bytes( nOffset, ( uint64_t )rSource.m_nChannelAllocationCount * sizeof( t3dVTChannelAllocation_t ) ) ||
	     !t3d_vt_pack_add_bytes( nOffset, ( uint64_t )rSource.m_nPageCount * sizeof( t3dVTPackPageRecord_t ) ) ||
	     !t3d_vt_pack_add_bytes( nOffset, ( uint64_t )rSource.m_nPayloadCount * sizeof( t3dVTPackPayloadRecord_t ) ) ||
	     !t3d_vt_pack_add_bytes( nOffset, ( uint64_t )rSource.m_nClusterCount * sizeof( t3dVTPackClusterRecord_t ) ) ||
	     !t3d_vt_pack_add_bytes( nOffset, ( uint64_t )rSource.m_nTailCount * sizeof( t3dVTPackTailRecord_t ) ) ||
	     !t3d_vt_pack_add_bytes( nOffset, ( uint64_t )rSource.m_nQualityViewCount * sizeof( t3dVTPackQualityView_t ) ) ||
	     !t3d_vt_pack_add_bytes( nOffset, ( uint64_t )rSource.m_nFormatFamilyCount * sizeof( t3dVTPackFormatFamilyRecord_t ) ) ) {
		return 0u;
	}
	return nOffset;
}

uint64_t t3d_vt_pack_compute_bytes( const t3dVTSerializedPackSource_t& rSource )
{
	if ( !t3d_vt_pack_validate_source( rSource ) ) {
		return 0u;
	}
	const uint64_t nOffset = t3d_vt_pack_compute_metadata_bytes( rSource );
	if ( nOffset == 0u || nOffset > UINT64_MAX - rSource.m_nPayloadDataBytes ) {
		return 0u;
	}
	return nOffset + rSource.m_nPayloadDataBytes;
}

#define T3D_VT_WRITE_DIRECTORY( header_field, source_ptr, count, type )                                                                    \
	pHeader->header_field = nOffset;                                                                                                       \
	if ( ( count ) != 0u ) {                                                                                                               \
		memcpy( pOut + nOffset, ( source_ptr ), ( size_t )( count ) * sizeof( type ) );                                                    \
	}                                                                                                                                      \
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset + ( uint64_t )( count ) * sizeof( type ), T3D_VT_PACK_ALIGNMENT )

bool t3d_vt_pack_write( const t3dVTSerializedPackSource_t& rSource, void *pDestination, const uint64_t nDestinationBytes,
                        uint64_t& rWrittenBytes )
{
	rWrittenBytes = 0u;
	const uint64_t nPackageBytes = t3d_vt_pack_compute_bytes( rSource );
	if ( nPackageBytes == 0u || pDestination == NULL || nDestinationBytes < nPackageBytes ) {
		return false;
	}
	uint8_t *const pOut = ( uint8_t * )pDestination;
	// Payload bytes may already occupy their final destination. Initialize all
	// directory padding, then copy external payloads or retain that exact alias.
	memset( pOut, 0, ( size_t )( nPackageBytes - rSource.m_nPayloadDataBytes ) );
	t3dVTPackHeader_t Header{};
	t3dVTPackHeader_t *const pHeader = &Header;
	pHeader->m_nMagic = T3D_VT_FILE_MAGIC;
	pHeader->m_nVersion = T3D_VT_FILE_FORMAT_VERSION;
	pHeader->m_nHeaderBytes = T3D_VT_PACK_HEADER_BYTES;
	pHeader->m_nFlags = T3D_VT_PACK_FLAG_CLUSTERED_PAYLOADS | T3D_VT_PACK_FLAG_TAILS_PRESENT | T3D_VT_PACK_FLAG_GPU_READY |
	                    T3D_VT_PACK_FLAG_QUALITY_VIEWS | T3D_VT_PACK_FLAG_FORMAT_FAMILY_MAP;
	pHeader->m_nAllocationCount = rSource.m_nAllocationCount;
	pHeader->m_nChannelAllocationCount = rSource.m_nChannelAllocationCount;
	pHeader->m_nPageCount = rSource.m_nPageCount;
	pHeader->m_nPayloadCount = rSource.m_nPayloadCount;
	pHeader->m_nClusterCount = rSource.m_nClusterCount;
	pHeader->m_nTailCount = rSource.m_nTailCount;
	pHeader->m_nChannelCount = T3D_VT_LOGICAL_CHANNEL_COUNT;
	pHeader->m_nInteriorTexels = T3D_VT_PAGE_INTERIOR;
	pHeader->m_nGutterTexels = T3D_VT_PAGE_GUTTER;
	pHeader->m_nPhysicalTexels = T3D_VT_PHYSICAL_TILE;
	pHeader->m_nPackageGeneration = rSource.m_nPackageGeneration;
	pHeader->m_nQualityViewCount = rSource.m_nQualityViewCount;
	pHeader->m_nFormatFamilyCount = rSource.m_nFormatFamilyCount;
	pHeader->m_nFullySupportedProfileMask = t3d_vt_pack_fully_supported_profile_mask( rSource );
	pHeader->m_nPackageBytes = nPackageBytes;
	for ( uint32_t iPage = 0u; iPage < rSource.m_nPageCount; ++iPage ) {
		++pHeader->m_nChannelPageCount[rSource.m_pPages[iPage].m_nChannel];
	}
	for ( uint32_t iTail = 0u; iTail < rSource.m_nTailCount; ++iTail ) {
		++pHeader->m_nChannelTailCount[rSource.m_pTails[iTail].m_nChannel];
	}
	for ( uint32_t iCluster = 0u; iCluster < rSource.m_nClusterCount; ++iCluster ) {
		if ( rSource.m_pClusters[iCluster].m_nCodec != T3D_VT_PACK_CODEC_NONE ) {
			pHeader->m_nFlags |= T3D_VT_PACK_FLAG_OUTER_COMPRESSION;
			break;
		}
	}

	uint64_t nOffset = T3D_VT_PACK_HEADER_BYTES;
	T3D_VT_WRITE_DIRECTORY( m_nAllocationOffset, rSource.m_pAllocations, rSource.m_nAllocationCount, t3dVTPackAllocationRecord_t );
	T3D_VT_WRITE_DIRECTORY( m_nAllocationKeyOffset, rSource.m_pAllocationKeys, rSource.m_nAllocationCount, t3dVTPackAllocationKeyRecord_t );
	T3D_VT_WRITE_DIRECTORY( m_nChannelAllocationOffset, rSource.m_pChannelAllocations, rSource.m_nChannelAllocationCount,
	                        t3dVTChannelAllocation_t );
	T3D_VT_WRITE_DIRECTORY( m_nPageOffset, rSource.m_pPages, rSource.m_nPageCount, t3dVTPackPageRecord_t );
	T3D_VT_WRITE_DIRECTORY( m_nPayloadDirectoryOffset, rSource.m_pPayloads, rSource.m_nPayloadCount, t3dVTPackPayloadRecord_t );
	T3D_VT_WRITE_DIRECTORY( m_nClusterDirectoryOffset, rSource.m_pClusters, rSource.m_nClusterCount, t3dVTPackClusterRecord_t );
	T3D_VT_WRITE_DIRECTORY( m_nTailMetadataOffset, rSource.m_pTails, rSource.m_nTailCount, t3dVTPackTailRecord_t );
	T3D_VT_WRITE_DIRECTORY( m_nQualityViewOffset, rSource.m_pQualityViews, rSource.m_nQualityViewCount, t3dVTPackQualityView_t );
	T3D_VT_WRITE_DIRECTORY( m_nFormatFamilyOffset, rSource.m_pFormatFamilies, rSource.m_nFormatFamilyCount, t3dVTPackFormatFamilyRecord_t );
#undef T3D_VT_WRITE_DIRECTORY
	pHeader->m_nPayloadDataOffset = nOffset;
	pHeader->m_nPayloadDataBytes = rSource.m_nPayloadDataBytes;
	if ( rSource.m_pPayloadData != pOut + nOffset ) {
		memcpy( pOut + nOffset, rSource.m_pPayloadData, ( size_t )rSource.m_nPayloadDataBytes );
	}
	pHeader->m_nContentSignature = t3d_vt_pack_hash64( pOut + T3D_VT_PACK_HEADER_BYTES, nPackageBytes - T3D_VT_PACK_HEADER_BYTES );
	pHeader->m_nHeaderChecksum = 0u;
	pHeader->m_nHeaderChecksum = t3d_vt_pack_checksum32( pHeader, sizeof( *pHeader ) );
	memcpy( pOut, pHeader, sizeof( Header ) );
	rWrittenBytes = nPackageBytes;
	return true;
}
