// Editor owner bridge: no resource mutation or sidecar IO in map edit callbacks.
#ifndef TERRAIN3D_COMPOSED_SURFACE_COORDINATOR_HOOKS_H
#define TERRAIN3D_COMPOSED_SURFACE_COORDINATOR_HOOKS_H
#include "authoring/vt/integration/terrain_3d_composed_surface_edit_journal.h"
#include "rendering/rd/vt/runtime/terrain_3d_terrain_megavt.h"

inline bool t3d_comp_surf_editor_region( const Terrain3DTerrainMegaVT& megaVT, uint32_t nIndex, t3dComposedSurfaceEditRegion_t& out )
{
	const t3dTerrainMegaVTRegion_t *region = megaVT.getRegion( nIndex );
	if ( !region || !( region->m_nFlags & T3D_TERRAIN_MEGAVT_REGION_PRESENT ) )
		return false;
	int32_t x = 0, z = 0;
	Terrain3DTerrainMegaVT::regionIndexToLocation( nIndex, x, z );
	out = { nIndex, x, z, region->m_nMip0WidthPages, region->m_nMip0HeightPages, region->m_nMipCount, true };
	return true;
}

inline void t3d_comp_surf_editor_dirty_bounds( const Terrain3DTerrainMegaVT& megaVT, Terrain3DComposedSurfaceEditJournal& journal,
                                               const t3dComposedSurfaceEditBounds_t& bounds, uint64_t nFingerprint )
{
	// Project the bounded control+gutter halo onto resident region slots.
	// This runs on an edit event and visits only overlapping regions rather
	// than scanning the full 32x32 world directory on each brush dab.
	if ( !nFingerprint || !megaVT.getTexelsPerMeter() || !isfinite( bounds.m_fMinX ) || !isfinite( bounds.m_fMinZ ) ||
	     !isfinite( bounds.m_fMaxX ) || !isfinite( bounds.m_fMaxZ ) || bounds.m_fMaxX <= bounds.m_fMinX ||
	     bounds.m_fMaxZ <= bounds.m_fMinZ )
		return;
	const float halo =
	    ( T3D_COMP_SURF_EDITOR_CONTROL_HALO_TEXELS + T3D_COMP_SURF_EDITOR_GUTTER_TEXELS ) / float( megaVT.getTexelsPerMeter() );
	const float side = T3D_COMP_SURF_EDITOR_REGION_SIZE;
	const int32_t half = int32_t( T3D_REGION_MAP_HALF );
	const float worldMin = -float( half ) * side;
	const float worldMax = float( half ) * side;
	if ( bounds.m_fMaxX + halo <= worldMin || bounds.m_fMaxZ + halo <= worldMin || bounds.m_fMinX - halo >= worldMax ||
	     bounds.m_fMinZ - halo >= worldMax )
		return;
	const int32_t x0 = int32_t( floorf( fmaxf( worldMin, bounds.m_fMinX - halo ) / side ) );
	const int32_t z0 = int32_t( floorf( fmaxf( worldMin, bounds.m_fMinZ - halo ) / side ) );
	const int32_t x1 = int32_t( floorf( ( fminf( worldMax, bounds.m_fMaxX + halo ) - 0.0001f ) / side ) );
	const int32_t z1 = int32_t( floorf( ( fminf( worldMax, bounds.m_fMaxZ + halo ) - 0.0001f ) / side ) );
	const uint64_t topology = megaVT.getTopologyGeneration() ? megaVT.getTopologyGeneration() : 1u;
	for ( int32_t z = z0; z <= z1; ++z ) {
		for ( int32_t x = x0; x <= x1; ++x ) {
			uint32_t index = 0u;
			if ( !Terrain3DTerrainMegaVT::locationToRegionIndex( x, z, index ) )
				continue;
			t3dComposedSurfaceEditRegion_t region{};
			if ( t3d_comp_surf_editor_region( megaVT, index, region ) )
				journal.dirtyRegionCold( region, bounds, megaVT.getTexelsPerMeter(), nFingerprint, topology );
		}
	}
}

inline void t3d_comp_surf_editor_invalidate_sources( const Terrain3DTerrainMegaVT& megaVT, Terrain3DComposedSurfaceEditJournal& journal,
                                                     uint64_t nFingerprint )
{
	const uint64_t topology = megaVT.getTopologyGeneration() ? megaVT.getTopologyGeneration() : 1u;
	for ( uint32_t i = 0u; i < T3D_COMP_SURF_EDITOR_REGION_COUNT; ++i ) {
		t3dComposedSurfaceEditRegion_t region{};
		if ( t3d_comp_surf_editor_region( megaVT, i, region ) )
			journal.invalidateRegionCold( region, nFingerprint, topology );
		else
			journal.forgetRegionCold( i, topology );
	}
}
#endif
