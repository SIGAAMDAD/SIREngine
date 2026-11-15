// Copyright (c) 2023-2026 Terrain3D contributors.
// Native SIREngine I/O boundary for Terrain3D Sparse VT V1.
#ifndef TERRAIN3D_VT_FILE_SOURCE_H
#define TERRAIN3D_VT_FILE_SOURCE_H

#include "Terrain3DTypes.h"
#include <Renderer/VirtualTexturing/SIRVirtualTexturePackFileSource.h>

struct t3dVTFileSource_s
{
	SIRVirtualTexturePackFileSource File;
	uint64_t nFileBytes = 0u;
	uint64_t nPayloadOffset = 0u;
	uint64_t nPayloadBytes = 0u;
	uint64_t nSignature = 0u;
	uint32_t nGeneration = 0u;
	uint64_t nReadCount = 0u;
	sirVirtualTexturePackView_t MetadataView{};
	t3dVTPackRuntimeAllocator_t ScratchAllocator{};
	uint8_t *pScratch = nullptr;
	uint32_t nStoredCapacity = 0u;
	uint32_t nDecodedCapacity = 0u;
};

typedef struct t3dVTFileSourceOpenRequest_s
{
	sirVirtualTexturePackRange_t range;
	uint8_t *pMetadata;
	uint64_t nMetadataCapacity;
} t3dVTFileSourceOpenRequest_t;

// Metadata is caller-owned. No per-read heap allocations or alternate file handle.
bool t3d_vt_file_source_open_cold( const t3dVTFileSourceOpenRequest_t& request, t3dVTFileSource_s& rSource, uint64_t& rMetadataBytes );
bool t3d_vt_file_source_read_payload( t3dVTFileSource_s& rSource, uint64_t nOffset, uint8_t *pDestination, uint32_t nLength );
// Bind the metadata copied into the permanent runtime allocation, then allocate
// one bounded decompression workspace using the caller's native allocator.
bool t3d_vt_file_source_bind_metadata( t3dVTFileSource_s& rSource, const uint8_t *pMetadata, uint64_t nMetadataBytes,
                                       const t3dVTPackRuntimeAllocator_t& rAllocator );
bool t3d_vt_file_source_read_decoded_payload( t3dVTFileSource_s& rSource, uint32_t nPayloadId, uint8_t *pDestination, uint32_t nCapacity );
void t3d_vt_file_source_close( t3dVTFileSource_s& rSource );
#endif
