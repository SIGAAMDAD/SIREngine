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
	if ( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &module ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return false;
	}
	if ( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &module ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
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
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_SUCCESS ||
	     choice.nModuleIndex != 0 || choice.adapter.nIndex != 2 ||
	     choice.adapter.nKind != SIR_GRAPHICS_ABI_ADAPTER_DISCRETE ) {
		return false;
	}
	policy.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_SUCCESS ||
	     choice.adapter.nIndex != 0 || choice.adapter.nKind != SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE ) {
		return false;
	}
	policy.nAllowSoftware = 0;
	choice.nModuleIndex = 99;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_UNSUPPORTED ||
	     choice.nModuleIndex != 99 ) {
		return false;
	}
	policy.nAllowSoftware = 1;
	policy.nBackend = SIR_GRAPHICS_BACKEND_D3D12;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_UNSUPPORTED ||
	     choice.nModuleIndex != 99 ) {
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
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     choice.nModuleIndex != 99 ) {
		return false;
	}
	pMode( 9 );
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ) {
		return false;
	}
	pMode( 13 );
	choice.nModuleIndex = 99;
	if ( sirGraphicsSelectABIModuleAdapter( szModules, 1, &policy, &choice ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     choice.nModuleIndex != 99 ) {
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
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_SUCCESS ||
	     hBuffer.eType != SIR_GRAPHICS_HANDLE_BUFFER || hBuffer.nDeviceGeneration != 5 ) {
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
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     hBuffer.nSlot != originalBuffer.nSlot || hBuffer.nDeviceGeneration != originalBuffer.nDeviceGeneration ) {
		return false;
	}
	pMode( 18 );
	if ( sirGraphicsCreateABIBuffer( &device, &buffer, &hBuffer ) != SIR_GRAPHICS_DEVICE_LOST ||
	     hBuffer.nSlot != originalBuffer.nSlot || hBuffer.nDeviceGeneration != originalBuffer.nDeviceGeneration ) {
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
	if ( sirGraphicsCreateABIView( &device, &view, &hView ) != SIR_GRAPHICS_SUCCESS ||
	     hView.eType != SIR_GRAPHICS_HANDLE_VIEW || hView.nDeviceGeneration != 5 ) {
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
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_UNSUPPORTED ||
	     iSelected != 99 ) {
		return false;
	}
	pMode( 16 );
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     iSelected != 99 ) {
		return false;
	}
	pMode( 10 );
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_SUCCESS ||
	     iSelected != 1 ) {
		return false;
	}
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;

	szVariants[0].pModern = nullptr;
	iSelected = 99;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     iSelected != 99 ) {
		return false;
	}
	szVariants[0].pModern = &modern;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 17, &iSelected ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ||
	     iSelected != 99 ) {
		return false;
	}
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 0, &iSelected ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	pMode( 0 );
	if ( sirGraphicsSelectABIPipelineVariant( &device, &szVariants[1], 2, &iSelected ) != SIR_GRAPHICS_UNSUPPORTED ||
	     iSelected != 99 ) {
		return false;
	}

	pMode( 11 );
	modelCaps.nDeviceGeneration = 77;
	if ( sirGraphicsQueryABIPipelineModelCaps( &device, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &modelCaps ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     modelCaps.nDeviceGeneration != 77 ) {
		return false;
	}
	iSelected = 99;
	if ( sirGraphicsSelectABIPipelineVariant( &device, szVariants, 3, &iSelected ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     iSelected != 99 ) {
		return false;
	}
	pMode( 0 );

	sirGraphicsABICommandList_t commands = {};
	commands.nSize = sizeof( commands );
	if ( device.pAcquireCommands( device.pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &commands ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     sirGraphicsValidateABICommandList( &commands ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     commands.pReset( commands.pCommands ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     commands.pDispatch( commands.pCommands, 1, 1, 1 ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     commands.pDispatch( commands.pCommands, 0, 1, 1 ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ||
	     commands.pClose( commands.pCommands ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return false;
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
	if ( device.pSubmit( device.pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &submission, &completion ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     completion.nDeviceGeneration != 5 || completion.eQueue != SIR_GRAPHICS_QUEUE_DIRECT || completion.nValue == 0 ) {
		return false;
	}
	uint32_t nDone = 0;
	if ( device.pPoll( device.pDevice, &completion, &nDone ) != SIR_GRAPHICS_ABI_SUCCESS || nDone != 1 ||
	     device.pWait( device.pDevice, &completion, 0 ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return false;
	}
	completion.nDeviceGeneration = 4;
	if ( device.pPoll( device.pDevice, &completion, &nDone ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	sirGraphicsMemoryBudget_t budget = {};
	if ( sirGraphicsQueryABIMemoryBudget( &device, &budget ) != SIR_GRAPHICS_SUCCESS ||
	     budget.nBudgetBytes != 1024U * 1024U ) {
		return false;
	}
	budget.nBudgetBytes = 12345;
	pMode( 19 );
	if ( sirGraphicsQueryABIMemoryBudget( &device, &budget ) != SIR_GRAPHICS_DRIVER_ERROR ||
	     budget.nBudgetBytes != 12345 ) {
		return false;
	}
	pMode( 20 );
	if ( sirGraphicsQueryABIMemoryBudget( &device, &budget ) != SIR_GRAPHICS_DEVICE_LOST ||
	     budget.nBudgetBytes != 12345 ) {
		return false;
	}
	pMode( 0 );
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
	return sirGraphicsUnloadABIModule( &module ) == SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
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
