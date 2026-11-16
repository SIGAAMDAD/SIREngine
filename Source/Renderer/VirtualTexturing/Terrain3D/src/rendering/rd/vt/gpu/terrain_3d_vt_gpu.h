// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_GPU_H
#define TERRAIN3D_VT_GPU_H

#include "rendering/rd/infrastructure/descriptors/terrain_3d_rd_global_descriptor_contract.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include "Terrain3DTypes.h"

#include "rendering/rd/vt/gpu/terrain_3d_vt_resources.h"
#include "rendering/rd/vt/gpu/terrain_3d_vt_feedback_epoch.h"
#include "rendering/rd/vt/gpu/terrain_3d_vt_feedback_multi_gpu.h"
#include "rendering/rd/vt/streaming/terrain_3d_vt_streaming.h"
#include "rendering/rd/vt/core/terrain_3d_vt_ancestor_hint.h"

#include <godot_cpp/variant/callable.hpp>
#include "rendering/rd/infrastructure/descriptors/terrain_3d_rd_uniform_set.h"
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/rid.hpp>

class Terrain3DVTGpu
{
public:
	Terrain3DVTGpu();
	~Terrain3DVTGpu();

	bool prepare_cpu_cold( const t3dVTGpuResourcePlan_t& pPlan, const t3dWorldLimits_t& pLimits );
	// Precompose file-backed mip tails before RD publication; no I/O in GPU upload stage.
	bool prepare_file_tails_cold( const t3dVTDatabaseView_t& rDatabase );
	bool stage_file_tails_warm( const t3dVTDatabaseView_t& rDatabase );
	void backup_file_tails_warm( const t3dVTDatabaseView_t& rDatabase );
	void discard_staged_file_tails_warm();
	bool prepare_storage_cold( const Terrain3DVTResourceImage *pResources, const t3dWorldLimits_t& pLimits );
	bool bind_executable_cold();
	bool service_bind_executable_cold( uint32_t nMaxWorkUnits );
	bool initialize_cold( const Terrain3DVTResourceImage *pResources, const t3dWorldLimits_t& pLimits );
	void shutdown_cold( bool bPreserveTailStaging = false );
	void seal_hot_runtime();
	bool begin_package_publication_warm();
	void set_database_view_warm( const t3dVTDatabaseView_t *pDatabase )
	{
		m_pDatabaseView = pDatabase;
	}
	bool upload_database_warm( const t3dVTDatabaseView_t& pDatabase, const t3dVTResidencyState_t& pResidency );
	bool service_database_upload_warm( const t3dVTDatabaseView_t& pDatabase, const t3dVTResidencyState_t& pResidency, uint64_t nMaxBytes,
	                                   uint32_t nMaxWorkUnits );
	bool is_database_upload_pending_warm() const;
	bool update_allocation_quality_hot( const t3dVTResidencyState_t& rResidency );
	// BR07-BR09: derived hint snapshot, disabled until every affected record is published.
	bool service_ancestor_hints_hot( const t3dVTResidencyState_t& rResidency, uint32_t nBudget = T3D_VT_HINT_MAX_REBUILD_PER_FRAME );
	bool upload_neutral_page_set_warm( t3dVTResidencyState_t& rResidency, uint32_t pPhysicalSlot );
	bool service_retired_slots_hot( t3dVTResidencyState_t& rResidency, uint32_t pFrameId );
	bool begin_upload_batch_hot();
	bool end_upload_batch_hot( t3dVTResidencyState_t& rResidency, uint32_t pFrameId );
	bool upload_page_set_commit_hot( t3dVTResidencyState_t& rResidency, const t3dVTUploadReservation_t& pReservation,
	                                 const t3dVTPageSetPayloadView_t& pPayloads, uint32_t pFrameId );
	bool upload_commit_hot( t3dVTResidencyState_t& rResidency, const t3dVTUploadReservation_t& pReservation, const uint8_t *pDecodedBytes,
	                        uint32_t pDecodedByteCount, uint32_t pFrameId );
	static bool upload_batch_begin_callback( void *pUser );
	static bool upload_batch_end_callback( t3dVTResidencyState_t& rResidency, uint32_t pFrameId, void *pUser );
	static bool upload_commit_callback( t3dVTResidencyState_t& rResidency, const t3dVTUploadReservation_t& pReservation,
	                                    const uint8_t *pDecodedBytes, uint32_t pDecodedByteCount, uint32_t pFrameId, void *pUser );

