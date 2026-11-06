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

#include <Renderer/Drivers/D3D11/SIRD3D11Device.h>

#include <d3d11.h>

SIRD3D11Device::SIRD3D11Device( const sirD3D11NativeDeviceInit_t& init ) noexcept
    : m_pNativeDevice( init.pNativeDevice ), m_pImmediateContext( init.pImmediateContext ), m_profile( init.profile ),
      m_nResidentBudgetBytes( init.nResidentBudgetBytes ), m_nFramesInFlight( init.nFramesInFlight ),
      m_nDeviceGeneration( init.nDeviceGeneration ), m_bSoftwareAdapter( init.nSoftwareAdapter != 0 )
{
}

SIRD3D11Device::~SIRD3D11Device( void ) noexcept
{
	( void )shutdown();
}

sirGraphicsBackend_t SIRD3D11Device::getBackend( void ) const noexcept
{
	return SIR_GRAPHICS_BACKEND_D3D11;
}

uint32_t SIRD3D11Device::getDeviceGeneration( void ) const noexcept
{
	return m_pNativeDevice != nullptr ? m_nDeviceGeneration : 0U;
}

static uint32_t sirD3D11MaximumColorSamples( void *pNativeDevice ) noexcept
{
	sirD3D11FormatSupport_t support = {};
	support.nSize = sizeof( support );
	if ( sirD3D11QueryFormatSupport( pNativeDevice, SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &support ) != SIR_GRAPHICS_SUCCESS ) {
		return 1;
	}
	const uint32_t nSamples = sirD3D11GetMaxSampleCount( &support );
	return nSamples > 0 ? nSamples : 1;
}

