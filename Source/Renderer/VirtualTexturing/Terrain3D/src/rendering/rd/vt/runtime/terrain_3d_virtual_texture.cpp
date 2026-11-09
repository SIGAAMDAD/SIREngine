#include "diagnostics/core/terrain_3d_streaming_profiler.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "rendering/rd/vt/runtime/terrain_3d_virtual_texture.h"
#include "core/utility/terrain_3d_checked_math.h"
#include "core/utility/terrain_3d_generation.h"
#include "core/utility/terrain_3d_hash_bytes.h"
#include "rendering/rd/core/terrain_3d_rd_types.h"
#include "rendering/rd/vt/io/terrain_3d_vt_file_source.h"
#include "rendering/rd/vt/runtime/terrain_3d_vt_feedback_multi_reader.h"
#include "data/formats/terrain_3d_vt_pack_format.h"

#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static uint64_t t3dVtTicksUsec( void )
{
	godot::Time *const pTime = godot::Time::get_singleton();
	return pTime ? pTime->get_ticks_usec() : 0u;
}

static void *t3dVtRuntimePackAllocate( void *const pUser, const size_t nBytes, const size_t nAlignment )
{
	Terrain3DMemoryZone *const pZone = ( Terrain3DMemoryZone * )pUser;
	return pZone != NULL ? pZone->allocate( nBytes, nAlignment ) : NULL;
}

static void t3dVtRuntimePackFree( void *const pUser, void *const pMemory )
{
	Terrain3DMemoryZone *const pZone = ( Terrain3DMemoryZone * )pUser;
	if ( pZone != NULL && pMemory != NULL ) {
		pZone->deallocate( pMemory );
	}
}

static t3dVTPackRuntimeAllocator_t t3dVtRuntimePackAllocator( Terrain3DMemoryZone *const pZone )
{
	t3dVTPackRuntimeAllocator_t Allocator;
	Allocator.pUser = pZone;
	Allocator.pAllocate = t3dVtRuntimePackAllocate;
	Allocator.pFree = t3dVtRuntimePackFree;
	return Allocator;
}

struct t3dVtWarmPackAllocator_t
{
	Terrain3DMemoryZone *m_pZone;
	bool m_bDenied = false;
};

static void *t3dVtWarmPackAllocate( void *const pUser, const size_t nBytes, const size_t nAlignment )
{
	auto& allocator = *( t3dVtWarmPackAllocator_t * )pUser;
	void *const memory = allocator.m_pZone->allocate( nBytes, nAlignment );
	allocator.m_bDenied |= memory == NULL;
	return memory;
}

static void t3dVtWarmPackFree( void *const pUser, void *const pMemory )
{
	( ( t3dVtWarmPackAllocator_t * )pUser )->m_pZone->deallocate( pMemory );
}

Terrain3DVirtualTexture::Terrain3DVirtualTexture()
{
	m_pResources = NULL;
	m_pRuntimeAllocatorZone = NULL;
	memset( &m_Limits, 0, sizeof( m_Limits ) );
	memset( &m_Residency, 0, sizeof( m_Residency ) );
	memset( &m_Streaming, 0, sizeof( m_Streaming ) );
	memset( &m_StreamingLimits, 0, sizeof( m_StreamingLimits ) );
	t3d_vt_default_residency_policy( m_ResidencyPolicy );
	m_NeutralDatabase = t3dVTDatabaseView_t{};
	t3d_vt_pack_runtime_clear( m_RuntimePack );
	memset( &m_NeutralHeader, 0, sizeof( m_NeutralHeader ) );
	memset( m_NeutralAllocations, 0, sizeof( m_NeutralAllocations ) );
	memset( m_NeutralPages, 0, sizeof( m_NeutralPages ) );
	memset( m_NeutralPayloads, 0, sizeof( m_NeutralPayloads ) );
	memset( m_NeutralPayloadBytes, 0, sizeof( m_NeutralPayloadBytes ) );
	m_pCpuImage = NULL;
	m_nCpuImageBytes = 0u;
	m_pAllocations = NULL;
	m_pTailMetadata = NULL;
	m_pPageTable = NULL;
	m_pPhysicalPageSets = NULL;
	m_pStreamJobs = NULL;
	m_pStreamPageToJob = NULL;
	m_pStreamFreeJobs = NULL;
	m_pStreamQueuedJobs = NULL;
	m_pStreamJobQueuePosition = NULL;
	m_pStreamReadyJobs = NULL;
	m_pStreamMergedWantedMip = NULL;
	m_pStreamMergedPriority = NULL;
	m_pFreePhysicalSlots = NULL;
	m_pRetiredPageSets = NULL;
	m_pLastEvictedFrame = NULL;
	m_pStagedRequests = NULL;
	m_pStagedByPage = NULL;
	m_nStagedRequestCount = 0u;
	m_nStagedRequestFrame = UINT32_MAX;
	m_pStreamScratch = NULL;
	m_pScratchLaneOwner = NULL;
	for ( uint32_t i = 0u; i < T3D_VT_MAX_STREAM_SCRATCH_LANES; ++i ) {
		m_ScratchLaneCompletion[i].store( T3D_VT_STREAM_COMPLETION_IDLE, T3D_MEMORY_ORDER_RELAXED );
	}
	memset( &m_Stats, 0, sizeof( m_Stats ) );
	memset( &m_QualityStats, 0, sizeof( m_QualityStats ) );
	m_nRequestedQualityProfile = 2u; // Normal. Runtime selection remains data-only.
	// Opt-in, cold-owned reuse of an unchanged package's resident page sets.
	const char *const pIdenticalReuse = getenv( "TERRAIN3D_VT_IDENTICAL_PACKAGE_REUSE" );
	m_bIdenticalPackageReuse = pIdenticalReuse && pIdenticalReuse[0] == '1' && !pIdenticalReuse[1];
	m_nQualityGeneration = 0u;
	m_nGpuHelperCreateEpoch = 0u;
	m_nTelemetryStartUsec = 0u;
	m_nTelemetryLastUploadedPageSets = 0u;
	m_nTelemetryLastEvictions = 0u;
	m_nTelemetryUploadPageSetsFrame = 0u;
	m_nTelemetryUploadBytesFrame = 0u;
	m_nTelemetryEvictionsFrame = 0u;
	m_nStartupGeneration = 0u;
	m_nStartupPackagePrepareCount = 0u;
	m_nStartupPackagePrepareUsecLast = 0u;
	m_nStartupCpuImagePrepareCount = 0u;
	m_nStartupCpuImagePrepareUsecLast = 0u;
	m_nStartupGpuInitializeCount = 0u;
	m_nStartupGpuInitializeUsecLast = 0u;
	m_nStartupGpuStoragePrepareCount = 0u;
	m_nStartupGpuStoragePrepareUsecLast = 0u;
	m_nStartupGpuBindCount = 0u;
	m_nStartupGpuBindUsecLast = 0u;
	m_nStartupInitialPublicationCount = 0u;
	m_nStartupInitialPublicationUsecLast = 0u;
	m_nResolverDepth0Samples = 0u;
	m_nResolverDepth1Samples = 0u;
	m_nResolverDepth2To3Samples = 0u;
	m_nResolverDepth4PlusSamples = 0u;
	m_nResolverTailSamples = 0u;
	m_nResolverTelemetrySampleFrame = UINT32_MAX;
	m_nResolverTelemetrySamplesThisFrame = 0u;
	m_nUploadCadenceStableFrames = 0u;
	m_nUploadCadenceLastPageMisses = 0u;
	m_nUploadCadenceLastEvictions = 0u;
	m_nUploadCadenceLastThrash = 0u;
	m_bStreamingLimitsConfigured = false;
	m_bResidencyPolicyConfigured = false;
	m_bRuntimePackLoaded = false;
	m_bCpuPrepared = false;
	m_bGpuStoragePrepared = false;
	m_bExecutableBound = false;
	m_bInitialPublished = false;
	m_bInitialized = false;
	m_bHotSealed = false;
}

Terrain3DVirtualTexture::~Terrain3DVirtualTexture()
{
	shutdown_cold();
}

bool Terrain3DVirtualTexture::bindRuntimeAllocator( Terrain3DMemoryZone *const pZone )
{
	if ( pZone == NULL || m_bInitialized || ( m_pRuntimeAllocatorZone != NULL && m_pRuntimeAllocatorZone != pZone ) ) {
		return m_pRuntimeAllocatorZone == pZone && !m_bInitialized;
	}
	m_pRuntimeAllocatorZone = pZone;
	return true;
}

bool Terrain3DVirtualTexture::configure_streaming_limits_cold( const t3dVTStreamingLimits_t& pLimits )
{
	if ( m_bInitialized || pLimits.nMaxUploadBytesPerFrame == 0u || pLimits.nMaxPageSetsPerFrame == 0u ||
	     pLimits.nMaxInFlightIoJobs == 0u || pLimits.nMaxAdmittedRequestsPerFrame == 0u ) {
		return false;
	}
	m_StreamingLimits = pLimits;
	m_bStreamingLimitsConfigured = true;
	return true;
}

void Terrain3DVirtualTexture::set_frame_budgets_hot( const uint32_t nRequestBudget, const uint32_t nAdmissionBudget )
{
	m_nFrameRequestBudget = CLAMP( nRequestBudget, 1u, MAX( 1u, m_Limits.nMaxVtRequests ) );
	m_Streaming.Limits.nMaxAdmittedRequestsPerFrame = CLAMP( nAdmissionBudget, 1u, MAX( 1u, m_Limits.nMaxStreamJobs ) );
}

const t3dVTStreamingLimits_t& Terrain3DVirtualTexture::get_streaming_limits() const
{
	return m_StreamingLimits;
}

bool Terrain3DVirtualTexture::configure_residency_policy_cold( const t3dVTResidencyPolicy_t& pPolicy )
{
	if ( m_bInitialized || !t3d_vt_validate_residency_policy( pPolicy ) ) {
		return false;
	}
	m_ResidencyPolicy = pPolicy;
	m_bResidencyPolicyConfigured = true;
	return true;
}

bool Terrain3DVirtualTexture::configure_unified_regional_demand_cold( const bool bComposedOnly )
{
	if ( m_bInitialized || m_bHotSealed || m_bWarmPackagePending ) {
		return false;
	}
	m_bComposedOnlyRegionalDemand = bComposedOnly;
	return true;
}

const t3dVTResidencyPolicy_t& Terrain3DVirtualTexture::get_residency_policy() const
{
	return m_ResidencyPolicy;
}

void Terrain3DVirtualTexture::set_startup_generation( const uint32_t nGeneration )
{
	if ( m_nStartupGeneration == nGeneration ) {
		return;
	}
	m_nStartupGeneration = nGeneration;
	t3dStartupDomainRequest( m_startupState, nGeneration, 0u, 4u );
	godot::UtilityFunctions::print( "[Terrain3D][Startup][VT] REQUESTED generation=", nGeneration );
	m_nStartupPackagePrepareCount = 0u;
	m_nStartupPackagePrepareUsecLast = 0u;
	m_nStartupCpuImagePrepareCount = 0u;
	m_nStartupCpuImagePrepareUsecLast = 0u;
	m_nStartupGpuInitializeCount = 0u;
	m_nStartupGpuInitializeUsecLast = 0u;
	m_nStartupGpuStoragePrepareCount = 0u;
	m_nStartupGpuStoragePrepareUsecLast = 0u;
	m_nStartupGpuBindCount = 0u;
	m_nStartupGpuBindUsecLast = 0u;
	m_nStartupInitialPublicationCount = 0u;
	m_nStartupInitialPublicationUsecLast = 0u;
}

void Terrain3DVirtualTexture::mark_startup_failed( const uint32_t nGeneration )
{
	const t3dStartupDomainSnapshot_t startup = t3dStartupDomainSnapshot( m_startupState );
	if ( nGeneration != m_nStartupGeneration || startup.m_nGeneration != nGeneration || startup.m_eStage == T3D_STARTUP_COMMITTED ) {
		return;
	}
	const uint64_t nNowUsec = t3dVtTicksUsec();
	t3dStartupDomainFinish( m_startupState, T3D_STARTUP_FAILED, nNowUsec );
}

void Terrain3DVirtualTexture::cancel_startup_cold( const uint32_t nGeneration )
{
	if ( nGeneration != m_nStartupGeneration || !t3dStartupDomainMatches( m_startupState, nGeneration ) ) {
		return;
	}
	const t3dStartupStage_t eStage = t3dStartupDomainGetStage( m_startupState );
	if ( eStage == T3D_STARTUP_FAILED || eStage == T3D_STARTUP_CANCELLED ) {
		return;
	}
	( void )t3dStartupDomainCancel( m_startupState, nGeneration, t3dVtTicksUsec() );
}

godot::Dictionary Terrain3DVirtualTexture::get_startup_diagnostics() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	godot::Dictionary result;
	result["fragment_requests_accepted"] = ( int64_t )m_Gpu.get_fragment_feedback_consumed();
	result["fragment_requests_dropped"] = ( int64_t )m_Gpu.get_fragment_feedback_dropped();
	result["fragment_misses"] = ( int64_t )m_Gpu.get_fragment_feedback_misses();
	result["fragment_hash_overflow"] = ( int64_t )m_Gpu.get_fragment_feedback_overflow();
	result["spatial_demand_cells"] = ( int64_t )m_spatialDemand.getCellCount();
	result["spatial_demand_version"] = ( int64_t )m_spatialDemand.getVersion();
	result["spatial_demand_mapped_records"] = ( int64_t )m_spatialDemand.getMappedRecordCount();
	result["spatial_demand_primary"] = m_spatialDemand.hasBoundPredictor();
	result["spatial_demand_source"] = m_demandIndexSourceIdentity;
	result["spatial_demand_prefetch_attempts"] = ( int64_t )m_nSpatialDemandAttempts;
	result["spatial_demand_incomplete_fallbacks"] = ( int64_t )m_nSpatialDemandIncompleteFallbacks;
	result["spatial_demand_current_cell_complete"] = m_bSpatialDemandCurrentCellComplete;
	result["fragment_feedback_mask_uploads_saved"] = ( int64_t )m_Gpu.get_predicted_mask_uploads_saved();
	result["spatial_demand_package_match"] =
	    m_Residency.pDatabase != NULL && m_spatialDemand.isValidFor( m_Residency.pDatabase->nPackageContentSignature );
	const t3dStartupDomainSnapshot_t startup = t3dStartupDomainSnapshot( m_startupState );
	result["startup_generation"] = ( int64_t )( m_nStartupGeneration );
	const t3dStartupStage_t eStartupStage = startup.m_eStage;
	result["startup_stage"] = ( int64_t )( eStartupStage );
	result["startup_stage_name"] = godot::String( t3dStartupStageName( eStartupStage ) );
	result["startup_state_generation"] = ( int64_t )( startup.m_nGeneration );
	result["startup_completed_units"] = ( int64_t )( startup.m_nCompletedUnits );
	result["startup_total_units"] = ( int64_t )( startup.m_nTotalUnits );
	result["startup_start_usec"] = ( int64_t )( startup.m_nStartUsec );
	result["startup_end_usec"] = ( int64_t )( startup.m_nEndUsec );
	result["package_prepare_count"] = ( int64_t )( m_nStartupPackagePrepareCount );
	result["package_prepare_usec_last"] = ( int64_t )( m_nStartupPackagePrepareUsecLast );
	result["cpu_image_prepare_count"] = ( int64_t )( m_nStartupCpuImagePrepareCount );
	result["cpu_image_prepare_usec_last"] = ( int64_t )( m_nStartupCpuImagePrepareUsecLast );
	result["gpu_initialize_count"] = ( int64_t )( m_nStartupGpuInitializeCount );
	result["gpu_initialize_usec_last"] = ( int64_t )( m_nStartupGpuInitializeUsecLast );
	result["gpu_storage_prepare_count"] = ( int64_t )( m_nStartupGpuStoragePrepareCount );
	result["gpu_storage_prepare_usec_last"] = ( int64_t )( m_nStartupGpuStoragePrepareUsecLast );
	result["gpu_bind_count"] = ( int64_t )( m_nStartupGpuBindCount );
	result["gpu_bind_usec_last"] = ( int64_t )( m_nStartupGpuBindUsecLast );
	result["cpu_ready"] = m_bCpuPrepared;
	result["gpu_storage_ready"] = m_bGpuStoragePrepared;
	result["gpu_ready"] = m_bExecutableBound;
	result["initial_published"] = m_bInitialPublished;
	result["warm_publication_attempt_count"] = ( int64_t )m_nWarmPublicationAttemptCount;
	result["warm_publication_success_count"] = ( int64_t )m_nWarmPublicationSuccessCount;
	result["warm_publication_failure_count"] = ( int64_t )m_nWarmPublicationFailureCount;
	result["vt_identical_republish_skips"] = ( int64_t )m_nIdenticalRepublishSkips;
	result["warm_publication_usec_last"] = ( int64_t )m_nWarmPublicationUsecLast;
	result["package_publication_generation"] = ( int64_t )m_nPackagePublicationGeneration;
	result["warm_package_pending"] = m_bWarmPackagePending;
	result["warm_resource_rebuild_required"] = m_bWarmResourceRebuildRequired;
	result["warm_rollback_ready"] = m_bWarmRollbackReady;
	result["physical_bank_prepare_count"] = ( int64_t )m_nStartupGpuStoragePrepareCount;
	result["initial_publication_count"] = ( int64_t )( m_nStartupInitialPublicationCount );
	result["initial_publication_usec_last"] = ( int64_t )( m_nStartupInitialPublicationUsecLast );
	result["vt_cpu_ready"] = m_bCpuPrepared;
	result["vt_storage_ready"] = m_bGpuStoragePrepared;
	result["vt_gpu_ready"] = m_bExecutableBound;
	result["vt_cpu_bytes"] = ( int64_t )( m_nCpuImageBytes );
	result["vt_package_file_backed"] = m_bRuntimePackLoaded && m_RuntimePack.pFileSource != NULL;
	result["vt_package_metadata_bytes"] = ( int64_t )( m_bRuntimePackLoaded ? m_RuntimePack.nOwnedBytes : 0u );
	result["vt_package_file_bytes"] = ( int64_t )( m_bRuntimePackLoaded ? m_RuntimePack.nPackageBytes : 0u );
	result["vt_gpu_bytes"] = ( int64_t )( get_gpu_reserved_bytes() );
	const uint64_t nNowUsec = t3dVtTicksUsec();
	result["vt_elapsed_ms"] = ( double )( t3dStartupDomainElapsedUsec( startup, nNowUsec ) ) / 1000.0;
	return result;
}

bool Terrain3DVirtualTexture::loadPackageCold( const void *const pPackageBytes, const uint64_t nPackageBytes )
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	if ( pPackageBytes == NULL || nPackageBytes == 0u || m_pRuntimeAllocatorZone == NULL || m_bInitialized || m_bHotSealed ) {
		return false;
	}
	const uint64_t nPrepareStartedUsec = t3dVtTicksUsec();
	const bool bTrackStartup = m_nStartupGeneration != 0u && t3dStartupDomainGetStage( m_startupState ) != T3D_STARTUP_COMMITTED;
	if ( bTrackStartup ) {
		t3dStartupDomainSetStartUsecIfZero( m_startupState, nPrepareStartedUsec );
		t3dStartupDomainSetStage( m_startupState, T3D_STARTUP_PREPARING );
	}
	++m_nStartupPackagePrepareCount;
	t3dVTPackRuntimeStorage_t Candidate;
	t3d_vt_pack_runtime_clear( Candidate );
	if ( !open_package_candidate( pPackageBytes, nPackageBytes, Candidate ) ) {
		m_nStartupPackagePrepareUsecLast = t3dVtTicksUsec() - nPrepareStartedUsec;
		return false;
	}
	if ( m_bRuntimePackLoaded ) {
		const t3dVTPackRuntimeAllocator_t Allocator = t3dVtRuntimePackAllocator( m_pRuntimeAllocatorZone );
		t3d_vt_pack_runtime_release_cold( Allocator, m_RuntimePack );
	}
	if ( !t3d_vt_pack_runtime_transfer( m_RuntimePack, Candidate ) ) {
		t3d_vt_pack_runtime_release_cold( t3dVtRuntimePackAllocator( m_pRuntimeAllocatorZone ), Candidate );
		return false;
	}
	m_bRuntimePackLoaded = true;
	if ( !apply_quality_profile_state_v1( m_RuntimePack.pRuntimeAllocations, m_RuntimePack.Database.pHeader->nAllocationCount ) ) {
		release_runtime_pack_cold();
		m_nStartupPackagePrepareUsecLast = t3dVtTicksUsec() - nPrepareStartedUsec;
		return false;
	}
	++m_nQualityGeneration;
	m_nStartupPackagePrepareUsecLast = t3dVtTicksUsec() - nPrepareStartedUsec;
	godot::UtilityFunctions::print( "[Terrain3D][Startup][VT] PACKAGE PREP generation=", m_nStartupGeneration, " bytes=", nPackageBytes,
	                                " elapsed=", m_nStartupPackagePrepareUsecLast, " us" );
	return true;
}

