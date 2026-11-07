#include "rendering/rd/vt/gpu/terrain_3d_composed_surface_gpu.h"
#include "rendering/rd/core/terrain_3d_rd_context.h"
#include "rendering/rd/vt/core/terrain_3d_composed_surface_completion.h"
#include <string.h>

using namespace godot;

bool Terrain3DComposedSurfaceGpu::initializeCold( Terrain3DRdContext& rContext, const uint32_t nPageSlots, const uint32_t nTableCapacity,
                                                  t3dComposedSurfaceGpuEntry_t *pTableStorage,
                                                  t3dComposedSurfaceGpuPending_t *pPendingStorage, const uint32_t nDeviceGeneration )
{
	if ( m_bInitialized || !pPendingStorage || !nPageSlots || !nDeviceGeneration || nPageSlots > UINT32_MAX / 4u ||
	     nTableCapacity < nPageSlots * 4u || nTableCapacity > UINT32_MAX / sizeof( t3dComposedSurfaceGpuEntry_t ) ||
	     !m_table.initializeCold( pTableStorage, nTableCapacity, nDeviceGeneration ) )
		return false;
	m_pPending = pPendingStorage;
	m_nSlots = nPageSlots;
	m_nDeviceGeneration = nDeviceGeneration;
	for ( uint32_t slot = 0u; slot < nPageSlots; ++slot )
		m_pPending[slot] = {};

	if ( !rContext.getDevice() ||
	     nPageSlots > uint32_t( rContext.getDevice()->limit_get( RenderingDevice::LIMIT_MAX_TEXTURE_ARRAY_LAYERS ) ) ) {
		shutdownCold( rContext );
		return false;
	}
	const RenderingDevice::DataFormat formats[T3D_COMP_SURF_CHANNEL_COUNT] = { RenderingDevice::DATA_FORMAT_BC1_RGBA_UNORM_BLOCK,
	                                                                           RenderingDevice::DATA_FORMAT_BC5_UNORM_BLOCK,
	                                                                           RenderingDevice::DATA_FORMAT_BC7_UNORM_BLOCK };
	const uint64_t usage = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT |
	                       RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	for ( uint32_t i = 0u; i < T3D_COMP_SURF_CHANNEL_COUNT; ++i ) {
		t3dRdTextureSpec_t spec;
		spec.m_name = String( "Terrain3D composed surface channel " ) + String::num_uint64( i );
		spec.m_owner = T3D_RD_CONTEXT_OWNER_VT;
		spec.m_memory = T3D_RD_CONTEXT_MEMORY_COMPOSED_SURFACE;
		spec.m_layout.m_type = RenderingDevice::TEXTURE_TYPE_2D_ARRAY;
		spec.m_layout.m_format = formats[i];
		spec.m_layout.m_nWidth = T3D_COMP_SURF_PHYSICAL_TILE;
		spec.m_layout.m_nHeight = T3D_COMP_SURF_PHYSICAL_TILE;
		spec.m_layout.m_nLayers = nPageSlots;
		spec.m_layout.m_nMipmaps = 1u;
		spec.m_layout.m_nUsageBits = usage;
		spec.m_layout.m_nBytes = uint64_t( nPageSlots ) * t3d_comp_surf_channel_bytes( i );
		if ( !m_channels[i].createTracked( rContext, spec ) || m_staging[i].resize( t3d_comp_surf_channel_bytes( i ) ) != OK ) {
			shutdownCold( rContext );
			return false;
		}
	}
	t3dRdGpuBufferSpec_t tableSpec;
	tableSpec.m_pName = "Terrain3D composed surface page mappings";
	tableSpec.m_owner = T3D_RD_CONTEXT_OWNER_VT;
	tableSpec.m_memory = T3D_RD_CONTEXT_MEMORY_COMPOSED_SURFACE;
	tableSpec.m_type = T3D_RD_CONTEXT_RESOURCE_STORAGE_BUFFER;
	tableSpec.m_growth = T3D_GPU_EXACT;
	const uint32_t nTableBytes = nTableCapacity * sizeof( t3dComposedSurfaceGpuEntry_t );
	// Create the cold GPU lookup with a single zero-initialized payload.
	// The old path issued one separate buffer update per hash slot.
	PackedByteArray zeroTable;
	if ( zeroTable.resize( nTableBytes ) != OK ) {
		shutdownCold( rContext );
		return false;
	}
	memset( zeroTable.ptrw(), 0, nTableBytes );
	if ( !m_pageTable.configure( tableSpec ) || !m_pageTable.createTracked( rContext, nTableBytes, zeroTable ) ||
	     m_bufferFallback.resize( sizeof( t3dComposedSurfaceGpuEntry_t ) ) != OK ) {
		shutdownCold( rContext );
		return false;
	}
	m_bMappingsCoherent = true;
	m_bInitialized = true;
	return true;
}

