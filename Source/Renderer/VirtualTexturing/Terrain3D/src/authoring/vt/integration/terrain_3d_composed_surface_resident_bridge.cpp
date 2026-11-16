#include "authoring/vt/integration/terrain_3d_composed_surface_resident_bridge.h"

bool t3d_comp_surf_begin_dirty_page_retirements_hot( Terrain3DComposedSurfaceCache& rCache, const t3dComposedSurfaceCookPage_t *pNewPages,
                                                     const uint8_t *pDirtyFlags, const uint32_t nPages,
                                                     t3dComposedSurfaceJob_t *pRetirements, const uint32_t nRetirementCapacity,
                                                     uint32_t& rRetirementCount )
{
	rRetirementCount = 0u;
	if ( !pNewPages || !pDirtyFlags || !pRetirements || !nPages )
		return false;
	const t3dComposedSurfaceSidecarView_t& view = rCache.sidecarViewCold();
	if ( !view.m_pFile )
		return false;

	// Validate every dirty request before changing any residency state.
	// Revision is deliberately omitted: dirty authoring pages supersede the
	// old revisions currently mapped by this immutable sidecar view.
	uint32_t requiredRetirements = 0u;
	for ( uint32_t i = 0u; i < nPages; ++i ) {
		if ( pDirtyFlags[i] > 1u )
			return false;
		if ( i ) {
			const t3dComposedSurfaceCookPage_t& previous = pNewPages[i - 1u];
			const t3dComposedSurfaceCookPage_t& page = pNewPages[i];
			if ( previous.m_nRegionKey == page.m_nRegionKey && previous.m_nMip == page.m_nMip && previous.m_nPageX == page.m_nPageX &&
			     previous.m_nPageY == page.m_nPageY )
				return false;
		}
		if ( pDirtyFlags[i] == 0u )
			continue;
		const t3dComposedSurfaceCookPage_t& page = pNewPages[i];
		uint32_t oldIndex = T3D_COMP_SURF_INVALID_SLOT;
		if ( !t3d_comp_surf_find_page_location( view, page.m_nRegionKey, page.m_nMip, page.m_nPageX, page.m_nPageY, oldIndex ) )
			continue;
		const uint32_t state = rCache.stateForPageHot( oldIndex );
		if ( state == T3D_COMP_SURF_SLOT_UPLOADING || state == T3D_COMP_SURF_SLOT_RETIRING )
			return false;
		if ( state == T3D_COMP_SURF_SLOT_RESIDENT && ++requiredRetirements > nRetirementCapacity )
			return false;
	}

	for ( uint32_t i = 0u; i < nPages; ++i ) {
		if ( !pDirtyFlags[i] )
			continue;
		const t3dComposedSurfaceCookPage_t& page = pNewPages[i];
		uint32_t oldIndex = T3D_COMP_SURF_INVALID_SLOT;
		if ( !t3d_comp_surf_find_page_location( view, page.m_nRegionKey, page.m_nMip, page.m_nPageX, page.m_nPageY, oldIndex ) )
			continue;
		// The queued request still refers to the retired sidecar revision.
		// Remove it before any new sidecar may become authoritative.
		rCache.forgetPendingDemandHot( oldIndex );
		const uint32_t state = rCache.stateForPageHot( oldIndex );
		if ( state == T3D_COMP_SURF_SLOT_READING || state == T3D_COMP_SURF_SLOT_READY ) {
			if ( !rCache.cancelUnsubmittedPageHot( oldIndex ) )
				return false;
		} else if ( state == T3D_COMP_SURF_SLOT_RESIDENT ) {
			if ( !rCache.beginInvalidationHot( oldIndex, pRetirements[rRetirementCount] ) )
				return false;
			++rRetirementCount;
		}
	}
	return true;
}
