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

extern "C" SIR_TEST_EXPORT void SIRSetGraphicsABIMockMode( uint32_t nMode ) noexcept
{
	s_nMockMode = nMode;
}

static uint32_t mockAdapterCount( void *, uint32_t *pCount ) noexcept
{
	if ( pCount == nullptr ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	*pCount = 1;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockQueryAdapter( void *, sirGraphicsABIAdapter_t *pInfo ) noexcept
{
	if ( pInfo == nullptr || pInfo->nSize < sizeof( sirGraphicsABIAdapter_t ) || pInfo->nIndex != 0 ) {
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
	pInfo->nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	memcpy( pInfo->szName, "Mock Vulkan CPU", sizeof( "Mock Vulkan CPU" ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockQueryCapabilities( void *, sirGraphicsABICapabilities_t *pInfo ) noexcept
{
	if ( pInfo == nullptr || pInfo->nSize < sizeof( sirGraphicsABICapabilities_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	*pInfo = {};
	pInfo->nSize = sizeof( *pInfo );
	pInfo->nDeviceGeneration = 5;
	pInfo->nBackend = s_nMockMode == 3 ? SIR_GRAPHICS_BACKEND_D3D12 : SIR_GRAPHICS_BACKEND_VULKAN;
	pInfo->nAdapterKind = SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	pInfo->nEnabledFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	pInfo->nEnabledQueues = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	pInfo->nMaximumFramesInFlight = 2;
	pInfo->nMaximumSamples = 1;
	pInfo->nMaximumTextureDimension = 4096;
	pInfo->nMaximumBindings = 16;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockQueryFormat( void *, sirGraphicsABIFormat_t *pInfo ) noexcept
{
	if ( pInfo == nullptr || pInfo->nSize < sizeof( sirGraphicsABIFormat_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( pInfo->nFormat != SIR_GRAPHICS_FORMAT_RGBA8_UNORM ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	const uint32_t nRequested = pInfo->nFormat;
	*pInfo = {};
	pInfo->nSize = sizeof( *pInfo );
	pInfo->nFormat = s_nMockMode == 2 ? SIR_GRAPHICS_FORMAT_BGRA8_UNORM : nRequested;
	pInfo->nOptimalFeatures = SIR_GRAPHICS_ABI_FORMAT_SAMPLED | SIR_GRAPHICS_ABI_FORMAT_TRANSFER_DST;
	pInfo->nSampleCountMask = 1;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockPoll( void *, const sirGraphicsCompletion_t *pCompletion, uint32_t *pDone ) noexcept
{
	if ( pCompletion == nullptr || pDone == nullptr || pCompletion->nDeviceGeneration != 5 ||
	     pCompletion->eQueue != SIR_GRAPHICS_QUEUE_DIRECT || pCompletion->nValue == 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	*pDone = pCompletion->nValue <= s_nSubmitted ? 1 : 0;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockWait( void *, const sirGraphicsCompletion_t *pCompletion, uint64_t ) noexcept
{
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

static uint32_t mockCreateBuffer( void *, const sirGraphicsBufferDesc_t *, sirGraphicsHandle_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCreateTexture( void *, const sirGraphicsTextureDesc_t *, sirGraphicsHandle_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCreateView( void *, const sirGraphicsViewDesc_t *, sirGraphicsHandle_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCreateSampler( void *, const sirGraphicsSamplerDesc_t *, sirGraphicsHandle_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockCreatePipeline( void *, const sirGraphicsABIPipeline_t *, sirGraphicsHandle_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
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

static uint32_t mockCreateSurface( void *, const sirGraphicsABINativeSurface_t *, const sirGraphicsABIPresentDesc_t *,
                                   sirGraphicsHandle_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockResizeSurface( void *, sirGraphicsHandle_t, const sirGraphicsABINativeSurface_t *,
                                   const sirGraphicsABIPresentDesc_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockAcquireSurfaceImage( void *, sirGraphicsHandle_t, sirGraphicsSurfaceImage_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t mockPresent( void *, const sirGraphicsSurfaceImage_t *, const sirGraphicsCompletion_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
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
	*pBudget = {};
	pBudget->nBudgetBytes = 1024 * 1024;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockRetire( void *, sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t *pComplete ) noexcept
{
	if ( hResource.nDeviceGeneration != 5 || hResource.nGeneration == 0 || pComplete == nullptr || pComplete->nDeviceGeneration != 5 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockAcquireCommands( void *, uint32_t nQueue, sirGraphicsABICommandList_t *pCommands ) noexcept
{
	if ( nQueue != SIR_GRAPHICS_QUEUE_DIRECT || pCommands == nullptr || pCommands->nSize < sizeof( sirGraphicsABICommandList_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	*pCommands = {};
	pCommands->nSize = sizeof( *pCommands );
	pCommands->nDeviceGeneration = 5;
	pCommands->nQueue = nQueue;
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
	++s_nSubmitted;
	*pCompletion = { 5, SIR_GRAPHICS_QUEUE_DIRECT, s_nSubmitted };
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t mockCreate( void *, const sirGraphicsABIDeviceRequest_t *pRequest, sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pRequest == nullptr || pDevice == nullptr || pDevice->nSize < sizeof( sirGraphicsABIDevice_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( s_nLiveDevices != 0 || pRequest->nAdapterIndex != 0 || pRequest->nFramesInFlight > 2 ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	if ( s_nMockMode == 4 ) {
		return 99;
	}
	++s_nLiveDevices;
	*pDevice = {};
	pDevice->nSize = sizeof( *pDevice );
	pDevice->pDevice = &s_nDeviceIdentity;
	pDevice->pQueryCapabilities = mockQueryCapabilities;
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
	if ( pDevice == nullptr || pDevice->pDevice != &s_nDeviceIdentity || s_nLiveDevices == 0 ) {
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
