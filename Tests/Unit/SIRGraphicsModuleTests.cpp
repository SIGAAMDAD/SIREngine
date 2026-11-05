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

#include <Renderer/SIRGraphicsModule.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <stdio.h>
#include <stdlib.h>

static void *testAlloc( void *, uint64_t nBytes, uint64_t ) noexcept
{
	return malloc( ( size_t )nBytes );
}

static void testFree( void *, void *pData ) noexcept
{
	free( pData );
}

static void testLog( void *, uint32_t, const char * ) noexcept
{
}

typedef void ( *sirMockSetMode_t )( uint32_t );

#if defined( __linux__ )
static bool s_bRejectNextDLUnload = false;
extern "C" int __real_dlclose( void *pHandle ) noexcept;
extern "C" int __wrap_dlclose( void *pHandle ) noexcept
{
	if ( s_bRejectNextDLUnload ) {
		s_bRejectNextDLUnload = false;
		return -1;
	}
	return __real_dlclose( pHandle );
}
#endif

static bool testHostBoundaryInputs( const sirGraphicsABIDevice_t *pDevice ) noexcept
{
	const sirGraphicsABIDevice_t *pNullDevice = nullptr;
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	sirGraphicsABIPipelineModelCaps_t model = {};
	model.nSize = sizeof( model );
	sirGraphicsHandle_t handle = { 19, 1, 5, SIR_GRAPHICS_HANDLE_BUFFER };
	sirGraphicsSurfaceImage_t image = {};
	sirGraphicsMemoryBudget_t budget = {};
	sirGraphicsCompletion_t completion = { 5, SIR_GRAPHICS_QUEUE_DIRECT, 1 };
	sirGraphicsCompletionSet_t retirement = {};
	retirement.nDeviceGeneration = 5;
	uint32_t nDone = 0;
	uint32_t nIndex = 99;
	sirGraphicsABIDraw_t draw = { 1, 1, 0, 0 };
	( void )draw;
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	pipeline.nSampleCount = 1;
	uint32_t nShader = 0x07230203;
	pipeline.computeShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &nShader, 4, 1, "main" };
	pipeline.pModern = &modern;
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 64;
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 16;
	texture.nHeight = 16;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	sirGraphicsViewDesc_t view = {};
	view.hResource = { 1, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE };
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	sirGraphicsSamplerDesc_t sampler = {};
	sampler.fMaximumAnisotropy = 1;
	sampler.fMaximumLod = 4;
	sirGraphicsABICommandList_t commands = {};
	commands.nSize = sizeof( commands );
	const void *ppLists[] = { &commands };
	sirGraphicsABISubmission_t submission = {};
	submission.nSize = sizeof( submission );
	submission.nCommandListCount = 1;
	submission.ppCommandLists = ppLists;
	sirGraphicsABINativeSurface_t native = {};
	native.nSize = sizeof( native );
	native.nPlatform = SIR_GRAPHICS_SURFACE_X11;
	native.pWindow = &native;
	native.nWindowGeneration = 1;
	sirGraphicsABIPresentDesc_t present = {};
	present.nSize = sizeof( present );
	present.nWidth = 64;
	present.nHeight = 64;
	present.nBackbufferCount = 2;
	present.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	present.nMode = SIR_GRAPHICS_PRESENT_FIFO;
#define CHECK_HOST( expression, label )                                                                                                    \
	if ( ( expression ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {                                                                               \
		fputs( "Host boundary accepted " label "\n", stderr );                                                                             \
		return false;                                                                                                                      \
	}
	CHECK_HOST( sirGraphicsQueryABICapabilities( pDevice, SIR_GRAPHICS_BACKEND_VULKAN, nullptr ), "null capabilities out" );
	CHECK_HOST( sirGraphicsQueryABICapabilities( pNullDevice, SIR_GRAPHICS_BACKEND_VULKAN, &caps ), "null capabilities device" );
	CHECK_HOST( sirGraphicsQueryABIFormat( pDevice, SIR_GRAPHICS_FORMAT_RGBA8_UNORM, nullptr ), "null format out" );
	CHECK_HOST( sirGraphicsQueryABIFormat( pNullDevice, SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &format ), "null format device" );
	CHECK_HOST( sirGraphicsQueryABIFormat( pDevice, SIR_GRAPHICS_FORMAT_UNKNOWN, &format ), "unknown format ID" );
	CHECK_HOST( sirGraphicsQueryABIFormat( pDevice, SIR_GRAPHICS_FORMAT_BC7_UNORM + 1, &format ), "format out of range" );
	caps.nSize = 1;
	CHECK_HOST( sirGraphicsQueryABICapabilities( pDevice, SIR_GRAPHICS_BACKEND_VULKAN, &caps ), "truncated caps output" );
	caps.nSize = sizeof( caps );
	format.nSize = 1;
	CHECK_HOST( sirGraphicsQueryABIFormat( pDevice, SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &format ), "truncated format output" );
	format.nSize = sizeof( format );
	model.nSize = 1;
	CHECK_HOST( sirGraphicsQueryABIPipelineModelCaps( pDevice, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &model ), "truncated model output" );
	model.nSize = sizeof( model );
	CHECK_HOST( sirGraphicsQueryABIPipelineModelCaps( pDevice, SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION + 1, &model ),
	            "out of range pipeline model" );
	CHECK_HOST( sirGraphicsQueryABIPipelineModelCaps( pDevice, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, nullptr ), "null model caps" );
	CHECK_HOST( sirGraphicsQueryABIPipelineModelCaps( pNullDevice, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &model ), "null model device" );
	CHECK_HOST( sirGraphicsQueryABIPipelineModelCaps( pDevice, 0, &model ), "invalid pipeline model" );
	CHECK_HOST( sirGraphicsSelectABIPipelineVariant( pDevice, nullptr, 1, &nIndex ), "null candidates" );
	CHECK_HOST( sirGraphicsSelectABIPipelineVariant( pDevice, &pipeline, 1, nullptr ), "null variant index" );
	CHECK_HOST( sirGraphicsSelectABIPipelineVariant( pNullDevice, &pipeline, 1, &nIndex ), "null variant device" );
	CHECK_HOST( sirGraphicsAcquireABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, nullptr ), "null command table" );
	CHECK_HOST( sirGraphicsAcquireABICommands( pNullDevice, SIR_GRAPHICS_QUEUE_DIRECT, &commands ), "null command device" );
	CHECK_HOST( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, nullptr, &completion ), "null submission" );
	CHECK_HOST( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &submission, nullptr ), "null submit completion" );
	CHECK_HOST( sirGraphicsSubmitABICommands( pNullDevice, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ), "null submit device" );
	CHECK_HOST( sirGraphicsPollABICompletion( pDevice, nullptr, &nDone ), "null poll completion" );
	CHECK_HOST( sirGraphicsPollABICompletion( pDevice, &completion, nullptr ), "null poll output" );
	CHECK_HOST( sirGraphicsPollABICompletion( pNullDevice, &completion, &nDone ), "null poll device" );
	CHECK_HOST( sirGraphicsWaitABICompletion( pDevice, nullptr, 0 ), "null wait completion" );
	CHECK_HOST( sirGraphicsWaitABICompletion( pNullDevice, &completion, 0 ), "null wait device" );
	CHECK_HOST( sirGraphicsCreateABISurface( pDevice, &native, &present, nullptr ), "null surface output" );
	CHECK_HOST( sirGraphicsCreateABISurface( pNullDevice, &native, &present, &handle ), "null surface device" );
	CHECK_HOST( sirGraphicsResizeABISurface( pNullDevice, handle, &native, &present ), "null resize device" );
	CHECK_HOST( sirGraphicsAcquireABISurfaceImage( pDevice, handle, nullptr ), "null surface image output" );
	CHECK_HOST( sirGraphicsAcquireABISurfaceImage( pNullDevice, handle, &image ), "null acquire device" );
	CHECK_HOST( sirGraphicsPresentABISurfaceImage( pDevice, nullptr, &completion ), "null present image" );
	CHECK_HOST( sirGraphicsPresentABISurfaceImage( pDevice, &image, nullptr ), "null present token" );
	CHECK_HOST( sirGraphicsPresentABISurfaceImage( pNullDevice, &image, &completion ), "null present device" );
	CHECK_HOST( sirGraphicsRetireABIResource( pDevice, handle, nullptr ), "null retirement fences" );
	CHECK_HOST( sirGraphicsRetireABIResource( pNullDevice, handle, &retirement ), "null retirement device" );
	CHECK_HOST( sirGraphicsQueryABIMemoryBudget( pDevice, nullptr ), "null memory budget" );
	CHECK_HOST( sirGraphicsQueryABIMemoryBudget( pNullDevice, &budget ), "null memory device" );
	CHECK_HOST( sirGraphicsCreateABIBuffer( pDevice, nullptr, &handle ), "null buffer desc" );
	CHECK_HOST( sirGraphicsCreateABIBuffer( pDevice, &buffer, nullptr ), "null buffer output" );
	CHECK_HOST( sirGraphicsCreateABIBuffer( pNullDevice, &buffer, &handle ), "null buffer device" );
	CHECK_HOST( sirGraphicsCreateABIView( pDevice, nullptr, &handle ), "null view desc" );
	CHECK_HOST( sirGraphicsCreateABIView( pDevice, &view, nullptr ), "null view output" );
	CHECK_HOST( sirGraphicsCreateABIView( pNullDevice, &view, &handle ), "null view device" );
	CHECK_HOST( sirGraphicsCreateABISampler( pDevice, nullptr, &handle ), "null sampler desc" );
	CHECK_HOST( sirGraphicsCreateABISampler( pDevice, &sampler, nullptr ), "null sampler output" );
	CHECK_HOST( sirGraphicsCreateABISampler( pNullDevice, &sampler, &handle ), "null sampler device" );
	CHECK_HOST( sirGraphicsCreateABITexture( pDevice, nullptr, &handle ), "null texture desc" );
	CHECK_HOST( sirGraphicsCreateABITexture( pDevice, &texture, nullptr ), "null texture output" );
	CHECK_HOST( sirGraphicsCreateABITexture( pNullDevice, &texture, &handle ), "null texture device" );
	CHECK_HOST( sirGraphicsCreateABIPipeline( pDevice, nullptr, &handle ), "null pipeline desc" );
	CHECK_HOST( sirGraphicsCreateABIPipeline( pDevice, &pipeline, nullptr ), "null pipeline output" );
	CHECK_HOST( sirGraphicsCreateABIPipeline( pNullDevice, &pipeline, &handle ), "null pipeline device" );
#undef CHECK_HOST
	return true;
}

static uint32_t s_nFaultCapsMode = 0;
static uint32_t ( *s_pUnderlyingCaps )( void *, sirGraphicsABICapabilities_t * ) = nullptr;