	bool is_initialized() const;
	bool is_storage_ready() const;
	bool is_executable_ready() const;
	bool is_readback_enabled() const;
	// WORLD feedback variants use the coarse-grid set-3 ABI, not the old hash ABI.
	bool is_fragment_feedback_shader_ready() const;
	uint32_t get_fragment_feedback_shader_generation() const
	{
		return m_nFeedbackShaderGeneration;
	}
	// Render-thread state: coverage mask is valid only for the current camera cell and pass.
	void set_predicted_terrain_materials_hot( uint32_t nMask )
	{
		m_nPredictedTerrainMaterials = nMask;
	}
	uint32_t get_predicted_terrain_materials_hot() const
	{
		return m_nPredictedTerrainMaterials;
	}
	// Only the 32-byte UBO changes; no hash reset or feedback readback.
	bool publish_predicted_terrain_mask_hot();
	bool has_fragment_feedback_callback() const
	{
		return m_feedbackCallback.is_valid();
	}
	void set_fragment_feedback_callback_cold( const godot::Callable& rCallback );
	void consume_fragment_feedback_readback_hot( const godot::PackedByteArray& rBytes );
	// P00-P04: cold-owned 4-slot coarse-feedback capture. The grid producer is
	// opt-in until all raster shaders use the versioned word contract.
	bool enable_feedback_grid_cold( bool bEnable );
	bool begin_feedback_grid_capture_hot( uint32_t nFrame, uint32_t nDatabase, uint32_t nDevice, uint64_t nViewport, uint32_t nPageCount,
	                                      uint32_t nAllocationCount, uint32_t nViewportWidth, uint32_t nViewportHeight, uint32_t& rSlot );
	bool service_feedback_grid_capture_hot( uint32_t nFrame, uint32_t nDatabase, uint32_t nDevice, uint64_t nViewport, uint32_t nPageCount,
	                                        uint32_t nAllocationCount, uint32_t nViewportWidth, uint32_t nViewportHeight );
	bool mark_feedback_grid_drawn_hot( uint32_t nSlot );
	bool submit_feedback_grid_readback_hot( uint32_t nSlot );
	const uint8_t *peek_feedback_grid_ready_hot( uint32_t nSlot, t3dVTFeedbackGridHeader_t& rHeader ) const;
	bool retire_feedback_grid_ready_hot( uint32_t nSlot );
	void record_feedback_grid_consumed_hot( uint32_t nValidSamples );
	bool get_feedback_grid_layout_cold( t3dVTFeedbackGridLayout_t& rLayout ) const;
	uint64_t get_feedback_grid_samples() const
	{
		return m_nFeedbackGridSamples;
	}
	uint64_t get_feedback_grid_captures() const
	{
		return m_nFeedbackGridCaptures;
	}
	uint64_t get_feedback_grid_skipped() const
	{
		return m_nFeedbackGridSkipped;
	}
	uint64_t get_feedback_grid_stale() const
	{
		return m_nFeedbackGridStale;
	}
	bool is_feedback_grid_enabled() const
	{
		return m_bFeedbackGridEnabled;
	}
	// P31c: externally produced, complete and depth-tested V1 sources only.
	// The active V2 feedback producer remains authoritative.
	bool prepare_multi_feedback_cold( const godot::RID& rPages, const godot::RID& rCounts, uint32_t nViewportWidth,
	                                  uint32_t nViewportHeight, uint32_t nSourceLayers, uint32_t nPageCount );
	// P31d: optional full-resolution replay source; no production raster input.
	bool prepare_multi_feedback_replay_cold( uint32_t nViewportWidth, uint32_t nViewportHeight, uint32_t nSourceLayers,
	                                         uint32_t nPageCount );
	bool publish_multi_feedback_replay_hot( const t3dVTFeedbackMultiCaptureView_t& rCapture,
	                                        const t3dVTFeedbackMultiSourceProof_t& rProof );
	bool dispatch_multi_feedback_hot( const t3dVTFeedbackMultiSourceProof_t& rProof, uint32_t nFrame, uint32_t nEpoch, uint32_t nDatabase,
	                                  uint32_t nDevice, uint64_t nViewport, uint32_t& rSlot );
	bool submit_multi_feedback_readback_hot( uint32_t nSlot );
	const uint8_t *peek_multi_feedback_ready_hot( uint32_t nSlot, t3dVTFeedbackGridHeader_t& rHeader ) const;
	bool retire_multi_feedback_ready_hot( uint32_t nSlot );
	bool get_multi_feedback_params_cold( uint32_t nSlot, t3dVTFeedbackMultiParams_t& rParams ) const;
	bool is_multi_feedback_ready() const
	{
		return m_multiFeedbackGpu.ready();
	}
	uint32_t get_feedback_epoch_hot() const
	{
		return m_nFeedbackEpoch;
	}

