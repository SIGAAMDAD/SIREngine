#include "authoring/vt/integration/terrain_3d_composed_surface_edit_journal.h"

#include <string.h>

void Terrain3DComposedSurfaceEditJournal::resetCold()
{
	memset( m_records, 0, sizeof( m_records ) );
	m_stats = {};
}

uint32_t Terrain3DComposedSurfaceEditJournal::closure( uint32_t nWidth, uint32_t nHeight, uint32_t nMips, uint32_t nMask )
{
	if ( !nWidth || !nHeight || nWidth * nHeight > 16u || !nMips )
		return 0u;
	uint32_t result = nMask, offset = 0u;
	for ( uint32_t mip = 1u; mip < nMips; ++mip ) {
		const uint32_t nextWidth = nWidth > 1u ? ( nWidth + 1u ) / 2u : 1u;
		const uint32_t nextHeight = nHeight > 1u ? ( nHeight + 1u ) / 2u : 1u;
		const uint32_t nextOffset = offset + nWidth * nHeight;
		if ( nextOffset + nextWidth * nextHeight > 32u )
			return 0u;
		const uint32_t mask = ( result >> offset ) & ( ( 1u << ( nWidth * nHeight ) ) - 1u );
		for ( uint32_t y = 0u; y < nHeight; ++y )
			for ( uint32_t x = 0u; x < nWidth; ++x )
				if ( mask & ( 1u << ( y * nWidth + x ) ) )
					result |= 1u << ( nextOffset + ( y / 2u ) * nextWidth + x / 2u );
		offset = nextOffset;
		nWidth = nextWidth;
		nHeight = nextHeight;
	}
	return result;
}

bool Terrain3DComposedSurfaceEditJournal::invalidateCold( const t3dComposedSurfaceEditRegion_t& region, uint32_t nMip0Mask,
                                                          uint64_t nFingerprint, uint64_t nTopologyGeneration )
{
	if ( !region.m_bPresent || region.m_nIndex >= T3D_COMP_SURF_EDITOR_REGION_COUNT || !nFingerprint || !nTopologyGeneration ||
	     !region.m_nWidth || !region.m_nHeight || uint32_t( region.m_nWidth ) * region.m_nHeight > 16u || !region.m_nMips )
		return false;
	const uint32_t mask = nMip0Mask & ( ( 1u << ( region.m_nWidth * region.m_nHeight ) ) - 1u );
	if ( !mask )
		return false;
	const uint32_t dirtyClosure = closure( region.m_nWidth, region.m_nHeight, region.m_nMips, mask );
	if ( !dirtyClosure )
		return false;
	t3dComposedSurfaceEditRecord_t& record = m_records[region.m_nIndex];
	if ( !record.m_nDirtyMip0Mask )
		++m_stats.m_nDirtyRegions;
	record.m_nDirtyMip0Mask |= mask;
	record.m_nDirtyClosure |= dirtyClosure;
	record.m_nRevision = record.m_nRevision == UINT64_MAX ? 1u : record.m_nRevision + 1u;
	record.m_nSourceFingerprint = nFingerprint;
	record.m_nTopologyGeneration = nTopologyGeneration;
	// A stale in-flight claim is rejected at completion, never reused.
	return true;
}