static uint32_t testQueryCapabilitiesFault( void *pNative, sirGraphicsABICapabilities_t *pCaps ) noexcept
{
	if ( s_nFaultCapsMode == 1 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( s_nFaultCapsMode == 2 ) {
		return 0xFFFFFFFFU;
	}
	const uint32_t nResult = s_pUnderlyingCaps( pNative, pCaps );
	if ( nResult != SIR_GRAPHICS_SUCCESS ) {
		return nResult;
	}
	if ( s_nFaultCapsMode == 3 ) {
		pCaps->nDeviceGeneration = 0;
	} else if ( s_nFaultCapsMode == 4 ) {
		pCaps->nEnabledFeatures = 0;
	} else if ( s_nFaultCapsMode == 5 ) {
		pCaps->nMaximumTextureDimension = 1;
	} else if ( s_nFaultCapsMode == 6 ) {
		pCaps->nEnabledQueues = 0;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static bool testHostProviderFaults( const sirGraphicsABIDevice_t *pDevice ) noexcept
{
	sirGraphicsABIDevice_t test = *pDevice;
	s_pUnderlyingCaps = pDevice->pQueryCapabilities;
	test.pQueryCapabilities = testQueryCapabilitiesFault;
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 128;
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 16;
	texture.nHeight = 16;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	sirGraphicsViewDesc_t view = {};
	view.hResource = { 2, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE };
	view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	view.nMipLevels = 1;
	view.nLayers = 1;
	sirGraphicsSamplerDesc_t sampler = {};
	sampler.fMaximumAnisotropy = 1;
	sampler.fMaximumLod = 16;
	sirGraphicsHandle_t handle = { 99, 1, 5, SIR_GRAPHICS_HANDLE_BUFFER };
	sirGraphicsABICommandList_t commands = {};
	commands.nSize = sizeof( commands );
	sirGraphicsCompletion_t completion = { 5, SIR_GRAPHICS_QUEUE_DIRECT, 1 };
	sirGraphicsCompletionSet_t retire = {};
	retire.nDeviceGeneration = 5;
	uint32_t nDone = 0;
	sirGraphicsABINativeSurface_t native = {};
	native.nSize = sizeof( native );
	native.nPlatform = SIR_GRAPHICS_SURFACE_X11;
	native.pWindow = &native;
	native.nWindowGeneration = 1;
	sirGraphicsABIPresentDesc_t present = {};
	present.nSize = sizeof( present );
	present.nWidth = 16;
	present.nHeight = 16;
	present.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	present.nBackbufferCount = 2;
	present.nMode = SIR_GRAPHICS_PRESENT_FIFO;
	sirGraphicsSurfaceImage_t image = {};
	const void *pCommands[] = { &commands };
	sirGraphicsABISubmission_t submission = {};
	submission.nSize = sizeof( submission );
	submission.nCommandListCount = 1;
	submission.ppCommandLists = pCommands;
#define CHECK_PROVIDER( expression, expected, label )                                                                                      \
	if ( ( expression ) != ( expected ) ) {                                                                                                \
		fputs( "Provider fault: " label "\n", stderr );                                                                                    \
		return false;                                                                                                                      \
	}
	for ( uint32_t nMode = 1; nMode <= 3; ++nMode ) {
		s_nFaultCapsMode = nMode;
		const sirGraphicsResult_t eExpected = nMode == 1 ? SIR_GRAPHICS_DEVICE_LOST : SIR_GRAPHICS_DRIVER_ERROR;
		CHECK_PROVIDER( sirGraphicsQueryABICapabilities( &test, SIR_GRAPHICS_BACKEND_VULKAN, &caps ), eExpected, "capability query" );
		CHECK_PROVIDER( sirGraphicsAcquireABICommands( &test, SIR_GRAPHICS_QUEUE_DIRECT, &commands ), eExpected, "acquire commands" );
		CHECK_PROVIDER( sirGraphicsSubmitABICommands( &test, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ), eExpected,
		                "submit commands" );
		CHECK_PROVIDER( sirGraphicsPollABICompletion( &test, &completion, &nDone ), eExpected, "poll completion" );
		CHECK_PROVIDER( sirGraphicsWaitABICompletion( &test, &completion, 0 ), eExpected, "wait completion" );
		CHECK_PROVIDER( sirGraphicsCreateABIBuffer( &test, &buffer, &handle ), eExpected, "create buffer" );
		CHECK_PROVIDER( sirGraphicsCreateABIView( &test, &view, &handle ), eExpected, "create view" );
		CHECK_PROVIDER( sirGraphicsCreateABISampler( &test, &sampler, &handle ), eExpected, "create sampler" );
		CHECK_PROVIDER( sirGraphicsCreateABITexture( &test, &texture, &handle ), eExpected, "create texture" );
		CHECK_PROVIDER( sirGraphicsCreateABISurface( &test, &native, &present, &handle ), eExpected, "create surface" );
		CHECK_PROVIDER( sirGraphicsResizeABISurface( &test, handle, &native, &present ), eExpected, "resize surface" );
		CHECK_PROVIDER( sirGraphicsAcquireABISurfaceImage( &test, handle, &image ), eExpected, "acquire surface image" );
		CHECK_PROVIDER( sirGraphicsPresentABISurfaceImage( &test, &image, &completion ), eExpected, "present surface" );
		CHECK_PROVIDER( sirGraphicsRetireABIResource( &test, handle, &retire ), eExpected, "retire resource" );
		if ( handle.nSlot != 99 ) {
			fputs( "Faulting native provider mutated caller's handle\n", stderr );
			return false;
		}
	}
	s_nFaultCapsMode = 4;
	buffer.nUsage = SIR_GRAPHICS_USAGE_VERTEX;
	CHECK_PROVIDER( sirGraphicsCreateABIBuffer( &test, &buffer, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                "graphics buffer with disabled graphics" );
	buffer.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	CHECK_PROVIDER( sirGraphicsCreateABIBuffer( &test, &buffer, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                "storage buffer with disabled storage" );
	buffer.nUsage = SIR_GRAPHICS_USAGE_INDIRECT;
	CHECK_PROVIDER( sirGraphicsCreateABIBuffer( &test, &buffer, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                "indirect buffer with disabled indirect" );
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_UPLOAD;
	CHECK_PROVIDER( sirGraphicsCreateABIBuffer( &test, &buffer, &handle ), SIR_GRAPHICS_UNSUPPORTED, "GPU upload with disabled upload" );
	texture.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	CHECK_PROVIDER( sirGraphicsCreateABITexture( &test, &texture, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                "storage texture without shader storage" );
	CHECK_PROVIDER( sirGraphicsCreateABISurface( &test, &native, &present, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                "surface without present feature" );
	s_nFaultCapsMode = 5;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	CHECK_PROVIDER( sirGraphicsCreateABITexture( &test, &texture, &handle ), SIR_GRAPHICS_UNSUPPORTED, "texture exceeds dimension" );
	s_nFaultCapsMode = 6;
	CHECK_PROVIDER( sirGraphicsQueryABICapabilities( &test, SIR_GRAPHICS_BACKEND_VULKAN, &caps ), SIR_GRAPHICS_DRIVER_ERROR,
	                "invalid zero queue capability" );
	s_nFaultCapsMode = 0;
#undef CHECK_PROVIDER
	return true;
}

static bool testCheckedBinarySubmissionMatrix( const sirGraphicsABIDevice_t *pDevice, sirMockSetMode_t pMode ) noexcept
{
	pMode( 26 );
	sirGraphicsABICommandList_t commands = {};
	commands.nSize = sizeof( commands );
	if ( sirGraphicsAcquireABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &commands ) != SIR_GRAPHICS_SUCCESS ) {
		pMode( 0 );
		return false;
	}
	const void *pCommandLists[] = { commands.pCommands, commands.pCommands };
	sirGraphicsCompletion_t waits[] = { { 5, SIR_GRAPHICS_QUEUE_DIRECT, 1 } };
	sirGraphicsSurfaceImage_t acquisitions[] = {
	    { { 1, 1, 5, SIR_GRAPHICS_HANDLE_SURFACE }, { 2, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE }, 11, 2 },
	    { { 1, 1, 5, SIR_GRAPHICS_HANDLE_SURFACE }, { 2, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE }, 11, 2 } };
	sirGraphicsABISubmission_t base = {};
	base.nSize = sizeof( base );
	base.nCommandListCount = 1;
	base.ppCommandLists = pCommandLists;
	base.nWaitCount = 1;
	base.pWaits = waits;
	base.nAcquisitionCount = 1;
	base.pAcquisitions = acquisitions;
	sirGraphicsCompletion_t completion = { 5, SIR_GRAPHICS_QUEUE_DIRECT, 21 };
#define CHECK_BINARY_BATCH( expression, status, label )                                                                                    \
	if ( ( expression ) != status ) {                                                                                                      \
		fputs( "Binary queue submission: " label "\n", stderr );                                                                           \
		pMode( 0 );                                                                                                                        \
		return false;                                                                                                                      \
	}
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &base, &completion ), SIR_GRAPHICS_SUCCESS,
	                    "valid WSI submission" );
	sirGraphicsABISubmission_t test = base;
	test.nSize = 1;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "truncated size tag" );
	test = base;
	test.nReserved = 1;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "nonzero reserved" );
	test = base;
	test.nCommandListCount = 0;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "empty command list" );
	test = base;
	test.nCommandListCount = 65;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "command limit" );
	test = base;
	test.ppCommandLists = nullptr;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "null commands" );
	test = base;
	test.nWaitCount = 65;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "wait limit" );
	test = base;
	test.nAcquisitionCount = 9;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "acquisition limit" );
	test = base;
	test.pWaits = nullptr;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "null waits" );
	test = base;
	test.pAcquisitions = nullptr;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "null acquisitions" );
	test = base;
	const void *pNullCommandList[] = { nullptr };
	test.ppCommandLists = pNullCommandList;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "null command pointer" );
	test = base;
	test.nCommandListCount = 2;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "duplicate command handles" );
	test = base;
	waits[0].nDeviceGeneration = 4;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "stale wait device" );
	waits[0].nDeviceGeneration = 5;
	waits[0].eQueue = SIR_GRAPHICS_QUEUE_COUNT;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "invalid wait queue" );
	waits[0].eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	waits[0].nValue = 0;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "zero wait value" );
	waits[0].nValue = 1;
	acquisitions[0].hSurface.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "wrong surface type" );
	acquisitions[0].hSurface.eType = SIR_GRAPHICS_HANDLE_SURFACE;
	acquisitions[0].hImage.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "wrong image type" );
	acquisitions[0].hImage.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	acquisitions[0].nAcquisitionId = 0;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "zero acquired image identity" );
	acquisitions[0].nAcquisitionId = 11;
	acquisitions[0].nWindowGeneration = 0;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "missing window generation" );
	acquisitions[0].nWindowGeneration = 2;
	test.nAcquisitionCount = 2;
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ),
	                    SIR_GRAPHICS_INVALID_ARGUMENT, "duplicate acquired image" );
	acquisitions[1].nAcquisitionId = 12;
	// The structural checks pass. This mock provider only supports single-acquisition batches.
	CHECK_BINARY_BATCH( sirGraphicsSubmitABICommands( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &test, &completion ), SIR_GRAPHICS_SUCCESS,
	                    "provider accepts distinct acquired images" );
#undef CHECK_BINARY_BATCH
	pMode( 0 );
	return true;
}

static bool testNativeResourceAdmissionMatrix( const sirGraphicsABIDevice_t *pDevice, sirMockSetMode_t pMode ) noexcept
{
	pMode( 10 );
	sirGraphicsViewDesc_t baseView = {};
	baseView.hResource = { 4, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE };
	baseView.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	baseView.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	baseView.nMipLevels = 1;
	baseView.nLayers = 1;
	sirGraphicsHandle_t handle = { 99, 1, 5, SIR_GRAPHICS_HANDLE_VIEW };
#define CHECK_NATIVE_RESOURCE( expression, expected, label )                                                                               \
	if ( ( expression ) != expected ) {                                                                                                    \
		fputs( "Native resource admission: " label "\n", stderr );                                                                         \
		pMode( 0 );                                                                                                                        \
		return false;                                                                                                                      \
	}
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &baseView, &handle ), SIR_GRAPHICS_SUCCESS, "valid sampled texture view" );
	sirGraphicsViewDesc_t view = baseView;
	view.hResource.eType = SIR_GRAPHICS_HANDLE_SAMPLER;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "view of non-resource handle" );
	view = baseView;
	view.nBaseMip = UINT32_MAX;
	view.nMipLevels = 2;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT, "mip range overflow" );
	view = baseView;
	view.nBaseLayer = UINT32_MAX;
	view.nLayers = 2;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT, "layer range overflow" );
	view = baseView;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "multiple exclusive view usages" );
	view = baseView;
	view.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "non-depth format with depth usage" );
	view = baseView;
	view.eFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "depth format with color usage" );
	view.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_SUCCESS, "valid depth texture view" );
	view = baseView;
	view.hResource.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	view.nMipLevels = 0;
	view.nLayers = 0;
	view.nBufferSizeBytes = 0;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero buffer-view extent" );
	view.nBufferSizeBytes = 16;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_SUCCESS, "valid buffer view" );
	view.nBaseMip = 1;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "buffer view has mip extent" );
	view.nBaseMip = 0;
	view.nBufferOffsetBytes = UINT64_MAX;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "buffer view extent overflow" );
	view.nBufferOffsetBytes = 0;
	view.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "buffer attachment view forbidden" );
	view = baseView;
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	pMode( 14 );
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABIView( pDevice, &view, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                       "view attachment format not supported" );
	pMode( 10 );
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 16;
	texture.nHeight = 16;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_SUCCESS, "valid sampled resource" );
	texture.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "depth usage with color format" );
	texture.eFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	texture.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "color usage with depth format" );
	texture.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_SUCCESS, "valid depth image" );
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                       "transfer source feature absent" );
	texture.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_SUCCESS,
	                       "transfer destination supported" );
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                       "linear image format unavailable" );
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	texture.nDepth = 4097;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_3D;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                       "depth dimension exceeds device limit" );
	texture.nDepth = 1;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	pMode( 15 );
	texture.nSampleCount = 2;
	texture.nMipLevels = 2;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                       "multisample texture has mip chain" );
	texture.nMipLevels = 1;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_SUCCESS,
	                       "supported native 2x multisample" );
	texture.nSampleCount = 1;
	CHECK_NATIVE_RESOURCE( sirGraphicsCreateABITexture( pDevice, &texture, &handle ), SIR_GRAPHICS_UNSUPPORTED,
	                       "native format excludes sample-1" );
#undef CHECK_NATIVE_RESOURCE
	pMode( 0 );
	return true;
}

