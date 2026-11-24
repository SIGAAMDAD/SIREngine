/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.

===========================================================================
*/

#include <Renderer/Drivers/D3D12/SIRD3D12Driver.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Validation.h>
#include <Renderer/SIRGraphicsABIOperations.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <limits.h>

static constexpr uint64_t SIR_D3D12_SHARED_FEATURES = SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GRAPHICS | SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COMPUTE |
                                                      SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COPY_QUEUE | SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_SHADER_STORAGE |
                                                      SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_MRT | SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_PRESENT;

static uint32_t sirD3D12MaximumRgba8SampleCount( ID3D12Device *pDevice ) noexcept
{
	if ( pDevice == nullptr ) {
		return 1;
	}
	static constexpr uint32_t SIR_D3D12_SAMPLE_COUNTS[] = { 2, 4, 8, 16, 32 };
	uint32_t nMaximumSamples = 1;
	for ( const uint32_t nSampleCount : SIR_D3D12_SAMPLE_COUNTS ) {
		if ( sirD3D12SupportsSampleCount( pDevice, DXGI_FORMAT_R8G8B8A8_UNORM, nSampleCount ) ) {
			nMaximumSamples = nSampleCount;
		}
	}
	return nMaximumSamples;
}

static bool sirD3D12HasEnhancedBarriers( ID3D12Device *pDevice ) noexcept
{
	if ( pDevice == nullptr ) {
		return false;
	}
	D3D12_FEATURE_DATA_D3D12_OPTIONS12 options {};
	return SUCCEEDED( pDevice->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS12, &options, sizeof( options ) ) ) &&
	       options.EnhancedBarriersSupported != FALSE;
}

