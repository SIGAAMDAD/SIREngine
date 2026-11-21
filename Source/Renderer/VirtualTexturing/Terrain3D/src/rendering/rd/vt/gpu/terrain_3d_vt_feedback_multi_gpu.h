// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_MULTI_GPU_H
#define TERRAIN3D_VT_FEEDBACK_MULTI_GPU_H

#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi_capture.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/infrastructure/resources/terrain_3d_rd_texture.h"
#include "rendering/rd/infrastructure/resources/terrain_3d_rd_shader.h"
#include "rendering/rd/infrastructure/resources/terrain_3d_rd_sampler.h"
#include "rendering/rd/infrastructure/pipeline/terrain_3d_rd_pipeline.h"
#include "rendering/rd/infrastructure/buffers/terrain_3d_rd_gpu_buffer.h"
#include "rendering/rd/infrastructure/descriptors/terrain_3d_rd_global_descriptor_contract.h"
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/callable.hpp>

// Experimental V1 GPU service. Source RIDs are borrowed, not produced here.
// Creation/binding and destruction require a quiescent COLD device boundary.
// No V1 capture is admitted to production residency by this service.
class Terrain3DVTFeedbackMultiGpu
{
public:
	bool initializeCold( Terrain3DRdContext& rContext, const t3dVTFeedbackGridLayout_t& rLayout, const godot::RID& rPages,
	                     const godot::RID& rCounts, uint32_t nViewportWidth, uint32_t nViewportHeight, uint32_t nSourceLayers,
	                     uint32_t nPageCount );
	// P31d: debug/capture-replay source owned by this service, not a live raster
	// producer. Allocations and staging are completed before the HOT seal.
	bool initializeOwnedCold( Terrain3DRdContext& rContext, const t3dVTFeedbackGridLayout_t& rLayout, uint32_t nViewportWidth,
	                          uint32_t nViewportHeight, uint32_t nSourceLayers, uint32_t nPageCount );
	bool publishOwnedCaptureHot( Terrain3DRdContext& rContext, const t3dVTFeedbackMultiCaptureView_t& rCapture,
	                             const t3dVTFeedbackMultiSourceProof_t& rProof );
	bool hasOwnedSource() const
	{
		return m_ownedPages.isValid() && m_ownedCounts.isValid();
	}
	bool dispatchHot( Terrain3DRdContext& rContext, const t3dVTFeedbackMultiSourceProof_t& rProof, uint32_t nFrame, uint32_t nEpoch,
	                  uint32_t nDatabase, uint32_t nDevice, uint64_t nViewport, uint32_t& rSlot );
	bool requestReadbackHot( Terrain3DRdContext& rContext, uint32_t nSlot, const godot::Callable& rCallback );
	bool consumeReadbackHot( const godot::PackedByteArray& rBytes );
	const uint8_t *peekReadyHot( uint32_t nSlot, t3dVTFeedbackGridHeader_t& rHeader ) const;
	bool retireReadyHot( uint32_t nSlot );
	void invalidateHot();
	// Caller must have completed all submitted GPU work before normal shutdown.
	void shutdownCold( Terrain3DRdContext& rContext );
	void abandonDevice();
	bool ready() const
	{
		return m_pipeline.isValid() && m_shader.isValid() && m_output.isValid();
	}
	const t3dVTFeedbackMultiParams_t& params() const
	{
		return m_params;
	}
	const t3dVTFeedbackGridLayout_t& layout() const
	{
		return m_layout;
	}
	uint32_t skipped() const
	{
		return m_nSkipped;
	}

private:
	Terrain3DRdShader m_shader;
	Terrain3DRdPipeline m_pipeline;
	Terrain3DRdSampler m_sampler;
	Terrain3DRdTexture m_ownedPages;
	Terrain3DRdTexture m_ownedCounts;
	godot::PackedByteArray m_pageUpload[T3D_VT_FEEDBACK_MULTI_MAX_LAYERS];
	godot::PackedByteArray m_countUpload;
	t3dVTFeedbackMultiSourceProof_t m_publishedProof{};
	bool m_bOwnedCapturePublished = false;
	Terrain3DRdGpuBuffer m_output;
	t3dRdDescriptorLease_t m_lease{};
	t3dVTFeedbackGridLayout_t m_layout{};
	t3dVTFeedbackMultiParams_t m_params{};
	t3dVTFeedbackMultiSlot_t m_slots[T3D_VT_FEEDBACK_GRID_SLOT_COUNT];
	godot::PackedByteArray m_readback[T3D_VT_FEEDBACK_GRID_SLOT_COUNT];
	godot::PackedByteArray m_headerBytes;
	godot::PackedByteArray m_pushBytes;
	godot::RID m_sourcePages;
	godot::RID m_sourceCounts;
	uint64_t m_nDeviceGeneration = 0u;
	uint32_t m_nNextSlot = 0u;
	uint32_t m_nSkipped = 0u;
};

#endif // TERRAIN3D_VT_FEEDBACK_MULTI_GPU_H
