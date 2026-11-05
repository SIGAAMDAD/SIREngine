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
#include <Renderer/SIRGraphicsPipelineBridge.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <stdio.h>

static uint32_t s_nAssertions = 0;
static uint32_t s_nShader = 0x07230203;

static bool expectStatus( uint32_t nActual, uint32_t nExpected, const char *pszCase ) noexcept
{
	++s_nAssertions;
	if ( nActual != nExpected ) {
		fprintf( stderr, "%s: expected %u received %u\n", pszCase, nExpected, nActual );
		return false;
	}
	return true;
}

#define REQUIRE_BAD( type, base, field, bad, function )                                                                                    \
	do {                                                                                                                                   \
		type mutated = base;                                                                                                               \
		mutated.field = bad;                                                                                                               \
		if ( !expectStatus( function( &mutated ), SIR_GRAPHICS_INVALID_ARGUMENT, #field "=" #bad ) ) {                                     \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool testAdapter( void ) noexcept
{
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	adapter.nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	adapter.nKind = SIR_GRAPHICS_ABI_ADAPTER_DISCRETE;
	adapter.nApiMajor = 1;
	adapter.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	adapter.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( !expectStatus( sirGraphicsValidateABIAdapter( &adapter ), SIR_GRAPHICS_SUCCESS, "adapter baseline" ) ) {
		return false;
	}
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nSize, 0, sirGraphicsValidateABIAdapter );
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nBackend, 0, sirGraphicsValidateABIAdapter );
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nBackend, SIR_GRAPHICS_BACKEND_OPENGL + 1, sirGraphicsValidateABIAdapter );
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nKind, 0, sirGraphicsValidateABIAdapter );
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nKind, 6, sirGraphicsValidateABIAdapter );
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nApiMajor, 0, sirGraphicsValidateABIAdapter );
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nFeatureMask, 1ULL << 63, sirGraphicsValidateABIAdapter );
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nQueueMask, 0, sirGraphicsValidateABIAdapter );
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nQueueMask, 1U << 31, sirGraphicsValidateABIAdapter );
	REQUIRE_BAD( sirGraphicsABIAdapter_t, adapter, nReserved, 1, sirGraphicsValidateABIAdapter );
	adapter.szName[127] = 'x';
	return expectStatus( sirGraphicsValidateABIAdapter( &adapter ), SIR_GRAPHICS_INVALID_ARGUMENT, "unterminated name" );
}

static sirGraphicsABICapabilities_t makeCaps( void ) noexcept
{
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 7;
	caps.nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	caps.nAdapterKind = SIR_GRAPHICS_ABI_ADAPTER_DISCRETE;
	caps.nEnabledFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE;
	caps.nEnabledQueues = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nMaximumFramesInFlight = 2;
	caps.nMaximumSamples = 1;
	caps.nMaximumTextureDimension = 4096;
	caps.nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_MODERN - 1 );
	return caps;
}

static bool testCapabilities( void ) noexcept
{
	const sirGraphicsABICapabilities_t caps = makeCaps();
	if ( !expectStatus( sirGraphicsValidateABICapabilities( &caps ), SIR_GRAPHICS_SUCCESS, "device caps baseline" ) ) {
		return false;
	}
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nSize, 1, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nDeviceGeneration, 0, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nBackend, 0, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nBackend, SIR_GRAPHICS_BACKEND_OPENGL + 1, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nAdapterKind, 0, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nAdapterKind, 6, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledQueues, 0, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledQueues, 1U << 31, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nMaximumFramesInFlight, 0, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nMaximumSamples, 0, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nUnifiedMemory, 2, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nReserved, 1, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledFeatures, 1ULL << 63, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nPipelineModelMask, 0, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nPipelineModelMask, 1U << 31, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledFeatures, SIR_GRAPHICS_FEATURE_FIXED_FUNCTION,
	             sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nPipelineModelMask, 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION - 1 ),
	             sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledQueues, 1U << SIR_GRAPHICS_QUEUE_COPY, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledFeatures, SIR_GRAPHICS_FEATURE_PRESENT, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledFeatures, SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE,
	             sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledFeatures, SIR_GRAPHICS_FEATURE_COPY_QUEUE,
	             sirGraphicsValidateABICapabilities );
	sirGraphicsABICapabilities_t compute = caps;
	compute.nEnabledFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	compute.nEnabledQueues = 1U << SIR_GRAPHICS_QUEUE_COPY;
	return expectStatus( sirGraphicsValidateABICapabilities( &compute ), SIR_GRAPHICS_INVALID_ARGUMENT, "compute on copy-only queue" );
}

