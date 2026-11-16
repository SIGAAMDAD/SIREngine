#include "diagnostics/core/terrain_3d_streaming_profiler.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/core/terrain_3d_gpu_transfer_budget.h"
// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "rendering/rd/vt/gpu/terrain_3d_vt_gpu.h"
#include "rendering/rd/vt/gpu/terrain_3d_vt_upload_preflight.h"
#include "rendering/rd/shaders/terrain_3d_rd_shader_library.h"
#include "rendering/rd/infrastructure/descriptors/terrain_3d_rd_global_descriptor_cache.h"

#include "rendering/rd/vt/io/terrain_3d_vt_pack_runtime.h"
#include "rendering/rd/vt/core/terrain_3d_vt_neutral_payload.h"
#include <algorithm>

#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <limits.h>
#include "rendering/rd/infrastructure/descriptors/terrain_3d_rd_binding_builder.h"
#include <string.h>

using namespace godot;

// Byte-backed Godot staging arrays are not typed C++ object storage.
static t3dVTUploadReservation_t t3dVtReadReservation( const PackedByteArray& rBytes, uint32_t nIndex )
{
	t3dVTUploadReservation_t Record{};
	memcpy( &Record, rBytes.ptr() + ( size_t )nIndex * sizeof( Record ), sizeof( Record ) );
	return Record;
}

static void t3dVtWriteReservation( PackedByteArray& rBytes, uint32_t nIndex, const t3dVTUploadReservation_t& rRecord )
{
	memcpy( rBytes.ptrw() + ( size_t )nIndex * sizeof( rRecord ), &rRecord, sizeof( rRecord ) );
}

static t3dVTRequestHashEntry_t t3dVtReadFeedback( const PackedByteArray& rBytes, uint32_t nIndex )
{
	t3dVTRequestHashEntry_t Record{};
	memcpy( &Record, rBytes.ptr() + ( size_t )nIndex * sizeof( Record ), sizeof( Record ) );
	return Record;
}

static bool t3dVtUpload( const Terrain3DVTResourceImage *const pResources, const t3dVTGpuBufferId_t eBuffer, uint32_t nOffset,
                         uint32_t nBytes, const PackedByteArray& rBytes, Terrain3DGpuBufferPolicy policy = T3D_GPU_DIRTY_UPLOAD )
{
	Terrain3DRdContext *const context = pResources ? pResources->get_rd_context() : NULL;
	const Terrain3DRdGpuBuffer *const buffer = pResources ? pResources->get_buffer( eBuffer ) : NULL;
	return context && buffer && buffer->writePackedTracked( *context, nOffset, rBytes, nBytes, policy );
}

Terrain3DVTGpu::Terrain3DVTGpu()
{
	m_pRenderingDevice = NULL;
	m_pResources = NULL;
	m_pDatabaseView = NULL;
	memset( &m_Plan, 0, sizeof( m_Plan ) );
	memset( &m_Limits, 0, sizeof( m_Limits ) );
	memset( &m_Stats, 0, sizeof( m_Stats ) );
	m_nHotCreateEpoch = 0u;
	m_nHotFreeEpoch = 0u;
	m_nUploadBatchCount = 0u;
	m_nDatabaseUploadStage = 0u;
	m_nDatabaseTailChannel = 0u;
	m_bUploadBatchActive = false;
	m_bDatabaseUploadBegun = false;
	m_bCpuPrepared = false;
	m_bStorageReady = false;
	m_bExecutableReady = false;
	m_bInitialized = false;
	m_bDatabaseUploadedWarm = false;
	m_bHotSealed = false;
}

Terrain3DVTGpu::~Terrain3DVTGpu()
{
	// Explicit cold shutdown is required by the packet ownership contract.
}

static bool t3d_vt_gpu_can_initialize( const bool bInitialized, const Terrain3DVTResourceImage *const pResources,
                                       const RenderingDevice *const pRenderingDevice )
{
	return !bInitialized && pResources != NULL && pRenderingDevice != NULL && pResources->is_initialized();
}

static bool t3d_vt_gpu_has_valid_limits( const t3dWorldLimits_t& pLimits )
{
	return pLimits.nMaxVtRequests != 0u && pLimits.nMaxVtRequests <= UINT32_MAX / 2u && pLimits.nMaxReadbackBytesPerFrame <= UINT32_MAX;
}

bool Terrain3DVTGpu::prepare_cpu_cold( const t3dVTGpuResourcePlan_t& pPlan, const t3dWorldLimits_t& pLimits )
{
	if ( m_bCpuPrepared || m_bStorageReady || m_bExecutableReady || m_bInitialized || !t3d_vt_gpu_has_valid_limits( pLimits ) ||
	     pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_ALLOCATION_TABLE] == 0u ) {
		return false;
	}
	m_Limits = pLimits;
	m_Plan = pPlan;
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	const uint64_t nHintCapacity64 = pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_PAGE_TABLE] / sizeof( t3dVTPageTableEntry_t );
	if ( nHintCapacity64 == 0u || nHintCapacity64 > INT32_MAX ||
	     ( nHintCapacity64 + 1u ) * sizeof( t3dVTAncestorHint_t ) > pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_ANCESTOR_HINTS] )
		return false;
	const int64_t nCapacity = ( int64_t )nHintCapacity64;
	m_HintFirstChild.resize( nCapacity );
	m_HintNextSibling.resize( nCapacity );
	m_HintAllocationIndex.resize( nCapacity );
	m_HintQueue.resize( nCapacity );
	m_HintQueued.resize( nCapacity );
	m_HintCpuRecords.resize( ( nCapacity + 1 ) * sizeof( t3dVTAncestorHint_t ) );
	m_HintWorkIndices.resize( T3D_VT_HINT_MAX_REBUILD_PER_FRAME );
	m_HintUploadScratch.resize( T3D_VT_HINT_MAX_REBUILD_PER_FRAME * sizeof( t3dVTAncestorHint_t ) );
	if ( m_HintFirstChild.size() != nCapacity || m_HintNextSibling.size() != nCapacity || m_HintAllocationIndex.size() != nCapacity ||
	     m_HintQueue.size() != nCapacity || m_HintQueued.size() != nCapacity ||
	     m_HintCpuRecords.size() != ( nCapacity + 1 ) * sizeof( t3dVTAncestorHint_t ) ||
	     m_HintWorkIndices.size() != T3D_VT_HINT_MAX_REBUILD_PER_FRAME ||
	     m_HintUploadScratch.size() != T3D_VT_HINT_MAX_REBUILD_PER_FRAME * sizeof( t3dVTAncestorHint_t ) ) {
		shutdown_cold();
		return false;
	}
	memset( m_HintQueued.ptrw(), 0, ( size_t )nCapacity );
#endif
	// Reserve Godot-owned staging arrays before resizing any of them. Zone
	// allocations are charged separately by their backing allocation.
	const uint64_t database_bytes =
	    MAX( pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_MIP_LAYOUT_TABLE],
		     MAX( pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_PAGE_TABLE], pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_TAIL_METADATA] ) );
	uint64_t staging_bytes = pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_ALLOCATION_TABLE] + database_bytes +
	                         uint64_t( pLimits.nMaxVtPhysicalPageSets ) *
	                             ( sizeof( t3dVTPageTableEntry_t ) + 3u * sizeof( int32_t ) + sizeof( t3dVTUploadReservation_t ) );
	for ( uint32_t c = 0; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		staging_bytes += 2ull * physical_layer_bytes( c ) + pPlan.nVtTailBytes[c];
	}
	if ( !m_stagingMemory.resize( staging_bytes ) ) {
		return false;
	}

	uint64_t nNeutralTailBytesTotal = 0u;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c )
		nNeutralTailBytesTotal += pPlan.nVtTailBytes[c];
	UtilityFunctions::print( "[Terrain3D][VTInit] CPU STAGING PLAN allocation=", pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_ALLOCATION_TABLE],
	                         " B physical_page_sets=", pLimits.nMaxVtPhysicalPageSets, " neutral_tail_total=", nNeutralTailBytesTotal, " B",
	                         " tail_height=", pPlan.nVtTailAtlasHeight );
	const uint64_t nAllocationScratchBytes64 = pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_ALLOCATION_TABLE];
	if ( nAllocationScratchBytes64 == 0u || nAllocationScratchBytes64 > INT32_MAX ) {
		shutdown_cold();
		return false;
	}
	m_AllocationUpload.resize( ( int64_t )nAllocationScratchBytes64 );
	if ( m_AllocationUpload.size() != ( int64_t )nAllocationScratchBytes64 ) {
		shutdown_cold();
		return false;
	}
	uint64_t nDatabaseScratchBytes = pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_MIP_LAYOUT_TABLE];
	nDatabaseScratchBytes = MAX( nDatabaseScratchBytes, pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_PAGE_TABLE] );
	nDatabaseScratchBytes = MAX( nDatabaseScratchBytes, pPlan.nBufferBytes[T3D_VT_GPU_BUFFER_TAIL_METADATA] );
	if ( nDatabaseScratchBytes == 0u || nDatabaseScratchBytes > INT32_MAX ) {
		shutdown_cold();
		return false;
	}
	m_DatabaseUpload.resize( ( int64_t )nDatabaseScratchBytes );
	if ( m_DatabaseUpload.size() != ( int64_t )nDatabaseScratchBytes ) {
		shutdown_cold();
		return false;
	}
	const uint32_t nRetiredScratchCapacity = pLimits.nMaxVtPhysicalPageSets != 0u ? pLimits.nMaxVtPhysicalPageSets : 1u;
	const uint64_t nPageScratchBytes64 = ( uint64_t )nRetiredScratchCapacity * sizeof( t3dVTPageTableEntry_t );
	if ( nPageScratchBytes64 > INT32_MAX ) {
		shutdown_cold();
		return false;
	}
	m_PageEntryUpload.resize( ( int64_t )nPageScratchBytes64 );
	m_RetiredPageScratch.resize( ( int64_t )nRetiredScratchCapacity );
	m_RetiredSlotScratch.resize( ( int64_t )nRetiredScratchCapacity );
	m_UploadBatchOrder.resize( ( int64_t )nRetiredScratchCapacity );
	m_UploadBatchReservations.resize( ( int64_t )( ( uint64_t )nRetiredScratchCapacity * sizeof( t3dVTUploadReservation_t ) ) );
	if ( m_PageEntryUpload.size() != ( int64_t )nPageScratchBytes64 || m_RetiredPageScratch.size() != ( int64_t )nRetiredScratchCapacity ||
	     m_RetiredSlotScratch.size() != ( int64_t )nRetiredScratchCapacity ||
	     m_UploadBatchOrder.size() != ( int64_t )nRetiredScratchCapacity ||
	     m_UploadBatchReservations.size() != ( int64_t )( ( uint64_t )nRetiredScratchCapacity * sizeof( t3dVTUploadReservation_t ) ) ) {
		shutdown_cold();
		return false;
	}
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		const uint32_t nBytes = physical_layer_bytes( c );
		if ( nBytes == 0u ) {
			shutdown_cold();
			return false;
		}
		m_PhysicalUpload[c].resize( nBytes );
		if ( m_PhysicalUpload[c].size() != ( int64_t )nBytes ) {
			shutdown_cold();
			return false;
		}
	}
	uint64_t nReusedTailBytes = 0u;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		if ( m_NeutralTailUpload[c].size() == ( int64_t )m_Plan.nVtTailBytes[c] ) {
			nReusedTailBytes += m_Plan.nVtTailBytes[c];
		}
	}
	UtilityFunctions::print( "[Terrain3D][VTInit] NEUTRAL PAYLOAD PREP BEGIN bytes=", nNeutralTailBytesTotal,
	                         " reused_bytes=", nReusedTailBytes );
	if ( !initialize_neutral_payloads_cold() ) {
		UtilityFunctions::push_error( "[Terrain3D][VTInit] NEUTRAL PAYLOAD ALLOCATION FAIL" );
		shutdown_cold();
		return false;
	}
	UtilityFunctions::print( "[Terrain3D][VTInit] NEUTRAL PAYLOAD PREP READY bytes=", nNeutralTailBytesTotal );
	if ( !t3d_vt_feedback_grid_layout( pLimits.nMaxReadbackBytesPerFrame, m_feedbackGridLayout ) ||
	     m_feedbackGridLayout.m_nTotalBytes != m_Plan.nBufferBytes[T3D_VT_GPU_BUFFER_FEEDBACK_GRID] ) {
		shutdown_cold();
		return false;
	}
	m_feedbackGridReset.resize( m_feedbackGridLayout.m_nSlotBytes );
	if ( m_feedbackGridReset.size() != m_feedbackGridLayout.m_nSlotBytes ) {
		shutdown_cold();
		return false;
	}
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i ) {
		m_feedbackGridSlots[i].reset();
		m_feedbackGridReadbacks[i].resize( m_feedbackGridLayout.m_nSlotBytes );
		if ( m_feedbackGridReadbacks[i].size() != m_feedbackGridLayout.m_nSlotBytes ) {
			shutdown_cold();
			return false;
		}
	}
	if ( !m_stagingMemory.resize( m_stagingMemory.bytes() + ( uint64_t )m_feedbackGridLayout.m_nTotalBytes +
	                              m_feedbackGridLayout.m_nSlotBytes ) ) {
		shutdown_cold();
		return false;
	}
	const uint64_t feedback_bytes = m_Plan.nBufferBytes[T3D_VT_GPU_BUFFER_FRAGMENT_HASH];
	if ( !m_stagingMemory.resize( m_stagingMemory.bytes() + 2u * feedback_bytes + sizeof( t3dVTRequestGpuCounters_t ) +
	                              sizeof( t3dVTFragmentFeedbackParams_t ) ) ) {
		shutdown_cold();
		return false;
	}
	m_feedbackReset.resize( feedback_bytes );
	m_feedbackCounterReset.resize( sizeof( t3dVTRequestGpuCounters_t ) );
	m_feedbackParams.resize( sizeof( t3dVTFragmentFeedbackParams_t ) );
	memset( m_feedbackCounterReset.ptrw(), 0, ( size_t )m_feedbackCounterReset.size() );
	const uint32_t nHashCapacity = ( uint32_t )( m_feedbackReset.size() / sizeof( t3dVTRequestHashEntry_t ) );
	for ( uint32_t i = 0u; i < nHashCapacity; ++i ) {
		const t3dVTRequestHashEntry_t Entry{ UINT32_MAX, UINT32_MAX, 0u, 0u };
		memcpy( m_feedbackReset.ptrw() + ( size_t )i * sizeof( Entry ), &Entry, sizeof( Entry ) );
	}
	m_bCpuPrepared = true;
	return true;
}

bool Terrain3DVTGpu::prepare_storage_cold( const Terrain3DVTResourceImage *pResources, const t3dWorldLimits_t& pLimits )
{
	Terrain3DRdContext *const pContext = pResources ? pResources->get_rd_context() : NULL;
	RenderingDevice *const pRenderingDevice = pContext ? pContext->getDevice() : NULL;
	if ( !m_bCpuPrepared || m_bStorageReady || m_bExecutableReady || m_bInitialized ||
	     !t3d_vt_gpu_can_initialize( false, pResources, pRenderingDevice ) ||
	     memcmp( &pResources->get_plan(), &m_Plan, sizeof( m_Plan ) ) != 0 ) {
		return false;
	}
	const uint64_t nReadbackBytes = sizeof( t3dVTGpuCounters_t ) + ( uint64_t )pLimits.nMaxVtRequests * sizeof( t3dVTRuntimeRequest_t );
	if ( nReadbackBytes > pLimits.nMaxReadbackBytesPerFrame || nReadbackBytes > UINT32_MAX )
		return false;
	m_pRenderingDevice = pRenderingDevice;
	m_pResources = pResources;
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	if ( !invalidate_ancestor_header_hot() ) {
		shutdown_cold();
		return false;
	}
#endif
	UtilityFunctions::print( "[Terrain3D][VTInit] PHYSICAL BANK VALIDATION BEGIN" );
	if ( !validate_physical_bank_layout_cold() ) {
		UtilityFunctions::push_error( "[Terrain3D][VTInit] PHYSICAL BANK VALIDATION FAIL" );
		shutdown_cold();
		return false;
	}
	UtilityFunctions::print( "[Terrain3D][VTInit] PHYSICAL BANK VALIDATION READY" );
	m_bStorageReady = true;
	return true;
}

