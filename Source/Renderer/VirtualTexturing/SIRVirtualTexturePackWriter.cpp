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

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackWriter.h>

// Source: Terrain3D authoring/vt/io/terrain_3d_vt_pack_writer.cpp.
// Reuse the native V1 structural/semantic validator rather than maintaining
// a second set of parallel on-disk validation rules in the writer.
static bool sirVTAdvancePackOffset( uint64_t *pOffset, uint64_t nCount, uint64_t nStride ) noexcept
{
	if ( pOffset == nullptr || nCount > UINT64_MAX / nStride ) {
		return false;
	}
	const uint64_t nBytes = nCount * nStride;
	if ( nBytes > UINT64_MAX - ( SIR_VT_PACK_ALIGNMENT - 1U ) || *pOffset > UINT64_MAX - ( nBytes + ( SIR_VT_PACK_ALIGNMENT - 1U ) ) ) {
		return false;
	}
	*pOffset = ( *pOffset + nBytes + ( SIR_VT_PACK_ALIGNMENT - 1U ) ) & ~( uint64_t )( SIR_VT_PACK_ALIGNMENT - 1U );
	return true;
}

static bool sirVTSourceRecordsValid( const sirVirtualTexturePackSource_t& source ) noexcept
{
	if ( source.pAllocations == nullptr || source.pAllocationKeys == nullptr || source.pChannelAllocations == nullptr ||
	     ( source.nPageCount != 0 && source.pPages == nullptr ) || source.pPayloads == nullptr || source.pClusters == nullptr ||
	     source.pTails == nullptr || source.pQualityViews == nullptr || source.pFormatFamilies == nullptr ||
	     source.pPayloadData == nullptr ) {
		return false;
	}
	if ( source.nAllocationCount == 0 || source.nAllocationCount > UINT32_MAX / SIR_VT_PACK_CHANNEL_COUNT ||
	     source.nChannelAllocationCount != source.nAllocationCount * SIR_VT_PACK_CHANNEL_COUNT ||
	     source.nFormatFamilyCount != SIR_VT_PACK_CHANNEL_COUNT || source.nPayloadCount == 0 || source.nClusterCount == 0 ||
	     source.nTailCount == 0 || source.nQualityViewCount == 0 || source.nPayloadDataBytes == 0 ) {
		return false;
	}
	return true;
}

bool sirVTMeasurePack( const sirVirtualTexturePackSource_t& source, uint64_t *pMetadataBytes, uint64_t *pPackageBytes ) noexcept
{
	if ( pMetadataBytes == nullptr || pPackageBytes == nullptr ) {
		return false;
	}
	*pMetadataBytes = 0;
	*pPackageBytes = 0;
	if ( !sirVTSourceRecordsValid( source ) ) {
		return false;
	}

	const uint64_t szCounts[] = { source.nAllocationCount, source.nAllocationCount,  source.nChannelAllocationCount,
	                              source.nPageCount,       source.nPayloadCount,     source.nClusterCount,
	                              source.nTailCount,       source.nQualityViewCount, source.nFormatFamilyCount };
	const uint64_t szStrides[] = { sizeof( sirVirtualTexturePackAllocationRecord_t ),  sizeof( sirVirtualTexturePackAllocationKey_t ),
	                               sizeof( sirVirtualTexturePackChannelAllocation_t ), sizeof( sirVirtualTexturePackPageRecord_t ),
	                               sizeof( sirVirtualTexturePackPayloadRecord_t ),     sizeof( sirVirtualTexturePackClusterRecord_t ),
	                               sizeof( sirVirtualTexturePackTailRecord_t ),        sizeof( sirVirtualTexturePackQualityView_t ),
	                               sizeof( sirVirtualTexturePackFamilyRecord_t ) };
	uint64_t nOffset = SIR_VT_PACK_HEADER_BYTES;
	for ( uint32_t iDirectory = 0; iDirectory < 9; ++iDirectory ) {
		if ( !sirVTAdvancePackOffset( &nOffset, szCounts[iDirectory], szStrides[iDirectory] ) ) {
			return false;
		}
	}
	if ( source.nPayloadDataBytes > UINT64_MAX - nOffset || nOffset + source.nPayloadDataBytes > SIZE_MAX ) {
		return false;
	}
	*pMetadataBytes = nOffset;
	*pPackageBytes = nOffset + source.nPayloadDataBytes;
	return true;
}