static bool testFormat( void ) noexcept
{
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	format.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	format.nOptimalFeatures = SIR_GRAPHICS_ABI_FORMAT_SAMPLED;
	format.nSampleCountMask = 1;
	if ( !expectStatus( sirGraphicsValidateABIFormat( &format ), SIR_GRAPHICS_SUCCESS, "format baseline" ) ) {
		return false;
	}
	REQUIRE_BAD( sirGraphicsABIFormat_t, format, nSize, 0, sirGraphicsValidateABIFormat );
	REQUIRE_BAD( sirGraphicsABIFormat_t, format, nFormat, 0, sirGraphicsValidateABIFormat );
	REQUIRE_BAD( sirGraphicsABIFormat_t, format, nFormat, SIR_GRAPHICS_FORMAT_BC7_UNORM + 1, sirGraphicsValidateABIFormat );
	REQUIRE_BAD( sirGraphicsABIFormat_t, format, nOptimalFeatures, 1U << 31, sirGraphicsValidateABIFormat );
	REQUIRE_BAD( sirGraphicsABIFormat_t, format, nLinearFeatures, 1U << 31, sirGraphicsValidateABIFormat );
	REQUIRE_BAD( sirGraphicsABIFormat_t, format, nSampleCountMask, 1U << 31, sirGraphicsValidateABIFormat );
	REQUIRE_BAD( sirGraphicsABIFormat_t, format, nReserved, 1, sirGraphicsValidateABIFormat );
	REQUIRE_BAD( sirGraphicsABIFormat_t, format, nSampleCountMask, 0, sirGraphicsValidateABIFormat );
	format.nOptimalFeatures = 0;
	format.nSampleCountMask = 0;
	return expectStatus( sirGraphicsValidateABIFormat( &format ), SIR_GRAPHICS_SUCCESS, "unsupported valid format" );
}

static sirGraphicsABIPipelineModelCaps_t makeModelCaps( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 7;
	caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT | SIR_GRAPHICS_STAGE_COMPUTE;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE;
	caps.nMaximumSamples = 1;
	caps.nMaximumColorAttachments = 1;
	caps.nMaximumTextureDimension = 4096;
	return caps;
}

static bool testModelCaps( void ) noexcept
{
	const sirGraphicsABIPipelineModelCaps_t caps = makeModelCaps();
	if ( !expectStatus( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_SUCCESS, "model caps baseline" ) ) {
		return false;
	}
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nSize, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nDeviceGeneration, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nModel, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nReserved, 1, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nExecutionModel, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nExecutionModel, 4, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nQueueMask, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nQueueMask, 1U << 31, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nShaderStageMask, 1U << 31, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nShaderFormatMask, 1U << 31, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nMaximumSamples, 3, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nMaximumColorAttachments, 9, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nMaximumTextureDimension, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nSupportsNPOT, 2, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nFixedFeatureMask, 1U << 31, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nFixedTextureOperationMask, 1U << 31, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nFixedFogModeMask, 1U << 31, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nFixedFeatureMask, SIR_GRAPHICS_FIXED_TRANSFORM,
	             sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nMaximumFixedLights, 1, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nShaderStageMask, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nShaderFormatMask, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nShaderStageMask, SIR_GRAPHICS_STAGE_FRAGMENT,
	             sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nRequiredFeatureMask, SIR_GRAPHICS_FEATURE_COMPUTE,
	             sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nRequiredFeatureMask, SIR_GRAPHICS_FEATURE_GRAPHICS,
	             sirGraphicsValidateABIPipelineModelCaps );
	return true;
}

