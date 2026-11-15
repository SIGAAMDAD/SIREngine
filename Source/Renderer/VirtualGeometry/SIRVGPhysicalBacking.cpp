/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/VirtualGeometry/SIRVGPhysicalBacking.h>
#include <Core/Platform/SIRAtomic.h>

// Native handles are only unique within their owning device. Derived-resource
// callbacks require a unique process-local allocation token even when two
// devices reuse exactly the same driver buffer slot and generation.
static SIRAtomic<uint64_t> s_nNextGeometryNativeToken( 1 );

static uint64_t sirVGAllocateNativeToken( void ) noexcept
{
	uint64_t nNext = s_nNextGeometryNativeToken.load( SIR_MEMORY_ORDER_RELAXED );
	for ( ;; ) {
		if ( nNext == UINT64_MAX ) {
			return 0;
		}
		if ( s_nNextGeometryNativeToken.compareExchangeWeak( nNext, nNext + 1, SIR_MEMORY_ORDER_RELAXED, SIR_MEMORY_ORDER_RELAXED ) ) {
			return nNext;
		}
	}
}

SIRVGPhysicalBacking::SIRVGPhysicalBacking( void ) noexcept
    : m_pDevice( nullptr ), m_pRegistry( nullptr ), m_Key{}, m_Handle{}, m_hBacking{}, m_hUpload{}, m_nPhysicalBackingId( 0 ),
      m_nNativeToken( 0 ), m_nLastDirectFence( 0 ), m_nDeviceGeneration( 0 ), m_bPublished( false ), m_bRetiring( false )
{
}

