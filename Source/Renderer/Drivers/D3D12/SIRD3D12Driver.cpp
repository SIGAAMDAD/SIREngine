/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
CMake Warning at Build/CMake/CIFormat.cmake:34 (message):
  Formatting error: Source/Renderer/Drivers/D3D12/SIRD3D12Driver.cpp


License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.

===========================================================================
*/

#include <Renderer/Drivers/D3D12/SIRD3D12Driver.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <limits.h>

static constexpr uint64_t SIR_D3D12_SHARED_FEATURES = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE |
                                                      SIR_GRAPHICS_FEATURE_COPY_QUEUE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE |
                                                      SIR_GRAPHICS_FEATURE_MRT | SIR_GRAPHICS_FEATURE_PRESENT;

static sirGraphicsResult_t sirD3D12FindAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) noexcept
{
	if ( pInfo == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pInfo = {};
	Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory;
	if ( FAILED( CreateDXGIFactory2( 0, IID_PPV_ARGS( pFactory.GetAddressOf() ) ) ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pSelected;
	const HRESULT nSelected = sirD3D12EnumerateAdapters( pFactory.Get(), nAdapterIndex, pSelected.GetAddressOf(), nullptr );
	if ( FAILED( nSelected ) ) {
		return nSelected == DXGI_ERROR_NOT_FOUND ? SIR_GRAPHICS_UNSUPPORTED : SIR_GRAPHICS_DRIVER_ERROR;
	}
	DXGI_ADAPTER_DESC3 desc = {};
	if ( FAILED( pSelected->GetDesc3( &desc ) ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pInfo->nAdapterIndex = nAdapterIndex;
	pInfo->nVendorId = desc.VendorId;
	pInfo->nDeviceId = desc.DeviceId;
	pInfo->nAdapterLuid = ( ( uint64_t )( uint32_t )desc.AdapterLuid.HighPart << 32 ) | desc.AdapterLuid.LowPart;
	pInfo->nDedicatedVideoBytes = desc.DedicatedVideoMemory;
	pInfo->nSharedSystemBytes = desc.SharedSystemMemory;
	pInfo->nFeatureMask = SIR_D3D12_SHARED_FEATURES;
	pInfo->nApiVersion = { 12, 0, 0 };
	pInfo->bSoftwareAdapter = ( desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE ) != 0;
	if ( WideCharToMultiByte( CP_UTF8, 0, desc.Description, -1, pInfo->szName, ( int )sizeof( pInfo->szName ), nullptr, nullptr ) == 0 ) {
		pInfo->szName[0] = '\0';
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsBackend_t SIRD3D12RenderingDevice::getBackend( void ) const noexcept
{
	return SIR_GRAPHICS_BACKEND_D3D12;
}

uint32_t SIRD3D12RenderingDevice::getDeviceGeneration( void ) const noexcept
{
	sirD3D12CompletionSet_t native = {};
	if ( !m_Native.snapshotCompletionSet( &native ) || native.nDeviceGeneration > UINT32_MAX ) {
		return 0;
	}
	return ( uint32_t )native.nDeviceGeneration;
}

sirGraphicsResult_t SIRD3D12RenderingDevice::initialize( bool bSoftware, const sirGraphicsDeviceCreateDesc_t &desc,
                                                         const sirD3D12SealedPoolConfig_t *pSealedPools ) noexcept
{
	if ( desc.nFramesInFlight == 0 || desc.nFramesInFlight > 3 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGraphicsHasFeatures( SIR_D3D12_SHARED_FEATURES, desc.nRequiredFeatures ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pSealedPools != nullptr && !desc.bHeadless ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( desc.bEnableGpuValidation && !desc.bEnableApiValidation ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint64_t nSlotBytes = desc.nUploadBudgetBytes == 0 ? 64 * 1024 : desc.nUploadBudgetBytes / 3;
	if ( nSlotBytes == 0 || nSlotBytes > UINT64_MAX / 3 ||
	     ( desc.nReadbackBudgetBytes != 0 && desc.nReadbackBudgetBytes / 3 < nSlotBytes ) ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirD3D12TransferPoolConfig_t config = {};
	config.nSlotBytes = nSlotBytes;
	config.bPreferDirectMapped = true;
	sirD3D12DiagnosticConfig_t diagnostics = {};
	diagnostics.bEnableDebugLayer = desc.bEnableApiValidation;
	diagnostics.bEnableGpuValidation = desc.bEnableGpuValidation;
	diagnostics.bEnableDred = desc.bEnableDeviceFaultDiagnostics;
	if ( !m_Native.initialize( desc.nAdapterIndex, config, desc.pCpuHeap, diagnostics ) ) {
		return m_Native.getLastError() == E_NOTIMPL ? SIR_GRAPHICS_UNSUPPORTED : SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bSoftware = bSoftware;
	if ( !m_Native.getMemory()->configureResidentBudget( desc.nResidentBudgetBytes ) ) {
		m_Native.shutdown();
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( getDeviceGeneration() == 0 ) {
		m_Native.shutdown();
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( !m_Resources.initialize( &m_Native, getDeviceGeneration(), desc.nResidentBudgetBytes ) ) {
		m_Native.shutdown();
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( !m_Pipelines.initialize( &m_Native, getDeviceGeneration() ) ) {
		m_Resources.shutdown( true );
		m_Native.shutdown();
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( !m_Bindings.initialize( &m_Native, &m_Resources, &m_Pipelines, getDeviceGeneration() ) ) {
		m_Pipelines.shutdown( true );
		m_Resources.shutdown( true );
		m_Native.shutdown();
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( !m_Commands.initialize( &m_Native, &m_Resources, &m_Pipelines, &m_Bindings ) ) {
		m_Bindings.shutdown( true );
		m_Pipelines.shutdown( true );
		m_Resources.shutdown( true );
		m_Native.shutdown();
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( !m_Surfaces.initialize( &m_Native, &m_Resources, getDeviceGeneration() ) ) {
		m_Commands.shutdown( true );
		m_Bindings.shutdown( true );
		m_Pipelines.shutdown( true );
		m_Resources.shutdown( true );
		m_Native.shutdown();
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	// Preallocate fixed resource backing after all driver startup allocations.
	// Native D3D12 resources subsequently come only from these one-block pools.
	if ( pSealedPools != nullptr && !m_Native.getMemory()->configureSealedPools( *pSealedPools ) ) {
		const HRESULT nError = m_Native.getMemory()->getLastError();
		m_Surfaces.shutdown( true );
		m_Commands.shutdown( true );
		m_Bindings.shutdown( true );
		m_Pipelines.shutdown( true );
		m_Resources.shutdown( true );
		m_Native.shutdown();
		return nError == E_INVALIDARG    ? SIR_GRAPHICS_INVALID_ARGUMENT
		       : nError == E_NOTIMPL     ? SIR_GRAPHICS_UNSUPPORTED
		       : nError == E_OUTOFMEMORY ? SIR_GRAPHICS_CAPACITY_EXCEEDED
		                                 : SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bSealedPoolsActive = pSealedPools != nullptr;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12RenderingDevice::queryNativeDiagnostics( sirD3D12DiagnosticSnapshot_t *pSnapshot ) noexcept
{
	return pSnapshot == nullptr                     ? SIR_GRAPHICS_INVALID_ARGUMENT
	       : m_Native.queryDiagnostics( pSnapshot ) ? SIR_GRAPHICS_SUCCESS
	                                                : SIR_GRAPHICS_DEVICE_LOST;
}

sirGraphicsResult_t SIRD3D12RenderingDevice::queryNativeAdapterInfo( sirD3D12AdapterInfo_t *pInfo ) const noexcept
{
	return pInfo == nullptr                     ? SIR_GRAPHICS_INVALID_ARGUMENT
	       : m_Native.queryAdapterInfo( pInfo ) ? SIR_GRAPHICS_SUCCESS
	                                            : SIR_GRAPHICS_DEVICE_LOST;
}

sirGraphicsResult_t SIRD3D12RenderingDevice::queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept
{
	if ( pCapabilities == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCapabilities = {};
	const uint32_t nGeneration = getDeviceGeneration();
	if ( nGeneration == 0 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	pCapabilities->eBackend = getBackend();
	pCapabilities->nApiVersion = { 12, 0, 0 };
	pCapabilities->nFeatureMask = SIR_D3D12_SHARED_FEATURES;
	pCapabilities->nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_COUNT ) - 1U;
	pCapabilities->nMaximumFramesInFlight = 3;
	pCapabilities->nMaximumBindingSlots = 128;
	pCapabilities->nMaximumTextureDimension = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	pCapabilities->nMaximumColorAttachments = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	pCapabilities->nMaximumSampleCount = 1;
	pCapabilities->nDeviceGeneration = nGeneration;
	pCapabilities->bSoftwareAdapter = m_bSoftware;
	bool bUnified = false;
	if ( m_Native.queryUnifiedMemory( &bUnified ) ) {
		pCapabilities->bUnifiedMemory = bUnified;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12RenderingDevice::queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept
{
	if ( pBudget == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pBudget = {};
	if ( getDeviceGeneration() == 0 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D12MemoryInfo_t native = {};
	if ( !m_Native.queryLocalMemory( &native ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirD3D12AllocationLedger_t ledger = {};
	if ( !m_Native.queryAllocationLedger( &ledger ) ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	pBudget->nBudgetBytes = ledger.nResidentLimitBytes != 0 && ledger.nResidentLimitBytes < native.nBudgetBytes ? ledger.nResidentLimitBytes
	                                                                                                            : native.nBudgetBytes;
	pBudget->nResidentBytes = native.nCurrentUsageBytes;
	pBudget->nCommittedBytes = ledger.nCommittedBytes;
	pBudget->nPeakCommittedBytes = ledger.nPeakCommittedBytes;
	sirD3D12ResidentTransferView_t transfer = {};
	if ( m_Native.getTransfers().queryResidentArena( &transfer ) ) {
		pBudget->nUploadMappedBytes = transfer.nPersistentMappedBytes / 2;
		pBudget->nReadbackMappedBytes = transfer.nPersistentMappedBytes / 2;
	}
	pBudget->nUploadMappedBytes += m_Resources.getMappedUploadBytes();
	pBudget->nReadbackMappedBytes += m_Resources.getMappedReadbackBytes();
	return SIR_GRAPHICS_SUCCESS;
}

// General GPU resources and descriptor ownership are provided by the native
// resource table; pipeline/command/presentation stages remain separate.
sirGraphicsResult_t SIRD3D12RenderingDevice::createBuffer( const sirGraphicsBufferDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Resources.createBuffer( desc, pHandle );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::createTexture( const sirGraphicsTextureDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Resources.createTexture( desc, pHandle );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::createView( const sirGraphicsViewDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Resources.createView( desc, pHandle );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::createSampler( const sirGraphicsSamplerDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Resources.createSampler( desc, pHandle );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::createPipeline( const sirGraphicsPipelineDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Pipelines.createPipeline( desc, pHandle );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::createBindingSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex,
                                                               sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Bindings.createSet( hPipeline, nSetIndex, pHandle );
}

sirGraphicsResult_t SIRD3D12RenderingDevice::updateBindings( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
                                                             uint32_t nWriteCount ) noexcept
{
	return m_Bindings.updateSet( hSet, pWrites, nWriteCount );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes,
                                                        sirGraphicsMappedBuffer_t *pMapped ) noexcept
{
	return m_Resources.mapBuffer( hBuffer, nOffset, nBytes, pMapped );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	return m_Resources.unmapBuffer( hBuffer, nOffset, nBytes );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	return m_Resources.flushBuffer( hBuffer, nOffset, nBytes );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	return m_Resources.invalidateBuffer( hBuffer, nOffset, nBytes );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::acquireCommands( sirGraphicsQueue_t eQueue, SIRGraphicsCommandList **ppCommands ) noexcept
{
	return m_Commands.acquire( eQueue, ppCommands );
}

sirGraphicsResult_t SIRD3D12RenderingDevice::submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t &submission,
                                                     sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	const sirGraphicsResult_t eSurface = m_Surfaces.validateSubmission( eQueue, submission );
	if ( eSurface != SIR_GRAPHICS_SUCCESS ) {
		return eSurface;
	}
	const sirGraphicsResult_t eSubmit = m_Commands.submit( eQueue, submission, pCompletion );
	if ( eSubmit == SIR_GRAPHICS_SUCCESS ) {
		m_Surfaces.completeSubmission( submission, *pCompletion );
	}
	return eSubmit;
}

sirGraphicsResult_t SIRD3D12RenderingDevice::poll( const sirGraphicsCompletion_t &completion, bool *pCompleted ) noexcept
{
	if ( pCompleted == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompleted = false;
	if ( completion.nDeviceGeneration == 0 || completion.nDeviceGeneration != getDeviceGeneration() ||
	     completion.eQueue >= SIR_GRAPHICS_QUEUE_COUNT || completion.nValue == 0 ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirD3D12CompletionSet_t snapshot = {};
	if ( !m_Native.snapshotCompletionSet( &snapshot ) ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const uint64_t nLatest = completion.eQueue == SIR_GRAPHICS_QUEUE_DIRECT
	                             ? snapshot.nGraphicsFence
	                             : ( completion.eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ? snapshot.nComputeFence : snapshot.nCopyFence );
	if ( completion.nValue > nLatest ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D12CompletionSet_t native = {};
	native.nDeviceGeneration = snapshot.nDeviceGeneration;
	if ( completion.eQueue == SIR_GRAPHICS_QUEUE_DIRECT ) {
		native.nGraphicsFence = completion.nValue;
	} else if ( completion.eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ) {
		native.nComputeFence = completion.nValue;
	} else {
		native.nCopyFence = completion.nValue;
	}
	return m_Native.pollCompletionSet( native, pCompleted ) ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DEVICE_LOST;
}
sirGraphicsResult_t SIRD3D12RenderingDevice::wait( const sirGraphicsCompletion_t &completion, uint64_t nTimeoutNanoseconds ) noexcept
{
	bool bCompleted = false;
	const sirGraphicsResult_t eResult = poll( completion, &bCompleted );
	if ( eResult != SIR_GRAPHICS_SUCCESS || bCompleted ) {
		return eResult;
	}
	if ( nTimeoutNanoseconds == 0 ) {
		return SIR_GRAPHICS_TIMEOUT;
	}
	const uint64_t nMillis = nTimeoutNanoseconds / 1000000ULL + ( ( nTimeoutNanoseconds % 1000000ULL ) != 0 );
	const uint32_t nBoundedMillis = nMillis > UINT32_MAX ? UINT32_MAX : ( uint32_t )nMillis;
	bool bResult = false;
	if ( completion.eQueue == SIR_GRAPHICS_QUEUE_DIRECT ) {
		bResult = m_Native.waitForGraphicsFence( completion.nValue, nBoundedMillis );
	} else if ( completion.eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ) {
		bResult = m_Native.getComputeQueue().waitForFence( completion.nValue, nBoundedMillis );
	} else {
		bResult = m_Native.getTransfers().waitForFence( completion.nValue, nBoundedMillis );
	}
	if ( bResult ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	const sirGraphicsResult_t eAfter = poll( completion, &bCompleted );
	return eAfter != SIR_GRAPHICS_SUCCESS ? eAfter : ( bCompleted ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_TIMEOUT );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::retireResource( sirGraphicsHandle_t hResource,
                                                             const sirGraphicsCompletionSet_t &completion ) noexcept
{
	m_Commands.collectCompleted();
	m_Bindings.collectRetired();
	if ( hResource.eType == SIR_GRAPHICS_HANDLE_BINDING_SET ) {
		return m_Bindings.retireSet( hResource, completion );
	}
	if ( hResource.eType == SIR_GRAPHICS_HANDLE_SURFACE ) {
		return m_Surfaces.retireSurface( hResource, completion );
	}
	if ( hResource.eType == SIR_GRAPHICS_HANDLE_PIPELINE ) {
		return m_Pipelines.retirePipeline( hResource, completion );
	}
	return m_Resources.retireResource( hResource, completion );
}
sirGraphicsResult_t SIRD3D12RenderingDevice::createSurface( const sirGraphicsNativeSurface_t &native,
                                                            const sirGraphicsPresentDesc_t &present,
                                                            sirGraphicsHandle_t *pSurface ) noexcept
{
	if ( m_bSealedPoolsActive ) {
		if ( pSurface != nullptr ) {
			*pSurface = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return m_Surfaces.createSurface( native, present, pSurface );
}

sirGraphicsResult_t SIRD3D12RenderingDevice::resizeSurface( sirGraphicsHandle_t hSurface, const sirGraphicsNativeSurface_t &native,
                                                            const sirGraphicsPresentDesc_t &present ) noexcept
{
	m_Commands.collectCompleted();
	return m_Surfaces.resizeSurface( hSurface, native, present );
}

sirGraphicsResult_t SIRD3D12RenderingDevice::acquireSurfaceImage( sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept
{
	return m_Surfaces.acquireSurfaceImage( hSurface, pImage );
}

sirGraphicsResult_t SIRD3D12RenderingDevice::present( const sirGraphicsSurfaceImage_t &image,
                                                      const sirGraphicsCompletion_t &renderComplete ) noexcept
{
	const sirGraphicsResult_t eResult = m_Surfaces.present( image, renderComplete );
	if ( eResult == SIR_GRAPHICS_DEVICE_LOST ) {
		m_Native.captureDeviceFailure( DXGI_ERROR_DEVICE_REMOVED );
	}
	return eResult;
}

sirGraphicsResult_t SIRD3D12RenderingDevice::shutdown( void ) noexcept
{
	if ( !m_Native.isReady() ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D12CompletionSet_t snapshot = {};
	bool bComplete = false;
	if ( !m_Native.snapshotCompletionSet( &snapshot ) || !m_Native.pollCompletionSet( snapshot, &bComplete ) ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !bComplete ) {
		return SIR_GRAPHICS_BUSY;
	}
	m_Commands.shutdown( true );
	m_Surfaces.shutdown( true );
	m_Bindings.shutdown( true );
	m_Pipelines.shutdown( true );
	m_Resources.shutdown( true );
	m_Native.shutdown();
	m_bSealedPoolsActive = false;
	return m_Native.isReady() ? SIR_GRAPHICS_DRIVER_ERROR : SIR_GRAPHICS_SUCCESS;
}

sirGraphicsBackend_t SIRD3D12Driver::getBackend( void ) const noexcept
{
	return SIR_GRAPHICS_BACKEND_D3D12;
}
sirGraphicsResult_t SIRD3D12Driver::getAdapterCount( uint32_t *pCount ) const noexcept
{
	if ( pCount == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCount = 0;
	Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory;
	if ( FAILED( CreateDXGIFactory2( 0, IID_PPV_ARGS( pFactory.GetAddressOf() ) ) ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	const HRESULT nResult = sirD3D12EnumerateAdapters( pFactory.Get(), UINT32_MAX, nullptr, pCount );
	return SUCCEEDED( nResult ) ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}
sirGraphicsResult_t SIRD3D12Driver::queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept
{
	return sirD3D12FindAdapter( nAdapterIndex, pInfo );
}
sirGraphicsResult_t SIRD3D12Driver::createDevice( const sirGraphicsDeviceCreateDesc_t &desc, SIRRenderingDevice **ppDevice ) noexcept
{
	return createDeviceInternal( desc, nullptr, ppDevice );
}

sirGraphicsResult_t SIRD3D12Driver::createSealedDevice( const sirGraphicsDeviceCreateDesc_t &desc, const sirD3D12SealedPoolConfig_t &pools,
                                                        SIRRenderingDevice **ppDevice ) noexcept
{
	return createDeviceInternal( desc, &pools, ppDevice );
}

sirGraphicsResult_t SIRD3D12Driver::createDeviceInternal( const sirGraphicsDeviceCreateDesc_t &desc,
                                                          const sirD3D12SealedPoolConfig_t *pSealedPools,
                                                          SIRRenderingDevice **ppDevice ) noexcept
{
	if ( ppDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppDevice = nullptr;
	if ( m_bDeviceActive ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirGraphicsAdapterInfo_t info = {};
	const sirGraphicsResult_t eResult = queryAdapter( desc.nAdapterIndex, &info );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	if ( info.bSoftwareAdapter && !desc.bAllowSoftware ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGraphicsResult_t eInitialize = m_Device.initialize( info.bSoftwareAdapter, desc, pSealedPools );
	if ( eInitialize != SIR_GRAPHICS_SUCCESS ) {
		return eInitialize;
	}
	m_bDeviceActive = true;
	*ppDevice = &m_Device;
	return SIR_GRAPHICS_SUCCESS;
}
sirGraphicsResult_t SIRD3D12Driver::destroyDevice( SIRRenderingDevice *pDevice ) noexcept
{
	if ( !m_bDeviceActive || pDevice != &m_Device ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirGraphicsResult_t eResult = m_Device.shutdown();
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		m_bDeviceActive = false;
	}
	return eResult;
}