static bool testSourcePipeline( void ) noexcept
{
	sirGraphicsABIModernRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	sirGraphicsPipelineDesc_t source = {};
	source.eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	source.eType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	source.pModern = &recipe;
	source.nSampleCount = 1;
	source.vertexShader = { SIR_GRAPHICS_SHADER_SPIRV, &s_nShader, 4, 1, "main" };
	source.pixelShader = { SIR_GRAPHICS_SHADER_SPIRV, &s_nShader, 4, 2, "main" };
	if ( !expectStatus( sirGraphicsValidatePipeline( source ), SIR_GRAPHICS_SUCCESS, "source pipeline baseline" ) ) {
		return false;
	}
	sirGraphicsABIPipeline_t encoded = {};
	if ( !expectStatus( sirGraphicsEncodeABIPipeline( source, nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT, "bridge null output" ) ||
	     !expectStatus( sirGraphicsEncodeABIPipeline( source, &encoded ), SIR_GRAPHICS_SUCCESS, "bridge valid output" ) ) {
		return false;
	}
	sirGraphicsPipelineDesc_t test = source;
	test.nBindingCount = 1;
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source missing bindings" ) ) {
		return false;
	}
	test = source;
	test.nAttributeCount = 1;
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source missing attributes" ) ) {
		return false;
	}
	test = source;
	test.nSampleCount = 3;
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source invalid samples" ) ) {
		return false;
	}
	test = source;
	test.nColorFormatCount = 1;
	test.szColorFormats[0] = SIR_GRAPHICS_FORMAT_UNKNOWN;
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source unknown color format" ) ) {
		return false;
	}
	test = source;
	test.eTopology = ( sirGraphicsTopology_t )( SIR_GRAPHICS_TOPOLOGY_PATCHES + 1 );
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source invalid topology" ) ) {
		return false;
	}
	test = source;
	test.eCullMode = ( sirGraphicsCullMode_t )( SIR_GRAPHICS_CULL_BACK + 1 );
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source invalid culling" ) ) {
		return false;
	}
	test = source;
	test.eDepthCompare = ( sirGraphicsCompareOp_t )( SIR_GRAPHICS_COMPARE_ALWAYS + 1 );
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source invalid depth comparison" ) ) {
		return false;
	}
	sirGraphicsBindingDesc_t binding = {};
	binding.eType = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	binding.nArrayCount = 1;
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	test = source;
	test.nBindingCount = 1;
	test.pBindings = &binding;
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_SUCCESS, "source valid binding" ) ) {
		return false;
	}
	binding.nArrayCount = 0;
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source zero binding array" ) ) {
		return false;
	}
	binding.nArrayCount = 1;
	binding.nShaderStageMask = 1U << 31;
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source invalid stage" ) ) {
		return false;
	}
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	binding.eType = ( sirGraphicsBindingType_t )( SIR_GRAPHICS_BINDING_SAMPLER + 1 );
	return expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source invalid binding type" );
}

