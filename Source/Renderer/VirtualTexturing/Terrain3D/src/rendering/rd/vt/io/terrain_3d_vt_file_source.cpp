// Copyright (c) 2023-2026 Terrain3D contributors.
// Adaptation of original terrain_3d_vt_file_source.cpp to SIREngine PlatformFile.
#include "rendering/rd/vt/io/terrain_3d_vt_file_source.h"
#include <Renderer/VirtualTexturing/SIRVirtualTextureZstdCodec.h>
#include <string.h>

void t3d_vt_file_source_close( t3dVTFileSource_s& rSource )
{
	rSource.File.close();
	if ( rSource.pScratch != nullptr && rSource.ScratchAllocator.pFree != nullptr ) {
		rSource.ScratchAllocator.pFree( rSource.ScratchAllocator.pUser, rSource.pScratch );
	}
	rSource.pScratch = nullptr;
	rSource.nStoredCapacity = 0u;
	rSource.nDecodedCapacity = 0u;
	rSource.MetadataView = {};
	rSource.ScratchAllocator = {};
	rSource.nFileBytes = 0u;
	rSource.nPayloadOffset = 0u;
	rSource.nPayloadBytes = 0u;
	rSource.nSignature = 0u;
	rSource.nGeneration = 0u;
	rSource.nReadCount = 0u;
}

bool t3d_vt_file_source_open_cold( const t3dVTFileSourceOpenRequest_t& request, t3dVTFileSource_s& rSource, uint64_t& rMetadataBytes )
{
	t3d_vt_file_source_close( rSource );
	rMetadataBytes = 0u;
	if ( request.range.pszPath == nullptr || request.pMetadata == nullptr || request.nMetadataCapacity == 0u ) {
		return false;
	}
	sirVirtualTexturePackView_t view{};
	const sirVirtualTexturePackMetadataBuffer_t buffer{ request.pMetadata, request.nMetadataCapacity };
	if ( !rSource.File.openRange( request.range, buffer, &view ) ) {
		return false;
	}
	rSource.nFileBytes = rSource.File.getFileBytes();
	rSource.nPayloadOffset = view.header.nPayloadDataOffset;
	rSource.nPayloadBytes = view.header.nPayloadDataBytes;
	rSource.nSignature = view.header.nContentSignature;
	rSource.nGeneration = view.header.nPackageGeneration;
	rMetadataBytes = view.header.nPayloadDataOffset;
	return true;
}

bool t3d_vt_file_source_read_payload( t3dVTFileSource_s& rSource, uint64_t nOffset, uint8_t *pDestination, uint32_t nLength )
{
	if ( !rSource.File.readPayload( nOffset, pDestination, nLength ) ) {
		return false;
	}
	rSource.nReadCount = rSource.File.getReadCount();
	return true;
}

// This is the donor's single-owner, cold-loaded cluster decoding boundary.
// The runtime owns its one scratch allocation; no per-page heap allocation.
bool t3d_vt_file_source_bind_metadata( t3dVTFileSource_s& rSource, const uint8_t *pMetadata, uint64_t nMetadataBytes,
                                       const t3dVTPackRuntimeAllocator_t& rAllocator )
{
	if ( pMetadata == nullptr || rAllocator.pAllocate == nullptr || rAllocator.pFree == nullptr || !rSource.File.isOpen() ||
	     rSource.pScratch != nullptr ) {
		return false;
	}
	sirVirtualTexturePackView_t candidate{};
	if ( !sirVTPackOpenValidated( { pMetadata, nMetadataBytes, rSource.nFileBytes, true }, &candidate ) ) {
		return false;
	}
	uint32_t maxStored = 0u, maxDecoded = 0u;
	for ( uint32_t i = 0u; i < candidate.header.nClusterCount; ++i ) {
		sirVirtualTexturePackClusterRecord_t cluster{};
		memcpy( &cluster, candidate.szDirectories[SIR_VT_PACK_CLUSTERS].pData + ( uint64_t )i * sizeof( cluster ), sizeof( cluster ) );
		if ( cluster.nStoredBytes > maxStored )
			maxStored = cluster.nStoredBytes;
		if ( cluster.nDecodedBytes > maxDecoded )
			maxDecoded = cluster.nDecodedBytes;
	}
	if ( maxStored == 0u || maxDecoded == 0u || ( uint64_t )maxStored + maxDecoded > 64ull * 1024ull * 1024ull ) {
		return false;
	}
	// The scratch buffers must never alias for Zstd decoding.
	uint8_t *const scratch = ( uint8_t * )rAllocator.pAllocate( rAllocator.pUser, ( size_t )maxStored + maxDecoded, 64u );
	if ( scratch == nullptr )
		return false;
	rSource.ScratchAllocator = rAllocator;
	rSource.pScratch = scratch;
	rSource.nStoredCapacity = maxStored;
	rSource.nDecodedCapacity = maxDecoded;
	rSource.MetadataView = candidate;
	return true;
}

bool t3d_vt_file_source_read_decoded_payload( t3dVTFileSource_s& rSource, uint32_t nPayloadId, uint8_t *pDestination, uint32_t nCapacity )
{
	const sirVirtualTexturePackView_t& view = rSource.MetadataView;
	if ( pDestination == nullptr || rSource.pScratch == nullptr || nPayloadId >= view.header.nPayloadCount ||
	     view.szDirectories[SIR_VT_PACK_PAYLOADS].pData == nullptr ) {
		return false;
	}
	sirVirtualTexturePackPayloadRecord_t payload{};
	memcpy( &payload, view.szDirectories[SIR_VT_PACK_PAYLOADS].pData + ( uint64_t )nPayloadId * sizeof( payload ), sizeof( payload ) );
	if ( payload.nClusterId >= view.header.nClusterCount || payload.nDecodedBytes == 0u || payload.nDecodedBytes > nCapacity )
		return false;
	sirVirtualTexturePackClusterRecord_t cluster{};
	memcpy( &cluster, view.szDirectories[SIR_VT_PACK_CLUSTERS].pData + ( uint64_t )payload.nClusterId * sizeof( cluster ),
	        sizeof( cluster ) );
	if ( payload.nClusterOffset > cluster.nDecodedBytes || payload.nDecodedBytes > cluster.nDecodedBytes - payload.nClusterOffset )
		return false;
	sirVirtualTextureClusterBuffers_t buffers{ rSource.pScratch, rSource.nStoredCapacity, rSource.pScratch + rSource.nStoredCapacity,
	                                           rSource.nDecodedCapacity };
	const sirVirtualTextureClusterCodec_t codec = SIRVirtualTextureZstdCodec::makeDecoder();
	if ( !rSource.File.readCluster( view, payload.nClusterId, buffers, &codec ) )
		return false;
	memcpy( pDestination, buffers.pDecodedBytes + payload.nClusterOffset, payload.nDecodedBytes );
	rSource.nReadCount = rSource.File.getReadCount();
	return true;
}