void Terrain3DComposedSurfaceGpu::shutdownCold( Terrain3DRdContext& rContext )
{
	m_pageTable.releaseTracked( rContext );
	for ( uint32_t i = 0u; i < T3D_COMP_SURF_CHANNEL_COUNT; ++i ) {
		m_channels[i].releaseTracked( rContext );
		m_staging[i] = PackedByteArray();
	}
	m_bufferFallback = PackedByteArray();
	if ( m_pPending ) {
		for ( uint32_t i = 0u; i < m_nSlots; ++i )
			m_pPending[i] = {};
	}
	m_table.resetCold( 0u );
	m_pPending = nullptr;
	m_nSlots = 0u;
	m_nDeviceGeneration = 0u;
	m_bInitialized = false;
	m_bMappingsCoherent = false;
}

bool Terrain3DComposedSurfaceGpu::writeTableEntryHot( Terrain3DRdContext& rContext, const uint32_t index )
{
	if ( !m_bMappingsCoherent || index >= m_table.capacity() || !m_table.entries() || !m_pageTable.isValid() )
		return false;
	if ( m_pageTable.writeTracked( rContext, index * sizeof( t3dComposedSurfaceGpuEntry_t ), &m_table.entries()[index],
	                               sizeof( t3dComposedSurfaceGpuEntry_t ), m_bufferFallback, T3D_GPU_DIRTY_UPLOAD ) )
		return true;
	m_bMappingsCoherent = false;
	return false;
}

bool Terrain3DComposedSurfaceGpu::submitPageSetHot( Terrain3DRdContext& rContext, Terrain3DComposedSurfaceCache& rCache,
                                                    const t3dComposedSurfaceJob_t& job, const uint8_t *pPageBytes,
                                                    const uint32_t nPageBytes, const int32_t nWorldPageX, const int32_t nWorldPageY,
                                                    const uint32_t nMip, const uint64_t nRevision, const uint64_t nRegionKey,
                                                    const uint64_t nSubmissionEpoch )
{
	if ( !m_bInitialized || !m_bMappingsCoherent || !pPageBytes || nPageBytes != T3D_COMP_SURF_PAGE_PAYLOAD_BYTES ||
	     job.m_nSlot >= m_nSlots || job.m_nDeviceGeneration != m_nDeviceGeneration || nMip != 0u || !nRevision || !nSubmissionEpoch ||
	     m_pPending[job.m_nSlot].m_bPending )
		return false;
	t3dComposedSurfacePageRecord_t sourcePage{};
	if ( !nRegionKey || !rCache.readyForUploadHot( job, sourcePage ) || sourcePage.m_nRegionKey != nRegionKey ||
	     sourcePage.m_nCompositionRevision != nRevision || sourcePage.m_nMip != nMip )
		return false;
	uint32_t offset = 0u;
	uint32_t mask = 0u;
	for ( uint32_t channel = 0u; channel < T3D_COMP_SURF_CHANNEL_COUNT; ++channel ) {
		const uint32_t size = t3d_comp_surf_channel_bytes( channel );
		memcpy( m_staging[channel].ptrw(), pPageBytes + offset, size );
		if ( !m_channels[channel].updateLayerTracked( rContext, job.m_nSlot, m_staging[channel], T3D_GPU_DIRTY_UPLOAD ) )
			return false;
		mask |= ( 1u << channel );
		offset += size;
	}
	if ( !rCache.markUploadSubmittedHot( job ) )
		return false;
	t3dComposedSurfaceGpuPending_t& pending = m_pPending[job.m_nSlot];
	pending.m_job = job;
	pending.m_nWorldPageX = nWorldPageX;
	pending.m_nWorldPageY = nWorldPageY;
	pending.m_nRevision = nRevision;
	pending.m_nMip = nMip;
	pending.m_nUploadedChannels = mask;
	pending.m_nUploadEpoch = nSubmissionEpoch;
	pending.m_bPending = true;
	return true;
}