static bool testSourcePipelineEdges( void ) noexcept
{
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	sirGraphicsABIProgrammableRecipe_t programmable = {};
	programmable.nSize = sizeof( programmable );
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	sirGraphicsABIFixedStage_t stage = {};
	sirGraphicsABIFixedLight_t light = {};
	sirGraphicsPipelineDesc_t base = {};
	base.eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	base.eType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	base.pModern = &modern;
	base.nSampleCount = 1;
	base.vertexShader = { SIR_GRAPHICS_SHADER_SPIRV, &s_nShader, 4, 1, "main" };
	base.pixelShader = { SIR_GRAPHICS_SHADER_SPIRV, &s_nShader, 4, 2, "main" };
	sirGraphicsPipelineDesc_t test = base;
#define CHECK_SOURCE( expected, label )                                                                                                    \
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), expected, label ) )                                                           \
	return false
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "modern graphics" );
	test.pModern = nullptr;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern recipe missing" );
	test = base;
	test.pProgrammable = &programmable;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern and programmable tags" );
	test = base;
	test.pFixed = &fixed;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern and fixed tags" );
	test = base;
	modern.nSize = 1;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern undersized" );
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern wrong stages" );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	test.eType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern fixed type" );
	test = base;
	test.eType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern compute wrong recipe" );
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern compute missing shader" );
	test.vertexShader = {};
	test.pixelShader = {};
	test.computeShader = { SIR_GRAPHICS_SHADER_SPIRV, &s_nShader, 4, 3, "main" };
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "modern compute valid" );
	test.computeShader.nByteCount = 3;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern compute short shader" );
	test.computeShader.nByteCount = 4;
	test.vertexShader = base.vertexShader;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern compute has vertex shader" );
	test.vertexShader = {};
	test.pixelShader = base.pixelShader;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern compute has fragment shader" );
	test.pixelShader = {};
	test.nColorFormatCount = 1;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern compute has color attachment" );
	test.nColorFormatCount = 0;
	test.nAttributeCount = 1;
	sirGraphicsVertexAttribute_t attribute = {};
	test.pAttributes = &attribute;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern compute has vertex attribute" );
	test.nAttributeCount = 0;
	test.nSampleCount = 2;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern compute uses MSAA" );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	test = base;
	test.vertexShader.eFormat = SIR_GRAPHICS_SHADER_FIXED_FUNCTION;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern invalid vertex shader format" );
	test = base;
	test.pixelShader.pBytes = nullptr;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern missing pixel bytes" );
	test = base;
	test.pixelShader.eFormat = SIR_GRAPHICS_SHADER_DXBC;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern mixed shader formats" );
	test = base;
	test.computeShader = { SIR_GRAPHICS_SHADER_SPIRV, &s_nShader, 4, 3, "main" };
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "modern graphics has compute shader" );

	test = base;
	test.eModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	test.pModern = nullptr;
	test.pProgrammable = &programmable;
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "programmable graphics" );
	test.pModern = &modern;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable modern collision" );
	test.pModern = nullptr;
	test.pProgrammable = nullptr;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable missing recipe" );
	test.pProgrammable = &programmable;
	programmable.nSize = 1;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable small recipe" );
	programmable.nSize = sizeof( programmable );
	programmable.nReserved = 1;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable reserved" );
	programmable.nReserved = 0;
	test.eType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable compute forbidden" );
	test = base;
	test.eModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	test.eType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	test.pModern = nullptr;
	test.pFixed = &fixed;
	test.vertexShader = {};
	test.pixelShader = {};
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "fixed shaderless" );
	test.pModern = &modern;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed modern collision" );
	test.pModern = nullptr;
	fixed.nSize = 1;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed short recipe" );
	fixed.nSize = sizeof( fixed );
	fixed.nReserved = 1;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed reserved" );
	fixed.nReserved = 0;
	test.vertexShader = base.vertexShader;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed vertex shader forbidden" );
	test.vertexShader = {};
	test.pixelShader = base.pixelShader;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed fragment shader forbidden" );
	test.pixelShader = {};
	test.computeShader = base.vertexShader;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed compute shader forbidden" );
	test.computeShader = {};
	fixed.nTextureStageCount = 1;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed missing stages" );
	fixed.pStages = &stage;
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "fixed supplied stages" );
	fixed.nLightCount = 1;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed missing lights" );
	fixed.pLights = &light;
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "fixed supplied lights" );
	test.nBindingCount = 1;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed binding forbidden" );
	fixed.nTextureStageCount = 0;
	fixed.nLightCount = 0;
	test = base;
	test.eModel = ( sirGraphicsPipelineModel_t )99;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "unknown pipeline era" );
#undef CHECK_SOURCE
	return true;
}