static SIRGraphicsResult sirD3D12FindAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) noexcept
{
	if ( pInfo == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pInfo = {};
	Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory;
	if ( FAILED( CreateDXGIFactory2( 0, IID_PPV_ARGS( pFactory.GetAddressOf() ) ) ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pSelected;
	const HRESULT nSelected = sirD3D12EnumerateAdapters( pFactory.Get(), nAdapterIndex, pSelected.GetAddressOf(), nullptr );
	if ( FAILED( nSelected ) ) {
		return nSelected == DXGI_ERROR_NOT_FOUND ? SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	DXGI_ADAPTER_DESC3 desc {};
	if ( FAILED( pSelected->GetDesc3( &desc ) ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	pInfo->nAdapterIndex = nAdapterIndex;
	pInfo->nVendorId = desc.VendorId;
	pInfo->nDeviceId = desc.DeviceId;
	pInfo->nAdapterLuid = ( static_cast<uint64_t>static_cast<uint32_t>( desc.AdapterLuid.HighPart ) << 32 ) | desc.AdapterLuid.LowPart;
	pInfo->nDedicatedVideoBytes = desc.DedicatedVideoMemory;
	pInfo->nSharedSystemBytes = desc.SharedSystemMemory;
	pInfo->nFeatureMask = SIR_D3D12_SHARED_FEATURES;
	Microsoft::WRL::ComPtr<ID3D12Device> pProbe;
	if ( SUCCEEDED( D3D12CreateDevice( pSelected.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( pProbe.GetAddressOf() ) ) ) ) {
		pInfo->nFeatureMask |= sirD3D12NegotiatedOptionalFeatures( sirD3D12HasEnhancedBarriers( pProbe.Get() ),
		                                                           sirD3D12MaximumRgba8SampleCount( pProbe.Get() ) > 1 );
	}
	pInfo->nApiVersion = { 12, 0, 0 };
	pInfo->bSoftwareAdapter = ( desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE ) != 0;
	if ( WideCharToMultiByte( CP_UTF8, 0, desc.Description, -1, pInfo->szName, static_cast<int>(sizeof( pInfo->szName ), nullptr, nullptr ) == 0 )) {
		pInfo->szName[0] = '\0';
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsBackend SIRD3D12RenderingDevice::getBackend( void ) const noexcept
{
	return SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_D3D12;
}

uint32_t SIRD3D12RenderingDevice::getDeviceGeneration( void ) const noexcept
{
	sirD3D12CompletionSet_t native {};
	if ( !m_Native.snapshotCompletionSet( &native ) || native.nDeviceGeneration > UINT32_MAX ) {
		return 0;
	}
	return static_cast<uint32_t>(native.nDeviceGeneration);
}

SIRGraphicsResult SIRD3D12RenderingDevice::initialize( bool bSoftware, const sirGraphicsDeviceCreateDesc_t& desc,
                                                         const sirD3D12NativeDeviceConfig_t& nativeConfig,
                                                         const sirD3D12SealedPoolConfig_t *pSealedPools ) noexcept
{
	if ( desc.nFramesInFlight == 0 || desc.nFramesInFlight > 3 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGraphicsHasFeatures( SIR_D3D12_SHARED_FEATURES | SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_MULTISAMPLE | SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_ENHANCED_BARRIERS,
	                              desc.nRequiredFeatures ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( nativeConfig.diagnostics.bEnableGpuValidation && !nativeConfig.diagnostics.bEnableDebugLayer ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint64_t nSlotBytes = desc.nUploadBudgetBytes == 0 ? 64 * 1024 : desc.nUploadBudgetBytes / 3;
	if ( nSlotBytes == 0 || nSlotBytes > UINT64_MAX / 3 ||
	     ( desc.nReadbackBudgetBytes != 0 && desc.nReadbackBudgetBytes / 3 < nSlotBytes ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirD3D12TransferPoolConfig_t config {};
	config.nSlotBytes = nSlotBytes;
	config.bPreferDirectMapped = true;
	if ( !m_Native.initialize( desc.nAdapterIndex, config, nativeConfig.pCpuHeap, nativeConfig.diagnostics ) ) {
		return m_Native.getLastError() == E_NOTIMPL ? SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bSoftware = bSoftware;
	// Optional features must be admitted against this native device, not the
	// fixed baseline mask; the advertised maximum and admission must agree.
	sirGraphicsCapabilities_t nativeCaps {};
	const SIRGraphicsResult eCaps = queryCapabilities( &nativeCaps );
	if ( eCaps != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS || !sirGraphicsHasFeatures( nativeCaps.nFeatureMask, desc.nRequiredFeatures ) ) {
		m_Native.shutdown();
		return eCaps != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ? eCaps : SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !m_Native.getMemory()->configureResidentBudget( desc.nResidentBudgetBytes ) ) {
		m_Native.shutdown();
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( getDeviceGeneration() == 0 ) {
		m_Native.shutdown();
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( !m_Resources.initialize( &m_Native, getDeviceGeneration(), desc.nResidentBudgetBytes ) ) {
		m_Native.shutdown();
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( !m_Pipelines.initialize( &m_Native, getDeviceGeneration() ) ) {
		m_Resources.shutdown( true );
		m_Native.shutdown();
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( !m_Bindings.initialize( &m_Native, &m_Resources, &m_Pipelines, getDeviceGeneration() ) ) {
		m_Pipelines.shutdown( true );
		m_Resources.shutdown( true );
		m_Native.shutdown();
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( !m_Commands.initialize( &m_Native, &m_Resources, &m_Pipelines, &m_Bindings ) ) {
		m_Bindings.shutdown( true );
		m_Pipelines.shutdown( true );
		m_Resources.shutdown( true );
		m_Native.shutdown();
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( !m_Surfaces.initialize( &m_Native, &m_Resources, getDeviceGeneration() ) ) {
		m_Commands.shutdown( true );
		m_Bindings.shutdown( true );
		m_Pipelines.shutdown( true );
		m_Resources.shutdown( true );
		m_Native.shutdown();
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
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
		return nError == E_INVALIDARG    ? SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT
		       : nError == E_NOTIMPL     ? SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED
		       : nError == E_OUTOFMEMORY ? SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED
		                                 : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bSealedPoolsActive = pSealedPools != nullptr;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12RenderingDevice::loadNativePipelineCache( const void *pData, uint64_t nBytes ) noexcept
{
	return m_Pipelines.loadPipelineCache( pData, nBytes );
}

SIRGraphicsResult SIRD3D12RenderingDevice::serializeNativePipelineCache( void *pData, uint64_t *pBytes ) const noexcept
{
	return m_Pipelines.serializePipelineCache( pData, pBytes );
}

SIRGraphicsResult SIRD3D12RenderingDevice::queryNativePipelineCacheStats( sirD3D12PipelineCacheStats_t *pStats ) const noexcept
{
	return m_Pipelines.queryPipelineCacheStats( pStats );
}

SIRGraphicsResult SIRD3D12RenderingDevice::queryNativeDiagnostics( sirD3D12DiagnosticSnapshot_t *pSnapshot ) noexcept
{
	return pSnapshot == nullptr                     ? SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT
	       : m_Native.queryDiagnostics( pSnapshot ) ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS
	                                                : SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
}

SIRGraphicsResult SIRD3D12RenderingDevice::queryNativeAdapterInfo( sirD3D12AdapterInfo_t *pInfo ) const noexcept
{
	return pInfo == nullptr                     ? SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT
	       : m_Native.queryAdapterInfo( pInfo ) ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS
	                                            : SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
}

SIRGraphicsResult SIRD3D12RenderingDevice::queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept
{
	if ( pCapabilities == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCapabilities = {};
	const uint32_t nGeneration = getDeviceGeneration();
	if ( nGeneration == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	pCapabilities->eBackend = getBackend();
	pCapabilities->nApiVersion = { 12, 0, 0 };
	pCapabilities->nFeatureMask = SIR_D3D12_SHARED_FEATURES;
	pCapabilities->nPipelineModelMask = 1U << ( SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN - 1 );
	pCapabilities->nQueueMask = ( 1U << SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT ) - 1U;
	pCapabilities->nMaximumFramesInFlight = 3;
	pCapabilities->nMaximumBindingSlots = 128;
	pCapabilities->nMaximumTextureDimension = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	pCapabilities->nMaximumColorAttachments = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	const uint32_t nMaximumSamples = sirD3D12MaximumRgba8SampleCount( m_Native.getNativeDevice() );
	pCapabilities->nMaximumSampleCount = nMaximumSamples;
	pCapabilities->nFeatureMask |=
	    sirD3D12NegotiatedOptionalFeatures( sirD3D12HasEnhancedBarriers( m_Native.getNativeDevice() ), nMaximumSamples > 1 );
	pCapabilities->nDeviceGeneration = nGeneration;
	pCapabilities->bSoftwareAdapter = m_bSoftware;
	bool bUnified = false;
	if ( m_Native.queryUnifiedMemory( &bUnified ) ) {
		pCapabilities->bUnifiedMemory = bUnified;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12RenderingDevice::queryPipelineModelCaps( SIRGraphicsPipelineModel eModel,
                                                                     sirGraphicsABIPipelineModelCaps_t *pCaps ) const noexcept
{
	if ( pCaps == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( eModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsCapabilities_t available {};
	const SIRGraphicsResult eResult = queryCapabilities( &available );
	if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}

	sirGraphicsABIPipelineModelCaps_t caps {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = available.nDeviceGeneration;
	caps.nModel = SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	caps.nExecutionModel = SIRGraphicsExecutionModel::SIR_GRAPHICS_EXECUTION_EXPLICIT;
	caps.nShaderStageMask = SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_FRAGMENT | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE;
	caps.nQueueMask = available.nQueueMask;
	caps.nRequiredFeatureMask = SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GRAPHICS | SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COMPUTE;
	caps.nShaderFormatMask = 1U << ( SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL - 1 );
	caps.nMaximumTextureUnits = available.nMaximumBindingSlots;
	caps.nMaximumVertexStreams = 32;
	caps.nMaximumVertexAttributes = 32;
	caps.nMaximumVaryings = 16;
	caps.nMaximumConstantSlots = 14;
	caps.nMaximumBindings = available.nMaximumBindingSlots;
	caps.nMaximumColorAttachments = available.nMaximumColorAttachments;
	caps.nMaximumSamples = available.nMaximumSampleCount;
	caps.nMaximumTextureDimension = available.nMaximumTextureDimension;
	caps.nSupportsNPOT = 1;
	*pCaps = caps;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12RenderingDevice::queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept
{
	if ( pBudget == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pBudget = {};
	if ( getDeviceGeneration() == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D12MemoryInfo_t native {};
	if ( !m_Native.queryLocalMemory( &native ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	sirD3D12AllocationLedger_t ledger {};
	if ( !m_Native.queryAllocationLedger( &ledger ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	pBudget->nBudgetBytes = ledger.nResidentLimitBytes != 0 && ledger.nResidentLimitBytes < native.nBudgetBytes ? ledger.nResidentLimitBytes
	                                                                                                            : native.nBudgetBytes;
	pBudget->nResidentBytes = native.nCurrentUsageBytes;
	pBudget->nCommittedBytes = ledger.nCommittedBytes;
	pBudget->nPeakCommittedBytes = ledger.nPeakCommittedBytes;
	sirD3D12ResidentTransferView_t transfer {};
	if ( m_Native.getTransfers().queryResidentArena( &transfer ) ) {
		pBudget->nUploadMappedBytes = transfer.nPersistentMappedBytes / 2;
		pBudget->nReadbackMappedBytes = transfer.nPersistentMappedBytes / 2;
	}
	pBudget->nUploadMappedBytes += m_Resources.getMappedUploadBytes();
	pBudget->nReadbackMappedBytes += m_Resources.getMappedReadbackBytes();
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

// General GPU resources and descriptor ownership are provided by the native
// resource table; pipeline/command/presentation stages remain separate.
SIRGraphicsResult SIRD3D12RenderingDevice::createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Resources.createBuffer( desc, pHandle );
}
SIRGraphicsResult SIRD3D12RenderingDevice::createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Resources.createTexture( desc, pHandle );
}
SIRGraphicsResult SIRD3D12RenderingDevice::createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Resources.createView( desc, pHandle );
}
SIRGraphicsResult SIRD3D12RenderingDevice::createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Resources.createSampler( desc, pHandle );
}
SIRGraphicsResult SIRD3D12RenderingDevice::createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Pipelines.createPipeline( desc, pHandle );
}
SIRGraphicsResult SIRD3D12RenderingDevice::createBindingSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex,
                                                               sirGraphicsHandle_t *pHandle ) noexcept
{
	return m_Bindings.createSet( hPipeline, nSetIndex, pHandle );
}

SIRGraphicsResult SIRD3D12RenderingDevice::updateBindings( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
                                                             uint32_t nWriteCount ) noexcept
{
	return m_Bindings.updateSet( hSet, pWrites, nWriteCount );
}
SIRGraphicsResult SIRD3D12RenderingDevice::mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes,
                                                        sirGraphicsMappedBuffer_t *pMapped ) noexcept
{
	return m_Resources.mapBuffer( hBuffer, nOffset, nBytes, pMapped );
}
SIRGraphicsResult SIRD3D12RenderingDevice::unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	return m_Resources.unmapBuffer( hBuffer, nOffset, nBytes );
}
SIRGraphicsResult SIRD3D12RenderingDevice::flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	return m_Resources.flushBuffer( hBuffer, nOffset, nBytes );
}
SIRGraphicsResult SIRD3D12RenderingDevice::invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	return m_Resources.invalidateBuffer( hBuffer, nOffset, nBytes );
}
SIRGraphicsResult SIRD3D12RenderingDevice::acquireCommands( SIRGraphicsQueue eQueue, SIRGraphicsCommandList **ppCommands ) noexcept
{
	return m_Commands.acquire( eQueue, ppCommands );
}

SIRGraphicsResult SIRD3D12RenderingDevice::submit( SIRGraphicsQueue eQueue, const sirGraphicsSubmission_t& submission,
                                                     sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	const SIRGraphicsResult eSurface = m_Surfaces.validateSubmission( eQueue, submission );
	if ( eSurface != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eSurface;
	}
	const SIRGraphicsResult eSubmit = m_Commands.submit( eQueue, submission, pCompletion );
	if ( eSubmit == SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		m_Surfaces.completeSubmission( submission, *pCompletion );
	}
	return eSubmit;
}

SIRGraphicsResult SIRD3D12RenderingDevice::poll( const sirGraphicsCompletion_t& completion, bool *pCompleted ) noexcept
{
	if ( pCompleted == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompleted = false;
	if ( completion.nDeviceGeneration == 0 || completion.nDeviceGeneration != getDeviceGeneration() ||
	     completion.eQueue >= SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT || completion.nValue == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirD3D12CompletionSet_t snapshot {};
	if ( !m_Native.snapshotCompletionSet( &snapshot ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	const uint64_t nLatest = completion.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT
	                             ? snapshot.nGraphicsFence
	                             : ( completion.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE ? snapshot.nComputeFence : snapshot.nCopyFence );
	if ( completion.nValue > nLatest ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D12CompletionSet_t native {};
	native.nDeviceGeneration = snapshot.nDeviceGeneration;
	if ( completion.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		native.nGraphicsFence = completion.nValue;
	} else if ( completion.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE ) {
		native.nComputeFence = completion.nValue;
	} else {
		native.nCopyFence = completion.nValue;
	}
	return m_Native.pollCompletionSet( native, pCompleted ) ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
}
SIRGraphicsResult SIRD3D12RenderingDevice::wait( const sirGraphicsCompletion_t& completion, uint64_t nTimeoutNanoseconds ) noexcept
{
	bool bCompleted = false;
	const SIRGraphicsResult eResult = poll( completion, &bCompleted );
	if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS || bCompleted ) {
		return eResult;
	}
	if ( nTimeoutNanoseconds == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_TIMEOUT;
	}
	const uint64_t nMillis = nTimeoutNanoseconds / 1000000ULL + ( ( nTimeoutNanoseconds % 1000000ULL ) != 0 );
	const uint32_t nBoundedMillis = nMillis > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(nMillis);
	bool bResult = false;
	if ( completion.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		bResult = m_Native.waitForGraphicsFence( completion.nValue, nBoundedMillis );
	} else if ( completion.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE ) {
		bResult = m_Native.getComputeQueue().waitForFence( completion.nValue, nBoundedMillis );
	} else {
		bResult = m_Native.getTransfers().waitForFence( completion.nValue, nBoundedMillis );
	}
	if ( bResult ) {
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	const SIRGraphicsResult eAfter = poll( completion, &bCompleted );
	return eAfter != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ? eAfter : ( bCompleted ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : SIRGraphicsResult::SIR_GRAPHICS_TIMEOUT );
}
SIRGraphicsResult SIRD3D12RenderingDevice::retireResource( sirGraphicsHandle_t hResource,
                                                             const sirGraphicsCompletionSet_t& completion ) noexcept
{
	m_Commands.collectCompleted();
	m_Bindings.collectRetired();
	if ( hResource.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BINDING_SET ) {
		return m_Bindings.retireSet( hResource, completion );
	}
	if ( hResource.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_SURFACE ) {
		return m_Surfaces.retireSurface( hResource, completion );
	}
	if ( hResource.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE ) {
		return m_Pipelines.retirePipeline( hResource, completion );
	}
	return m_Resources.retireResource( hResource, completion );
}
SIRGraphicsResult SIRD3D12RenderingDevice::createSurface( const sirGraphicsNativeSurface_t& native,
                                                            const sirGraphicsPresentDesc_t& present,
                                                            sirGraphicsHandle_t *pSurface ) noexcept
{
	return m_Surfaces.createSurface( native, present, pSurface );
}

SIRGraphicsResult SIRD3D12RenderingDevice::resizeSurface( sirGraphicsHandle_t hSurface, const sirGraphicsNativeSurface_t& native,
                                                            const sirGraphicsPresentDesc_t& present ) noexcept
{
	m_Commands.collectCompleted();
	return m_Surfaces.resizeSurface( hSurface, native, present );
}

SIRGraphicsResult SIRD3D12RenderingDevice::acquireSurfaceImage( sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept
{
	return m_Surfaces.acquireSurfaceImage( hSurface, pImage );
}

SIRGraphicsResult SIRD3D12RenderingDevice::present( const sirGraphicsSurfaceImage_t& image,
                                                      const sirGraphicsCompletion_t& renderComplete ) noexcept
{
	const SIRGraphicsResult eResult = m_Surfaces.present( image, renderComplete );
	if ( eResult == SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST ) {
		m_Native.captureDeviceFailure( DXGI_ERROR_DEVICE_REMOVED );
	}
	return eResult;
}

SIRGraphicsResult SIRD3D12RenderingDevice::shutdown( void ) noexcept
{
	if ( !m_Native.isReady() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D12CompletionSet_t snapshot {};
	bool bComplete = false;
	if ( !m_Native.snapshotCompletionSet( &snapshot ) || !m_Native.pollCompletionSet( snapshot, &bComplete ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !bComplete ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	m_Commands.shutdown( true );
	m_Surfaces.shutdown( true );
	m_Bindings.shutdown( true );
	m_Pipelines.shutdown( true );
	m_Resources.shutdown( true );
	m_Native.shutdown();
	m_bSealedPoolsActive = false;
	return m_Native.isReady() ? SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR : SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsBackend SIRD3D12Driver::getBackend( void ) const noexcept
{
	return SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_D3D12;
}
SIRGraphicsResult SIRD3D12Driver::getAdapterCount( uint32_t *pCount ) const noexcept
{
	if ( pCount == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCount = 0;
	Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory;
	if ( FAILED( CreateDXGIFactory2( 0, IID_PPV_ARGS( pFactory.GetAddressOf() ) ) ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	const HRESULT nResult = sirD3D12EnumerateAdapters( pFactory.Get(), UINT32_MAX, nullptr, pCount );
	return SUCCEEDED( nResult ) ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
}
SIRGraphicsResult SIRD3D12Driver::queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept
{
	return sirD3D12FindAdapter( nAdapterIndex, pInfo );
}
SIRGraphicsResult SIRD3D12Driver::createDevice( const sirGraphicsDeviceCreateDesc_t& desc, SIRRenderingDevice **ppDevice ) noexcept
{
	const sirD3D12NativeDeviceConfig_t nativeConfig {};
	return createDeviceInternal( desc, nativeConfig, nullptr, ppDevice );
}

SIRGraphicsResult SIRD3D12Driver::createSealedDevice( const sirGraphicsDeviceCreateDesc_t& desc, const sirD3D12SealedPoolConfig_t& pools,
                                                        SIRRenderingDevice **ppDevice ) noexcept
{
	const sirD3D12NativeDeviceConfig_t nativeConfig {};
	return createDeviceInternal( desc, nativeConfig, &pools, ppDevice );
}

SIRGraphicsResult SIRD3D12Driver::createDeviceWithConfig( const sirGraphicsDeviceCreateDesc_t& desc,
                                                            const sirD3D12NativeDeviceConfig_t& nativeConfig,
                                                            SIRRenderingDevice **ppDevice ) noexcept
{
	return createDeviceInternal( desc, nativeConfig, nullptr, ppDevice );
}

SIRGraphicsResult SIRD3D12Driver::createSealedDeviceWithConfig( const sirGraphicsDeviceCreateDesc_t& desc,
                                                                  const sirD3D12SealedPoolConfig_t& pools,
                                                                  const sirD3D12NativeDeviceConfig_t& nativeConfig,
                                                                  SIRRenderingDevice **ppDevice ) noexcept
{
	return createDeviceInternal( desc, nativeConfig, &pools, ppDevice );
}

SIRGraphicsResult SIRD3D12Driver::createDeviceInternal( const sirGraphicsDeviceCreateDesc_t& desc,
                                                          const sirD3D12NativeDeviceConfig_t& nativeConfig,
                                                          const sirD3D12SealedPoolConfig_t *pSealedPools,
                                                          SIRRenderingDevice **ppDevice ) noexcept
{
	if ( ppDevice == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppDevice = nullptr;
	if ( m_bDeviceActive ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	sirGraphicsAdapterInfo_t info {};
	const SIRGraphicsResult eResult = queryAdapter( desc.nAdapterIndex, &info );
	if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	if ( info.bSoftwareAdapter && !desc.bAllowSoftware ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const SIRGraphicsResult eInitialize = m_Device.initialize( info.bSoftwareAdapter, desc, nativeConfig, pSealedPools );
	if ( eInitialize != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eInitialize;
	}
	m_bDeviceActive = true;
	*ppDevice = &m_Device;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
SIRGraphicsResult SIRD3D12Driver::destroyDevice( SIRRenderingDevice *pDevice ) noexcept
{
	if ( !m_bDeviceActive || pDevice != &m_Device ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const SIRGraphicsResult eResult = m_Device.shutdown();
	if ( eResult == SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		m_bDeviceActive = false;
	}
	return eResult;
}