bool Terrain3DComposedSurfaceEditJournal::dirtyRegionCold( const t3dComposedSurfaceEditRegion_t& region,
                                                           const t3dComposedSurfaceEditBounds_t& bounds, uint32_t nTexelsPerMeter,
                                                           uint64_t nFingerprint, uint64_t nTopologyGeneration )
{
	if ( !region.m_bPresent || !nTexelsPerMeter || !isfinite( bounds.m_fMinX ) || !isfinite( bounds.m_fMinZ ) ||
	     !isfinite( bounds.m_fMaxX ) || !isfinite( bounds.m_fMaxZ ) || bounds.m_fMaxX <= bounds.m_fMinX ||
	     bounds.m_fMaxZ <= bounds.m_fMinZ || !region.m_nWidth || !region.m_nHeight )
		return false;
	const float halo = ( T3D_COMP_SURF_EDITOR_CONTROL_HALO_TEXELS + T3D_COMP_SURF_EDITOR_GUTTER_TEXELS ) / float( nTexelsPerMeter );
	const float regionX = float( region.m_nRegionX ) * T3D_COMP_SURF_EDITOR_REGION_SIZE;
	const float regionZ = float( region.m_nRegionZ ) * T3D_COMP_SURF_EDITOR_REGION_SIZE;
	const float minX = fmaxf( bounds.m_fMinX - halo, regionX );
	const float minZ = fmaxf( bounds.m_fMinZ - halo, regionZ );
	const float maxX = fminf( bounds.m_fMaxX + halo, regionX + T3D_COMP_SURF_EDITOR_REGION_SIZE );
	const float maxZ = fminf( bounds.m_fMaxZ + halo, regionZ + T3D_COMP_SURF_EDITOR_REGION_SIZE );
	if ( minX >= maxX || minZ >= maxZ )
		return false;
	const float scaleX = float( region.m_nWidth ) / T3D_COMP_SURF_EDITOR_REGION_SIZE;
	const float scaleZ = float( region.m_nHeight ) / T3D_COMP_SURF_EDITOR_REGION_SIZE;
	const uint32_t x0 = uint32_t( fmaxf( 0.0f, floorf( ( minX - regionX ) * scaleX ) ) );
	const uint32_t z0 = uint32_t( fmaxf( 0.0f, floorf( ( minZ - regionZ ) * scaleZ ) ) );
	const uint32_t x1 = uint32_t( fminf( float( region.m_nWidth - 1u ), ceilf( ( maxX - regionX ) * scaleX ) - 1.0f ) );
	const uint32_t z1 = uint32_t( fminf( float( region.m_nHeight - 1u ), ceilf( ( maxZ - regionZ ) * scaleZ ) - 1.0f ) );
	uint32_t mask = 0u;
	for ( uint32_t z = z0; z <= z1; ++z )
		for ( uint32_t x = x0; x <= x1; ++x )
			mask |= 1u << ( z * region.m_nWidth + x );
	const bool changed = invalidateCold( region, mask, nFingerprint, nTopologyGeneration );
	if ( changed )
		++m_stats.m_nEditEvents;
	return changed;
}

bool Terrain3DComposedSurfaceEditJournal::invalidateRegionCold( const t3dComposedSurfaceEditRegion_t& region, uint64_t nFingerprint,
                                                                uint64_t nTopologyGeneration )
{
	if ( !region.m_nWidth || !region.m_nHeight || uint32_t( region.m_nWidth ) * region.m_nHeight > 16u )
		return false;
	const bool changed = invalidateCold( region, ( 1u << ( region.m_nWidth * region.m_nHeight ) ) - 1u, nFingerprint, nTopologyGeneration );
	if ( changed )
		++m_stats.m_nSourceInvalidations;
	return changed;
}

bool Terrain3DComposedSurfaceEditJournal::forgetRegionCold( uint32_t nIndex, uint64_t nTopologyGeneration )
{
	if ( nIndex >= T3D_COMP_SURF_EDITOR_REGION_COUNT || !nTopologyGeneration )
		return false;
	t3dComposedSurfaceEditRecord_t& record = m_records[nIndex];
	if ( record.m_nTopologyGeneration == nTopologyGeneration )
		return false;
	if ( !record.m_nRevision && !record.m_nDirtyMip0Mask )
		return false;
	if ( record.m_nDirtyMip0Mask )
		--m_stats.m_nDirtyRegions;
	record.m_nDirtyMip0Mask = 0u;
	record.m_nDirtyClosure = 0u;
	record.m_bInFlight = false;
	record.m_nClaimRevision = 0u;
	record.m_nRevision = record.m_nRevision == UINT64_MAX ? 1u : record.m_nRevision + 1u;
	record.m_nTopologyGeneration = nTopologyGeneration;
	record.m_nSourceFingerprint = 0u;
	++m_stats.m_nRemovedRegions;
	return true;
}

