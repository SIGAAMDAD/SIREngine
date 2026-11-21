// CS09: match editor dirtied composed pages to old resident .t3sc records.
#ifndef TERRAIN3D_COMPOSED_SURFACE_RESIDENT_BRIDGE_H
#define TERRAIN3D_COMPOSED_SURFACE_RESIDENT_BRIDGE_H
#include "authoring/vt/build/terrain_3d_composed_surface_cooker.h"
#include "rendering/rd/vt/runtime/terrain_3d_composed_surface_cache.h"

// Called by the editor/render owner before committing changed sidecar pages.
// Cancels non-submitted reads, and yields explicit GPU retirement jobs for
// residents. Uploading and already-retiring pages defer the entire operation.
// The caller must run the GPU owner retirement protocol before slot reuse.
// Caller supplies fixed retirement storage; no allocation occurs here.
bool t3d_comp_surf_begin_dirty_page_retirements_hot( Terrain3DComposedSurfaceCache& rCache, const t3dComposedSurfaceCookPage_t *pNewPages,
                                                     const uint8_t *pDirtyFlags, uint32_t nPages, t3dComposedSurfaceJob_t *pRetirements,
                                                     uint32_t nRetirementCapacity, uint32_t& rRetirementCount );
#endif
