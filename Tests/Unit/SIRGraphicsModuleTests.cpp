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
	if ( device.pQueryMemoryBudget( device.pDevice, &budget ) != SIR_GRAPHICS_ABI_SUCCESS || budget.nBudgetBytes != 1024U * 1024U ) {
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
