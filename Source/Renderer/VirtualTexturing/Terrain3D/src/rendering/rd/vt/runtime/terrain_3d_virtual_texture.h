// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VIRTUAL_TEXTURE_H
#define TERRAIN3D_VIRTUAL_TEXTURE_H

#include "Terrain3DTypes.h"
#include "Terrain3DConstants.h"

#include "core/config/terrain_3d_startup_state.h"
#include "memory/arena/terrain_3d_memory_zone.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "rendering/rd/vt/gpu/terrain_3d_vt_gpu.h"
#include "rendering/rd/vt/io/terrain_3d_vt_pack_runtime.h"
#include "rendering/rd/vt/runtime/terrain_3d_vt_feedback_reducer.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi_shadow.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi_parity.h"
#include "rendering/rd/vt/streaming/terrain_3d_vt_spatial_demand.h"
#include "rendering/rd/vt/streaming/terrain_3d_vt_demand_bundle_store.h"

#include <godot_cpp/variant/dictionary.hpp>

struct t3dRdForwardPassContext_s;

typedef struct t3dVTStagedRequest_s
{
	t3dVTRuntimeRequest_t Request;
	uint16_t nProducer;
	uint16_t nReserved;
} t3dVTStagedRequest_t;

typedef struct t3dVTMaterialReadSnapshot_s
{
	uint32_t nDatabaseKind;
	uint32_t nDatabaseGeneration;
	uint32_t nPackageGeneration;
	uint32_t nMaterialCount;
	uint64_t nPackageContentSignature;
	uint64_t nPackagePublicationGeneration;
	uint32_t nMaterialAllocation[T3D_RD_MATERIAL_LAYER_COUNT];
} t3dVTMaterialReadSnapshot_t;

class Terrain3DVirtualTexture
{
public:
	Terrain3DVirtualTexture();
	~Terrain3DVirtualTexture();

	bool bindRuntimeAllocator( Terrain3DMemoryZone *pZone );
	bool configure_streaming_limits_cold( const t3dVTStreamingLimits_t& pLimits );
	void set_frame_budgets_hot( uint32_t nRequestBudget, uint32_t nAdmissionBudget );
	const t3dVTStreamingLimits_t& get_streaming_limits() const;
	bool configure_residency_policy_cold( const t3dVTResidencyPolicy_t& pPolicy );
	// ST02: opt out of transitional FAR material demand only after the
	// composed authoring/parity gate is approved by the integration owner.
	bool configure_unified_regional_demand_cold( bool bComposedOnly );
	const t3dVTResidencyPolicy_t& get_residency_policy() const;
	bool prepare_cpu_cold( const t3dWorldLimits_t& pLimits, const t3dVTGpuResourcePlan_t& pPlan, uint32_t nStartupGeneration );
	bool prepare_gpu_storage_cold( Terrain3DVTResourceImage *pResources );
	bool bind_gpu_executable_cold();
	bool service_gpu_executable_cold( uint32_t nMaxWorkUnits );
	bool publish_initial_cold();
	bool service_initial_publication_cold( uint64_t nMaxBytes, uint32_t nMaxWorkUnits );
	void shutdown_cold( bool bPreserveTailStaging = false );
	void seal_hot_runtime();