bool Terrain3DVTGpu::bind_executable_cold()
{
	while ( !m_bExecutableReady ) {
		if ( !service_bind_executable_cold( UINT32_MAX ) ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DVTGpu::service_bind_executable_cold( const uint32_t nMaxWorkUnits )
{
	if ( !m_bStorageReady || m_bInitialized || m_pRenderingDevice == NULL || m_pResources == NULL || nMaxWorkUnits == 0u ) {
		return false;
	}
	if ( m_bExecutableReady ) {
		return true;
	}
	// The scene predictor was owned by World. Shared VT publication only
	// uploads tables and textures, so no compute executable is required here.
	m_bExecutableReady = true;
	m_bInitialized = true;
	return true;
}

bool Terrain3DVTGpu::initialize_cold( const Terrain3DVTResourceImage *pResources, const t3dWorldLimits_t& pLimits )
{
	return pResources != NULL && prepare_cpu_cold( pResources->get_plan(), pLimits ) && prepare_storage_cold( pResources, pLimits ) &&
	       bind_executable_cold();
}

void Terrain3DVTGpu::shutdown_cold( const bool bPreserveTailStaging )
{
	++m_nFeedbackShaderGeneration;
	// Global RenderingDevice is submitted by Godot, not this extension.
	// sync() is only legal on a local device. Epoch invalidation below makes
	// already queued readbacks harmless when their callbacks arrive.
	m_feedbackCallback = godot::Callable();
	m_bHotSealed = false;
	if ( m_pResources != NULL && m_pResources->get_rd_context() != NULL )
		m_multiFeedbackGpu.shutdownCold( *m_pResources->get_rd_context() );
	++m_nFeedbackEpoch;
	m_nFeedbackFrame = UINT32_MAX;
	m_feedbackEpoch.invalidate( m_nFeedbackEpoch );
	m_bFeedbackWritten = false;
	if ( m_pResources != NULL && m_pResources->get_rd_context() != NULL ) {
		Terrain3DRdGlobalDescriptorCache *const pCache = m_pResources->get_rd_context()->tryGetGlobalDescriptorCacheHot();
		for ( uint32_t i = 0u; i < 8u * T3D_RD_FORWARD_COLOR_VARIANT_COUNT; ++i ) {
			if ( pCache ) {
				pCache->releaseDescriptorCold( m_feedbackSets[i] );
			}
			m_feedbackSets[i] = t3dRdDescriptorLease_t{};
			m_feedbackResolvedSets[i] = RID();
			m_feedbackSetShaders[i] = RID();
		}
	}
	invalidate_feedback_grid_hot();
	m_feedbackGridReset.clear();
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i )
		m_feedbackGridReadbacks[i].clear();
	m_feedbackGridLayout = t3dVTFeedbackGridLayout_t{};
	m_bFeedbackGridEnabled = false;
	m_feedbackReadback.clear();
	m_feedbackReset.clear();
	m_feedbackCounterReset.clear();
	m_feedbackParams.clear();
	m_AllocationUpload.resize( 0 );
	m_DatabaseUpload.resize( 0 );
	discard_staged_file_tails_warm();
	m_nPreparedFileTailSignature = 0u;
	m_PageEntryUpload.resize( 0 );
	shutdown_ancestor_hints_cold();
	m_RetiredPageScratch.resize( 0 );
	m_RetiredSlotScratch.resize( 0 );
	m_UploadBatchOrder.resize( 0 );
	m_UploadBatchReservations.resize( 0 );
	m_nUploadBatchCount = 0u;
	m_nDatabaseUploadStage = 0u;
	m_nDatabaseTailChannel = 0u;
	m_bUploadBatchActive = false;
	m_bDatabaseUploadBegun = false;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		m_PhysicalUpload[c].resize( 0 );
		m_NeutralPhysicalUpload[c].resize( 0 );
		if ( !bPreserveTailStaging ) {
			m_NeutralTailUpload[c].resize( 0 );
		}
	}
	uint64_t retained_tail_bytes = 0;
	for ( uint32_t c = 0; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		retained_tail_bytes += m_NeutralTailUpload[c].size();
	}
	if ( retained_tail_bytes ) {
		m_stagingMemory.resize( retained_tail_bytes );
	} else {
		m_stagingMemory.reset();
	}
	m_pRenderingDevice = NULL;
	m_pResources = NULL;
	m_pDatabaseView = NULL;
	m_bDatabaseUploadedWarm = false;
	m_bCpuPrepared = false;
	m_bStorageReady = false;
	m_bExecutableReady = false;
	m_bInitialized = false;
}

void Terrain3DVTGpu::seal_hot_runtime()
{
	if ( m_feedbackCallback.is_valid() && !m_bFeedbackGridEnabled ) {
		( void )enable_feedback_grid_cold( true );
	}
	if ( !m_bInitialized ) {
		return;
	}
	m_nHotCreateEpoch = m_Stats.nGpuCreateCount;
	m_nHotFreeEpoch = m_Stats.nGpuFreeCount;
	m_bHotSealed = true;
}

bool Terrain3DVTGpu::update_allocation_quality_hot( const t3dVTResidencyState_t& rResidency )
{
	if ( !m_bInitialized || !m_bDatabaseUploadedWarm || m_pRenderingDevice == NULL || m_pResources == NULL ||
	     rResidency.Buffers.pAllocations == NULL )
		return false;
	const uint64_t nBytes = ( uint64_t )rResidency.nAllocationCount * sizeof( t3dVTAllocation_t );
	if ( nBytes == 0u || nBytes > UINT32_MAX || m_AllocationUpload.size() < ( int64_t )nBytes )
		return false;
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	if ( !invalidate_ancestor_header_hot() )
		return false;
	for ( uint32_t i = 0u; i < m_nHintPageCount; ++i ) {
		if ( !enqueue_ancestor_root_hot( i ) )
			return false;
	}
#endif
	memcpy( m_AllocationUpload.ptrw(), rResidency.Buffers.pAllocations, ( size_t )nBytes );
	if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_ALLOCATION_TABLE, 0u, ( uint32_t )nBytes, m_AllocationUpload ) )
		return false;
	return !m_bHotSealed || hot_epoch_unchanged();
}

bool Terrain3DVTGpu::upload_database_warm( const t3dVTDatabaseView_t& pDatabase, const t3dVTResidencyState_t& pResidency )
{
	if ( m_bDatabaseUploadedWarm ) {
		return false;
	}
	while ( !m_bDatabaseUploadedWarm ) {
		if ( !service_database_upload_warm( pDatabase, pResidency, UINT64_MAX, UINT32_MAX ) ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DVTGpu::prepare_file_tails_cold( const t3dVTDatabaseView_t& rDatabase )
{
	if ( rDatabase.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK || rDatabase.pFileSource == NULL || rDatabase.pHeader == NULL ||
	     m_bHotSealed ) {
		return false;
	}
	const t3dVTGpuResourcePlan_t& plan = m_Plan;
	for ( uint32_t iTail = 0u; iTail < rDatabase.nRuntimeTailCount; ++iTail ) {
		for ( uint32_t iChannel = 0u; iChannel < T3D_VT_CHANNEL_COUNT; ++iChannel ) {
			const uint32_t c = t3d_vt_physical_family_legacy_resource_channel( ( t3dVTPhysicalFormatFamily_t )iChannel );
			if ( c >= T3D_VT_CHANNEL_COUNT || m_NeutralTailUpload[c].size() != ( int64_t )plan.nVtTailBytes[c] ||
			     !t3d_vt_pack_runtime_copy_tail_mip_chain( rDatabase, iTail, iChannel, m_NeutralTailUpload[c].ptrw(), plan.nVtTailBytes[c],
			                                               plan.nVtTailAtlasWidth, plan.nVtTailAtlasHeight ) ) {
				return false;
			}
		}
	}
	m_nPreparedFileTailSignature = rDatabase.nPackageContentSignature;
	return true;
}

bool Terrain3DVTGpu::stage_file_tails_warm( const t3dVTDatabaseView_t& rDatabase )
{
	if ( rDatabase.pFileSource == NULL || rDatabase.pHeader == NULL || !m_bInitialized ) {
		return false;
	}
	m_nCandidateTailSignature = 0u;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		const uint64_t bytes = m_Plan.nVtTailBytes[c];
		if ( bytes > INT32_MAX ) {
			return false;
		}
		m_CandidateTailUpload[c].resize( ( int64_t )bytes );
		if ( m_CandidateTailUpload[c].size() != ( int64_t )bytes || m_NeutralPhysicalUpload[c].is_empty() ) {
			return false;
		}
		t3d_vt_repeat_neutral_payload_cold( m_CandidateTailUpload[c].ptrw(), ( size_t )bytes, m_NeutralPhysicalUpload[c].ptr(),
		                                    ( size_t )m_NeutralPhysicalUpload[c].size() );
	}
	for ( uint32_t iTail = 0u; iTail < rDatabase.nRuntimeTailCount; ++iTail ) {
		for ( uint32_t iChannel = 0u; iChannel < T3D_VT_CHANNEL_COUNT; ++iChannel ) {
			const uint32_t c = t3d_vt_physical_family_legacy_resource_channel( ( t3dVTPhysicalFormatFamily_t )iChannel );
			if ( c >= T3D_VT_CHANNEL_COUNT ||
			     !t3d_vt_pack_runtime_copy_tail_mip_chain( rDatabase, iTail, iChannel, m_CandidateTailUpload[c].ptrw(),
			                                               m_Plan.nVtTailBytes[c], m_Plan.nVtTailAtlasWidth, m_Plan.nVtTailAtlasHeight ) ) {
				return false;
			}
		}
	}
	m_nCandidateTailSignature = rDatabase.nPackageContentSignature;
	return true;
}

void Terrain3DVTGpu::backup_file_tails_warm( const t3dVTDatabaseView_t& rDatabase )
{
	m_nRollbackTailSignature = 0u;
	if ( rDatabase.pFileSource == NULL ) {
		return;
	}
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		// PackedByteArray copy-on-write retains the original atlas until the live
		// upload buffer is reset. No disk work is required if publication fails.
		m_RollbackTailUpload[c] = m_NeutralTailUpload[c];
	}
	m_nRollbackTailSignature = rDatabase.nPackageContentSignature;
}

void Terrain3DVTGpu::discard_staged_file_tails_warm()
{
	m_nCandidateTailSignature = 0u;
	m_nRollbackTailSignature = 0u;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		m_CandidateTailUpload[c].clear();
		m_RollbackTailUpload[c].clear();
	}
}

