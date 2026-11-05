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

#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <string.h>

#if defined( _WIN32 )
#define SIR_TEST_EXPORT __declspec( dllexport )
#else
#define SIR_TEST_EXPORT __attribute__( ( visibility( "default" ) ) )
#endif

static uint32_t s_nMockMode = 0;
static uint32_t s_nLiveDevices = 0;
static uint64_t s_nSubmitted = 0;
static uint32_t s_nDriverIdentity = 17;
static uint32_t s_nDeviceIdentity = 23;

static bool mockHasGraphicsModels( void ) noexcept
{
	return s_nMockMode == 10 || ( s_nMockMode >= 14 && s_nMockMode <= 18 ) || ( s_nMockMode >= 26 && s_nMockMode <= 29 ) ||
	       ( s_nMockMode >= 50 && s_nMockMode <= 54 ) || s_nMockMode == 56 || s_nMockMode == 60 || s_nMockMode == 61;
}

extern "C" SIR_TEST_EXPORT void SIRSetGraphicsABIMockMode( uint32_t nMode ) noexcept
{
	s_nMockMode = nMode;
}

static uint32_t mockAdapterCount( void *, uint32_t *pCount ) noexcept
{
	if ( s_nMockMode == 42 ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pCount == nullptr ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	*pCount = s_nMockMode == 9 ? 257 : ( s_nMockMode == 12 ? 3 : 1 );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockQueryAdapter( void *, sirGraphicsABIAdapter_t *pInfo ) noexcept
{
	if ( s_nMockMode == 43 ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pInfo == nullptr || pInfo->nSize < sizeof( sirGraphicsABIAdapter_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( ( s_nMockMode == 12 && pInfo->nIndex >= 3 ) || ( s_nMockMode != 12 && pInfo->nIndex != 0 ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const uint32_t nIndex = pInfo->nIndex;
	*pInfo = {};
	pInfo->nSize = sizeof( *pInfo );
	pInfo->nIndex = nIndex;
	pInfo->nBackend = s_nMockMode == 1 ? SIR_GRAPHICS_BACKEND_D3D12 : SIR_GRAPHICS_BACKEND_VULKAN;
	pInfo->nKind = SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	pInfo->nApiMajor = 1;
	pInfo->nApiMinor = 4;
	pInfo->nStableId = 0x1234;
	pInfo->nFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE;
	if ( s_nMockMode == 12 ) {
		pInfo->nKind = nIndex == 0 ? SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE
		                           : ( nIndex == 1 ? SIR_GRAPHICS_ABI_ADAPTER_INTEGRATED : SIR_GRAPHICS_ABI_ADAPTER_DISCRETE );
		pInfo->nStableId = 0x1234 + nIndex;
		pInfo->nDedicatedBytes = nIndex == 2 ? 4096 : 0;
		pInfo->nSharedBytes = nIndex == 1 ? 8192 : 0;
		if ( nIndex == 0 ) {
			pInfo->nFeatureMask |= SIR_GRAPHICS_FEATURE_GRAPHICS;
		}
	}
	if ( mockHasGraphicsModels() ) {
		pInfo->nFeatureMask |= SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	}
	if ( s_nMockMode == 46 ) {
		pInfo->nFeatureMask |= SIR_GRAPHICS_FEATURE_GRAPHICS;
	}
	if ( s_nMockMode == 13 ) {
		pInfo->nFeatureMask |= 1ULL << 63;
	}
	pInfo->nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	memcpy( pInfo->szName, "Mock Vulkan CPU", sizeof( "Mock Vulkan CPU" ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockQueryCapabilities( void *, sirGraphicsABICapabilities_t *pInfo ) noexcept
{
	if ( s_nMockMode == 44 ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( s_nMockMode == 8 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( pInfo == nullptr || pInfo->nSize < sizeof( sirGraphicsABICapabilities_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	*pInfo = {};
	pInfo->nSize = sizeof( *pInfo );
	pInfo->nDeviceGeneration = 5;
	pInfo->nBackend = s_nMockMode == 3 ? SIR_GRAPHICS_BACKEND_D3D12 : SIR_GRAPHICS_BACKEND_VULKAN;
	pInfo->nAdapterKind = SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	pInfo->nEnabledFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	pInfo->nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_MODERN - 1 );
	if ( mockHasGraphicsModels() ) {
		pInfo->nEnabledFeatures |= SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
		pInfo->nPipelineModelMask = ( 1U << SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) - 1U;
		pInfo->nMaximumColorAttachments = 1;
	}
	if ( ( s_nMockMode >= 26 && s_nMockMode <= 29 ) || s_nMockMode == 60 ) {
		pInfo->nEnabledFeatures |= SIR_GRAPHICS_FEATURE_PRESENT;
	}
	pInfo->nEnabledQueues = s_nMockMode == 45 ? 0U : 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	if ( s_nMockMode == 54 ) {
		pInfo->nEnabledFeatures |= SIR_GRAPHICS_FEATURE_SHADER_STORAGE;
	}
	pInfo->nMaximumFramesInFlight = 2;
	pInfo->nMaximumSamples = s_nMockMode == 15 ? 2 : 1;
	pInfo->nMaximumTextureDimension = 4096;
	pInfo->nMaximumBindings = 16;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

// A synthetic provider exercises model discovery across the DSO boundary.
// Mode 10 advertises all three models for conformance, not native GPU support.
static uint32_t mockQueryPipelineModelCaps( void *, uint32_t nModel, sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept
{
	if ( s_nMockMode == 52 ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pCaps == nullptr || pCaps->nSize < sizeof( sirGraphicsABIPipelineModelCaps_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nModel != SIR_GRAPHICS_PIPELINE_MODEL_MODERN && !mockHasGraphicsModels() && s_nMockMode != 11 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( nModel < SIR_GRAPHICS_PIPELINE_MODEL_MODERN || nModel > SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	*pCaps = {};
	pCaps->nSize = sizeof( *pCaps );
	pCaps->nModel = nModel;
	pCaps->nDeviceGeneration = s_nMockMode == 11 ? 4 : 5;
	pCaps->nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	pCaps->nMaximumSamples = s_nMockMode == 15 ? 2 : 1;
	pCaps->nMaximumTextureDimension = 4096;
	pCaps->nMaximumBindings = 16;
	pCaps->nMaximumVertexStreams = 4;
	pCaps->nMaximumVertexAttributes = 8;
	pCaps->nMaximumTextureUnits = 8;
	pCaps->nMaximumColorAttachments = mockHasGraphicsModels() ? 1 : 0;

	if ( nModel == SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		pCaps->nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
		pCaps->nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
		pCaps->nMaximumTextureStages = 4;
		pCaps->nMaximumFixedLights = 4;
		pCaps->nFixedFeatureMask = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING | SIR_GRAPHICS_FIXED_FOG |
		                           SIR_GRAPHICS_FIXED_ALPHA_TEST | SIR_GRAPHICS_FIXED_CPU_TRANSFORM;
		pCaps->nFixedTextureOperationMask =
		    ( 1U << SIR_GRAPHICS_FIXED_OP_DISABLE ) | ( 1U << SIR_GRAPHICS_FIXED_OP_SELECT ) | ( 1U << SIR_GRAPHICS_FIXED_OP_MODULATE );
		pCaps->nFixedFogModeMask = ( 1U << SIR_GRAPHICS_FIXED_FOG_NONE ) | ( 1U << SIR_GRAPHICS_FIXED_FOG_LINEAR );
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( nModel == SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		pCaps->nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
		pCaps->nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
		pCaps->nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
		pCaps->nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_GLSL - 1 );
		pCaps->nMaximumVaryings = 8;
		pCaps->nMaximumConstantSlots = 32;
		pCaps->nMaximumVertexShaderProfile = 101;
		pCaps->nMaximumFragmentShaderProfile = 101;
		return SIR_GRAPHICS_SUCCESS;
	}
	pCaps->nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	pCaps->nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE;
	pCaps->nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	pCaps->nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
	if ( mockHasGraphicsModels() ) {
		pCaps->nRequiredFeatureMask |= SIR_GRAPHICS_FEATURE_GRAPHICS;
		pCaps->nShaderStageMask |= SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockQueryFormat( void *, sirGraphicsABIFormat_t *pInfo ) noexcept
{
	if ( s_nMockMode == 51 || s_nMockMode == 60 ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( s_nMockMode == 53 && pInfo != nullptr && pInfo->nFormat == SIR_GRAPHICS_FORMAT_D16_UNORM ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pInfo == nullptr || pInfo->nSize < sizeof( sirGraphicsABIFormat_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( pInfo->nFormat != SIR_GRAPHICS_FORMAT_RGBA8_UNORM &&
	     ( pInfo->nFormat != SIR_GRAPHICS_FORMAT_D16_UNORM || !mockHasGraphicsModels() ) ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	const uint32_t nRequested = pInfo->nFormat;
	*pInfo = {};
	pInfo->nSize = sizeof( *pInfo );
	pInfo->nFormat = ( s_nMockMode == 2 || s_nMockMode == 16 ) ? SIR_GRAPHICS_FORMAT_BGRA8_UNORM : nRequested;
	pInfo->nOptimalFeatures = SIR_GRAPHICS_ABI_FORMAT_SAMPLED | SIR_GRAPHICS_ABI_FORMAT_TRANSFER_DST;
	if ( s_nMockMode == 54 ) {
		pInfo->nOptimalFeatures |= SIR_GRAPHICS_ABI_FORMAT_STORAGE;
	}
	if ( mockHasGraphicsModels() ) {
		if ( nRequested == SIR_GRAPHICS_FORMAT_RGBA8_UNORM && s_nMockMode != 14 ) {
			pInfo->nOptimalFeatures |= SIR_GRAPHICS_ABI_FORMAT_COLOR_ATTACHMENT;
		}
		if ( nRequested == SIR_GRAPHICS_FORMAT_D16_UNORM && s_nMockMode != 61 ) {
			pInfo->nOptimalFeatures |= SIR_GRAPHICS_ABI_FORMAT_DEPTH_STENCIL;
		}
	}
	pInfo->nSampleCountMask = s_nMockMode == 15 ? 2 : 1;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockPoll( void *, const sirGraphicsCompletion_t *pCompletion, uint32_t *pDone ) noexcept
{
	if ( pCompletion == nullptr || pDone == nullptr || pCompletion->nDeviceGeneration != 5 ||
	     pCompletion->eQueue != SIR_GRAPHICS_QUEUE_DIRECT || pCompletion->nValue == 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( s_nMockMode == 25 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pDone = s_nMockMode == 24 ? 2U : ( pCompletion->nValue <= s_nSubmitted ? 1U : 0U );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockWait( void *, const sirGraphicsCompletion_t *pCompletion, uint64_t ) noexcept
{
	if ( s_nMockMode == 25 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	uint32_t nComplete = 0;
	const uint32_t nResult = mockPoll( nullptr, pCompletion, &nComplete );
	if ( nResult != SIR_GRAPHICS_ABI_SUCCESS ) {
		return nResult;
	}
	return nComplete != 0 ? SIR_GRAPHICS_ABI_SUCCESS : SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockShutdown( void * ) noexcept
{
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockCreateBuffer( void *, const sirGraphicsBufferDesc_t *pDesc, sirGraphicsHandle_t *pBuffer ) noexcept
{
	if ( pDesc == nullptr || pBuffer == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_nMockMode != 10 && s_nMockMode != 17 && s_nMockMode != 18 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( s_nMockMode == 18 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pBuffer = { 5, 1, s_nMockMode == 17 ? 4U : 5U, SIR_GRAPHICS_HANDLE_BUFFER };
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockCreateTexture( void *, const sirGraphicsTextureDesc_t *pDesc, sirGraphicsHandle_t *pTexture ) noexcept
{
	if ( pDesc == nullptr || pTexture == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !mockHasGraphicsModels() ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( s_nMockMode == 18 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pTexture = { 4, 1, s_nMockMode == 17 ? 4U : 5U, SIR_GRAPHICS_HANDLE_TEXTURE };
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockCreateView( void *, const sirGraphicsViewDesc_t *pDesc, sirGraphicsHandle_t *pView ) noexcept
{
	if ( pDesc == nullptr || pView == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_nMockMode != 10 && s_nMockMode != 17 && s_nMockMode != 18 && s_nMockMode != 54 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( s_nMockMode == 18 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pView = { 6, 1, s_nMockMode == 17 ? 4U : 5U, SIR_GRAPHICS_HANDLE_VIEW };
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockCreateSampler( void *, const sirGraphicsSamplerDesc_t *pDesc, sirGraphicsHandle_t *pSampler ) noexcept
{
	if ( pDesc == nullptr || pSampler == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_nMockMode != 10 && s_nMockMode != 17 && s_nMockMode != 18 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( s_nMockMode == 18 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pSampler = { 7, 1, s_nMockMode == 17 ? 4U : 5U, SIR_GRAPHICS_HANDLE_SAMPLER };
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockCreatePipeline( void *, const sirGraphicsABIPipeline_t *pPipeline, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( s_nMockMode == 50 ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pPipeline == nullptr || pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	const uint32_t nQuery = mockQueryPipelineModelCaps( nullptr, pPipeline->nModel, &caps );
	if ( nQuery != SIR_GRAPHICS_SUCCESS ) {
		return nQuery;
	}
	if ( pPipeline->nModel != caps.nModel || pPipeline->nSize < sizeof( sirGraphicsABIPipeline_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = { 3, 1, 5, SIR_GRAPHICS_HANDLE_PIPELINE };
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockCreateBindingSet( void *, sirGraphicsHandle_t, uint32_t, sirGraphicsHandle_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockUpdateBindings( void *, sirGraphicsHandle_t, const sirGraphicsDescriptorWrite_t *, uint32_t ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockMapBuffer( void *, const sirGraphicsABIBufferRange_t *, sirGraphicsMappedBuffer_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockUnmapBuffer( void *, const sirGraphicsABIBufferRange_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockFlushBuffer( void *, const sirGraphicsABIBufferRange_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockInvalidateBuffer( void *, const sirGraphicsABIBufferRange_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCreateSurface( void *, const sirGraphicsABINativeSurface_t *pNative, const sirGraphicsABIPresentDesc_t *pPresent,
                                   sirGraphicsHandle_t *pSurface ) noexcept
{
	if ( pNative == nullptr || pPresent == nullptr || pSurface == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_nMockMode < 26 || s_nMockMode > 29 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( s_nMockMode == 28 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pSurface = { 8, 1, s_nMockMode == 27 ? 4U : 5U, SIR_GRAPHICS_HANDLE_SURFACE };
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockResizeSurface( void *, sirGraphicsHandle_t hSurface, const sirGraphicsABINativeSurface_t *pNative,
                                   const sirGraphicsABIPresentDesc_t *pPresent ) noexcept
{
	if ( pNative == nullptr || pPresent == nullptr || hSurface.eType != SIR_GRAPHICS_HANDLE_SURFACE ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_nMockMode == 28 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	return s_nMockMode == 26 ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t mockAcquireSurfaceImage( void *, sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept
{
	if ( pImage == nullptr || hSurface.eType != SIR_GRAPHICS_HANDLE_SURFACE ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_nMockMode == 28 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( s_nMockMode < 26 || s_nMockMode > 29 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	*pImage = {};
	pImage->hSurface = hSurface;
	pImage->hImage = { 9, 1, s_nMockMode == 27 ? 4U : 5U, SIR_GRAPHICS_HANDLE_TEXTURE };
	pImage->nAcquisitionId = s_nMockMode == 29 ? 0 : 41;
	pImage->nWindowGeneration = 11;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockPresent( void *, const sirGraphicsSurfaceImage_t *pImage, const sirGraphicsCompletion_t *pComplete ) noexcept
{
	if ( pImage == nullptr || pComplete == nullptr || pImage->nAcquisitionId == 0 || pComplete->eQueue != SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_nMockMode == 28 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	return s_nMockMode == 26 ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_UNSUPPORTED;
}
static uint32_t mockCmdReset( void * ) noexcept
{
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockCmdBarrier( void *, const sirGraphicsBarrier_t *, uint32_t ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdCopyBuffer( void *, const sirGraphicsABICopyBuffer_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdCopyBufferTexture( void *, const sirGraphicsABIBufferTextureCopy_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdBeginRenderPass( void *, const sirGraphicsABIRenderPass_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdEndRenderPass( void * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdBindPipeline( void *, sirGraphicsHandle_t ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdBindBindingSet( void *, uint32_t, sirGraphicsHandle_t ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdBindVertexBuffer( void *, const sirGraphicsABIVertexBuffer_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdBindIndexBuffer( void *, const sirGraphicsABIIndexBuffer_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdSetViewport( void *, const sirGraphicsViewport_t *, const sirGraphicsRect_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdDraw( void *, const sirGraphicsABIDraw_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdDrawIndexed( void *, const sirGraphicsABIDrawIndexed_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCmdClose( void * ) noexcept
{
	return SIR_GRAPHICS_ABI_SUCCESS;
}
static uint32_t mockCmdDispatch( void *, uint32_t nX, uint32_t nY, uint32_t nZ ) noexcept
{
	return nX != 0 && nY != 0 && nZ != 0 ? SIR_GRAPHICS_ABI_SUCCESS : SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
}

static uint32_t mockBudget( void *, sirGraphicsMemoryBudget_t *pBudget ) noexcept
{
	if ( pBudget == nullptr ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( s_nMockMode == 20 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pBudget = {};
	pBudget->nBudgetBytes = 1024 * 1024;
	if ( s_nMockMode == 19 ) {
		pBudget->nCommittedBytes = 1;
		pBudget->nResidentBytes = 2;
		pBudget->nPeakCommittedBytes = 1;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockRetire( void *, sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t *pComplete ) noexcept
{
	if ( hResource.nDeviceGeneration != 5 || hResource.nGeneration == 0 || pComplete == nullptr || pComplete->nDeviceGeneration != 5 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	return s_nMockMode == 30 ? SIR_GRAPHICS_DEVICE_LOST : SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockAcquireCommands( void *, uint32_t nQueue, sirGraphicsABICommandList_t *pCommands ) noexcept
{
	if ( nQueue != SIR_GRAPHICS_QUEUE_DIRECT || pCommands == nullptr || pCommands->nSize < sizeof( sirGraphicsABICommandList_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( s_nMockMode == 25 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pCommands = {};
	pCommands->nSize = sizeof( *pCommands );
	pCommands->nDeviceGeneration = s_nMockMode == 21 ? 4U : 5U;
	pCommands->nQueue = s_nMockMode == 22 ? SIR_GRAPHICS_QUEUE_COMPUTE : nQueue;
	pCommands->pCommands = &s_nDeviceIdentity;
	pCommands->pReset = mockCmdReset;
	pCommands->pBarrier = mockCmdBarrier;
	pCommands->pCopyBuffer = mockCmdCopyBuffer;
	pCommands->pCopyBufferTexture = mockCmdCopyBufferTexture;
	pCommands->pBeginRenderPass = mockCmdBeginRenderPass;
	pCommands->pEndRenderPass = mockCmdEndRenderPass;
	pCommands->pBindPipeline = mockCmdBindPipeline;
	pCommands->pBindBindingSet = mockCmdBindBindingSet;
	pCommands->pBindVertexBuffer = mockCmdBindVertexBuffer;
	pCommands->pBindIndexBuffer = mockCmdBindIndexBuffer;
	pCommands->pSetViewport = mockCmdSetViewport;
	pCommands->pDraw = mockCmdDraw;
	pCommands->pDrawIndexed = mockCmdDrawIndexed;
	pCommands->pDispatch = mockCmdDispatch;
	pCommands->pClose = mockCmdClose;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockSubmit( void *, uint32_t nQueue, const sirGraphicsABISubmission_t *pBatch,
                            sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( nQueue != SIR_GRAPHICS_QUEUE_DIRECT || pBatch == nullptr || pBatch->nSize < sizeof( *pBatch ) || pBatch->nCommandListCount != 1 ||
	     pBatch->ppCommandLists == nullptr || pBatch->ppCommandLists[0] != &s_nDeviceIdentity || pCompletion == nullptr ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( s_nMockMode == 25 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	++s_nSubmitted;
	*pCompletion = { s_nMockMode == 23 ? 4U : 5U, SIR_GRAPHICS_QUEUE_DIRECT, s_nSubmitted };
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockCreate( void *, const sirGraphicsABIDeviceRequest_t *pRequest, sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pRequest == nullptr || pDevice == nullptr || pDevice->nSize < sizeof( sirGraphicsABIDevice_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( ( s_nLiveDevices != 0 && s_nMockMode != 48 && s_nMockMode != 62 ) || pRequest->nAdapterIndex != 0 ||
	     ( pRequest->nFramesInFlight > 2 && s_nMockMode != 47 ) ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	if ( s_nMockMode == 4 ) {
		return 99;
	}
	if ( s_nMockMode == 6 ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	if ( s_nMockMode == 7 ) {
		return SIR_GRAPHICS_BUSY;
	}
	++s_nLiveDevices;
	*pDevice = {};
	pDevice->nSize = sizeof( *pDevice );
	pDevice->pDevice = s_nMockMode == 62 && s_nLiveDevices == 2 ? &s_nDriverIdentity : &s_nDeviceIdentity;
	pDevice->pQueryCapabilities = mockQueryCapabilities;
	pDevice->pQueryPipelineModelCaps = mockQueryPipelineModelCaps;
	pDevice->pQueryFormat = mockQueryFormat;
	pDevice->pPoll = mockPoll;
	pDevice->pWait = mockWait;
	pDevice->pShutdown = mockShutdown;
	pDevice->pQueryMemoryBudget = mockBudget;
	pDevice->pCreateBuffer = mockCreateBuffer;
	pDevice->pCreateTexture = mockCreateTexture;
	pDevice->pCreateView = mockCreateView;
	pDevice->pCreateSampler = mockCreateSampler;
	pDevice->pCreatePipeline = mockCreatePipeline;
	pDevice->pCreateBindingSet = mockCreateBindingSet;
	pDevice->pUpdateBindings = mockUpdateBindings;
	pDevice->pMapBuffer = mockMapBuffer;
	pDevice->pUnmapBuffer = mockUnmapBuffer;
	pDevice->pFlushBuffer = mockFlushBuffer;
	pDevice->pInvalidateBuffer = mockInvalidateBuffer;
	pDevice->pAcquireCommands = mockAcquireCommands;
	pDevice->pSubmit = mockSubmit;
	pDevice->pRetireResource = mockRetire;
	pDevice->pCreateSurface = mockCreateSurface;
	pDevice->pResizeSurface = mockResizeSurface;
	pDevice->pAcquireSurfaceImage = mockAcquireSurfaceImage;
	pDevice->pPresent = mockPresent;
	if ( s_nMockMode == 5 ) {
		pDevice->pWait = nullptr;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockDestroy( void *, sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( s_nMockMode == 49 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( pDevice == nullptr || ( pDevice->pDevice != &s_nDeviceIdentity && pDevice->pDevice != &s_nDriverIdentity ) ||
	     s_nLiveDevices == 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	--s_nLiveDevices;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static void mockRelease( void * ) noexcept
{
}

extern "C" SIR_TEST_EXPORT uint32_t SIRGetGraphicsDriverABI( uint32_t nMajor, uint32_t nMinor, const sirGraphicsABIHost_t *pHost,
                                                             sirGraphicsABIDriver_t *pDriver ) noexcept
{
	if ( s_nMockMode == 41 ) {
		return SIR_GRAPHICS_ABI_VERSION_MISMATCH;
	}
	if ( nMajor != SIR_GRAPHICS_DRIVER_ABI_MAJOR || nMinor > SIR_GRAPHICS_DRIVER_ABI_MINOR ) {
		return SIR_GRAPHICS_ABI_VERSION_MISMATCH;
	}
	if ( pHost == nullptr || pDriver == nullptr || pDriver->nSize < sizeof( sirGraphicsABIDriver_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	*pDriver = {};
	pDriver->nSize = sizeof( *pDriver );
	pDriver->nMajor = nMajor;
	pDriver->nMinor = nMinor;
	pDriver->nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	pDriver->pDriver = &s_nDriverIdentity;
	pDriver->pAdapterCount = mockAdapterCount;
	pDriver->pQueryAdapter = mockQueryAdapter;
	pDriver->pCreateDevice = mockCreate;
	pDriver->pDestroyDevice = mockDestroy;
	pDriver->pReleaseDriver = mockRelease;
	return SIR_GRAPHICS_ABI_SUCCESS;
}