	uint32_t service_fragment_feedback_hot( t3dVTResidencyState_t& rResidency, t3dVTStreamingState_t& rStreaming, uint32_t nFrameId,
	                                        uint32_t nRequestBudget );
	godot::RID get_fragment_feedback_set_cold( const godot::RID& rShader, uint32_t nVariant );
	godot::RID resolve_fragment_feedback_set_hot( const godot::RID& rPrevious );
	uint64_t get_fragment_feedback_consumed() const
	{
		return m_nFeedbackConsumed;
	}
	uint64_t get_fragment_feedback_misses() const
	{
		return m_nFeedbackMisses;
	}
	uint64_t get_fragment_feedback_overflow() const
	{
		return m_nFeedbackOverflow;
	}
	uint64_t get_fragment_feedback_dropped() const
	{
		return m_nFeedbackDropped;
	}
	uint64_t get_predicted_mask_uploads_saved() const
	{
		return m_nPredictedMaskUploadsSaved;
	}
	bool get_physical_family_binding_v1( uint32_t pFamily, godot::RID& rRid, uint32_t& rLayerCount ) const;
	bool validate_physical_format_families_v1_cold() const;
	uint64_t get_reserved_gpu_bytes() const;
	const t3dVTGpuStats_t& get_stats() const;
	const t3dVTGpuResourcePlan_t& get_cpu_resource_plan() const
	{
		return m_Plan;
	}

private:
	bool prepare_ancestor_graph_warm( const t3dVTResidencyState_t& rResidency );
	bool invalidate_ancestor_header_hot();
	bool enqueue_ancestor_root_hot( uint32_t nPage );
	bool invalidate_ancestor_page_hot( uint32_t nPage );
	void shutdown_ancestor_hints_cold();
	void invalidate_feedback_grid_hot();
	bool consume_feedback_grid_callback_hot( const godot::PackedByteArray& rBytes );
	bool update_page_entry_hot( uint32_t pVirtualPage, const t3dVTPageTableEntry_t& pEntry );
	bool publish_upload_batch_page_table_hot( const t3dVTResidencyState_t& rResidency );
	void abort_upload_batch_hot( t3dVTResidencyState_t& rResidency );
	bool make_gpu_page_entry( uint32_t nVirtualPage, const t3dVTPageTableEntry_t& pCpuEntry, t3dVTPageTableEntry_t& rGpuEntry ) const;
	bool retire_victim_gpu_hot( t3dVTResidencyState_t& rResidency, uint32_t pPhysicalSlot, uint32_t pVictimPage, uint32_t pFrameId );
	bool map_physical_slot_hot( uint32_t pPhysicalSlot, uint32_t& rBank, uint32_t& rLayer ) const;
	bool validate_physical_bank_layout_cold() const;
	bool initialize_neutral_payloads_cold();
	uint32_t physical_layer_bytes( uint32_t pChannel ) const;
	bool hot_epoch_unchanged() const;

	uint64_t m_nPreparedFileTailSignature = 0u;
	uint64_t m_nCandidateTailSignature = 0u;
	uint64_t m_nRollbackTailSignature = 0u;
	godot::PackedByteArray m_CandidateTailUpload[T3D_VT_CHANNEL_COUNT];
	godot::PackedByteArray m_RollbackTailUpload[T3D_VT_CHANNEL_COUNT];
	godot::Callable m_feedbackCallback;
	godot::PackedByteArray m_feedbackReset;
	godot::PackedByteArray m_feedbackCounterReset;
	godot::PackedByteArray m_feedbackParams;
	godot::PackedByteArray m_feedbackReadback;
	// All storage and byte snapshots are allocated before sealing.
	Terrain3DVTFeedbackMultiGpu m_multiFeedbackGpu;
	t3dVTFeedbackGridLayout_t m_feedbackGridLayout{};
	t3dVTFeedbackGridSlot_t m_feedbackGridSlots[T3D_VT_FEEDBACK_GRID_SLOT_COUNT];
	godot::PackedByteArray m_feedbackGridReset;
	godot::PackedByteArray m_feedbackGridReadbacks[T3D_VT_FEEDBACK_GRID_SLOT_COUNT];
	uint32_t m_nFeedbackGridNextSlot = 0u;
	bool m_bFeedbackGridEnabled = false;
	uint64_t m_nFeedbackGridSkipped = 0u;
	uint64_t m_nFeedbackGridCaptures = 0u;
	uint64_t m_nFeedbackGridSamples = 0u;
	uint64_t m_nFeedbackGridStale = 0u;
	uint32_t m_nFeedbackGridActiveFrame = UINT32_MAX;
	uint64_t m_nFeedbackGridActiveViewport = 0u;

