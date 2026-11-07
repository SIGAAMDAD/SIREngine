#include "rendering/rd/vt/runtime/terrain_3d_shared_vt_runtime.h"
#include "rendering/rd/core/terrain_3d_rd_context.h"
#include "rendering/rd/infrastructure/descriptors/terrain_3d_rd_global_descriptor_cache.h"
#include "core/utility/terrain_3d_generation.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <string.h>
using namespace godot;

Terrain3DSharedVTRuntime::Terrain3DSharedVTRuntime() = default;
Terrain3DSharedVTRuntime::~Terrain3DSharedVTRuntime()
{
	shutdown_cold();
}

bool Terrain3DSharedVTRuntime::findComposedRegionAllocationCold( const uint32_t nRegionMapIndex, uint32_t& rAllocation ) const
{
	rAllocation = T3D_VT_INVALID_ALLOCATION;
	return nRegionMapIndex < T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY &&
	       m_VirtualTexture.findMaterialAllocationCold( t3d_vt_composed_region_stable_key( nRegionMapIndex ), rAllocation );
}

bool Terrain3DSharedVTRuntime::bindRuntimeAllocator( Terrain3DMemoryZone *const pZone )
{
	if ( pZone == NULL || ( m_pRuntimeAllocatorZone != NULL && m_pRuntimeAllocatorZone != pZone ) ||
	     !m_VirtualTexture.bindRuntimeAllocator( pZone ) ) {
		return false;
	}
	m_pRuntimeAllocatorZone = pZone;
	return true;
}
bool Terrain3DSharedVTRuntime::refresh_shared_vt_bindings_hot()
{
	if ( !m_Resources.is_initialized() || !m_VirtualTexture.is_initialized() ) {
		return false;
	}
	t3dSharedVTBindings_t next = {};
	next.hAllocationTable = m_Resources.get_buffer_rid( T3D_VT_GPU_BUFFER_ALLOCATION_TABLE );
	next.hPageTable = m_Resources.get_buffer_rid( T3D_VT_GPU_BUFFER_PAGE_TABLE );
	next.hAncestorHints = m_Resources.get_buffer_rid( T3D_VT_GPU_BUFFER_ANCESTOR_HINTS );
	next.hMipLayoutTable = m_Resources.get_buffer_rid( T3D_VT_GPU_BUFFER_MIP_LAYOUT_TABLE );
	next.hTailMetadata = m_Resources.get_buffer_rid( T3D_VT_GPU_BUFFER_TAIL_METADATA );
	if ( !next.hAllocationTable.is_valid() || !next.hPageTable.is_valid() || !next.hMipLayoutTable.is_valid() ||
	     !next.hTailMetadata.is_valid() ) {
		return false;
	}
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		next.hTail[c] = m_Resources.get_vt_tail_rid( ( t3dVTResourceChannel_t )c );
		next.nBankCount[c] = m_Resources.get_vt_bank_count( ( t3dVTResourceChannel_t )c );
		// Current shader/descriptor ABI binds exactly one physical bank per channel.
		if ( !next.hTail[c].is_valid() || next.nBankCount[c] != 1u ) {
			return false;
		}
		for ( uint32_t b = 0u; b < next.nBankCount[c]; ++b ) {
			next.hPhysical[c][b] = m_Resources.get_vt_bank_rid( ( t3dVTResourceChannel_t )c, b );
			next.nBankLayers[c][b] = m_Resources.get_vt_bank_layer_count( ( t3dVTResourceChannel_t )c, b );
			if ( !next.hPhysical[c][b].is_valid() || next.nBankLayers[c][b] == 0u ) {
				return false;
			}
		}
	}
	next.nAllocationCount = m_VirtualTexture.getActiveAllocationCount();
	next.nVirtualPageCount = m_VirtualTexture.getActiveVirtualPageCount();
	next.nVTPageTableGeneration = m_VirtualTexture.get_page_table_generation();
	next.nResourceGeneration = m_Resources.get_stats().nResourceGeneration;
	const uint64_t nDeviceGeneration = Terrain3DRdContext::getShared().getDeviceGeneration();
	bool bResourceChanged = m_SharedVTBindings.nBindingGeneration == 0u || m_nPublishedDeviceGeneration != nDeviceGeneration ||
	                        m_SharedVTBindings.nResourceGeneration != next.nResourceGeneration ||
	                        m_SharedVTBindings.hAllocationTable != next.hAllocationTable ||
	                        m_SharedVTBindings.hPageTable != next.hPageTable || m_SharedVTBindings.hAncestorHints != next.hAncestorHints ||
	                        m_SharedVTBindings.hMipLayoutTable != next.hMipLayoutTable ||
	                        m_SharedVTBindings.hTailMetadata != next.hTailMetadata;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		if ( m_SharedVTBindings.hTail[c] != next.hTail[c] || m_SharedVTBindings.nBankCount[c] != next.nBankCount[c] ) {
			bResourceChanged = true;
		}
		for ( uint32_t b = 0u; b < T3D_VT_MAX_TEXTURE_BANKS; ++b ) {
			if ( m_SharedVTBindings.hPhysical[c][b] != next.hPhysical[c][b] ||
			     m_SharedVTBindings.nBankLayers[c][b] != next.nBankLayers[c][b] ) {
				bResourceChanged = true;
			}
		}
	}
	uint64_t nBindingGeneration = m_SharedVTBindings.nBindingGeneration;
	if ( bResourceChanged ) {
		nBindingGeneration = t3dNextGeneration( nBindingGeneration );
		if ( nBindingGeneration == 0u ) {
			nBindingGeneration = 1u;
		}
	}
	next.nBindingGeneration = nBindingGeneration;
	m_SharedVTBindings = next;
	m_nPublishedDeviceGeneration = nDeviceGeneration;
	return true;
}