bool Terrain3DComposedSurfaceEditJournal::claimCold( uint32_t nIndex, t3dComposedSurfaceEditTicket_t& rTicket )
{
	if ( nIndex >= T3D_COMP_SURF_EDITOR_REGION_COUNT )
		return false;
	t3dComposedSurfaceEditRecord_t& record = m_records[nIndex];
	if ( record.m_bInFlight || !record.m_nDirtyMip0Mask )
		return false;
	record.m_bInFlight = true;
	record.m_nClaimRevision = record.m_nRevision;
	rTicket = { record.m_nRevision,      record.m_nSourceFingerprint, record.m_nTopologyGeneration, nIndex,
	            record.m_nDirtyMip0Mask, record.m_nDirtyClosure };
	return true;
}

bool Terrain3DComposedSurfaceEditJournal::completionCurrentCold( const t3dComposedSurfaceEditTicket_t& ticket, uint64_t nFingerprint,
                                                                 uint64_t nTopologyGeneration ) const
{
	if ( ticket.m_nRegionIndex >= T3D_COMP_SURF_EDITOR_REGION_COUNT )
		return false;
	const t3dComposedSurfaceEditRecord_t& record = m_records[ticket.m_nRegionIndex];
	return record.m_bInFlight && record.m_nClaimRevision == ticket.m_nRevision && record.m_nRevision == ticket.m_nRevision &&
	       record.m_nSourceFingerprint == ticket.m_nSourceFingerprint && record.m_nTopologyGeneration == ticket.m_nTopologyGeneration &&
	       nFingerprint == ticket.m_nSourceFingerprint && nTopologyGeneration == ticket.m_nTopologyGeneration &&
	       record.m_nDirtyMip0Mask == ticket.m_nDirtyMip0Mask && record.m_nDirtyClosure == ticket.m_nDirtyClosure;
}

bool Terrain3DComposedSurfaceEditJournal::acceptCold( const t3dComposedSurfaceEditTicket_t& ticket, uint64_t nFingerprint,
                                                      uint64_t nTopologyGeneration )
{
	if ( ticket.m_nRegionIndex >= T3D_COMP_SURF_EDITOR_REGION_COUNT )
		return false;
	if ( !completionCurrentCold( ticket, nFingerprint, nTopologyGeneration ) ) {
		++m_stats.m_nStaleCompletions;
		cancelCold( ticket );
		return false;
	}
	t3dComposedSurfaceEditRecord_t& record = m_records[ticket.m_nRegionIndex];
	record.m_nDirtyMip0Mask = 0u;
	record.m_nDirtyClosure = 0u;
	record.m_bInFlight = false;
	record.m_nClaimRevision = 0u;
	--m_stats.m_nDirtyRegions;
	++m_stats.m_nAcceptedCompletions;
	return true;
}

void Terrain3DComposedSurfaceEditJournal::cancelCold( const t3dComposedSurfaceEditTicket_t& ticket )
{
	if ( ticket.m_nRegionIndex >= T3D_COMP_SURF_EDITOR_REGION_COUNT )
		return;
	t3dComposedSurfaceEditRecord_t& record = m_records[ticket.m_nRegionIndex];
	// Late cancellation from an earlier revision must not touch a new claim.
	if ( record.m_bInFlight && record.m_nClaimRevision == ticket.m_nRevision ) {
		record.m_bInFlight = false;
		record.m_nClaimRevision = 0u;
	}
}

const t3dComposedSurfaceEditRecord_t *Terrain3DComposedSurfaceEditJournal::regionCold( uint32_t nIndex ) const
{
	return nIndex < T3D_COMP_SURF_EDITOR_REGION_COUNT ? &m_records[nIndex] : nullptr;
}
