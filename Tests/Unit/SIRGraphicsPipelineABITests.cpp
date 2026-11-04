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
#include <stdio.h>

static uint32_t s_nMockShader = 0x07230203;

static bool checkResult( uint32_t nObserved, uint32_t nExpected, const char *pszLabel ) noexcept
{
	if ( nObserved != nExpected ) {
		fprintf( stderr, "%s: expected %u, received %u\n", pszLabel, nExpected, nObserved );
		return false;
	}
	return true;
}

static sirGraphicsABIPipelineModelCaps_t makeModernCaps( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 5;
	caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT | SIR_GRAPHICS_STAGE_COMPUTE;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
	caps.nMaximumSamples = 4;
	caps.nMaximumColorAttachments = 4;
	caps.nMaximumTextureDimension = 4096;
	caps.nMaximumBindings = 16;
	caps.nMaximumVertexAttributes = 16;
	caps.nMaximumVertexStreams = 4;
	caps.nMaximumTextureUnits = 16;
	caps.nSupportsNPOT = 1;
	return caps;
}

static sirGraphicsABIPipeline_t makeModernPipeline( sirGraphicsABIModernRecipe_t *pRecipe ) noexcept
{
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	pipeline.nTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.vertexShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &s_nMockShader, 4, 1, "main" };
	pipeline.pixelShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &s_nMockShader, 4, 2, "main" };
	pipeline.pModern = pRecipe;
	return pipeline;
}

static bool testModern( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = makeModernCaps();
	sirGraphicsABIModernRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	recipe.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	sirGraphicsABIPipeline_t pipeline = makeModernPipeline( &recipe );

	if ( !checkResult( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_SUCCESS, "modern caps" ) ) {
		return false;
	}
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "modern graphics" ) ) {
		return false;
	}
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "stateful modern GL" ) ) {
		return false;
	}
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "wrong model" ) ) {
		return false;
	}
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	pipeline.nSize = 1;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "undersized pipeline" ) ) {
		return false;
	}
	pipeline.nSize = sizeof( pipeline );
	pipeline.vertexShader.nFormat = SIR_GRAPHICS_SHADER_GLSL;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "unsupported format" ) ) {
		return false;
	}
	pipeline.vertexShader.nFormat = SIR_GRAPHICS_SHADER_SPIRV;
	pipeline.nBindingCount = 17;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "binding limit" ) ) {
		return false;
	}
	pipeline.nBindingCount = 0;
	sirGraphicsBindingDesc_t binding = {};
	binding.nBinding = 0;
	binding.nArrayCount = 1;
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	binding.eType = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	pipeline.pBindings = &binding;
	pipeline.nBindingCount = 1;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "valid binding" ) ) {
		return false;
	}
	binding.nBinding = 16;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "binding index limit" ) ) {
		return false;
	}
	binding.nBinding = 0;
	binding.nArrayCount = 20;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "binding array limit" ) ) {
		return false;
	}
	binding.nArrayCount = 1;
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "graphics stage mismatch" ) ) {
		return false;
	}
	pipeline.nBindingCount = 0;
	pipeline.pBindings = nullptr;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_D16_UNORM;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "depth-as-color" ) ) {
		return false;
	}
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.szBlendStates[0].nEnabled = 1;
	pipeline.szBlendStates[0].nSourceColor = 99;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "invalid blend" ) ) {
		return false;
	}
	pipeline.szBlendStates[0] = {};
	sirGraphicsVertexAttribute_t attribute = {};
	attribute.nLocation = 16;
	attribute.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.pAttributes = &attribute;
	pipeline.nAttributeCount = 1;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "vertex attribute slot" ) ) {
		return false;
	}
	pipeline.nAttributeCount = 0;
	pipeline.pAttributes = nullptr;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	pipeline.nColorFormatCount = 0;
	pipeline.vertexShader = {};
	pipeline.pixelShader = {};
	pipeline.computeShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &s_nMockShader, 4, 3, "main" };
	recipe.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "modern compute" ) ) {
		return false;
	}
	caps.nShaderStageMask &= ~SIR_GRAPHICS_STAGE_COMPUTE;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "missing compute" ) ) {
		return false;
	}
	caps = makeModernCaps();
	caps.nQueueMask = 1U << 8;
	return checkResult( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "invalid queue mask" );
}