bool Terrain3DVirtualTexture::loadPackageFileCold( const godot::String& rPath )
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	if ( m_pRuntimeAllocatorZone == NULL || m_bInitialized || m_bHotSealed ) {
		return false;
	}
	const uint64_t nStartedUsec = t3dVtTicksUsec();
	t3dVTPackRuntimeStorage_t candidate{};
	const t3dVTPackRuntimeAllocator_t allocator = t3dVtRuntimePackAllocator( m_pRuntimeAllocatorZone );
	if ( !t3d_vt_pack_runtime_open_file_cold( rPath, allocator, candidate ) ) {
		return false;
	}
	if ( !apply_quality_profile_state_v1( candidate.pRuntimeAllocations, candidate.Database.pHeader->nAllocationCount ) ) {
		t3d_vt_pack_runtime_release_cold( allocator, candidate );
		return false;
	}
	if ( m_bRuntimePackLoaded ) {
		t3d_vt_pack_runtime_release_cold( allocator, m_RuntimePack );
	}
	if ( !t3d_vt_pack_runtime_transfer( m_RuntimePack, candidate ) ) {
		t3d_vt_pack_runtime_release_cold( allocator, candidate );
		return false;
	}
	m_bRuntimePackLoaded = true;
	++m_nStartupPackagePrepareCount;
	++m_nQualityGeneration;
	m_nStartupPackagePrepareUsecLast = t3dVtTicksUsec() - nStartedUsec;
	return true;
}

bool Terrain3DVirtualTexture::open_package_candidate( const void *const pBytes, const uint64_t nBytes,
                                                      t3dVTPackRuntimeStorage_t& rCandidate )
{
	const t3dVTPackRuntimeAllocator_t Allocator = t3dVtRuntimePackAllocator( m_pRuntimeAllocatorZone );
	if ( !t3d_vt_pack_runtime_open_cold( pBytes, nBytes, Allocator, rCandidate ) ) {
		return false;
	}
	t3dVTQualityRuntimeStats_t Quality{};
	if ( !t3d_vt_pack_runtime_apply_quality_profile( rCandidate.Database, rCandidate.pRuntimeAllocations,
	                                                 rCandidate.Database.pHeader->nAllocationCount, m_nRequestedQualityProfile,
	                                                 Quality ) ) {
		t3d_vt_pack_runtime_release_cold( Allocator, rCandidate );
		return false;
	}
	return true;
}

bool Terrain3DVirtualTexture::stagePackageWarm( const void *const pBytes, const uint64_t nBytes )
{
	if ( !m_bInitialized || !m_bInitialPublished || !m_bWarmRollbackReady || m_bWarmPackagePending || m_pRuntimeAllocatorZone == NULL ||
	     m_pResources == NULL || ( pBytes == NULL ) != ( nBytes == 0u ) ) {
		return false;
	}
	++m_nWarmPublicationAttemptCount;
	m_bWarmResourceRebuildRequired = false;
	m_bWarmCandidateNeutral = nBytes == 0u;
	t3dVtWarmPackAllocator_t allocation{ m_pRuntimeAllocatorZone };
	const t3dVTPackRuntimeAllocator_t allocator{ &allocation, t3dVtWarmPackAllocate, t3dVtWarmPackFree };
	bool bValid = m_bWarmCandidateNeutral || t3d_vt_pack_runtime_open_cold( pBytes, nBytes, allocator, m_WarmCandidate );
	const char *failure_stage = allocation.m_bDenied ? "candidate runtime storage capacity" : "candidate package validation";
	if ( allocation.m_bDenied ) {
		// A validated package may fit alone but not alongside the retained live
		// package. Use the existing controlled rebuild/rollback transaction.
		m_bWarmResourceRebuildRequired = true;
	}
	if ( bValid && !m_bWarmCandidateNeutral ) {
		t3dVTQualityRuntimeStats_t quality{};
		bValid = t3d_vt_pack_runtime_apply_quality_profile( m_WarmCandidate.Database, m_WarmCandidate.pRuntimeAllocations,
		                                                    m_WarmCandidate.Database.pHeader->nAllocationCount, m_nRequestedQualityProfile,
		                                                    quality );
		failure_stage = "candidate quality profile";
	}
	if ( bValid && !m_bWarmCandidateNeutral ) {
		uint32_t nHeight = 0u;
		const bool bLimitsFit = validate_package_limits( m_WarmCandidate );
		const bool bTailsFit =
		    t3d_vt_compute_active_tail_height_cold( m_WarmCandidate.pRuntimeTails, m_WarmCandidate.Database.nRuntimeTailCount, nHeight ) &&
		    nHeight <= m_Gpu.get_cpu_resource_plan().nVtTailAtlasHeight;
		m_bWarmResourceRebuildRequired = !bLimitsFit || !bTailsFit;
		failure_stage = !bLimitsFit ? "runtime table capacity" : "tail atlas capacity";
		bValid = bLimitsFit && bTailsFit;
	}
	if ( !bValid ) {
		t3d_vt_pack_runtime_release_cold( t3dVtRuntimePackAllocator( m_pRuntimeAllocatorZone ), m_WarmCandidate );
		++m_nWarmPublicationFailureCount;
		godot::UtilityFunctions::print( "[Terrain3D][VT] WARM PACKAGE REJECTED publication_generation=", m_nPackagePublicationGeneration,
		                                " resource_rebuild_required=", m_bWarmResourceRebuildRequired, " reason=", failure_stage,
		                                " candidate_bytes=", nBytes,
		                                " runtime_free_extent_bytes=", m_pRuntimeAllocatorZone->freeExtentBytes() );
		return false;
	}
	m_bWarmPackagePending = true;
	return true;
}

bool Terrain3DVirtualTexture::stagePackageFileWarm( const godot::String& rPath )
{
	if ( !m_bInitialized || !m_bInitialPublished || !m_bWarmRollbackReady || m_bWarmPackagePending || m_pRuntimeAllocatorZone == NULL ||
	     m_pResources == NULL || rPath.is_empty() ) {
		return false;
	}
	++m_nWarmPublicationAttemptCount;
	m_bWarmResourceRebuildRequired = false;
	m_bWarmCandidateNeutral = false;
	t3dVtWarmPackAllocator_t allocation{ m_pRuntimeAllocatorZone };
	const t3dVTPackRuntimeAllocator_t allocator{ &allocation, t3dVtWarmPackAllocate, t3dVtWarmPackFree };
	bool valid = t3d_vt_pack_runtime_open_file_cold( rPath, allocator, m_WarmCandidate );
	if ( valid ) {
		t3dVTQualityRuntimeStats_t quality{};
		valid = t3d_vt_pack_runtime_apply_quality_profile( m_WarmCandidate.Database, m_WarmCandidate.pRuntimeAllocations,
		                                                   m_WarmCandidate.Database.pHeader->nAllocationCount, m_nRequestedQualityProfile,
		                                                   quality );
	}
	if ( valid ) {
		uint32_t height = 0u;
		valid =
		    validate_package_limits( m_WarmCandidate ) &&
		    t3d_vt_compute_active_tail_height_cold( m_WarmCandidate.pRuntimeTails, m_WarmCandidate.Database.nRuntimeTailCount, height ) &&
		    height <= m_Gpu.get_cpu_resource_plan().nVtTailAtlasHeight;
		m_bWarmResourceRebuildRequired = !valid;
	}
	if ( valid ) {
		valid = m_Gpu.stage_file_tails_warm( m_WarmCandidate.Database );
	}
	if ( !valid ) {
		m_bWarmResourceRebuildRequired |= allocation.m_bDenied;
		m_Gpu.discard_staged_file_tails_warm();
		t3d_vt_pack_runtime_release_cold( t3dVtRuntimePackAllocator( m_pRuntimeAllocatorZone ), m_WarmCandidate );
		++m_nWarmPublicationFailureCount;
		return false;
	}
	m_bWarmPackagePending = true;
	return true;
}

bool Terrain3DVirtualTexture::publish_package_state_warm( const t3dVTDatabaseView_t& rDatabase, const bool bNeutral,
                                                          const uint64_t nMaxBytes, const uint32_t nMaxWorkUnits,
                                                          bool *const pMutationStarted )
{
	if ( !m_Gpu.begin_package_publication_warm() ) {
		return false;
	}
	if ( pMutationStarted != NULL ) {
		*pMutationStarted = true;
	}
	const uint32_t nGeneration = t3dNextGeneration( m_Residency.nDatabaseGeneration );
	const uint32_t nTableGeneration = t3dNextGeneration( m_Residency.nPageTableGeneration );
	if ( bNeutral ? !prepare_neutral_legacy_cpu_cold() : !initialize_runtime_state_cold( rDatabase, nGeneration ) ) {
		return false;
	}
	m_Residency.nDatabaseGeneration = nGeneration;
	m_Residency.nPageTableGeneration = nTableGeneration;
	m_Streaming.bQuiescing = true;
	m_nStagedRequestCount = 0u;
	m_nStagedRequestFrame = UINT32_MAX;
	// The budget bounds individual upload slices. Completing this atomic unit
	// before returning prevents draws/readbacks from observing mixed tables/tails.
	do {
		if ( !m_Gpu.service_database_upload_warm( rDatabase, m_Residency, nMaxBytes, nMaxWorkUnits ) ) {
			return false;
		}
	} while ( m_Gpu.is_database_upload_pending_warm() );
	return bNeutral ? ( m_Gpu.upload_neutral_page_set_warm( m_Residency, 0u ) && t3d_vt_finalize_neutral_publication_warm( m_Residency ) )
	                : t3d_vt_finalize_package_publication_warm( m_Residency );
}

bool Terrain3DVirtualTexture::servicePackageRepublishWarm( const uint64_t nMaxBytes, const uint32_t nMaxWorkUnits )
{
	if ( !m_bWarmPackagePending || !m_bInitialized || nMaxBytes == 0u || nMaxWorkUnits == 0u ) {
		return false;
	}
	if ( !t3d_vt_streaming_quiesce_package_warm( m_Streaming ) ) {
		return true;
	}
	// Owner-thread material/MegaVT publication also resolves immutable identities.
	// Hold their read gate until residency, GPU tables and package ownership agree.
	Terrain3DScopedLock packageLock( m_packageMutex );
	const uint64_t nStartedUsec = t3dVtTicksUsec();
	const bool bWasSealed = m_bHotSealed;
	const uint32_t nPreviousDatabaseGeneration = m_Residency.nDatabaseGeneration;
	const uint32_t nPreviousTableGeneration = m_Residency.nPageTableGeneration;
	const t3dVTDatabaseView_t& CandidateDatabase = m_bWarmCandidateNeutral ? m_NeutralDatabase : m_WarmCandidate.Database;
	// Identical-pack fast path. Exact package generation, immutable content
	// identity and quality policy must all agree before preserving GPU mappings.
	// Non-identical/neutral publications retain the original transactional rebuild.
	if ( m_bIdenticalPackageReuse && !m_bWarmCandidateNeutral && m_bRuntimePackLoaded && m_bWarmRollbackReady &&
	     m_Residency.pDatabase == &m_RuntimePack.Database && CandidateDatabase.nRuntimeKind == m_RuntimePack.Database.nRuntimeKind &&
	     CandidateDatabase.nPackageGeneration != 0u && CandidateDatabase.nPackageGeneration == m_RuntimePack.Database.nPackageGeneration &&
	     CandidateDatabase.nPackageContentSignature != 0u &&
	     CandidateDatabase.nPackageContentSignature == m_RuntimePack.Database.nPackageContentSignature && CandidateDatabase.pHeader &&
	     m_RuntimePack.Database.pHeader && CandidateDatabase.pHeader->nPageCount == m_RuntimePack.Database.pHeader->nPageCount &&
	     CandidateDatabase.pHeader->nAllocationCount == m_RuntimePack.Database.pHeader->nAllocationCount &&
	     m_QualityStats.nRequestedProfile == m_nRequestedQualityProfile ) {
		++m_nIdenticalRepublishSkips;
		++m_nWarmPublicationSuccessCount;
		m_nWarmPublicationUsecLast = t3dVtTicksUsec() - nStartedUsec;
		abortPackageRepublishWarm();
		return true;
	}
	// Preserve the current file-backed atlas before the live upload state is reset.
	m_Gpu.backup_file_tails_warm( m_bRuntimePackLoaded ? m_RuntimePack.Database : m_NeutralDatabase );
	bool bMutationStarted = false;
	const bool bPublished =
	    publish_package_state_warm( CandidateDatabase, m_bWarmCandidateNeutral, nMaxBytes, nMaxWorkUnits, &bMutationStarted );
	if ( bPublished ) {
		t3dVTPackRuntimeStorage_t Previous;
		t3d_vt_pack_runtime_clear( Previous );
		// The neutral package has no owned block; clear it without transferring.
		if ( m_bRuntimePackLoaded ) {
			( void )t3d_vt_pack_runtime_transfer( Previous, m_RuntimePack );
		}
		if ( !m_bWarmCandidateNeutral ) {
			( void )t3d_vt_pack_runtime_transfer( m_RuntimePack, m_WarmCandidate );
		}
		m_bRuntimePackLoaded = !m_bWarmCandidateNeutral;
		rebuildFarRegionAllocationTableCold();
		m_Residency.pDatabase = m_bRuntimePackLoaded ? &m_RuntimePack.Database : &m_NeutralDatabase;
		m_Gpu.set_database_view_warm( m_Residency.pDatabase );
		( void )apply_quality_profile_state_v1( m_pAllocations, m_Residency.nAllocationCount );
		++m_nQualityGeneration;
		++m_nPackagePublicationGeneration;
		++m_nWarmPublicationSuccessCount;
		t3d_vt_pack_runtime_release_cold( t3dVtRuntimePackAllocator( m_pRuntimeAllocatorZone ), Previous );
	} else {
		const t3dVTDatabaseView_t& PreviousDatabase = m_bRuntimePackLoaded ? m_RuntimePack.Database : m_NeutralDatabase;
		m_bWarmRollbackReady =
		    !bMutationStarted || publish_package_state_warm( PreviousDatabase, !m_bRuntimePackLoaded, UINT64_MAX, UINT32_MAX );
		m_Residency.nDatabaseGeneration = nPreviousDatabaseGeneration;
		m_Residency.nPageTableGeneration = nPreviousTableGeneration;
		++m_nWarmPublicationFailureCount;
		m_Gpu.set_database_view_warm( &PreviousDatabase );
	}
	// CPU residency counters restarted; reset cadence baselines as well.
	m_nTelemetryLastUploadedPageSets = m_Streaming.Stats.nUploadedPageCount;
	m_nTelemetryLastEvictions = m_Residency.Stats.nEvictionCount;
	m_nUploadCadenceStableFrames = 0u;
	m_nUploadCadenceLastPageMisses = m_Streaming.Stats.nPageMissCount;
	m_nUploadCadenceLastEvictions = m_Residency.Stats.nEvictionCount;
	m_nUploadCadenceLastThrash = m_Residency.Stats.nThrashCount;
	m_nWarmPublicationUsecLast = t3dVtTicksUsec() - nStartedUsec;
	abortPackageRepublishWarm();
	if ( bWasSealed ) {
		m_Gpu.seal_hot_runtime();
	}
	if ( !m_bWarmRollbackReady ) {
		m_bInitialized = false;
	}
	godot::UtilityFunctions::print( "[Terrain3D][VT] WARM PACKAGE PUBLICATION publication_generation=", m_nPackagePublicationGeneration,
	                                " elapsed=", m_nWarmPublicationUsecLast, " us ready=", bPublished,
	                                " rollback_ready=", m_bWarmRollbackReady );
	return bPublished;
}

void Terrain3DVirtualTexture::abortPackageRepublishWarm()
{
	m_Gpu.discard_staged_file_tails_warm();
	t3d_vt_pack_runtime_release_cold( t3dVtRuntimePackAllocator( m_pRuntimeAllocatorZone ), m_WarmCandidate );
	m_bWarmPackagePending = false;
	t3d_vt_streaming_resume_package_warm( m_Streaming );
}

bool Terrain3DVirtualTexture::clearPackageCold( void )
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	if ( m_bInitialized || m_bHotSealed ) {
		return false;
	}
	release_runtime_pack_cold();
	return true;
}

void Terrain3DVirtualTexture::release_runtime_pack_cold()
{
	if ( m_bRuntimePackLoaded ) {
		const t3dVTPackRuntimeAllocator_t Allocator = t3dVtRuntimePackAllocator( m_pRuntimeAllocatorZone );
		t3d_vt_pack_runtime_release_cold( Allocator, m_RuntimePack );
	}
	m_bRuntimePackLoaded = false;
	rebuildFarRegionAllocationTableCold();
	memset( &m_QualityStats, 0, sizeof( m_QualityStats ) );
	m_QualityStats.nRequestedProfile = m_nRequestedQualityProfile;
}

bool Terrain3DVirtualTexture::findMaterialAllocationCold( const uint64_t nStableKey, uint32_t& rAllocationId ) const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	if ( !m_bRuntimePackLoaded ) {
		rAllocationId = T3D_VT_INVALID_ALLOCATION;
		return false;
	}
	if ( !t3d_vt_pack_runtime_find_allocation_by_stable_key( m_RuntimePack, nStableKey, rAllocationId ) ) {
		return false;
	}
	if ( m_RuntimePack.Database.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK ) {
		rAllocationId = t3d_vt_runtime_allocation_base( rAllocationId );
	}
	return true;
}

bool Terrain3DVirtualTexture::snapshotMaterialReadStateCold( t3dVTMaterialReadSnapshot_t& rSnapshot ) const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	memset( &rSnapshot, 0, sizeof( rSnapshot ) );
	for ( uint32_t i = 0u; i < T3D_RD_MATERIAL_LAYER_COUNT; ++i ) {
		rSnapshot.nMaterialAllocation[i] = T3D_VT_INVALID_ALLOCATION;
	}
	const t3dVTDatabaseView_t *const pDatabase = m_Residency.pDatabase;
	rSnapshot.nDatabaseKind = pDatabase != NULL ? pDatabase->nRuntimeKind : T3D_VT_RUNTIME_DATABASE_LEGACY_NEUTRAL;
	rSnapshot.nDatabaseGeneration = m_Residency.nDatabaseGeneration;
	rSnapshot.nPackageGeneration = pDatabase != NULL ? pDatabase->nPackageGeneration : 0u;
	rSnapshot.nPackageContentSignature = pDatabase != NULL ? pDatabase->nPackageContentSignature : 0u;
	rSnapshot.nPackagePublicationGeneration = m_nPackagePublicationGeneration.load( T3D_MEMORY_ORDER_ACQUIRE );
	rSnapshot.nMaterialCount = MIN( m_Limits.nMaxMaterials, ( uint32_t )T3D_RD_MATERIAL_LAYER_COUNT );
	if ( !m_bRuntimePackLoaded ) {
		return true;
	}
	for ( uint32_t i = 0u; i < rSnapshot.nMaterialCount; ++i ) {
		uint32_t nAllocation = T3D_VT_INVALID_ALLOCATION;
		if ( t3d_vt_pack_runtime_find_allocation_by_stable_key( m_RuntimePack, t3d_vt_terrain_material_stable_key( i ), nAllocation ) ) {
			rSnapshot.nMaterialAllocation[i] = m_RuntimePack.Database.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK
			                                       ? t3d_vt_runtime_allocation_base( nAllocation )
			                                       : nAllocation;
		}
	}
	return true;
}