	bool is_initialized() const;
	bool is_cpu_ready() const;
	bool is_gpu_storage_ready() const;
	bool is_gpu_ready() const;
	bool is_initial_published() const;
	bool is_hot_sealed() const;
	bool is_readback_enabled() const;
	bool validate_hot_plateau_reference_debug();
	uint64_t get_cpu_image_bytes() const;
	uint64_t get_gpu_reserved_bytes() const;
	uint32_t get_page_table_generation() const;
	uint32_t getMaterialVTAllocationId( uint32_t nMaterialId ) const;
	bool loadPackageCold( const void *pPackageBytes, uint64_t nPackageBytes );
	bool loadPackageFileCold( const godot::String& rPath );
	bool stagePackageWarm( const void *pPackageBytes, uint64_t nPackageBytes );
	bool stagePackageFileWarm( const godot::String& rPath );
	// Quiescing may span calls. Once GPU mutation starts, all upload slices and
	// any rollback complete in this render-thread call; consumers see one commit.
	bool servicePackageRepublishWarm( uint64_t nMaxBytes, uint32_t nMaxWorkUnits );
	void abortPackageRepublishWarm();
	bool isPackageRepublishPendingWarm() const
	{
		return m_bWarmPackagePending;
	}
	bool isPackageResourceRebuildRequiredWarm() const
	{
		return m_bWarmResourceRebuildRequired;
	}
	bool isPackageRollbackReadyWarm() const
	{
		return m_bWarmRollbackReady;
	}
	uint64_t getPackagePublicationGeneration() const
	{
		return m_nPackagePublicationGeneration.load( T3D_MEMORY_ORDER_ACQUIRE );
	}
	bool clearPackageCold( void );
	void set_startup_generation( uint32_t nGeneration );
	void cancel_startup_cold( uint32_t nGeneration );
	EA_FORCE_INLINE t3dStartupDomainSnapshot_t get_startup_snapshot() const
	{
		return t3dStartupDomainSnapshot( m_startupState );
	}
	void mark_startup_failed( uint32_t nGeneration );
	godot::Dictionary get_startup_diagnostics() const;
	bool findMaterialAllocationCold( uint64_t nStableKey, uint32_t& rAllocationId ) const;
	bool snapshotMaterialReadStateCold( t3dVTMaterialReadSnapshot_t& rSnapshot ) const;
	bool snapshotTerrainMegaVTReadStateCold( uint32_t *pAllocationIds, uint32_t *pCookedGenerations, uint8_t *pCookedValid,
	                                         uint32_t nCount ) const;
	// Called on the render lane when publishing a bounded diagnostics sample.
	EA_FORCE_INLINE bool hasFileBackedPackageOnRenderThread() const
	{
		return m_bRuntimePackLoaded && m_RuntimePack.pFileSource != NULL;
	}
	EA_FORCE_INLINE uint64_t getPackageMetadataBytesOnRenderThread() const
	{
		return m_bRuntimePackLoaded ? m_RuntimePack.nOwnedBytes : 0u;
	}
	EA_FORCE_INLINE uint64_t getPackageFileBytesOnRenderThread() const
	{
		return m_bRuntimePackLoaded ? m_RuntimePack.nPackageBytes : 0u;
	}
	EA_FORCE_INLINE uint64_t getPredictionIncompleteFallbacksOnRenderThread() const
	{
		return m_nSpatialDemandIncompleteFallbacks;
	}
	EA_FORCE_INLINE bool isPredictionCurrentCellCompleteOnRenderThread() const
	{
		return m_bSpatialDemandCurrentCellComplete;
	}
	uint32_t getActiveAllocationCount() const;
	uint32_t getActiveVirtualPageCount() const;
	uint32_t getLoadedPackageAllocationCountCold() const;
	uint32_t getLoadedPackageVirtualPageCountCold() const;
	uint32_t getLoadedPackageTailAtlasHeightCold() const;
	uint32_t getDatabaseGeneration() const;
	uint32_t getPackageGeneration() const;
	uint64_t getPackageContentSignature() const;
	uint32_t getDatabaseKind() const;
	bool supportsSparseV1() const;
	uint32_t getTerrainMegaVTAllocationBase() const;
	uint32_t getTerrainMegaVTAllocationId( uint32_t nRegionMapIndex ) const;
	bool getComposedRegionAllocationCold( uint32_t nRegionMapIndex, uint32_t& rAllocation, uint32_t& rGeneration ) const;
	bool getTerrainMegaVTCookedGenerationCold( uint32_t nRegionMapIndex, uint32_t& rGeneration ) const;
	const t3dVTDatabaseView_t *get_database() const;
	const Terrain3DVTResourceImage *get_resource_image() const;
	bool set_quality_profile( uint32_t nProfile );
	uint32_t get_requested_quality_profile() const;
	uint32_t get_package_fully_supported_profile_mask() const;
	uint32_t get_quality_fallback_allocation_count() const;
	uint32_t get_quality_generation() const;
	uint32_t get_allocations_by_highest_profile( uint32_t nProfile ) const;
	uint32_t get_allocations_by_resolved_profile( uint32_t nProfile ) const;
	uint32_t getPhysicalFormatFamilyCountV1() const;
	bool getPhysicalFormatFamilyBindingV1( uint32_t nFamily, godot::RID& rRid, uint32_t& rLayerCount ) const;
	t3dVTResidencyState_t& get_residency();
	const t3dVTResidencyState_t& get_residency() const;
	t3dVTStreamingState_t& get_streaming();
	const t3dVTStreamingState_t& get_streaming() const;
	Terrain3DVTGpu& get_gpu();
	const Terrain3DVTGpu& get_gpu() const;
	const t3dVirtualTextureStats_t& get_stats() const;
	t3dVTFinalTelemetry_t get_final_telemetry_reference_debug() const;
	void service_fragment_feedback_hot( uint32_t nFrameId );
	void service_streaming_hot( uint32_t nFrameId );
	bool shouldRefreshMaterialDemandHot( uint32_t nLane, const t3dRdForwardPassContext_s& rContext, uint64_t nSourceSignature );
	uint32_t stage_material_footprint_hot( uint32_t nAllocationId, uint32_t nFrameId, const float pUvMin[2], const float pUvMax[2],
	                                       uint16_t nWantedMip, uint32_t nMaxRequests, uint16_t nPriority, t3dVTRequestProducer_t eProducer,
	                                       uint32_t nFootprintWindow = UINT32_MAX, bool *pFullyStaged = NULL );
	// Cold-only source staging; empty bytes disable prediction for missing/unsaved sidecars.
	bool stageDemandIndexCold( const uint8_t *pBytes, uint64_t nBytes, const godot::String& rSourceIdentity );
	// T04: v1 is an optional sibling of the canonical v2 presence sidecar.
	bool stageDemandV1Cold( const godot::String& rPath, const godot::String& rSourceIdentity );
	bool stagedDemandMatchesLoadedPackageCold() const;
	bool load_spatial_demand_index_cold( const uint8_t *pBytes, uint64_t nBytes );
	uint32_t stage_spatial_demand_hot( float fCameraX, float fCameraZ, float fForwardX, float fForwardZ, uint32_t nFrameId,
	                                   float fFovDegrees = 90.0f, uint32_t nViewportHeight = 256u );
	bool shouldScanTerrainAllocationHot( float fCameraX, float fCameraZ, uint32_t nAllocation, uint32_t nFrameId );
	bool configure_feedback_policy_cold( const t3dVTFeedbackPolicy_t& rPolicy, uint32_t nPhysicalCapDivisor );
	bool service_feedback_grid_hot( uint32_t nFrameId, uint64_t nViewportKey, uint32_t nDeviceGeneration );
	// P31e: consume V1 readbacks for diagnostics only, after V2 staging.
	void service_multi_feedback_shadow_hot( uint32_t nFrameId, uint64_t nViewportKey, uint32_t nDeviceGeneration );
	void update_feedback_view_hot( float fCameraX, float fCameraZ, float fForwardX, float fForwardZ, uint32_t nFrameId );
	uint64_t get_feedback_resident_rescues() const
	{
		return m_nFeedbackResidentRescues;
	}
	uint64_t get_feedback_staged_requests() const
	{
		return m_nFeedbackStagedRequests;
	}
	uint64_t get_feedback_rejected_noise() const
	{
		return m_nFeedbackRejectedNoise;
	}
	uint64_t get_feedback_parent_promotions() const
	{
		return m_nFeedbackParentPromotions;
	}
	uint64_t get_feedback_invalid_parents() const
	{
		return m_nFeedbackInvalidParents;
	}
	uint64_t get_feedback_grid_direct_unique_pages() const
	{
		return m_nFeedbackGridDirectUniquePages;
	}
	uint64_t get_feedback_grid_selected_candidates() const
	{
		return m_nFeedbackGridSelectedCandidates;
	}
	const t3dVTFeedbackMultiShadowStats_t& get_multi_feedback_shadow_stats_hot() const
	{
		return m_multiFeedbackShadowStats;
	}
	bool flush_staged_material_requests_hot( uint32_t nFrameId, uint32_t& rTerrainAccepted, uint32_t& rInstanceAccepted );

