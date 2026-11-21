// Cold, camera-independent subset of canonical V1 terrain material composition.
// Source VT footprint/filtering is supplied by the offline VT reader. This
// evaluator refuses view-dependent material configurations rather than baking
// a result that differs from the raster shader.
#ifndef TERRAIN3D_COMPOSED_SURFACE_STATIC_V1_H
#define TERRAIN3D_COMPOSED_SURFACE_STATIC_V1_H

#include "authoring/vt/build/terrain_3d_composed_surface_cooker.h"

#define T3D_COMP_SURF_STATIC_MAX_LAYERS 32u
#define T3D_COMP_SURF_STATIC_SAMPLE_NEAREST 2u
#define T3D_COMP_SURF_STATIC_SAMPLE_BILINEAR 1u
#define T3D_COMP_SURF_STATIC_SAMPLE_TOP_TWO 5u

struct t3dComposedStaticLayer_t
{
	uint32_t m_nAllocation;
	float m_fColor[3];
	float m_fPbr0[4]; // x=source UV scale, y=normal strength, z=roughness offset, w=metallic
	float m_fPbr1[4]; // x=AO weight, y=AO influence, z=detile rotation, w=detile shift
};

// First interior texel has coordinates (origin + 0.5 * step).
// World and control texel coordinates may differ. The page's gutter is sampled
// from real neighboring source data, including negative world coordinates.
struct t3dComposedStaticFootprint_t
{
	float m_fWorldOrigin[2];
	float m_fControlOrigin[2];
	float m_fWorldStepX[2];
	float m_fWorldStepY[2];
	float m_fControlStepX[2];
	float m_fControlStepY[2];
};

// The source reader must reproduce clipmap canonical V1 filtering, tail/parent
// fallback and decode-after-filter albedo semantics at the supplied gradients.
// Returned albedo is LINEAR RGB; pbr = (normal_x,normal_z,roughness,AO).
struct t3dComposedStaticSample_t
{
	float m_fAlbedo[3];
	float m_fPbr[4];
	float m_fHeight;
};

typedef bool ( *t3dCompSurfStaticPageFn )( void *pUser, const t3dComposedSurfaceCookPage_t& page,
                                           t3dComposedStaticFootprint_t& rFootprint );
typedef bool ( *t3dCompSurfStaticControlFn )( void *pUser, int32_t nX, int32_t nY, uint32_t& rControl );
// The editor takes immutable height/normal snapshots at the composed page's
// output sample positions. A callback is required only for explicit auto cells.
typedef bool ( *t3dCompSurfStaticSurfaceFn )( void *pUser, uint32_t nX, uint32_t nY, float& rHeight, float& rNormalY );
typedef bool ( *t3dCompSurfStaticSourceFn )( void *pUser, uint32_t nAllocation, const float *pUv, const float *pGradX, const float *pGradY,
                                             float fLodBias, uint32_t nFiltering, t3dComposedStaticSample_t& rSample );

struct t3dComposedStaticSource_t
{
	void *m_pUser;
	t3dCompSurfStaticPageFn m_pResolvePage;
	t3dCompSurfStaticControlFn m_pReadControl;
	t3dCompSurfStaticSurfaceFn m_pReadTerrainSurface = nullptr;
	t3dCompSurfStaticSourceFn m_pSampleSource;
	const t3dComposedStaticLayer_t *m_pLayers;
	uint32_t m_nLayerCount;
	uint32_t m_nSamplingMode; // 1 bilinear, 2 nearest, 5 top-two
	uint32_t m_nVtFiltering;
	float m_fStaticMipBias;
	float m_fBlendSharpness;
	// Canonical shader detiling uses 2=off, 1=8-way, 3=translation,
	// 4=4-way and 5=16-way. No runtime camera distance enters this recipe.
	uint32_t m_nDetilingMode = 2u;
	float m_fAutoSlope = 1.0f;
	float m_fAutoHeightReduction = 0.1f;
	uint32_t m_nAutoBaseTexture = 0u;
	uint32_t m_nAutoOverlayTexture = 1u;
	// Auto controls and deterministic detiling can be composed from an
	// immutable authoring snapshot. Camera-distance dual scale, triplanar
	// projection and adaptive sampling still require another representation.
	bool m_bAutoShader;
	bool m_bDualScale;
	bool m_bSideProjection;
	bool m_bDetiling;
	bool m_bAdaptiveSampling;
	bool m_bImplicitControls;
	bool m_bViewDependentMipBias;
	bool m_bCanonicalMaterialParityAccepted;
	bool m_bReferenceOnly = false; // CPU candidate; never a parity certificate.
};

bool t3d_comp_surf_static_baseline_eligible( const t3dComposedStaticSource_t& source );
bool t3d_comp_surf_static_baseline_evaluate( void *pUser, const t3dComposedSurfaceCookPage_t& page, uint32_t nChannel, uint8_t *pRgba,
                                             uint32_t nBytes );
// Single evaluation per texel produces the complete three-channel page set.
bool t3d_comp_surf_static_baseline_evaluate_all( void *pUser, const t3dComposedSurfaceCookPage_t& page, uint8_t *pRgbaAll,
                                                 uint32_t nRgbaBytes );
// P09 uncompressed pre-BC reference. nFloatCount is 136*136*4, not bytes.
// Linear albedo, normalized tangent XYZ normal, or linear AO/R/M/AO-affect.
// This independent diagnostic endpoint does not imply source/parity approval.
bool t3d_comp_surf_static_baseline_evaluate_linear( void *pUser, const t3dComposedSurfaceCookPage_t& page, uint32_t nChannel,
                                                    float *pLinear, uint32_t nFloatCount );

// Connects this evaluator to the existing cold sidecar cooker and BC encoder;
// it never raises the material-parity flag on behalf of the caller.
struct t3dComposedStaticCookAdapter_t
{
	t3dComposedStaticSource_t m_source;
	t3dComposedSurfaceEncodeFn m_pEncoder;
	void *m_pEncoderUser;
};

bool t3d_comp_surf_static_baseline_encode( void *pUser, uint32_t nChannel, const uint8_t *pRgba, uint32_t nRgbaBytes, uint8_t *pCompressed,
                                           uint32_t nCapacity, uint32_t& rWritten );
bool t3d_comp_surf_static_baseline_make_cook_source( t3dComposedStaticCookAdapter_t& rAdapter, uint64_t nFingerprint,
                                                     t3dComposedSurfaceCookSource_t& rCook );

#endif