bool Terrain3DVirtualTexture::snapshotTerrainMegaVTReadStateCold( uint32_t *const pAllocationIds, uint32_t *const pCookedGenerations,
                                                                  uint8_t *const pCookedValid, const uint32_t nCount ) const
{
	if ( pAllocationIds == NULL || pCookedGenerations == NULL || pCookedValid == NULL || nCount == 0u ||
	     nCount > T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY ) {
		return false;
	}
	Terrain3DScopedLock packageLock( m_packageMutex );
	const uint32_t nBase = m_Limits.nMaxMaterials * T3D_VT_CHANNEL_COUNT;
	for ( uint32_t i = 0u; i < nCount; ++i ) {
		pAllocationIds[i] = nBase + i * T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION;
		pCookedGenerations[i] = 0u;
		pCookedValid[i] = 0u;
		if ( !m_bRuntimePackLoaded || m_RuntimePack.Database.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK ) {
			continue;
		}
		uint32_t nRawAllocation = T3D_VT_INVALID_ALLOCATION;
		if ( !t3d_vt_pack_runtime_find_allocation_by_stable_key( m_RuntimePack, t3d_vt_terrain_region_stable_key( i ), nRawAllocation ) ) {
			continue;
		}
		pAllocationIds[i] = t3d_vt_runtime_allocation_base( nRawAllocation );
		const t3dVTPackAllocationRecord_t& allocation = m_RuntimePack.PackViewV1.m_pAllocations[nRawAllocation];
		if ( ( allocation.m_nFlags & T3D_VT_ALLOCATION_MEGATEXTURE ) != 0u && allocation.m_nGeneration != 0u ) {
			pCookedGenerations[i] = allocation.m_nGeneration;
			pCookedValid[i] = 1u;
		}
	}
	return true;
}

