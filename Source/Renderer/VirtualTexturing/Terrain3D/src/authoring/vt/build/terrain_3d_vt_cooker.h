// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_COOKER_H
#define TERRAIN3D_VT_COOKER_H

#include "Terrain3DTypes.h"

#include "authoring/vt/build/terrain_3d_vt_page_builder.h"
#include "authoring/vt/io/terrain_3d_vt_pack_writer.h"
#include "rendering/rd/vt/runtime/terrain_3d_vt_fixed_profile.h"

#include <stdint.h>

typedef bool ( *t3dVTEncodeTileFn )( void *pUser, const t3dVTEncodeTileRequest_t& rRequest, uint32_t& rWrittenBytes );

typedef void *( *t3dVTCookAllocateFn )( void *pUser, uint64_t nBytes, uint32_t nAlignment );
typedef void ( *t3dVTCookFreeFn )( void *pUser, void *pMemory );

typedef struct t3dVTChannelNormalization_s
{
	t3dVTCookImageView_t m_source{};
	t3dVTLogicalChannel_t m_channel{};
	uint32_t m_nSourceLayout = 0u;
	uint32_t m_nTargetExtent = 0u;
	bool m_bAlphaScissor = false;
	uint8_t m_nAlphaThreshold = 0u;
} t3dVTChannelNormalization_t;

bool t3d_vt_cooker_normalize_channel_to_profile( const t3dVTChannelNormalization_t& rRequest, const t3dVTCookAllocator_t& rAllocator,
                                                 uint8_t *pDestination, uint64_t nDestinationBytes );

typedef uint64_t ( *t3dVTCookClusterBoundFn )( void *pUser, uint32_t nCodec, uint64_t nDecodedBytes );
typedef bool ( *t3dVTCookClusterEncodeFn )( void *pUser, uint32_t nCodec, const uint8_t *pSource, uint32_t nSourceBytes,
                                            uint8_t *pDestination, uint32_t nDestinationCapacity, uint32_t& rWrittenBytes );

void t3d_vt_cooker_default_settings( t3dVTCookerSettings_t& rSettings );
bool t3d_vt_cooker_validate_settings( const t3dVTCookerSettings_t& rSettings );
bool t3d_vt_cooker_encode_tile( const t3dVTEncoderInterface_t& rEncoder, const t3dVTEncodeTileRequest_t& rRequest,
                                uint32_t& rWrittenBytes );
bool t3d_vt_cooker_build_channel_pyramid( const t3dVTCookAllocator_t& rAllocator, const t3dVTCookSparseSource_t& rSource,
                                          t3dVTFixedProfile_t Profile, t3dVTLogicalChannel_t Channel, uint8_t *& rpMipBytes,
                                          t3dVTMipChainLayout_t& rLayout, uint32_t nMipQuality = 1u );
bool t3d_vt_cooker_cook_sparse_pages( const t3dVTCookerSettings_t& rSettings, const t3dVTCookSparseSource_t *pSources,
                                      uint32_t nSourceCount, const t3dVTEncoderInterface_t& rEncoder,
                                      const t3dVTCookAllocator_t& rAllocator, const t3dVTCookClusterCodec_t *pClusterCodec,
                                      t3dVTCookedSparsePackage_t& rPackage );
void t3d_vt_cooker_release_sparse_package( t3dVTCookedSparsePackage_t& rPackage );
bool t3d_vt_cooker_build_pack_source( const t3dVTCookedSparsePackage_t& rPackage, t3dVTSerializedPackSource_t& rSource );

#endif // TERRAIN3D_VT_COOKER_H
