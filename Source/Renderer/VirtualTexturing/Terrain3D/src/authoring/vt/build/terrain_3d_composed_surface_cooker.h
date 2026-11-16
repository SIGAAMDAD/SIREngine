// CS03: cold/offline composition into independent compressed .t3sc page sets.
#ifndef TERRAIN3D_COMPOSED_SURFACE_COOKER_H
#define TERRAIN3D_COMPOSED_SURFACE_COOKER_H

#include "rendering/rd/vt/core/terrain_3d_composed_surface_contract.h"

// Sorted ascending by (region, mip, y, x). The page footprint includes
// the neighbor halo and mips, supplied by the source evaluator; no border clamp.
struct t3dComposedSurfaceCookPage_t
{
	uint64_t m_nRegionKey;
	uint64_t m_nCompositionRevision;
	uint32_t m_nMip;
	uint32_t m_nPageX;
	uint32_t m_nPageY;
	uint32_t m_nGeneration;
};

// Evaluate 136x136 actual texels, INCLUDING neighbors and an explicit mip.
// Writes interleaved RGBA8 in shader-contract channel order. The producer
// must implement full static raster semantics and return false otherwise.
typedef bool ( *t3dComposedSurfaceEvaluateFn )( void *pUser, const t3dComposedSurfaceCookPage_t& page, uint32_t nChannel, uint8_t *pRgba,
                                                uint32_t nBytes );
// Offline compression, compatible with the existing V1 BC1/BC5/BC7 encoder.
typedef bool ( *t3dComposedSurfaceEncodeFn )( void *pUser, uint32_t nChannel, const uint8_t *pRgba, uint32_t nRgbaBytes,
                                              uint8_t *pCompressed, uint32_t nCapacity, uint32_t& rWritten );

// Optional pre-encoding producer. Each channel occupies nRgbaBytes bytes in
// canonical channel order. The cooker falls back to m_pEvaluate when scratch
// capacity is insufficient or this callback is absent.
typedef bool ( *t3dComposedSurfaceEvaluateAllFn )( void *pUser, const t3dComposedSurfaceCookPage_t& page, uint8_t *pRgbaAll,
                                                   uint32_t nRgbaBytes );

struct t3dComposedSurfaceCookSource_t
{
	void *m_pUser;
	t3dComposedSurfaceEvaluateFn m_pEvaluate;
	t3dComposedSurfaceEncodeFn m_pEncode;
	uint64_t m_nSourceFingerprint;
	// Must only be true after full CS02 material-equivalence acceptance.
	bool m_bCanonicalMaterialParityAccepted;
	// Candidate-only bake for offline verification. Never authorizes GPU cutover.
	bool m_bReferenceOnly = false;
	t3dComposedSurfaceEvaluateAllFn m_pEvaluateAll = nullptr;
};

#define T3D_COMP_SURF_RGBA_SCRATCH_BYTES ( T3D_COMP_SURF_PHYSICAL_TILE * T3D_COMP_SURF_PHYSICAL_TILE * 4u )
// Scratch = exactly one uncompressed RGBA8 tile plus one compressed channel.
#define T3D_COMP_SURF_COOK_SCRATCH_BYTES                                                                                                   \
	( T3D_COMP_SURF_RGBA_SCRATCH_BYTES + ( T3D_COMP_SURF_PHYSICAL_TILE * T3D_COMP_SURF_PHYSICAL_TILE ) )
// Optional: three independent RGBA channels and the existing encoder scratch.
// No change to the serialized .t3sc page-set contract or legacy cook buffers.
#define T3D_COMP_SURF_COOK_FUSED_SCRATCH_BYTES                                                                                             \
	( T3D_COMP_SURF_RGBA_SCRATCH_BYTES * T3D_COMP_SURF_CHANNEL_COUNT + ( T3D_COMP_SURF_PHYSICAL_TILE * T3D_COMP_SURF_PHYSICAL_TILE ) )

bool t3d_comp_surf_cook_total_bytes( uint32_t nPages, uint64_t& rBytes );
bool t3d_comp_surf_cook_pages_cold( const t3dComposedSurfaceCookPage_t *pPages, uint32_t nPages,
                                    const t3dComposedSurfaceCookSource_t& source, uint8_t *pScratch, uint32_t nScratchBytes,
                                    uint8_t *pOutput, uint64_t nOutputCapacity, uint64_t& rWrittenBytes );

// Incremental cold rewrite. Reuse validated prior page payloads when their
// logical identity, source fingerprint, revision and generation are unchanged.
// Mark dirty pages explicitly; a changed revision always triggers recomposition.
bool t3d_comp_surf_recook_incremental_cold( const t3dComposedSurfaceCookPage_t *pPages, uint32_t nPages,
                                            const t3dComposedSurfaceCookSource_t& source, const uint8_t *pPreviousFile,
                                            uint64_t nPreviousBytes, const uint8_t *pDirtyFlags, uint8_t *pScratch, uint32_t nScratchBytes,
                                            uint8_t *pOutput, uint64_t nOutputCapacity, uint64_t& rWrittenBytes );

#endif
