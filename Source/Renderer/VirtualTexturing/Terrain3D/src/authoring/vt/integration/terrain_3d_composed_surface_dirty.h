// CS03 incremental planner follows MegaVT's existing dirty-bound expansion,
// mip0 closure and generation authority; no parallel scan or painter hook.
#ifndef TERRAIN3D_COMPOSED_SURFACE_DIRTY_H
#define TERRAIN3D_COMPOSED_SURFACE_DIRTY_H
#include "rendering/rd/vt/runtime/terrain_3d_terrain_megavt.h"
#include "authoring/vt/build/terrain_3d_composed_surface_cooker.h"

struct t3dComposedSurfaceDirtyRegion_t
{
	uint32_t m_nRegionIndex;
	uint32_t m_nSourceGeneration;
	uint32_t m_nDirtyMip0Mask;
	uint32_t m_nDirtyClosure;
	uint64_t m_nSourceFingerprint;
};

inline bool t3d_comp_surf_capture_dirty_region_cold( const Terrain3DTerrainMegaVT& megavt, const uint32_t nRegionIndex,
                                                     const uint64_t nSourceFingerprint, t3dComposedSurfaceDirtyRegion_t& out )
{
	const t3dTerrainMegaVTRegion_t *region = megavt.getRegion( nRegionIndex );
	uint32_t generation = 0u, mask = 0u;
	if ( !region || !nSourceFingerprint || !megavt.getPersistentEditorState( nRegionIndex, generation, mask ) || mask == 0u )
		return false;
	const uint32_t closure =
	    Terrain3DTerrainMegaVT::computeDirtyPageClosure( region->m_nMip0WidthPages, region->m_nMip0HeightPages, region->m_nMipCount, mask );
	if ( !closure )
		return false;
	out = { nRegionIndex, generation, mask, closure, nSourceFingerprint };
	return true;
}

inline bool t3d_comp_surf_dirty_region_still_current( const Terrain3DTerrainMegaVT& megavt, const t3dComposedSurfaceDirtyRegion_t& ticket,
                                                      const uint64_t nCurrentSourceFingerprint )
{
	uint32_t generation = 0u, mask = 0u;
	return nCurrentSourceFingerprint == ticket.m_nSourceFingerprint &&
	       megavt.getPersistentEditorState( ticket.m_nRegionIndex, generation, mask ) && generation == ticket.m_nSourceGeneration &&
	       mask == ticket.m_nDirtyMip0Mask;
}

// CS07 will wire this independent sidecar capture to editor publication. No
// second call to claimIncrementalCookHot(): that claim belongs to MegaVT.
#endif