bool Terrain3DVirtualTexture::allocate_cpu_image_cold()
{
	if ( m_pCpuImage != NULL || m_pRuntimeAllocatorZone == NULL || m_Limits.nStreamScratchLaneCount > T3D_VT_MAX_STREAM_SCRATCH_LANES ) {
		return false;
	}
	if ( t3d_vt_allocation_capacity( m_Limits.nMaxMaterials ) == 0u ) {
		return false;
	}
	const uint32_t nAllocationCapacity = t3d_vt_allocation_capacity( m_Limits.nMaxMaterials );
	// Retired GPU multi-feedback pilot has no production shader or dispatch.
	m_bMultiFeedbackParityRequested = false;
	uint64_t nBytes = 0u;
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )nAllocationCapacity * sizeof( t3dVTAllocation_t );
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )nAllocationCapacity * sizeof( t3dVTTailMetadata_t );
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( t3dVTPageTableEntry_t );
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtPhysicalPageSets * sizeof( t3dVTPhysicalSlot_t );
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxStreamJobs * sizeof( t3dVTStreamJob_t );
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( uint16_t );
	for ( uint32_t i = 0u; i < 6u; ++i ) {
		nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxStreamJobs * sizeof( uint16_t );
	}
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtPhysicalPageSets * sizeof( uint32_t );
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtPhysicalPageSets * sizeof( t3dVTRetiredPageSet_t );
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( uint32_t );
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( uint32_t );
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtRequests * sizeof( t3dVTStagedRequest_t );
	// Sparse epoch-stamped histogram plus bounded top-K. All scratch is cold-owned.
	for ( uint32_t i = 0u; i < 4u; ++i ) {
		nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( uint32_t );
	}
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nMaxVtRequests * sizeof( t3dVTFeedbackCandidate_t );
	if ( m_bMultiFeedbackParityRequested ) {
		nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )T3D_VT_MULTI_PARITY_TOTAL_WORDS * sizeof( uint32_t );
	}
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nStreamScratchLaneCount * m_Limits.nStreamScratchBytesPerLane;
	nBytes = t3dAlignUpOrZero<uint64_t>( nBytes, 64u ) + ( uint64_t )m_Limits.nStreamScratchLaneCount * sizeof( uint32_t );
	if ( nBytes == 0u || nBytes > SIZE_MAX ) {
		return false;
	}
	m_pCpuImage = m_pRuntimeAllocatorZone->allocate( ( size_t )nBytes, 64u );
	if ( m_pCpuImage == NULL ) {
		return false;
	}
	m_nCpuImageBytes = nBytes;
	memset( m_pCpuImage, 0, ( size_t )nBytes );
	uint8_t *const pBase = ( uint8_t * )m_pCpuImage;
	uint64_t nOffset = 0u;
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pAllocations = ( t3dVTAllocation_t * )( pBase + nOffset );
	nOffset += ( uint64_t )nAllocationCapacity * sizeof( t3dVTAllocation_t );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pTailMetadata = ( t3dVTTailMetadata_t * )( pBase + nOffset );
	nOffset += ( uint64_t )nAllocationCapacity * sizeof( t3dVTTailMetadata_t );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pPageTable = ( t3dVTPageTableEntry_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( t3dVTPageTableEntry_t );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pPhysicalPageSets = ( t3dVTPhysicalSlot_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxVtPhysicalPageSets * sizeof( t3dVTPhysicalSlot_t );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pStreamJobs = ( t3dVTStreamJob_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxStreamJobs * sizeof( t3dVTStreamJob_t );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pStreamPageToJob = ( uint16_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( uint16_t );
	uint16_t **const pJobArrays[6] = { &m_pStreamFreeJobs,  &m_pStreamQueuedJobs,      &m_pStreamJobQueuePosition,
	                                   &m_pStreamReadyJobs, &m_pStreamMergedWantedMip, &m_pStreamMergedPriority };
	for ( uint32_t i = 0u; i < 6u; ++i ) {
		nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
		*pJobArrays[i] = ( uint16_t * )( pBase + nOffset );
		nOffset += ( uint64_t )m_Limits.nMaxStreamJobs * sizeof( uint16_t );
	}
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pFreePhysicalSlots = ( uint32_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxVtPhysicalPageSets * sizeof( uint32_t );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pRetiredPageSets = ( t3dVTRetiredPageSet_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxVtPhysicalPageSets * sizeof( t3dVTRetiredPageSet_t );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pLastEvictedFrame = ( uint32_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( uint32_t );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pStagedByPage = ( uint32_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( uint32_t );
	memset( m_pStagedByPage, 0xff, ( size_t )m_Limits.nMaxVtVirtualPages * sizeof( uint32_t ) );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pStagedRequests = ( t3dVTStagedRequest_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxVtRequests * sizeof( t3dVTStagedRequest_t );
	m_nStagedRequestCount = 0u;
	m_nStagedRequestFrame = UINT32_MAX;
	uint32_t **const feedbackArrays[4] = { &m_pFeedbackEpochs, &m_pFeedbackDirect, &m_pFeedbackAggregate, &m_pFeedbackTouched };
	for ( uint32_t i = 0u; i < 4u; ++i ) {
		nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
		*feedbackArrays[i] = ( uint32_t * )( pBase + nOffset );
		nOffset += ( uint64_t )m_Limits.nMaxVtVirtualPages * sizeof( uint32_t );
	}
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pFeedbackCandidates = ( t3dVTFeedbackCandidate_t * )( pBase + nOffset );
	nOffset += ( uint64_t )m_Limits.nMaxVtRequests * sizeof( t3dVTFeedbackCandidate_t );
	m_feedbackReducer.bindCold( m_Limits.nMaxVtVirtualPages, m_Limits.nMaxVtRequests, m_pFeedbackEpochs, m_pFeedbackDirect,
	                            m_pFeedbackAggregate, m_pFeedbackTouched, m_pFeedbackCandidates );
	if ( m_bMultiFeedbackParityRequested ) {
		nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
		m_multiFeedbackParity.bindCold( ( uint32_t * )( pBase + nOffset ), T3D_VT_MULTI_PARITY_TOTAL_WORDS );
		nOffset += ( uint64_t )T3D_VT_MULTI_PARITY_TOTAL_WORDS * sizeof( uint32_t );
	} else {
		m_multiFeedbackParity.bindCold( NULL, 0u );
	}
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pStreamScratch = pBase + nOffset;
	nOffset += ( uint64_t )m_Limits.nStreamScratchLaneCount * m_Limits.nStreamScratchBytesPerLane;
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset, 64u );
	m_pScratchLaneOwner = ( uint32_t * )( pBase + nOffset );
	return nOffset + ( uint64_t )m_Limits.nStreamScratchLaneCount * sizeof( uint32_t ) <= nBytes;
}

void Terrain3DVirtualTexture::release_cpu_image_cold()
{
	m_feedbackReducer.resetCold();
	m_multiFeedbackParity.bindCold( NULL, 0u );
	m_bMultiFeedbackParityRequested = false;
	m_pFeedbackEpochs = NULL;
	m_pFeedbackDirect = NULL;
	m_pFeedbackAggregate = NULL;
	m_pFeedbackTouched = NULL;
	m_pFeedbackCandidates = NULL;
	if ( m_pCpuImage != NULL && m_pRuntimeAllocatorZone != NULL ) {
		m_pRuntimeAllocatorZone->deallocate( m_pCpuImage );
	}
	m_pCpuImage = NULL;
	m_nCpuImageBytes = 0u;
	m_pAllocations = NULL;
	m_pTailMetadata = NULL;
	m_pPageTable = NULL;
	m_pPhysicalPageSets = NULL;
	m_pStreamJobs = NULL;
	m_pStreamPageToJob = NULL;
	m_pStreamFreeJobs = NULL;
	m_pStreamQueuedJobs = NULL;
	m_pStreamJobQueuePosition = NULL;
	m_pStreamReadyJobs = NULL;
	m_pStreamMergedWantedMip = NULL;
	m_pStreamMergedPriority = NULL;
	m_pFreePhysicalSlots = NULL;
	m_pRetiredPageSets = NULL;
	m_pLastEvictedFrame = NULL;
	m_pStagedRequests = NULL;
	m_pStagedByPage = NULL;
	m_nStagedRequestCount = 0u;
	m_nStagedRequestFrame = UINT32_MAX;
	m_pStreamScratch = NULL;
	m_pScratchLaneOwner = NULL;
}

bool Terrain3DVirtualTexture::prepare_neutral_legacy_cpu_cold()
{
	memset( &m_NeutralHeader, 0, sizeof( m_NeutralHeader ) );
	m_NeutralHeader.nMagic = T3D_VT_LEGACY_FILE_MAGIC;
	m_NeutralHeader.nVersion = T3D_VT_LEGACY_FILE_VERSION;
	m_NeutralHeader.nHeaderBytes = sizeof( t3dVTFileHeader_t );
	m_NeutralHeader.nAllocationCount = T3D_VT_CHANNEL_COUNT;
	m_NeutralHeader.nPageCount = T3D_VT_CHANNEL_COUNT;
	m_NeutralHeader.nPayloadCount = T3D_VT_CHANNEL_COUNT;
	m_NeutralHeader.nChannelCount = T3D_VT_CHANNEL_COUNT;
	m_NeutralHeader.nPayloadDataBytes = sizeof( m_NeutralPayloadBytes );
	m_NeutralHeader.nInteriorTexels = T3D_VT_PAGE_INTERIOR;
	m_NeutralHeader.nGutterTexels = T3D_VT_PAGE_GUTTER;
	m_NeutralHeader.nPhysicalTexels = T3D_VT_PHYSICAL_TILE;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		t3dVTAllocationRecord_t& allocation = m_NeutralAllocations[c];
		memset( &allocation, 0, sizeof( allocation ) );
		allocation.nBaseVirtualPage = c;
		allocation.nPageCount = 1u;
		allocation.nMip0WidthPages = 1u;
		allocation.nMip0HeightPages = 1u;
		allocation.nMipCount = 1u;
		allocation.nChannel = ( uint16_t )c;
		allocation.nFlags = T3D_VT_PAGE_NEUTRAL | T3D_VT_PAGE_ROOT;
		allocation.nRootPage = c;
		t3dVTPageFileRecord_t& page = m_NeutralPages[c];
		memset( &page, 0, sizeof( page ) );
		page.nVirtualPage = c;
		page.nParentPage = T3D_VT_INVALID_PAGE;
		page.nAllocationId = c;
		page.nPayloadId = c;
		page.nMip = 0u;
		page.nChannel = ( uint16_t )c;
		page.nFlags = T3D_VT_PAGE_NEUTRAL | T3D_VT_PAGE_ROOT;
		page.nGeneration = 1u;
		memset( &m_NeutralPayloads[c], 0, sizeof( m_NeutralPayloads[c] ) );
		m_NeutralPayloads[c].nOffset = c;
		m_NeutralPayloads[c].nCompressedBytes = 1u;
		m_NeutralPayloads[c].nDecodedBytes = 1u;
	}
	m_NeutralDatabase.pHeader = &m_NeutralHeader;
	m_NeutralDatabase.pAllocations = m_NeutralAllocations;
	m_NeutralDatabase.pPages = m_NeutralPages;
	m_NeutralDatabase.pPayloads = m_NeutralPayloads;
	m_NeutralDatabase.pPayloadData = m_NeutralPayloadBytes;
	m_NeutralDatabase.nFileBytes = sizeof( m_NeutralHeader ) + sizeof( m_NeutralAllocations ) + sizeof( m_NeutralPages ) +
	                               sizeof( m_NeutralPayloads ) + sizeof( m_NeutralPayloadBytes );

	return initialize_runtime_state_cold( m_NeutralDatabase, 1u );
}

bool Terrain3DVirtualTexture::initialize_runtime_state_cold( const t3dVTDatabaseView_t& rDatabase, const uint32_t nDatabaseGeneration )
{
	t3dVTResidencyBuffers_t ResidencyBuffers = {};
	ResidencyBuffers.pAllocations = m_pAllocations;
	ResidencyBuffers.nAllocationCapacity = t3d_vt_allocation_capacity( m_Limits.nMaxMaterials );
	ResidencyBuffers.pPageTable = m_pPageTable;
	ResidencyBuffers.nPageTableCapacity = m_Limits.nMaxVtVirtualPages;
	ResidencyBuffers.pTailMetadata = m_pTailMetadata;
	ResidencyBuffers.nTailMetadataCapacity = t3d_vt_allocation_capacity( m_Limits.nMaxMaterials );
	ResidencyBuffers.pPhysicalPageSets = m_pPhysicalPageSets;
	ResidencyBuffers.nPhysicalPageSetCapacity = m_Limits.nMaxVtPhysicalPageSets;
	ResidencyBuffers.pFreeSlots = m_pFreePhysicalSlots;
	ResidencyBuffers.pRetiredPageSets = m_pRetiredPageSets;
	ResidencyBuffers.pLastEvictedFrame = m_pLastEvictedFrame;
	if ( !t3d_vt_initialize_residency_cold( rDatabase, ResidencyBuffers, nDatabaseGeneration, m_ResidencyPolicy.nRetireDelayFrames,
	                                        m_Residency ) ) {
		return false;
	}
	m_Residency.Policy = m_ResidencyPolicy;
	m_Residency.nRetirementDelayFrames = m_ResidencyPolicy.nRetireDelayFrames;

	t3dVTStreamingBuffers_t StreamBuffers = {};
	StreamBuffers.pJobs = m_pStreamJobs;
	StreamBuffers.nJobCapacity = m_Limits.nMaxStreamJobs;
	StreamBuffers.pScratchBytes = m_pStreamScratch;
	StreamBuffers.nScratchLaneCount = m_Limits.nStreamScratchLaneCount;
	StreamBuffers.nScratchLaneBytes = ( uint32_t )m_Limits.nStreamScratchBytesPerLane;
	StreamBuffers.pScratchLaneOwner = m_pScratchLaneOwner;
	StreamBuffers.pScratchLaneCompletion = m_ScratchLaneCompletion;
	StreamBuffers.pPageToJob = m_pStreamPageToJob;
	StreamBuffers.nPageToJobCapacity = m_Limits.nMaxVtVirtualPages;
	StreamBuffers.pFreeJobs = m_pStreamFreeJobs;
	StreamBuffers.pQueuedJobs = m_pStreamQueuedJobs;
	StreamBuffers.pJobQueuePosition = m_pStreamJobQueuePosition;
	StreamBuffers.pReadyJobs = m_pStreamReadyJobs;
	StreamBuffers.pMergedWantedMip = m_pStreamMergedWantedMip;
	StreamBuffers.pMergedPriority = m_pStreamMergedPriority;
	for ( uint32_t i = 0u; i < m_Limits.nStreamScratchLaneCount; ++i ) {
		m_ScratchLaneCompletion[i].store( T3D_VT_STREAM_COMPLETION_IDLE, T3D_MEMORY_ORDER_RELAXED );
	}
	return t3d_vt_streaming_initialize_cold( m_Residency, StreamBuffers, m_StreamingLimits, m_Streaming );
}

bool Terrain3DVirtualTexture::publish_neutral_legacy_warm()
{
	return m_Gpu.upload_database_warm( m_NeutralDatabase, m_Residency ) && m_Gpu.upload_neutral_page_set_warm( m_Residency, 0u ) &&
	       t3d_vt_finalize_neutral_publication_warm( m_Residency );
}

bool Terrain3DVirtualTexture::validate_package_limits( const t3dVTPackRuntimeStorage_t& rCandidate ) const
{
	const t3dVTDatabaseView_t& Database = rCandidate.Database;
	if ( Database.pHeader == NULL || Database.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK || Database.pRuntimePayloads == NULL ||
	     Database.pRuntimeAllocations == NULL || Database.pHeader->nAllocationCount == 0u ||
	     Database.pHeader->nAllocationCount > t3d_vt_allocation_capacity( m_Limits.nMaxMaterials ) || Database.pHeader->nPageCount == 0u ||
	     Database.pHeader->nPageCount > m_Limits.nMaxVtVirtualPages || Database.nRuntimeTailCount == 0u ||
	     Database.nRuntimeTailCount > t3d_vt_allocation_capacity( m_Limits.nMaxMaterials ) ||
	     m_Limits.nStreamScratchBytesPerLane > UINT32_MAX ) {
		return false;
	}
	const uint32_t nV1Count = rCandidate.PackViewV1.m_pHeader->m_nAllocationCount;
	if ( rCandidate.PackViewV1.m_pAllocations == NULL || rCandidate.pRuntimeTails == NULL ||
	     nV1Count > UINT32_MAX / T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION )
		return false;
	for ( uint32_t i = 0u; i < nV1Count; ++i ) {
		const t3dVTPackAllocationRecord_t& source = rCandidate.PackViewV1.m_pAllocations[i];
		if ( source.m_nConsumer != T3D_VT_CONSUMER_COMPOSED )
			continue;
		const uint32_t required = T3D_VT_COMPOSED_REQUIRED_CHANNEL_MASK;
		if ( !source.m_nGeneration || ( source.m_nChannelMask & required ) != required )
			return false;
		const uint32_t base = t3d_vt_runtime_allocation_base( i );
		for ( uint32_t c = 0u; c < T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION; ++c ) {
			const t3dVTAllocation_t& allocation = rCandidate.pRuntimeAllocations[base + c];
			if ( !allocation.m_nMipCount || allocation.m_nGeneration != source.m_nGeneration ||
			     allocation.m_nTailIndex == T3D_VT_INVALID_TAIL || allocation.m_nTailIndex >= Database.nRuntimeTailCount )
				return false;
			const t3dVTTailMetadata_t& tail = rCandidate.pRuntimeTails[allocation.m_nTailIndex];
			if ( !tail.m_nMipCount || !tail.m_nChannelMask || tail.m_nGeneration != source.m_nGeneration )
				return false;
		}
	}
	for ( uint32_t iPage = 0u; iPage < Database.pHeader->nPageCount; ++iPage ) {
		const t3dVTPageFileRecord_t& Page = Database.pPages[iPage];
		if ( Page.nPayloadId >= Database.nRuntimePayloadCount ) {
			return false;
		}
		const uint32_t nDecodedBytes = Database.pRuntimePayloads[Page.nPayloadId].nBytes;
		if ( nDecodedBytes == 0u || nDecodedBytes > m_Limits.nStreamScratchBytesPerLane ) {
			return false;
		}
	}

	return true;
}

bool Terrain3DVirtualTexture::prepare_runtime_package_cpu_cold()
{
	const t3dVTDatabaseView_t& Database = m_RuntimePack.Database;
	if ( !m_bRuntimePackLoaded || !validate_package_limits( m_RuntimePack ) ) {
		return false;
	}

	if ( m_spatialDemand.isValidFor( Database.nPackageContentSignature ) &&
	     m_spatialDemand.getVersion() == T3D_VT_DEMAND_STABLE_MATERIAL_FORMAT_VERSION ) {
		const auto& view = m_RuntimePack.PackViewV1;
		const uint32_t nKeys = view.m_pHeader->m_nAllocationCount;
		std::vector<uint64_t> keys;
		std::vector<uint32_t> ids;
		keys.reserve( nKeys );
		ids.reserve( nKeys );
		for ( uint32_t i = 0u; i < nKeys; ++i ) {
			keys.push_back( view.m_pAllocationKeys[i].m_nStableKey );
			ids.push_back( t3d_vt_runtime_allocation_base( view.m_pAllocationKeys[i].m_nAllocationId ) );
		}
		( void )m_spatialDemand.bindStableKeysCold( keys.data(), ids.data(), nKeys );
	}

	// T04: on-demand v1 loading is cold and optional; an unreadable or stale
	// histogram never invalidates the already-published canonical v2 package.
	m_v1Demand.resetCold();
	m_v1MaterialKeys.clear();
	m_v1MaterialAllocations.clear();
	if ( !m_v1DemandPath.is_empty() && m_nV1AssetIdentityHash != 0u &&
	     m_v1Demand.openCold( m_v1DemandPath, Database.nPackageContentSignature, m_nV1AssetIdentityHash ) ) {
		const auto& view = m_RuntimePack.PackViewV1;
		{
			const uint32_t nKeys = view.m_pHeader->m_nAllocationCount;
			m_v1MaterialKeys.reserve( nKeys );
			m_v1MaterialAllocations.reserve( nKeys );
			for ( uint32_t i = 0u; i < nKeys; ++i ) {
				m_v1MaterialKeys.push_back( view.m_pAllocationKeys[i].m_nStableKey );
				m_v1MaterialAllocations.push_back( t3d_vt_runtime_allocation_base( view.m_pAllocationKeys[i].m_nAllocationId ) );
			}
		}
	}

	if ( !apply_quality_profile_state_v1( m_RuntimePack.pRuntimeAllocations, Database.pHeader->nAllocationCount ) ) {
		return false;
	}

	// Stable-key lookup is a binary search on the final cooked package keys.
	for ( uint32_t i = 1u; i < m_v1MaterialKeys.size(); ++i ) {
		if ( m_v1MaterialKeys[i - 1u] >= m_v1MaterialKeys[i] ) {
			m_v1MaterialKeys.clear();
			m_v1MaterialAllocations.clear();
			break;
		}
	}
	const uint32_t nDatabaseGeneration = Database.nPackageGeneration != 0u ? Database.nPackageGeneration : 1u;
	rebuildFarRegionAllocationTableCold();
	return initialize_runtime_state_cold( Database, nDatabaseGeneration );
}

bool Terrain3DVirtualTexture::publish_runtime_package_warm()
{
	const t3dVTDatabaseView_t& Database = m_RuntimePack.Database;
	return m_Gpu.upload_database_warm( Database, m_Residency ) && t3d_vt_finalize_package_publication_warm( m_Residency );
}

uint32_t Terrain3DVirtualTexture::count_physical_banks() const
{
	if ( m_pResources == NULL ) {
		return 0u;
	}
	uint32_t nCount = 0u;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		nCount += m_pResources->get_vt_bank_count( ( t3dVTResourceChannel_t )c );
	}
	return nCount;
}

bool Terrain3DVirtualTexture::stageDemandIndexCold( const uint8_t *const pBytes, const uint64_t nBytes,
                                                    const godot::String& rSourceIdentity )
{
	if ( m_bInitialized || m_bHotSealed || m_bCpuPrepared ) {
		return false;
	}
	m_spatialDemand.resetCold();
	m_demandIndexSourceIdentity = rSourceIdentity;
	m_nSpatialDemandLastStageFrame = UINT32_MAX;
	m_nSpatialDemandLastCellX = INT32_MIN;
	m_nSpatialDemandLastCellZ = INT32_MIN;
	m_nSpatialDemandLastOctant = 0u;
	m_bSpatialDemandCurrentCellComplete = false;
	m_nSpatialDemandCoveredAllocationCount = 0u;
	if ( pBytes == NULL || nBytes == 0u ) {
		return true;
	}
	if ( !m_spatialDemand.loadCold( pBytes, nBytes ) ) {
		return false;
	}
	if ( m_spatialDemand.getVersion() == T3D_VT_DEMAND_STABLE_MATERIAL_FORMAT_VERSION ) {
		const godot::CharString bytes = rSourceIdentity.utf8();
		const uint64_t expected = t3d_vt_demand_hash( ( const uint8_t * )bytes.get_data(), bytes.length() );
		if ( m_spatialDemand.getAssetIdentityHash() != expected ) {
			m_spatialDemand.resetCold();
			return false;
		}
	}
	return true;
}

bool Terrain3DVirtualTexture::stageDemandV1Cold( const godot::String& path, const godot::String& sourceIdentity )
{
	if ( m_bInitialized || m_bHotSealed || m_bCpuPrepared )
		return false;
	m_v1Demand.resetCold();
	m_v1DemandPath = path;
	m_v1MaterialKeys.clear();
	m_v1MaterialAllocations.clear();
	m_nV1AssetIdentityHash = 0u;
	if ( path.is_empty() || sourceIdentity.is_empty() )
		return true;
	const godot::CharString utf8 = sourceIdentity.utf8();
	m_nV1AssetIdentityHash = t3d_vt_demand_hash( ( const uint8_t * )utf8.get_data(), utf8.length() );
	return m_nV1AssetIdentityHash != 0u;
}

bool Terrain3DVirtualTexture::stagedDemandMatchesLoadedPackageCold() const
{
	return m_bRuntimePackLoaded && m_spatialDemand.isValidFor( m_RuntimePack.Database.nPackageContentSignature );
}

bool Terrain3DVirtualTexture::load_spatial_demand_index_cold( const uint8_t *const pBytes, const uint64_t nBytes )
{
	if ( m_bInitialized || m_bHotSealed ) {
		return false;
	}
	return m_spatialDemand.loadCold( pBytes, nBytes );
}

uint32_t Terrain3DVirtualTexture::stage_v1_demand_hot( const float cameraX, const float cameraZ, const float forwardX, const float forwardZ,
                                                       const uint32_t frame, const float fov, const uint32_t viewHeight )
{
	const t3dVTDatabaseView_t *const db = m_Residency.pDatabase;
	if ( !m_bInitialized || !m_Streaming.bInitialized || !db || m_bWarmPackagePending ||
	     !m_v1Demand.validFor( db->nPackageContentSignature ) || m_v1MaterialKeys.empty() || !viewHeight || m_nV1LastStageFrame == frame )
		return 0u;
	m_nV1LastStageFrame = frame;
	const float cell = ( float )T3D_VT_DEMAND_BAKED_VISIBILITY_CELL_METERS;
	const int32_t x = ( int32_t )floorf( cameraX / cell );
	const int32_t z = ( int32_t )floorf( cameraZ / cell );
	const int32_t lx = ( int32_t )floorf( ( cameraX + forwardX * cell ) / cell );
	const int32_t lz = ( int32_t )floorf( ( cameraZ + forwardZ * cell ) / cell );
	const uint8_t view =
	    ( uint8_t )( ( ( int32_t )floorf( atan2f( forwardZ, forwardX ) * ( 4.0f / 3.14159265358979323846f ) + 0.5f ) ) & 7 );
	// Request current and look-ahead bundles first; cache miss is always a
	// nonblocking no-op in this render frame.
	m_v1Demand.requestCellHot( x, z, 255u );
	if ( lx != x || lz != z )
		m_v1Demand.requestCellHot( lx, lz, 128u );
	const uint32_t limit = MIN( 8u, m_nFrameRequestBudget / 4u );
	if ( !limit )
		return 0u;
	const float fullMin[2] = { 0.0f, 0.0f };
	const float fullMax[2] = { 1.0f, 1.0f };
	uint32_t staged = 0u;
	for ( uint32_t sector = 0u; sector < 2u && staged < limit; ++sector ) {
		if ( sector && lx == x && lz == z )
			break;
		uint32_t count = 0u, flags = 0u;
		const t3dVTDemandBakedVisibilityRecord_t *const entries = m_v1Demand.findCellHot( sector ? lx : x, sector ? lz : z, count, flags );
		if ( !entries )
			continue;
		// Top eight scores only. A v1 bundle is sorted by stable key, not
		// priority, and its truncated PARTIAL records need reactive fallback.
		struct Candidate
		{
			uint32_t id, score, allocation;
			uint16_t mip;
		};
		Candidate top[8]{};
		uint32_t selected = 0u;
		const uint32_t scanCount = MIN( count, 128u );
		for ( uint32_t i = 0u; i < scanCount; ++i ) {
			const auto& r = entries[i];
			if ( r.m_nViewBin != view || ( r.m_nFlags & T3D_VT_VISIBILITY_NO_VISIBLE ) != 0u )
				continue;
			uint32_t low = 0u, high = ( uint32_t )m_v1MaterialKeys.size();
			while ( low < high ) {
				const uint32_t mid = low + ( high - low ) / 2u;
				if ( m_v1MaterialKeys[mid] < r.m_nStableMaterialKey )
					low = mid + 1u;
				else
					high = mid;
			}
			if ( low >= m_v1MaterialKeys.size() || m_v1MaterialKeys[low] != r.m_nStableMaterialKey )
				continue;
			const uint32_t allocationId = m_v1MaterialAllocations[low];
			if ( allocationId >= m_Residency.nAllocationCount )
				continue;
			const t3dVTAllocation_t& a = m_Residency.Buffers.pAllocations[allocationId];
			if ( !a.m_nPageCount || !a.m_nMipCount )
				continue;
			const uint32_t sourceWidth = MAX( 1u, ( uint32_t )a.m_nMip0WidthPages ) * T3D_VT_PAGE_INTERIOR;
			const uint16_t wanted = t3d_vt_demand_select_mip( r, m_v1Demand.configCold(), sourceWidth, viewHeight, fov, a.m_nMipCount );
			if ( wanted == UINT16_MAX )
				continue;
			// Ignore single-bin outliers and use confidence as the principal
			// score. Current-cell demand wins ties over speculative lookahead.
			const uint32_t score = ( uint32_t )r.m_nCoverageConfidence + MIN( 16383u, r.m_nSampleCount / 4u );
			Candidate candidate = { i, score, allocationId, wanted };
			uint32_t pos = 0u;
			while ( pos < selected && top[pos].score >= score )
				++pos;
			if ( pos >= 8u )
				continue;
			if ( selected < 8u )
				++selected;
			for ( uint32_t j = selected - 1u; j > pos; --j )
				top[j] = top[j - 1u];
			top[pos] = candidate;
		}
		for ( uint32_t i = 0u; i < selected && staged < limit; ++i ) {
			const t3dVTAllocation_t& a = m_Residency.Buffers.pAllocations[top[i].allocation];
			// Material histogram provides no UV page occupancy. At most a
			// 2x2 coarse footprint is prefetched, leaving all fine pages to
			// precise fragment feedback / the T05 occupancy feature.
			uint32_t coarse = 0u;
			uint32_t width = a.m_nMip0WidthPages;
			while ( width > 2u && coarse + 1u < a.m_nMipCount ) {
				width = ( width + 1u ) >> 1u;
				++coarse;
			}
			const uint32_t actualMip = MAX( ( uint32_t )top[i].mip, coarse );
			const uint32_t qualityBase = t3d_vt_allocation_quality_base_mip( a.m_nFlags );
			const uint16_t relativeMip = ( uint16_t )( actualMip > qualityBase ? actualMip - qualityBase : 0u );
			const uint16_t priority =
			    ( uint16_t )MIN( 65535u, ( sector ? T3D_VT_PRIORITY_NORMAL_MIN : T3D_VT_PRIORITY_NEAR_MIN ) + top[i].score / 8u );
			staged += stage_material_footprint_hot( top[i].allocation, frame, fullMin, fullMax, relativeMip, MIN( 2u, limit - staged ),
			                                        priority, T3D_VT_REQUEST_PRODUCER_TERRAIN, frame / 3u + sector, NULL );
		}
		m_nV1HistogramRecords += selected;
	}
	m_nV1PrefetchRequests += staged;
	return staged;
}

// AVT-03: the MegaVT source region key already resolves to a V1 allocation.
// The cache is read only by the render hot path and is rebuilt after each
// successful package swap. A zero means no cooked region is available.
void Terrain3DVirtualTexture::rebuildFarRegionAllocationTableCold()
{
	memset( m_szFarRegionAllocations, 0, sizeof( m_szFarRegionAllocations ) );
	memset( m_szComposedRegionAllocations, 0, sizeof( m_szComposedRegionAllocations ) );
	memset( m_szFarLastMip, 0xff, sizeof( m_szFarLastMip ) );
	memset( m_szFarLastMipFrame, 0, sizeof( m_szFarLastMipFrame ) );
	m_nFarRegionLastStageFrame = UINT32_MAX;
	m_nRegionalCameraFrame = UINT32_MAX;
	if ( !m_bRuntimePackLoaded || m_RuntimePack.PackViewV1.m_pHeader == NULL || m_RuntimePack.PackViewV1.m_pAllocationKeys == NULL ||
	     m_RuntimePack.PackViewV1.m_pAllocations == NULL )
		return;
	const t3dVTPackBaselineView_t& view = m_RuntimePack.PackViewV1;
	for ( uint32_t i = 0u; i < view.m_pHeader->m_nAllocationCount; ++i ) {
		const uint64_t key = view.m_pAllocationKeys[i].m_nStableKey;
		const bool composed = t3d_vt_is_composed_region_stable_key( key );
		if ( !composed && !t3d_vt_is_terrain_region_stable_key( key ) )
			continue;
		const uint32_t region = ( uint32_t )( key & 0xffffu );
		if ( region >= T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY )
			continue;
		const uint32_t source = view.m_pAllocationKeys[i].m_nAllocationId;
		if ( source >= view.m_pHeader->m_nAllocationCount )
			continue;
		if ( view.m_pAllocations[source].m_nConsumer != ( composed ? T3D_VT_CONSUMER_COMPOSED : T3D_VT_CONSUMER_CLIPMAP ) )
			continue;
		const uint32_t allocation = t3d_vt_runtime_allocation_base( source );
		if ( allocation + 2u >= m_Residency.nAllocationCount && m_bInitialized )
			continue;
		// Tag with +1; zero is an absent package allocation. All physical
		// slots, page table, upload budgets and eviction remain V1-owned.
		( composed ? m_szComposedRegionAllocations : m_szFarRegionAllocations )[region] = allocation + 1u;
	}
}

// Adaptively prefetch the already cooked regional page hierarchy, through
// the same staged-request heap and residency/eviction path as source materials.
uint32_t Terrain3DVirtualTexture::stage_region_surface_demand_hot( const float cameraX, const float cameraZ, const float forwardX,
                                                                   const float forwardZ, const uint32_t frame, const float fov,
                                                                   const uint32_t viewHeight )
{
	if ( !m_bInitialized || !m_Streaming.bInitialized || !m_Residency.pDatabase || m_bWarmPackagePending || !viewHeight ||
	     m_nFarRegionLastStageFrame == frame )
		return 0u;
	const float cell = ( float )T3D_REGION_WORLD_SIZE_METERS;
	// A teleport must not wait for the next quarter-frame prediction tick.
	// All state is fixed-size and updated on the render lane; page admission
	// and physical uploads remain governed by the existing VT budgets.
	const bool bTeleport = m_nRegionalCameraFrame != UINT32_MAX &&
	                       ( fabsf( cameraX - m_fRegionalCameraX ) > cell * 1.5f || fabsf( cameraZ - m_fRegionalCameraZ ) > cell * 1.5f );
	m_nRegionalCameraFrame = frame;
	m_fRegionalCameraX = cameraX;
	m_fRegionalCameraZ = cameraZ;
	if ( ( frame & 3u ) != 0u && !bTeleport )
		return 0u;
	if ( bTeleport ) {
		// Old near-mip hysteresis belongs to the previous camera locality.
		memset( m_szFarLastMip, 0xff, sizeof( m_szFarLastMip ) );
		memset( m_szFarLastMipFrame, 0, sizeof( m_szFarLastMipFrame ) );
	}
	m_nFarRegionLastStageFrame = frame;
	const int32_t regionsX[2] = { ( int32_t )floorf( cameraX / cell ), ( int32_t )floorf( ( cameraX + forwardX * cell ) / cell ) };
	const int32_t regionsZ[2] = { ( int32_t )floorf( cameraZ / cell ), ( int32_t )floorf( ( cameraZ + forwardZ * cell ) / cell ) };
	const float radians = CLAMP( fov, 30.0f, 140.0f ) * 0.00872664625997f;
	const float focal = ( float )viewHeight / ( 2.0f * tanf( radians ) );
	const float uvMin[2] = { 0.0f, 0.0f };
	const float uvMax[2] = { 1.0f, 1.0f };
	const uint32_t limit = MIN( 12u, MAX( 1u, m_nFrameRequestBudget / 6u ) );
	uint32_t attempted = 0u;
	for ( uint32_t sector = 0u; sector < 2u && attempted < limit; ++sector ) {
		if ( sector != 0u && regionsX[0] == regionsX[1] && regionsZ[0] == regionsZ[1] )
			break;
		for ( int32_t z = -1; z <= 1 && attempted < limit; ++z ) {
			for ( int32_t x = -1; x <= 1 && attempted < limit; ++x ) {
				const int32_t rx = regionsX[sector] + x;
				const int32_t rz = regionsZ[sector] + z;
				if ( rx < -T3D_REGION_MAP_HALF || rz < -T3D_REGION_MAP_HALF || rx >= T3D_REGION_MAP_HALF || rz >= T3D_REGION_MAP_HALF )
					continue;
				const uint32_t region =
				    ( uint32_t )( rz + T3D_REGION_MAP_HALF ) * T3D_REGION_MAP_SIZE + ( uint32_t )( rx + T3D_REGION_MAP_HALF );
				if ( !m_szComposedRegionAllocations[region] && ( m_bComposedOnlyRegionalDemand || !m_szFarRegionAllocations[region] ) )
					continue;
				const float dx = ( ( float )rx + 0.5f ) * cell - cameraX;
				const float dz = ( ( float )rz + 0.5f ) * cell - cameraZ;
				const float distance = MAX( 16.0f, sqrtf( dx * dx + dz * dz ) - cell * 0.5f );
				const uint32_t priority = ( sector == 0u && x == 0 && z == 0 ) ? T3D_VT_PRIORITY_NEAR_MIN : T3D_VT_PRIORITY_PREFETCH_MIN;
				// One V1 admission queue and LRU. Prefer already-authored
				// composed pages, then the regional source and its height data.
				for ( uint32_t family = 0u; family < 2u && attempted < limit; ++family ) {
					const uint32_t tagged = family == 0u ? m_szComposedRegionAllocations[region] : m_szFarRegionAllocations[region];
					if ( !tagged )
						continue;
					const uint32_t base = tagged - 1u;
					if ( base + 2u >= m_Residency.nAllocationCount )
						continue;
					for ( uint32_t slot = 0u; slot < T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION && attempted < limit; ++slot ) {
						const uint32_t id = base + slot;
						const t3dVTAllocation_t& alloc = m_Residency.Buffers.pAllocations[id];
						if ( !alloc.m_nPageCount || !alloc.m_nMipCount )
							continue;
						const float texels = ( float )alloc.m_nMip0WidthPages * T3D_VT_PAGE_INTERIOR;
						const float projected = MAX( 1.0f, cell * focal / distance );
						uint16_t wanted = 0u;
						for ( float ratio = texels / projected; ratio > 2.0f && wanted + 1u < alloc.m_nMipCount; ratio *= 0.5f )
							++wanted;
						const uint32_t historySlot = ( region * 3u + family ) * T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION + slot;
						const uint8_t old = m_szFarLastMip[historySlot];
						if ( old != 0xffu && wanted > old && frame - m_szFarLastMipFrame[historySlot] < 16u )
							wanted = old;
						if ( wanted != old )
							m_szFarLastMipFrame[historySlot] = frame;
						m_szFarLastMip[historySlot] = ( uint8_t )wanted;
						const uint32_t qualityBase = t3d_vt_allocation_quality_base_mip( alloc.m_nFlags );
						const uint16_t relative = wanted > qualityBase ? ( uint16_t )( wanted - qualityBase ) : 0u;
						attempted += stage_material_footprint_hot( id, frame, uvMin, uvMax, relative, MIN( 2u, limit - attempted ),
						                                           ( uint16_t )priority, T3D_VT_REQUEST_PRODUCER_TERRAIN, frame >> 2u );
					}
				}
			}
		}
	}
	m_nFarRegionRequests += attempted;
	return attempted;
}

uint32_t Terrain3DVirtualTexture::stage_spatial_demand_hot( const float fCameraX, const float fCameraZ, const float fForwardX,
                                                            const float fForwardZ, const uint32_t nFrameId, const float fFovDegrees,
                                                            const uint32_t nViewportHeight )
{
	T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_PREDICTION );
	// V1 demand and V2 presence are separate bounded producers. V1 cannot
	// disable normal fragment feedback or V2's periodic correction lane.
	const uint32_t v1Requests = stage_v1_demand_hot( fCameraX, fCameraZ, fForwardX, fForwardZ, nFrameId, fFovDegrees, nViewportHeight );
	const uint32_t farRequests =
	    stage_region_surface_demand_hot( fCameraX, fCameraZ, fForwardX, fForwardZ, nFrameId, fFovDegrees, nViewportHeight );
	const t3dVTDatabaseView_t *const db = m_Residency.pDatabase;
	if ( !m_bInitialized || !m_Streaming.bInitialized || db == NULL || !m_spatialDemand.isValidFor( db->nPackageContentSignature ) ||
	     m_nSpatialDemandLastStageFrame == nFrameId ) {
		return v1Requests + farRequests;
	}
	if ( m_spatialDemand.getVersion() == T3D_VT_DEMAND_STABLE_MATERIAL_FORMAT_VERSION && !m_spatialDemand.hasBoundPredictor() ) {
		return v1Requests + farRequests;
	}
	const float cell = ( float )m_spatialDemand.getCellMeters();
	const int32_t x = ( int32_t )floorf( fCameraX / cell );
	const int32_t z = ( int32_t )floorf( fCameraZ / cell );
	const float octants = atan2f( fForwardZ, fForwardX ) * ( 4.0f / 3.14159265358979323846f );
	const uint8_t mask = ( uint8_t )( 1u << ( ( ( int32_t )floorf( octants + 0.5f ) ) & 7 ) );
	// Reuse the last predictive window while the camera stays in the same
	// coverage cell/octant. Refresh every fourth frame for residency touches.
	if ( m_nSpatialDemandLastCellX == x && m_nSpatialDemandLastCellZ == z && m_nSpatialDemandLastOctant == mask &&
	     ( nFrameId & 3u ) != 0u ) {
		return v1Requests + farRequests;
	}
	m_nSpatialDemandLastCellX = x;
	m_nSpatialDemandLastCellZ = z;
	m_nSpatialDemandLastOctant = mask;
	m_nSpatialDemandLastStageFrame = nFrameId;
	const int32_t lookX = ( int32_t )floorf( ( fCameraX + fForwardX * cell ) / cell );
	const int32_t lookZ = ( int32_t )floorf( ( fCameraZ + fForwardZ * cell ) / cell );
	const int32_t cx[2] = { x, lookX };
	const int32_t cz[2] = { z, lookZ };
	const uint32_t cap = MIN( 48u, MAX( 1u, m_nFrameRequestBudget / 2u ) );
	uint32_t attempted = 0u;
	m_nSpatialDemandCoveredAllocationCount = 0u;
	m_bSpatialDemandHeapDisplaced = false;
	bool currentCellComplete = true;
	uint32_t currentCellRecords = 0u;
	for ( uint32_t sector = 0u; sector < 2u; ++sector ) {
		if ( sector == 1u && ( attempted >= cap || ( cx[0] == cx[1] && cz[0] == cz[1] ) ) ) {
			break;
		}
		uint32_t count = 0u;
		const t3dVTDemandRecord_t *const records = m_spatialDemand.findCellHot( cx[sector], cz[sector], count );
		for ( uint32_t i = 0u; records && i < count; ++i ) {
			const t3dVTDemandRecord_t& item = records[i];
			if ( ( item.nViewMask & mask ) == 0u ) {
				continue;
			}
			uint32_t tracked = UINT32_MAX;
			if ( sector == 0u ) {
				++currentCellRecords;
				if ( item.nAllocation < m_Residency.nAllocationCount ) {
					for ( uint32_t j = 0u; j < m_nSpatialDemandCoveredAllocationCount; ++j ) {
						if ( m_szSpatialDemandCoveredAllocations[j] == item.nAllocation ) {
							tracked = j;
							break;
						}
					}
					if ( tracked == UINT32_MAX && m_nSpatialDemandCoveredAllocationCount < T3D_RD_MATERIAL_LAYER_COUNT ) {
						tracked = m_nSpatialDemandCoveredAllocationCount++;
						m_szSpatialDemandCoveredAllocations[tracked] = item.nAllocation;
						m_szSpatialDemandAllocationComplete[tracked] = 1u;
					}
				}
				if ( tracked == UINT32_MAX || item.nReserved1 != 0u ) {
					currentCellComplete = false;
					if ( tracked != UINT32_MAX ) {
						m_szSpatialDemandAllocationComplete[tracked] = 0u;
					}
				}
			}
			if ( item.nAllocation >= m_Residency.nAllocationCount ) {
				if ( sector == 0u ) {
					currentCellComplete = false;
				}
				continue;
			}
			const t3dVTAllocation_t& a = m_Residency.Buffers.pAllocations[item.nAllocation];
			if ( a.m_nPageCount == 0u || a.m_nMipCount == 0u || attempted >= cap ) {
				if ( sector == 0u ) {
					currentCellComplete = false;
					if ( tracked != UINT32_MAX ) {
						m_szSpatialDemandAllocationComplete[tracked] = 0u;
					}
				}
				continue;
			}
			const float uvMin[2] = { ( float )item.nUvMinX / 65535.0f, ( float )item.nUvMinY / 65535.0f };
			const float uvMax[2] = { ( float )item.nUvMaxX / 65535.0f, ( float )item.nUvMaxY / 65535.0f };
			const uint16_t priority = sector == 0u ? ( uint16_t )MIN( 65535u, T3D_VT_PRIORITY_NEAR_MIN + ( uint32_t )item.nCoverage / 8u )
			                                       : ( uint16_t )( T3D_VT_PRIORITY_NORMAL_MIN + ( uint32_t )item.nCoverage / 32u );
			bool fullyStaged = false;
			// A baked record does not prove that its requests fit this frame's
			// admission budget. Only a complete current-cell sweep may suppress
			// reactive demand or fragment feedback for covered terrain.
			attempted += stage_material_footprint_hot( item.nAllocation, nFrameId, uvMin, uvMax, item.nMip, cap - attempted, priority,
			                                           T3D_VT_REQUEST_PRODUCER_TERRAIN, nFrameId / 3u + sector, &fullyStaged );
			if ( sector == 0u && ( !fullyStaged || item.nReserved1 != 0u ) ) {
				currentCellComplete = false;
				if ( tracked != UINT32_MAX ) {
					m_szSpatialDemandAllocationComplete[tracked] = 0u;
				}
			}
		}
	}
	if ( m_bSpatialDemandHeapDisplaced ) {
		// A displaced page can belong to an earlier complete allocation;
		// drop all suppression proofs for this frame rather than guessing.
		currentCellComplete = false;
		m_nSpatialDemandCoveredAllocationCount = 0u;
	}
	m_bSpatialDemandCurrentCellComplete = currentCellRecords != 0u && currentCellComplete;
	if ( currentCellRecords != 0u && !m_bSpatialDemandCurrentCellComplete ) {
		++m_nSpatialDemandIncompleteFallbacks;
	}
	m_nSpatialDemandAttempts += attempted;
	return attempted + v1Requests + farRequests;
}

bool Terrain3DVirtualTexture::shouldScanTerrainAllocationHot( const float x, const float z, const uint32_t nAllocation,
                                                              const uint32_t nFrameId )
{
	// Histogram material/mip visibility has no virtual-page coverage proof.
	// Until T05/T07, the reactive VT discovery path must remain authoritative.
	if ( m_Residency.pDatabase && m_v1Demand.validFor( m_Residency.pDatabase->nPackageContentSignature ) )
		return true;
	if ( !m_spatialDemand.hasBoundPredictor() || m_nSpatialDemandCoveredAllocationCount == 0u || m_Residency.pDatabase == NULL ||
	     !m_spatialDemand.isValidFor( m_Residency.pDatabase->nPackageContentSignature ) ||
	     ( m_nSpatialDemandLastStageFrame == UINT32_MAX || ( uint32_t )( nFrameId - m_nSpatialDemandLastStageFrame ) > 3u ) ||
	     ( nFrameId & 3u ) == 0u ) {
		return true;
	}
	const float cell = ( float )m_spatialDemand.getCellMeters();
	const int32_t cellX = ( int32_t )floorf( x / cell );
	const int32_t cellZ = ( int32_t )floorf( z / cell );
	if ( cellX != m_nSpatialDemandLastCellX || cellZ != m_nSpatialDemandLastCellZ ) {
		return true;
	}
	for ( uint32_t i = 0u; i < m_nSpatialDemandCoveredAllocationCount; ++i ) {
		if ( m_szSpatialDemandCoveredAllocations[i] == nAllocation ) {
			return m_szSpatialDemandAllocationComplete[i] == 0u;
		}
	}
	return true;
}

bool Terrain3DVirtualTexture::prepare_cpu_cold( const t3dWorldLimits_t& pLimits, const t3dVTGpuResourcePlan_t& pPlan,
                                                const uint32_t nStartupGeneration )
{
	if ( m_bInitialized || m_bCpuPrepared || m_pRuntimeAllocatorZone == NULL || !t3d_world_validate_limits( pLimits ) ) {
		return false;
	}
	if ( nStartupGeneration != 0u ) {
		set_startup_generation( nStartupGeneration );
	}
	if ( !m_bStreamingLimitsConfigured ) {
		m_StreamingLimits.nMaxUploadBytesPerFrame = pLimits.nMaxUploadBytesPerFrame;
		m_StreamingLimits.nMaxPageSetsPerFrame = pLimits.nMaxStreamJobs < 8u ? pLimits.nMaxStreamJobs : 8u;
		m_StreamingLimits.nMaxInFlightIoJobs = pLimits.nStreamScratchLaneCount < 4u ? pLimits.nStreamScratchLaneCount : 4u;
		m_StreamingLimits.nMaxAdmittedRequestsPerFrame = pLimits.nMaxStreamJobs < 64u ? pLimits.nMaxStreamJobs : 64u;
	}
	if ( m_StreamingLimits.nMaxUploadBytesPerFrame == 0u || m_StreamingLimits.nMaxUploadBytesPerFrame > pLimits.nMaxUploadBytesPerFrame ||
	     m_StreamingLimits.nMaxPageSetsPerFrame == 0u || m_StreamingLimits.nMaxPageSetsPerFrame > pLimits.nMaxStreamJobs ||
	     m_StreamingLimits.nMaxInFlightIoJobs == 0u || m_StreamingLimits.nMaxInFlightIoJobs > pLimits.nStreamScratchLaneCount ||
	     m_StreamingLimits.nMaxAdmittedRequestsPerFrame == 0u || m_StreamingLimits.nMaxAdmittedRequestsPerFrame > pLimits.nMaxStreamJobs ) {
		return false;
	}
	m_Limits = pLimits;
	m_nFrameRequestBudget = MAX( 1u, pLimits.nMaxVtRequests );
	const bool bTrackStartup = m_nStartupGeneration != 0u && t3dStartupDomainGetStage( m_startupState ) != T3D_STARTUP_COMMITTED;
	const uint64_t nCpuStartedUsec = t3dVtTicksUsec();
	if ( bTrackStartup ) {
		t3dStartupDomainSetStartUsecIfZero( m_startupState, nCpuStartedUsec );
		t3dStartupDomainSetTotalUnits( m_startupState, 4u );
		t3dStartupDomainSetStage( m_startupState, T3D_STARTUP_PREPARING );
	}
	++m_nStartupCpuImagePrepareCount;
	const bool bCpuReady =
	    allocate_cpu_image_cold() && ( m_bRuntimePackLoaded ? prepare_runtime_package_cpu_cold() : prepare_neutral_legacy_cpu_cold() ) &&
	    m_Gpu.prepare_cpu_cold( pPlan, pLimits ) &&
	    ( !m_bRuntimePackLoaded || m_RuntimePack.Database.pFileSource == NULL || m_Gpu.prepare_file_tails_cold( m_RuntimePack.Database ) );
	m_nStartupCpuImagePrepareUsecLast = t3dVtTicksUsec() - nCpuStartedUsec;
	godot::UtilityFunctions::print( "[Terrain3D][Startup][VT] CPU PREP generation=", m_nStartupGeneration, " bytes=", m_nCpuImageBytes,
	                                " elapsed=", m_nStartupCpuImagePrepareUsecLast, " us ready=", bCpuReady );
	if ( !bCpuReady ) {
		if ( bTrackStartup ) {
			mark_startup_failed( m_nStartupGeneration );
		}
		shutdown_cold();
		return false;
	}
	// Persistent dispatch and fixed scratch lanes are prepared before the hot seal.
	if ( !m_streamWorker.joinable() ) {
		m_bStreamWorkerStop = false;
		m_nStreamWorkerHead = 0u;
		m_nStreamWorkerCount = 0u;
		m_streamWorker = Terrain3DThread( &Terrain3DVirtualTexture::streamWorkerMain, this );
	}
	m_bCpuPrepared = true;
	if ( bTrackStartup ) {
		t3dStartupDomainSetCompletedUnits( m_startupState, 1u );
		t3dStartupDomainSetStage( m_startupState, T3D_STARTUP_CPU_READY );
		godot::UtilityFunctions::print( "[Terrain3D][Startup][VT] CPU READY generation=", m_nStartupGeneration );
	}
	return true;
}

bool Terrain3DVirtualTexture::prepare_gpu_storage_cold( Terrain3DVTResourceImage *const pResources )
{
	if ( !m_bCpuPrepared || m_bGpuStoragePrepared || pResources == NULL || !pResources->is_initialized() ) {
		return false;
	}
	m_pResources = pResources;
	const bool bTrackStartup = m_nStartupGeneration != 0u && t3dStartupDomainGetStage( m_startupState ) != T3D_STARTUP_COMMITTED;
	const uint64_t nStartedUsec = t3dVtTicksUsec();
	++m_nStartupGpuStoragePrepareCount;
	const bool bReady = m_Gpu.prepare_storage_cold( pResources, m_Limits );
	m_nStartupGpuStoragePrepareUsecLast = t3dVtTicksUsec() - nStartedUsec;
	godot::UtilityFunctions::print( "[Terrain3D][Startup][VT] GPU STORAGE generation=", m_nStartupGeneration,
	                                " elapsed=", m_nStartupGpuStoragePrepareUsecLast, " us ready=", bReady );
	if ( !bReady ) {
		if ( bTrackStartup )
			mark_startup_failed( m_nStartupGeneration );
		shutdown_cold();
		return false;
	}
	m_bGpuStoragePrepared = true;
	if ( bTrackStartup ) {
		t3dStartupDomainSetCompletedUnits( m_startupState, 2u );
		t3dStartupDomainSetStage( m_startupState, T3D_STARTUP_GPU_PENDING );
		godot::UtilityFunctions::print( "[Terrain3D][Startup][VT] STORAGE READY generation=", m_nStartupGeneration );
	}
	return true;
}

bool Terrain3DVirtualTexture::bind_gpu_executable_cold()
{
	while ( !m_bExecutableBound ) {
		if ( !service_gpu_executable_cold( UINT32_MAX ) ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DVirtualTexture::service_gpu_executable_cold( const uint32_t nMaxWorkUnits )
{
	if ( !m_bGpuStoragePrepared || m_bInitialPublished || nMaxWorkUnits == 0u ) {
		return false;
	}
	if ( m_bExecutableBound ) {
		return true;
	}
	const bool bTrackStartup = m_nStartupGeneration != 0u && t3dStartupDomainGetStage( m_startupState ) != T3D_STARTUP_COMMITTED;
	const uint64_t nStartedUsec = t3dVtTicksUsec();
	++m_nStartupGpuBindCount;
	++m_nStartupGpuInitializeCount;
	const bool bReady = m_Gpu.service_bind_executable_cold( nMaxWorkUnits );
	m_nStartupGpuBindUsecLast += t3dVtTicksUsec() - nStartedUsec;
	m_nStartupGpuInitializeUsecLast = m_nStartupGpuStoragePrepareUsecLast + m_nStartupGpuBindUsecLast;
	if ( !bReady ) {
		if ( bTrackStartup )
			mark_startup_failed( m_nStartupGeneration );
		shutdown_cold();
		return false;
	}
	if ( m_Gpu.is_executable_ready() ) {
		m_bExecutableBound = true;
		if ( bTrackStartup ) {
			t3dStartupDomainSetCompletedUnits( m_startupState, 3u );
			t3dStartupDomainSetStage( m_startupState, T3D_STARTUP_GPU_READY );
			godot::UtilityFunctions::print( "[Terrain3D][Startup][VT] GPU READY generation=", m_nStartupGeneration );
		}
	}
	return true;
}

bool Terrain3DVirtualTexture::publish_initial_cold()
{
	while ( !m_bInitialPublished ) {
		if ( !service_initial_publication_cold( UINT64_MAX, UINT32_MAX ) ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DVirtualTexture::service_initial_publication_cold( const uint64_t nMaxBytes, const uint32_t nMaxWorkUnits )
{
	if ( !m_bCpuPrepared || !m_bGpuStoragePrepared || !m_bExecutableBound || m_pResources == NULL || nMaxBytes == 0u ||
	     nMaxWorkUnits == 0u ) {
		return false;
	}
	if ( m_bInitialPublished ) {
		return true;
	}
	const bool bTrackStartup = m_nStartupGeneration != 0u && t3dStartupDomainGetStage( m_startupState ) != T3D_STARTUP_COMMITTED;
	const uint64_t nPublicationStartedUsec = t3dVtTicksUsec();
	if ( !m_bInitialPublicationStarted ) {
		++m_nStartupInitialPublicationCount;
		m_bInitialPublicationStarted = true;
	}
	const t3dVTDatabaseView_t& Database = m_bRuntimePackLoaded ? m_RuntimePack.Database : m_NeutralDatabase;
	if ( !m_Gpu.service_database_upload_warm( Database, m_Residency, nMaxBytes, nMaxWorkUnits ) ) {
		if ( bTrackStartup )
			mark_startup_failed( m_nStartupGeneration );
		shutdown_cold();
		return false;
	}
	if ( m_Gpu.is_database_upload_pending_warm() ) {
		m_nStartupInitialPublicationUsecLast += t3dVtTicksUsec() - nPublicationStartedUsec;
		return true;
	}
	bool bPublicationReady = true;
	if ( m_bRuntimePackLoaded ) {
		bPublicationReady = t3d_vt_finalize_package_publication_warm( m_Residency );
	} else if ( !m_Residency.bNeutralPublished ) {
		bPublicationReady =
		    m_Gpu.upload_neutral_page_set_warm( m_Residency, 0u ) && t3d_vt_finalize_neutral_publication_warm( m_Residency );
	}
	m_nStartupInitialPublicationUsecLast += t3dVtTicksUsec() - nPublicationStartedUsec;
	if ( !bPublicationReady ) {
		if ( bTrackStartup )
			mark_startup_failed( m_nStartupGeneration );
		shutdown_cold();
		return false;
	}
	m_bInitialPublished = true;
	++m_nPackagePublicationGeneration;
	m_Stats.nPhysicalPageSetCapacity = m_Limits.nMaxVtPhysicalPageSets;
	m_Stats.nPhysicalCacheAllocationEvents = count_physical_banks();
	m_Stats.nCpuImageBytes = m_nCpuImageBytes;
	m_nTelemetryStartUsec = godot::Time::get_singleton() != NULL ? godot::Time::get_singleton()->get_ticks_usec() : 0u;
	m_nTelemetryLastUploadedPageSets = m_Streaming.Stats.nUploadedPageCount;
	m_nTelemetryLastEvictions = m_Residency.Stats.nEvictionCount;
	m_nTelemetryUploadPageSetsFrame = 0u;
	m_nTelemetryUploadBytesFrame = 0u;
	m_nTelemetryEvictionsFrame = 0u;
	m_nResolverDepth0Samples = 0u;
	m_nResolverDepth1Samples = 0u;
	m_nResolverDepth2To3Samples = 0u;
	m_nResolverDepth4PlusSamples = 0u;
	m_nResolverTailSamples = 0u;
	m_nResolverTelemetrySampleFrame = UINT32_MAX;
	m_nResolverTelemetrySamplesThisFrame = 0u;
	m_nUploadCadenceStableFrames = 0u;
	m_nUploadCadenceLastPageMisses = m_Streaming.Stats.nPageMissCount;
	m_nUploadCadenceLastEvictions = m_Residency.Stats.nEvictionCount;
	m_nUploadCadenceLastThrash = m_Residency.Stats.nThrashCount;
	++m_Stats.nInitializeCount;
	m_bInitialized = true;
	if ( bTrackStartup ) {
		t3dStartupDomainSetCompletedUnits( m_startupState, 4u );
		t3dStartupDomainFinish( m_startupState, T3D_STARTUP_COMMITTED, t3dVtTicksUsec() );
	}
	godot::UtilityFunctions::print( "[Terrain3D][Startup][VT] INITIAL PUBLICATION generation=", m_nStartupGeneration,
	                                " elapsed=", m_nStartupInitialPublicationUsecLast, " us ready=true" );
	return true;
}

void Terrain3DVirtualTexture::shutdown_cold( const bool bPreserveTailStaging )
{
	m_v1Demand.resetCold();
	m_v1MaterialKeys.clear();
	m_v1MaterialAllocations.clear();
	m_v1DemandPath = godot::String();
	m_nV1AssetIdentityHash = 0u;
	m_nV1LastStageFrame = UINT32_MAX;
	m_nSpatialDemandLastStageFrame = UINT32_MAX;
	m_bSpatialDemandCurrentCellComplete = false;
	m_nSpatialDemandCoveredAllocationCount = 0u;
	// Cold teardown may join; the render service only polls lane completion.
	// Keep the old database, job records and scratch image alive until this drain.
	stopStreamWorkerCold();
	abortPackageRepublishWarm();
	m_bInitialPublicationStarted = false;
	m_bWarmRollbackReady = true;
	m_bWarmResourceRebuildRequired = false;
	m_Gpu.shutdown_cold( bPreserveTailStaging );
	m_multiFeedbackShadowStats = t3dVTFeedbackMultiShadowStats_t{};
	if ( m_bInitialized ) {
		++m_Stats.nShutdownCount;
	}
	m_bHotSealed = false;
	for ( uint32_t i = 0u; i < 3u; ++i ) {
		m_szDemandRefreshFrame[i] = UINT32_MAX;
	}
	m_bCpuPrepared = false;
	m_bGpuStoragePrepared = false;
	m_bExecutableBound = false;
	m_bInitialPublished = false;
	m_bInitialized = false;
	memset( &m_Residency, 0, sizeof( m_Residency ) );
	memset( &m_Streaming, 0, sizeof( m_Streaming ) );
	m_NeutralDatabase = t3dVTDatabaseView_t{};
	release_cpu_image_cold();
	release_runtime_pack_cold();
	m_pResources = NULL;
	m_nGpuHelperCreateEpoch = 0u;
	m_nTelemetryStartUsec = 0u;
	m_nTelemetryLastUploadedPageSets = 0u;
	m_nTelemetryLastEvictions = 0u;
	m_nTelemetryUploadPageSetsFrame = 0u;
	m_nTelemetryUploadBytesFrame = 0u;
	m_nTelemetryEvictionsFrame = 0u;
	m_nResolverDepth0Samples = 0u;
	m_nResolverDepth1Samples = 0u;
	m_nResolverDepth2To3Samples = 0u;
	m_nResolverDepth4PlusSamples = 0u;
	m_nResolverTailSamples = 0u;
	m_nResolverTelemetrySampleFrame = UINT32_MAX;
	m_nResolverTelemetrySamplesThisFrame = 0u;
	memset( &m_Limits, 0, sizeof( m_Limits ) );
}

void Terrain3DVirtualTexture::seal_hot_runtime()
{
	if ( !m_bInitialized || m_pResources == NULL ) {
		return;
	}
	m_Gpu.seal_hot_runtime();
	m_Stats.nPhysicalCacheCreateEpoch = m_pResources->get_stats().nGpuCreateCount;
	m_Stats.nPhysicalCacheFreeEpoch = m_pResources->get_stats().nGpuFreeCount;
	m_nGpuHelperCreateEpoch = m_Gpu.get_stats().nGpuCreateCount;
	m_bHotSealed = true;
}

bool Terrain3DVirtualTexture::validate_hot_plateau_reference_debug()
{
	if ( !m_bInitialized || !m_bHotSealed || m_pResources == NULL ) {
		return false;
	}
	if ( m_pResources->get_stats().nGpuCreateCount != m_Stats.nPhysicalCacheCreateEpoch ||
	     m_pResources->get_stats().nGpuFreeCount != m_Stats.nPhysicalCacheFreeEpoch ) {
		++m_Stats.nPhysicalCacheAllocationEventsAfterSeal;
		++m_Stats.nHotViolationCount;
		return false;
	}
	return true;
}

bool Terrain3DVirtualTexture::is_initialized() const
{
	return m_bInitialized;
}
bool Terrain3DVirtualTexture::is_cpu_ready() const
{
	return m_bCpuPrepared;
}
bool Terrain3DVirtualTexture::is_gpu_storage_ready() const
{
	return m_bGpuStoragePrepared;
}
bool Terrain3DVirtualTexture::is_gpu_ready() const
{
	return m_bExecutableBound;
}
bool Terrain3DVirtualTexture::is_initial_published() const
{
	return m_bInitialPublished;
}
bool Terrain3DVirtualTexture::is_hot_sealed() const
{
	return m_bHotSealed;
}
bool Terrain3DVirtualTexture::is_readback_enabled() const
{
	return m_Gpu.is_readback_enabled();
}
uint64_t Terrain3DVirtualTexture::get_cpu_image_bytes() const
{
	return m_nCpuImageBytes;
}

uint64_t Terrain3DVirtualTexture::get_gpu_reserved_bytes() const
{
	return m_Gpu.get_reserved_gpu_bytes();
}
uint32_t Terrain3DVirtualTexture::get_page_table_generation() const
{
	return m_Residency.nPageTableGeneration;
}

uint32_t Terrain3DVirtualTexture::getActiveAllocationCount() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	return m_Residency.pDatabase != NULL && m_Residency.pDatabase->pHeader != NULL ? m_Residency.pDatabase->pHeader->nAllocationCount : 0u;
}

uint32_t Terrain3DVirtualTexture::getActiveVirtualPageCount() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	return m_Residency.pDatabase != NULL && m_Residency.pDatabase->pHeader != NULL ? m_Residency.pDatabase->pHeader->nPageCount : 0u;
}

uint32_t Terrain3DVirtualTexture::getLoadedPackageAllocationCountCold() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	return m_bRuntimePackLoaded && m_RuntimePack.Database.pHeader != NULL ? m_RuntimePack.Database.pHeader->nAllocationCount : 0u;
}

uint32_t Terrain3DVirtualTexture::getLoadedPackageVirtualPageCountCold() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	return m_bRuntimePackLoaded && m_RuntimePack.Database.pHeader != NULL ? m_RuntimePack.Database.pHeader->nPageCount : 0u;
}

uint32_t Terrain3DVirtualTexture::getLoadedPackageTailAtlasHeightCold() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	if ( !m_bRuntimePackLoaded )
		return T3D_VT_TAIL_TILE;
	uint32_t nHeight = 0u;
	return t3d_vt_compute_active_tail_height_cold( m_RuntimePack.pRuntimeTails, m_RuntimePack.Database.nRuntimeTailCount, nHeight )
	           ? nHeight
			   : 0u;
}

uint32_t Terrain3DVirtualTexture::getDatabaseGeneration() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	return m_Residency.nDatabaseGeneration;
}

uint32_t Terrain3DVirtualTexture::getPackageGeneration() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	return m_Residency.pDatabase != NULL ? m_Residency.pDatabase->nPackageGeneration : 0u;
}

uint64_t Terrain3DVirtualTexture::getPackageContentSignature() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	return m_Residency.pDatabase != NULL ? m_Residency.pDatabase->nPackageContentSignature : 0u;
}

uint32_t Terrain3DVirtualTexture::getDatabaseKind() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	return m_Residency.pDatabase != NULL ? m_Residency.pDatabase->nRuntimeKind : T3D_VT_RUNTIME_DATABASE_LEGACY_NEUTRAL;
}

uint32_t Terrain3DVirtualTexture::getMaterialVTAllocationId( const uint32_t nMaterialId ) const
{
	if ( nMaterialId >= m_Limits.nMaxMaterials || !m_bRuntimePackLoaded ) {
		return T3D_VT_INVALID_ALLOCATION;
	}
	uint32_t nAllocationId = T3D_VT_INVALID_ALLOCATION;
	return findMaterialAllocationCold( t3d_vt_terrain_material_stable_key( nMaterialId ), nAllocationId ) ? nAllocationId
	                                                                                                      : T3D_VT_INVALID_ALLOCATION;
}

bool Terrain3DVirtualTexture::supportsSparseV1() const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	return m_bInitialized && m_Residency.pDatabase != NULL && m_Residency.pDatabase->nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK;
}

