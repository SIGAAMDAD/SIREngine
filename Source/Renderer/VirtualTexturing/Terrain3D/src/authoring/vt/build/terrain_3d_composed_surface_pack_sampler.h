// Cold V1 source-pack sampler for composed-surface authoring. No shipping V1 ABI changes.
#ifndef TERRAIN3D_COMPOSED_SURFACE_V1_PACK_SAMPLER_H
#define TERRAIN3D_COMPOSED_SURFACE_V1_PACK_SAMPLER_H

#include "authoring/vt/build/terrain_3d_composed_surface_static_baseline.h"
#include "data/formats/terrain_3d_vt_pack_format.h"

#define T3D_COMPOSED_SURFACE_PACK_CACHE_MAX_SLOTS 32u
#define T3D_COMPOSED_SURFACE_PACK_DECODED_TILE_BYTES ( T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE * 4u )

// Decode compressed V1 blocks into exactly 136x136 RGBA8, including gutters.
// BC7/BC1/BC5 decoding is deliberately a cold authoring operation.
typedef bool ( *t3dComposedSurfacePackDecodeFn )( void *pUser, uint32_t nPhysicalFamily, const uint8_t *pEncoded, uint32_t nEncodedBytes,
                                                  uint8_t *pRgba, uint32_t nRgbaCapacity );

struct t3dComposedSurfacePackCacheEntry_t
{
	uint32_t m_nPayloadId;
	uint32_t m_nTimestamp;
};

struct t3dComposedSurfacePackSampler_t
{
	const t3dVTPackBaselineView_t *m_pPack;
	t3dComposedSurfacePackDecodeFn m_pDecode;
	void *m_pDecoderUser;
	uint8_t *m_pCacheBytes; // caller-owned, capacity * T3D_COMPOSED_SURFACE_PACK_DECODED_TILE_BYTES
	uint32_t m_nCacheSlots;
	uint32_t m_nClock;
	t3dComposedSurfacePackCacheEntry_t m_Entries[T3D_COMPOSED_SURFACE_PACK_CACHE_MAX_SLOTS];
	uint32_t m_nCacheHits;
	uint32_t m_nDecodeCalls;
	uint32_t m_nChecksumFailures;
};

// Full package must have passed t3d_vt_pack_open_baseline_view(). Metadata-only
// views are forbidden because source bytes are required for deterministic cooks.
bool t3d_composed_surface_pack_find_material_allocation( const t3dVTPackBaselineView_t& rPack, uint32_t nLogicalMaterialId,
                                                         uint32_t& rAllocation );

// Cold zero-copy open of a complete .t3sp package. The byte span must outlive
// rView, sampler and all decoded tile cache entries. Uses the frozen V1 reader.
bool t3d_composed_surface_pack_open_source_pack( const void *pPackage, uint64_t nPackageBytes, t3dVTPackBaselineView_t& rView,
                                                 t3dComposedSurfacePackSampler_t& rSampler, t3dComposedSurfacePackDecodeFn pDecoder,
                                                 void *pDecoderUser, uint8_t *pCacheBytes, uint32_t nCacheSlots );

bool t3d_composed_surface_pack_sampler_init( t3dComposedSurfacePackSampler_t& rSampler, const t3dVTPackBaselineView_t& rValidatedPack,
                                             t3dComposedSurfacePackDecodeFn pDecode, void *pDecoderUser, uint8_t *pCacheBytes,
                                             uint32_t nSlots );

// Packs use the physical ALBEDO/PBR/HEIGHT channels of each terrain allocation.
// Supported filter classes: 3=nearest, 1=bilinear with virtual trilinear.
// Anisotropic classes (0/2) are explicitly rejected pending GPU parity captures.
bool t3d_composed_surface_pack_sample_static( void *pUser, uint32_t nAllocation, const float *pUv, const float *pGradX, const float *pGradY,
                                              float fLodBias, uint32_t nFiltering, t3dComposedStaticSample_t& rSample );

// Binds only the source callback. Caller controls material-parity acceptance.
struct t3dComposedSurfacePackProducerBridge_t
{
	t3dComposedSurfacePackSampler_t *m_pSampler;
	void *m_pOriginalUser;
	t3dCompSurfStaticPageFn m_pOriginalPage;
	t3dCompSurfStaticControlFn m_pOriginalControl;
	t3dCompSurfStaticSurfaceFn m_pOriginalSurface;
};

// Preserves the producer's page/control callback contexts while installing
// source-pack sampling. Caller must keep the bridge alive throughout cooking.
// Invalid filter classes fail before any callback is replaced.
bool t3d_composed_surface_pack_bind_static_source( t3dComposedSurfacePackSampler_t& rSampler,
                                                   t3dComposedSurfacePackProducerBridge_t& rBridge, t3dComposedStaticSource_t& rSource );

#endif