bool Terrain3DComposedSurfaceGpu::confirmAndPublishHot( Terrain3DRdContext& rContext, Terrain3DComposedSurfaceCache& rCache,
                                                        const t3dComposedSurfaceJob_t& job, const uint64_t nCompletedEpoch )
{
	if ( !m_bInitialized || !m_bMappingsCoherent || !nCompletedEpoch || job.m_nSlot >= m_nSlots )
		return false;
	const t3dComposedSurfaceGpuPending_t pending = m_pPending[job.m_nSlot];
	if ( !pending.m_bPending || pending.m_job.m_nToken != job.m_nToken || pending.m_job.m_nPageIndex != job.m_nPageIndex ||
	     pending.m_job.m_nDeviceGeneration != job.m_nDeviceGeneration || pending.m_nUploadedChannels != T3D_COMP_SURF_CHANNEL_MASK_ALL ||
	     !pending.m_nUploadEpoch || !t3d_comp_surf_gpu_epoch_completed( pending.m_nUploadEpoch, nCompletedEpoch ) )
		return false;
	t3dComposedSurfacePageRecord_t page{};
	if ( !rCache.uploadingRecordHot( job, page ) || page.m_nCompositionRevision != pending.m_nRevision )
		return false;
	t3dComposedSurfacePageMapping_t mapping{};
	mapping.m_nRegionKey = page.m_nRegionKey;
	mapping.m_nCompositionRevision = page.m_nCompositionRevision;
	mapping.m_nPageX = page.m_nPageX;
	mapping.m_nPageY = page.m_nPageY;
	mapping.m_nMip = page.m_nMip;
	mapping.m_nGeneration = page.m_nGeneration;
	mapping.m_nDeviceGeneration = m_nDeviceGeneration;
	mapping.m_nPhysicalSlot = job.m_nSlot;
	mapping.m_nResidentChannels = pending.m_nUploadedChannels;
	mapping.m_nState = T3D_COMP_SURF_PAGE_RESIDENT;
	uint32_t dirty = T3D_COMP_SURF_INVALID_SLOT;
	t3dComposedSurfaceGpuEntry_t previous{};
	if ( !m_table.stageResident( pending.m_nWorldPageX, pending.m_nWorldPageY, pending.m_nMip, mapping, dirty, &previous ) )
		return false;
	const t3dComposedSurfaceGpuEntry_t staged = m_table.entries()[dirty];
	if ( !writeTableEntryHot( rContext, dirty ) ) {
		m_table.rollbackStagedResident( dirty, staged, previous );
		return false;
	}
	if ( !rCache.completeUploadHot( job, pending.m_nUploadedChannels, true ) ) {
		// The newly published entry must be removed and the previous empty
		// or tombstone state restored. Failure to upload the rollback poisons
		// the mapping owner and disables all future composed acquisitions.
		if ( !m_table.rollbackStagedResident( dirty, staged, previous ) || !writeTableEntryHot( rContext, dirty ) )
			m_bMappingsCoherent = false;
		return false;
	}
	m_pPending[job.m_nSlot] = {};
	return true;
}