bool Terrain3DVTGpu::service_database_upload_warm( const t3dVTDatabaseView_t& pDatabase, const t3dVTResidencyState_t& pResidency,
                                                   const uint64_t nMaxBytes, const uint32_t nMaxWorkUnits )
{
	m_pDatabaseView = &pDatabase;
	if ( !m_bInitialized || m_bHotSealed || m_pRenderingDevice == NULL || pDatabase.pHeader == NULL || !pResidency.bInitialized ||
	     pResidency.bNeutralPublished || pResidency.Buffers.pAllocations == NULL || pResidency.Buffers.pTailMetadata == NULL ||
	     nMaxWorkUnits == 0u || nMaxBytes == 0u ) {
		return false;
	}
	if ( m_bDatabaseUploadedWarm ) {
		return true;
	}
	uint32_t nUnits = 0u;
	uint64_t nBytesUsed = 0u;
	if ( !m_bDatabaseUploadBegun ) {
		m_bDatabaseUploadBegun = true;
		m_nDatabaseUploadStage = 0u;
		m_nDatabaseTailChannel = 0u;
	}

	if ( m_nDatabaseUploadStage == 0u && nUnits < nMaxWorkUnits ) {
		if ( !invalidate_ancestor_header_hot() )
			return false;
		if ( !prepare_ancestor_graph_warm( pResidency ) )
			return false;
		const uint64_t nAllocationBytes = ( uint64_t )pResidency.nAllocationCount * sizeof( t3dVTAllocation_t );
		const uint64_t nMipLayoutCount = ( uint64_t )pResidency.nAllocationCount * T3D_VT_MAX_MIPS;
		const uint64_t nMipLayoutBytes = nMipLayoutCount * sizeof( t3dVTMipLayout_t );
		const uint64_t nPageBytes = ( uint64_t )pDatabase.pHeader->nPageCount * sizeof( t3dVTPageTableEntry_t );
		const uint64_t nTailMetadataBytes = ( uint64_t )pResidency.nTailCount * sizeof( t3dVTTailMetadata_t );
		if ( nAllocationBytes == 0u || nAllocationBytes > m_pResources->get_plan().nBufferBytes[T3D_VT_GPU_BUFFER_ALLOCATION_TABLE] ||
		     nAllocationBytes > UINT32_MAX || nMipLayoutBytes == 0u || nMipLayoutBytes > UINT32_MAX ||
		     nMipLayoutBytes > m_pResources->get_plan().nBufferBytes[T3D_VT_GPU_BUFFER_MIP_LAYOUT_TABLE] || nTailMetadataBytes == 0u ||
		     nTailMetadataBytes > m_pResources->get_plan().nBufferBytes[T3D_VT_GPU_BUFFER_TAIL_METADATA] ||
		     nTailMetadataBytes > UINT32_MAX || nPageBytes > m_pResources->get_plan().nBufferBytes[T3D_VT_GPU_BUFFER_PAGE_TABLE] ||
		     nPageBytes > UINT32_MAX || m_AllocationUpload.size() < ( int64_t )nAllocationBytes ) {
			return false;
		}
		// Validate every source table and packed tail before the first live-RD write.
		// Publication rollback remains a separate safety net for device errors.
		if ( pResidency.Buffers.pPageTable == NULL || m_DatabaseUpload.size() < ( int64_t )MAX( nMipLayoutBytes, nPageBytes ) ) {
			return false;
		}
		for ( uint32_t iAllocation = 0u; iAllocation < pResidency.nAllocationCount; ++iAllocation ) {
			const t3dVTAllocation_t& a = pResidency.Buffers.pAllocations[iAllocation];
			if ( pDatabase.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK && a.m_nPageCount == 0u ) {
				continue;
			}
			if ( a.m_nMipCount == 0u || a.m_nMipCount > T3D_VT_MAX_MIPS || a.m_nMip0WidthPages == 0u || a.m_nMip0HeightPages == 0u ||
			     ( uint64_t )a.m_nBaseVirtualPage + a.m_nPageCount > pDatabase.pHeader->nPageCount ) {
				return false;
			}
		}
		for ( uint32_t iPage = 0u; iPage < pDatabase.pHeader->nPageCount; ++iPage ) {
			t3dVTPageTableEntry_t preview;
			if ( !make_gpu_page_entry( iPage, pResidency.Buffers.pPageTable[iPage], preview ) ) {
				return false;
			}
		}
		if ( pDatabase.pFileSource != NULL && m_nPreparedFileTailSignature != pDatabase.nPackageContentSignature ) {
			const godot::PackedByteArray *source = NULL;
			if ( m_nCandidateTailSignature == pDatabase.nPackageContentSignature ) {
				source = m_CandidateTailUpload;
			} else if ( m_nRollbackTailSignature == pDatabase.nPackageContentSignature ) {
				source = m_RollbackTailUpload;
			}
			if ( source == NULL ) {
				return false;
			}
			for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
				if ( source[c].size() != m_NeutralTailUpload[c].size() ) {
					return false;
				}
				memcpy( m_NeutralTailUpload[c].ptrw(), source[c].ptr(), ( size_t )source[c].size() );
			}
		}
		if ( pDatabase.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK && pDatabase.pFileSource == NULL ) {
			const t3dVTGpuResourcePlan_t& plan = m_pResources->get_plan();
			for ( uint32_t iTail = 0u; iTail < pDatabase.nRuntimeTailCount; ++iTail ) {
				for ( uint32_t iChannel = 0u; iChannel < T3D_VT_CHANNEL_COUNT; ++iChannel ) {
					const uint32_t c = t3d_vt_physical_family_legacy_resource_channel( ( t3dVTPhysicalFormatFamily_t )iChannel );
					if ( c >= T3D_VT_CHANNEL_COUNT || m_NeutralTailUpload[c].size() != ( int64_t )plan.nVtTailBytes[c] ||
					     !t3d_vt_pack_runtime_copy_tail_mip_chain( pDatabase, iTail, iChannel, m_NeutralTailUpload[c].ptrw(),
					                                               plan.nVtTailBytes[c], plan.nVtTailAtlasWidth,
					                                               plan.nVtTailAtlasHeight ) ) {
						return false;
					}
				}
			}
		}
		memcpy( m_AllocationUpload.ptrw(), pResidency.Buffers.pAllocations, ( size_t )nAllocationBytes );
		if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_ALLOCATION_TABLE, 0u, ( uint32_t )nAllocationBytes, m_AllocationUpload,
		                   T3D_GPU_PUBLICATION ) )
			return false;

		PackedByteArray& Upload = m_DatabaseUpload;
		if ( Upload.size() < ( int64_t )nMipLayoutBytes )
			return false;
		memset( Upload.ptrw(), 0, ( size_t )nMipLayoutBytes );
		uint8_t *const pMipLayouts = Upload.ptrw();
		for ( uint32_t iAllocation = 0u; iAllocation < pResidency.nAllocationCount; ++iAllocation ) {
			const t3dVTAllocation_t& Allocation = pResidency.Buffers.pAllocations[iAllocation];
			uint64_t nBasePage = Allocation.m_nBaseVirtualPage;
			uint32_t nWidth = Allocation.m_nMip0WidthPages;
			uint32_t nHeight = Allocation.m_nMip0HeightPages;
			const uint64_t nAllocationEnd = ( uint64_t )Allocation.m_nBaseVirtualPage + Allocation.m_nPageCount;
			if ( pDatabase.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK && Allocation.m_nPageCount == 0u )
				continue;
			if ( Allocation.m_nMipCount == 0u || Allocation.m_nMipCount > T3D_VT_MAX_MIPS || nWidth == 0u || nHeight == 0u ||
			     nAllocationEnd > pDatabase.pHeader->nPageCount )
				return false;
			for ( uint32_t iMip = 0u; iMip < Allocation.m_nMipCount; ++iMip ) {
				t3dVTMipLayout_t Layout{};
				const uint64_t nMipPages = ( uint64_t )nWidth * nHeight;
				if ( nBasePage >= nAllocationEnd || nMipPages == 0u || nBasePage + nMipPages > nAllocationEnd ) {
					if ( pDatabase.nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK || Allocation.m_nPageCount == 0u )
						return false;
					Layout.m_nBaseVirtualPage = ( uint32_t )( nAllocationEnd - 1u );
					Layout.m_nWidthPages = 1u;
					Layout.m_nHeightPages = 1u;
				} else {
					if ( nBasePage > UINT32_MAX || nWidth > 0xffffu || nHeight > 0xffffu )
						return false;
					Layout.m_nBaseVirtualPage = ( uint32_t )nBasePage;
					Layout.m_nWidthPages = ( uint16_t )nWidth;
					Layout.m_nHeightPages = ( uint16_t )nHeight;
					nBasePage += nMipPages;
				}
				memcpy( pMipLayouts + ( ( uint64_t )iAllocation * T3D_VT_MAX_MIPS + iMip ) * sizeof( Layout ), &Layout, sizeof( Layout ) );
				nWidth = nWidth > 1u ? ( nWidth + 1u ) >> 1u : 1u;
				nHeight = nHeight > 1u ? ( nHeight + 1u ) >> 1u : 1u;
			}
		}
		if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_MIP_LAYOUT_TABLE, 0u, ( uint32_t )nMipLayoutBytes, Upload,
		                   T3D_GPU_PUBLICATION ) )
			return false;

		if ( Upload.size() < ( int64_t )nPageBytes )
			return false;
		for ( uint32_t i = 0u; i < pDatabase.pHeader->nPageCount; ++i ) {
			t3dVTPageTableEntry_t GpuEntry{};
			if ( !make_gpu_page_entry( i, pResidency.Buffers.pPageTable[i], GpuEntry ) )
				return false;
			memcpy( Upload.ptrw() + ( size_t )i * sizeof( GpuEntry ), &GpuEntry, sizeof( GpuEntry ) );
		}
		if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_PAGE_TABLE, 0u, ( uint32_t )nPageBytes, Upload, T3D_GPU_PUBLICATION ) )
			return false;

		if ( Upload.size() < ( int64_t )nTailMetadataBytes )
			return false;
		memcpy( Upload.ptrw(), pResidency.Buffers.pTailMetadata, ( size_t )nTailMetadataBytes );
		if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_TAIL_METADATA, 0u, ( uint32_t )nTailMetadataBytes, Upload,
		                   T3D_GPU_PUBLICATION ) )
			return false;

		if ( pDatabase.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK && pDatabase.pFileSource == NULL ) {
			const t3dVTGpuResourcePlan_t& Plan = m_pResources->get_plan();
			for ( uint32_t iTail = 0u; iTail < pDatabase.nRuntimeTailCount; ++iTail ) {
				for ( uint32_t iChannel = 0u; iChannel < T3D_VT_CHANNEL_COUNT; ++iChannel ) {
					const uint32_t nResourceChannel =
					    t3d_vt_physical_family_legacy_resource_channel( ( t3dVTPhysicalFormatFamily_t )iChannel );
					if ( nResourceChannel >= T3D_VT_CHANNEL_COUNT ||
					     m_NeutralTailUpload[nResourceChannel].size() != ( int64_t )Plan.nVtTailBytes[nResourceChannel] ||
					     !t3d_vt_pack_runtime_copy_tail_mip_chain( pDatabase, iTail, iChannel, m_NeutralTailUpload[nResourceChannel].ptrw(),
					                                               Plan.nVtTailBytes[nResourceChannel], Plan.nVtTailAtlasWidth,
					                                               Plan.nVtTailAtlasHeight ) )
						return false;
				}
			}
		}
		nBytesUsed += nAllocationBytes + nMipLayoutBytes + nPageBytes + nTailMetadataBytes;
		++nUnits;
		m_nDatabaseUploadStage = 1u;
	}

	while ( m_nDatabaseUploadStage == 1u && m_nDatabaseTailChannel < T3D_VT_CHANNEL_COUNT && nUnits < nMaxWorkUnits ) {
		const uint32_t c = m_nDatabaseTailChannel;
		const uint64_t nTailBytes = m_Plan.nVtTailBytes[c];
		if ( nTailBytes == 0u || nTailBytes > UINT32_MAX || m_NeutralTailUpload[c].size() != ( int64_t )nTailBytes ||
		     !m_pResources->get_vt_tail_rid( ( t3dVTResourceChannel_t )c ).is_valid() )
			return false;
		if ( nBytesUsed != 0u && nBytesUsed + nTailBytes > nMaxBytes )
			break;
		if ( !m_pResources->update_vt_tail_layer( ( t3dVTResourceChannel_t )c, 0u, m_NeutralTailUpload[c] ) )
			return false;
		m_Stats.nUploadBytes += nTailBytes;
		nBytesUsed += nTailBytes;
		++m_nDatabaseTailChannel;
		++nUnits;
	}
	if ( m_nDatabaseUploadStage == 1u && m_nDatabaseTailChannel == T3D_VT_CHANNEL_COUNT ) {
		m_bDatabaseUploadedWarm = true;
		m_bDatabaseUploadBegun = false;
		m_nDatabaseUploadStage = 0u;
		m_nDatabaseTailChannel = 0u;
	}
	return true;
}

bool Terrain3DVTGpu::begin_package_publication_warm()
{
	m_nPreparedFileTailSignature = 0u;
	if ( !m_bInitialized || m_bUploadBatchActive || m_pRenderingDevice == NULL ) {
		return false;
	}
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		if ( m_NeutralTailUpload[c].size() != ( int64_t )m_Plan.nVtTailBytes[c] || m_NeutralPhysicalUpload[c].is_empty() ) {
			return false;
		}
	}
	// Shared VT uses Godot's global RenderingDevice, which cannot sync().
	// Render-thread buffer and texture updates are ordered by the engine's
	// resource barriers. Invalidate pre-publication feedback so late readbacks
	// cannot be consumed against the replacement page table.
	++m_nFeedbackEpoch;
	m_nFeedbackFrame = UINT32_MAX;
	m_feedbackEpoch.invalidate( m_nFeedbackEpoch );
	invalidate_feedback_grid_hot();
	m_bFeedbackWritten = false;
	m_feedbackReadback.clear();
	m_bHotSealed = false;
	m_bDatabaseUploadedWarm = false;
	m_bDatabaseUploadBegun = false;
	m_nDatabaseUploadStage = 0u;
	m_nDatabaseTailChannel = 0u;
	// Previous publications wrote package tails into these retained buffers.
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		t3d_vt_repeat_neutral_payload_cold( m_NeutralTailUpload[c].ptrw(), ( size_t )m_NeutralTailUpload[c].size(),
		                                    m_NeutralPhysicalUpload[c].ptr(), ( size_t )m_NeutralPhysicalUpload[c].size() );
	}
	return true;
}

bool Terrain3DVTGpu::is_database_upload_pending_warm() const
{
	return m_bDatabaseUploadBegun && !m_bDatabaseUploadedWarm;
}

bool Terrain3DVTGpu::upload_neutral_page_set_warm( t3dVTResidencyState_t& rResidency, uint32_t pPhysicalSlot )
{
	if ( !m_bInitialized || !m_bDatabaseUploadedWarm || m_bHotSealed || m_pRenderingDevice == NULL ||
	     pPhysicalSlot >= rResidency.Buffers.nPhysicalPageSetCapacity || rResidency.bNeutralPublished ) {
		return false;
	}
	uint32_t nBank = 0u, nLayer = 0u;
	if ( !map_physical_slot_hot( pPhysicalSlot, nBank, nLayer ) ) {
		return false;
	}
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		const uint32_t nExpected = physical_layer_bytes( c );
		if ( nExpected == 0u || m_NeutralPhysicalUpload[c].size() != ( int64_t )nExpected ) {
			return false;
		}
		if ( !m_pResources->update_vt_bank_layer( ( t3dVTResourceChannel_t )c, nBank, nLayer, m_NeutralPhysicalUpload[c] ) ) {
			return false;
		}
		m_Stats.nUploadBytes += nExpected;
	}

	// Texture uploads are ordered before page-table publication by Godot 4.7.2 automatic RD barriers.
	t3dVTPageTableEntry_t Previous[T3D_VT_CHANNEL_COUNT] = {};
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		const uint32_t nRootPage = rResidency.nNeutralRootPage[c];
		if ( nRootPage == T3D_VT_INVALID_PAGE || nRootPage >= rResidency.pDatabase->pHeader->nPageCount ) {
			return false;
		}
		Previous[c] = rResidency.Buffers.pPageTable[nRootPage];
		t3dVTPageTableEntry_t Preview = Previous[c];
		Preview.nPhysicalSlot = pPhysicalSlot;
		Preview.nFlags |= T3D_VT_PAGE_RESIDENT | T3D_VT_PAGE_PINNED | T3D_VT_PAGE_NEUTRAL | T3D_VT_PAGE_ROOT;
		if ( !update_page_entry_hot( nRootPage, Preview ) ) {
			for ( uint32_t r = 0u; r < c; ++r ) {
				update_page_entry_hot( rResidency.nNeutralRootPage[r], Previous[r] );
			}
			return false;
		}
	}
	if ( !t3d_vt_commit_neutral_backing_warm( rResidency, pPhysicalSlot ) ) {
		for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
			update_page_entry_hot( rResidency.nNeutralRootPage[c], Previous[c] );
		}
		return false;
	}
	return true;
}

bool Terrain3DVTGpu::service_retired_slots_hot( t3dVTResidencyState_t& rResidency, const uint32_t pFrameId )
{
	if ( !m_bInitialized || !m_bHotSealed || !hot_epoch_unchanged() || rResidency.Buffers.pRetiredPageSets == NULL ) {
		++m_Stats.nHotViolationCount;
		return false;
	}
	const uint32_t nRetiredCount = rResidency.Stats.nRetiredSlotCount;
	if ( nRetiredCount == 0u ) {
		return true;
	}
	if ( nRetiredCount > ( uint32_t )m_RetiredPageScratch.size() || nRetiredCount > ( uint32_t )m_RetiredSlotScratch.size() ) {
		return false;
	}

	int32_t *const pPages = m_RetiredPageScratch.ptrw();
	int32_t *const pSlots = m_RetiredSlotScratch.ptrw();
	uint32_t nReadyCount = 0u;
	for ( uint32_t i = 0u; i < nRetiredCount; ++i ) {
		const uint32_t nRingIndex = ( rResidency.nRetireHead + i ) % rResidency.Buffers.nPhysicalPageSetCapacity;
		const t3dVTRetiredPageSet_t& Retired = rResidency.Buffers.pRetiredPageSets[nRingIndex];
		if ( !t3d_vt_retirement_ready_reference_debug( rResidency, Retired, pFrameId ) ) {
			++rResidency.Stats.nRetireDeferredCount;
			break;
		}
		if ( Retired.nPhysicalSlot >= rResidency.Buffers.nPhysicalPageSetCapacity ||
		     Retired.nVirtualPage >= rResidency.pDatabase->pHeader->nPageCount ) {
			return false;
		}
		const t3dVTPhysicalSlot_t& Slot = rResidency.Buffers.pPhysicalPageSets[Retired.nPhysicalSlot];
		const t3dVTPageTableEntry_t& Entry = rResidency.Buffers.pPageTable[Retired.nVirtualPage];
		if ( ( Slot.m_nFlags & T3D_VT_SLOT_RETIRED ) == 0u || Slot.m_nVirtualPage != Retired.nVirtualPage ||
		     Slot.m_nGeneration != Retired.nGeneration || ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u ||
		     Entry.nPhysicalSlot != Retired.nPhysicalSlot ) {
			return false;
		}
		uint32_t nInsert = nReadyCount;
		while ( nInsert > 0u && ( uint32_t )pPages[nInsert - 1u] > Retired.nVirtualPage ) {
			pPages[nInsert] = pPages[nInsert - 1u];
			pSlots[nInsert] = pSlots[nInsert - 1u];
			--nInsert;
		}
		pPages[nInsert] = ( int32_t )Retired.nVirtualPage;
		pSlots[nInsert] = ( int32_t )Retired.nPhysicalSlot;
		++nReadyCount;
	}
	if ( nReadyCount == 0u ) {
		return false;
	}

	for ( uint32_t i = 0u; i < nReadyCount; ++i ) {
		if ( !invalidate_ancestor_page_hot( ( uint32_t )pPages[i] ) )
			return false;
	}
	uint32_t nFirst = 0u;
	while ( nFirst < nReadyCount ) {
		uint32_t nLast = nFirst + 1u;
		while ( nLast < nReadyCount && ( uint32_t )pPages[nLast] == ( uint32_t )pPages[nLast - 1u] + 1u ) {
			++nLast;
		}
		const uint32_t nRunCount = nLast - nFirst;
		const uint32_t nRunBytes = nRunCount * sizeof( t3dVTPageTableEntry_t );
		if ( nRunBytes > ( uint32_t )m_PageEntryUpload.size() ) {
			return false;
		}
		for ( uint32_t i = 0u; i < nRunCount; ++i ) {
			const uint32_t nPage = ( uint32_t )pPages[nFirst + i];
			t3dVTPageTableEntry_t Invalid = rResidency.Buffers.pPageTable[nPage];
			Invalid.nPhysicalSlot = T3D_VT_INVALID_SLOT;
			Invalid.nFlags &= ( uint16_t )~T3D_VT_PAGE_RESIDENT;
			t3dVTPageTableEntry_t GpuEntry = {};
			if ( !make_gpu_page_entry( nPage, Invalid, GpuEntry ) ) {
				return false;
			}
			memcpy( m_PageEntryUpload.ptrw() + i * sizeof( t3dVTPageTableEntry_t ), &GpuEntry, sizeof( GpuEntry ) );
		}
		const uint64_t nOffset64 = ( uint64_t )( uint32_t )pPages[nFirst] * sizeof( t3dVTPageTableEntry_t );
		if ( nOffset64 > UINT32_MAX ||
		     !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_PAGE_TABLE, ( uint32_t )nOffset64, nRunBytes, m_PageEntryUpload ) ) {
			++m_Stats.nRetireRetryCount;
			++rResidency.Stats.nRetireDeferredCount;
			return false;
		}
		m_Stats.nPageTableUpdateBytes += nRunBytes;
		++m_Stats.nPageTableUpdateCalls;
		nFirst = nLast;
	}

	// Buffer updates become visible to later RD consumers through Godot 4.7.2 automatic barriers.
	const uint32_t nOldHead = rResidency.nRetireHead;
	for ( uint32_t i = 0u; i < nReadyCount; ++i ) {
		if ( !t3d_vt_finalize_eviction_hot( rResidency, ( uint32_t )pSlots[i], ( uint32_t )pPages[i], pFrameId ) ) {
			return false;
		}
	}
	for ( uint32_t i = 0u; i < nReadyCount; ++i ) {
		t3dVTRetiredPageSet_t& Retired =
		    rResidency.Buffers.pRetiredPageSets[( nOldHead + i ) % rResidency.Buffers.nPhysicalPageSetCapacity];
		Retired.nPhysicalSlot = T3D_VT_INVALID_SLOT;
		Retired.nVirtualPage = T3D_VT_INVALID_PAGE;
		Retired.nGeneration = 0u;
		Retired.nReleaseFrame = 0u;
	}
	rResidency.nRetireHead = ( nOldHead + nReadyCount ) % rResidency.Buffers.nPhysicalPageSetCapacity;
	return nReadyCount == nRetiredCount;
}
bool Terrain3DVTGpu::begin_upload_batch_hot()
{
	if ( !m_bInitialized || !m_bHotSealed || m_bUploadBatchActive || !hot_epoch_unchanged() ) {
		return false;
	}
	m_nUploadBatchCount = 0u;
	m_bUploadBatchActive = true;
	return true;
}