static bool testDeviceRequestPreflightMatrix( sirGraphicsABIModule_t *pModule, const sirGraphicsABIDeviceRequest_t& valid ) noexcept
{
	sirGraphicsABIDevice_t output = {};
	output.nSize = sizeof( output );
#define CHECK_DEVICE_REQUEST( expression, expected, label )                                                                                \
	if ( ( expression ) != expected ) {                                                                                                    \
		fputs( "Device request contract: " label "\n", stderr );                                                                           \
		return false;                                                                                                                      \
	}
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( nullptr, &valid, &output ), SIR_GRAPHICS_INVALID_ARGUMENT, "null module" );
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( pModule, nullptr, &output ), SIR_GRAPHICS_INVALID_ARGUMENT, "null request" );
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( pModule, &valid, nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT, "null device" );
	sirGraphicsABIDeviceRequest_t request = valid;
	request.nSize = 1;
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( pModule, &request, &output ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "short request size tag" );
	request = valid;
	request.nFramesInFlight = 0;
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( pModule, &request, &output ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "zero frames in flight" );
	request = valid;
	request.nAllowSoftware = 2;
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( pModule, &request, &output ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "invalid software boolean" );
	request = valid;
	request.nHeadless = 2;
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( pModule, &request, &output ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "invalid headless boolean" );
	request = valid;
	request.nReserved = 1;
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( pModule, &request, &output ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "nonzero reserved request" );
	output.nSize = 1;
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( pModule, &valid, &output ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "short native device output" );
	output.nSize = sizeof( output );
	request = valid;
	request.nAdapterIndex = 255;
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( pModule, &request, &output ), SIR_GRAPHICS_UNSUPPORTED,
	                      "nonexistent selected adapter" );
	sirGraphicsABIModule_t saturated = *pModule;
	saturated.nActiveDevices = 16;
	CHECK_DEVICE_REQUEST( sirGraphicsCreateABIDevice( &saturated, &valid, &output ), SIR_GRAPHICS_CAPACITY_EXCEEDED,
	                      "native module device capacity" );
	if ( output.pDevice != nullptr || pModule->nActiveDevices != 0 ) {
		return false;
	}
#undef CHECK_DEVICE_REQUEST
	return true;
}

static uint32_t s_nModelCapsFaultMode = 0;
static uint32_t ( *s_pUnderlyingModelCaps )( void *, uint32_t, sirGraphicsABIPipelineModelCaps_t * ) = nullptr;