uint32_t Terrain3DComposedSurfaceGpu::serviceCompletedUploadsHot( Terrain3DRdContext& rContext, Terrain3DComposedSurfaceCache& rCache,
                                                                  const uint64_t nCompletedEpoch )
{
	if ( !m_bInitialized || !m_bMappingsCoherent || !nCompletedEpoch )
		return 0u;
	uint32_t published = 0u;
	for ( uint32_t slot = 0u; slot < m_nSlots; ++slot ) {
		const t3dComposedSurfaceGpuPending_t pending = m_pPending[slot];
		if ( pending.m_bPending && pending.m_nUploadEpoch && pending.m_nUploadEpoch <= nCompletedEpoch &&
		     confirmAndPublishHot( rContext, rCache, pending.m_job, nCompletedEpoch ) )
			++published;
	}
	return published;
}

bool Terrain3DComposedSurfaceGpu::retireHot( Terrain3DRdContext& rContext, Terrain3DComposedSurfaceCache& rCache,
                                             const t3dComposedSurfaceJob_t& retirement, const uint64_t nLastRasterUseEpoch,
                                             const uint64_t nCompletedEpoch )
{
	if ( !m_bInitialized || !m_bMappingsCoherent || !t3d_comp_surf_gpu_epoch_completed( nLastRasterUseEpoch, nCompletedEpoch ) ||
	     retirement.m_nSlot >= m_nSlots || retirement.m_nDeviceGeneration != m_nDeviceGeneration ||
	     !rCache.retirementPendingHot( retirement ) )
		return false;
	// Keep the last CPU mapping if the RD update is rejected; never acknowledge
	// CS04 retirement while the GPU still refers to the physical layer.
	uint32_t previousIndex = T3D_COMP_SURF_INVALID_SLOT;
	for ( uint32_t i = 0u; i < m_table.capacity(); ++i ) {
		const t3dComposedSurfaceGpuEntry_t& entry = m_table.entries()[i];
		if ( entry.m_nState == T3D_COMP_SURF_GPU_MAP_RESIDENT && entry.m_nPhysicalSlot == retirement.m_nSlot ) {
			previousIndex = i;
			break;
		}
	}
	if ( previousIndex == T3D_COMP_SURF_INVALID_SLOT )
		return false;
	const t3dComposedSurfaceGpuEntry_t previous = m_table.entries()[previousIndex];
	uint32_t dirty = T3D_COMP_SURF_INVALID_SLOT;
	if ( !m_table.retireSlot( retirement.m_nSlot, dirty ) )
		return false;
	if ( !writeTableEntryHot( rContext, dirty ) ) {
		m_table.restoreRetirementAfterFailedWrite( dirty, previous );
		return false;
	}
	// Caller must guarantee that pre-existing raster submissions have retired
	// before confirming CS04 slot reuse. This call never forces an RD sync.
	return rCache.confirmEvictionHot( retirement );
}

bool Terrain3DComposedSurfaceGpu::cancelPendingHot( Terrain3DComposedSurfaceCache& rCache, const t3dComposedSurfaceJob_t& job,
                                                    const bool bGpuUseRetired )
{
	if ( !m_bMappingsCoherent || !bGpuUseRetired || job.m_nSlot >= m_nSlots || !m_pPending || !m_pPending[job.m_nSlot].m_bPending ||
	     m_pPending[job.m_nSlot].m_job.m_nToken != job.m_nToken || !rCache.cancelJobHot( job ) )
		return false;
	m_pPending[job.m_nSlot] = {};
	return true;
}

bool Terrain3DComposedSurfaceGpu::acquireBindingsHot( t3dComposedSurfaceGpuBindings_t& rBindings ) const
{
	rBindings = {};
	if ( !m_bInitialized || !m_bMappingsCoherent || !m_pageTable.isValid() )
		return false;
	for ( uint32_t i = 0u; i < T3D_COMP_SURF_CHANNEL_COUNT; ++i ) {
		if ( !m_channels[i].isValid() )
			return false;
		rBindings.m_hChannels[i] = m_channels[i].rid();
	}
	rBindings.m_hPageTable = m_pageTable.rid();
	rBindings.m_nPageCapacity = m_nSlots;
	rBindings.m_nTableCapacity = m_table.capacity();
	rBindings.m_nDeviceGeneration = m_nDeviceGeneration;
	return true;
}