	t3dRdDescriptorLease_t m_feedbackSets[8u * T3D_RD_FORWARD_COLOR_VARIANT_COUNT];
	godot::RID m_feedbackResolvedSets[8u * T3D_RD_FORWARD_COLOR_VARIANT_COUNT];
	godot::RID m_feedbackSetShaders[8u * T3D_RD_FORWARD_COLOR_VARIANT_COUNT];
	bool m_feedbackSetGridMode[8u * T3D_RD_FORWARD_COLOR_VARIANT_COUNT]{};
	uint32_t m_nPredictedTerrainMaterials = 0u;
	uint32_t m_nFeedbackEpoch = 1u;
	uint32_t m_nFeedbackShaderGeneration = 1u;
	uint32_t m_nFeedbackFrame = UINT32_MAX;
	uint32_t m_nFeedbackDatabase = 0u;
	t3dVTFeedbackEpochState_t m_feedbackEpoch;
	bool m_bFeedbackWritten = false;
	uint64_t m_nFeedbackConsumed = 0u;
	uint64_t m_nFeedbackDropped = 0u;
	uint64_t m_nFeedbackMisses = 0u;
	uint64_t m_nFeedbackOverflow = 0u;
	uint64_t m_nPredictedMaskUploadsSaved = 0u;
	godot::RenderingDevice *m_pRenderingDevice;
	const Terrain3DVTResourceImage *m_pResources;
	const t3dVTDatabaseView_t *m_pDatabaseView;
	t3dWorldLimits_t m_Limits;
	t3dVTGpuResourcePlan_t m_Plan;
	Terrain3DMemoryReservation m_stagingMemory;
	godot::PackedByteArray m_AllocationUpload;
	godot::PackedByteArray m_DatabaseUpload;
	godot::PackedByteArray m_PageEntryUpload;
	godot::PackedInt32Array m_RetiredPageScratch;
	godot::PackedInt32Array m_RetiredSlotScratch;
	godot::PackedInt32Array m_UploadBatchOrder;
	godot::PackedByteArray m_UploadBatchReservations;
	godot::PackedByteArray m_PhysicalUpload[T3D_VT_CHANNEL_COUNT];
	godot::PackedByteArray m_NeutralPhysicalUpload[T3D_VT_CHANNEL_COUNT];
	godot::PackedByteArray m_NeutralTailUpload[T3D_VT_CHANNEL_COUNT];
	t3dVTGpuStats_t m_Stats;
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	// Fixed CPU queue and reverse adjacency, allocated before hot sealing.
	godot::PackedInt32Array m_HintFirstChild;
	godot::PackedInt32Array m_HintNextSibling;
	godot::PackedInt32Array m_HintAllocationIndex;
	godot::PackedInt32Array m_HintQueue;
	godot::PackedInt32Array m_HintWorkIndices;
	godot::PackedByteArray m_HintQueued;
	godot::PackedByteArray m_HintCpuRecords;
	godot::PackedByteArray m_HintUploadScratch;
	uint32_t m_nHintPageCount = 0u;
	uint32_t m_nHintQueueHead = 0u;
	uint32_t m_nHintQueueTail = 0u;
	uint32_t m_nHintQueueCount = 0u;
	bool m_bHintGraphReady = false;
	bool m_bHintHeaderEnabled = false;
	bool m_bHintHeaderInitialized = false;
	uint32_t m_nPublishedHintGeneration = 0u;
#endif
	uint64_t m_nHotCreateEpoch;
	uint64_t m_nHotFreeEpoch;
	uint32_t m_nUploadBatchCount;
	uint32_t m_nDatabaseUploadStage;
	uint32_t m_nDatabaseTailChannel;
	bool m_bUploadBatchActive;
	bool m_bDatabaseUploadBegun;
	bool m_bCpuPrepared;
	bool m_bStorageReady;
	bool m_bExecutableReady;
	bool m_bInitialized;
	bool m_bDatabaseUploadedWarm;
	bool m_bHotSealed;
};

#endif // TERRAIN3D_VT_GPU_H