static bool sirVTSourceNotOverlapping( const void *pSource, uint64_t nBytes, const void *pTarget, uint64_t nTargetBytes ) noexcept
{
	const uintptr_t nSource = ( uintptr_t )pSource;
	const uintptr_t nTarget = ( uintptr_t )pTarget;
	if ( nBytes > UINTPTR_MAX - nSource || nTargetBytes > UINTPTR_MAX - nTarget ) {
		return false;
	}
	return nSource >= nTarget + nTargetBytes || nTarget >= nSource + nBytes;
}

bool sirVTWritePack( const sirVirtualTexturePackSource_t& source, void *pDestination, uint64_t nCapacity, uint64_t *pWrittenBytes ) noexcept
{
	if ( pWrittenBytes == nullptr ) {
		return false;
	}
	*pWrittenBytes = 0;

	uint64_t nMetadataBytes = 0;
	uint64_t nPackageBytes = 0;
	if ( pDestination == nullptr || !sirVTMeasurePack( source, &nMetadataBytes, &nPackageBytes ) || nCapacity < nPackageBytes ) {
		return false;
	}

	const void *szSources[] = { source.pAllocations, source.pAllocationKeys, source.pChannelAllocations,
	                            source.pPages,       source.pPayloads,       source.pClusters,
	                            source.pTails,       source.pQualityViews,   source.pFormatFamilies };
	const uint64_t szCounts[] = { source.nAllocationCount, source.nAllocationCount,  source.nChannelAllocationCount,
	                              source.nPageCount,       source.nPayloadCount,     source.nClusterCount,
	                              source.nTailCount,       source.nQualityViewCount, source.nFormatFamilyCount };
	const uint64_t szStrides[] = { sizeof( sirVirtualTexturePackAllocationRecord_t ),  sizeof( sirVirtualTexturePackAllocationKey_t ),
	                               sizeof( sirVirtualTexturePackChannelAllocation_t ), sizeof( sirVirtualTexturePackPageRecord_t ),
	                               sizeof( sirVirtualTexturePackPayloadRecord_t ),     sizeof( sirVirtualTexturePackClusterRecord_t ),
	                               sizeof( sirVirtualTexturePackTailRecord_t ),        sizeof( sirVirtualTexturePackQualityView_t ),
	                               sizeof( sirVirtualTexturePackFamilyRecord_t ) };
	for ( uint32_t iDirectory = 0; iDirectory < 9; ++iDirectory ) {
		if ( !sirVTSourceNotOverlapping( szSources[iDirectory], szCounts[iDirectory] * szStrides[iDirectory], pDestination,
		                                 nPackageBytes ) ) {
			return false;
		}
	}
	uint8_t *pBytes = ( uint8_t * )pDestination;
	if ( source.pPayloadData != pBytes + nMetadataBytes &&
	     !sirVTSourceNotOverlapping( source.pPayloadData, source.nPayloadDataBytes, pDestination, nPackageBytes ) ) {
		return false;
	}

	sirVirtualTexturePackHeader_t header = {};
	header.nMagic = SIR_VT_PACK_MAGIC;
	header.nVersion = SIR_VT_PACK_VERSION;
	header.nHeaderBytes = SIR_VT_PACK_HEADER_BYTES;
	header.nFlags = SIR_VT_PACK_FLAG_CLUSTERED | SIR_VT_PACK_FLAG_TAILS | SIR_VT_PACK_FLAG_GPU_READY | SIR_VT_PACK_FLAG_QUALITY_VIEWS |
	                SIR_VT_PACK_FLAG_FAMILY_MAP;
	header.nAllocationCount = source.nAllocationCount;
	header.nChannelAllocationCount = source.nChannelAllocationCount;
	header.nPageCount = source.nPageCount;
	header.nPayloadCount = source.nPayloadCount;
	header.nClusterCount = source.nClusterCount;
	header.nTailCount = source.nTailCount;
	header.nQualityViewCount = source.nQualityViewCount;
	header.nFormatFamilyCount = source.nFormatFamilyCount;
	header.nChannelCount = SIR_VT_PACK_CHANNEL_COUNT;
	header.nInteriorTexels = SIR_VT_PAGE_INTERIOR;
	header.nGutterTexels = SIR_VT_PAGE_GUTTER;
	header.nPhysicalTexels = SIR_VT_PHYSICAL_TILE;
	header.nPackageGeneration = source.nPackageGeneration;
	header.nPackageBytes = nPackageBytes;
	header.nFullySupportedProfileMask = ( 1U << SIR_VT_PACK_QUALITY_PROFILES ) - 1U;
	for ( uint32_t iAllocation = 0; iAllocation < source.nAllocationCount; ++iAllocation ) {
		header.nFullySupportedProfileMask &= source.pAllocations[iAllocation].nAvailableProfiles;
	}
	for ( uint32_t iPage = 0; iPage < source.nPageCount; ++iPage ) {
		const uint32_t nChannel = source.pPages[iPage].nChannel;
		if ( nChannel >= SIR_VT_PACK_CHANNEL_COUNT ) {
			return false;
		}
		++header.szChannelPageCount[nChannel];
	}
	for ( uint32_t iTail = 0; iTail < source.nTailCount; ++iTail ) {
		const uint32_t nChannel = source.pTails[iTail].nChannel;
		if ( nChannel >= SIR_VT_PACK_CHANNEL_COUNT ) {
			return false;
		}
		++header.szChannelTailCount[nChannel];
	}
	for ( uint32_t iCluster = 0; iCluster < source.nClusterCount; ++iCluster ) {
		if ( source.pClusters[iCluster].nCodec != 0 ) {
			header.nFlags |= SIR_VT_PACK_FLAG_OUTER_COMPRESSION;
			break;
		}
	}

	// Serial layout follows the donor writer exactly: records in frozen order,
	// each table rounded up to 16 bytes and zero-filled between tables.
	memset( pBytes, 0, ( size_t )nMetadataBytes );
	uint64_t nOffset = SIR_VT_PACK_HEADER_BYTES;
	uint64_t *szHeaderOffsets[] = { &header.nAllocationOffset,   &header.nAllocationKeyOffset,    &header.nChannelAllocationOffset,
	                                &header.nPageOffset,         &header.nPayloadDirectoryOffset, &header.nClusterDirectoryOffset,
	                                &header.nTailMetadataOffset, &header.nQualityViewOffset,      &header.nFormatFamilyOffset };
	for ( uint32_t iDirectory = 0; iDirectory < 9; ++iDirectory ) {
		*szHeaderOffsets[iDirectory] = nOffset;
		const uint64_t nBytes = szCounts[iDirectory] * szStrides[iDirectory];
		if ( nBytes != 0 ) {
			memcpy( pBytes + nOffset, szSources[iDirectory], ( size_t )nBytes );
		}
		nOffset = ( nOffset + nBytes + ( SIR_VT_PACK_ALIGNMENT - 1U ) ) & ~( uint64_t )( SIR_VT_PACK_ALIGNMENT - 1U );
	}
	header.nPayloadDataOffset = nOffset;
	header.nPayloadDataBytes = source.nPayloadDataBytes;
	if ( source.pPayloadData != pBytes + nOffset ) {
		memcpy( pBytes + nOffset, source.pPayloadData, ( size_t )source.nPayloadDataBytes );
	}
	header.nContentSignature = sirVTPackHash64( pBytes + SIR_VT_PACK_HEADER_BYTES, nPackageBytes - SIR_VT_PACK_HEADER_BYTES );
	header.nHeaderChecksum = 0;
	header.nHeaderChecksum = sirVTPackChecksum32( &header, sizeof( header ) );
	memcpy( pBytes, &header, sizeof( header ) );

	const sirVirtualTexturePackInput_t input = { pBytes, nPackageBytes, nPackageBytes, false };
	sirVirtualTexturePackView_t validated = {};
	if ( !sirVTPackOpenValidated( input, &validated ) ) {
		return false;
	}
	*pWrittenBytes = nPackageBytes;
	return true;
}
