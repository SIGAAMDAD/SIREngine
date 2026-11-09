// CS04: bounded CPU-only composed page requests; no RD objects or workers.
#ifndef TERRAIN3D_COMPOSED_SURFACE_CACHE_H
#define TERRAIN3D_COMPOSED_SURFACE_CACHE_H
#include "rendering/rd/vt/core/terrain_3d_composed_surface_contract.h"
#include <stddef.h>

#define T3D_COMP_SURF_SLOT_FREE 0u
#define T3D_COMP_SURF_SLOT_READING 1u
#define T3D_COMP_SURF_SLOT_READY 2u
#define T3D_COMP_SURF_SLOT_UPLOADING 3u
#define T3D_COMP_SURF_SLOT_RESIDENT 4u
#define T3D_COMP_SURF_SLOT_RETIRING 5u

struct t3dComposedSurfaceSidecarView_t
{
	const uint8_t *m_pFile = nullptr;
	uint64_t m_nFileBytes = 0u;
	t3dComposedSurfaceFileHeader_t m_header{};
};
struct t3dComposedSurfaceRequest_t
{
	uint32_t m_nPageIndex = 0u;
	uint32_t m_nPriority = 0u;
};
struct t3dComposedSurfaceSlot_t
{
	uint64_t m_nToken = 0u;
	uint64_t m_nLastTouch = 0u;
	uint32_t m_nPageIndex = T3D_COMP_SURF_INVALID_SLOT;
	uint32_t m_nDeviceGeneration = 0u;
	uint32_t m_nState = T3D_COMP_SURF_SLOT_FREE;
};
struct t3dComposedSurfaceJob_t
{
	uint64_t m_nToken = 0u;
	uint32_t m_nPageIndex = T3D_COMP_SURF_INVALID_SLOT;
	uint32_t m_nSlot = T3D_COMP_SURF_INVALID_SLOT;
	uint32_t m_nDeviceGeneration = 0u;
};
struct t3dComposedSurfaceStats_t
{
	uint64_t m_nHits = 0u;
	uint64_t m_nMisses = 0u;
	uint64_t m_nDeduplicated = 0u;
	uint64_t m_nRetirements = 0u;
	uint64_t m_nStaleCompletions = 0u;
	uint64_t m_nCorruptPages = 0u;
	uint32_t m_nPending = 0u;
};

// Checked once on cold open. Record directory is sorted by the editor writer.
bool t3d_comp_surf_open_sidecar_cold( const uint8_t *pBytes, size_t nBytes, uint64_t nExpectedSourceFingerprint,
                                      t3dComposedSurfaceSidecarView_t& out );
bool t3d_comp_surf_sidecar_record( const t3dComposedSurfaceSidecarView_t& view, uint32_t nPageIndex, t3dComposedSurfacePageRecord_t& out );
bool t3d_comp_surf_find_page( const t3dComposedSurfaceSidecarView_t& view, uint64_t nRegionKey, uint32_t nMip, uint32_t nX, uint32_t nY,
                              uint64_t nRevision, uint32_t& rPageIndex );
// Editor dirty page queries must also locate the *old* revision being retired.
bool t3d_comp_surf_find_page_location( const t3dComposedSurfaceSidecarView_t& view, uint64_t nRegionKey, uint32_t nMip, uint32_t nX,
                                       uint32_t nY, uint32_t& rPageIndex );

class Terrain3DComposedSurfaceCache
{
public:
	// Both arrays are cold-allocated by the shared owner and memory-accounted.
	bool initializeCold( const t3dComposedSurfaceSidecarView_t& view, t3dComposedSurfaceSlot_t *pSlots, uint32_t nSlots,
	                     t3dComposedSurfaceRequest_t *pQueue, uint32_t nQueueCapacity, uint32_t nDeviceGeneration );
	void resetDeviceCold( uint32_t nDeviceGeneration );
	bool demandHot( uint32_t nPageIndex, uint32_t nPriority, uint64_t nTouch );
	// When no free slots exist, returns a retirement token; caller retires
	// the GPU mapping before confirming retirement and reusing the slot.
	bool beginEvictionHot( t3dComposedSurfaceJob_t& rRetirement );
	// Explicit edit-time invalidation. Never evict an unrelated resident page
	// when an existing composed page has been dirtied by the authoring owner.
	bool beginInvalidationHot( uint32_t nPageIndex, t3dComposedSurfaceJob_t& rRetirement );
	bool retirementPendingHot( const t3dComposedSurfaceJob_t& retirement ) const;
	// Restore CPU visibility when a GPU table update was rejected. The GPU
	// owner alone decides when previous physical texture use has retired.
	bool cancelRetirementHot( const t3dComposedSurfaceJob_t& retirement );
	bool confirmEvictionHot( const t3dComposedSurfaceJob_t& retirement );
	// Shared VT upload budget is admitted by caller BEFORE work is claimed;
	// pass false to defer without consuming a request or physical slot.
	bool claimNextHot( bool bTransferBudgetGranted, t3dComposedSurfaceJob_t& rJob );
	bool readJobCold( const t3dComposedSurfaceJob_t& job, uint8_t *pOutput, uint32_t nOutputCapacity );
	// Validate job ownership *before* touching any reusable physical GPU slot.
	bool readyForUploadHot( const t3dComposedSurfaceJob_t& job, t3dComposedSurfacePageRecord_t& rRecord ) const;
	bool uploadingRecordHot( const t3dComposedSurfaceJob_t& job, t3dComposedSurfacePageRecord_t& rRecord ) const;
	bool markUploadSubmittedHot( const t3dComposedSurfaceJob_t& job );
	bool completeUploadHot( const t3dComposedSurfaceJob_t& job, uint32_t nUploadedChannelMask, bool bSubmissionConfirmed );
	bool cancelJobHot( const t3dComposedSurfaceJob_t& job );
	bool residentMappingHot( uint32_t nPageIndex, t3dComposedSurfacePageMapping_t& rMapping ) const;
	// Read-only residency view for the editor dirty-page reconciliation bridge.
	uint32_t stateForPageHot( uint32_t nPageIndex ) const;
	bool cancelUnsubmittedPageHot( uint32_t nPageIndex );
	// Forget an obsolete queued request when its source revision is dirtied.
	bool forgetPendingDemandHot( uint32_t nPageIndex );
	const t3dComposedSurfaceSidecarView_t& sidecarViewCold() const
	{
		return m_view;
	}
	const t3dComposedSurfaceStats_t& stats() const
	{
		return m_stats;
	}

private:
	bool owns( const t3dComposedSurfaceJob_t& job, uint32_t nState ) const;
	uint32_t locate( uint32_t nPageIndex ) const;
	t3dComposedSurfaceSidecarView_t m_view{};
	t3dComposedSurfaceSlot_t *m_pSlots = nullptr;
	t3dComposedSurfaceRequest_t *m_pRequests = nullptr;
	uint32_t m_nCapacity = 0u;
	uint32_t m_nRequestCapacity = 0u;
	uint32_t m_nDeviceGeneration = 0u;
	uint64_t m_nNextToken = 1u;
	t3dComposedSurfaceStats_t m_stats{};
};
#endif