sirGraphicsResult_t SIRVGPhysicalBacking::initialize( const sirVGPhysicalBackingConfig_t& config ) noexcept
{
	if ( m_bPublished || config.pDevice == nullptr || config.pRegistry == nullptr || config.pScope == nullptr ||
	     config.pSource == nullptr || !config.pScope->isActive() || config.nCapacityBytes == 0 || config.nUploadBytes == 0 ||
	     config.keyRepresentation.m_nProviderId == 0 || config.keyRepresentation.m_nRepresentationId == 0 ||
	     config.keyRepresentation.m_nRevision == 0 )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	const uint32_t nDeviceGeneration = config.pDevice->getDeviceGeneration();
	if ( nDeviceGeneration == 0 || config.keyRepresentation.m_nRevision != config.pSource->getRevision() )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	if ( config.pRegistry->isReady() == false )
		return SIR_GRAPHICS_BUSY;
	SIRDerivedResourceLease existing = {};
	if ( config.pRegistry->acquireRepresentation( config.keyRepresentation, &existing ) == SIR_RESOURCE_RESULT_SUCCESS ) {
		( void )config.pRegistry->releaseRepresentation( existing );
		return SIR_GRAPHICS_BUSY;
	}
	const sirDerivedDeviceIdentity_t deviceIdentity = { ( uint64_t )( uintptr_t )config.pDevice, nDeviceGeneration };
	if ( config.pRegistry->acquireCanonicalRepresentation( config.keyRepresentation.m_Resource, config.keyRepresentation.m_nRevision,
	                                                       deviceIdentity, &existing ) == SIR_RESOURCE_RESULT_SUCCESS ) {
		( void )config.pRegistry->releaseRepresentation( existing );
		return SIR_GRAPHICS_BUSY;
	}
	const uint64_t nNativeToken = sirVGAllocateNativeToken();
	if ( nNativeToken == 0 ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = config.nCapacityBytes;
	desc.nAlignmentBytes = 0; // Native Vulkan placement does not accept explicit resource alignment.
	desc.nUsage = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_INDEX | SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COPY_DESTINATION |
	              SIR_GRAPHICS_USAGE_COPY_SOURCE;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hBacking = {};
	sirGraphicsResult_t eCreate = config.pDevice->createBuffer( desc, &hBacking );
	if ( eCreate != SIR_GRAPHICS_SUCCESS )
		return eCreate;
	sirGraphicsHandle_t hUpload = {};
	desc.nSizeBytes = config.nUploadBytes;
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	eCreate = config.pDevice->createBuffer( desc, &hUpload );
	sirGraphicsCompletionSet_t noGpuWork = {};
	noGpuWork.nDeviceGeneration = nDeviceGeneration;
	noGpuWork.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		( void )config.pDevice->retireResource( hBacking, noGpuWork );
		return eCreate;
	}
	m_pDevice = config.pDevice;
	m_pRegistry = config.pRegistry;
	m_Key = config.keyRepresentation;
	m_hBacking = hBacking;
	m_hUpload = hUpload;
	m_nPhysicalBackingId = 0;
	m_nDeviceGeneration = nDeviceGeneration;
	m_nNativeToken = nNativeToken;
	const SIRDerivedProviderCallbacks callbacks = { this, releaseNative };
	SIRResourceResult eRegistration =
	    m_pRegistry->registerProvider( config.keyRepresentation.m_nProviderId, nDeviceGeneration, 1, callbacks );
	const bool bProviderRegistered = eRegistration == SIR_RESOURCE_RESULT_SUCCESS;
	if ( eRegistration == SIR_RESOURCE_RESULT_BUSY ) {
		// The device's provider is shared by many canonical geometry assets.
		// Per-allocation callbacks below retain the exact owning object.
		eRegistration = SIR_RESOURCE_RESULT_SUCCESS;
	}
	if ( eRegistration == SIR_RESOURCE_RESULT_SUCCESS ) {
		SIRDerivedResourceRegistration representation = {};
		representation.m_Key = config.keyRepresentation;
		representation.m_Accounting = { SIR_MEMORY_SPACE_DEVICE, config.nMemoryDomain, config.nCapacityBytes, config.nCapacityBytes,
		                                config.keyRepresentation.m_nRevision };
		representation.m_nNativeToken = m_nNativeToken;
		representation.m_nPhysicalBackingId = 0;
		representation.m_nDeviceGeneration = nDeviceGeneration;
		representation.m_nDeviceIdentity = deviceIdentity.nDeviceId;
		representation.m_bCanonicalSource = 1;
		representation.m_pReleaseContext = this;
		representation.m_pfnReleaseNative = releaseNative;
		eRegistration = m_pRegistry->registerRepresentation( *config.pScope, *config.pSource, representation, &m_Handle );
	}
	if ( eRegistration != SIR_RESOURCE_RESULT_SUCCESS ) {
		if ( bProviderRegistered )
			( void )m_pRegistry->unregisterProvider( config.keyRepresentation.m_nProviderId );
		( void )config.pDevice->retireResource( m_hUpload, noGpuWork );
		( void )config.pDevice->retireResource( m_hBacking, noGpuWork );
		m_hBacking = {};
		m_hUpload = {};
		m_pDevice = nullptr;
		m_pRegistry = nullptr;
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_pRegistry->getPhysicalBackingId( m_Handle, &m_nPhysicalBackingId ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     m_nPhysicalBackingId == 0 ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bPublished = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGPhysicalBacking::getBackingId( uint64_t *pBackingId ) const noexcept
{
	if ( !m_bPublished || m_bRetiring || pBackingId == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	*pBackingId = m_nPhysicalBackingId;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGPhysicalBacking::getBackingBuffer( sirGraphicsHandle_t *pBuffer ) const noexcept
{
	if ( !m_bPublished || m_bRetiring || pBuffer == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	*pBuffer = m_hBacking;
	return SIR_GRAPHICS_SUCCESS;
}

SIRResourceResult SIRVGPhysicalBacking::acquire( SIRDerivedResourceLease *pLease ) noexcept
{
	if ( !m_bPublished || m_bRetiring || pLease == nullptr )
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	return m_pRegistry->acquireRepresentation( m_Key, pLease );
}

SIRResourceResult SIRVGPhysicalBacking::release( const SIRDerivedResourceLease& lease ) noexcept
{
	if ( !m_bPublished )
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	return m_pRegistry->releaseRepresentation( lease );
}

sirGraphicsResult_t SIRVGPhysicalBacking::resolve( const SIRDerivedResourceLease& lease, sirGraphicsHandle_t *pBacking ) const noexcept
{
	if ( !m_bPublished || pBacking == nullptr || m_bRetiring || lease.nPhysicalBackingId != m_nPhysicalBackingId ||
	     lease.nNativeToken != m_nNativeToken || lease.nDeviceGeneration != m_nDeviceGeneration ||
	     lease.nDeviceIdentity != ( uint64_t )( uintptr_t )m_pDevice || lease.nSourceRevision != m_Key.m_nRevision ||
	     lease.hRepresentation.m_nSlot != m_Handle.m_nSlot || lease.hRepresentation.m_nGeneration != m_Handle.m_nGeneration )
		return SIR_GRAPHICS_INVALID_HANDLE;
	*pBacking = m_hBacking;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGPhysicalBacking::getUploadBuffer( sirGraphicsHandle_t *pUpload ) const noexcept
{
	if ( pUpload == nullptr || !m_bPublished || m_bRetiring )
		return SIR_GRAPHICS_INVALID_HANDLE;
	*pUpload = m_hUpload;
	return SIR_GRAPHICS_SUCCESS;
}

SIRResourceResult SIRVGPhysicalBacking::retire( uint64_t nDirectFence ) noexcept
{
	if ( !m_bPublished || m_bRetiring || m_nDeviceGeneration != m_pDevice->getDeviceGeneration() )
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	sirProviderCompletionSet_t required = {};
	required.nProviderId = m_Key.m_nProviderId;
	required.nProviderGeneration = m_nDeviceGeneration;
	required.nLaneMask = 1;
	required.nLaneCount = 1;
	required.szCompletedValues[0] = nDirectFence;
	const SIRResourceResult eRetire = m_pRegistry->retireRepresentation( m_Handle, required );
	if ( eRetire == SIR_RESOURCE_RESULT_SUCCESS ) {
		m_nLastDirectFence = nDirectFence;
		m_bRetiring = true;
	}
	return eRetire;
}

SIRResourceResult SIRVGPhysicalBacking::observeRetirement( uint64_t nDirectFence ) noexcept
{
	if ( !m_bPublished || !m_bRetiring || nDirectFence < m_nLastDirectFence )
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	if ( nDirectFence != 0 ) {
		const sirGraphicsCompletion_t native = { m_nDeviceGeneration, SIR_GRAPHICS_QUEUE_DIRECT, nDirectFence };
		bool bCompleted = false;
		if ( m_pDevice->poll( native, &bCompleted ) != SIR_GRAPHICS_SUCCESS || !bCompleted )
			return SIR_RESOURCE_RESULT_BUSY;
	}
	sirProviderCompletionSet_t observed = {};
	observed.nProviderId = m_Key.m_nProviderId;
	observed.nProviderGeneration = m_nDeviceGeneration;
	observed.nLaneMask = 1;
	observed.nLaneCount = 1;
	observed.szCompletedValues[0] = nDirectFence;
	return m_pRegistry->updateProviderCompletion( observed );
}

bool SIRVGPhysicalBacking::releaseNative( void *pContext, uint64_t nNativeToken ) noexcept
{
	SIRVGPhysicalBacking *pBacking = ( SIRVGPhysicalBacking * )pContext;
	return pBacking != nullptr && pBacking->retireBuffers( nNativeToken );
}

bool SIRVGPhysicalBacking::retireBuffers( uint64_t nNativeToken ) noexcept
{
	if ( !m_bRetiring || m_nNativeToken != nNativeToken || m_pDevice == nullptr )
		return false;
	sirGraphicsCompletionSet_t completed = {};
	completed.nDeviceGeneration = m_nDeviceGeneration;
	completed.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	completed.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = m_nLastDirectFence;
	if ( m_hBacking.nGeneration != 0 ) {
		if ( m_pDevice->retireResource( m_hBacking, completed ) != SIR_GRAPHICS_SUCCESS )
			return false;
		m_hBacking = {};
	}
	// Registry's callback is retryable. A native failure must not attempt
	// to retire the same already-invalid buffer again on the next poll.
	const sirGraphicsResult_t eUpload = m_pDevice->retireResource( m_hUpload, completed );
	if ( eUpload != SIR_GRAPHICS_SUCCESS )
		return false;
	m_hUpload = {};
	m_bPublished = false;
	return true;
}