static bool testProgrammable( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = makeModernCaps();
	caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_GLSL - 1 );
	caps.nMaximumTextureUnits = 4;
	caps.nMaximumVaryings = 8;
	caps.nMaximumConstantSlots = 32;
	caps.nMaximumVertexShaderProfile = 101;
	caps.nMaximumFragmentShaderProfile = 101;

	sirGraphicsABIProgrammableRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nVertexShaderProfile = 101;
	recipe.nFragmentShaderProfile = 101;
	recipe.nTextureUnits = 4;
	recipe.nVaryings = 8;
	recipe.nConstantSlots = 32;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	pipeline.vertexShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_GLSL, &s_nMockShader, 4, 1, "main" };
	pipeline.pixelShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_GLSL, &s_nMockShader, 4, 2, "main" };
	pipeline.pProgrammable = &recipe;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "programmable graphics" ) ) {
		return false;
	}
	recipe.nTextureUnits = 5;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "texture slot overflow" ) ) {
		return false;
	}
	recipe.nTextureUnits = 4;
	recipe.nVertexShaderProfile = 102;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "shader profile" ) ) {
		return false;
	}
	recipe.nVertexShaderProfile = 101;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "programmable compute" ) ) {
		return false;
	}
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	return checkResult( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "missing fragment" );
}

static bool testFixed( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 5;
	caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	caps.nMaximumSamples = 1;
	caps.nMaximumColorAttachments = 1;
	caps.nMaximumTextureDimension = 2048;
	caps.nMaximumTextureUnits = 2;
	caps.nMaximumTextureStages = 2;
	caps.nMaximumFixedLights = 1;
	caps.nMaximumVertexStreams = 2;
	caps.nMaximumVertexAttributes = 8;
	caps.nFixedFeatureMask = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING | SIR_GRAPHICS_FIXED_FOG;
	caps.nFixedTextureOperationMask = ( 1U << SIR_GRAPHICS_FIXED_OP_SELECT ) | ( 1U << SIR_GRAPHICS_FIXED_OP_MODULATE );
	caps.nFixedFogModeMask = ( 1U << SIR_GRAPHICS_FIXED_FOG_NONE ) | ( 1U << SIR_GRAPHICS_FIXED_FOG_LINEAR );
	sirGraphicsABIFixedStage_t stage = {};
	stage.nTextureUnit = 0;
	stage.nCoordinateSet = 0;
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_MODULATE;
	stage.nAlphaOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	sirGraphicsABIFixedLight_t light = {};
	sirGraphicsABIFixedRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING;
	recipe.nTextureStageCount = 1;
	recipe.nLightCount = 1;
	recipe.pStages = &stage;
	recipe.pLights = &light;
	recipe.nFogMode = SIR_GRAPHICS_FIXED_FOG_NONE;
	recipe.nAlphaCompare = SIR_GRAPHICS_COMPARE_ALWAYS;
	recipe.fAlphaReference = 0.5f;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.pFixed = &recipe;

	if ( !checkResult( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_SUCCESS, "fixed caps" ) ) {
		return false;
	}
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "fixed graphics" ) ) {
		return false;
	}
	recipe.nTextureStageCount = 3;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "stage overflow" ) ) {
		return false;
	}
	recipe.nTextureStageCount = 1;
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_DOT3;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "stage operation" ) ) {
		return false;
	}
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_MODULATE;
	stage.nColorArgument0 = 100;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "texture argument" ) ) {
		return false;
	}
	stage.nColorArgument0 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	recipe.nFogMode = SIR_GRAPHICS_FIXED_FOG_EXP2;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "fog limit" ) ) {
		return false;
	}
	recipe.nFogMode = SIR_GRAPHICS_FIXED_FOG_NONE;
	pipeline.computeShader.pBytes = &s_nMockShader;
	pipeline.computeShader.nByteCount = 4;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "fixed with shader" ) ) {
		return false;
	}
	pipeline.computeShader = {};
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	if ( !checkResult( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "fixed explicit queue" ) ) {
		return false;
	}
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	return checkResult( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "fixed shaders" );
}

int main( void )
{
	if ( !testModern() || !testProgrammable() || !testFixed() ) {
		fputs( "graphics.abi.pipeline-eras FAILED\n", stderr );
		return 1;
	}
	puts( "PASS: shared ABI v1 modern, programmable and fixed-function recipes and negative cases" );
	return 0;
}
