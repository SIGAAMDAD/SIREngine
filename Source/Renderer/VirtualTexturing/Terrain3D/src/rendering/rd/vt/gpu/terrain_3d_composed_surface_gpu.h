// CS05: cold physical composed page arrays and render-owner transaction boundary.
#ifndef TERRAIN3D_COMPOSED_SURFACE_GPU_H
#define TERRAIN3D_COMPOSED_SURFACE_GPU_H

#include "rendering/rd/vt/core/terrain_3d_composed_surface_gpu_table.h"
#include "rendering/rd/vt/runtime/terrain_3d_composed_surface_cache.h"
#include "rendering/rd/infrastructure/resources/terrain_3d_rd_texture.h"
#include "rendering/rd/infrastructure/buffers/terrain_3d_rd_gpu_buffer.h"

struct t3dComposedSurfaceGpuPending_t
{
	t3dComposedSurfaceJob_t m_job{};
	int32_t m_nWorldPageX = 0;
	int32_t m_nWorldPageY = 0;
	uint64_t m_nRevision = 0u;
	uint32_t m_nMip = 0u;
	uint32_t m_nUploadedChannels = 0u;
	uint64_t m_nUploadEpoch = 0u;
	bool m_bPending = false;
};

struct t3dComposedSurfaceGpuBindings_t
{
	godot::RID m_hChannels[T3D_COMP_SURF_CHANNEL_COUNT];
	godot::RID m_hPageTable;
	uint32_t m_nPageCapacity = 0u;
	uint32_t m_nTableCapacity = 0u;
	uint32_t m_nDeviceGeneration = 0u;
};

class Terrain3DComposedSurfaceGpu
{
public:
	// Caller supplies fixed CPU table and transaction lanes. Table capacity
	// must be power-of-two and >=4 * pageSlots. Source VT V1 is unaffected.
	bool initializeCold( Terrain3DRdContext& rContext, uint32_t nPageSlots, uint32_t nTableCapacity,
	                     t3dComposedSurfaceGpuEntry_t *pTableStorage, t3dComposedSurfaceGpuPending_t *pPendingStorage,
	                     uint32_t nDeviceGeneration );
	void shutdownCold( Terrain3DRdContext& rContext );
	// Source payload comes from CS04 readJobCold (all CRCs validated there).
	// Shared VT transfer-budget admission must precede this call.
	bool submitPageSetHot( Terrain3DRdContext& rContext, Terrain3DComposedSurfaceCache& rCache, const t3dComposedSurfaceJob_t& job,
	                       const uint8_t *pPageBytes, uint32_t nPageBytes, int32_t nWorldPageX, int32_t nWorldPageY, uint32_t nMip,
	                       uint64_t nRevision, uint64_t nRegionKey, uint64_t nSubmissionEpoch );
	// The caller provides positive GPU completion evidence (frame epoch/fence)
	// and invokes this only at a pass-coherent render-owner publication boundary.
	bool confirmAndPublishHot( Terrain3DRdContext& rContext, Terrain3DComposedSurfaceCache& rCache, const t3dComposedSurfaceJob_t& job,
	                           uint64_t nCompletedEpoch );
	// Must unpublish before CS04 confirmEvictionHot permits slot reuse.
	bool retireHot( Terrain3DRdContext& rContext, Terrain3DComposedSurfaceCache& rCache, const t3dComposedSurfaceJob_t& retirement,
	                uint64_t nLastRasterUseEpoch, uint64_t nCompletedEpoch );
	// Only confirmed epochs from the global GPU readback owner may publish.
	uint32_t serviceCompletedUploadsHot( Terrain3DRdContext& rContext, Terrain3DComposedSurfaceCache& rCache, uint64_t nCompletedEpoch );
	bool cancelPendingHot( Terrain3DComposedSurfaceCache& rCache, const t3dComposedSurfaceJob_t& job, bool bGpuUseRetired );
	bool acquireBindingsHot( t3dComposedSurfaceGpuBindings_t& rBindings ) const;
	bool isInitialized() const
	{
		return m_bInitialized;
	}
	bool areMappingsCoherent() const
	{
		return m_bInitialized && m_bMappingsCoherent;
	}

private:
	bool writeTableEntryHot( Terrain3DRdContext& rContext, uint32_t index );
	Terrain3DRdTexture m_channels[T3D_COMP_SURF_CHANNEL_COUNT];
	Terrain3DRdGpuBuffer m_pageTable;
	Terrain3DComposedSurfaceGpuTable m_table;
	t3dComposedSurfaceGpuPending_t *m_pPending = nullptr;
	uint32_t m_nSlots = 0u;
	uint32_t m_nDeviceGeneration = 0u;
	bool m_bInitialized = false;
	// Fail-closed on an unconfirmed table write. A cold resource replacement
	// is required before any bindings can be acquired again.
	bool m_bMappingsCoherent = false;
	godot::PackedByteArray m_staging[T3D_COMP_SURF_CHANNEL_COUNT];
	godot::PackedByteArray m_bufferFallback;
};

#endif