void Terrain3DVTGpu::abort_upload_batch_hot( t3dVTResidencyState_t& rResidency )
{
	if ( m_UploadBatchReservations.size() >= ( int64_t )( m_nUploadBatchCount * sizeof( t3dVTUploadReservation_t ) ) ) {
		for ( uint32_t i = 0u; i < m_nUploadBatchCount; ++i ) {
			t3d_vt_abort_upload_hot( rResidency, t3dVtReadReservation( m_UploadBatchReservations, i ) );
		}
	}
	m_nUploadBatchCount = 0u;
	m_bUploadBatchActive = false;
}

bool Terrain3DVTGpu::publish_upload_batch_page_table_hot( const t3dVTResidencyState_t& rResidency )
{
	T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_PAGE_TABLE_PUBLISH );
	if ( m_nUploadBatchCount == 0u ) {
		return true;
	}
	if ( m_nUploadBatchCount > ( uint32_t )m_UploadBatchOrder.size() ||
	     m_UploadBatchReservations.size() < ( int64_t )( m_nUploadBatchCount * sizeof( t3dVTUploadReservation_t ) ) ) {
		return false;
	}

	int32_t *const pOrder = m_UploadBatchOrder.ptrw();
	for ( uint32_t i = 0u; i < m_nUploadBatchCount; ++i ) {
		pOrder[i] = ( int32_t )i;
		uint32_t nInsert = i;
		while ( nInsert > 0u && t3dVtReadReservation( m_UploadBatchReservations, ( uint32_t )pOrder[nInsert - 1u] ).nVirtualPage >
		                            t3dVtReadReservation( m_UploadBatchReservations, i ).nVirtualPage ) {
			pOrder[nInsert] = pOrder[nInsert - 1u];
			--nInsert;
		}
		pOrder[nInsert] = ( int32_t )i;
	}
	uint32_t nFirst = 0u;
	while ( nFirst < m_nUploadBatchCount ) {
		uint32_t nLast = nFirst + 1u;
		while ( nLast < m_nUploadBatchCount ) {
			const uint32_t nPrev = t3dVtReadReservation( m_UploadBatchReservations, ( uint32_t )pOrder[nLast - 1u] ).nVirtualPage;
			const uint32_t nNext = t3dVtReadReservation( m_UploadBatchReservations, ( uint32_t )pOrder[nLast] ).nVirtualPage;
			if ( nNext != nPrev + 1u ) {
				break;
			}
			++nLast;
		}
		const uint32_t nRunCount = nLast - nFirst;
		const uint32_t nRunBytes = nRunCount * sizeof( t3dVTPageTableEntry_t );
		if ( nRunBytes > ( uint32_t )m_PageEntryUpload.size() ) {
			return false;
		}
		for ( uint32_t i = 0u; i < nRunCount; ++i ) {
			const t3dVTUploadReservation_t& Reservation = t3dVtReadReservation( m_UploadBatchReservations, ( uint32_t )pOrder[nFirst + i] );
			if ( Reservation.nVirtualPage >= rResidency.pDatabase->pHeader->nPageCount ) {
				return false;
			}
			t3dVTPageTableEntry_t Preview = rResidency.Buffers.pPageTable[Reservation.nVirtualPage];
			Preview.nPhysicalSlot = Reservation.nPhysicalSlot;
			Preview.nResolvedMip = Reservation.nWantedMip;
			Preview.nFlags |= T3D_VT_PAGE_RESIDENT;
			t3dVTPageTableEntry_t GpuEntry = {};
			if ( !make_gpu_page_entry( Reservation.nVirtualPage, Preview, GpuEntry ) ) {
				return false;
			}
			memcpy( m_PageEntryUpload.ptrw() + i * sizeof( t3dVTPageTableEntry_t ), &GpuEntry, sizeof( GpuEntry ) );
		}
		const uint32_t nFirstPage = t3dVtReadReservation( m_UploadBatchReservations, ( uint32_t )pOrder[nFirst] ).nVirtualPage;
		const uint64_t nOffset64 = ( uint64_t )nFirstPage * sizeof( t3dVTPageTableEntry_t );
		if ( nOffset64 > UINT32_MAX ||
		     !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_PAGE_TABLE, ( uint32_t )nOffset64, nRunBytes, m_PageEntryUpload ) ) {
			return false;
		}
		m_Stats.nPageTableUpdateBytes += nRunBytes;
		++m_Stats.nPageTableUpdateCalls;
		nFirst = nLast;
	}
	++m_Stats.nPageTableBatchCount;
	m_Stats.nPageTableBatchEntries += m_nUploadBatchCount;
	return true;
}

bool Terrain3DVTGpu::end_upload_batch_hot( t3dVTResidencyState_t& rResidency, const uint32_t pFrameId )
{
	if ( !m_bUploadBatchActive || !m_bInitialized || !m_bHotSealed || !hot_epoch_unchanged() ) {
		return false;
	}
	if ( m_nUploadBatchCount == 0u ) {
		m_bUploadBatchActive = false;
		return true;
	}

	uint32_t nVictimCount = 0u;
	for ( uint32_t i = 0u; i < m_nUploadBatchCount; ++i ) {
		if ( !t3d_vt_can_commit_upload_hot( rResidency, t3dVtReadReservation( m_UploadBatchReservations, i ) ) ) {
			abort_upload_batch_hot( rResidency );
			return false;
		}
		// P07: pairwise page/slot/victim conflicts were rejected before texture
		// uploads in upload_page_set_commit_hot. Do not repeat that O(N^2)
		// verification during final page-table publication.
		if ( t3dVtReadReservation( m_UploadBatchReservations, i ).nVictimPage != T3D_VT_INVALID_PAGE ) {
			++nVictimCount;
		}
	}
	if ( !t3dVtUploadVictimCapacityValid( nVictimCount, rResidency.Buffers.nPhysicalPageSetCapacity,
	                                      rResidency.Stats.nRetiredSlotCount ) ) {
		abort_upload_batch_hot( rResidency );
		return false;
	}
	// Hint validity is revoked before a page can change. Each changed page
	// invalidates its descendants through the prebuilt reverse-parent index.
	for ( uint32_t i = 0u; i < m_nUploadBatchCount; ++i ) {
		const t3dVTUploadReservation_t reservation = t3dVtReadReservation( m_UploadBatchReservations, i );
		if ( !invalidate_ancestor_page_hot( reservation.nVirtualPage ) ) {
			abort_upload_batch_hot( rResidency );
			return false;
		}
	}
	// Publish all logical mappings only after every backing texture update is staged.
	// CPU residency state remains unchanged until the full page-table batch is visible.
	if ( !publish_upload_batch_page_table_hot( rResidency ) ) {
		bool bRestored = true;
		for ( uint32_t i = 0u; i < m_nUploadBatchCount; ++i ) {
			if ( !update_page_entry_hot(
			         t3dVtReadReservation( m_UploadBatchReservations, i ).nVirtualPage,
			         rResidency.Buffers.pPageTable[t3dVtReadReservation( m_UploadBatchReservations, i ).nVirtualPage] ) ) {
				bRestored = false;
			}
		}
		abort_upload_batch_hot( rResidency );
		if ( !bRestored ) {
			// A partially rolled-back table cannot be safely used for later draws.
			++m_Stats.nHotViolationCount;
			m_bInitialized = false;
			m_bHotSealed = true;
		}
		return false;
	}
	for ( uint32_t i = 0u; i < m_nUploadBatchCount; ++i ) {
		const uint32_t nMask =
		    rResidency.pDatabase->nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK
		        ? ( 1u << rResidency.pDatabase->pPages[t3dVtReadReservation( m_UploadBatchReservations, i ).nVirtualPage].nChannel )
		        : T3D_VT_PAGE_SET_CHANNEL_MASK;
		if ( !t3d_vt_commit_upload_hot( rResidency, t3dVtReadReservation( m_UploadBatchReservations, i ), nMask, pFrameId ) ) {
			++m_Stats.nHotViolationCount;
			m_nUploadBatchCount = 0u;
			m_bUploadBatchActive = false;
			return false;
		}
	}
	m_nUploadBatchCount = 0u;
	m_bUploadBatchActive = false;
	return hot_epoch_unchanged();
}

bool Terrain3DVTGpu::upload_batch_begin_callback( void *pUser )
{
	Terrain3DVTGpu *const pSelf = ( Terrain3DVTGpu * )pUser;
	return pSelf != NULL && pSelf->begin_upload_batch_hot();
}

bool Terrain3DVTGpu::upload_batch_end_callback( t3dVTResidencyState_t& rResidency, const uint32_t pFrameId, void *pUser )
{
	Terrain3DVTGpu *const pSelf = ( Terrain3DVTGpu * )pUser;
	return pSelf != NULL && pSelf->end_upload_batch_hot( rResidency, pFrameId );
}

bool Terrain3DVTGpu::upload_page_set_commit_hot( t3dVTResidencyState_t& rResidency, const t3dVTUploadReservation_t& pReservation,
                                                 const t3dVTPageSetPayloadView_t& pPayloads, uint32_t pFrameId )
{
	T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_PHYSICAL_UPLOAD );
	if ( !m_bInitialized || !m_bHotSealed || m_pRenderingDevice == NULL || !hot_epoch_unchanged() ||
	     ( pPayloads.nPresentMask & ~T3D_VT_PAGE_SET_CHANNEL_MASK ) != 0u || !t3d_vt_can_commit_upload_hot( rResidency, pReservation ) ) {
		++m_Stats.nHotViolationCount;
		return false;
	}
	// P07: reject conflicting/overflowing reservations before any physical
	// texture is mutated. The previous post-upload rejection could leave
	// freshly decoded bytes in a slot belonging to another queued page.
	if ( m_bUploadBatchActive ) {
		const uint32_t nCapacity = ( uint32_t )( m_UploadBatchReservations.size() / sizeof( t3dVTUploadReservation_t ) );
		if ( m_nUploadBatchCount >= nCapacity ) {
			++m_Stats.nUploadPreflightRejected;
			return false;
		}
		uint32_t nPendingVictims = pReservation.nVictimPage != T3D_VT_INVALID_PAGE ? 1u : 0u;
		for ( uint32_t i = 0u; i < m_nUploadBatchCount; ++i ) {
			const t3dVTUploadReservation_t Existing = t3dVtReadReservation( m_UploadBatchReservations, i );
			if ( t3dVtUploadReservationsConflict( Existing, pReservation, T3D_VT_INVALID_PAGE ) ) {
				++m_Stats.nUploadPreflightRejected;
				return false;
			}
			nPendingVictims += Existing.nVictimPage != T3D_VT_INVALID_PAGE ? 1u : 0u;
		}
		const uint32_t nPhysicalCapacity = rResidency.Buffers.nPhysicalPageSetCapacity;
		const uint32_t nRetired = rResidency.Stats.nRetiredSlotCount;
		if ( !t3dVtUploadVictimCapacityValid( nPendingVictims, nPhysicalCapacity, nRetired ) ) {
			++m_Stats.nUploadPreflightRejected;
			return false;
		}
	}
	uint32_t nBank = 0u, nLayer = 0u;
	if ( !map_physical_slot_hot( pReservation.nPhysicalSlot, nBank, nLayer ) ) {
		return false;
	}

	uint64_t nUploadedBytes = 0u;
	const bool bV1 = rResidency.pDatabase != NULL && rResidency.pDatabase->nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK;
	const uint32_t nFirstChannel = bV1 ? rResidency.pDatabase->pPages[pReservation.nVirtualPage].nChannel : 0u;
	const uint32_t nLastChannel = bV1 ? nFirstChannel + 1u : T3D_VT_CHANNEL_COUNT;
	if ( bV1 && ( nFirstChannel >= T3D_VT_CHANNEL_COUNT || pPayloads.nPresentMask != ( 1u << nFirstChannel ) ) ) {
		return false;
	}
	// Preflight all participating channels together. This prevents a failure
	// in a later channel from exposing a partially overwritten page-set.
	for ( uint32_t c = nFirstChannel; c < nLastChannel; ++c ) {
		const uint32_t nResourceChannel = bV1 ? t3d_vt_physical_family_legacy_resource_channel( ( t3dVTPhysicalFormatFamily_t )c ) : c;
		if ( nResourceChannel >= T3D_VT_CHANNEL_COUNT ) {
			++m_Stats.nUploadPreflightRejected;
			return false;
		}
		const bool bPresent = ( pPayloads.nPresentMask & ( 1u << c ) ) != 0u;
		const uint8_t *const pBytes = bPresent ? pPayloads.pChannelBytes[c] : m_NeutralPhysicalUpload[nResourceChannel].ptr();
		const uint32_t nBytes = bPresent ? pPayloads.nChannelBytes[c] : ( uint32_t )m_NeutralPhysicalUpload[nResourceChannel].size();
		const uint32_t nExpected = physical_layer_bytes( nResourceChannel );
		if ( !t3dVtUploadChannelViewValid( nResourceChannel, T3D_VT_CHANNEL_COUNT, pBytes, nBytes, nExpected,
		                                   m_PhysicalUpload[nResourceChannel].size() ) ) {
			++m_Stats.nUploadPreflightRejected;
			return false;
		}
	}
	for ( uint32_t c = nFirstChannel; c < nLastChannel; ++c ) {
		const uint32_t nResourceChannel = bV1 ? t3d_vt_physical_family_legacy_resource_channel( ( t3dVTPhysicalFormatFamily_t )c ) : c;
		if ( nResourceChannel >= T3D_VT_CHANNEL_COUNT ) {
			return false;
		}
		const uint32_t nExpected = physical_layer_bytes( nResourceChannel );
		const bool bPresent = ( pPayloads.nPresentMask & ( 1u << c ) ) != 0u;
		const uint8_t *const pBytes = bPresent ? pPayloads.pChannelBytes[c] : m_NeutralPhysicalUpload[nResourceChannel].ptr();
		// P07: neutral pages are already persistent byte arrays and do not
		// need another memcpy into per-channel mutable staging.
		if ( bPresent ) {
			memcpy( m_PhysicalUpload[nResourceChannel].ptrw(), pBytes, nExpected );
		} else {
			m_Stats.nNeutralStageCopyBytesSaved += nExpected;
		}
		const PackedByteArray& rPhysicalBytes = bPresent ? m_PhysicalUpload[nResourceChannel] : m_NeutralPhysicalUpload[nResourceChannel];
		if ( !m_pResources->update_vt_bank_layer( ( t3dVTResourceChannel_t )nResourceChannel, nBank, nLayer, rPhysicalBytes ) ) {
			return false;
		}
		nUploadedBytes += nExpected;
		T3D_STREAM_PROFILE_BYTES( T3D_SP_VT_PHYSICAL_UPLOAD, nExpected );
	}
	// All participating backing texture layers are staged before the logical page mapping is published.

	// Godot 4.7.2 automatically orders backing texture transfers before later shader reads.
	// Victim publication remains valid until the delayed retirement service invalidates it.

	if ( m_bUploadBatchActive ) {
		t3dVtWriteReservation( m_UploadBatchReservations, m_nUploadBatchCount++, pReservation );
		m_Stats.nUploadBytes += nUploadedBytes;
		return hot_epoch_unchanged();
	}

	const t3dVTPageTableEntry_t PreviousEntry = rResidency.Buffers.pPageTable[pReservation.nVirtualPage];
	t3dVTPageTableEntry_t Preview = PreviousEntry;
	Preview.nPhysicalSlot = pReservation.nPhysicalSlot;
	Preview.nResolvedMip = pReservation.nWantedMip;
	Preview.nFlags |= T3D_VT_PAGE_RESIDENT;
	if ( !update_page_entry_hot( pReservation.nVirtualPage, Preview ) ) {
		return false;
	}
	// The page-table buffer update is automatically synchronized before later RD consumers.
	const uint32_t nCommitMask = bV1 ? pPayloads.nPresentMask : T3D_VT_PAGE_SET_CHANNEL_MASK;
	if ( !t3d_vt_commit_upload_hot( rResidency, pReservation, nCommitMask, pFrameId ) ) {
		update_page_entry_hot( pReservation.nVirtualPage, PreviousEntry );
		return false;
	}
	m_Stats.nUploadBytes += nUploadedBytes;
	return hot_epoch_unchanged();
}