uint32_t Terrain3DVirtualTexture::getTerrainMegaVTAllocationBase() const
{
	return m_Limits.nMaxMaterials * T3D_VT_CHANNEL_COUNT;
}

bool Terrain3DVirtualTexture::getComposedRegionAllocationCold( const uint32_t nRegionMapIndex, uint32_t& rAllocation,
                                                               uint32_t& rGeneration ) const
{
	rAllocation = T3D_VT_INVALID_ALLOCATION;
	rGeneration = 0u;
	if ( nRegionMapIndex >= T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY )
		return false;
	Terrain3DScopedLock packageLock( m_packageMutex );
	if ( !m_bInitialized || !m_bRuntimePackLoaded || m_bWarmPackagePending ||
	     m_RuntimePack.Database.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK )
		return false;
	uint32_t source = T3D_VT_INVALID_ALLOCATION;
	if ( !t3d_vt_pack_runtime_find_allocation_by_stable_key( m_RuntimePack, t3d_vt_composed_region_stable_key( nRegionMapIndex ),
	                                                         source ) ||
	     m_RuntimePack.PackViewV1.m_pHeader == NULL || source >= m_RuntimePack.PackViewV1.m_pHeader->m_nAllocationCount )
		return false;
	const t3dVTPackAllocationRecord_t& allocation = m_RuntimePack.PackViewV1.m_pAllocations[source];
	if ( allocation.m_nConsumer != T3D_VT_CONSUMER_COMPOSED || allocation.m_nGeneration == 0u ||
	     ( allocation.m_nChannelMask & T3D_VT_COMPOSED_REQUIRED_CHANNEL_MASK ) != T3D_VT_COMPOSED_REQUIRED_CHANNEL_MASK )
		return false;
	const uint32_t runtimeAllocation = t3d_vt_runtime_allocation_base( source );
	if ( runtimeAllocation > m_Residency.nAllocationCount ||
	     m_Residency.nAllocationCount - runtimeAllocation < T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION )
		return false;
	rAllocation = runtimeAllocation;
	rGeneration = allocation.m_nGeneration;
	return true;
}