static bool testSourcePipelineBounds( void ) noexcept
{
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	sirGraphicsPipelineDesc_t base = {};
	base.eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	base.eType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	base.pModern = &modern;
	base.nSampleCount = 1;
	base.vertexShader = { SIR_GRAPHICS_SHADER_SPIRV, &s_nShader, 4, 1, "main" };
	base.pixelShader = { SIR_GRAPHICS_SHADER_SPIRV, &s_nShader, 4, 2, "main" };
	sirGraphicsPipelineDesc_t test = base;
#define CHECK_SOURCE( expected, label )                                                                                                    \
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), expected, label ) )                                                           \
	return false
	test.nColorFormatCount = 9;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "color count over eight" );
	test = base;
	sirGraphicsBindingDesc_t binding = {};
	binding.nArrayCount = 1;
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	test.pBindings = &binding;
	test.nBindingCount = 4097;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "binding count exceeds fixed boundary" );
	test.nBindingCount = 1;
	binding.nShaderStageMask = 0;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "binding zero stages" );
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	binding.nArrayCount = 0;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "binding zero array" );
	binding.nArrayCount = 1;
	binding.eType = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "binding admitted" );
	test = base;
	sirGraphicsVertexAttribute_t attribute = {};
	test.pAttributes = &attribute;
	test.nAttributeCount = 33;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "vertex attributes over 32" );
	test = base;
	test.nSampleCount = 0;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "zero samples" );
	test.nSampleCount = 3;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "non-power sample count" );
	test.nSampleCount = 4;
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "power two sample count" );
	test = base;
	test.nColorFormatCount = 1;
	test.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	test.szBlendStates[0].bEnabled = true;
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "blending valid" );
	test.szBlendStates[0].eSourceColor = ( sirGraphicsBlendFactor_t )99;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "blend source color" );
	test.szBlendStates[0].eSourceColor = SIR_GRAPHICS_BLEND_ONE;
	test.szBlendStates[0].eDestinationColor = ( sirGraphicsBlendFactor_t )99;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "blend destination color" );
	test.szBlendStates[0].eDestinationColor = SIR_GRAPHICS_BLEND_ONE;
	test.szBlendStates[0].eSourceAlpha = ( sirGraphicsBlendFactor_t )99;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "blend source alpha" );
	test.szBlendStates[0].eSourceAlpha = SIR_GRAPHICS_BLEND_ONE;
	test.szBlendStates[0].eDestinationAlpha = ( sirGraphicsBlendFactor_t )99;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "blend destination alpha" );
	test.szBlendStates[0].eDestinationAlpha = SIR_GRAPHICS_BLEND_ONE;
	test.szBlendStates[0].eColorOperation = ( sirGraphicsBlendOp_t )99;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "blend color op" );
	test.szBlendStates[0].eColorOperation = SIR_GRAPHICS_BLEND_ADD;
	test.szBlendStates[0].eAlphaOperation = ( sirGraphicsBlendOp_t )99;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "blend alpha op" );
	test.szBlendStates[0].eAlphaOperation = SIR_GRAPHICS_BLEND_ADD;
	test.szBlendStates[0].nColorWriteMask = 16;
	CHECK_SOURCE( SIR_GRAPHICS_INVALID_ARGUMENT, "blend channel mask" );
	test.szBlendStates[0].nColorWriteMask = 15;
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "valid 4-bit blend mask" );
	test.szBlendStates[0].bEnabled = false;
	test.szBlendStates[0].eSourceColor = ( sirGraphicsBlendFactor_t )99;
	CHECK_SOURCE( SIR_GRAPHICS_SUCCESS, "disabled blend does not require active factors" );
#undef CHECK_SOURCE
	return true;
}