bool Terrain3DVTGpu::upload_commit_hot( t3dVTResidencyState_t& rResidency, const t3dVTUploadReservation_t& pReservation,
                                        const uint8_t *pDecodedBytes, uint32_t pDecodedByteCount, uint32_t pFrameId )
{
	if ( pDecodedBytes == NULL || rResidency.pDatabase == NULL || rResidency.pDatabase->pHeader == NULL ||
	     pReservation.nVirtualPage >= rResidency.pDatabase->pHeader->nPageCount ) {
		return false;
	}
	const t3dVTPageFileRecord_t& Page = rResidency.pDatabase->pPages[pReservation.nVirtualPage];
	const uint32_t nResourceChannel = t3d_vt_physical_family_legacy_resource_channel( ( t3dVTPhysicalFormatFamily_t )Page.nChannel );
	if ( Page.nChannel >= T3D_VT_CHANNEL_COUNT || nResourceChannel >= T3D_VT_CHANNEL_COUNT ||
	     pDecodedByteCount != physical_layer_bytes( nResourceChannel ) ) {
		return false;
	}
	t3dVTPageSetPayloadView_t Payloads = {};
	Payloads.pChannelBytes[Page.nChannel] = pDecodedBytes;
	Payloads.nChannelBytes[Page.nChannel] = pDecodedByteCount;
	Payloads.nPresentMask = 1u << Page.nChannel;
	return upload_page_set_commit_hot( rResidency, pReservation, Payloads, pFrameId );
}

bool Terrain3DVTGpu::upload_commit_callback( t3dVTResidencyState_t& rResidency, const t3dVTUploadReservation_t& pReservation,
                                             const uint8_t *pDecodedBytes, uint32_t pDecodedByteCount, uint32_t pFrameId, void *pUser )
{
	Terrain3DVTGpu *pSelf = ( Terrain3DVTGpu * )pUser;
	return pSelf != NULL && pSelf->upload_commit_hot( rResidency, pReservation, pDecodedBytes, pDecodedByteCount, pFrameId );
}

void Terrain3DVTGpu::set_fragment_feedback_callback_cold( const godot::Callable& rCallback )
{
	m_feedbackCallback = rCallback;
	++m_nFeedbackShaderGeneration;
}