uint32_t Terrain3DVirtualTexture::getTerrainMegaVTAllocationId( const uint32_t nRegionMapIndex ) const
{
	if ( nRegionMapIndex >= T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY ) {
		return T3D_VT_INVALID_ALLOCATION;
	}
	uint32_t nAllocation = T3D_VT_INVALID_ALLOCATION;
	return findMaterialAllocationCold( t3d_vt_terrain_region_stable_key( nRegionMapIndex ), nAllocation )
	           ? nAllocation
			   : getTerrainMegaVTAllocationBase() + nRegionMapIndex * T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION;
}
bool Terrain3DVirtualTexture::getTerrainMegaVTCookedGenerationCold( const uint32_t nRegionMapIndex, uint32_t& rGeneration ) const
{
	Terrain3DScopedLock packageLock( m_packageMutex );
	uint32_t nAllocation = 0u;
	if ( !m_bRuntimePackLoaded || m_RuntimePack.Database.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK ||
	     !t3d_vt_pack_runtime_find_allocation_by_stable_key( m_RuntimePack, t3d_vt_terrain_region_stable_key( nRegionMapIndex ),
	                                                         nAllocation ) ) {
		return false;
	}
	const auto& allocation = m_RuntimePack.PackViewV1.m_pAllocations[nAllocation];
	if ( ( allocation.m_nFlags & T3D_VT_ALLOCATION_MEGATEXTURE ) == 0u || allocation.m_nGeneration == 0u ) {
		return false;
	}
	rGeneration = allocation.m_nGeneration;
	return true;
}
const t3dVTDatabaseView_t *Terrain3DVirtualTexture::get_database() const
{
	return m_Residency.pDatabase;
}
const Terrain3DVTResourceImage *Terrain3DVirtualTexture::get_resource_image() const
{
	return m_bInitialized ? m_pResources : NULL;
}
bool Terrain3DVirtualTexture::apply_quality_profile_state_v1( t3dVTAllocation_t *const pAllocations, const uint32_t nAllocationCount )
{
	if ( !m_bRuntimePackLoaded || m_RuntimePack.Database.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK ) {
		memset( &m_QualityStats, 0, sizeof( m_QualityStats ) );
		m_QualityStats.nRequestedProfile = m_nRequestedQualityProfile;
		return true;
	}
	return t3d_vt_pack_runtime_apply_quality_profile( m_RuntimePack.Database, pAllocations, nAllocationCount, m_nRequestedQualityProfile,
	                                                  m_QualityStats );
}

bool Terrain3DVirtualTexture::set_quality_profile( const uint32_t nProfile )
{
	if ( nProfile >= T3D_VT_QUALITY_PROFILE_COUNT ) {
		return false;
	}
	if ( nProfile == m_nRequestedQualityProfile && m_nQualityGeneration != 0u ) {
		return true;
	}

	const uint32_t nOldRequestedProfile = m_nRequestedQualityProfile;
	const t3dVTQualityRuntimeStats_t OldStats = m_QualityStats;
	m_nRequestedQualityProfile = nProfile;

	if ( !m_bRuntimePackLoaded || m_RuntimePack.Database.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK ) {
		memset( &m_QualityStats, 0, sizeof( m_QualityStats ) );
		m_QualityStats.nRequestedProfile = nProfile;
		++m_nQualityGeneration;
		return true;
	}

	const uint32_t nAllocationCount = m_RuntimePack.Database.pHeader->nAllocationCount;
	if ( !apply_quality_profile_state_v1( m_RuntimePack.pRuntimeAllocations, nAllocationCount ) ) {
		m_nRequestedQualityProfile = nOldRequestedProfile;
		m_QualityStats = OldStats;
		return false;
	}

	if ( m_bInitialized ) {
		memcpy( m_Residency.Buffers.pAllocations, m_RuntimePack.pRuntimeAllocations,
		        ( size_t )nAllocationCount * sizeof( t3dVTAllocation_t ) );
		if ( !m_Gpu.update_allocation_quality_hot( m_Residency ) ) {
			m_nRequestedQualityProfile = nOldRequestedProfile;
			m_QualityStats = OldStats;
			if ( apply_quality_profile_state_v1( m_RuntimePack.pRuntimeAllocations, nAllocationCount ) ) {
				memcpy( m_Residency.Buffers.pAllocations, m_RuntimePack.pRuntimeAllocations,
				        ( size_t )nAllocationCount * sizeof( t3dVTAllocation_t ) );
				m_Gpu.update_allocation_quality_hot( m_Residency );
			}
			m_QualityStats = OldStats;
			return false;
		}
		m_nStagedRequestCount = 0u;
		m_nStagedRequestFrame = UINT32_MAX;
		m_Stats.nQualityDemandResetCount += t3d_vt_streaming_invalidate_queued_demand_hot( m_Streaming );
	}
	++m_nQualityGeneration;
	return true;
}

uint32_t Terrain3DVirtualTexture::get_requested_quality_profile() const
{
	return m_nRequestedQualityProfile;
}
uint32_t Terrain3DVirtualTexture::get_package_fully_supported_profile_mask() const
{
	return m_QualityStats.nPackageFullySupportedProfileMask;
}
uint32_t Terrain3DVirtualTexture::get_quality_fallback_allocation_count() const
{
	return m_QualityStats.nFallbackAllocationCount;
}
uint32_t Terrain3DVirtualTexture::get_quality_generation() const
{
	return m_nQualityGeneration;
}
uint32_t Terrain3DVirtualTexture::get_allocations_by_highest_profile( const uint32_t nProfile ) const
{
	return nProfile < T3D_VT_QUALITY_PROFILE_COUNT ? m_QualityStats.nAllocationsByHighestProfile[nProfile] : 0u;
}
uint32_t Terrain3DVirtualTexture::get_allocations_by_resolved_profile( const uint32_t nProfile ) const
{
	return nProfile < T3D_VT_QUALITY_PROFILE_COUNT ? m_QualityStats.nAllocationsByResolvedProfile[nProfile] : 0u;
}

uint32_t Terrain3DVirtualTexture::getPhysicalFormatFamilyCountV1() const
{
	return T3D_VT_PHYSICAL_FORMAT_COUNT;
}
bool Terrain3DVirtualTexture::getPhysicalFormatFamilyBindingV1( const uint32_t nFamily, RID& rRid, uint32_t& rLayerCount ) const
{
	return m_bInitialized && m_Gpu.get_physical_family_binding_v1( nFamily, rRid, rLayerCount );
}
t3dVTResidencyState_t& Terrain3DVirtualTexture::get_residency()
{
	return m_Residency;
}
const t3dVTResidencyState_t& Terrain3DVirtualTexture::get_residency() const
{
	return m_Residency;
}
t3dVTStreamingState_t& Terrain3DVirtualTexture::get_streaming()
{
	return m_Streaming;
}
const t3dVTStreamingState_t& Terrain3DVirtualTexture::get_streaming() const
{
	return m_Streaming;
}
Terrain3DVTGpu& Terrain3DVirtualTexture::get_gpu()
{
	return m_Gpu;
}
const Terrain3DVTGpu& Terrain3DVirtualTexture::get_gpu() const
{
	return m_Gpu;
}
const t3dVirtualTextureStats_t& Terrain3DVirtualTexture::get_stats() const
{
	return m_Stats;
}

