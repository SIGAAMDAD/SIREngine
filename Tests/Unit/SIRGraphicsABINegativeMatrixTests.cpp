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

#define REQUIRE_BAD( type, base, field, bad, function )                            \
	do {                                                                          \
		type mutated = base;                                                      \
		mutated.field = bad;                                                      \
		if ( !expectStatus( function( &mutated ), SIR_GRAPHICS_INVALID_ARGUMENT,   \
		                    #field "=" #bad ) ) {                                 \
			return false;                                                         \
		}                                                                         \
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
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledFeatures, SIR_GRAPHICS_FEATURE_FIXED_FUNCTION, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nPipelineModelMask, 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION - 1 ), sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledQueues, 1U << SIR_GRAPHICS_QUEUE_COPY, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledFeatures, SIR_GRAPHICS_FEATURE_PRESENT, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledFeatures, SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE, sirGraphicsValidateABICapabilities );
	REQUIRE_BAD( sirGraphicsABICapabilities_t, caps, nEnabledFeatures, SIR_GRAPHICS_FEATURE_COPY_QUEUE, sirGraphicsValidateABICapabilities );
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
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nFixedFeatureMask, SIR_GRAPHICS_FIXED_TRANSFORM, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nMaximumFixedLights, 1, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nShaderStageMask, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nShaderFormatMask, 0, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nShaderStageMask, SIR_GRAPHICS_STAGE_FRAGMENT, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nRequiredFeatureMask, SIR_GRAPHICS_FEATURE_COMPUTE, sirGraphicsValidateABIPipelineModelCaps );
	REQUIRE_BAD( sirGraphicsABIPipelineModelCaps_t, caps, nRequiredFeatureMask, SIR_GRAPHICS_FEATURE_GRAPHICS, sirGraphicsValidateABIPipelineModelCaps );
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
	test.nTopology = ( sirGraphicsTopology_t )( SIR_GRAPHICS_TOPOLOGY_PATCHES + 1 );
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source invalid topology" ) ) {
		return false;
	}
	test = source;
	test.nCullMode = ( sirGraphicsCullMode_t )( SIR_GRAPHICS_CULL_BACK + 1 );
	if ( !expectStatus( sirGraphicsValidatePipeline( test ), SIR_GRAPHICS_INVALID_ARGUMENT, "source invalid culling" ) ) {
		return false;
	}
	test = source;
	test.nDepthCompare = ( sirGraphicsCompareOp_t )( SIR_GRAPHICS_COMPARE_ALWAYS + 1 );
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

int main( void )
{
	if ( !testAdapter() || !testCapabilities() || !testFormat() || !testModelCaps() || !testSourcePipeline() ) {
		return 1;
	}
	printf( "PASS: %u binary/source ABI negative-matrix assertions\n", s_nAssertions );
	return 0;
}