bool Terrain3DVTGpu::is_fragment_feedback_shader_ready() const
{
	if ( !m_bFeedbackGridEnabled || !is_readback_enabled() || !m_pResources ) {
		return false;
	}
	Terrain3DRdContext *const pContext = m_pResources->get_rd_context();
	if ( !pContext || !pContext->getDevice() || !pContext->tryGetGlobalDescriptorCacheHot() ) {
		return false;
	}
	const t3dVTGpuBufferId_t buffers[5] = { T3D_VT_GPU_BUFFER_ALLOCATION_TABLE, T3D_VT_GPU_BUFFER_PAGE_TABLE,
	                                        T3D_VT_GPU_BUFFER_FEEDBACK_GRID, T3D_VT_GPU_BUFFER_FRAGMENT_COUNTERS,
	                                        T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS };
	for ( uint32_t i = 0u; i < 5u; ++i ) {
		if ( !m_pResources->get_buffer_rid( buffers[i] ).is_valid() ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DVTGpu::is_readback_enabled() const
{
	if ( !m_bInitialized || !m_feedbackCallback.is_valid() ) {
		return false;
	}
	if ( m_bFeedbackGridEnabled ) {
		return m_feedbackGridLayout.m_nSlotBytes >= sizeof( t3dVTFeedbackGridHeader_t ) + sizeof( uint32_t ) &&
		       m_feedbackGridLayout.m_nSlotBytes <= m_Limits.nMaxReadbackBytesPerFrame;
	}
	return ( uint64_t )m_feedbackReset.size() + sizeof( t3dVTRequestGpuCounters_t ) <= m_Limits.nMaxReadbackBytesPerFrame &&
	       m_feedbackReset.size() >= 2 * ( int64_t )sizeof( t3dVTRequestHashEntry_t );
}

bool Terrain3DVTGpu::is_initialized() const
{
	return m_bInitialized;
}
bool Terrain3DVTGpu::is_storage_ready() const
{
	return m_bStorageReady;
}
bool Terrain3DVTGpu::is_executable_ready() const
{
	return m_bExecutableReady;
}
uint64_t Terrain3DVTGpu::get_reserved_gpu_bytes() const
{
	uint64_t nBytes = 0u;
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_CHANNEL_COUNT; ++iChannel ) {
		nBytes += m_Plan.nVtPhysicalBytes[iChannel] + m_Plan.nVtTailBytes[iChannel];
	}
	static const t3dVTGpuBufferId_t s_vtBuffers[] = {
	    T3D_VT_GPU_BUFFER_ALLOCATION_TABLE, T3D_VT_GPU_BUFFER_PAGE_TABLE,    T3D_VT_GPU_BUFFER_MIP_LAYOUT_TABLE,
	    T3D_VT_GPU_BUFFER_TAIL_METADATA,    T3D_VT_GPU_BUFFER_FRAGMENT_HASH, T3D_VT_GPU_BUFFER_FRAGMENT_COUNTERS,
	    T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS,
	};
	for ( size_t iBuffer = 0u; iBuffer < sizeof( s_vtBuffers ) / sizeof( s_vtBuffers[0] ); ++iBuffer ) {
		nBytes += m_Plan.nBufferBytes[s_vtBuffers[iBuffer]];
	}
	return nBytes;
}

const t3dVTGpuStats_t& Terrain3DVTGpu::get_stats() const
{
	return m_Stats;
}

bool Terrain3DVTGpu::make_gpu_page_entry( const uint32_t nVirtualPage, const t3dVTPageTableEntry_t& pCpuEntry,
                                          t3dVTPageTableEntry_t& rGpuEntry ) const
{
	rGpuEntry = pCpuEntry;
	if ( ( pCpuEntry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u || pCpuEntry.nPhysicalSlot == T3D_VT_INVALID_SLOT ) {
		rGpuEntry.nPhysicalSlot = T3D_VT_INVALID_SLOT;
		return true;
	}
	uint32_t nBank = 0u, nLayer = 0u;
	if ( !map_physical_slot_hot( pCpuEntry.nPhysicalSlot, nBank, nLayer ) ) {
		return false;
	}
	if ( m_pDatabaseView != NULL && m_pDatabaseView->nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK ) {
		if ( m_pDatabaseView->pPages == NULL || nVirtualPage >= m_pDatabaseView->pHeader->nPageCount ) {
			return false;
		}
		const uint32_t nFamily = m_pDatabaseView->pPages[nVirtualPage].nChannel;
		if ( nFamily >= T3D_VT_PHYSICAL_FORMAT_COUNT ) {
			return false;
		}
		rGpuEntry.nPhysicalSlot = t3d_vt_pack_physical_address( ( t3dVTPhysicalFormatFamily_t )nFamily, nLayer );
		return rGpuEntry.nPhysicalSlot != T3D_VT_INVALID_SLOT;
	}
	if ( nBank > 0xffffu || nLayer > 0xffffu ) {
		return false;
	}
	rGpuEntry.nPhysicalSlot = ( nBank << 16u ) | nLayer;
	return true;
}

bool Terrain3DVTGpu::update_page_entry_hot( uint32_t pVirtualPage, const t3dVTPageTableEntry_t& pEntry )
{
	const uint64_t nOffset = ( uint64_t )pVirtualPage * sizeof( t3dVTPageTableEntry_t );
	if ( nOffset + sizeof( t3dVTPageTableEntry_t ) > m_pResources->get_plan().nBufferBytes[T3D_VT_GPU_BUFFER_PAGE_TABLE] ||
	     nOffset > UINT32_MAX || m_PageEntryUpload.size() < ( int64_t )sizeof( t3dVTPageTableEntry_t ) ) {
		return false;
	}
	if ( !invalidate_ancestor_page_hot( pVirtualPage ) )
		return false;
	t3dVTPageTableEntry_t GpuEntry = {};
	if ( !make_gpu_page_entry( pVirtualPage, pEntry, GpuEntry ) ) {
		return false;
	}
	memcpy( m_PageEntryUpload.ptrw(), &GpuEntry, sizeof( GpuEntry ) );
	if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_PAGE_TABLE, ( uint32_t )nOffset, sizeof( t3dVTPageTableEntry_t ),
	                   m_PageEntryUpload ) ) {
		return false;
	}
	m_Stats.nPageTableUpdateBytes += sizeof( t3dVTPageTableEntry_t );
	++m_Stats.nPageTableUpdateCalls;
	return true;
}

bool Terrain3DVTGpu::retire_victim_gpu_hot( t3dVTResidencyState_t& rResidency, uint32_t pPhysicalSlot, uint32_t pVictimPage,
                                            uint32_t pFrameId )
{
	if ( pPhysicalSlot >= rResidency.Buffers.nPhysicalPageSetCapacity || pVictimPage >= rResidency.pDatabase->pHeader->nPageCount ) {
		return false;
	}
	const t3dVTPhysicalSlot_t& Slot = rResidency.Buffers.pPhysicalPageSets[pPhysicalSlot];
	const t3dVTPageTableEntry_t CpuEntry = rResidency.Buffers.pPageTable[pVictimPage];
	if ( Slot.m_nVirtualPage != pVictimPage || ( Slot.m_nFlags & T3D_VT_SLOT_RETIRED ) == 0u ||
	     ( CpuEntry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u || CpuEntry.nPhysicalSlot != pPhysicalSlot ) {
		return false;
	}
	t3dVTPageTableEntry_t Invalid = CpuEntry;
	Invalid.nPhysicalSlot = T3D_VT_INVALID_SLOT;
	Invalid.nFlags &= ( uint16_t )~T3D_VT_PAGE_RESIDENT;
	if ( !update_page_entry_hot( pVictimPage, Invalid ) ) {
		return false;
	}
	// Godot 4.7.2 automatically synchronizes the invalidation buffer update for later GPU consumers.
	return t3d_vt_finalize_eviction_hot( rResidency, pPhysicalSlot, pVictimPage, pFrameId );
}
bool Terrain3DVTGpu::get_physical_family_binding_v1( const uint32_t pFamily, RID& rRid, uint32_t& rLayerCount ) const
{
	rRid = RID();
	rLayerCount = 0u;
	if ( m_pResources == NULL || pFamily >= T3D_VT_PHYSICAL_FORMAT_COUNT ) {
		return false;
	}
	const uint32_t nResourceChannel = t3d_vt_physical_family_legacy_resource_channel( ( t3dVTPhysicalFormatFamily_t )pFamily );
	if ( nResourceChannel >= T3D_VT_CHANNEL_COUNT || m_pResources->get_vt_bank_count( ( t3dVTResourceChannel_t )nResourceChannel ) != 1u ) {
		return false;
	}
	rRid = m_pResources->get_vt_bank_rid( ( t3dVTResourceChannel_t )nResourceChannel, 0u );
	rLayerCount = m_pResources->get_vt_bank_layer_count( ( t3dVTResourceChannel_t )nResourceChannel, 0u );
	return rRid.is_valid() && rLayerCount > 0u;
}

bool Terrain3DVTGpu::validate_physical_format_families_v1_cold() const
{
	if ( m_pResources == NULL || m_Limits.nMaxVtPhysicalPageSets == 0u ) {
		return false;
	}
	for ( uint32_t nFamily = 0u; nFamily < T3D_VT_PHYSICAL_FORMAT_COUNT; ++nFamily ) {
		RID hBank;
		uint32_t nLayers = 0u;
		if ( !get_physical_family_binding_v1( nFamily, hBank, nLayers ) || nLayers != m_Limits.nMaxVtPhysicalPageSets ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DVTGpu::map_physical_slot_hot( uint32_t pPhysicalSlot, uint32_t& rBank, uint32_t& rLayer ) const
{
	if ( m_pResources == NULL || pPhysicalSlot >= m_Limits.nMaxVtPhysicalPageSets ||
	     m_pResources->get_vt_bank_count( T3D_VT_RESOURCE_ALBEDO ) != 1u ||
	     pPhysicalSlot >= m_pResources->get_vt_bank_layer_count( T3D_VT_RESOURCE_ALBEDO, 0u ) ) {
		return false;
	}
	rBank = 0u;
	rLayer = pPhysicalSlot;
	return true;
}

bool Terrain3DVTGpu::validate_physical_bank_layout_cold() const
{
	if ( m_pResources == NULL || m_Limits.nMaxVtPhysicalPageSets == 0u ) {
		return false;
	}
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		const t3dVTResourceChannel_t eChannel = ( t3dVTResourceChannel_t )c;
		if ( m_pResources->get_vt_bank_count( eChannel ) != 1u ||
		     m_pResources->get_vt_bank_layer_count( eChannel, 0u ) != m_Limits.nMaxVtPhysicalPageSets ) {
			return false;
		}
	}
	return validate_physical_format_families_v1_cold();
}

static void t3d_vt_set_bit_cold( uint8_t *pBytes, uint32_t& rBit, uint32_t pValue )
{
	if ( pValue != 0u ) {
		pBytes[rBit >> 3u] |= ( uint8_t )( 1u << ( rBit & 7u ) );
	}
	++rBit;
}

static void t3d_vt_write_bits_cold( uint8_t *pBytes, uint32_t& rBit, uint32_t pValue, uint32_t pCount )
{
	for ( uint32_t i = 0u; i < pCount; ++i ) {
		t3d_vt_set_bit_cold( pBytes, rBit, ( pValue >> i ) & 1u );
	}
}

bool Terrain3DVTGpu::initialize_neutral_payloads_cold()
{
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		const uint32_t nBytes = physical_layer_bytes( c );
		const uint64_t nTailBytes = m_Plan.nVtTailBytes[c];
		m_NeutralPhysicalUpload[c].resize( nBytes );
		if ( nBytes == 0u || m_NeutralPhysicalUpload[c].size() != ( int64_t )nBytes || nTailBytes == 0u || nTailBytes > INT64_MAX ) {
			return false;
		}
		m_NeutralTailUpload[c].resize( ( int64_t )nTailBytes );
		if ( m_NeutralTailUpload[c].size() != ( int64_t )nTailBytes ) {
			return false;
		}
		memset( m_NeutralPhysicalUpload[c].ptrw(), 0, nBytes );
	}

	// BC1: endpoint 0 is exact white and every texel selects endpoint 0.
	uint8_t *const pAlbedo = m_NeutralPhysicalUpload[T3D_VT_CHANNEL_ALBEDO].ptrw();
	for ( uint32_t i = 0u; i < ( uint32_t )m_NeutralPhysicalUpload[T3D_VT_CHANNEL_ALBEDO].size(); i += 8u ) {
		pAlbedo[i + 0u] = 0xffu;
		pAlbedo[i + 1u] = 0xffu;
	}

	// BC5: two constant BC4 blocks at 128 encode the neutral tangent-space normal.
	uint8_t *const pNormal = m_NeutralPhysicalUpload[T3D_VT_CHANNEL_NORMAL].ptrw();
	for ( uint32_t i = 0u; i < ( uint32_t )m_NeutralPhysicalUpload[T3D_VT_CHANNEL_NORMAL].size(); i += 16u ) {
		pNormal[i + 0u] = T3D_VT_NEUTRAL_NORMAL_X8;
		pNormal[i + 1u] = T3D_VT_NEUTRAL_NORMAL_X8;
		pNormal[i + 8u] = T3D_VT_NEUTRAL_NORMAL_Y8;
		pNormal[i + 9u] = T3D_VT_NEUTRAL_NORMAL_Y8;
	}

	// BC7 mode 6 constant RGBA=(255,255,255,255), all indices zero.
	uint8_t bc7[16] = {};
	uint32_t nBit = 0u;
	for ( uint32_t i = 0u; i < 6u; ++i ) {
		t3d_vt_set_bit_cold( bc7, nBit, 0u );
	}
	t3d_vt_set_bit_cold( bc7, nBit, 1u );
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		t3d_vt_write_bits_cold( bc7, nBit, 0x7fu, 7u );
	}
	t3d_vt_set_bit_cold( bc7, nBit, 1u );
	t3d_vt_set_bit_cold( bc7, nBit, 1u );
	uint8_t *const pSurface = m_NeutralPhysicalUpload[T3D_VT_CHANNEL_SURFACE].ptrw();
	for ( uint32_t i = 0u; i < ( uint32_t )m_NeutralPhysicalUpload[T3D_VT_CHANNEL_SURFACE].size(); i += 16u ) {
		memcpy( pSurface + i, bc7, sizeof( bc7 ) );
	}

	memset( m_NeutralPhysicalUpload[T3D_VT_CHANNEL_HEIGHT].ptrw(), T3D_VT_NEUTRAL_HEIGHT8,
	        ( size_t )m_NeutralPhysicalUpload[T3D_VT_CHANNEL_HEIGHT].size() );

	// Each physical tile already contains the channel's neutral block pattern.
	// Fill the large tail buffers once with bulk copies instead of clearing them
	// first and then visiting every compressed block again.
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		t3d_vt_repeat_neutral_payload_cold( m_NeutralTailUpload[c].ptrw(), ( size_t )m_NeutralTailUpload[c].size(),
		                                    m_NeutralPhysicalUpload[c].ptr(), ( size_t )m_NeutralPhysicalUpload[c].size() );
	}
	return true;
}

uint32_t Terrain3DVTGpu::physical_layer_bytes( uint32_t pChannel ) const
{
	if ( pChannel >= T3D_VT_CHANNEL_COUNT || m_Limits.nMaxVtPhysicalPageSets == 0u ) {
		return 0u;
	}
	const uint64_t nBytes = m_Plan.nVtPhysicalBytes[pChannel] / m_Limits.nMaxVtPhysicalPageSets;
	return nBytes != 0u && nBytes <= UINT32_MAX ? ( uint32_t )nBytes : 0u;
}

bool Terrain3DVTGpu::hot_epoch_unchanged() const
{
	return m_Stats.nGpuCreateCount == m_nHotCreateEpoch && m_Stats.nGpuFreeCount == m_nHotFreeEpoch;
}

// Select the coarse fragment producer after executable resources and the
// callback have both been published; physical residency is unchanged.
bool Terrain3DVTGpu::prepare_multi_feedback_cold( const RID& rPages, const RID& rCounts, const uint32_t nViewportWidth,
                                                  const uint32_t nViewportHeight, const uint32_t nSourceLayers, const uint32_t nPageCount )
{
	if ( !m_bInitialized || m_bHotSealed || !m_pResources || !Terrain3DRdShaderLibrary::isMultiFeedbackExperimentEnabledCold() ||
	     m_multiFeedbackGpu.ready() || !m_pResources->get_rd_context() )
		return false;
	t3dVTFeedbackGridLayout_t layout{};
	return t3d_vt_feedback_multi_layout( m_Limits.nMaxReadbackBytesPerFrame, layout ) &&
	       m_multiFeedbackGpu.initializeCold( *m_pResources->get_rd_context(), layout, rPages, rCounts, nViewportWidth, nViewportHeight,
	                                          nSourceLayers, nPageCount );
}

bool Terrain3DVTGpu::prepare_multi_feedback_replay_cold( const uint32_t nViewportWidth, const uint32_t nViewportHeight,
                                                         const uint32_t nSourceLayers, const uint32_t nPageCount )
{
	if ( !m_bInitialized || m_bHotSealed || !m_pResources || !Terrain3DRdShaderLibrary::isMultiFeedbackExperimentEnabledCold() ||
	     m_multiFeedbackGpu.ready() || !m_pResources->get_rd_context() )
		return false;
	t3dVTFeedbackGridLayout_t layout{};
	return t3d_vt_feedback_multi_layout( m_Limits.nMaxReadbackBytesPerFrame, layout ) &&
	       m_multiFeedbackGpu.initializeOwnedCold( *m_pResources->get_rd_context(), layout, nViewportWidth, nViewportHeight, nSourceLayers,
	                                               nPageCount );
}

bool Terrain3DVTGpu::publish_multi_feedback_replay_hot( const t3dVTFeedbackMultiCaptureView_t& rCapture,
                                                        const t3dVTFeedbackMultiSourceProof_t& rProof )
{
	return m_bInitialized && m_bHotSealed && m_pResources && m_pResources->get_rd_context() &&
	       m_multiFeedbackGpu.publishOwnedCaptureHot( *m_pResources->get_rd_context(), rCapture, rProof );
}

bool Terrain3DVTGpu::dispatch_multi_feedback_hot( const t3dVTFeedbackMultiSourceProof_t& rProof, const uint32_t nFrame,
                                                  const uint32_t nEpoch, const uint32_t nDatabase, const uint32_t nDevice,
                                                  const uint64_t nViewport, uint32_t& rSlot )
{
	return m_bInitialized && m_bHotSealed && m_pResources && m_pResources->get_rd_context() &&
	       m_multiFeedbackGpu.dispatchHot( *m_pResources->get_rd_context(), rProof, nFrame, nEpoch, nDatabase, nDevice, nViewport, rSlot );
}

bool Terrain3DVTGpu::submit_multi_feedback_readback_hot( const uint32_t nSlot )
{
	return m_bInitialized && m_bHotSealed && m_pResources && m_pResources->get_rd_context() && m_feedbackCallback.is_valid() &&
	       m_multiFeedbackGpu.requestReadbackHot( *m_pResources->get_rd_context(), nSlot, m_feedbackCallback );
}

const uint8_t *Terrain3DVTGpu::peek_multi_feedback_ready_hot( const uint32_t nSlot, t3dVTFeedbackGridHeader_t& rHeader ) const
{
	return m_multiFeedbackGpu.peekReadyHot( nSlot, rHeader );
}

bool Terrain3DVTGpu::retire_multi_feedback_ready_hot( const uint32_t nSlot )
{
	return m_multiFeedbackGpu.retireReadyHot( nSlot );
}

bool Terrain3DVTGpu::get_multi_feedback_params_cold( const uint32_t nSlot, t3dVTFeedbackMultiParams_t& rParams ) const
{
	if ( !m_multiFeedbackGpu.ready() || nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT )
		return false;
	rParams = m_multiFeedbackGpu.params();
	rParams.m_nSlot = nSlot;
	return true;
}

bool Terrain3DVTGpu::enable_feedback_grid_cold( const bool bEnable )
{
	if ( !m_bInitialized || m_bHotSealed || m_pResources == NULL ||
	     !m_pResources->get_buffer_rid( T3D_VT_GPU_BUFFER_FEEDBACK_GRID ).is_valid() )
		return false;
	if ( bEnable && !m_feedbackCallback.is_valid() )
		return false;
	invalidate_feedback_grid_hot();
	if ( m_bFeedbackGridEnabled != bEnable ) {
		m_bFeedbackGridEnabled = bEnable;
		++m_nFeedbackShaderGeneration;
	}
	return true;
}

void Terrain3DVTGpu::invalidate_feedback_grid_hot()
{
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i )
		m_feedbackGridSlots[i].invalidate();
	m_nFeedbackGridActiveFrame = UINT32_MAX;
	m_nFeedbackGridActiveViewport = 0u;
}

bool Terrain3DVTGpu::get_feedback_grid_layout_cold( t3dVTFeedbackGridLayout_t& rLayout ) const
{
	rLayout = m_feedbackGridLayout;
	return m_bCpuPrepared && rLayout.m_nSlotBytes != 0u;
}

bool Terrain3DVTGpu::begin_feedback_grid_capture_hot( const uint32_t nFrame, const uint32_t nDatabase, const uint32_t nDevice,
                                                      const uint64_t nViewport, const uint32_t nPageCount, const uint32_t nAllocationCount,
                                                      const uint32_t nViewportWidth, const uint32_t nViewportHeight, uint32_t& rSlot )
{
	rSlot = UINT32_MAX;
	if ( !m_bFeedbackGridEnabled || !m_bInitialized || !m_bHotSealed || !hot_epoch_unchanged() || m_pResources == NULL ||
	     !m_feedbackCallback.is_valid() || nViewport == 0u || nPageCount == 0u || m_feedbackGridLayout.m_nSlotBytes == 0u ||
	     m_feedbackGridReset.size() != m_feedbackGridLayout.m_nSlotBytes )
		return false;
	if ( m_pResources->get_rd_context() == NULL || m_pResources->get_rd_context()->getDeviceGeneration() != nDevice )
		return false;
	uint32_t nSlot = UINT32_MAX;
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i ) {
		const uint32_t candidate = ( m_nFeedbackGridNextSlot + i ) % T3D_VT_FEEDBACK_GRID_SLOT_COUNT;
		if ( m_feedbackGridSlots[candidate].m_eStatus == T3D_VT_FEEDBACK_SLOT_FREE ) {
			nSlot = candidate;
			break;
		}
	}
	if ( nSlot == UINT32_MAX ) {
		++m_nFeedbackGridSkipped;
		return false;
	}
	const uint32_t nWords = m_feedbackGridLayout.m_nWidth * m_feedbackGridLayout.m_nHeight;
	const t3dVTFeedbackGridHeader_t header{ T3D_VT_FEEDBACK_GRID_MAGIC,
	                                        T3D_VT_FEEDBACK_GRID_CONTRACT_VERSION,
	                                        nSlot,
	                                        nFrame,
	                                        m_nFeedbackEpoch,
	                                        nDatabase,
	                                        nDevice,
	                                        nWords,
	                                        nViewport };
	// The reset buffer is cold-allocated. No per-frame heap or resource creation.
	memset( m_feedbackGridReset.ptrw(), 0xff, ( size_t )m_feedbackGridLayout.m_nSlotBytes );
	memcpy( m_feedbackGridReset.ptrw(), &header, sizeof( header ) );
	t3dVTFeedbackGridParams_t params{};
	if ( !t3d_vt_feedback_grid_make_params( m_feedbackGridLayout, nAllocationCount, nPageCount, nViewportWidth, nViewportHeight, nSlot,
	                                        m_nPredictedTerrainMaterials, params ) )
		return false;
	t3dGpuTransferTicket_t ticket;
	if ( !Terrain3DGpuTransferBudget::get().reserve( T3D_GPU_TRANSFER_FEEDBACK,
	                                                 ( uint64_t )m_feedbackGridLayout.m_nSlotBytes + sizeof( params ), 2u, ticket ) ) {
		++m_nFeedbackGridSkipped;
		return false;
	}
	if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_FEEDBACK_GRID, nSlot * m_feedbackGridLayout.m_nSlotBytes,
	                   m_feedbackGridLayout.m_nSlotBytes, m_feedbackGridReset ) ) {
		Terrain3DGpuTransferBudget::get().refund( ticket, m_feedbackGridLayout.m_nSlotBytes + sizeof( params ), 2u );
		++m_nFeedbackGridSkipped;
		return false;
	}
	memcpy( m_feedbackParams.ptrw(), &params, sizeof( params ) );
	if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS, 0u, sizeof( params ), m_feedbackParams ) ) {
		Terrain3DGpuTransferBudget::get().refund( ticket, sizeof( params ), 1u );
		++m_nFeedbackGridSkipped;
		return false;
	}
	if ( !m_feedbackGridSlots[nSlot].prepare( header ) )
		return false;
	m_nFeedbackGridNextSlot = ( nSlot + 1u ) % T3D_VT_FEEDBACK_GRID_SLOT_COUNT;
	rSlot = nSlot;
	return true;
}