t3dVTFinalTelemetry_t Terrain3DVirtualTexture::get_final_telemetry_reference_debug() const
{
	t3dVTFinalTelemetry_t Result = {};
	if ( !m_bInitialized || m_Residency.pDatabase == NULL || m_Residency.pDatabase->pHeader == NULL ) {
		return Result;
	}
	Result.nAllocations = m_Residency.pDatabase->pHeader->nAllocationCount;
	Result.nVirtualPages = m_Residency.pDatabase->pHeader->nPageCount;
	Result.nPhysicalCapacity = m_Residency.Buffers.nPhysicalPageSetCapacity;
	for ( uint32_t iSlot = 0u; iSlot < m_Residency.Buffers.nPhysicalPageSetCapacity; ++iSlot ) {
		const t3dVTPhysicalSlot_t& Slot = m_Residency.Buffers.pPhysicalPageSets[iSlot];
		const bool bResident = Slot.m_nVirtualPage != T3D_VT_INVALID_PAGE;
		const bool bPending = ( Slot.m_nFlags & T3D_VT_SLOT_PENDING ) != 0u;
		const bool bRetiring = ( Slot.m_nFlags & T3D_VT_SLOT_RETIRED ) != 0u;
		Result.nPhysicalResident += bResident ? 1u : 0u;
		Result.nPhysicalFree += !bResident && !bPending ? 1u : 0u;
		Result.nPhysicalRetiring += bRetiring ? 1u : 0u;
		if ( !bResident || bRetiring ) {
			continue;
		}
		const uint16_t nCategory = t3d_vt_residency_category_reference_debug( *m_Residency.pDatabase, Slot.m_nVirtualPage );
		switch ( nCategory ) {
		case T3D_VT_RESIDENCY_CLUTTER:
			++Result.nResidentClutter;
			break;
		case T3D_VT_RESIDENCY_GROUND_COVER:
			++Result.nResidentGroundCover;
			break;
		case T3D_VT_RESIDENCY_OCCLUDER:
			++Result.nResidentOccluder;
			break;
		case T3D_VT_RESIDENCY_HLOD:
			++Result.nResidentHlod;
			break;
		case T3D_VT_RESIDENCY_TERRAIN:
		default:
			++Result.nResidentTerrain;
			break;
		}
	}
	const t3dVTGpuStats_t& GpuStats = m_Gpu.get_stats();
	Result.nRequestCapacity = m_Limits.nMaxVtRequests;
	Result.nRequestAttempts = GpuStats.nRequestAttempts;
	Result.nRequestUnique = GpuStats.nRequestUnique;
	Result.nRequestSameKey = GpuStats.nRequestSameKey;
	Result.nRequestHashOverflow = GpuStats.nRequestHashOverflow;
	Result.nRequestListOverflow = GpuStats.nRequestListOverflow;
	Result.nRequestDropped = GpuStats.nRequestHashOverflow + GpuStats.nRequestListOverflow;
	Result.nPageHits = m_Streaming.Stats.nPageHitCount;
	Result.nPageMisses = m_Streaming.Stats.nPageMissCount;
	Result.nParentFallbackHits = m_Residency.Stats.nAncestorFallbackCount;
	Result.nTailFallbackHits = m_Residency.Stats.nTailFallbackCount;
	Result.nResolverDepth0Samples = m_nResolverDepth0Samples;
	Result.nResolverDepth1Samples = m_nResolverDepth1Samples;
	Result.nResolverDepth2To3Samples = m_nResolverDepth2To3Samples;
	Result.nResolverDepth4PlusSamples = m_nResolverDepth4PlusSamples;
	Result.nResolverTailSamples = m_nResolverTailSamples;
	Result.nIoJobsPending = m_Streaming.Stats.nActiveJobCount;
	Result.nIoBytesTotal = m_Streaming.Stats.nIoBytesTotal;
	const godot::Time *const pTime = godot::Time::get_singleton();
	if ( pTime != NULL && m_nTelemetryStartUsec != 0u ) {
		const uint64_t nNowUsec = pTime->get_ticks_usec();
		const uint64_t nElapsedUsec = nNowUsec > m_nTelemetryStartUsec ? nNowUsec - m_nTelemetryStartUsec : 0u;
		if ( nElapsedUsec != 0u && Result.nIoBytesTotal <= UINT64_MAX / 1000000u ) {
			Result.nIoBytesPerSecond = Result.nIoBytesTotal * 1000000u / nElapsedUsec;
		}
	}
	Result.nUploadPageSetsFrame = m_nTelemetryUploadPageSetsFrame;
	Result.nUploadBytesFrame = m_nTelemetryUploadBytesFrame;
	Result.nUploadBytesTotal = m_Streaming.Stats.nUploadedBytes;
	Result.nEvictionsFrame = m_nTelemetryEvictionsFrame;
	Result.nEvictionsTotal = m_Residency.Stats.nEvictionCount;
	Result.nThrashCount = m_Residency.Stats.nThrashCount;
	if ( m_bHotSealed && m_pResources != NULL ) {
		const uint64_t nResourceCreates = m_pResources->get_stats().nGpuCreateCount;
		const uint64_t nHelperCreates = GpuStats.nGpuCreateCount;
		Result.nGpuResourceCreatesAfterSeal =
		    ( nResourceCreates > m_Stats.nPhysicalCacheCreateEpoch ? nResourceCreates - m_Stats.nPhysicalCacheCreateEpoch : 0u ) +
		    ( nHelperCreates > m_nGpuHelperCreateEpoch ? nHelperCreates - m_nGpuHelperCreateEpoch : 0u );
	}
	return Result;
}

bool Terrain3DVirtualTexture::shouldRefreshMaterialDemandHot( const uint32_t nLane, const t3dRdForwardPassContext_s& rContext,
                                                              const uint64_t nSourceSignature )
{
	if ( nLane >= 3u || !m_bInitialized || m_bWarmPackagePending ) {
		return false;
	}
	uint64_t signature = t3dHashCombine64( nSourceSignature, rContext.m_nViewportKey );
	signature = t3dHashCombine64( signature, m_nQualityGeneration );
	signature = t3dHashCombine64( signature, m_Residency.nDatabaseGeneration );
	// Newly resident pages do not change the view's desired footprint. Re-scan
	// on storage replacement or eviction, not after each ordinary upload batch.
	signature = t3dHashCombine64( signature, m_Stats.nPhysicalCacheCreateEpoch );
	signature = t3dHashCombine64( signature, m_Residency.Stats.nEvictionCount );
	signature = t3dHashCombine64( signature, rContext.m_nCameraVisibleLayers );
	signature = t3dHashCombine64( signature, rContext.m_nViewCount );
	signature = t3dHashFnv1a64( &rContext.m_cameraTransform, sizeof( rContext.m_cameraTransform ), signature );
	signature = t3dHashFnv1a64( &rContext.m_renderSize, sizeof( rContext.m_renderSize ), signature );
	// Exclude temporal projection jitter; a periodic sweep also covers FOV and
	// demand that was displaced by admission limits, without forgetting evictions.
	const uint32_t frame = ( uint32_t )rContext.m_nRenderFrame;
	const uint32_t period = nLane == 1u ? 4u : 8u;
	if ( m_szDemandRefreshFrame[nLane] != UINT32_MAX && m_szDemandSignature[nLane] == signature &&
	     frame - m_szDemandRefreshFrame[nLane] < period ) {
		return false;
	}
	m_szDemandSignature[nLane] = signature;
	m_szDemandRefreshFrame[nLane] = frame;
	return true;
}

static void t3dVtRestoreStagedPriorityHeap( t3dVTStagedRequest_t *requests, uint32_t *by_page, const uint32_t count, uint32_t index )
{
	auto swap_requests = [requests, by_page]( const uint32_t a, const uint32_t b ) {
		const t3dVTStagedRequest_t temporary = requests[a];
		requests[a] = requests[b];
		requests[b] = temporary;
		by_page[requests[a].Request.nVirtualPage] = a;
		by_page[requests[b].Request.nVirtualPage] = b;
	};
	while ( index > 0u ) {
		const uint32_t parent = ( index - 1u ) / 2u;
		if ( requests[parent].Request.nPriority <= requests[index].Request.nPriority ) {
			break;
		}
		swap_requests( parent, index );
		index = parent;
	}
	for ( ;; ) {
		uint32_t lowest = index;
		const uint32_t left = index * 2u + 1u;
		const uint32_t right = left + 1u;
		if ( left < count && requests[left].Request.nPriority < requests[lowest].Request.nPriority ) {
			lowest = left;
		}
		if ( right < count && requests[right].Request.nPriority < requests[lowest].Request.nPriority ) {
			lowest = right;
		}
		if ( lowest == index ) {
			break;
		}
		swap_requests( index, lowest );
		index = lowest;
	}
}

uint32_t Terrain3DVirtualTexture::stage_material_footprint_hot( const uint32_t nAllocationId, const uint32_t nFrameId,
                                                                const float pUvMin[2], const float pUvMax[2], const uint16_t nWantedMip,
                                                                const uint32_t nMaxRequests, const uint16_t nPriority,
                                                                const t3dVTRequestProducer_t eProducer, const uint32_t nFootprintWindow,
                                                                bool *const pFullyStaged )
{
	if ( pFullyStaged ) {
		*pFullyStaged = false;
	}
	if ( !m_bInitialized || !m_Streaming.bInitialized || m_pStagedRequests == NULL || nMaxRequests == 0u ||
	     nAllocationId >= m_Residency.nAllocationCount || m_Residency.Buffers.pAllocations == NULL ||
	     eProducer >= T3D_VT_REQUEST_PRODUCER_COUNT ) {
		return 0u;
	}
	if ( m_nStagedRequestFrame != nFrameId ) {
		m_nStagedRequestFrame = nFrameId;
		m_nStagedRequestCount = 0u;
	}
	const t3dVTAllocation_t& allocation = m_Residency.Buffers.pAllocations[nAllocationId];
	if ( !t3d_vt_allocation_quality_enabled( allocation.m_nFlags ) || allocation.m_nMipCount == 0u ) {
		return 0u;
	}
	const uint32_t nBaseMip = t3d_vt_allocation_quality_base_mip( allocation.m_nFlags );
	const uint16_t nAdjustedMip =
	    ( uint16_t )( nBaseMip + nWantedMip < allocation.m_nMipCount ? nBaseMip + nWantedMip : allocation.m_nMipCount - 1u );
	t3dVTPredictedPageRect_t rect;
	const bool bInstance = eProducer == T3D_VT_REQUEST_PRODUCER_INSTANCE;
	if ( !t3d_vt_predict_page_rect_reference_debug( allocation, pUvMin, pUvMax, nAdjustedMip, bInstance ? UINT32_MAX : nMaxRequests,
	                                                rect ) ) {
		return 0u;
	}
	// Admission can be full while visible resident pages still need a hot touch.
	const uint32_t nRequestLimit = nMaxRequests;
	uint32_t nAttempted = 0u;
	const uint32_t nRectWidth = rect.nMaxPageX - rect.nMinPageX + 1u;
	const uint32_t nRectPages = nRectWidth * ( rect.nMaxPageY - rect.nMinPageY + 1u );
	const uint32_t nWindowPages = MIN( nRequestLimit, nRectPages );
	bool fullyStaged = nWindowPages == nRectPages;
	// Advance by a candidate sweep in the unified lane, where a footprint need not be visited every frame.
	// This avoids repeating the same page window when the scan period divides the rectangle size.
	const uint32_t nWindow = nFootprintWindow == UINT32_MAX ? nFrameId : nFootprintWindow;
	const uint32_t nStartPage = ( bInstance || nFootprintWindow != UINT32_MAX ) && nRectPages > nWindowPages
	                                ? ( uint32_t )( ( uint64_t )nWindow * nWindowPages % nRectPages )
	                                : 0u;
	for ( uint32_t iPage = 0u; iPage < nWindowPages; ++iPage ) {
		const uint32_t nRectPage = ( nStartPage + iPage ) % nRectPages;
		const uint32_t x = rect.nMinPageX + nRectPage % nRectWidth;
		const uint32_t y = rect.nMinPageY + nRectPage / nRectWidth;
		uint32_t nVirtualPage = T3D_VT_INVALID_PAGE;
		if ( !t3d_vt_allocation_page_id( allocation, rect.nMip, x, y, nVirtualPage ) ) {
			fullyStaged = false;
			continue;
		}
		++nAttempted;
		if ( ( nFrameId & 63u ) == 0u ) {
			if ( m_nResolverTelemetrySampleFrame != nFrameId ) {
				m_nResolverTelemetrySampleFrame = nFrameId;
				m_nResolverTelemetrySamplesThisFrame = 0u;
			}
			if ( m_nResolverTelemetrySamplesThisFrame < 32u ) {
				++m_nResolverTelemetrySamplesThisFrame;
				t3dVTResolveResult_t Resolve = {};
				if ( t3d_vt_resolve_allocation_page_reference_debug( m_Residency, nAllocationId, nVirtualPage, ( uint16_t )rect.nMip,
				                                                     Resolve ) ) {
					if ( ( Resolve.m_nFlags & T3D_VT_RESOLVE_TAIL ) != 0u ) {
						++m_nResolverTailSamples;
					} else if ( Resolve.m_nFallbackDepth == 0u ) {
						++m_nResolverDepth0Samples;
					} else if ( Resolve.m_nFallbackDepth == 1u ) {
						++m_nResolverDepth1Samples;
					} else if ( Resolve.m_nFallbackDepth <= 3u ) {
						++m_nResolverDepth2To3Samples;
					} else {
						++m_nResolverDepth4PlusSamples;
					}
				}
			}
		}
		// Visible resident pages still refresh the existing hot/warm hysteresis;
		// only misses enter the shared CPU admission phase.
		if ( t3d_vt_touch_page_hot( m_Residency, nVirtualPage, nFrameId ) ) {
			++m_Streaming.Stats.nPageHitCount;
			++m_Streaming.Stats.nResidentRequestCount;
			continue;
		}
		if ( nVirtualPage >= m_Limits.nMaxVtVirtualPages ) {
			fullyStaged = false;
			continue;
		}
		const uint32_t existing_index = m_pStagedByPage[nVirtualPage];
		// Validate the index against this frame's array; old-frame and displaced
		// entries need no full-table clear on the hot path.
		if ( existing_index < m_nStagedRequestCount && m_pStagedRequests[existing_index].Request.nVirtualPage == nVirtualPage ) {
			t3dVTStagedRequest_t& existing = m_pStagedRequests[existing_index];
			if ( nPriority > existing.Request.nPriority ) {
				existing.nProducer = ( uint16_t )eProducer;
			}
			existing.Request.nPriority = MAX( existing.Request.nPriority, nPriority );
			existing.Request.nWantedMip = MIN( existing.Request.nWantedMip, ( uint16_t )rect.nMip );
			t3dVtRestoreStagedPriorityHeap( m_pStagedRequests, m_pStagedByPage, m_nStagedRequestCount, existing_index );
			continue;
		}
		uint32_t nStagedIndex = m_nStagedRequestCount;
		if ( m_nStagedRequestCount >=
		         ( m_nFrameRequestBudget -
		           MIN( m_nFrameRequestBudget, m_nFragmentRequestFrame == nFrameId ? m_nFragmentRequestsUsed : 0u ) ) ||
		     m_nStagedRequestCount >= m_Limits.nMaxVtRequests ) {
			// Close visible demand may displace lower-priority prediction, within
			// the same staging budget. Streaming still owns admission and eviction.
			if ( m_nStagedRequestCount == 0u || m_pStagedRequests[0].Request.nPriority >= nPriority ) {
				fullyStaged = false;
				continue;
			}
			// Evicting an earlier prediction invalidates complete-coverage proof.
			fullyStaged = false;
			if ( eProducer == T3D_VT_REQUEST_PRODUCER_TERRAIN ) {
				m_bSpatialDemandHeapDisplaced = true;
			}
			nStagedIndex = 0u;
		} else {
			++m_nStagedRequestCount;
		}
		m_pStagedByPage[nVirtualPage] = nStagedIndex;
		t3dVTStagedRequest_t& staged = m_pStagedRequests[nStagedIndex];
		staged.Request.nVirtualPage = nVirtualPage;
		staged.Request.nWantedMip = ( uint16_t )rect.nMip;
		staged.Request.nPriority = nPriority;
		staged.nProducer = ( uint16_t )eProducer;
		staged.nReserved = 0u;
		t3dVtRestoreStagedPriorityHeap( m_pStagedRequests, m_pStagedByPage, m_nStagedRequestCount, nStagedIndex );
	}
	if ( pFullyStaged ) {
		*pFullyStaged = fullyStaged && nAttempted == nRectPages;
	}
	return nAttempted;
}

static bool t3dVtFeedbackPageLookup( void *const pUser, const uint32_t nPage, uint32_t& rParent, uint16_t& rMip )
{
	const t3dVTResidencyState_t& residency = *( const t3dVTResidencyState_t * )pUser;
	if ( !residency.pDatabase || !residency.pDatabase->pHeader || !residency.Buffers.pPageTable ||
	     nPage >= residency.pDatabase->pHeader->nPageCount )
		return false;
	rParent = residency.Buffers.pPageTable[nPage].nParentPage;
	rMip = residency.pDatabase->pPages ? residency.pDatabase->pPages[nPage].nMip : residency.Buffers.pPageTable[nPage].nResolvedMip;
	return true;
}

bool Terrain3DVirtualTexture::configure_feedback_policy_cold( const t3dVTFeedbackPolicy_t& rPolicy, const uint32_t nPhysicalCapDivisor )
{
	if ( m_bHotSealed || rPolicy.m_nMinimumSamples == 0u || rPolicy.m_nMaxCandidates == 0u ||
	     rPolicy.m_nMaxCandidates > m_Limits.nMaxVtRequests || rPolicy.m_nMotionLevel > 2u || nPhysicalCapDivisor > 64u )
		return false;
	m_feedbackPolicy = rPolicy;
	m_nFeedbackPhysicalCapDivisor = nPhysicalCapDivisor;
	return true;
}

void Terrain3DVirtualTexture::update_feedback_view_hot( const float fCameraX, const float fCameraZ, const float fForwardX,
                                                        const float fForwardZ, const uint32_t nFrameId )
{
	if ( m_nFeedbackViewFrame == nFrameId )
		return;
	uint32_t motion = 0u;
	if ( m_nFeedbackViewFrame != UINT32_MAX && nFrameId > m_nFeedbackViewFrame ) {
		const float dx = fCameraX - m_fFeedbackX, dz = fCameraZ - m_fFeedbackZ;
		const float frameDelta = ( float )MAX( 1u, nFrameId - m_nFeedbackViewFrame );
		const float motionSquared = ( dx * dx + dz * dz ) / ( frameDelta * frameDelta );
		const float dot = fForwardX * m_fFeedbackForwardX + fForwardZ * m_fFeedbackForwardZ;
		motion = motionSquared > 64.0f || dot < 0.25f ? 2u : motionSquared > 4.0f || dot < 0.8f ? 1u : 0u;
	}
	m_nFeedbackViewFrame = nFrameId;
	m_fFeedbackX = fCameraX;
	m_fFeedbackZ = fCameraZ;
	m_fFeedbackForwardX = fForwardX;
	m_fFeedbackForwardZ = fForwardZ;
	m_feedbackPolicy.m_nMotionLevel = motion;
}

bool Terrain3DVirtualTexture::stage_feedback_page_hot( const uint32_t nPage, const uint16_t nMip, const uint16_t nPriority,
                                                       const uint32_t nFrameId )
{
	if ( !m_pStagedRequests || !m_pStagedByPage || nPage >= m_Limits.nMaxVtVirtualPages )
		return false;
	if ( m_nStagedRequestFrame != nFrameId ) {
		m_nStagedRequestFrame = nFrameId;
		m_nStagedRequestCount = 0u;
	}
	const uint32_t existingIndex = m_pStagedByPage[nPage];
	if ( existingIndex < m_nStagedRequestCount && m_pStagedRequests[existingIndex].Request.nVirtualPage == nPage ) {
		t3dVTStagedRequest_t& entry = m_pStagedRequests[existingIndex];
		if ( nPriority > entry.Request.nPriority )
			entry.nProducer = T3D_VT_REQUEST_PRODUCER_FEEDBACK;
		entry.Request.nPriority = MAX( entry.Request.nPriority, nPriority );
		entry.Request.nWantedMip = MIN( entry.Request.nWantedMip, nMip );
		t3dVtRestoreStagedPriorityHeap( m_pStagedRequests, m_pStagedByPage, m_nStagedRequestCount, existingIndex );
		return true;
	}
	uint32_t index = m_nStagedRequestCount;
	const uint32_t cap = MIN( m_nFrameRequestBudget, m_Limits.nMaxVtRequests );
	if ( index >= cap ) {
		if ( index == 0u || m_pStagedRequests[0].Request.nPriority >= nPriority )
			return false;
		// A feedback page displacing a prediction invalidates complete coverage proof.
		m_bSpatialDemandHeapDisplaced = true;
		m_bSpatialDemandCurrentCellComplete = false;
		for ( uint32_t i = 0u; i < m_nSpatialDemandCoveredAllocationCount; ++i )
			m_szSpatialDemandAllocationComplete[i] = 0u;
		index = 0u;
	} else {
		++m_nStagedRequestCount;
	}
	m_pStagedByPage[nPage] = index;
	t3dVTStagedRequest_t& entry = m_pStagedRequests[index];
	entry.Request = { nPage, nMip, nPriority };
	entry.nProducer = T3D_VT_REQUEST_PRODUCER_FEEDBACK;
	entry.nReserved = 0u;
	t3dVtRestoreStagedPriorityHeap( m_pStagedRequests, m_pStagedByPage, m_nStagedRequestCount, index );
	return true;
}