sirGraphicsResult_t SIRD3D11Device::queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept
{
	if ( pCapabilities == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCapabilities = {};
	if ( m_pNativeDevice == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}

	pCapabilities->eBackend = SIR_GRAPHICS_BACKEND_D3D11;
	pCapabilities->nApiVersion = { 11, m_profile.nD3D11_1InterfaceAvailable != 0 ? 1U : 0U, 0 };
	pCapabilities->nFeatureMask = m_profile.nEnabledFeatures;
	pCapabilities->nQueueMask = m_profile.nDirectQueueMask;
	pCapabilities->nPipelineModelMask = m_profile.nPipelineModelMask;
	pCapabilities->nMaximumTextureDimension = m_profile.nMaximumTexture2D;
	pCapabilities->nMaximumColorAttachments = m_profile.nMaximumColorTargets;
	pCapabilities->nMaximumFramesInFlight = m_nFramesInFlight;
	pCapabilities->nMaximumBindingSlots = m_profile.nMaximumShaderResourceSlots;
	pCapabilities->nMaximumSampleCount = sirD3D11MaximumColorSamples( m_pNativeDevice );
	pCapabilities->nDeviceGeneration = m_nDeviceGeneration;
	pCapabilities->bSoftwareAdapter = m_bSoftwareAdapter;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D11Device::queryPipelineModelCaps( sirGraphicsPipelineModel_t eModel,
                                                            sirGraphicsABIPipelineModelCaps_t *pCaps ) const noexcept
{
	if ( pCaps == nullptr || pCaps->nSize < sizeof( *pCaps ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_pNativeDevice == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( eModel != SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = m_nDeviceGeneration;
	caps.nModel = eModel;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nQueueMask = m_profile.nDirectQueueMask;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_DXBC - 1U );
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	if ( ( m_profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER ) != 0 ) {
		caps.nShaderStageMask |= SIR_GRAPHICS_STAGE_GEOMETRY;
	}
	if ( ( m_profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_TESSELLATION ) != 0 ) {
		caps.nShaderStageMask |= SIR_GRAPHICS_STAGE_TESS_CONTROL | SIR_GRAPHICS_STAGE_TESS_EVALUATION;
	}
	if ( ( m_profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_COMPUTE ) != 0 ) {
		caps.nShaderStageMask |= SIR_GRAPHICS_STAGE_COMPUTE;
	}
	caps.nMaximumTextureUnits = m_profile.nMaximumShaderResourceSlots;
	caps.nMaximumBindings = m_profile.nMaximumShaderResourceSlots;
	caps.nMaximumColorAttachments = m_profile.nMaximumColorTargets;
	caps.nMaximumSamples = sirD3D11MaximumColorSamples( m_pNativeDevice );
	caps.nMaximumTextureDimension = m_profile.nMaximumTexture2D;
	caps.nMaximumVertexShaderProfile = m_profile.nShaderProfile;
	caps.nMaximumFragmentShaderProfile = m_profile.nShaderProfile;
	caps.nSupportsNPOT = m_profile.nFeatureLevel >= SIR_D3D11_FEATURE_LEVEL_10_0 ? 1U : 0U;
	*pCaps = caps;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D11Device::queryNativeFormatSupport( sirGraphicsFormat_t eFormat,
                                                              sirD3D11FormatSupport_t *pSupport ) const noexcept
{
	if ( m_pNativeDevice == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	return sirD3D11QueryFormatSupport( m_pNativeDevice, eFormat, pSupport );
}

sirGraphicsResult_t SIRD3D11Device::queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept
{
	if ( pBudget == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pBudget = {};
	if ( m_pNativeDevice == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	pBudget->nBudgetBytes = m_nResidentBudgetBytes;
	return SIR_GRAPHICS_SUCCESS;
}

// D11-02 only owns native device creation and destruction. Resource and
// submission callbacks are deliberately unsupported until D11-04 through 14.
sirGraphicsResult_t SIRD3D11Device::createBuffer( const sirGraphicsBufferDesc_t&, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle != nullptr ) {
		*pHandle = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::createTexture( const sirGraphicsTextureDesc_t&, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle != nullptr ) {
		*pHandle = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::createView( const sirGraphicsViewDesc_t&, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle != nullptr ) {
		*pHandle = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::createSampler( const sirGraphicsSamplerDesc_t&, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle != nullptr ) {
		*pHandle = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::createPipeline( const sirGraphicsPipelineDesc_t&, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle != nullptr ) {
		*pHandle = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::createBindingSet( sirGraphicsHandle_t, uint32_t, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle != nullptr ) {
		*pHandle = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::updateBindings( sirGraphicsHandle_t, const sirGraphicsDescriptorWrite_t *, uint32_t ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::mapBuffer( sirGraphicsHandle_t, uint64_t, uint64_t, sirGraphicsMappedBuffer_t *pMapped ) noexcept
{
	if ( pMapped != nullptr ) {
		*pMapped = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::unmapBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::flushBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::invalidateBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::acquireCommands( sirGraphicsQueue_t, SIRGraphicsCommandList **ppCommands ) noexcept
{
	if ( ppCommands != nullptr ) {
		*ppCommands = nullptr;
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::submit( sirGraphicsQueue_t, const sirGraphicsSubmission_t&,
                                            sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion != nullptr ) {
		*pCompletion = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::poll( const sirGraphicsCompletion_t&, bool *pCompleted ) noexcept
{
	if ( pCompleted != nullptr ) {
		*pCompleted = false;
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::wait( const sirGraphicsCompletion_t&, uint64_t ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::retireResource( sirGraphicsHandle_t, const sirGraphicsCompletionSet_t& ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::createSurface( const sirGraphicsNativeSurface_t&, const sirGraphicsPresentDesc_t&,
                                                   sirGraphicsHandle_t *pSurface ) noexcept
{
	if ( pSurface != nullptr ) {
		*pSurface = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::resizeSurface( sirGraphicsHandle_t, const sirGraphicsNativeSurface_t&,
                                                   const sirGraphicsPresentDesc_t& ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::acquireSurfaceImage( sirGraphicsHandle_t, sirGraphicsSurfaceImage_t *pImage ) noexcept
{
	if ( pImage != nullptr ) {
		*pImage = {};
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}
sirGraphicsResult_t SIRD3D11Device::present( const sirGraphicsSurfaceImage_t&, const sirGraphicsCompletion_t& ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

sirGraphicsResult_t SIRD3D11Device::shutdown( void ) noexcept
{
	if ( m_pImmediateContext != nullptr ) {
		ID3D11DeviceContext *pContext = ( ID3D11DeviceContext * )m_pImmediateContext;
		pContext->ClearState();
		pContext->Flush();
		pContext->Release();
		m_pImmediateContext = nullptr;
	}
	if ( m_pNativeDevice != nullptr ) {
		( ( ID3D11Device * )m_pNativeDevice )->Release();
		m_pNativeDevice = nullptr;
	}
	return SIR_GRAPHICS_SUCCESS;
}