bool Terrain3DVTGpu::service_feedback_grid_capture_hot( const uint32_t nFrame, const uint32_t nDatabase, const uint32_t nDevice,
                                                        const uint64_t nViewport, const uint32_t nPageCount,
                                                        const uint32_t nAllocationCount, const uint32_t nViewportWidth,
                                                        const uint32_t nViewportHeight )
{
	if ( !m_bFeedbackGridEnabled )
		return false;
	// Ready slots remain immutable until Terrain3DVirtualTexture has reduced
	// and staged their demand. Only the VT admission owner retires them.
	if ( m_nFeedbackGridActiveFrame == nFrame && m_nFeedbackGridActiveViewport == nViewport )
		return true;
	// This callback is invoked during PREPARE, after the previous frame's
	// SUBMIT commands were recorded and before the next capture is cleared.
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i ) {
		t3dVTFeedbackGridSlot_t& slot = m_feedbackGridSlots[i];
		if ( slot.m_eStatus == T3D_VT_FEEDBACK_SLOT_PREPARED &&
		     ( slot.m_Header.m_nFrame != nFrame || slot.m_Header.m_nViewportKey != nViewport ) )
			( void )slot.markDrawn();
		if ( slot.m_eStatus == T3D_VT_FEEDBACK_SLOT_DRAWN &&
		     ( slot.m_Header.m_nFrame != nFrame || slot.m_Header.m_nViewportKey != nViewport ) )
			( void )submit_feedback_grid_readback_hot( i );
	}
	uint32_t slot = UINT32_MAX;
	if ( !begin_feedback_grid_capture_hot( nFrame, nDatabase, nDevice, nViewport, nPageCount, nAllocationCount, nViewportWidth,
	                                       nViewportHeight, slot ) ) {
		m_nFeedbackGridActiveFrame = UINT32_MAX;
		m_nFeedbackGridActiveViewport = 0u;
		// Disable writes when the slot ring is full. The original 32-byte
		// staging buffer is preallocated; this is a tracked update, not a new
		// descriptor or resource. Prediction remains available.
		if ( m_feedbackParams.size() == sizeof( t3dVTFeedbackGridParams_t ) ) {
			t3dVTFeedbackGridParams_t disabled{};
			memcpy( m_feedbackParams.ptrw(), &disabled, sizeof( disabled ) );
			( void )t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS, 0u, sizeof( disabled ), m_feedbackParams );
		}
		return false;
	}
	m_nFeedbackGridActiveFrame = nFrame;
	m_nFeedbackGridActiveViewport = nViewport;
	return true;
}

bool Terrain3DVTGpu::mark_feedback_grid_drawn_hot( const uint32_t nSlot )
{
	return nSlot < T3D_VT_FEEDBACK_GRID_SLOT_COUNT && m_feedbackGridSlots[nSlot].markDrawn();
}

bool Terrain3DVTGpu::submit_feedback_grid_readback_hot( const uint32_t nSlot )
{
	if ( nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT || !m_bFeedbackGridEnabled || m_pResources == NULL || m_pRenderingDevice == NULL ||
	     !m_feedbackCallback.is_valid() || m_feedbackGridSlots[nSlot].m_eStatus != T3D_VT_FEEDBACK_SLOT_DRAWN )
		return false;
	t3dGpuTransferTicket_t ticket;
	if ( !Terrain3DGpuTransferBudget::get().reserve( T3D_GPU_TRANSFER_FEEDBACK, m_feedbackGridLayout.m_nSlotBytes, 1u, ticket ) ) {
		++m_nFeedbackGridSkipped;
		m_feedbackGridSlots[nSlot].submit( false );
		return false;
	}
	const bool accepted =
	    m_pRenderingDevice->buffer_get_data_async( m_pResources->get_buffer_rid( T3D_VT_GPU_BUFFER_FEEDBACK_GRID ), m_feedbackCallback,
		                                           nSlot * m_feedbackGridLayout.m_nSlotBytes, m_feedbackGridLayout.m_nSlotBytes ) == OK;
	m_feedbackGridSlots[nSlot].submit( accepted );
	if ( !accepted ) {
		Terrain3DGpuTransferBudget::get().refund( ticket, m_feedbackGridLayout.m_nSlotBytes, 1u );
		++m_nFeedbackGridSkipped;
	}
	return accepted;
}

bool Terrain3DVTGpu::consume_feedback_grid_callback_hot( const PackedByteArray& rBytes )
{
	if ( rBytes.size() < ( int64_t )sizeof( t3dVTFeedbackGridHeader_t ) )
		return false;
	t3dVTFeedbackGridHeader_t header{};
	memcpy( &header, rBytes.ptr(), sizeof( header ) );
	if ( header.m_nMagic != T3D_VT_FEEDBACK_GRID_MAGIC )
		return false;
	if ( header.m_nVersion != T3D_VT_FEEDBACK_GRID_CONTRACT_VERSION || rBytes.size() != m_feedbackGridLayout.m_nSlotBytes )
		return true;
	if ( header.m_nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT ||
	     header.m_nWordCount != m_feedbackGridLayout.m_nWidth * m_feedbackGridLayout.m_nHeight )
		return true;
	t3dVTFeedbackGridSlot_t& slot = m_feedbackGridSlots[header.m_nSlot];
	if ( !slot.complete( header ) )
		return true;
	memcpy( m_feedbackGridReadbacks[header.m_nSlot].ptrw(), rBytes.ptr(), ( size_t )rBytes.size() );
	return true;
}

const uint8_t *Terrain3DVTGpu::peek_feedback_grid_ready_hot( const uint32_t nSlot, t3dVTFeedbackGridHeader_t& rHeader ) const
{
	if ( nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT || m_feedbackGridSlots[nSlot].m_eStatus != T3D_VT_FEEDBACK_SLOT_READY )
		return NULL;
	rHeader = m_feedbackGridSlots[nSlot].m_Header;
	return m_feedbackGridReadbacks[nSlot].ptr() + sizeof( t3dVTFeedbackGridHeader_t );
}

void Terrain3DVTGpu::record_feedback_grid_consumed_hot( const uint32_t nValidSamples )
{
	++m_nFeedbackGridCaptures;
	m_nFeedbackGridSamples += nValidSamples;
}

bool Terrain3DVTGpu::retire_feedback_grid_ready_hot( const uint32_t nSlot )
{
	if ( nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT || m_feedbackGridSlots[nSlot].m_eStatus != T3D_VT_FEEDBACK_SLOT_READY )
		return false;
	m_feedbackGridSlots[nSlot].reset();
	return true;
}

void Terrain3DVTGpu::consume_fragment_feedback_readback_hot( const PackedByteArray& rBytes )
{
	// Grid callbacks use an in-band 40-byte tag; do not confuse them with
	// legacy hash/counter callbacks sharing this callable.
	if ( m_multiFeedbackGpu.consumeReadbackHot( rBytes ) || consume_feedback_grid_callback_hot( rBytes ) )
		return;

	if ( m_bInitialized && rBytes.size() == sizeof( t3dVTRequestGpuCounters_t ) ) {
		t3dVTRequestGpuCounters_t counters{};
		memcpy( &counters, rBytes.ptr(), sizeof( counters ) );
		// RequestCapacity is an owner epoch tag for this hash-only producer.
		if ( counters.m_nRequestCapacity != m_nFeedbackEpoch ) {
			return;
		}
		if ( !m_feedbackEpoch.completeCounters( counters.m_nRequestCapacity ) ) {
			return;
		}
		if ( counters.m_nGeneration == m_nFeedbackDatabase ) {
			m_nFeedbackMisses += counters.m_nFragmentMissCount;
			m_nFeedbackOverflow += counters.m_nProbeOverflowCount;
		}
		return;
	}
	if ( !m_bInitialized || rBytes.size() != m_feedbackReset.size() || rBytes.size() < ( int64_t )sizeof( t3dVTRequestHashEntry_t ) ) {
		return;
	}
	const t3dVTRequestHashEntry_t tag = t3dVtReadFeedback( rBytes, 0u );
	if ( tag.m_nVirtualPage != UINT32_MAX - 1u || tag.m_nPriority != m_nFeedbackEpoch ) {
		return;
	}
	if ( !m_feedbackEpoch.completeHash( tag.m_nPriority ) ) {
		return;
	}
	if ( tag.m_nWantedMip != m_nFeedbackDatabase ) {
		++m_nFeedbackDropped;
		return;
	}
	m_feedbackReadback = rBytes; // Borrow the bounded engine-owned asynchronous result.
}

RID Terrain3DVTGpu::get_fragment_feedback_set_cold( const RID& rShader, const uint32_t nVariant )
{
	if ( !is_fragment_feedback_shader_ready() || !rShader.is_valid() || nVariant >= 8u * T3D_RD_FORWARD_COLOR_VARIANT_COUNT ) {
		return RID();
	}
	Terrain3DRdContext *const pContext = m_pResources->get_rd_context();
	Terrain3DRdGlobalDescriptorCache *const pCache = pContext ? pContext->tryGetGlobalDescriptorCacheHot() : nullptr;
	if ( !pCache ) {
		return RID();
	}
	RID resolved;
	if ( m_feedbackSetShaders[nVariant] == rShader && m_feedbackSetGridMode[nVariant] == m_bFeedbackGridEnabled &&
	     pCache->resolveDescriptorHot( m_feedbackSets[nVariant], pCache->currentSubmissionEpochHot(), resolved ) == T3D_RD_DESCRIPTOR_OK ) {
		m_feedbackResolvedSets[nVariant] = resolved;
		return resolved;
	}
	pCache->releaseDescriptorCold( m_feedbackSets[nVariant] );
	m_feedbackSetShaders[nVariant] = RID();
	m_feedbackResolvedSets[nVariant] = RID();
	const t3dRdDescriptorDeviceKey_t device{ T3D_RD_DESCRIPTOR_GLOBAL_DEVICE_ID, pContext->getDeviceGeneration(),
	                                         T3D_RD_DESCRIPTOR_DEVICE_GLOBAL };
	t3dRdDescriptorRecipe_t recipe{};
	if ( t3dRdDescriptorRecipeBeginCold( recipe, device, rShader, 3u ) != T3D_RD_DESCRIPTOR_OK ) {
		return RID();
	}
	const t3dVTGpuBufferId_t buffers[5] = { T3D_VT_GPU_BUFFER_ALLOCATION_TABLE, T3D_VT_GPU_BUFFER_PAGE_TABLE,
	                                        T3D_VT_GPU_BUFFER_FEEDBACK_GRID, T3D_VT_GPU_BUFFER_FRAGMENT_COUNTERS,
	                                        T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS };
	for ( uint32_t binding = 0u; binding < 5u; ++binding ) {
		const RID hBuffer = m_pResources->get_buffer_rid( buffers[binding] );
		if ( t3dRdDescriptorRecipeAddBindingCold( recipe, binding,
		                                          binding == 4u ? RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER
		                                                        : RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER,
		                                          &hBuffer, 1u ) != T3D_RD_DESCRIPTOR_OK ) {
			return RID();
		}
	}
	if ( pCache->acquireDescriptorCold( recipe, T3D_RD_CONTEXT_OWNER_VT, m_feedbackSets[nVariant] ) != T3D_RD_DESCRIPTOR_OK ) {
		return RID();
	}
	if ( pCache->resolveDescriptorHot( m_feedbackSets[nVariant], pCache->currentSubmissionEpochHot(), resolved ) != T3D_RD_DESCRIPTOR_OK ) {
		pCache->releaseDescriptorCold( m_feedbackSets[nVariant] );
		return RID();
	}
	m_feedbackSetShaders[nVariant] = rShader;
	m_feedbackSetGridMode[nVariant] = m_bFeedbackGridEnabled;
	m_feedbackResolvedSets[nVariant] = resolved;
	return resolved;
}

RID Terrain3DVTGpu::resolve_fragment_feedback_set_hot( const RID& rPrevious )
{
	if ( !rPrevious.is_valid() || !is_fragment_feedback_shader_ready() ) {
		return RID();
	}
	Terrain3DRdGlobalDescriptorCache *const pCache = m_pResources->get_rd_context()->tryGetGlobalDescriptorCacheHot();
	if ( !pCache ) {
		return RID();
	}
	for ( uint32_t i = 0u; i < 8u * T3D_RD_FORWARD_COLOR_VARIANT_COUNT; ++i ) {
		if ( m_feedbackResolvedSets[i] != rPrevious ) {
			continue;
		}
		RID result;
		return pCache->resolveDescriptorHot( m_feedbackSets[i], pCache->currentSubmissionEpochHot(), result ) == T3D_RD_DESCRIPTOR_OK
		           ? result
				   : RID();
	}
	return RID();
}

bool Terrain3DVTGpu::publish_predicted_terrain_mask_hot()
{
	if ( m_bFeedbackGridEnabled )
		return true; // The grid UBO is published per capture.
	if ( !m_bFeedbackWritten || !is_readback_enabled() || m_feedbackParams.size() != ( int64_t )sizeof( t3dVTFragmentFeedbackParams_t ) ) {
		return false;
	}
	t3dVTFragmentFeedbackParams_t params{};
	memcpy( &params, m_feedbackParams.ptr(), sizeof( params ) );
	if ( params.m_nPredictedTerrainMaterials == m_nPredictedTerrainMaterials ) {
		++m_nPredictedMaskUploadsSaved;
		return true; // The GPU already has this exact mask from the last successful update.
	}
	t3dGpuTransferTicket_t ticket;
	if ( !Terrain3DGpuTransferBudget::get().reserve( T3D_GPU_TRANSFER_FEEDBACK, sizeof( t3dVTFragmentFeedbackParams_t ), 1u, ticket ) ) {
		return false;
	}
	const t3dVTFragmentFeedbackParams_t previous = params;
	params.m_nPredictedTerrainMaterials = m_nPredictedTerrainMaterials;
	memcpy( m_feedbackParams.ptrw(), &params, sizeof( params ) );
	if ( t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS, 0u, sizeof( params ), m_feedbackParams ) ) {
		return true;
	}
	// Keep the CPU shadow consistent with the last successful GPU publication,
	// so a failed update is retried instead of being treated as unchanged.
	memcpy( m_feedbackParams.ptrw(), &previous, sizeof( previous ) );
	return false;
}