	bool claim_stream_job_owner_hot( uint32_t& rJobIndex );
	bool cancel_stream_job_owner_hot( uint32_t pJobIndex );
	bool execute_stream_job_async( uint32_t pJobIndex, t3dVTStreamReadDecodeFn_t pReadDecode, void *pUser );
	uint64_t service_uploads_hot( uint32_t pFrameId );
	bool service_retired_slots_hot( uint32_t pFrameId );

private:
	void rebuildFarRegionAllocationTableCold();
	uint32_t stage_region_surface_demand_hot( float fCameraX, float fCameraZ, float fForwardX, float fForwardZ, uint32_t nFrameId,
	                                          float fFovDegrees, uint32_t nViewportHeight );
	uint32_t stage_v1_demand_hot( float fCameraX, float fCameraZ, float fForwardX, float fForwardZ, uint32_t nFrameId, float fFovDegrees,
	                              uint32_t nViewportHeight );
	bool stage_feedback_page_hot( uint32_t nPage, uint16_t nMip, uint16_t nPriority, uint32_t nFrameId );
	void streamWorkerMain( void );
	void stopStreamWorkerCold( void );
	bool dispatchStreamJobHot( uint32_t nJob );
	bool open_package_candidate( const void *pBytes, uint64_t nBytes, t3dVTPackRuntimeStorage_t& rCandidate );
	bool validate_package_limits( const t3dVTPackRuntimeStorage_t& rCandidate ) const;
	bool publish_package_state_warm( const t3dVTDatabaseView_t& rDatabase, bool bNeutral, uint64_t nMaxBytes, uint32_t nMaxWorkUnits,
	                                 bool *pMutationStarted = NULL );
	bool allocate_cpu_image_cold();
	void release_cpu_image_cold();
	bool prepare_neutral_legacy_cpu_cold();
	bool initialize_runtime_state_cold( const t3dVTDatabaseView_t& rDatabase, uint32_t nDatabaseGeneration );
	bool prepare_runtime_package_cpu_cold();
	bool publish_neutral_legacy_warm();
	bool publish_runtime_package_warm();
	void release_runtime_pack_cold();
	uint32_t count_physical_banks() const;
	bool apply_quality_profile_state_v1( t3dVTAllocation_t *pAllocations, uint32_t nAllocationCount );

