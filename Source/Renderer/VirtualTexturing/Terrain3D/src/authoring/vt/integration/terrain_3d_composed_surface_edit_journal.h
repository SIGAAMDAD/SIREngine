// CS07: editor-owner only composed-surface invalidation; no render-thread RID access.
#ifndef TERRAIN3D_COMPOSED_SURFACE_EDIT_JOURNAL_H
#define TERRAIN3D_COMPOSED_SURFACE_EDIT_JOURNAL_H

#include <math.h>
#include <stdint.h>

#define T3D_COMP_SURF_EDITOR_REGION_COUNT 1024u
#define T3D_COMP_SURF_EDITOR_REGION_SIZE 128.0f
#define T3D_COMP_SURF_EDITOR_CONTROL_HALO_TEXELS 2.0f
#define T3D_COMP_SURF_EDITOR_GUTTER_TEXELS 4.0f

struct t3dComposedSurfaceEditBounds_t
{
	float m_fMinX, m_fMinZ, m_fMaxX, m_fMaxZ;
};
struct t3dComposedSurfaceEditRegion_t
{
	uint32_t m_nIndex = 0u;
	int32_t m_nRegionX = 0, m_nRegionZ = 0;
	uint16_t m_nWidth = 0u, m_nHeight = 0u, m_nMips = 0u;
	bool m_bPresent = false;
};
struct t3dComposedSurfaceEditRecord_t
{
	uint64_t m_nRevision = 0u;
	uint64_t m_nClaimRevision = 0u;
	uint64_t m_nSourceFingerprint = 0u;
	uint64_t m_nTopologyGeneration = 0u;
	uint32_t m_nDirtyMip0Mask = 0u;
	uint32_t m_nDirtyClosure = 0u;
	bool m_bInFlight = false;
};
struct t3dComposedSurfaceEditTicket_t
{
	uint64_t m_nRevision = 0u;
	uint64_t m_nSourceFingerprint = 0u;
	uint64_t m_nTopologyGeneration = 0u;
	uint32_t m_nRegionIndex = 0u;
	uint32_t m_nDirtyMip0Mask = 0u;
	uint32_t m_nDirtyClosure = 0u;
};
struct t3dComposedSurfaceEditStats_t
{
	uint64_t m_nEditEvents = 0u;
	uint64_t m_nSourceInvalidations = 0u;
	uint64_t m_nStaleCompletions = 0u;
	uint64_t m_nAcceptedCompletions = 0u;
	uint64_t m_nRemovedRegions = 0u;
	uint32_t m_nDirtyRegions = 0u;
};

// No heap allocations, no timer, no second MegaVT cook claim. This state
// must not clear merely because the independent source MegaVT job completed.
class Terrain3DComposedSurfaceEditJournal
{
public:
	void resetCold();
	bool dirtyRegionCold( const t3dComposedSurfaceEditRegion_t& region, const t3dComposedSurfaceEditBounds_t& bounds,
	                      uint32_t nTexelsPerMeter, uint64_t nFingerprint, uint64_t nTopologyGeneration );
	bool invalidateRegionCold( const t3dComposedSurfaceEditRegion_t& region, uint64_t nFingerprint, uint64_t nTopologyGeneration );
	bool forgetRegionCold( uint32_t nIndex, uint64_t nTopologyGeneration );
	bool claimCold( uint32_t nIndex, t3dComposedSurfaceEditTicket_t& rTicket );
	bool completionCurrentCold( const t3dComposedSurfaceEditTicket_t& ticket, uint64_t nFingerprint, uint64_t nTopologyGeneration ) const;
	bool acceptCold( const t3dComposedSurfaceEditTicket_t& ticket, uint64_t nFingerprint, uint64_t nTopologyGeneration );
	void cancelCold( const t3dComposedSurfaceEditTicket_t& ticket );
	const t3dComposedSurfaceEditRecord_t *regionCold( uint32_t nIndex ) const;
	const t3dComposedSurfaceEditStats_t& statsCold() const
	{
		return m_stats;
	}

private:
	static uint32_t closure( uint32_t nWidth, uint32_t nHeight, uint32_t nMips, uint32_t nMask );
	bool invalidateCold( const t3dComposedSurfaceEditRegion_t& region, uint32_t nMip0Mask, uint64_t nFingerprint,
	                     uint64_t nTopologyGeneration );
	t3dComposedSurfaceEditRecord_t m_records[T3D_COMP_SURF_EDITOR_REGION_COUNT]{};
	t3dComposedSurfaceEditStats_t m_stats{};
};
#endif