uint32_t Terrain3DVTGpu::service_fragment_feedback_hot( t3dVTResidencyState_t& rResidency, t3dVTStreamingState_t& rStreaming,
                                                        const uint32_t nFrameId, const uint32_t nRequestBudget )
{
	if ( m_bFeedbackGridEnabled )
		return 0u;
	if ( !is_readback_enabled() || m_nFeedbackFrame == nFrameId || m_pResources == NULL ) {
		return 0u;
	}
	m_nFeedbackFrame = nFrameId;
	m_nFeedbackDatabase = rResidency.nDatabaseGeneration;
	uint32_t attempts = 0u;
	// Do not ingest one member of a readback pair while its partner is pending.
	// Rejected pairs retain their GPU contents for the next full submission.
	if ( !m_feedbackEpoch.canSubmit() || ( m_feedbackEpoch.m_bPairAccepted && !m_feedbackEpoch.canConsume() ) ) {
		( void )publish_predicted_terrain_mask_hot();
		return attempts;
	}
	if ( !m_feedbackReadback.is_empty() ) {
		if ( t3dVtReadFeedback( m_feedbackReadback, 0u ).m_nWantedMip == rResidency.nDatabaseGeneration ) {
			for ( uint32_t i = 1u; i < m_feedbackReadback.size() / ( int64_t )sizeof( t3dVTRequestHashEntry_t ); ++i ) {
				const t3dVTRequestHashEntry_t entry = t3dVtReadFeedback( m_feedbackReadback, i );
				if ( entry.m_nVirtualPage == UINT32_MAX || entry.m_nVirtualPage >= rResidency.pDatabase->pHeader->nPageCount ) {
					continue;
				}
				if ( t3d_vt_touch_page_hot( rResidency, entry.m_nVirtualPage, nFrameId ) ) {
					continue;
				}
				if ( attempts >= nRequestBudget ) {
					++m_nFeedbackDropped;
					continue;
				}
				++attempts;
				t3dVTRuntimeRequest_t request{};
				request.nVirtualPage = entry.m_nVirtualPage;
				request.nWantedMip = ( uint16_t )entry.m_nWantedMip;
				request.nPriority = ( uint16_t )entry.m_nPriority;
				const t3dVTAdmissionStatus_t status = t3d_vt_streaming_admit_request_hot( rStreaming, request, nFrameId );
				if ( status == T3D_VT_ADMISSION_ACCEPTED_NEW || status == T3D_VT_ADMISSION_MERGED_EXISTING ) {
					++m_nFeedbackConsumed;
				} else {
					++m_nFeedbackDropped;
				}
			}
		}
		m_feedbackReadback.clear();
	}
	// Do not reset hash or counter storage while asynchronous readers still own it.
	// The producer can keep merging requests; the next eligible call collects them.

	// Feedback hash reset, counters, params and async readbacks share the
	// same transfer budget as physical tiles and region/world publications.
	// Failure leaves the existing hash intact for a later readback/reset.
	t3dGpuTransferTicket_t feedback_ticket;
	const uint64_t feedback_bytes = ( uint64_t )m_feedbackReset.size() * ( m_bFeedbackWritten ? 2u : 1u ) +
	                                ( uint64_t )m_feedbackCounterReset.size() * ( m_bFeedbackWritten ? 2u : 1u ) +
	                                ( uint64_t )m_feedbackParams.size();
	const uint32_t feedback_calls = m_bFeedbackWritten ? 5u : 3u;
	if ( !Terrain3DGpuTransferBudget::get().reserve( T3D_GPU_TRANSFER_FEEDBACK, feedback_bytes, feedback_calls, feedback_ticket ) ) {
		( void )publish_predicted_terrain_mask_hot();
		return attempts;
	}
	// Capture the previous raster's writes asynchronously. RenderingDevice's
	// transfer dependencies order this copy before subsequent reset and draws.
	if ( m_bFeedbackWritten && m_feedbackEpoch.canSubmit() ) {
		T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_FEEDBACK_READBACK_SUBMIT );
		const bool hashAccepted =
		    m_pRenderingDevice->buffer_get_data_async( m_pResources->get_buffer_rid( T3D_VT_GPU_BUFFER_FRAGMENT_HASH ), m_feedbackCallback,
			                                           0u, ( uint32_t )m_feedbackReset.size() ) == OK;
		const bool countersAccepted =
		    m_pRenderingDevice->buffer_get_data_async( m_pResources->get_buffer_rid( T3D_VT_GPU_BUFFER_FRAGMENT_COUNTERS ),
			                                           m_feedbackCallback, 0u, ( uint32_t )sizeof( t3dVTRequestGpuCounters_t ) ) == OK;
		if ( !m_feedbackEpoch.acceptSubmissions( hashAccepted, countersAccepted ) ) {
			// Even if one request succeeded, wait for its callback and retry
			// both buffers; no GPU feedback reset is permitted on this path.
			const uint64_t submittedBytes = ( hashAccepted ? ( uint64_t )m_feedbackReset.size() : 0u ) +
			                                ( countersAccepted ? ( uint64_t )sizeof( t3dVTRequestGpuCounters_t ) : 0u );
			const uint32_t submittedCalls = ( hashAccepted ? 1u : 0u ) + ( countersAccepted ? 1u : 0u );
			Terrain3DGpuTransferBudget::get().refund( feedback_ticket, feedback_bytes - submittedBytes, feedback_calls - submittedCalls );
			return attempts;
		}
	}
	t3dVTRequestGpuCounters_t counters{};
	counters.m_nFrameId = nFrameId;
	counters.m_nGeneration = rResidency.nDatabaseGeneration;
	counters.m_nHashCapacity = ( uint32_t )( m_feedbackReset.size() / sizeof( t3dVTRequestHashEntry_t ) );
	counters.m_nRequestCapacity = m_nFeedbackEpoch;
	memcpy( m_feedbackCounterReset.ptrw(), &counters, sizeof( counters ) );
	const t3dVTRequestHashEntry_t reset = { UINT32_MAX - 1u, rResidency.nDatabaseGeneration, m_nFeedbackEpoch, nFrameId };
	memcpy( m_feedbackReset.ptrw(), &reset, sizeof( reset ) );
	t3dVTFragmentFeedbackParams_t params{ ( uint32_t )( m_feedbackReset.size() / sizeof( reset ) ),
	                                      rResidency.nAllocationCount,
	                                      rResidency.pDatabase->pHeader->nPageCount,
	                                      T3D_VT_FRAGMENT_FEEDBACK_ENABLED,
	                                      m_nPredictedTerrainMaterials,
	                                      0u,
	                                      0u,
	                                      0u };
	memcpy( m_feedbackParams.ptrw(), &params, sizeof( params ) );
	bool reset_ok = false;
	{
		T3D_STREAM_PROFILE_SCOPE( T3D_SP_VT_FEEDBACK_RESET_SUBMIT );
		reset_ok =
		    t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_FRAGMENT_HASH, 0u, ( uint32_t )m_feedbackReset.size(), m_feedbackReset ) &&
		    t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_FRAGMENT_COUNTERS, 0u, ( uint32_t )m_feedbackCounterReset.size(),
			             m_feedbackCounterReset ) &&
		    t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS, 0u, ( uint32_t )m_feedbackParams.size(), m_feedbackParams );
	}
	m_bFeedbackWritten = reset_ok;
	return attempts;
}

// BR07-BR09: optional derived page hints. The header is invalidated before
// authoritative page-table writes; it is re-enabled only after bounded uploads
// have drained every changed subtree. The default build has no extra buffers.
void Terrain3DVTGpu::shutdown_ancestor_hints_cold()
{
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	m_bHintGraphReady = false;
	m_bHintHeaderEnabled = false;
	m_bHintHeaderInitialized = false;
	m_nHintPageCount = 0u;
	m_nHintQueueHead = m_nHintQueueTail = m_nHintQueueCount = 0u;
	m_nPublishedHintGeneration = 0u;
	m_HintFirstChild.clear();
	m_HintNextSibling.clear();
	m_HintAllocationIndex.clear();
	m_HintQueue.clear();
	m_HintWorkIndices.clear();
	m_HintQueued.clear();
	m_HintCpuRecords.clear();
	m_HintUploadScratch.clear();
#endif
}

bool Terrain3DVTGpu::invalidate_ancestor_header_hot()
{
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	if ( !m_pResources || !m_pResources->get_buffer_rid( T3D_VT_GPU_BUFFER_ANCESTOR_HINTS ).is_valid() ||
	     m_HintUploadScratch.size() < ( int64_t )sizeof( t3dVTAncestorHint_t ) )
		return false;
	if ( !m_bHintHeaderInitialized || m_bHintHeaderEnabled ) {
		const t3dVTAncestorHint_t invalid = t3d_vt_invalid_ancestor_hint();
		memcpy( m_HintUploadScratch.ptrw(), &invalid, sizeof( invalid ) );
		if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_ANCESTOR_HINTS, 0u, sizeof( invalid ), m_HintUploadScratch ) )
			return false;
	}
	m_bHintHeaderEnabled = false;
	m_bHintHeaderInitialized = true;
#endif
	return true;
}

bool Terrain3DVTGpu::enqueue_ancestor_root_hot( const uint32_t nPage )
{
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	if ( !m_bHintGraphReady || nPage >= m_nHintPageCount )
		return true;
	uint8_t *const queued = m_HintQueued.ptrw();
	if ( queued[nPage] != 0u )
		return true;
	if ( m_nHintQueueCount >= m_nHintPageCount )
		return false;
	m_HintQueue.ptrw()[m_nHintQueueTail] = ( int32_t )nPage;
	m_nHintQueueTail = ( m_nHintQueueTail + 1u ) % m_nHintPageCount;
	++m_nHintQueueCount;
	queued[nPage] = 1u;
#else
	( void )nPage;
#endif
	return true;
}

bool Terrain3DVTGpu::invalidate_ancestor_page_hot( const uint32_t nPage )
{
	return invalidate_ancestor_header_hot() && enqueue_ancestor_root_hot( nPage );
}

bool Terrain3DVTGpu::prepare_ancestor_graph_warm( const t3dVTResidencyState_t& rResidency )
{
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	m_bHintGraphReady = false;
	m_nHintQueueHead = m_nHintQueueTail = m_nHintQueueCount = 0u;
	if ( !rResidency.pDatabase || !rResidency.pDatabase->pHeader || !rResidency.Buffers.pAllocations || !rResidency.Buffers.pPageTable ||
	     !rResidency.pDatabase->pPages || rResidency.pDatabase->nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK )
		return true;
	const uint32_t count = rResidency.pDatabase->pHeader->nPageCount;
	if ( count == 0u || count > ( uint32_t )m_HintFirstChild.size() || count > ( uint32_t )m_HintAllocationIndex.size() )
		return true;
	m_nHintPageCount = count;
	int32_t *const first = m_HintFirstChild.ptrw();
	int32_t *const next = m_HintNextSibling.ptrw();
	int32_t *const owners = m_HintAllocationIndex.ptrw();
	for ( uint32_t i = 0u; i < count; ++i ) {
		first[i] = next[i] = owners[i] = -1;
	}
	for ( uint32_t a = 0u; a < rResidency.nAllocationCount; ++a ) {
		const t3dVTAllocation_t& alloc = rResidency.Buffers.pAllocations[a];
		if ( alloc.m_nPageCount == 0u )
			continue;
		if ( alloc.m_nBaseVirtualPage >= count || alloc.m_nPageCount > count - alloc.m_nBaseVirtualPage )
			return true;
		for ( uint32_t i = 0u; i < alloc.m_nPageCount; ++i ) {
			const uint32_t page = alloc.m_nBaseVirtualPage + i;
			if ( owners[page] != -1 )
				return true;
			owners[page] = ( int32_t )a;
		}
	}
	for ( uint32_t i = 0u; i < count; ++i ) {
		const uint32_t parent = rResidency.Buffers.pPageTable[i].nParentPage;
		if ( parent == T3D_VT_INVALID_PAGE || owners[i] < 0 )
			continue;
		if ( parent >= count || parent == i || owners[parent] != owners[i] )
			return true;
		// Strictly increasing authored mip forbids cycles and cross-mip aliasing.
		if ( rResidency.pDatabase->pPages[parent].nMip <= rResidency.pDatabase->pPages[i].nMip )
			return true;
		next[i] = first[parent];
		first[parent] = ( int32_t )i;
	}
	m_bHintGraphReady = true;
	memset( m_HintQueued.ptrw(), 0, ( size_t )count );
	for ( uint32_t i = 0u; i < count; ++i )
		if ( !enqueue_ancestor_root_hot( i ) )
			return false;
#endif
	return true;
}

bool Terrain3DVTGpu::service_ancestor_hints_hot( const t3dVTResidencyState_t& rResidency, uint32_t nBudget )
{
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	if ( !m_bHintGraphReady || !m_bDatabaseUploadedWarm || !m_pResources || !rResidency.bInitialized || !rResidency.pDatabase ||
	     rResidency.pDatabase != m_pDatabaseView || nBudget == 0u )
		return true;
	if ( m_bHintHeaderEnabled && m_nPublishedHintGeneration != rResidency.nPageTableGeneration ) {
		if ( !invalidate_ancestor_header_hot() )
			return false;
		for ( uint32_t i = 0u; i < m_nHintPageCount; ++i )
			if ( !enqueue_ancestor_root_hot( i ) )
				return false;
	}
	if ( m_nHintQueueCount == 0u ) {
		if ( m_bHintHeaderEnabled )
			return true;
		t3dVTAncestorHint_t header{};
		header.nResolvedPage = T3D_VT_HINT_ENABLED;
		header.nPhysicalSlot = rResidency.nPageTableGeneration;
		header.nAllocationGeneration = m_nHintPageCount;
		header.nPackedMipDepthFlags = rResidency.nDatabaseGeneration;
		memcpy( m_HintUploadScratch.ptrw(), &header, sizeof( header ) );
		if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_ANCESTOR_HINTS, 0u, sizeof( header ), m_HintUploadScratch ) )
			return false;
		m_nPublishedHintGeneration = rResidency.nPageTableGeneration;
		m_bHintHeaderEnabled = true;
		return true;
	}
	const uint32_t limit = MIN( nBudget, T3D_VT_HINT_MAX_REBUILD_PER_FRAME );
	int32_t *const work = m_HintWorkIndices.ptrw();
	uint32_t completed = 0u;
	while ( completed < limit && m_nHintQueueCount != 0u ) {
		const uint32_t page = ( uint32_t )m_HintQueue.ptr()[m_nHintQueueHead];
		m_nHintQueueHead = ( m_nHintQueueHead + 1u ) % m_nHintPageCount;
		--m_nHintQueueCount;
		m_HintQueued.ptrw()[page] = 0u;
		for ( int32_t child = m_HintFirstChild.ptr()[page]; child >= 0; child = m_HintNextSibling.ptr()[child] )
			if ( !enqueue_ancestor_root_hot( ( uint32_t )child ) )
				return false;
		t3dVTAncestorHint_t hint = t3d_vt_invalid_ancestor_hint();
		const int32_t allocationId = m_HintAllocationIndex.ptr()[page];
		if ( allocationId >= 0 && ( uint32_t )allocationId < rResidency.nAllocationCount &&
		     rResidency.pDatabase->pPages[page].nMip < T3D_VT_MAX_MIPS &&
		     t3d_vt_build_ancestor_hint_reference( rResidency.Buffers.pAllocations[allocationId], rResidency.Buffers.pPageTable,
		                                           m_nHintPageCount, page, rResidency.pDatabase->pPages[page].nMip, hint ) ) {
			t3dVTPageTableEntry_t gpuEntry{};
			if ( !make_gpu_page_entry( hint.nResolvedPage, rResidency.Buffers.pPageTable[hint.nResolvedPage], gpuEntry ) )
				return false;
			hint.nPhysicalSlot = gpuEntry.nPhysicalSlot;
		}
		memcpy( m_HintCpuRecords.ptrw() + ( size_t )( page + 1u ) * sizeof( hint ), &hint, sizeof( hint ) );
		work[completed++] = ( int32_t )page;
	}
	std::sort( work, work + completed );
	uint32_t first = 0u;
	uint32_t uploadRuns = 0u;
	while ( first < completed ) {
		if ( uploadRuns == T3D_VT_HINT_MAX_UPLOAD_RUNS_PER_FRAME ) {
			// Do not turn scattered page invalidations into hundreds of hot RD uploads.
			// Unpublished records remain inaccessible while the header is invalid.
			for ( uint32_t i = first; i < completed; ++i )
				if ( !enqueue_ancestor_root_hot( ( uint32_t )work[i] ) )
					return false;
			break;
		}
		uint32_t last = first + 1u;
		while ( last < completed && ( uint32_t )work[last] == ( uint32_t )work[last - 1u] + 1u )
			++last;
		const uint32_t byteCount = ( last - first ) * sizeof( t3dVTAncestorHint_t );
		const uint32_t offset = ( ( uint32_t )work[first] + 1u ) * sizeof( t3dVTAncestorHint_t );
		memcpy( m_HintUploadScratch.ptrw(), m_HintCpuRecords.ptr() + offset, byteCount );
		if ( !t3dVtUpload( m_pResources, T3D_VT_GPU_BUFFER_ANCESTOR_HINTS, offset, byteCount, m_HintUploadScratch ) ) {
			for ( uint32_t i = 0u; i < completed; ++i )
				( void )enqueue_ancestor_root_hot( ( uint32_t )work[i] );
			return false;
		}
		first = last;
		++uploadRuns;
	}
	if ( m_nHintQueueCount == 0u )
		return service_ancestor_hints_hot( rResidency, nBudget );
#else
	( void )rResidency;
	( void )nBudget;
#endif
	return true;
}