bool Terrain3DSharedVTRuntime::prepare_shared_vt_cpu_cold( const t3dVTSharedResourceClass_t eResourceClass,
                                                           const uint32_t nPreviewMaterialCapacity,
                                                           const uint32_t nPreviewVirtualPageCapacity,
                                                           const t3dWorldResourceLimits_t *const pProductionWorldLimits )
{
	if ( eResourceClass != T3D_VT_SHARED_RESOURCES_PREVIEW && eResourceClass != T3D_VT_SHARED_RESOURCES_PRODUCTION ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][CpuPlan] Invalid resource class=", ( int )eResourceClass );
		return false;
	}
	if ( m_bSharedResourcesInitialized || m_pRuntimeAllocatorZone == NULL ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][CpuPlan] Invalid owner resources_initialized=",
		                              m_bSharedResourcesInitialized, " allocator_bound=", m_pRuntimeAllocatorZone != NULL );
		return false;
	}
	if ( m_ePreparingSharedResourceClass != T3D_VT_SHARED_RESOURCES_NONE && m_ePreparingSharedResourceClass != eResourceClass ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][CpuPlan] Resource class conflict requested=", ( int )eResourceClass,
		                              " preparing=", ( int )m_ePreparingSharedResourceClass );
		return false;
	}
	if ( m_VirtualTexture.is_cpu_ready() ) {
		return m_ePreparingSharedResourceClass == eResourceClass;
	}
	if ( eResourceClass == T3D_VT_SHARED_RESOURCES_PREVIEW ) {
		m_Limits = t3d_world_make_preview_limits( nPreviewMaterialCapacity, nPreviewVirtualPageCapacity );
		m_Profile = t3d_vt_make_preview_gpu_profile();
	} else {
		if ( pProductionWorldLimits == NULL ) {
			UtilityFunctions::push_error( "[Terrain3D][Startup][VT][CpuPlan] Missing production world limits" );
			return false;
		}
		m_Limits = t3d_world_make_runtime_limits( *pProductionWorldLimits );
		m_Profile = t3d_vt_make_runtime_gpu_profile( *pProductionWorldLimits );
	}
	if ( !t3d_world_validate_limits( m_Limits ) || !t3d_vt_validate_gpu_profile( m_Profile ) ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][CpuPlan] Invalid world limits or GPU profile class=",
		                              ( int )eResourceClass );
		memset( &m_Limits, 0, sizeof( m_Limits ) );
		memset( &m_Profile, 0, sizeof( m_Profile ) );
		return false;
	}
	t3dVTGpuResourcePlan_t Plan = {};
	const uint32_t nTailHeight = m_VirtualTexture.getLoadedPackageTailAtlasHeightCold();
	if ( nTailHeight == 0u || !t3d_vt_compute_resource_plan( m_Limits, m_Profile, Plan, nTailHeight ) ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][CpuPlan] Resource-plan generation failed tail_height=", nTailHeight,
		                              " class=", ( int )eResourceClass );
		memset( &m_Limits, 0, sizeof( m_Limits ) );
		memset( &m_Profile, 0, sizeof( m_Profile ) );
		return false;
	}
	if ( g_t3dMemoryAccount ) {
		const auto domain = g_t3dMemoryAccount->stats().domains[Terrain3DMemoryAccount::VIRTUAL_TEXTURE];
		if ( domain.limit ) {
			const uint64_t available = domain.limit > domain.committed ? domain.limit - domain.committed : 0;
			if ( !t3d_vt_fit_resource_plan_to_budget( m_Limits, m_Profile, available, Plan, nTailHeight ) ) {
				UtilityFunctions::push_error( "[Terrain3D][Startup][VT][CpuPlan] Memory budget rejection limit=", domain.limit,
				                              " committed=", domain.committed, " available=", available, " tail_height=", nTailHeight );
				g_t3dMemoryAccount->recordDenial( Terrain3DMemoryAccount::VIRTUAL_TEXTURE );
				memset( &m_Limits, 0, sizeof( m_Limits ) );
				memset( &m_Profile, 0, sizeof( m_Profile ) );
				return false;
			}
		}
	}
	m_ePreparingSharedResourceClass = eResourceClass;
	const uint32_t nGeneration = m_VirtualTexture.get_startup_snapshot().m_nGeneration;
	if ( !m_VirtualTexture.prepare_cpu_cold( m_Limits, Plan, nGeneration ) ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][CpuPlan] VT CPU preparation rejected generation=", nGeneration,
		                              " class=", ( int )eResourceClass );
		m_ePreparingSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
		memset( &m_Limits, 0, sizeof( m_Limits ) );
		memset( &m_Profile, 0, sizeof( m_Profile ) );
		return false;
	}
	UtilityFunctions::print( "[Terrain3D][Startup][VT][CpuPlan] READY generation=", nGeneration, " class=", ( int )eResourceClass,
	                         " tail_height=", nTailHeight );
	return true;
}

