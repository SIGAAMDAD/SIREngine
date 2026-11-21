// CS07: bounded mapping from a journal ticket to ordered .t3sc cook-page flags.
#ifndef TERRAIN3D_COMPOSED_SURFACE_RECOOK_BRIDGE_H
#define TERRAIN3D_COMPOSED_SURFACE_RECOOK_BRIDGE_H
#include "authoring/vt/integration/terrain_3d_composed_surface_edit_journal.h"
#include "authoring/vt/build/terrain_3d_composed_surface_cooker.h"

// pFlags is caller-owned fixed/cold scratch, nCapacity >= nPages.
// Return false for stale/incompatible page lists or unsupported page counts.
// Consumes no dirty ticket and never acknowledges publication.
bool t3d_comp_surf_make_recook_flags_cold( const t3dComposedSurfaceEditTicket_t& ticket, uint64_t nRegionKey, uint32_t nMip0Width,
                                           uint32_t nMip0Height, const t3dComposedSurfaceCookPage_t *pPages, uint32_t nPages,
                                           uint8_t *pFlags, uint32_t nCapacity );

// Only calls the CS03–CS04 incremental writer if the canonical producer has
// established CS02 parity. Callers must verify current journal generation
// before and after work and acknowledge only AFTER sidecar atomic commit.
bool t3d_comp_surf_recook_journal_ticket_cold( const Terrain3DComposedSurfaceEditJournal& journal,
                                               const t3dComposedSurfaceEditTicket_t& ticket, uint64_t nRegionKey, uint32_t nMip0Width,
                                               uint32_t nMip0Height, const t3dComposedSurfaceCookPage_t *pPages, uint32_t nPages,
                                               const t3dComposedSurfaceCookSource_t& source, const uint8_t *pPreviousFile,
                                               uint64_t nPreviousBytes, uint8_t *pDirtyFlags, uint32_t nDirtyCapacity, uint8_t *pScratch,
                                               uint32_t nScratchBytes, uint8_t *pOutput, uint64_t nOutputCapacity,
                                               uint64_t& rWrittenBytes );
#endif