static uint32_t testQueryModelCapsFault( void *pNative, uint32_t nModel, sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept
{
	if ( s_nModelCapsFaultMode == 9 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( s_nModelCapsFaultMode == 10 ) {
		return 99;
	}
	if ( s_nModelCapsFaultMode == 11 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nResult = s_pUnderlyingModelCaps( pNative, nModel, pCaps );
	if ( nResult != SIR_GRAPHICS_SUCCESS ) {
		return nResult;
	}
	switch ( s_nModelCapsFaultMode ) {
	case 1:
		pCaps->nSize = 1;
		break;
	case 2:
		pCaps->nDeviceGeneration = 4;
		break;
	case 3:
		pCaps->nRequiredFeatureMask |= SIR_GRAPHICS_FEATURE_RAY_TRACING;
		break;
	case 4:
		pCaps->nQueueMask |= 1U << SIR_GRAPHICS_QUEUE_COMPUTE;
		break;
	case 5:
		pCaps->nMaximumSamples = 2;
		break;
	case 6:
		pCaps->nMaximumColorAttachments = 1;
		break;
	case 7:
		pCaps->nMaximumTextureDimension = 8192;
		break;
	case 8:
		pCaps->nMaximumBindings = 17;
		break;
	default:
		break;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static bool testModelProviderContractMatrix( const sirGraphicsABIDevice_t *pDevice, sirMockSetMode_t pMode ) noexcept
{
	pMode( 0 );
	sirGraphicsABIDevice_t wrapped = *pDevice;
	s_pUnderlyingModelCaps = pDevice->pQueryPipelineModelCaps;
	wrapped.pQueryPipelineModelCaps = testQueryModelCapsFault;
	sirGraphicsABIPipelineModelCaps_t out = {};
	out.nSize = sizeof( out );
	for ( uint32_t i = 0; i <= 11; ++i ) {
		s_nModelCapsFaultMode = i;
		out.nDeviceGeneration = 77;
		const sirGraphicsResult_t eActual = sirGraphicsQueryABIPipelineModelCaps( &wrapped, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &out );
		const sirGraphicsResult_t eExpected = i == 0    ? SIR_GRAPHICS_SUCCESS
		                                      : i == 9  ? SIR_GRAPHICS_DEVICE_LOST
		                                      : i == 11 ? SIR_GRAPHICS_UNSUPPORTED
		                                                : SIR_GRAPHICS_DRIVER_ERROR;
		if ( eActual != eExpected || ( i != 0 && out.nDeviceGeneration != 77 ) ) {
			fprintf( stderr, "Model capability contract mode %u: received %u, expected %u\n", i, eActual, eExpected );
			s_nModelCapsFaultMode = 0;
			return false;
		}
	}
	s_nModelCapsFaultMode = 0;
	return true;
}

static bool testModuleLifecyclePreflightMatrix( const sirGraphicsABIHost_t& host, const sirGraphicsABIModule_t& live ) noexcept
{
	sirGraphicsABIModule_t empty = {};
#define CHECK_MODULE_PREFLIGHT( expression, expected, label )                                                                              \
	if ( ( expression ) != expected ) {                                                                                                    \
		fputs( "Module boundary: " label "\n", stderr );                                                                                   \
		return false;                                                                                                                      \
	}
	CHECK_MODULE_PREFLIGHT( sirGraphicsLoadABIModule( nullptr, &host, &empty ), SIR_GRAPHICS_INVALID_ARGUMENT, "null path" );
	CHECK_MODULE_PREFLIGHT( sirGraphicsLoadABIModule( "", &host, &empty ), SIR_GRAPHICS_INVALID_ARGUMENT, "empty path" );
	CHECK_MODULE_PREFLIGHT( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                        "null module output" );
	empty.driver.pDriver = &empty;
	CHECK_MODULE_PREFLIGHT( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &empty ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                        "dirty native driver identity" );
	empty = {};
	empty.nActiveDevices = 1;
	CHECK_MODULE_PREFLIGHT( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &empty ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                        "dirty active device count" );
	empty = {};
	sirGraphicsABIHost_t invalidHost = host;
	invalidHost.pFree = nullptr;
	CHECK_MODULE_PREFLIGHT( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &invalidHost, &empty ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                        "invalid host callback" );
	CHECK_MODULE_PREFLIGHT( sirGraphicsUnloadABIModule( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT, "null module unload" );
	CHECK_MODULE_PREFLIGHT( sirGraphicsUnloadABIModule( &empty ), SIR_GRAPHICS_INVALID_ARGUMENT, "empty module unload" );
	empty.library.pNativeHandle = &empty;
	CHECK_MODULE_PREFLIGHT( sirGraphicsUnloadABIModule( &empty ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                        "module with invalid driver cannot unload" );
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	CHECK_MODULE_PREFLIGHT( sirGraphicsQueryABIAdapter( nullptr, 0, &adapter ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                        "adapter query null module" );
	CHECK_MODULE_PREFLIGHT( sirGraphicsQueryABIAdapter( &live, 0, nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT, "adapter query null output" );
	adapter.nSize = 1;
	CHECK_MODULE_PREFLIGHT( sirGraphicsQueryABIAdapter( &live, 0, &adapter ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                        "adapter query truncated output" );
	adapter.nSize = sizeof( adapter );
	empty = live;
	empty.driver.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR + 1;
	CHECK_MODULE_PREFLIGHT( sirGraphicsQueryABIAdapter( &empty, 0, &adapter ), SIR_GRAPHICS_VERSION_MISMATCH,
	                        "adapter query invalid driver ABI version" );
#undef CHECK_MODULE_PREFLIGHT
	return true;
}

static uint32_t s_nNativeHandleFaultMode = 0;
static uint32_t ( *s_pOriginalNativeBuffer )( void *, const sirGraphicsBufferDesc_t *, sirGraphicsHandle_t * ) = nullptr;
static uint32_t ( *s_pOriginalNativeTexture )( void *, const sirGraphicsTextureDesc_t *, sirGraphicsHandle_t * ) = nullptr;
static uint32_t ( *s_pOriginalNativeView )( void *, const sirGraphicsViewDesc_t *, sirGraphicsHandle_t * ) = nullptr;
static uint32_t ( *s_pOriginalNativeSampler )( void *, const sirGraphicsSamplerDesc_t *, sirGraphicsHandle_t * ) = nullptr;
static uint32_t ( *s_pOriginalNativePipeline )( void *, const sirGraphicsABIPipeline_t *, sirGraphicsHandle_t * ) = nullptr;

static uint32_t testCorruptNativeHandle( uint32_t eResult, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	if ( s_nNativeHandleFaultMode == 1 ) {
		pHandle->eType = SIR_GRAPHICS_HANDLE_NONE;
	} else if ( s_nNativeHandleFaultMode == 2 ) {
		pHandle->nGeneration = 0;
	} else if ( s_nNativeHandleFaultMode == 3 ) {
		pHandle->nDeviceGeneration = 4;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t testCreateNativeBufferHandle( void *pNative, const sirGraphicsBufferDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return testCorruptNativeHandle( s_pOriginalNativeBuffer( pNative, pDesc, pHandle ), pHandle );
}

static uint32_t testCreateNativeTextureHandle( void *pNative, const sirGraphicsTextureDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return testCorruptNativeHandle( s_pOriginalNativeTexture( pNative, pDesc, pHandle ), pHandle );
}

static uint32_t testCreateNativeViewHandle( void *pNative, const sirGraphicsViewDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return testCorruptNativeHandle( s_pOriginalNativeView( pNative, pDesc, pHandle ), pHandle );
}

static uint32_t testCreateNativeSamplerHandle( void *pNative, const sirGraphicsSamplerDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	return testCorruptNativeHandle( s_pOriginalNativeSampler( pNative, pDesc, pHandle ), pHandle );
}

static uint32_t testCreateNativePipelineHandle( void *pNative, const sirGraphicsABIPipeline_t *pDesc,
                                                sirGraphicsHandle_t *pHandle ) noexcept
{
	return testCorruptNativeHandle( s_pOriginalNativePipeline( pNative, pDesc, pHandle ), pHandle );
}

static bool testNativeHandleReturnContractMatrix( const sirGraphicsABIDevice_t *pDevice, sirMockSetMode_t pMode ) noexcept
{
	pMode( 10 );
	sirGraphicsABIDevice_t device = *pDevice;
	s_pOriginalNativeBuffer = pDevice->pCreateBuffer;
	s_pOriginalNativeTexture = pDevice->pCreateTexture;
	s_pOriginalNativeView = pDevice->pCreateView;
	s_pOriginalNativeSampler = pDevice->pCreateSampler;
	s_pOriginalNativePipeline = pDevice->pCreatePipeline;
	device.pCreateBuffer = testCreateNativeBufferHandle;
	device.pCreateTexture = testCreateNativeTextureHandle;
	device.pCreateView = testCreateNativeViewHandle;
	device.pCreateSampler = testCreateNativeSamplerHandle;
	device.pCreatePipeline = testCreateNativePipelineHandle;
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 128;
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 16;
	texture.nHeight = 16;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsViewDesc_t view = {};
	view.hResource = { 4, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE };
	view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	view.nMipLevels = 1;
	view.nLayers = 1;
	sirGraphicsSamplerDesc_t sampler = {};
	sampler.fMaximumAnisotropy = 1;
	sampler.fMaximumLod = 16;
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	uint32_t nShader = 0x07230203;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	pipeline.nSampleCount = 1;
	pipeline.computeShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &nShader, 4, 1, "main" };
	pipeline.pModern = &modern;
	for ( uint32_t i = 0; i <= 3; ++i ) {
		s_nNativeHandleFaultMode = i;
		const sirGraphicsResult_t eExpected = i == 0 ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
		sirGraphicsHandle_t handle = { 99, 1, 5, SIR_GRAPHICS_HANDLE_BUFFER };
#define CHECK_NATIVE_HANDLE( expression, name )                                                                                            \
	if ( ( expression ) != eExpected || ( i != 0 && handle.nSlot != 99 ) ) {                                                               \
		fprintf( stderr, "Native %s handle contract mode %u failed\n", name, i );                                                          \
		s_nNativeHandleFaultMode = 0;                                                                                                      \
		pMode( 0 );                                                                                                                        \
		return false;                                                                                                                      \
	}
		CHECK_NATIVE_HANDLE( sirGraphicsCreateABIBuffer( &device, &buffer, &handle ), "buffer" );
		handle = { 99, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE };
		CHECK_NATIVE_HANDLE( sirGraphicsCreateABITexture( &device, &texture, &handle ), "texture" );
		handle = { 99, 1, 5, SIR_GRAPHICS_HANDLE_VIEW };
		CHECK_NATIVE_HANDLE( sirGraphicsCreateABIView( &device, &view, &handle ), "view" );
		handle = { 99, 1, 5, SIR_GRAPHICS_HANDLE_SAMPLER };
		CHECK_NATIVE_HANDLE( sirGraphicsCreateABISampler( &device, &sampler, &handle ), "sampler" );
		handle = { 99, 1, 5, SIR_GRAPHICS_HANDLE_PIPELINE };
		CHECK_NATIVE_HANDLE( sirGraphicsCreateABIPipeline( &device, &pipeline, &handle ), "pipeline" );
#undef CHECK_NATIVE_HANDLE
	}
	s_nNativeHandleFaultMode = 0;
	pMode( 0 );
	return true;
}

static uint32_t s_nMemoryBudgetFaultMode = 0;

static uint32_t testQueryNativeBudgetFault( void *, sirGraphicsMemoryBudget_t *pBudget ) noexcept
{
	if ( s_nMemoryBudgetFaultMode == 5 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( s_nMemoryBudgetFaultMode == 6 ) {
		return 99;
	}
	*pBudget = {};
	pBudget->nBudgetBytes = 1024;
	pBudget->nCommittedBytes = 256;
	pBudget->nResidentBytes = 128;
	pBudget->nPeakCommittedBytes = 256;
	if ( s_nMemoryBudgetFaultMode == 1 ) {
		pBudget->nPeakCommittedBytes = 0;
	} else if ( s_nMemoryBudgetFaultMode == 2 ) {
		pBudget->nPeakCommittedBytes = 127;
	} else if ( s_nMemoryBudgetFaultMode == 3 ) {
		pBudget->nResidentBytes = 300;
	} else if ( s_nMemoryBudgetFaultMode == 4 ) {
		pBudget->nPeakCommittedBytes = 257;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static bool testMemoryBudgetProviderMatrix( const sirGraphicsABIDevice_t *pDevice ) noexcept
{
	sirGraphicsABIDevice_t wrapped = *pDevice;
	wrapped.pQueryMemoryBudget = testQueryNativeBudgetFault;
	for ( uint32_t i = 0; i <= 6; ++i ) {
		s_nMemoryBudgetFaultMode = i;
		sirGraphicsMemoryBudget_t budget = {};
		budget.nCommittedBytes = 777;
		const sirGraphicsResult_t eStatus = sirGraphicsQueryABIMemoryBudget( &wrapped, &budget );
		const sirGraphicsResult_t eExpected = i == 2 || i == 3 || i == 6 ? SIR_GRAPHICS_DRIVER_ERROR
		                                      : i == 5                   ? SIR_GRAPHICS_DEVICE_LOST
		                                                                 : SIR_GRAPHICS_SUCCESS;
		if ( eStatus != eExpected || ( eStatus != SIR_GRAPHICS_SUCCESS && budget.nCommittedBytes != 777 ) ||
		     ( eStatus == SIR_GRAPHICS_SUCCESS && budget.nCommittedBytes != 256 ) ) {
			fprintf( stderr, "Memory budget provider fault mode %u returned %u expected %u\n", i, eStatus, eExpected );
			s_nMemoryBudgetFaultMode = 0;
			return false;
		}
	}
	s_nMemoryBudgetFaultMode = 0;
	return true;
}

static uint32_t s_nSyntheticAdapterMode = 0;

static uint32_t testCountRankedAdapters( void *, uint32_t *pCount ) noexcept
{
	if ( s_nSyntheticAdapterMode == 6 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( s_nSyntheticAdapterMode == 7 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pCount = s_nSyntheticAdapterMode == 4 ? 1 : 4;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t testDescribeRankedAdapter( void *, sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	if ( s_nSyntheticAdapterMode == 8 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const uint32_t nIndex = pAdapter->nIndex;
	*pAdapter = {};
	pAdapter->nSize = sizeof( *pAdapter );
	pAdapter->nIndex = nIndex;
	pAdapter->nBackend = s_nSyntheticAdapterMode == 4 ? SIR_GRAPHICS_BACKEND_D3D12 : SIR_GRAPHICS_BACKEND_VULKAN;
	pAdapter->nKind = SIR_GRAPHICS_ABI_ADAPTER_INTEGRATED;
	pAdapter->nApiMajor = 1;
	pAdapter->nApiMinor = 4;
	pAdapter->nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	pAdapter->nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE;
	pAdapter->nDedicatedBytes = 0;
	pAdapter->nSharedBytes = nIndex == 0 ? 100 : 200;
	pAdapter->nStableId = nIndex == 0 ? 300 : ( nIndex == 1 ? 200 : 50 );
	if ( s_nSyntheticAdapterMode == 1 && nIndex == 3 ) {
		pAdapter->nFeatureMask |= SIR_GRAPHICS_FEATURE_HDR_OUTPUT;
	}
	if ( s_nSyntheticAdapterMode == 2 && nIndex == 3 ) {
		pAdapter->nDedicatedBytes = 100;
	}
	if ( s_nSyntheticAdapterMode == 3 && nIndex == 3 ) {
		pAdapter->nSharedBytes = 300;
	}
	if ( s_nSyntheticAdapterMode == 5 ) {
		pAdapter->nApiMajor = 0;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static bool testAdapterRankingBoundaries( const sirGraphicsABIModule_t *pModule ) noexcept
{
	sirGraphicsABIModule_t synthetic = *pModule;
	synthetic.driver.pAdapterCount = testCountRankedAdapters;
	synthetic.driver.pQueryAdapter = testDescribeRankedAdapter;
	const sirGraphicsABIModule_t *modules[] = { &synthetic };
	sirGraphicsABIModuleSelection_t policy = {};
	policy.nSize = sizeof( policy );
	policy.nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	policy.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsABIAdapterChoice_t choice = {};
	choice.nSize = sizeof( choice );
#define CHECK_ADAPTER( expression, expected, label )                                                                                       \
	if ( ( expression ) != ( expected ) ) {                                                                                                \
		fputs( "Adapter selection rejected wrong: " label "\n", stderr );                                                                  \
		return false;                                                                                                                      \
	}
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( nullptr, 1, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT, "null list" );
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 0, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT, "empty list" );
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 17, &policy, &choice ), SIR_GRAPHICS_CAPACITY_EXCEEDED,
	               "module overcapacity" );
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, nullptr, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT, "missing policy" );
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT, "missing output" );
	policy.nSize = 1;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT, "short policy" );
	policy.nSize = sizeof( policy );
	choice.nSize = 1;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT, "short output" );
	choice.nSize = sizeof( choice );
	policy.nBackend = 99;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT, "bad backend" );
	policy.nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	policy.nAllowSoftware = 2;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT, "bad boolean" );
	policy.nAllowSoftware = 0;
	policy.nReserved = 1;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT, "reserved policy" );
	policy.nReserved = 0;
	policy.nMinimumApiMinor = 1;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT,
	               "minor without major" );
	policy.nMinimumApiMinor = 0;
	const sirGraphicsABIModule_t *missing[] = { nullptr };
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( missing, 1, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT, "null module record" );
	sirGraphicsABIModule_t broken = synthetic;
	broken.driver.pQueryAdapter = nullptr;
	const sirGraphicsABIModule_t *invalid[] = { &broken };
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( invalid, 1, &policy, &choice ), SIR_GRAPHICS_INVALID_ARGUMENT,
	               "malformed binary driver" );
	s_nSyntheticAdapterMode = 0;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_SUCCESS, "stable id rank" );
	if ( choice.adapter.nIndex != 2 ) {
		fputs( "Stable-id tie-break selected wrong adapter\n", stderr );
		return false;
	}
	for ( uint32_t mode = 1; mode <= 3; ++mode ) {
		s_nSyntheticAdapterMode = mode;
		CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_SUCCESS, "feature/memory ordering" );
		if ( choice.adapter.nIndex != 3 ) {
			fputs( "Capability or memory ranking ignored\n", stderr );
			return false;
		}
	}
	s_nSyntheticAdapterMode = 5;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_DRIVER_ERROR,
	               "zero API version from provider" );
	s_nSyntheticAdapterMode = 6;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_UNSUPPORTED,
	               "explicit unsupported count" );
	policy.nBackend = SIR_GRAPHICS_BACKEND_AUTO;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_UNSUPPORTED,
	               "skip unsupported AUTO module" );
	s_nSyntheticAdapterMode = 7;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_DEVICE_LOST,
	               "provider count device loss" );
	s_nSyntheticAdapterMode = 8;
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( modules, 1, &policy, &choice ), SIR_GRAPHICS_DEVICE_LOST,
	               "provider enumerate device loss" );
	sirGraphicsABIModule_t d3d12 = synthetic;
	d3d12.driver.nBackend = SIR_GRAPHICS_BACKEND_D3D12;
	const sirGraphicsABIModule_t *both[] = { &d3d12, &synthetic };
	s_nSyntheticAdapterMode = 0;
	// D3D12 would also return Vulkan-advertised adapters in this shared
	// callback variant; explicit requests must reject a backend mismatch.
	CHECK_ADAPTER( sirGraphicsSelectABIModuleAdapter( both, 2, &policy, &choice ), SIR_GRAPHICS_DRIVER_ERROR,
	               "provider adapter/backend identity mismatch" );
	s_nSyntheticAdapterMode = 0;
#undef CHECK_ADAPTER
	return true;
}

// Each fault is produced by the loaded DSO, so these checks exercise host
// translation and atomic output preservation across the actual ABI boundary.
static bool testProviderFaultCoverage( sirGraphicsABIModule_t *pModule, sirGraphicsABIDevice_t *pDevice, sirMockSetMode_t pMode ) noexcept
{
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	adapter.nStableId = 77;
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	sirGraphicsABIPipelineModelCaps_t model = {};
	model.nSize = sizeof( model );
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	format.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	const sirGraphicsABIModule_t *ppModules[] = { pModule };
	sirGraphicsABIModuleSelection_t policy = {};
	policy.nSize = sizeof( policy );
	policy.nAllowSoftware = 1;
	sirGraphicsABIAdapterChoice_t choice = {};
	choice.nSize = sizeof( choice );
	choice.nModuleIndex = 88;
	pMode( 41 );
	sirGraphicsABIModule_t rejectedModule = {};
	if ( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, nullptr, &rejectedModule ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = testAlloc;
	host.pFree = testFree;
	host.pLog = testLog;
	if ( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &rejectedModule ) != SIR_GRAPHICS_ABI_VERSION_MISMATCH ||
	     rejectedModule.library.pNativeHandle != nullptr ) {
		return false;
	}
	pMode( 42 );
	if ( sirGraphicsQueryABIAdapter( pModule, 0, &adapter ) != SIR_GRAPHICS_BUSY || adapter.nStableId != 77 ||
	     sirGraphicsSelectABIModuleAdapter( ppModules, 1, &policy, &choice ) != SIR_GRAPHICS_BUSY || choice.nModuleIndex != 88 ) {
		return false;
	}
	pMode( 43 );
	if ( sirGraphicsQueryABIAdapter( pModule, 0, &adapter ) != SIR_GRAPHICS_BUSY || adapter.nStableId != 77 ||
	     sirGraphicsSelectABIModuleAdapter( ppModules, 1, &policy, &choice ) != SIR_GRAPHICS_BUSY || choice.nModuleIndex != 88 ) {
		return false;
	}
	pMode( 44 );
	if ( sirGraphicsQueryABICapabilities( pDevice, SIR_GRAPHICS_BACKEND_VULKAN, &caps ) != SIR_GRAPHICS_BUSY ||
	     sirGraphicsQueryABIPipelineModelCaps( pDevice, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &model ) != SIR_GRAPHICS_BUSY ) {
		return false;
	}
	pMode( 45 );
	if ( sirGraphicsQueryABICapabilities( pDevice, SIR_GRAPHICS_BACKEND_VULKAN, &caps ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     sirGraphicsQueryABIPipelineModelCaps( pDevice, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &model ) != SIR_GRAPHICS_DRIVER_ERROR ) {
		return false;
	}
	pMode( 48 );
	sirGraphicsABIDeviceRequest_t request = {};
	request.nSize = sizeof( request );
	request.nAdapterIndex = 0;
	request.nAllowSoftware = 1;
	request.nHeadless = 1;
	request.nFramesInFlight = 2;
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsABIDevice_t duplicate = {};
	duplicate.nSize = sizeof( duplicate );
	if ( sirGraphicsCreateABIDevice( pModule, &request, &duplicate ) != SIR_GRAPHICS_DRIVER_ERROR || pModule->nActiveDevices != 1 ||
	     duplicate.pDevice != nullptr ) {
		return false;
	}
	pMode( 50 );
	uint32_t nShader = 0x07230203;
	sirGraphicsABIModernRecipe_t computeRecipe = {};
	computeRecipe.nSize = sizeof( computeRecipe );
	computeRecipe.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	computeRecipe.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	pipeline.nSampleCount = 1;
	pipeline.computeShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &nShader, 4, 1, "main" };
	pipeline.pModern = &computeRecipe;
	sirGraphicsHandle_t hPipeline = { 99, 1, 5, SIR_GRAPHICS_HANDLE_PIPELINE };
	if ( sirGraphicsCreateABIPipeline( pDevice, &pipeline, &hPipeline ) != SIR_GRAPHICS_BUSY || hPipeline.nSlot != 99 ) {
		return false;
	}
	pMode( 51 );
	if ( sirGraphicsQueryABIFormat( pDevice, SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &format ) != SIR_GRAPHICS_BUSY ) {
		return false;
	}
	pMode( 52 );
	if ( sirGraphicsQueryABIPipelineModelCaps( pDevice, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &model ) != SIR_GRAPHICS_BUSY ||
	     sirGraphicsCreateABIPipeline( pDevice, &pipeline, &hPipeline ) != SIR_GRAPHICS_BUSY ) {
		return false;
	}
	pMode( 53 );
	sirGraphicsABIModernRecipe_t graphicsRecipe = {};
	graphicsRecipe.nSize = sizeof( graphicsRecipe );
	graphicsRecipe.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	graphicsRecipe.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.nDepthFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	pipeline.vertexShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &nShader, 4, 2, "main" };
	pipeline.pixelShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &nShader, 4, 3, "main" };
	pipeline.pModern = &graphicsRecipe;
	if ( sirGraphicsCreateABIPipeline( pDevice, &pipeline, &hPipeline ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	pMode( 61 );
	if ( sirGraphicsCreateABIPipeline( pDevice, &pipeline, &hPipeline ) != SIR_GRAPHICS_UNSUPPORTED ) {
		fputs( "Depth attachment without native depth support was admitted\n", stderr );
		return false;
	}
	pMode( 15 );
	pipeline.nDepthFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	pipeline.nSampleCount = 2;
	if ( sirGraphicsCreateABIPipeline( pDevice, &pipeline, &hPipeline ) != SIR_GRAPHICS_SUCCESS ) {
		fputs( "Two-sample pipeline admission was rejected despite declared capabilities\n", stderr );
		return false;
	}
	pMode( 56 );
	pipeline.nColorFormatCount = 0;
	pipeline.nSampleCount = 1;
	if ( sirGraphicsCreateABIPipeline( pDevice, &pipeline, &hPipeline ) != SIR_GRAPHICS_SUCCESS ) {
		fputs( "Attachment-free graphics pipeline was rejected\n", stderr );
		return false;
	}
	pMode( 10 );
	pipeline.nColorFormatCount = 0;
	pipeline.nDepthFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	pipeline.nSampleCount = 1;
	sirGraphicsBindingDesc_t crossStage = {};
	crossStage.nArrayCount = 1;
	crossStage.nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	crossStage.eType = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	pipeline.nBindingCount = 1;
	pipeline.pBindings = &crossStage;
	uint32_t iSelection = 79;
	if ( sirGraphicsValidateABIPipelineStructure( &pipeline ) != SIR_GRAPHICS_SUCCESS ||
	     sirGraphicsSelectABIPipelineVariant( pDevice, &pipeline, 1, &iSelection ) != SIR_GRAPHICS_INVALID_ARGUMENT || iSelection != 79 ) {
		fputs( "Cross-stage binding was accepted by variant selection\n", stderr );
		return false;
	}
	pMode( 0 );
	return true;
}

static bool testDeviceAdmissionFaultCoverage( sirGraphicsABIModule_t *pModule, const sirGraphicsABIDeviceRequest_t& base,
                                              sirMockSetMode_t pMode ) noexcept
{
	sirGraphicsABIDeviceRequest_t request = base;
	sirGraphicsABIDevice_t rejected = {};
	rejected.nSize = sizeof( rejected );
	pMode( 46 );
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( sirGraphicsCreateABIDevice( pModule, &request, &rejected ) != SIR_GRAPHICS_ABI_UNSUPPORTED || pModule->nActiveDevices != 0 ) {
		return false;
	}
	pMode( 47 );
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	request.nFramesInFlight = 3;
	if ( sirGraphicsCreateABIDevice( pModule, &request, &rejected ) != SIR_GRAPHICS_ABI_UNSUPPORTED || pModule->nActiveDevices != 0 ) {
		return false;
	}
	pMode( 0 );
	return true;
}

static bool testDestroyFaultCoverage( sirGraphicsABIModule_t *pModule, sirGraphicsABIDevice_t *pDevice, sirMockSetMode_t pMode ) noexcept
{
	pMode( 49 );
	const void *pIdentity = pDevice->pDevice;
	if ( sirGraphicsDestroyABIDevice( pModule, pDevice ) != SIR_GRAPHICS_DEVICE_LOST || pModule->nActiveDevices != 1 ||
	     pDevice->pDevice != pIdentity ) {
		return false;
	}
	pMode( 0 );
	return true;
}

// Rejected WSI and resource inputs must never mutate caller-owned outputs.
static bool testAdditionalAdmissionCoverage( sirGraphicsABIDevice_t *pDevice, sirMockSetMode_t pMode ) noexcept
{
	sirGraphicsABINativeSurface_t native = {};
	native.nSize = sizeof( native );
	native.nPlatform = SIR_GRAPHICS_SURFACE_X11;
	native.pWindow = &native;
	native.nWindowGeneration = 1;
	sirGraphicsABIPresentDesc_t present = {};
	present.nSize = sizeof( present );
	present.nWidth = 16;
	present.nHeight = 16;
	present.nBackbufferCount = 2;
	present.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	present.nMode = SIR_GRAPHICS_PRESENT_FIFO;
	sirGraphicsHandle_t hSurface = { 1, 1, 5, SIR_GRAPHICS_HANDLE_SURFACE };
	sirGraphicsHandle_t hInvalid = hSurface;
	hInvalid.nDeviceGeneration = 4;
	sirGraphicsSurfaceImage_t image = {};
	image.hSurface = hSurface;
	image.hImage = { 4, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE };
	image.nAcquisitionId = 1;
	image.nWindowGeneration = 1;
	sirGraphicsCompletion_t completion = { 5, SIR_GRAPHICS_QUEUE_DIRECT, 1 };
	pMode( 0 );
	if ( sirGraphicsResizeABISurface( pDevice, hInvalid, &native, &present ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirGraphicsAcquireABISurfaceImage( pDevice, hInvalid, &image ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirGraphicsAcquireABISurfaceImage( pDevice, hSurface, &image ) != SIR_GRAPHICS_UNSUPPORTED ||
	     sirGraphicsPresentABISurfaceImage( pDevice, &image, &completion ) != SIR_GRAPHICS_UNSUPPORTED ) {
		fputs( "Failed to reject headless WSI requests in the correct admission stage\n", stderr );
		return false;
	}
	pMode( 60 );
	if ( sirGraphicsCreateABISurface( pDevice, &native, &present, &hSurface ) != SIR_GRAPHICS_BUSY ) {
		fputs( "WSI provider status was not propagated\n", stderr );
		return false;
	}
	sirGraphicsViewDesc_t view = {};
	view.hResource = { 2, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE };
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	sirGraphicsHandle_t hView = { 99, 1, 5, SIR_GRAPHICS_HANDLE_VIEW };
	pMode( 51 );
	if ( sirGraphicsCreateABIView( pDevice, &view, &hView ) != SIR_GRAPHICS_BUSY || hView.nSlot != 99 ) {
		fputs( "View provider status was not propagated\n", stderr );
		return false;
	}
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 16;
	texture.nHeight = 16;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	sirGraphicsHandle_t hTexture = { 99, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE };
	pMode( 54 );
	if ( sirGraphicsCreateABITexture( pDevice, &texture, &hTexture ) != SIR_GRAPHICS_SUCCESS ||
	     hTexture.eType != SIR_GRAPHICS_HANDLE_TEXTURE ) {
		fputs( "Storage texture admission failed\n", stderr );
		return false;
	}
	view.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	if ( sirGraphicsCreateABIView( pDevice, &view, &hView ) != SIR_GRAPHICS_SUCCESS || hView.eType != SIR_GRAPHICS_HANDLE_VIEW ) {
		fputs( "Storage view failed despite enabled device and format bits\n", stderr );
		return false;
	}
	pMode( 0 );
	return true;
}

static uint32_t testCrossBackendAdapter( void *, sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	if ( pAdapter == nullptr || pAdapter->nIndex != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pAdapter = {};
	pAdapter->nSize = sizeof( *pAdapter );
	pAdapter->nBackend = SIR_GRAPHICS_BACKEND_D3D12;
	pAdapter->nKind = SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	pAdapter->nApiMajor = 12;
	pAdapter->nFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE;
	pAdapter->nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	return SIR_GRAPHICS_SUCCESS;
}

static bool testDifferentBackendRanking( const sirGraphicsABIModule_t *pLiveModule ) noexcept
{
	sirGraphicsABIModule_t synthetic = *pLiveModule;
	synthetic.driver.nBackend = SIR_GRAPHICS_BACKEND_D3D12;
	synthetic.driver.pQueryAdapter = testCrossBackendAdapter;
	const sirGraphicsABIModule_t *ppModules[] = { &synthetic, pLiveModule };
	sirGraphicsABIModuleSelection_t policy = {};
	policy.nSize = sizeof( policy );
	policy.nAllowSoftware = 1;
	policy.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsABIAdapterChoice_t choice = {};
	choice.nSize = sizeof( choice );
	return sirGraphicsSelectABIModuleAdapter( ppModules, 2, &policy, &choice ) == SIR_GRAPHICS_SUCCESS && choice.nModuleIndex == 1 &&
	       choice.adapter.nBackend == SIR_GRAPHICS_BACKEND_VULKAN;
}

static bool testMultipleDeviceRetirement( sirGraphicsABIModule_t *pModule, sirGraphicsABIDevice_t *pFirst, sirMockSetMode_t pMode ) noexcept
{
	sirGraphicsABIDeviceRequest_t request = {};
	request.nSize = sizeof( request );
	request.nAdapterIndex = 0;
	request.nAllowSoftware = 1;
	request.nHeadless = 1;
	request.nFramesInFlight = 2;
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsABIDevice_t second = {};
	second.nSize = sizeof( second );
	pMode( 62 );
	if ( sirGraphicsCreateABIDevice( pModule, &request, &second ) != SIR_GRAPHICS_SUCCESS || pModule->nActiveDevices != 2 ||
	     second.pDevice == pFirst->pDevice ) {
		return false;
	}
	if ( sirGraphicsDestroyABIDevice( pModule, &second ) != SIR_GRAPHICS_SUCCESS || pModule->nActiveDevices != 1 ||
	     second.pDevice != nullptr || pModule->szActiveDevices[0] != pFirst->pDevice ) {
		return false;
	}
	// A structurally valid foreign device must not be destroyed by this
	// module because it is absent from the ownership ledger.
	sirGraphicsABIDevice_t foreign = *pFirst;
	foreign.pDevice = &foreign;
	if ( sirGraphicsDestroyABIDevice( pModule, &foreign ) != SIR_GRAPHICS_INVALID_ARGUMENT || pModule->nActiveDevices != 1 ||
	     foreign.pDevice != &foreign ) {
		return false;
	}
	pMode( 0 );
	return true;
}

static bool testBinaryModule( void ) noexcept
{
	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = testAlloc;
	host.pFree = testFree;
	host.pLog = testLog;
	sirGraphicsABIModule_t module = {};
	if ( sirGraphicsLoadABIModule( "/not/a/real/graphics-driver", &host, &module ) != SIR_GRAPHICS_ABI_UNSUPPORTED ||
	     module.library.pNativeHandle != nullptr ) {
		return false;
	}
	// A loadable DSO with no graphics ABI factory must be rejected and unloaded.
	if ( sirGraphicsLoadABIModule( SIR_TEST_NO_ABI_MODULE_PATH, &host, &module ) != SIR_GRAPHICS_ABI_UNSUPPORTED ||
	     module.library.pNativeHandle != nullptr ) {
		return false;
	}
	if ( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &module ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return false;
	}
	if ( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &module ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	if ( !testModuleLifecyclePreflightMatrix( host, module ) ) {
		return false;
	}
	const sirMockSetMode_t pMode = ( sirMockSetMode_t )SIRDynamicLibrary::findSymbol( &module.library, "SIRSetGraphicsABIMockMode" );
	if ( pMode == nullptr ) {
		return false;
	}
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	if ( sirGraphicsQueryABIAdapter( &module, 0, &adapter ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     adapter.nKind != SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE || adapter.nBackend != SIR_GRAPHICS_BACKEND_VULKAN ) {
		return false;
	}
	pMode( 1 );
	adapter.nStableId = 0x4321;
	if ( sirGraphicsQueryABIAdapter( &module, 0, &adapter ) != SIR_GRAPHICS_ABI_DRIVER_ERROR || adapter.nStableId != 0x4321 ) {
		return false;
	}
	pMode( 9 );
	if ( sirGraphicsQueryABIAdapter( &module, 0, &adapter ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ) {
		return false;
	}
	pMode( 0 );
	if ( sirGraphicsQueryABIAdapter( &module, 1, &adapter ) != SIR_GRAPHICS_ABI_UNSUPPORTED ) {
		return false;
	}
	if ( !testAdapterRankingBoundaries( &module ) ) {
		return false;
	}
	if ( !testDifferentBackendRanking( &module ) ) {
		return false;
	}

	// ABI v1 ranks physical adapters even when the software adapter is first.
	const sirGraphicsABIModule_t *szModules[] = { &module };
	sirGraphicsABIModuleSelection_t policy = {};
	policy.nSize = sizeof( policy );
	policy.nBackend = SIR_GRAPHICS_BACKEND_AUTO;
	policy.nAllowSoftware = 1;
	policy.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsABIAdapterChoice_t choice = {};
	choice.nSize = sizeof( choice );
	choice.nModuleIndex = 99;
	pMode( 12 );
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_SUCCESS || choice.nModuleIndex != 0 ||
	     choice.adapter.nIndex != 2 || choice.adapter.nKind != SIR_GRAPHICS_ABI_ADAPTER_DISCRETE ) {
		return false;
	}
	policy.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_SUCCESS || choice.adapter.nIndex != 0 ||
	     choice.adapter.nKind != SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE ) {
		return false;
	}
	policy.nAllowSoftware = 0;
	choice.nModuleIndex = 99;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_UNSUPPORTED || choice.nModuleIndex != 99 ) {
		return false;
	}
	policy.nAllowSoftware = 1;
	policy.nBackend = SIR_GRAPHICS_BACKEND_D3D12;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_UNSUPPORTED || choice.nModuleIndex != 99 ) {
		return false;
	}
	policy.nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	policy.nMinimumApiMajor = 1;
	policy.nMinimumApiMinor = 5;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	policy.nMinimumApiMinor = 4;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	policy.nBackend = SIR_GRAPHICS_BACKEND_AUTO;
	choice.nModuleIndex = 99;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     choice.nModuleIndex != 99 ) {
		return false;
	}
	policy.nMinimumApiMajor = 0;
	policy.nMinimumApiMinor = 0;
	policy.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	pMode( 1 );
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_DRIVER_ERROR || choice.nModuleIndex != 99 ) {
		return false;
	}
	pMode( 9 );
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ) {
		return false;
	}
	pMode( 13 );
	choice.nModuleIndex = 99;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_DRIVER_ERROR || choice.nModuleIndex != 99 ) {
		return false;
	}
	pMode( 0 );
	policy.nAllowSoftware = 0;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	policy.nAllowSoftware = 1;

	sirGraphicsABIDeviceRequest_t request = {};
	request.nSize = sizeof( request );
	request.nAdapterIndex = 0;
	request.nFramesInFlight = 2;
	request.nHeadless = 1;
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_UNSUPPORTED || module.nActiveDevices != 0 ) {
		return false;
	}
	request.nAllowSoftware = 1;
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_UNSUPPORTED ) {
		return false;
	}
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	if ( !testDeviceRequestPreflightMatrix( &module, request ) ) {
		return false;
	}
	if ( !testDeviceAdmissionFaultCoverage( &module, request, pMode ) ) {
		return false;
	}
	pMode( 6 );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_OUT_OF_MEMORY || module.nActiveDevices != 0 ) {
		return false;
	}
	pMode( 7 );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_BUSY || module.nActiveDevices != 0 ) {
		return false;
	}
	pMode( 8 );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_DEVICE_LOST || module.nActiveDevices != 0 ) {
		return false;
	}
	pMode( 5 );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_DRIVER_ERROR || module.nActiveDevices != 0 ||
	     device.pDevice != nullptr ) {
		return false;
	}
	pMode( 3 );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_DRIVER_ERROR || module.nActiveDevices != 0 ) {
		return false;
	}
	pMode( 0 );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_SUCCESS || module.nActiveDevices != 1 ) {
		return false;
	}
	if ( !testProviderFaultCoverage( &module, &device, pMode ) ) {
		return false;
	}
	if ( !testAdditionalAdmissionCoverage( &device, pMode ) ) {
		return false;
	}
	if ( !testHostBoundaryInputs( &device ) ) {
		return false;
	}
	if ( !testCheckedBinarySubmissionMatrix( &device, pMode ) ) {
		return false;
	}
	if ( !testNativeResourceAdmissionMatrix( &device, pMode ) ) {
		return false;
	}
	if ( !testNativeHandleReturnContractMatrix( &device, pMode ) ) {
		return false;
	}
	if ( !testModelProviderContractMatrix( &device, pMode ) ) {
		return false;
	}
	if ( !testMemoryBudgetProviderMatrix( &device ) ) {
		return false;
	}
	if ( !testHostProviderFaults( &device ) ) {
		return false;
	}
	// Validate every mandatory native function-table slot independently.
	sirGraphicsABIDevice_t invalidDevice = device;
	invalidDevice.nSize = 1;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	invalidDevice = device;
	invalidDevice.nReserved = 1;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	invalidDevice = device;
	invalidDevice.pQueryCapabilities = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pQueryCapabilities was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pQueryFormat = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pQueryFormat was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pQueryPipelineModelCaps = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pQueryPipelineModelCaps was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pPoll = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pPoll was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pWait = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pWait was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pQueryMemoryBudget = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pQueryMemoryBudget was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pCreateBuffer = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pCreateBuffer was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pCreateTexture = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pCreateTexture was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pCreateView = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pCreateView was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pCreateSampler = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pCreateSampler was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pCreatePipeline = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pCreatePipeline was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pCreateBindingSet = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pCreateBindingSet was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pUpdateBindings = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pUpdateBindings was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pMapBuffer = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pMapBuffer was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pUnmapBuffer = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pUnmapBuffer was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pFlushBuffer = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pFlushBuffer was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pInvalidateBuffer = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pInvalidateBuffer was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pAcquireCommands = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pAcquireCommands was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pSubmit = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pSubmit was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pRetireResource = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pRetireResource was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pCreateSurface = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pCreateSurface was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pResizeSurface = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pResizeSurface was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pAcquireSurfaceImage = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pAcquireSurfaceImage was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pPresent = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pPresent was accepted\n", stderr );
		return false;
	}
	invalidDevice = device;
	invalidDevice.pShutdown = nullptr;
	if ( sirGraphicsValidateABIDevice( &invalidDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		fputs( "Missing callback pShutdown was accepted\n", stderr );
		return false;
	}
	if ( sirGraphicsValidateABIDevice( &device ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( sirGraphicsUnloadABIModule( &module ) != SIR_GRAPHICS_BUSY ) {
		return false;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	if ( sirGraphicsQueryABICapabilities( &device, SIR_GRAPHICS_BACKEND_VULKAN, &caps ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     caps.nDeviceGeneration != 5 ) {
		return false;
	}
	caps.nDeviceGeneration = 93;
	if ( sirGraphicsQueryABICapabilities( &device, SIR_GRAPHICS_BACKEND_D3D12, &caps ) != SIR_GRAPHICS_ABI_DRIVER_ERROR ||
	     caps.nDeviceGeneration != 93 ) {
		return false;
	}
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	if ( sirGraphicsQueryABIFormat( &device, SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &format ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     format.nFormat != SIR_GRAPHICS_FORMAT_RGBA8_UNORM ) {
		return false;
	}
	pMode( 2 );
	format.nFormat = 123;
	if ( sirGraphicsQueryABIFormat( &device, SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &format ) != SIR_GRAPHICS_ABI_DRIVER_ERROR ||
	     format.nFormat != 123 ) {
		return false;
	}
	pMode( 0 );
	if ( sirGraphicsQueryABIFormat( &device, SIR_GRAPHICS_FORMAT_D16_UNORM, &format ) != SIR_GRAPHICS_ABI_UNSUPPORTED ) {
		return false;
	}

	sirGraphicsABIPipelineModelCaps_t modelCaps = {};
	modelCaps.nSize = sizeof( modelCaps );
	if ( sirGraphicsQueryABIPipelineModelCaps( &device, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &modelCaps ) != SIR_GRAPHICS_SUCCESS ||
	     modelCaps.nModel != SIR_GRAPHICS_PIPELINE_MODEL_MODERN || modelCaps.nDeviceGeneration != 5 ) {
		return false;
	}
	modelCaps.nDeviceGeneration = 77;
	if ( sirGraphicsQueryABIPipelineModelCaps( &device, SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE, &modelCaps ) !=
	         SIR_GRAPHICS_UNSUPPORTED ||
	     modelCaps.nDeviceGeneration != 77 ) {
		return false;
	}
	modelCaps.nSize = 1;
	if ( sirGraphicsQueryABIPipelineModelCaps( &device, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &modelCaps ) !=
	     SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	modelCaps.nSize = sizeof( modelCaps );

	uint32_t nShaderCode = 0x07230203;
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	pipeline.nSampleCount = 1;
	pipeline.computeShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &nShaderCode, 4, 1, "main" };
	pipeline.pModern = &modern;
	sirGraphicsHandle_t hPipeline = {};
	if ( sirGraphicsCreateABIPipeline( &device, &pipeline, &hPipeline ) != SIR_GRAPHICS_SUCCESS || hPipeline.nDeviceGeneration != 5 ||
	     hPipeline.eType != SIR_GRAPHICS_HANDLE_PIPELINE ) {
		return false;
	}
	pipeline.pModern = nullptr;
	if ( sirGraphicsCreateABIPipeline( &device, &pipeline, &hPipeline ) != SIR_GRAPHICS_INVALID_ARGUMENT || hPipeline.nGeneration != 1 ) {
		return false;
	}
	pipeline.pModern = &modern;

	pMode( 10 );
	if ( sirGraphicsQueryABIPipelineModelCaps( &device, SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE, &modelCaps ) != SIR_GRAPHICS_SUCCESS ||
	     modelCaps.nExecutionModel != SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL ) {
		return false;
	}
	if ( sirGraphicsQueryABIPipelineModelCaps( &device, SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION, &modelCaps ) != SIR_GRAPHICS_SUCCESS ||
	     modelCaps.nShaderStageMask != 0 || modelCaps.nMaximumTextureStages != 4 ) {
		return false;
	}
	sirGraphicsABIProgrammableRecipe_t programmable = {};
	programmable.nSize = sizeof( programmable );
	programmable.nVertexShaderProfile = 101;
	programmable.nFragmentShaderProfile = 101;
	programmable.nTextureUnits = 1;
	programmable.nVaryings = 1;
	programmable.nConstantSlots = 1;
	sirGraphicsABIPipeline_t programmablePipeline = {};
	programmablePipeline.nSize = sizeof( programmablePipeline );
	programmablePipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	programmablePipeline.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	programmablePipeline.nSampleCount = 1;
	programmablePipeline.nColorFormatCount = 1;
	programmablePipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	programmablePipeline.vertexShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_GLSL, &nShaderCode, 4, 1, "main" };
	programmablePipeline.pixelShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_GLSL, &nShaderCode, 4, 2, "main" };
	programmablePipeline.pProgrammable = &programmable;
	if ( sirGraphicsCreateABIPipeline( &device, &programmablePipeline, &hPipeline ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	programmable.nTextureUnits = 9;
	if ( sirGraphicsCreateABIPipeline( &device, &programmablePipeline, &hPipeline ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}

	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	fixed.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM;
	fixed.nFogMode = SIR_GRAPHICS_FIXED_FOG_NONE;
	fixed.nAlphaCompare = SIR_GRAPHICS_COMPARE_ALWAYS;
	fixed.fAlphaReference = 0.5f;
	sirGraphicsABIPipeline_t fixedPipeline = {};
	fixedPipeline.nSize = sizeof( fixedPipeline );
	fixedPipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	fixedPipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	fixedPipeline.nSampleCount = 1;
	fixedPipeline.nColorFormatCount = 1;
	fixedPipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	fixedPipeline.pFixed = &fixed;
	if ( sirGraphicsCreateABIPipeline( &device, &fixedPipeline, &hPipeline ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirGraphicsABIFixedStage_t szFixedStages[5] = {};
	fixed.pStages = szFixedStages;
	fixed.nTextureStageCount = 5;
	if ( sirGraphicsCreateABIPipeline( &device, &fixedPipeline, &hPipeline ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	fixed.nTextureStageCount = 0;
	programmable.nTextureUnits = 1;

	// Author-ordered fallback prefers first eligible recipe, not API name.
	sirGraphicsABIPipeline_t szVariants[3] = { pipeline, programmablePipeline, fixedPipeline };
	uint32_t iSelected = 99;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_SUCCESS || iSelected != 0 ) {
		return false;
	}
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_RAY_TRACING;
	iSelected = 99;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_SUCCESS || iSelected != 1 ) {
		return false;
	}
	programmable.nTextureUnits = 9;
	iSelected = 99;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_SUCCESS || iSelected != 2 ) {
		return false;
	}
	fixed.nTextureStageCount = 5;
	iSelected = 99;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_UNSUPPORTED || iSelected != 99 ) {
		return false;
	}
	fixed.nTextureStageCount = 0;
	programmable.nTextureUnits = 1;
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;

	// Buffer and budget entry points must preserve the caller's handles and
	// snapshots when a provider rejects allocation or reports device loss.
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 4096;
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hBuffer = {};
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_SUCCESS || hBuffer.eType != SIR_GRAPHICS_HANDLE_BUFFER ||
	     hBuffer.nDeviceGeneration != 5 ) {
		return false;
	}
	buffer.nSizeBytes = 0;
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	buffer.nSizeBytes = 4096;
	buffer.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	buffer.nUsage = SIR_GRAPHICS_USAGE_INDIRECT;
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_UPLOAD;
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	const sirGraphicsHandle_t originalBuffer = hBuffer;
	pMode( 17 );
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_DRIVER_ERROR || hBuffer.nSlot != originalBuffer.nSlot ||
	     hBuffer.nDeviceGeneration != originalBuffer.nDeviceGeneration ) {
		return false;
	}
	pMode( 18 );
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_DEVICE_LOST || hBuffer.nSlot != originalBuffer.nSlot ||
	     hBuffer.nDeviceGeneration != originalBuffer.nDeviceGeneration ) {
		return false;
	}
	pMode( 10 );

	// Native resource creation is admitted through real format and device
	// limits before the provider is allowed to allocate GPU memory.
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 128;
	texture.nHeight = 128;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hTexture = {};
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_SUCCESS ||
	     hTexture.eType != SIR_GRAPHICS_HANDLE_TEXTURE || hTexture.nDeviceGeneration != 5 ) {
		return false;
	}
	sirGraphicsViewDesc_t view = {};
	view.hResource = hTexture;
	view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	sirGraphicsHandle_t hView = {};
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_SUCCESS || hView.eType != SIR_GRAPHICS_HANDLE_VIEW ||
	     hView.nDeviceGeneration != 5 ) {
		return false;
	}
	view.hResource.nDeviceGeneration = 4;
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	view.hResource = hTexture;
	view.nLayers = 0;
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	view.eFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	view.hResource = hBuffer;
	view.eFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	view.nMipLevels = 0;
	view.nLayers = 0;
	view.nBufferSizeBytes = 4096;
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	view.nBufferOffsetBytes = UINT64_MAX;
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	view.nBufferOffsetBytes = 0;
	view.nBufferSizeBytes = 0;
	view.hResource = hTexture;
	view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	view.nMipLevels = 1;
	view.nLayers = 1;

	sirGraphicsSamplerDesc_t sampler = {};
	sampler.fMaximumAnisotropy = 1.0f;
	sampler.fMinimumLod = 0.0f;
	sampler.fMaximumLod = 16.0f;
	sirGraphicsHandle_t hSampler = {};
	if ( sirGraphicsCreateABISampler( &device, &sampler, &hSampler ) != SIR_GRAPHICS_SUCCESS ||
	     hSampler.eType != SIR_GRAPHICS_HANDLE_SAMPLER || hSampler.nDeviceGeneration != 5 ) {
		return false;
	}
	sampler.fMaximumAnisotropy = 0.0f;
	if ( sirGraphicsCreateABISampler( &device, &sampler, &hSampler ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sampler.fMaximumAnisotropy = 1.0f;
	pMode( 17 );
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     sirGraphicsCreateABISampler( &device, &sampler, &hSampler ) != SIR_GRAPHICS_DRIVER_ERROR ) {
		return false;
	}
	pMode( 18 );
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_DEVICE_LOST ||
	     sirGraphicsCreateABISampler( &device, &sampler, &hSampler ) != SIR_GRAPHICS_DEVICE_LOST ) {
		return false;
	}
	pMode( 10 );

	// Table-driven invalid descriptors cover each host admission boundary.
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.nMipLevels = 0;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.nMipLevels=0 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.nLayers = 0;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.nLayers=0 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.nBufferOffsetBytes = 1;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.nBufferOffsetBytes=1 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.nBufferSizeBytes = 1;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.nBufferSizeBytes=1 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.nUsage = 0;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.nUsage=0 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.nUsage = SIR_GRAPHICS_USAGE_CONSTANT;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.nUsage=SIR_GRAPHICS_USAGE_CONSTANT was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.nUsage=SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_UNSUPPORTED ) {
			fputs( "Invalid sirGraphicsViewDesc_t.nUsage=SIR_GRAPHICS_USAGE_STORAGE was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.nUsage=SIR_GRAPHICS_USAGE_DEPTH_STENCIL was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.eFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.eFormat=SIR_GRAPHICS_FORMAT_UNKNOWN was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsViewDesc_t invalid = view;
		invalid.eFormat = ( sirGraphicsFormat_t )99;
		if ( sirGraphicsCreateABIView( &device, &invalid, &hView ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsViewDesc_t.eFormat=( sirGraphicsFormat_t )99 was admitted\n", stderr );
			return false;
		}
	}
	// Table-driven invalid descriptors cover each host admission boundary.
	{
		sirGraphicsTextureDesc_t invalid = texture;
		invalid.nHeight = 8192;
		if ( sirGraphicsCreateABITexture( &device, &invalid, &hTexture ) != SIR_GRAPHICS_UNSUPPORTED ) {
			fputs( "Invalid sirGraphicsTextureDesc_t.nHeight=8192 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsTextureDesc_t invalid = texture;
		invalid.nDepth = 8192;
		if ( sirGraphicsCreateABITexture( &device, &invalid, &hTexture ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsTextureDesc_t.nDepth=8192 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsTextureDesc_t invalid = texture;
		invalid.nSampleCount = 4;
		if ( sirGraphicsCreateABITexture( &device, &invalid, &hTexture ) != SIR_GRAPHICS_UNSUPPORTED ) {
			fputs( "Invalid sirGraphicsTextureDesc_t.nSampleCount=4 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsTextureDesc_t invalid = texture;
		invalid.nSampleCount = 3;
		if ( sirGraphicsCreateABITexture( &device, &invalid, &hTexture ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsTextureDesc_t.nSampleCount=3 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsTextureDesc_t invalid = texture;
		invalid.nMipLevels = 2;
		if ( sirGraphicsCreateABITexture( &device, &invalid, &hTexture ) != SIR_GRAPHICS_SUCCESS ) {
			fputs( "Invalid sirGraphicsTextureDesc_t.nMipLevels=2 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsTextureDesc_t invalid = texture;
		invalid.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
		if ( sirGraphicsCreateABITexture( &device, &invalid, &hTexture ) != SIR_GRAPHICS_UNSUPPORTED ) {
			fputs( "Invalid sirGraphicsTextureDesc_t.nUsage=SIR_GRAPHICS_USAGE_STORAGE was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsTextureDesc_t invalid = texture;
		invalid.eFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
		if ( sirGraphicsCreateABITexture( &device, &invalid, &hTexture ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsTextureDesc_t.eFormat=SIR_GRAPHICS_FORMAT_UNKNOWN was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsTextureDesc_t invalid = texture;
		invalid.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
		if ( sirGraphicsCreateABITexture( &device, &invalid, &hTexture ) != SIR_GRAPHICS_UNSUPPORTED ) {
			fputs( "Invalid sirGraphicsTextureDesc_t.eMemory=SIR_GRAPHICS_MEMORY_CPU_UPLOAD was admitted\n", stderr );
			return false;
		}
	}
	texture.nWidth = 8192;
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	texture.nWidth = 128;
	texture.nUsage = SIR_GRAPHICS_USAGE_VERTEX;
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	texture.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	texture.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	texture.eFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pMode( 14 );
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	pMode( 15 );
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	texture.nSampleCount = 2;
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	texture.nSampleCount = 1;
	pMode( 16 );
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_DRIVER_ERROR ) {
		return false;
	}
	const sirGraphicsHandle_t previousTexture = hTexture;
	pMode( 17 );
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     hTexture.nDeviceGeneration != previousTexture.nDeviceGeneration || hTexture.nSlot != previousTexture.nSlot ) {
		return false;
	}
	pMode( 18 );
	if ( sirGraphicsCreateABITexture( &device, &texture, &hTexture ) != SIR_GRAPHICS_DEVICE_LOST ||
	     hTexture.nDeviceGeneration != previousTexture.nDeviceGeneration || hTexture.nSlot != previousTexture.nSlot ) {
		return false;
	}
	pMode( 10 );

	// Pipeline formats are device-enabled facts: a valid recipe may still be
	// unsuitable for the native attachment format or sample-count mask.
	programmablePipeline.nDepthFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	if ( sirGraphicsCreateABIPipeline( &device, &programmablePipeline, &hPipeline ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	pMode( 14 );
	if ( sirGraphicsCreateABIPipeline( &device, &programmablePipeline, &hPipeline ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	pMode( 15 );
	if ( sirGraphicsCreateABIPipeline( &device, &programmablePipeline, &hPipeline ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	pMode( 16 );
	if ( sirGraphicsCreateABIPipeline( &device, &programmablePipeline, &hPipeline ) != SIR_GRAPHICS_DRIVER_ERROR ) {
		return false;
	}
	programmablePipeline.nDepthFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_RAY_TRACING;
	pMode( 14 );
	iSelected = 99;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_UNSUPPORTED || iSelected != 99 ) {
		return false;
	}
	pMode( 16 );
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_DRIVER_ERROR || iSelected != 99 ) {
		return false;
	}
	pMode( 10 );
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_SUCCESS || iSelected != 1 ) {
		return false;
	}
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;

	szVariants[0].pModern = nullptr;
	iSelected = 99;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_INVALID_ARGUMENT || iSelected != 99 ) {
		return false;
	}
	szVariants[0].pModern = &modern;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 17, &iSelected ) != SIR_GRAPHICS_CAPACITY_EXCEEDED || iSelected != 99 ) {
		return false;
	}
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 0, &iSelected ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	pMode( 0 );
	if ( sirGraphicsSelectABIPipelineVariant( &device, &szVariants[1], 2, &iSelected ) != SIR_GRAPHICS_UNSUPPORTED || iSelected != 99 ) {
		return false;
	}

	pMode( 11 );
	modelCaps.nDeviceGeneration = 77;
	if ( sirGraphicsQueryABIPipelineModelCaps( &device, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &modelCaps ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     modelCaps.nDeviceGeneration != 77 ) {
		return false;
	}
	iSelected = 99;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_DRIVER_ERROR || iSelected != 99 ) {
		return false;
	}
	pMode( 0 );

	sirGraphicsABICommandList_t commands = {};
	commands.nSize = sizeof( commands );
	if ( sirGraphicsAcquireABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &commands ) != SIR_GRAPHICS_SUCCESS ||
	     sirGraphicsValidateABICommandList( &commands ) != SIR_GRAPHICS_SUCCESS ||
	     commands.pReset( commands.pCommands ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     commands.pDispatch( commands.pCommands, 1, 1, 1 ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     commands.pDispatch( commands.pCommands, 0, 1, 1 ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ||
	     commands.pClose( commands.pCommands ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return false;
	}
	if ( sirGraphicsAcquireABICommands( &device, SIR_GRAPHICS_QUEUE_COMPUTE, &commands ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	const void *pOriginalCommands = commands.pCommands;
	sirGraphicsABICommandList_t invalidCommands = commands;
	invalidCommands.nDeviceGeneration = 0;
	if ( sirGraphicsValidateABICommandList( &invalidCommands ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	invalidCommands = commands;
	invalidCommands.nQueue = SIR_GRAPHICS_QUEUE_COUNT;
	if ( sirGraphicsValidateABICommandList( &invalidCommands ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	invalidCommands = commands;
	invalidCommands.nReserved = 1;
	if ( sirGraphicsValidateABICommandList( &invalidCommands ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	invalidCommands = commands;
	invalidCommands.pCommands = nullptr;
	if ( sirGraphicsValidateABICommandList( &invalidCommands ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	invalidCommands = commands;
	invalidCommands.nSize = 1;
	if ( sirGraphicsAcquireABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &invalidCommands ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     invalidCommands.pCommands != pOriginalCommands ) {
		return false;
	}
	pMode( 21 );
	if ( sirGraphicsAcquireABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &commands ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     commands.pCommands != pOriginalCommands ) {
		return false;
	}
	pMode( 22 );
	if ( sirGraphicsAcquireABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &commands ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     commands.pCommands != pOriginalCommands ) {
		return false;
	}
	pMode( 25 );
	if ( sirGraphicsAcquireABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &commands ) != SIR_GRAPHICS_DEVICE_LOST ||
	     commands.pCommands != pOriginalCommands ) {
		return false;
	}
	pMode( 0 );
	// Every command callback is a required ABI v1 capability-table slot.
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pReset = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pReset was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pBarrier = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pBarrier was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pCopyBuffer = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pCopyBuffer was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pCopyBufferTexture = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pCopyBufferTexture was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pBeginRenderPass = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pBeginRenderPass was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pEndRenderPass = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pEndRenderPass was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pBindPipeline = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pBindPipeline was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pBindBindingSet = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pBindBindingSet was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pBindVertexBuffer = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pBindVertexBuffer was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pBindIndexBuffer = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pBindIndexBuffer was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pSetViewport = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pSetViewport was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pDraw = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pDraw was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pDrawIndexed = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pDrawIndexed was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pDispatch = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pDispatch was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.pClose = nullptr;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Missing command callback pClose was accepted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABICommandList_t invalid = commands;
		invalid.nQueue = SIR_GRAPHICS_QUEUE_COUNT;
		if ( sirGraphicsValidateABICommandList( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	sirGraphicsABICommandList_t damaged = commands;
	damaged.pDraw = nullptr;
	if ( sirGraphicsValidateABICommandList( &damaged ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	const void *szCommandLists[] = { commands.pCommands };
	sirGraphicsABISubmission_t submission = {};
	submission.nSize = sizeof( submission );
	submission.nCommandListCount = 1;
	submission.ppCommandLists = szCommandLists;
	sirGraphicsCompletion_t completion = {};
	if ( sirGraphicsSubmitABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ) != SIR_GRAPHICS_SUCCESS ||
	     completion.nDeviceGeneration != 5 || completion.eQueue != SIR_GRAPHICS_QUEUE_DIRECT || completion.nValue == 0 ) {
		return false;
	}
	uint32_t nDone = 0;
	if ( sirGraphicsPollABICompletion( &device, &completion, &nDone ) != SIR_GRAPHICS_SUCCESS || nDone != 1 ||
	     sirGraphicsWaitABICompletion( &device, &completion, 0 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( sirGraphicsSubmitABICommands( &device, SIR_GRAPHICS_QUEUE_COMPUTE, &submission, &completion ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	const void *szDuplicates[] = { commands.pCommands, commands.pCommands };
	submission.ppCommandLists = szDuplicates;
	submission.nCommandListCount = 2;
	if ( sirGraphicsSubmitABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	submission.ppCommandLists = szCommandLists;
	submission.nCommandListCount = 1;
	const sirGraphicsSurfaceImage_t acquired = {
	    { 0, 1, 5, SIR_GRAPHICS_HANDLE_SURFACE }, { 1, 1, 5, SIR_GRAPHICS_HANDLE_TEXTURE }, 42, 1 };
	submission.pAcquisitions = &acquired;
	submission.nAcquisitionCount = 1;
	if ( sirGraphicsSubmitABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	submission.pAcquisitions = nullptr;
	submission.nAcquisitionCount = 0;
	submission.nReserved = 1;
	if ( sirGraphicsSubmitABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	submission.nReserved = 0;
	sirGraphicsCompletion_t invalidWait = completion;
	invalidWait.nDeviceGeneration = 4;
	submission.pWaits = &invalidWait;
	submission.nWaitCount = 1;
	if ( sirGraphicsSubmitABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	submission.pWaits = nullptr;
	submission.nWaitCount = 0;
	const sirGraphicsCompletion_t originalCompletion = completion;
	pMode( 23 );
	if ( sirGraphicsSubmitABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     completion.nValue != originalCompletion.nValue ) {
		return false;
	}
	pMode( 25 );
	if ( sirGraphicsSubmitABICommands( &device, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ) != SIR_GRAPHICS_DEVICE_LOST ||
	     completion.nValue != originalCompletion.nValue ) {
		return false;
	}
	pMode( 0 );
	nDone = 7;
	pMode( 24 );
	if ( sirGraphicsPollABICompletion( &device, &completion, &nDone ) != SIR_GRAPHICS_DRIVER_ERROR || nDone != 7 ) {
		return false;
	}
	pMode( 25 );
	if ( sirGraphicsPollABICompletion( &device, &completion, &nDone ) != SIR_GRAPHICS_DEVICE_LOST || nDone != 7 ||
	     sirGraphicsWaitABICompletion( &device, &completion, 0 ) != SIR_GRAPHICS_DEVICE_LOST ) {
		return false;
	}
	pMode( 0 );
	completion.nDeviceGeneration = 4;
	if ( sirGraphicsPollABICompletion( &device, &completion, &nDone ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirGraphicsWaitABICompletion( &device, &completion, 0 ) != SIR_GRAPHICS_INVALID_ARGUMENT || nDone != 7 ) {
		return false;
	}
	// WSI is explicitly device-admitted: headless compute cannot publish
	// a surface, and native surface/acquisition generations are checked.
	sirGraphicsABINativeSurface_t native = {};
	native.nSize = sizeof( native );
	native.nPlatform = SIR_GRAPHICS_SURFACE_X11;
	native.pWindow = &module;
	native.pDisplayOrInstance = &module;
	native.nWindowGeneration = 11;
	sirGraphicsABIPresentDesc_t present = {};
	present.nSize = sizeof( present );
	present.nWidth = 640;
	present.nHeight = 480;
	present.nBackbufferCount = 2;
	present.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	present.nMode = SIR_GRAPHICS_PRESENT_FIFO;
	sirGraphicsHandle_t hSurface = { 99, 1, 5, SIR_GRAPHICS_HANDLE_SURFACE };
	if ( sirGraphicsCreateABISurface( &device, &native, &present, &hSurface ) != SIR_GRAPHICS_UNSUPPORTED || hSurface.nSlot != 99 ) {
		return false;
	}
	pMode( 26 );
	if ( sirGraphicsCreateABISurface( &device, &native, &present, &hSurface ) != SIR_GRAPHICS_SUCCESS ||
	     hSurface.eType != SIR_GRAPHICS_HANDLE_SURFACE || hSurface.nSlot != 8 ) {
		return false;
	}
	if ( sirGraphicsResizeABISurface( &device, hSurface, &native, &present ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirGraphicsSurfaceImage_t surfaceImage = {};
	if ( sirGraphicsAcquireABISurfaceImage( &device, hSurface, &surfaceImage ) != SIR_GRAPHICS_SUCCESS ||
	     surfaceImage.hImage.eType != SIR_GRAPHICS_HANDLE_TEXTURE || surfaceImage.nAcquisitionId != 41 ) {
		return false;
	}
	const sirGraphicsCompletion_t presentCompletion = { 5, SIR_GRAPHICS_QUEUE_DIRECT, 1 };
	if ( sirGraphicsPresentABISurfaceImage( &device, &surfaceImage, &presentCompletion ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	// Table-driven invalid descriptors cover each host admission boundary.
	{
		sirGraphicsABINativeSurface_t invalid = native;
		invalid.nSize = 1;
		if ( sirGraphicsCreateABISurface( &device, &invalid, &present, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABINativeSurface_t.nSize=1 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABINativeSurface_t invalid = native;
		invalid.nPlatform = 0;
		if ( sirGraphicsCreateABISurface( &device, &invalid, &present, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABINativeSurface_t.nPlatform=0 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABINativeSurface_t invalid = native;
		invalid.pWindow = nullptr;
		if ( sirGraphicsCreateABISurface( &device, &invalid, &present, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABINativeSurface_t.pWindow=nullptr was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABINativeSurface_t invalid = native;
		invalid.nWindowGeneration = 0;
		if ( sirGraphicsCreateABISurface( &device, &invalid, &present, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABINativeSurface_t.nWindowGeneration=0 was admitted\n", stderr );
			return false;
		}
	}
	// Table-driven invalid descriptors cover each host admission boundary.
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nSize = 1;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nSize=1 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nWidth = 0;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nWidth=0 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nHeight = 0;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nHeight=0 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nHeight = 8192;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_UNSUPPORTED ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nHeight=8192 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nBackbufferCount = 0;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nBackbufferCount=0 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nBackbufferCount = 9;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nBackbufferCount=9 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nMode = 99;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nMode=99 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nAllowTearing = 2;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nAllowTearing=2 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nHdr = 2;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nHdr=2 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nHdr = 1;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_UNSUPPORTED ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nHdr=1 was admitted\n", stderr );
			return false;
		}
	}
	{
		sirGraphicsABIPresentDesc_t invalid = present;
		invalid.nFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
		if ( sirGraphicsCreateABISurface( &device, &native, &invalid, &hSurface ) != SIR_GRAPHICS_UNSUPPORTED ) {
			fputs( "Invalid sirGraphicsABIPresentDesc_t.nFormat=SIR_GRAPHICS_FORMAT_D16_UNORM was admitted\n", stderr );
			return false;
		}
	}
	present.nWidth = 0;
	if ( sirGraphicsResizeABISurface( &device, hSurface, &native, &present ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	present.nWidth = 640;
	present.nHdr = 1;
	if ( sirGraphicsCreateABISurface( &device, &native, &present, &hSurface ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	present.nHdr = 0;
	const sirGraphicsHandle_t validSurface = hSurface;
	const sirGraphicsSurfaceImage_t validImage = surfaceImage;
	pMode( 27 );
	if ( sirGraphicsCreateABISurface( &device, &native, &present, &hSurface ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     hSurface.nSlot != validSurface.nSlot ||
	     sirGraphicsAcquireABISurfaceImage( &device, hSurface, &surfaceImage ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     surfaceImage.nAcquisitionId != validImage.nAcquisitionId ) {
		return false;
	}
	pMode( 29 );
	if ( sirGraphicsAcquireABISurfaceImage( &device, hSurface, &surfaceImage ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     surfaceImage.nAcquisitionId != validImage.nAcquisitionId ) {
		return false;
	}
	pMode( 28 );
	if ( sirGraphicsCreateABISurface( &device, &native, &present, &hSurface ) != SIR_GRAPHICS_DEVICE_LOST ||
	     sirGraphicsResizeABISurface( &device, hSurface, &native, &present ) != SIR_GRAPHICS_DEVICE_LOST ||
	     sirGraphicsAcquireABISurfaceImage( &device, hSurface, &surfaceImage ) != SIR_GRAPHICS_DEVICE_LOST ||
	     sirGraphicsPresentABISurfaceImage( &device, &surfaceImage, &presentCompletion ) != SIR_GRAPHICS_DEVICE_LOST ) {
		return false;
	}
	pMode( 26 );
	sirGraphicsCompletion_t invalidPresent = presentCompletion;
	invalidPresent.nDeviceGeneration = 4;
	if ( sirGraphicsPresentABISurfaceImage( &device, &surfaceImage, &invalidPresent ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	pMode( 0 );

	// Deferred retirement honors only completion fences belonging to the
	// current device and one of its actually enabled queues.
	sirGraphicsCompletionSet_t retire = {};
	retire.nDeviceGeneration = 5;
	retire.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	retire.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 1;
	if ( sirGraphicsRetireABIResource( &device, hBuffer, &retire ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	retire.nDeviceGeneration = 4;
	if ( sirGraphicsRetireABIResource( &device, hBuffer, &retire ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	retire.nDeviceGeneration = 5;
	retire.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_COMPUTE;
	retire.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 0;
	retire.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE] = 1;
	if ( sirGraphicsRetireABIResource( &device, hBuffer, &retire ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	retire.nQueueMask = 0;
	retire.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE] = 0;
	pMode( 30 );
	if ( sirGraphicsRetireABIResource( &device, hBuffer, &retire ) != SIR_GRAPHICS_DEVICE_LOST ) {
		return false;
	}
	pMode( 0 );

	sirGraphicsMemoryBudget_t budget = {};
	if ( sirGraphicsQueryABIMemoryBudget( &device, &budget ) != SIR_GRAPHICS_SUCCESS || budget.nBudgetBytes != 1024U * 1024U ) {
		return false;
	}
	budget.nBudgetBytes = 12345;
	pMode( 19 );
	if ( sirGraphicsQueryABIMemoryBudget( &device, &budget ) != SIR_GRAPHICS_DRIVER_ERROR || budget.nBudgetBytes != 12345 ) {
		return false;
	}
	pMode( 20 );
	if ( sirGraphicsQueryABIMemoryBudget( &device, &budget ) != SIR_GRAPHICS_DEVICE_LOST || budget.nBudgetBytes != 12345 ) {
		return false;
	}
	pMode( 0 );
	if ( !testMultipleDeviceRetirement( &module, &device, pMode ) ) {
		return false;
	}
	if ( !testDestroyFaultCoverage( &module, &device, pMode ) ) {
		return false;
	}
	if ( sirGraphicsDestroyABIDevice( &module, &device ) != SIR_GRAPHICS_ABI_SUCCESS || device.pDevice != nullptr ||
	     module.nActiveDevices != 0 ) {
		return false;
	}
	if ( sirGraphicsDestroyABIDevice( &module, &device ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	if ( sirGraphicsUnloadABIModule( &module ) != SIR_GRAPHICS_ABI_SUCCESS || module.library.pNativeHandle != nullptr ) {
		return false;
	}
	if ( sirGraphicsUnloadABIModule( &module ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
#if defined( __linux__ )
	// The real platform loader normally succeeds, so explicitly exercise the
	// production error mapping without touching any global linker state.
	sirGraphicsABIModule_t unloadFailure = {};
	if ( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &unloadFailure ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return false;
	}
	s_bRejectNextDLUnload = true;
	if ( sirGraphicsUnloadABIModule( &unloadFailure ) != SIR_GRAPHICS_DRIVER_ERROR || unloadFailure.library.pNativeHandle == nullptr ||
	     s_bRejectNextDLUnload ) {
		return false;
	}
	// Release has already run. Close the DSO directly, avoiding a second
	// release callback when verifying the failed unload case.
	if ( __real_dlclose( unloadFailure.library.pNativeHandle ) != 0 ) {
		return false;
	}
#endif
	return true;
}

int main( void )
{
	if ( !testBinaryModule() ) {
		fputs( "graphics.abi.module FAILED\n", stderr );
		return 1;
	}
	puts( "PASS: dynamic graphics ABI module, software gating, device lifetime and malformed native responses" );
	return 0;
}
