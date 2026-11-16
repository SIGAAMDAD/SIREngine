#ifndef TERRAIN3D_VT_VISIBILITY_BAKER_H
#define TERRAIN3D_VT_VISIBILITY_BAKER_H

#include "authoring/vt/visibility/terrain_3d_vt_visibility_types.h"
#include <stdint.h>
#include <vector>

// T02: a cold-only local RenderingDevice raster bake. This output is per-probe
// *raw* visibility; T03 owns aggregation and publication into T3VD v1.
#define T3D_VT_VISIBILITY_MAX_MATERIALS 1024u
#define T3D_VT_VISIBILITY_MAX_PROBES 256u
#define T3D_VT_VISIBILITY_MAX_TRIANGLES_PER_PROBE 65536u

struct t3dVTVisibilityProbeResult_t
{
	int32_t m_nCellX = 0, m_nCellZ = 0;
	uint32_t m_nSourceRevision = 0u;
	uint32_t m_nMaterialCount = 0u;
	uint32_t m_nNominalTextureSize = 0u;
	uint32_t m_nNominalRenderSize = 0u;
	uint32_t m_nOpaqueTriangles = 0u;
	uint32_t m_nUncapturedTriangles = 0u;
	// material-major, 8 yaw bins, 16 mip bins (uint32 visible samples).
	std::vector<uint32_t> m_counts;
	bool m_bPartial = true;
	bool m_bHasVisibleSamples = false;
};

struct t3dVTVisibilityBakeStats_t
{
	uint64_t m_nGpuVisibleSamples = 0u;
	uint32_t m_nCapturedProbes = 0u;
	uint32_t m_nUncapturedTriangles = 0u;
	uint32_t m_nGpuPasses = 0u;
	uint32_t m_nOverflowedCounters = 0u;
	uint32_t m_nReadbackBytes = 0u;
};

class Terrain3DVTVisibilityBaker
{
public:
	// Cold authoring helper: pick a deterministic center/quarter-cell sample
	// where an opaque near-horizontal authored triangle supplies a valid ground
	// height. Returns false and no probes if the footprint is unrepresented.
	static bool selectCellProbes( const t3dVTVisibilityStaticVertex_t *pVertices, uint32_t nVertexCount,
	                              const t3dVTVisibilityStaticTriangle_t *pTriangles, uint32_t nFirstTriangle, uint32_t nTriangleCount,
	                              int32_t nCellX, int32_t nCellZ, uint32_t nSourceRevision, uint32_t nNominalRenderSize,
	                              std::vector<t3dVTVisibilityProbeInput_t>& rProbes );

	// Synchronous *offline* bake. Requires a Vulkan-capable local RD; returns
	// false without publishing partial results on any GPU/resource failure.
	// Caller supplies immutable scene buffers with lifetimes covering the call.
	bool bake( const t3dVTVisibilityStaticSceneView_t& rScene, const t3dVTDemandVisibilityCookConfig_t& rConfig,
	           std::vector<t3dVTVisibilityProbeResult_t>& rResults, t3dVTVisibilityBakeStats_t& rStats ) const;
};

#endif