	Terrain3DVTResourceImage *m_pResources;
	Terrain3DMemoryZone *m_pRuntimeAllocatorZone;
	Terrain3DVTGpu m_Gpu;
	Terrain3DVTSpatialDemand m_spatialDemand;
	Terrain3DVTDemandBundleStore m_v1Demand;
	godot::String m_v1DemandPath;
	uint64_t m_nV1AssetIdentityHash = 0u;
	std::vector<uint64_t> m_v1MaterialKeys;
	std::vector<uint32_t> m_v1MaterialAllocations;
	uint32_t m_nV1LastStageFrame = UINT32_MAX;
	uint64_t m_nV1HistogramRecords = 0u;
	uint64_t m_nV1PrefetchRequests = 0u;
	// Rebuilt only on cold package publication. Fixed-size hot registry; no
	// package mutex, stable-key lookups, or allocations during frame demand.
	uint32_t m_szFarRegionAllocations[T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY]{};
	uint32_t m_szComposedRegionAllocations[T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY]{};
	uint8_t m_szFarLastMip[T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY * 3u * T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION]{};
	uint32_t m_szFarLastMipFrame[T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY * 3u * T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION]{};
	uint32_t m_nFarRegionLastStageFrame = UINT32_MAX;
	uint64_t m_nFarRegionRequests = 0u;
	bool m_bComposedOnlyRegionalDemand = false;
	uint32_t m_nRegionalCameraFrame = UINT32_MAX;
	float m_fRegionalCameraX = 0.0f;
	float m_fRegionalCameraZ = 0.0f;