bool Terrain3DVirtualTexture::service_feedback_grid_hot( const uint32_t nFrameId, const uint64_t nViewportKey,
                                                         const uint32_t nDeviceGeneration )
{
	if ( !m_bInitialized || !m_Streaming.bInitialized || !m_Gpu.is_feedback_grid_enabled() || !m_Residency.pDatabase ||
	     !m_Residency.pDatabase->pHeader || m_bWarmPackagePending )
		return false;
	const uint32_t pageCount = m_Residency.pDatabase->pHeader->nPageCount;
	for ( uint32_t slot = 0u; slot < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++slot ) {
		t3dVTFeedbackGridHeader_t header{};
		const uint8_t *const words = m_Gpu.peek_feedback_grid_ready_hot( slot, header );
		if ( !words )
			continue;
		if ( header.m_nDatabaseGeneration != m_Residency.nDatabaseGeneration || header.m_nDeviceGeneration != nDeviceGeneration ||
		     header.m_nViewportKey != nViewportKey || header.m_nFrame > nFrameId ) {
			( void )m_Gpu.retire_feedback_grid_ready_hot( slot );
			continue;
		}
		t3dVTFeedbackReductionStats_t stats{};
		t3dVTFeedbackPolicy_t policy = m_feedbackPolicy;
		const uint32_t physical = m_Residency.Buffers.nPhysicalPageSetCapacity;
		const uint32_t physicalCap =
		    m_nFeedbackPhysicalCapDivisor ? MAX( 1u, physical / m_nFeedbackPhysicalCapDivisor ) : m_Limits.nMaxVtRequests;
		policy.m_nMaxCandidates = MIN( policy.m_nMaxCandidates, MIN( physicalCap, MIN( m_nFrameRequestBudget, m_Limits.nMaxVtRequests ) ) );
		if ( m_feedbackReducer.reduce( words, header.m_nWordCount, pageCount, t3dVtFeedbackPageLookup, &m_Residency, policy, stats ) ) {
			( void )m_multiFeedbackParity.capture( header, words );
			m_nFeedbackRejectedNoise += stats.m_nRejectedNoise;
			m_nFeedbackGridDirectUniquePages += stats.m_nDirectUniquePages;
			m_nFeedbackGridSelectedCandidates += stats.m_nCandidates;
			m_nFeedbackParentPromotions += stats.m_nParentPromotions;
			m_nFeedbackInvalidParents += stats.m_nInvalidParents;
			// Touch all observed resident pages, even when admission is capped.
			for ( uint32_t i = 0u; i < m_feedbackReducer.getTouchedCount(); ++i ) {
				const uint32_t page = m_feedbackReducer.getTouchedPage( i );
				if ( t3d_vt_touch_page_hot( m_Residency, page, nFrameId ) ) {
					++m_nFeedbackResidentRescues;
					++m_Streaming.Stats.nPageHitCount;
					++m_Streaming.Stats.nResidentRequestCount;
				}
			}
			for ( uint32_t i = 0u; i < m_feedbackReducer.getSelectedCount(); ++i ) {
				const t3dVTFeedbackCandidate_t& candidate = m_feedbackReducer.getSelected( i );
				const uint32_t page = candidate.m_nPage;
				if ( page >= pageCount || ( m_Residency.Buffers.pPageTable[page].nFlags & T3D_VT_PAGE_RESIDENT ) != 0u )
					continue;
				const uint16_t priority = ( uint16_t )MIN( 65535u, T3D_VT_PRIORITY_NORMAL_MIN + candidate.m_nScore );
				if ( stage_feedback_page_hot( page, candidate.m_nMip, priority, nFrameId ) )
					++m_nFeedbackStagedRequests;
			}
			m_Gpu.record_feedback_grid_consumed_hot( stats.m_nValidSamples );
		}
		( void )m_Gpu.retire_feedback_grid_ready_hot( slot );
	}
	return true;
}

void Terrain3DVirtualTexture::service_multi_feedback_shadow_hot( const uint32_t nFrameId, const uint64_t nViewportKey,
                                                                 const uint32_t nDeviceGeneration )
{
	if ( !m_bInitialized || !m_Streaming.bInitialized || m_bWarmPackagePending || !m_Gpu.is_multi_feedback_ready() ||
	     !m_Residency.pDatabase || !m_Residency.pDatabase->pHeader || !m_Residency.Buffers.pPageTable )
		return;
	for ( uint32_t slot = 0u; slot < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++slot ) {
		t3dVTFeedbackGridHeader_t header{};
		const uint8_t *const data = m_Gpu.peek_multi_feedback_ready_hot( slot, header );
		if ( !data )
			continue;
		t3dVTFeedbackMultiParams_t params{};
		params.m_nSlot = slot;
		if ( !m_Gpu.get_multi_feedback_params_cold( slot, params ) ||
		     !t3d_vt_feedback_multi_shadow_frame_valid( header, params, nFrameId, m_Gpu.get_feedback_epoch_hot(),
		                                                m_Residency.nDatabaseGeneration, nDeviceGeneration, nViewportKey ) ) {
			++m_multiFeedbackShadowStats.m_nStale;
			( void )m_Gpu.retire_multi_feedback_ready_hot( slot );
			continue;
		}
		t3dVTFeedbackReductionStats_t stats{};
		t3dVTFeedbackPolicy_t policy = m_feedbackPolicy;
		const uint32_t physical = m_Residency.Buffers.nPhysicalPageSetCapacity;
		const uint32_t physicalCap =
		    m_nFeedbackPhysicalCapDivisor ? MAX( 1u, physical / m_nFeedbackPhysicalCapDivisor ) : m_Limits.nMaxVtRequests;
		policy.m_nMaxCandidates = MIN( policy.m_nMaxCandidates, MIN( physicalCap, MIN( m_nFrameRequestBudget, m_Limits.nMaxVtRequests ) ) );
		if ( t3d_vt_feedback_multi_reduce_capture( data, ( size_t )params.m_nSlotWords * sizeof( uint32_t ), params, header.m_nFrame,
		                                           header.m_nFeedbackEpoch, header.m_nDatabaseGeneration, header.m_nDeviceGeneration,
		                                           header.m_nViewportKey, m_feedbackReducer, t3dVtFeedbackPageLookup, &m_Residency, policy,
		                                           stats ) ) {
			t3d_vt_feedback_multi_shadow_accept( m_multiFeedbackShadowStats, stats.m_nSamples, stats.m_nValidSamples,
			                                     stats.m_nDirectUniquePages, stats.m_nCandidates );
			t3dVTFeedbackMultiParitySample_t parity{};
			if ( m_multiFeedbackParity.compare( header, m_Residency.pDatabase->pHeader->nPageCount, m_feedbackReducer, parity ) ) {
				++m_multiFeedbackShadowStats.m_nPairedCaptures;
				m_multiFeedbackShadowStats.m_nV2ObservedSamples += parity.m_nObserved;
				m_multiFeedbackShadowStats.m_nV2CoveredSamples += parity.m_nCovered;
				m_multiFeedbackShadowStats.m_nV2MissingSamples += parity.m_nMissing;
			} else if ( m_bMultiFeedbackParityRequested ) {
				++m_multiFeedbackShadowStats.m_nUnpairedCaptures;
			}
		} else {
			++m_multiFeedbackShadowStats.m_nRejected;
		}
		// V1 never stages requests, touches resident pages or changes eviction.
		m_feedbackReducer.discardCapture();
		( void )m_Gpu.retire_multi_feedback_ready_hot( slot );
	}
}

bool Terrain3DVirtualTexture::flush_staged_material_requests_hot( const uint32_t nFrameId, uint32_t& rTerrainAccepted,
                                                                  uint32_t& rInstanceAccepted )
{
	T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_REQUEST_ADMISSION );
	rTerrainAccepted = 0u;
	rInstanceAccepted = 0u;
	if ( !m_bInitialized || !m_Streaming.bInitialized || m_pStagedRequests == NULL ) {
		return false;
	}
	if ( m_nStagedRequestFrame != nFrameId || m_nStagedRequestCount == 0u ) {
		return true;
	}
	// Admit both native producers through one owner-controlled phase. Interleave
	// them so a terrain-heavy frame cannot consume the whole admission budget
	// before visible instance materials get a chance to enter the same queue.
	uint32_t cursor[T3D_VT_REQUEST_PRODUCER_COUNT] = {};
	uint32_t remaining[T3D_VT_REQUEST_PRODUCER_COUNT] = {};
	for ( uint32_t i = 0u; i < m_nStagedRequestCount; ++i ) {
		const uint16_t nProducer = m_pStagedRequests[i].nProducer;
		if ( nProducer < T3D_VT_REQUEST_PRODUCER_COUNT ) {
			++remaining[nProducer];
		}
	}
	bool bProgress = true;
	while ( bProgress ) {
		bProgress = false;
		for ( uint32_t producer = 0u; producer < T3D_VT_REQUEST_PRODUCER_COUNT; ++producer ) {
			if ( remaining[producer] == 0u ) {
				continue;
			}
			while ( cursor[producer] < m_nStagedRequestCount && m_pStagedRequests[cursor[producer]].nProducer != producer ) {
				++cursor[producer];
			}
			if ( cursor[producer] >= m_nStagedRequestCount ) {
				remaining[producer] = 0u;
				continue;
			}
			const t3dVTAdmissionStatus_t eStatus =
			    t3d_vt_streaming_admit_request_hot( m_Streaming, m_pStagedRequests[cursor[producer]].Request, nFrameId );
			if ( eStatus == T3D_VT_ADMISSION_ACCEPTED_NEW ) {
				if ( producer == T3D_VT_REQUEST_PRODUCER_TERRAIN ) {
					++rTerrainAccepted;
				} else if ( producer == T3D_VT_REQUEST_PRODUCER_INSTANCE ) {
					++rInstanceAccepted;
				}
			}
			++cursor[producer];
			--remaining[producer];
			bProgress = true;
		}
	}
	m_nStagedRequestCount = 0u;
	return true;
}

bool Terrain3DVirtualTexture::claim_stream_job_owner_hot( uint32_t& rJobIndex )
{
	return m_bInitialized && !m_bWarmPackagePending && m_bHotSealed && t3d_vt_streaming_claim_job_owner_hot( m_Streaming, rJobIndex );
}

bool Terrain3DVirtualTexture::cancel_stream_job_owner_hot( const uint32_t pJobIndex )
{
	return m_bInitialized && t3d_vt_streaming_cancel_claim_owner_hot( m_Streaming, pJobIndex );
}

bool Terrain3DVirtualTexture::execute_stream_job_async( const uint32_t pJobIndex, t3dVTStreamReadDecodeFn_t pReadDecode, void *const pUser )
{
	return m_bInitialized && m_bHotSealed && t3d_vt_streaming_execute_job_async( m_Streaming, pJobIndex, pReadDecode, pUser );
}

uint64_t Terrain3DVirtualTexture::service_uploads_hot( const uint32_t pFrameId )
{
	if ( !m_bInitialized || !m_bHotSealed || m_bWarmPackagePending ) {
		T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_UPLOAD_SERVICE );
		return 0u;
	}
	const uint64_t nPageMisses = m_Streaming.Stats.nPageMissCount;
	const uint64_t nEvictionsBefore = m_Residency.Stats.nEvictionCount;
	const uint64_t nThrash = m_Residency.Stats.nThrashCount;
	const uint32_t nConfiguredPageSets = m_Streaming.Limits.nMaxPageSetsPerFrame;
	const uint32_t nCapacity = m_Residency.Buffers.nPhysicalPageSetCapacity;
	const uint32_t nResident = m_Residency.Stats.nResidentCount;
	const bool bQuiet = nPageMisses == m_nUploadCadenceLastPageMisses && nEvictionsBefore == m_nUploadCadenceLastEvictions &&
	                    nThrash == m_nUploadCadenceLastThrash;
	const bool bCacheWarm = nCapacity != 0u && ( uint64_t )nResident * 4u >= ( uint64_t )nCapacity * 3u;
	const bool bSmallBacklog = m_Streaming.nReadyJobCount <= nConfiguredPageSets;
	if ( bQuiet && bCacheWarm && bSmallBacklog ) {
		if ( m_nUploadCadenceStableFrames != UINT32_MAX ) {
			++m_nUploadCadenceStableFrames;
		}
	} else {
		m_nUploadCadenceStableFrames = 0u;
	}
	uint32_t nEffectivePageSets = nConfiguredPageSets;
	if ( m_nUploadCadenceStableFrames >= 16u && nEffectivePageSets > 2u ) {
		nEffectivePageSets = 2u;
	} else if ( m_nUploadCadenceStableFrames >= 8u && nEffectivePageSets > 4u ) {
		nEffectivePageSets = 4u;
	}
	m_Streaming.Limits.nMaxPageSetsPerFrame = nEffectivePageSets;
	const uint64_t nUploadedBytes = t3d_vt_streaming_service_uploads_batched_hot(
	    m_Streaming, pFrameId, Terrain3DVTGpu::upload_batch_begin_callback, Terrain3DVTGpu::upload_commit_callback,
	    Terrain3DVTGpu::upload_batch_end_callback, &m_Gpu );
	m_Streaming.Limits.nMaxPageSetsPerFrame = nConfiguredPageSets;
	m_nUploadCadenceLastPageMisses = nPageMisses;
	m_nUploadCadenceLastEvictions = m_Residency.Stats.nEvictionCount;
	m_nUploadCadenceLastThrash = nThrash;
	const uint64_t nUploadedPageSets = m_Streaming.Stats.nUploadedPageCount;
	const uint64_t nEvictions = m_Residency.Stats.nEvictionCount;
	m_nTelemetryUploadPageSetsFrame =
	    nUploadedPageSets >= m_nTelemetryLastUploadedPageSets ? nUploadedPageSets - m_nTelemetryLastUploadedPageSets : 0u;
	m_nTelemetryUploadBytesFrame = nUploadedBytes;
	m_nTelemetryEvictionsFrame = nEvictions >= m_nTelemetryLastEvictions ? nEvictions - m_nTelemetryLastEvictions : 0u;
	m_nTelemetryLastUploadedPageSets = nUploadedPageSets;
	m_nTelemetryLastEvictions = nEvictions;
	return nUploadedBytes;
}

bool Terrain3DVirtualTexture::service_retired_slots_hot( const uint32_t pFrameId )
{
	return m_bInitialized && m_bHotSealed && m_Gpu.service_retired_slots_hot( m_Residency, pFrameId );
}

void Terrain3DVirtualTexture::service_fragment_feedback_hot( const uint32_t nFrameId )
{
	T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_FEEDBACK_SERVICE );
	if ( !m_bInitialized || !m_Streaming.bInitialized || m_nFragmentRequestFrame == nFrameId ) {
		return;
	}
	m_nFragmentRequestFrame = nFrameId;
	m_nFragmentRequestsUsed = m_Gpu.service_fragment_feedback_hot( m_Residency, m_Streaming, nFrameId, m_nFrameRequestBudget );
}

static bool t3dVtCopyResidentPackagePayloadHot( const t3dVTDatabaseView_t& rDatabase, const t3dVTStreamJob_t& rJob, uint8_t *pScratch,
                                                const uint32_t nScratchBytes, uint32_t& rDecodedBytes, void * )
{
	T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_IO_READ_DECODE );
	rDecodedBytes = 0u;
	if ( rDatabase.pHeader == NULL || rDatabase.pPayloads == NULL || ( rDatabase.pPayloadData == NULL && rDatabase.pFileSource == NULL ) ||
	     rJob.nPayloadId >= rDatabase.pHeader->nPayloadCount || rJob.nReadBytes > nScratchBytes ||
	     rJob.nReadOffset > rDatabase.pHeader->nPayloadDataBytes ||
	     rJob.nReadBytes > rDatabase.pHeader->nPayloadDataBytes - rJob.nReadOffset ) {
		return false;
	}
	const t3dVTPayloadRecord_t& payload = rDatabase.pPayloads[rJob.nPayloadId];
	if ( rDatabase.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK ) {
		if ( rDatabase.pRuntimePayloads == NULL || rJob.nPayloadId >= rDatabase.nRuntimePayloadCount ||
		     rDatabase.pHeader->nInteriorTexels != T3D_VT_PAGE_INTERIOR || rDatabase.pHeader->nGutterTexels != T3D_VT_PAGE_GUTTER ) {
			return false;
		}
		const t3dVTRuntimePayloadRecord_t& physical = rDatabase.pRuntimePayloads[rJob.nPayloadId];
		const uint32_t tile_bytes = t3d_vt_pack_expected_physical_tile_bytes( physical.nChannel );
		if ( tile_bytes == 0u || physical.nChannel != rJob.nChannel || physical.nBytes != tile_bytes ||
		     physical.nOffset != rJob.nReadOffset || rJob.nExpectedDecodedBytes != tile_bytes ) {
			return false;
		}
	}
	// V1 raw clusters: disk-backed pages read directly into fixed scratch lanes.
	// A per-payload checksum detects mutations after cold source validation.
	if ( payload.nCodec != T3D_VT_PACK_CODEC_NONE || payload.nOffset != rJob.nReadOffset || payload.nCompressedBytes != rJob.nReadBytes ||
	     payload.nDecodedBytes != rJob.nReadBytes || payload.nDecodedBytes != rJob.nExpectedDecodedBytes ) {
		return false;
	}
	if ( rDatabase.pFileSource != NULL ) {
		if ( rDatabase.pFileSource->nGeneration != rDatabase.nPackageGeneration ) {
			return false;
		}
		if ( !t3d_vt_file_source_read_payload( *const_cast<t3dVTFileSource_s *>( rDatabase.pFileSource ), rJob.nReadOffset, pScratch,
		                                       rJob.nReadBytes ) ||
		     t3d_vt_pack_checksum32( pScratch, rJob.nReadBytes ) != payload.nChecksum ) {
			return false;
		}
		T3D_STREAM_PROFILE_BYTES( T3D_SP_VT_IO_READ_DECODE, rJob.nReadBytes );
	} else {
		memcpy( pScratch, rDatabase.pPayloadData + rJob.nReadOffset, rJob.nReadBytes );
	}
	rDecodedBytes = rJob.nReadBytes;
	return true;
}

void Terrain3DVirtualTexture::streamWorkerMain( void )
{
	for ( ;; ) {
		uint32_t job = T3D_VT_INVALID_ID;
		{
			Terrain3DUniqueLock lock( m_streamWorkerMutex );
			while ( m_nStreamWorkerCount == 0u && !m_bStreamWorkerStop ) {
				m_streamWorkerCondition.wait( lock );
			}
			if ( m_nStreamWorkerCount == 0u && m_bStreamWorkerStop ) {
				return;
			}
			job = m_szStreamWorkerJobs[m_nStreamWorkerHead];
			m_nStreamWorkerHead = ( m_nStreamWorkerHead + 1u ) % T3D_VT_MAX_STREAM_SCRATCH_LANES;
			--m_nStreamWorkerCount;
		}
		// Only this claimed record and its fixed scratch lane are mutable here.
		// The streaming executor release-publishes completion as its last access.
		( void )t3d_vt_streaming_execute_job_async( m_Streaming, job, t3dVtCopyResidentPackagePayloadHot, NULL );
	}
}

bool Terrain3DVirtualTexture::dispatchStreamJobHot( const uint32_t nJob )
{
	if ( !m_streamWorker.joinable() ) {
		return false;
	}
	{
		Terrain3DScopedLock lock( m_streamWorkerMutex );
		if ( m_bStreamWorkerStop || m_nStreamWorkerCount >= T3D_VT_MAX_STREAM_SCRATCH_LANES ) {
			return false;
		}
		m_szStreamWorkerJobs[( m_nStreamWorkerHead + m_nStreamWorkerCount ) % T3D_VT_MAX_STREAM_SCRATCH_LANES] = nJob;
		++m_nStreamWorkerCount;
	}
	m_streamWorkerCondition.notifyOne();
	return true;
}

void Terrain3DVirtualTexture::stopStreamWorkerCold( void )
{
	if ( !m_streamWorker.joinable() ) {
		return;
	}
	{
		Terrain3DScopedLock lock( m_streamWorkerMutex );
		m_bStreamWorkerStop = true;
	}
	m_streamWorkerCondition.notifyAll();
	m_streamWorker.join();
}

void Terrain3DVirtualTexture::service_streaming_hot( const uint32_t nFrameId )
{
	T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_JOB_DISPATCH );
	if ( !m_bInitialized || !m_bHotSealed || m_bWarmPackagePending || m_nStreamingServiceFrame == nFrameId ) {
		return;
	}
	m_nStreamingServiceFrame = nFrameId;
	( void )service_retired_slots_hot( nFrameId );
	// Release completed scratch lanes before admitting replacement work.
	( void )service_uploads_hot( nFrameId );
	( void )m_Gpu.service_ancestor_hints_hot( m_Residency );
	uint64_t remaining = m_Streaming.Limits.nMaxUploadBytesPerFrame;
	const uint32_t work_limit = MIN( m_Streaming.Limits.nMaxPageSetsPerFrame, m_Streaming.Limits.nMaxInFlightIoJobs );
	for ( uint32_t i = 0u; i < work_limit; ++i ) {
		uint32_t job = T3D_VT_INVALID_ID;
		if ( !claim_stream_job_owner_hot( job ) ) {
			break;
		}
		const uint32_t bytes = m_Streaming.Buffers.pJobs[job].nReadBytes;
		if ( bytes > remaining ) {
			( void )cancel_stream_job_owner_hot( job );
			break;
		}
		remaining -= bytes;
		if ( !dispatchStreamJobHot( job ) ) {
			( void )cancel_stream_job_owner_hot( job );
			break;
		}
	}
}