static bool testResourceAndCompletionEdges( void ) noexcept
{
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 1;
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	if ( !expectStatus( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_SUCCESS, "buffer base" ) )
		return false;
	buffer.nUsage = ( 1U << 31 );
	if ( !expectStatus( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_INVALID_ARGUMENT, "buffer invalid usage" ) )
		return false;
	buffer.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	if ( !expectStatus( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_INVALID_ARGUMENT, "buffer depth usage" ) )
		return false;
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = ( sirGraphicsMemoryClass_t )( SIR_GRAPHICS_MEMORY_TRANSIENT + 1 );
	if ( !expectStatus( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_INVALID_ARGUMENT, "buffer bad memory" ) )
		return false;
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 16;
	texture.nHeight = 16;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_SUCCESS, "valid texture" ) )
		return false;
	texture.nWidth = 0;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero width" ) )
		return false;
	texture.nWidth = 16;
	texture.nHeight = 0;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero height" ) )
		return false;
	texture.nHeight = 16;
	texture.nDepth = 0;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero depth" ) )
		return false;
	texture.nDepth = 1;
	texture.nArrayLayers = 0;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero layers" ) )
		return false;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 0;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero mips" ) )
		return false;
	texture.nMipLevels = 1;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_1D;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "1D texture non-unit height" ) )
		return false;
	texture.nHeight = 1;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_SUCCESS, "1D texture valid" ) )
		return false;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_3D;
	texture.nArrayLayers = 2;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "3D array invalid" ) )
		return false;
	texture.nArrayLayers = 1;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_SUCCESS, "3D valid" ) )
		return false;
	texture.eDimension = ( sirGraphicsTextureDimension_t )0;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "invalid dimension low" ) )
		return false;
	texture.eDimension = ( sirGraphicsTextureDimension_t )99;
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "invalid dimension high" ) )
		return false;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eMemory = ( sirGraphicsMemoryClass_t )( SIR_GRAPHICS_MEMORY_TRANSIENT + 1 );
	if ( !expectStatus( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "texture invalid memory class" ) )
		return false;
	sirGraphicsSamplerDesc_t sampler = {};
	sampler.fMaximumAnisotropy = 1;
	sampler.fMaximumLod = 4;
	if ( !expectStatus( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_SUCCESS, "sampler base" ) )
		return false;
	sampler.eFilter = ( sirGraphicsSamplerFilter_t )99;
	if ( !expectStatus( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_INVALID_ARGUMENT, "sampler bad filter" ) )
		return false;
	sampler.eFilter = SIR_GRAPHICS_FILTER_LINEAR;
	sampler.eAddressU = ( sirGraphicsAddressMode_t )99;
	if ( !expectStatus( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_INVALID_ARGUMENT, "sampler bad U" ) )
		return false;
	sampler.eAddressU = SIR_GRAPHICS_ADDRESS_REPEAT;
	sampler.eAddressV = ( sirGraphicsAddressMode_t )99;
	if ( !expectStatus( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_INVALID_ARGUMENT, "sampler bad V" ) )
		return false;
	sampler.eAddressV = SIR_GRAPHICS_ADDRESS_REPEAT;
	sampler.eAddressW = ( sirGraphicsAddressMode_t )99;
	if ( !expectStatus( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_INVALID_ARGUMENT, "sampler bad W" ) )
		return false;
	sirGraphicsHandle_t handle = { 9, 1, 7, SIR_GRAPHICS_HANDLE_TEXTURE };
	if ( !expectStatus( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_TEXTURE, 7 ) ? 1U : 0U, 1, "valid texture handle" ) )
		return false;
	handle.eType = SIR_GRAPHICS_HANDLE_NONE;
	if ( !expectStatus( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_NONE, 7 ) ? 1U : 0U, 0, "invalid none handle type" ) )
		return false;
	handle.eType = ( sirGraphicsHandleType_t )99;
	if ( !expectStatus( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_NONE, 7 ) ? 1U : 0U, 0, "invalid high handle type" ) )
		return false;
	sirGraphicsCompletionSet_t fences = {};
	fences.nDeviceGeneration = 7;
	fences.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	fences.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 10;
	if ( !expectStatus( sirGraphicsIsCompletionSetValid( fences, 7 ) ? 1U : 0U, 1, "valid fences" ) )
		return false;
	fences.nQueueMask = 1U << 31;
	if ( !expectStatus( sirGraphicsIsCompletionSetValid( fences, 7 ) ? 1U : 0U, 0, "invalid queue bit" ) )
		return false;
	fences.nQueueMask = 0;
	fences.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 0;
	if ( !expectStatus( sirGraphicsIsCompletionSetValid( fences, 7 ) ? 1U : 0U, 1, "valid empty fence set" ) )
		return false;
	return true;
}

int main( void )
{
	if ( !testAdapter() || !testCapabilities() || !testFormat() || !testModelCaps() || !testSourcePipeline() ||
	     !testSourcePipelineEdges() || !testSourcePipelineBounds() || !testResourceAndCompletionEdges() ) {
		return 1;
	}
	printf( "PASS: %u binary/source ABI negative-matrix assertions\n", s_nAssertions );
	return 0;
}