	godot::String m_demandIndexSourceIdentity;
	uint32_t m_nSpatialDemandLastStageFrame = UINT32_MAX;
	int32_t m_nSpatialDemandLastCellX = INT32_MIN;
	int32_t m_nSpatialDemandLastCellZ = INT32_MIN;
	uint8_t m_nSpatialDemandLastOctant = 0u;
	bool m_bSpatialDemandCurrentCellComplete = false;
	bool m_bSpatialDemandHeapDisplaced = false;
	uint32_t m_nSpatialDemandCoveredAllocationCount = 0u;
	uint32_t m_szSpatialDemandCoveredAllocations[T3D_RD_MATERIAL_LAYER_COUNT]{};
	uint8_t m_szSpatialDemandAllocationComplete[T3D_RD_MATERIAL_LAYER_COUNT]{};
	uint64_t m_nSpatialDemandIncompleteFallbacks = 0u;
	uint64_t m_nSpatialDemandAttempts = 0u;
	uint32_t m_nStreamingServiceFrame = UINT32_MAX;
	uint64_t m_szDemandSignature[3]{}; // Terrain, visible instances, world candidates.
	uint32_t m_szDemandRefreshFrame[3]{ UINT32_MAX, UINT32_MAX, UINT32_MAX };
	Terrain3DThread m_streamWorker;
	Terrain3DMutex m_streamWorkerMutex;
	Terrain3DCondition m_streamWorkerCondition;
	uint32_t m_szStreamWorkerJobs[T3D_VT_MAX_STREAM_SCRATCH_LANES]{};
	uint32_t m_nStreamWorkerHead = 0u;
	uint32_t m_nStreamWorkerCount = 0u;
	bool m_bStreamWorkerStop = false;
	t3dWorldLimits_t m_Limits;
	t3dVTResidencyState_t m_Residency;
	t3dVTStreamingState_t m_Streaming;
	t3dVTStreamingLimits_t m_StreamingLimits;
	t3dVTResidencyPolicy_t m_ResidencyPolicy;
	t3dVTDatabaseView_t m_NeutralDatabase;
	t3dVTPackRuntimeStorage_t m_RuntimePack;
	mutable Terrain3DMutex m_packageMutex;
	t3dVTPackRuntimeStorage_t m_WarmCandidate{};
	t3dAtomicU64_t m_nPackagePublicationGeneration{ 0u };
	uint64_t m_nWarmPublicationAttemptCount = 0u;
	uint64_t m_nWarmPublicationSuccessCount = 0u;
	uint64_t m_nWarmPublicationFailureCount = 0u;
	uint64_t m_nIdenticalRepublishSkips = 0u;
	bool m_bIdenticalPackageReuse = false;
	uint64_t m_nWarmPublicationUsecLast = 0u;
	bool m_bWarmPackagePending = false;
	bool m_bWarmCandidateNeutral = false;
	bool m_bWarmResourceRebuildRequired = false;
	bool m_bWarmRollbackReady = true;
	bool m_bInitialPublicationStarted = false;
	t3dVTFileHeader_t m_NeutralHeader;
	t3dVTAllocationRecord_t m_NeutralAllocations[T3D_VT_CHANNEL_COUNT];
	t3dVTPageFileRecord_t m_NeutralPages[T3D_VT_CHANNEL_COUNT];
	t3dVTPayloadRecord_t m_NeutralPayloads[T3D_VT_CHANNEL_COUNT];
	uint8_t m_NeutralPayloadBytes[T3D_VT_CHANNEL_COUNT];
	void *m_pCpuImage;
	uint64_t m_nCpuImageBytes;
	t3dVTAllocation_t *m_pAllocations;
	t3dVTTailMetadata_t *m_pTailMetadata;
	t3dVTPageTableEntry_t *m_pPageTable;
	t3dVTPhysicalSlot_t *m_pPhysicalPageSets;
	t3dVTStreamJob_t *m_pStreamJobs;
	uint16_t *m_pStreamPageToJob;
	uint16_t *m_pStreamFreeJobs;
	uint16_t *m_pStreamQueuedJobs;
	uint16_t *m_pStreamJobQueuePosition;
	uint16_t *m_pStreamReadyJobs;
	uint16_t *m_pStreamMergedWantedMip;
	uint16_t *m_pStreamMergedPriority;
	uint32_t *m_pFreePhysicalSlots;
	t3dVTRetiredPageSet_t *m_pRetiredPageSets;
	uint32_t *m_pLastEvictedFrame;
	t3dVTStagedRequest_t *m_pStagedRequests;
	uint32_t *m_pStagedByPage;
	Terrain3DVTFeedbackReducer m_feedbackReducer;
	t3dVTFeedbackMultiShadowStats_t m_multiFeedbackShadowStats{};
	Terrain3DVTFeedbackMultiParityRing m_multiFeedbackParity;
	bool m_bMultiFeedbackParityRequested = false;
	uint32_t *m_pFeedbackEpochs = NULL;
	uint32_t *m_pFeedbackDirect = NULL;
	uint32_t *m_pFeedbackAggregate = NULL;
	uint32_t *m_pFeedbackTouched = NULL;
	t3dVTFeedbackCandidate_t *m_pFeedbackCandidates = NULL;
	t3dVTFeedbackPolicy_t m_feedbackPolicy;
	uint32_t m_nFeedbackPhysicalCapDivisor = 3u; // Zero disables the optional cache-fraction cap.
	uint64_t m_nFeedbackParentPromotions = 0u;
	uint64_t m_nFeedbackInvalidParents = 0u;
	uint64_t m_nFeedbackResidentRescues = 0u;
	uint64_t m_nFeedbackStagedRequests = 0u;
	uint64_t m_nFeedbackRejectedNoise = 0u;
	uint64_t m_nFeedbackGridDirectUniquePages = 0u;
	uint64_t m_nFeedbackGridSelectedCandidates = 0u;
	uint32_t m_nFeedbackViewFrame = UINT32_MAX;
	float m_fFeedbackX = 0.0f, m_fFeedbackZ = 0.0f;
	float m_fFeedbackForwardX = 0.0f, m_fFeedbackForwardZ = 0.0f;
	uint32_t m_nStagedRequestCount;
	uint32_t m_nStagedRequestFrame;
	uint32_t m_nFrameRequestBudget = 1u;
	uint32_t m_nFragmentRequestFrame = UINT32_MAX;
	uint32_t m_nFragmentRequestsUsed = 0u;
	uint8_t *m_pStreamScratch;
	uint32_t *m_pScratchLaneOwner;
	Terrain3DAtomic<uint32_t> m_ScratchLaneCompletion[T3D_VT_MAX_STREAM_SCRATCH_LANES];
	t3dVirtualTextureStats_t m_Stats;
	t3dVTQualityRuntimeStats_t m_QualityStats;
	uint32_t m_nRequestedQualityProfile;
	uint32_t m_nQualityGeneration;
	uint64_t m_nGpuHelperCreateEpoch;
	uint64_t m_nTelemetryStartUsec;
	uint64_t m_nTelemetryLastUploadedPageSets;
	uint64_t m_nTelemetryLastEvictions;
	uint64_t m_nTelemetryUploadPageSetsFrame;
	uint64_t m_nTelemetryUploadBytesFrame;
	uint64_t m_nTelemetryEvictionsFrame;
	t3dAtomicU32_t m_nStartupGeneration{ 0u };
	t3dStartupDomainState_t m_startupState;
	uint64_t m_nStartupPackagePrepareCount;
	uint64_t m_nStartupPackagePrepareUsecLast;
	uint64_t m_nStartupCpuImagePrepareCount;
	uint64_t m_nStartupCpuImagePrepareUsecLast;
	uint64_t m_nStartupGpuInitializeCount;
	uint64_t m_nStartupGpuInitializeUsecLast;
	uint64_t m_nStartupGpuStoragePrepareCount;
	uint64_t m_nStartupGpuStoragePrepareUsecLast;
	uint64_t m_nStartupGpuBindCount;
	uint64_t m_nStartupGpuBindUsecLast;
	uint64_t m_nStartupInitialPublicationCount;
	uint64_t m_nStartupInitialPublicationUsecLast;
	uint64_t m_nResolverDepth0Samples;
	uint64_t m_nResolverDepth1Samples;
	uint64_t m_nResolverDepth2To3Samples;
	uint64_t m_nResolverDepth4PlusSamples;
	uint64_t m_nResolverTailSamples;
	uint32_t m_nResolverTelemetrySampleFrame;
	uint32_t m_nResolverTelemetrySamplesThisFrame;
	uint32_t m_nUploadCadenceStableFrames;
	uint64_t m_nUploadCadenceLastPageMisses;
	uint64_t m_nUploadCadenceLastEvictions;
	uint64_t m_nUploadCadenceLastThrash;
	bool m_bStreamingLimitsConfigured;
	bool m_bResidencyPolicyConfigured;
	bool m_bRuntimePackLoaded;
	bool m_bCpuPrepared;
	bool m_bGpuStoragePrepared;
	bool m_bExecutableBound;
	bool m_bInitialPublished;
	bool m_bInitialized;
	bool m_bHotSealed;
};

#endif // TERRAIN3D_VIRTUAL_TEXTURE_H