bool Terrain3DSharedVTRuntime::prepare_shared_vt_storage_cold( Terrain3DRdContext& rContext )
{
	while ( !m_VirtualTexture.is_gpu_storage_ready() ) {
		if ( !service_shared_vt_storage_cold( rContext, UINT32_MAX ) ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DSharedVTRuntime::service_shared_vt_storage_cold( Terrain3DRdContext& rContext, const uint32_t nMaxWorkUnits )
{
	if ( m_bSharedResourcesInitialized || m_ePreparingSharedResourceClass == T3D_VT_SHARED_RESOURCES_NONE ||
	     !m_VirtualTexture.is_cpu_ready() || nMaxWorkUnits == 0u ) {
		return false;
	}
	if ( m_VirtualTexture.is_gpu_storage_ready() ) {
		return m_Resources.is_initialized();
	}
	if ( !rContext.initialize() || !rContext.validateDevice() ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][Storage] RD context/device invalid reason=", rContext.getLastFailure() );
		return false;
	}
	uint32_t nUnitsRemaining = nMaxWorkUnits;
	if ( !m_Resources.is_initialized() ) {
		if ( !m_Resources.is_initialize_pending_cold() &&
		     !m_Resources.begin_initialize_cold( rContext, m_Limits, m_Profile,
		                                         m_VirtualTexture.get_gpu().get_cpu_resource_plan().nVtTailAtlasHeight ) ) {
			UtilityFunctions::push_error( "[Terrain3D][Startup][VT][Storage] GPU resource allocation begin failed reason=",
			                              rContext.getLastFailure() );
			return false;
		}
		if ( !m_Resources.service_initialize_cold( nUnitsRemaining ) ) {
			UtilityFunctions::push_error( "[Terrain3D][Startup][VT][Storage] GPU resource allocation service failed reason=",
			                              rContext.getLastFailure() );
			return false;
		}
		if ( !m_Resources.is_initialized() ) {
			return true;
		}
		if ( nMaxWorkUnits != UINT32_MAX ) {
			return true;
		}
	}
	if ( !m_VirtualTexture.prepare_gpu_storage_cold( &m_Resources ) ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][Storage] VT GPU storage publication rejected" );
		m_Resources.shutdown_cold();
		m_ePreparingSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
		memset( &m_Limits, 0, sizeof( m_Limits ) );
		memset( &m_Profile, 0, sizeof( m_Profile ) );
		return false;
	}
	return true;
}

bool Terrain3DSharedVTRuntime::bind_shared_vt_executable_cold()
{
	while ( !m_VirtualTexture.is_gpu_ready() ) {
		if ( !service_shared_vt_executable_cold( UINT32_MAX ) ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DSharedVTRuntime::service_shared_vt_executable_cold( const uint32_t nMaxWorkUnits )
{
	if ( m_bSharedResourcesInitialized || !m_VirtualTexture.is_gpu_storage_ready() || nMaxWorkUnits == 0u ) {
		return false;
	}
	if ( m_VirtualTexture.is_gpu_ready() ) {
		return true;
	}
	if ( !m_VirtualTexture.service_gpu_executable_cold( nMaxWorkUnits ) ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][Executable] GPU executable initialization failed" );
		m_Resources.shutdown_cold();
		m_ePreparingSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
		memset( &m_Limits, 0, sizeof( m_Limits ) );
		memset( &m_Profile, 0, sizeof( m_Profile ) );
		return false;
	}
	return true;
}

bool Terrain3DSharedVTRuntime::publish_shared_vt_cold()
{
	while ( !m_bSharedResourcesInitialized ) {
		if ( !service_shared_vt_publication_cold( UINT64_MAX, UINT32_MAX ) ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DSharedVTRuntime::service_shared_vt_publication_cold( const uint64_t nMaxBytes, const uint32_t nMaxWorkUnits )
{
	if ( m_bSharedResourcesInitialized ) {
		return true;
	}
	if ( m_ePreparingSharedResourceClass == T3D_VT_SHARED_RESOURCES_NONE || !m_VirtualTexture.is_gpu_ready() || nMaxBytes == 0u ||
	     nMaxWorkUnits == 0u ) {
		return false;
	}
	if ( !m_VirtualTexture.is_initial_published() && !m_VirtualTexture.service_initial_publication_cold( nMaxBytes, nMaxWorkUnits ) ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][Publication] Initial page/bank publication failed bytes_budget=", nMaxBytes,
		                              " work_budget=", nMaxWorkUnits );
		m_Resources.shutdown_cold();
		m_ePreparingSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
		memset( &m_Limits, 0, sizeof( m_Limits ) );
		memset( &m_Profile, 0, sizeof( m_Profile ) );
		return false;
	}
	if ( !m_VirtualTexture.is_initial_published() ) {
		return true;
	}
	m_eSharedResourceClass = m_ePreparingSharedResourceClass;
	m_ePreparingSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
	m_bSharedResourcesInitialized = true;
	if ( !refresh_shared_vt_bindings_hot() ) {
		UtilityFunctions::push_error( "[Terrain3D][Startup][VT][Publication] Published descriptor binding validation failed" );
		m_VirtualTexture.shutdown_cold();
		m_Resources.shutdown_cold();
		m_bSharedResourcesInitialized = false;
		m_eSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
		m_SharedVTBindings = t3dSharedVTBindings_t{};
		memset( &m_Limits, 0, sizeof( m_Limits ) );
		memset( &m_Profile, 0, sizeof( m_Profile ) );
		return false;
	}
	// Initial tables and persistent storage are published. Seal the owner so
	// admitted requests can use its existing bounded streaming/upload path.
	m_Resources.seal_hot_runtime();
	m_VirtualTexture.seal_hot_runtime();
	UtilityFunctions::print( "[Terrain3D][Startup][VT][Publication] READY binding_generation=", m_SharedVTBindings.nBindingGeneration,
	                         " page_table_generation=", m_SharedVTBindings.nVTPageTableGeneration,
	                         " resources_generation=", m_SharedVTBindings.nResourceGeneration );
	return true;
}

bool Terrain3DSharedVTRuntime::ensure_shared_resources_cold( Terrain3DRdContext& rContext, const t3dVTSharedResourceClass_t eResourceClass,
                                                             const uint32_t nPreviewMaterialCapacity,
                                                             const uint32_t nPreviewVirtualPageCapacity,
                                                             const t3dWorldResourceLimits_t *const pProductionWorldLimits )
{
	if ( eResourceClass != T3D_VT_SHARED_RESOURCES_PREVIEW && eResourceClass != T3D_VT_SHARED_RESOURCES_PRODUCTION ) {
		return false;
	}
	if ( m_bSharedResourcesInitialized ) {
		const bool bClassCompatible =
		    m_eSharedResourceClass == eResourceClass ||
		    ( m_eSharedResourceClass == T3D_VT_SHARED_RESOURCES_PRODUCTION && eResourceClass == T3D_VT_SHARED_RESOURCES_PREVIEW );
		if ( !bClassCompatible || !m_Resources.is_initialized() || !m_VirtualTexture.is_initialized() ) {
			return false;
		}
		const bool bBindingsReady = m_SharedVTBindings.hAllocationTable.is_valid() && m_SharedVTBindings.hPageTable.is_valid() &&
		                            m_SharedVTBindings.hMipLayoutTable.is_valid() && m_SharedVTBindings.hTailMetadata.is_valid() &&
		                            m_SharedVTBindings.nResourceGeneration == m_Resources.get_stats().nResourceGeneration;
		if ( bBindingsReady ) {
			m_SharedVTBindings.nAllocationCount = m_VirtualTexture.getActiveAllocationCount();
			m_SharedVTBindings.nVirtualPageCount = m_VirtualTexture.getActiveVirtualPageCount();
			m_SharedVTBindings.nVTPageTableGeneration = m_VirtualTexture.get_page_table_generation();
			return true;
		}
		return refresh_shared_vt_bindings_hot();
	}
	return ( m_VirtualTexture.is_cpu_ready() || prepare_shared_vt_cpu_cold( eResourceClass, nPreviewMaterialCapacity,
	                                                                        nPreviewVirtualPageCapacity, pProductionWorldLimits ) ) &&
	       ( m_VirtualTexture.is_gpu_storage_ready() || prepare_shared_vt_storage_cold( rContext ) ) &&
	       ( m_VirtualTexture.is_gpu_ready() || bind_shared_vt_executable_cold() ) && publish_shared_vt_cold();
}

void Terrain3DSharedVTRuntime::invalidateBoundDescriptorsCold( Terrain3DRdContext& rContext )
{
	Terrain3DRdGlobalDescriptorCache *const pCache = rContext.tryGetGlobalDescriptorCacheHot();
	if ( !pCache || !pCache->isInitialized() ) {
		return;
	}
	const uint64_t nDeviceGeneration = m_nPublishedDeviceGeneration;
	if ( nDeviceGeneration == 0u ) {
		return;
	}
	const t3dSharedVTBindings_t& rBindings = m_SharedVTBindings;
	pCache->invalidateDependencyCold( rBindings.hAllocationTable, nDeviceGeneration );
	pCache->invalidateDependencyCold( rBindings.hPageTable, nDeviceGeneration );
	pCache->invalidateDependencyCold( rBindings.hMipLayoutTable, nDeviceGeneration );
	pCache->invalidateDependencyCold( rBindings.hTailMetadata, nDeviceGeneration );
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		pCache->invalidateDependencyCold( rBindings.hTail[c], nDeviceGeneration );
		for ( uint32_t b = 0u; b < T3D_VT_MAX_TEXTURE_BANKS; ++b ) {
			pCache->invalidateDependencyCold( rBindings.hPhysical[c][b], nDeviceGeneration );
		}
	}
}

bool Terrain3DSharedVTRuntime::beginVtResourceReplacementCold( Terrain3DRdContext& rContext )
{
	if ( !Terrain3DRdContext::isOnRenderThread() || m_bResourceReplacementInProgress ) {
		return false;
	}
	m_bResourceReplacementInProgress = true;
	// No future PREPARE can acquire this publication. All dependent descriptor
	// leases become stale before underlying buffers and textures are released.
	invalidateBoundDescriptorsCold( rContext );
	return true;
}

bool Terrain3DSharedVTRuntime::commitVtResourceReplacementCold( Terrain3DRdContext& rContext )
{
	if ( !Terrain3DRdContext::isOnRenderThread() || !m_bResourceReplacementInProgress || !m_bSharedResourcesInitialized ||
	     !rContext.validateDevice() || !refresh_shared_vt_bindings_hot() ) {
		return false;
	}
	m_bResourceReplacementInProgress = false;
	return true;
}

void Terrain3DSharedVTRuntime::abortVtResourceReplacementCold()
{
	if ( !Terrain3DRdContext::isOnRenderThread() ) {
		return;
	}
	m_bResourceReplacementInProgress = false;
}

void Terrain3DSharedVTRuntime::shutdown_cold( const bool bPreserveTailStaging )
{
	const bool bReplacementAlreadyActive = m_bResourceReplacementInProgress;
	if ( Terrain3DRdContext::isOnRenderThread() && !bReplacementAlreadyActive ) {
		beginVtResourceReplacementCold( Terrain3DRdContext::getShared() );
	}
	m_VirtualTexture.shutdown_cold( bPreserveTailStaging );
	m_Resources.unseal_hot_runtime_cold();
	m_Resources.shutdown_cold();
	m_bSharedResourcesInitialized = false;
	m_eSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
	m_ePreparingSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
	// Retained world descriptors key on this generation. Reusing generation 1
	// after a VT-only replacement would let them bind retired resource handles.
	const uint64_t nBindingGeneration = m_SharedVTBindings.nBindingGeneration;
	m_SharedVTBindings = t3dSharedVTBindings_t{};
	m_SharedVTBindings.nBindingGeneration = nBindingGeneration;
	memset( &m_Limits, 0, sizeof( m_Limits ) );
	memset( &m_Profile, 0, sizeof( m_Profile ) );
	m_bResourceReplacementInProgress = bReplacementAlreadyActive;
}

bool Terrain3DSharedVTRuntime::replacePackageResourcesCold( Terrain3DRdContext& rContext, const void *const pBytes, const uint64_t nBytes,
                                                            const bool bNeutral, const t3dVTSharedResourceClass_t eResourceClass,
                                                            const t3dWorldResourceLimits_t *const pProductionWorldLimits )
{
	if ( !Terrain3DRdContext::isOnRenderThread() || m_pRuntimeAllocatorZone == NULL ||
	     ( eResourceClass != T3D_VT_SHARED_RESOURCES_PREVIEW && eResourceClass != T3D_VT_SHARED_RESOURCES_PRODUCTION ) ||
	     ( eResourceClass == T3D_VT_SHARED_RESOURCES_PRODUCTION && pProductionWorldLimits == NULL ) ||
	     ( !bNeutral && ( pBytes == NULL || nBytes == 0u ) ) ) {
		return false;
	}
	// The caller retains last-good package bytes for rollback. Free only VT
	// ownership to admit the candidate without simultaneous old/new storage.
	if ( !beginVtResourceReplacementCold( rContext ) ) {
		return false;
	}
	shutdown_cold( true );
	if ( !( bNeutral ? m_VirtualTexture.clearPackageCold() : m_VirtualTexture.loadPackageCold( pBytes, nBytes ) ) ) {
		abortVtResourceReplacementCold();
		return false;
	}
	const uint32_t nMaterials = MAX( 1u, m_VirtualTexture.getLoadedPackageAllocationCountCold() );
	const uint32_t nPages = MAX( 1u, m_VirtualTexture.getLoadedPackageVirtualPageCountCold() );
	const bool bReady = ensure_shared_resources_cold( rContext, eResourceClass, nMaterials, nPages, pProductionWorldLimits );
	if ( bReady && commitVtResourceReplacementCold( rContext ) ) {
		return true;
	}
	abortVtResourceReplacementCold();
	return false;
}

bool Terrain3DSharedVTRuntime::stagePackageWarm( const void *const pBytes, const uint64_t nBytes )
{
	return Terrain3DRdContext::isOnRenderThread() && are_shared_resources_ready() && m_VirtualTexture.stagePackageWarm( pBytes, nBytes );
}

bool Terrain3DSharedVTRuntime::stagePackageFileWarm( const godot::String& rPath )
{
	return Terrain3DRdContext::isOnRenderThread() && are_shared_resources_ready() && m_VirtualTexture.stagePackageFileWarm( rPath );
}

bool Terrain3DSharedVTRuntime::servicePackageRepublishWarm( const uint64_t nMaxBytes, const uint32_t nMaxWorkUnits )
{
	if ( !Terrain3DRdContext::isOnRenderThread() || !are_shared_resources_ready() ) {
		return false;
	}
	const bool bResult = m_VirtualTexture.servicePackageRepublishWarm( nMaxBytes, nMaxWorkUnits );
	// Compatible publication changes table contents/counts, never resource handles.
	if ( !m_VirtualTexture.isPackageRepublishPendingWarm() && m_VirtualTexture.is_initialized() ) {
		m_SharedVTBindings.nAllocationCount = m_VirtualTexture.getActiveAllocationCount();
		m_SharedVTBindings.nVirtualPageCount = m_VirtualTexture.getActiveVirtualPageCount();
		m_SharedVTBindings.nVTPageTableGeneration = m_VirtualTexture.get_page_table_generation();
	}
	return bResult;
}

void Terrain3DSharedVTRuntime::abortPackageRepublishWarm()
{
	if ( Terrain3DRdContext::isOnRenderThread() ) {
		m_VirtualTexture.abortPackageRepublishWarm();
	}
}

bool Terrain3DSharedVTRuntime::are_shared_resources_ready() const
{
	t3dSharedVTBindings_t bindings = {};
	return acquirePublishedBindingsHot( m_eSharedResourceClass, bindings ) == T3D_VT_ACQUIRE_READY;
}
t3dVTSharedResourceClass_t Terrain3DSharedVTRuntime::get_shared_resource_class() const
{
	return m_eSharedResourceClass;
}
const t3dWorldLimits_t& Terrain3DSharedVTRuntime::get_limits() const
{
	return m_Limits;
}
const Terrain3DVTResourceImage& Terrain3DSharedVTRuntime::get_resources() const
{
	return m_Resources;
}
t3dVTAcquireStatus_t Terrain3DSharedVTRuntime::acquirePublishedBindingsHot( const t3dVTSharedResourceClass_t eRequestedClass,
                                                                            t3dSharedVTBindings_t& rBindings ) const
{
	if ( eRequestedClass != T3D_VT_SHARED_RESOURCES_PREVIEW && eRequestedClass != T3D_VT_SHARED_RESOURCES_PRODUCTION ) {
		return T3D_VT_ACQUIRE_FAILED;
	}
	if ( m_bResourceReplacementInProgress ||
	     ( m_ePreparingSharedResourceClass != T3D_VT_SHARED_RESOURCES_NONE && !m_bSharedResourcesInitialized ) ) {
		return T3D_VT_ACQUIRE_REBUILD_IN_PROGRESS;
	}
	if ( !m_bSharedResourcesInitialized || !m_Resources.is_initialized() || !m_VirtualTexture.is_initialized() ) {
		return T3D_VT_ACQUIRE_UNPUBLISHED;
	}
	if ( m_eSharedResourceClass != eRequestedClass &&
	     !( m_eSharedResourceClass == T3D_VT_SHARED_RESOURCES_PRODUCTION && eRequestedClass == T3D_VT_SHARED_RESOURCES_PREVIEW ) ) {
		return T3D_VT_ACQUIRE_RESOURCE_CLASS_MISMATCH;
	}
	const t3dSharedVTBindings_t& published = m_SharedVTBindings;
	if ( published.nBindingGeneration == 0u || published.nResourceGeneration != m_Resources.get_stats().nResourceGeneration ||
	     m_nPublishedDeviceGeneration != Terrain3DRdContext::getShared().getDeviceGeneration() || !published.hAllocationTable.is_valid() ||
	     !published.hPageTable.is_valid() || !published.hMipLayoutTable.is_valid() || !published.hTailMetadata.is_valid() ||
	     published.hAllocationTable != m_Resources.get_buffer_rid( T3D_VT_GPU_BUFFER_ALLOCATION_TABLE ) ||
	     published.hPageTable != m_Resources.get_buffer_rid( T3D_VT_GPU_BUFFER_PAGE_TABLE ) ||
	     published.hMipLayoutTable != m_Resources.get_buffer_rid( T3D_VT_GPU_BUFFER_MIP_LAYOUT_TABLE ) ||
	     published.hTailMetadata != m_Resources.get_buffer_rid( T3D_VT_GPU_BUFFER_TAIL_METADATA ) ) {
		return T3D_VT_ACQUIRE_INVALID_BINDINGS;
	}
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		const t3dVTResourceChannel_t eChannel = ( t3dVTResourceChannel_t )c;
		if ( published.nBankCount[c] != 1u || !published.hTail[c].is_valid() || !published.hPhysical[c][0].is_valid() ||
		     published.nBankLayers[c][0] == 0u || published.hTail[c] != m_Resources.get_vt_tail_rid( eChannel ) ||
		     published.nBankCount[c] != m_Resources.get_vt_bank_count( eChannel ) ||
		     published.hPhysical[c][0] != m_Resources.get_vt_bank_rid( eChannel, 0u ) ||
		     published.nBankLayers[c][0] != m_Resources.get_vt_bank_layer_count( eChannel, 0u ) ) {
			return T3D_VT_ACQUIRE_INVALID_BINDINGS;
		}
	}
	rBindings = published;
	return T3D_VT_ACQUIRE_READY;
}
