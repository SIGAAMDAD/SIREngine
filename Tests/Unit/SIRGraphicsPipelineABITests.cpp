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
	sirGraphicsBindingDesc_t szExtraBindings[17] = {};
	pipeline.nBindingCount = 17;
	pipeline.pBindings = szExtraBindings;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "binding limit" ) ) {
		return false;
	}
	pipeline.nBindingCount = 0;
	pipeline.pBindings = nullptr;
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
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "programmable compute" ) ) {
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

static bool testStructuralMatrix( void ) noexcept
{
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	sirGraphicsABIPipeline_t base = makeModernPipeline( &modern );
	sirGraphicsABIPipeline_t test = base;
#define CHECK_STRUCTURE( status, label )                                                                                                   \
	if ( !checkResult( sirGraphicsValidateABIPipelineStructure( &test ), status, label ) )                                                 \
	return false
	CHECK_STRUCTURE( SIR_GRAPHICS_SUCCESS, "structure baseline" );
	test.nSize = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure short" );
	test = base;
	test.nReserved = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure reserved" );
	test = base;
	test.nModel = 0;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure bad model" );
	test = base;
	test.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION + 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure new model" );
	test = base;
	test.nType = 99;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure type out of range" );
	test = base;
	test.nColorFormatCount = 9;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure 9 attachments" );
	test = base;
	test.nSampleCount = 0;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure zero samples" );
	test = base;
	test.nSampleCount = 3;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure nonpower samples" );
	test = base;
	test.nBindingCount = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure null binding pointer" );
	test = base;
	test.nAttributeCount = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure null attribute pointer" );
	test = base;
	test.vertexShader.nSize = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure malformed vertex size" );
	test = base;
	test.vertexShader.nByteCount = 3;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure short vertex data" );
	test = base;
	test.vertexShader.nFormat = 0;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure shader format underflow" );
	test = base;
	test.vertexShader.nFormat = SIR_GRAPHICS_SHADER_DXBC + 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure shader format overflow" );
	test = base;
	test.pixelShader.pBytes = nullptr;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure missing pixel shader bytes" );
	test = base;
	test.computeShader.nContentHash = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure empty shader with content hash" );
	test = base;
	test.computeShader.nSize = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure undersized empty shader" );
	test = base;
	test.computeShader.nSize = sizeof( sirGraphicsABIShaderCode_t );
	CHECK_STRUCTURE( SIR_GRAPHICS_SUCCESS, "structure empty shader with size tag" );
	test = base;
	test.pModern = nullptr;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure modern tag missing" );
	test = base;
	sirGraphicsABIProgrammableRecipe_t legacy = {};
	legacy.nSize = sizeof( legacy );
	legacy.nVertexShaderProfile = 101;
	legacy.nFragmentShaderProfile = 101;
	test.pProgrammable = &legacy;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure mixed recipes" );
	test = base;
	modern.nSize = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure modern recipe undersized" );
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure modern wrong graphics stage" );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	test = base;
	test.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure compute with graphics shaders" );
	test.vertexShader = {};
	test.pixelShader = {};
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure compute absent shader" );
	test.computeShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &s_nMockShader, 4, 3, "main" };
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure compute with color" );
	test.nColorFormatCount = 0;
	CHECK_STRUCTURE( SIR_GRAPHICS_SUCCESS, "structure valid compute" );
	test.nAttributeCount = 1;
	sirGraphicsVertexAttribute_t attribute = {};
	test.pAttributes = &attribute;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure compute attributes" );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	test = base;
	test.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure modern fixed type" );
	test = base;
	test.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	test.pModern = nullptr;
	test.pProgrammable = &legacy;
	CHECK_STRUCTURE( SIR_GRAPHICS_SUCCESS, "structure programmable baseline" );
	legacy.nVertexShaderProfile = 0;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure programmable zero vertex profile" );
	legacy.nVertexShaderProfile = 101;
	legacy.nFragmentShaderProfile = 0;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure programmable zero fragment profile" );
	legacy.nFragmentShaderProfile = 101;
	legacy.nReserved = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure programmable reserved" );
	legacy.nReserved = 0;
	legacy.nSize = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure programmable undersized" );
	legacy.nSize = sizeof( legacy );
	test.pixelShader = {};
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure programmable missing pixel" );
	test = base;
	test.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	test.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	test.pModern = nullptr;
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	test.pFixed = &fixed;
	test.vertexShader = {};
	test.pixelShader = {};
	CHECK_STRUCTURE( SIR_GRAPHICS_SUCCESS, "structure shaderless fixed baseline" );
	fixed.nTextureStageCount = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure fixed missing stages" );
	fixed.nTextureStageCount = 0;
	fixed.nLightCount = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure fixed missing lights" );
	fixed.nLightCount = 0;
	fixed.nReserved = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure fixed reserved" );
	fixed.nReserved = 0;
	fixed.nSize = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure fixed short recipe" );
	fixed.nSize = sizeof( fixed );
	test.vertexShader = base.vertexShader;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure fixed vertex shader" );
	test.vertexShader = {};
	test.nBindingCount = 1;
	CHECK_STRUCTURE( SIR_GRAPHICS_INVALID_ARGUMENT, "structure fixed requires no bindings" );
#undef CHECK_STRUCTURE
	return true;
}

static bool testModernCommonMatrix( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = makeModernCaps();
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	sirGraphicsABIPipeline_t base = makeModernPipeline( &modern );
	sirGraphicsABIPipeline_t test = base;
#define CHECK_PIPELINE( status, label )                                                                                                    \
	if ( !checkResult( sirGraphicsValidateABIPipeline( &test, &caps ), status, label ) )                                                   \
	return false
	CHECK_PIPELINE( SIR_GRAPHICS_SUCCESS, "common baseline" );
	test.nReserved = 1;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common reserved" );
	test = base;
	test.nTopology = 99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common topology" );
	test = base;
	test.nCullMode = 99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common culling" );
	test = base;
	test.nDepthCompare = 99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common depth compare" );
	test = base;
	test.nDepthTest = 2;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common depth test Boolean" );
	test = base;
	test.nDepthWrite = 2;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common depth write Boolean" );
	test = base;
	test.nSampleCount = 8;
	CHECK_PIPELINE( SIR_GRAPHICS_UNSUPPORTED, "common sample exceeds caps" );
	test = base;
	test.nDepthFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common color depth mismatch" );
	test = base;
	test.nDepthFormat = SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
	CHECK_PIPELINE( SIR_GRAPHICS_SUCCESS, "common depth format valid" );
	test = base;
	test.szColorFormats[0] = SIR_GRAPHICS_FORMAT_UNKNOWN;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common unknown color format" );
	test = base;
	test.szBlendStates[0].nEnabled = 2;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common invalid blending Boolean" );
	test = base;
	test.szBlendStates[0].nColorWriteMask = 0xFF;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common blend channel mask" );
	test = base;
	test.szBlendStates[0].nEnabled = 1;
	test.szBlendStates[0].nSourceColor = 99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common blend src color" );
	test.szBlendStates[0].nSourceColor = SIR_GRAPHICS_BLEND_ONE;
	test.szBlendStates[0].nDestinationColor = 99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common blend dst color" );
	test.szBlendStates[0].nDestinationColor = SIR_GRAPHICS_BLEND_ONE;
	test.szBlendStates[0].nSourceAlpha = 99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common blend src alpha" );
	test.szBlendStates[0].nSourceAlpha = SIR_GRAPHICS_BLEND_ONE;
	test.szBlendStates[0].nDestinationAlpha = 99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common blend dst alpha" );
	test.szBlendStates[0].nDestinationAlpha = SIR_GRAPHICS_BLEND_ONE;
	test.szBlendStates[0].nColorOperation = 99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common blend color operation" );
	test.szBlendStates[0].nColorOperation = SIR_GRAPHICS_BLEND_ADD;
	test.szBlendStates[0].nAlphaOperation = 99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common blend alpha operation" );
	test.szBlendStates[0].nAlphaOperation = SIR_GRAPHICS_BLEND_ADD;
	CHECK_PIPELINE( SIR_GRAPHICS_SUCCESS, "common valid blending" );
	test = base;
	sirGraphicsVertexAttribute_t attribute = {};
	attribute.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	attribute.nLocation = 0;
	attribute.nBufferSlot = 0;
	test.nAttributeCount = 1;
	test.pAttributes = &attribute;
	CHECK_PIPELINE( SIR_GRAPHICS_SUCCESS, "common valid vertex attribute" );
	attribute.nBufferSlot = 4;
	CHECK_PIPELINE( SIR_GRAPHICS_UNSUPPORTED, "common vertex stream overflow" );
	attribute.nBufferSlot = 0;
	attribute.eFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common unknown vertex format" );
	attribute.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	test = base;
	sirGraphicsBindingDesc_t binding = {};
	binding.eType = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	binding.nArrayCount = 1;
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	test.nBindingCount = 1;
	test.pBindings = &binding;
	CHECK_PIPELINE( SIR_GRAPHICS_SUCCESS, "common valid binding" );
	binding.nArrayCount = 0;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common zero binding count" );
	binding.nArrayCount = 1;
	binding.nShaderStageMask = 0;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common zero binding stages" );
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	binding.eType = ( sirGraphicsBindingType_t )99;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common invalid descriptor type" );
	binding.eType = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common binding crosses pipeline stage" );
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	test = base;
	test.pixelShader.nSize = 1;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common pixel shader too small" );
	test = base;
	test.pixelShader.nByteCount = 2;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common pixel shader bytecount" );
	test = base;
	test.pixelShader.nFormat = SIR_GRAPHICS_SHADER_GLSL;
	CHECK_PIPELINE( SIR_GRAPHICS_UNSUPPORTED, "common unsupported pixel shader format" );
	test = base;
	test.vertexShader.nFormat = SIR_GRAPHICS_SHADER_GLSL;
	CHECK_PIPELINE( SIR_GRAPHICS_UNSUPPORTED, "common unsupported vertex shader format" );
	test = base;
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	CHECK_PIPELINE( SIR_GRAPHICS_INVALID_ARGUMENT, "common wrong modern stages" );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
#undef CHECK_PIPELINE
	return true;
}

static bool testFixedDetailedMatrix( void ) noexcept
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
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	stage.nAlphaOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	sirGraphicsABIFixedLight_t light = {};
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	fixed.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING;
	fixed.nTextureStageCount = 1;
	fixed.nLightCount = 1;
	fixed.pStages = &stage;
	fixed.pLights = &light;
	fixed.nFogMode = SIR_GRAPHICS_FIXED_FOG_NONE;
	fixed.nAlphaCompare = SIR_GRAPHICS_COMPARE_ALWAYS;
	fixed.fAlphaReference = 0.5f;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.pFixed = &fixed;
#define CHECK_FIXED( status, label )                                                                                                       \
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), status, label ) )                                               \
	return false
#define CHECK_FIXED_CAPS( status, label )                                                                                                  \
	if ( !checkResult( sirGraphicsValidateABIPipelineModelCaps( &caps ), status, label ) )                                                 \
	return false
	CHECK_FIXED( SIR_GRAPHICS_SUCCESS, "fixed detail baseline" );
	fixed.nRequiredFeatures = SIR_GRAPHICS_FIXED_TEXCOORD_GEN;
	CHECK_FIXED( SIR_GRAPHICS_UNSUPPORTED, "fixed unsupported feature" );
	fixed.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING;
	fixed.nLightCount = 2;
	CHECK_FIXED( SIR_GRAPHICS_UNSUPPORTED, "fixed light count overflow" );
	fixed.nLightCount = 1;
	fixed.pLights = nullptr;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed null lights" );
	fixed.pLights = &light;
	fixed.pStages = nullptr;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed null stages" );
	fixed.pStages = &stage;
	fixed.nFogMode = 99;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed unrecognized fog" );
	fixed.nFogMode = SIR_GRAPHICS_FIXED_FOG_NONE;
	fixed.nAlphaCompare = 99;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed unrecognized alpha compare" );
	fixed.nAlphaCompare = SIR_GRAPHICS_COMPARE_ALWAYS;
	fixed.nFogMode = SIR_GRAPHICS_FIXED_FOG_LINEAR;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed fog without required feature" );
	fixed.nRequiredFeatures |= SIR_GRAPHICS_FIXED_FOG;
	CHECK_FIXED( SIR_GRAPHICS_SUCCESS, "fixed fog feature enabled" );
	fixed.nRequiredFeatures &= ~SIR_GRAPHICS_FIXED_FOG;
	fixed.nFogMode = SIR_GRAPHICS_FIXED_FOG_NONE;
	fixed.fAlphaReference = -0.1f;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed alpha reference below zero" );
	fixed.fAlphaReference = 2.0f;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed alpha reference above one" );
	fixed.fAlphaReference = 0.5f;
	fixed.fFogStart = 10.0f;
	fixed.fFogEnd = 2.0f;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed fog end before start" );
	fixed.fFogEnd = 20.0f;
	fixed.fFogDensity = -1.0f;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed negative fog density" );
	fixed.fFogDensity = 0.0f;
	fixed.nRequiredFeatures &= ~SIR_GRAPHICS_FIXED_LIGHTING;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed light requires feature bit" );
	fixed.nRequiredFeatures |= SIR_GRAPHICS_FIXED_LIGHTING;
	stage.nTextureUnit = 2;
	CHECK_FIXED( SIR_GRAPHICS_UNSUPPORTED, "fixed texture unit overflow" );
	stage.nTextureUnit = 0;
	stage.nCoordinateSet = 2;
	CHECK_FIXED( SIR_GRAPHICS_UNSUPPORTED, "fixed coordinate set overflow" );
	stage.nCoordinateSet = 0;
	stage.nColorOperation = 99;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed color operation invalid" );
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	stage.nAlphaOperation = 99;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed alpha operation invalid" );
	stage.nAlphaOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_ADD;
	CHECK_FIXED( SIR_GRAPHICS_UNSUPPORTED, "fixed color operation missing" );
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	stage.nAlphaOperation = SIR_GRAPHICS_FIXED_OP_ADD;
	CHECK_FIXED( SIR_GRAPHICS_UNSUPPORTED, "fixed alpha operation missing" );
	stage.nAlphaOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	stage.nColorArgument0 = 99;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed color arg0 invalid" );
	stage.nColorArgument0 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	stage.nColorArgument1 = 99;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed color arg1 invalid" );
	stage.nColorArgument1 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	stage.nAlphaArgument0 = 99;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed alpha arg0 invalid" );
	stage.nAlphaArgument0 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	stage.nAlphaArgument1 = 99;
	CHECK_FIXED( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed alpha arg1 invalid" );
	stage.nAlphaArgument1 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	CHECK_FIXED( SIR_GRAPHICS_SUCCESS, "fixed valid arguments restored" );
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	CHECK_FIXED_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed caps missing fixed-function bit" );
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	CHECK_FIXED_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed caps missing graphics bit" );
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_COPY;
	CHECK_FIXED_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed caps missing direct queue" );
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nMaximumTextureStages = 0;
	CHECK_FIXED_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed caps no texture stages" );
	caps.nMaximumTextureStages = 3;
	CHECK_FIXED_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed caps texture stages too high" );
	caps.nMaximumTextureStages = 2;
	caps.nFixedTextureOperationMask = 0;
	CHECK_FIXED_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed caps no combiner ops" );
	caps.nFixedTextureOperationMask = 1U << SIR_GRAPHICS_FIXED_OP_SELECT;
	caps.nFixedFogModeMask = 1U << SIR_GRAPHICS_FIXED_FOG_LINEAR;
	CHECK_FIXED_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed caps no no-fog mode" );
	caps.nFixedFogModeMask = 1U << SIR_GRAPHICS_FIXED_FOG_NONE;
	caps.nFixedFeatureMask = SIR_GRAPHICS_FIXED_LIGHTING;
	CHECK_FIXED_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "fixed caps missing transform or CPU transform" );
	caps.nFixedFeatureMask |= SIR_GRAPHICS_FIXED_CPU_TRANSFORM;
	CHECK_FIXED_CAPS( SIR_GRAPHICS_SUCCESS, "fixed caps CPU transform permitted" );
#undef CHECK_FIXED
#undef CHECK_FIXED_CAPS
	return true;
}

static bool testProgrammableLimitsMatrix( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = makeModernCaps();
	caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_GLSL - 1 );
	caps.nMaximumVertexShaderProfile = 101;
	caps.nMaximumFragmentShaderProfile = 101;
	caps.nMaximumTextureUnits = 4;
	caps.nMaximumVaryings = 8;
	caps.nMaximumConstantSlots = 16;
	sirGraphicsABIProgrammableRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nVertexShaderProfile = 101;
	recipe.nFragmentShaderProfile = 101;
	recipe.nTextureUnits = 4;
	recipe.nVaryings = 8;
	recipe.nConstantSlots = 16;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.vertexShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_GLSL, &s_nMockShader, 4, 1, "main" };
	pipeline.pixelShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_GLSL, &s_nMockShader, 4, 2, "main" };
	pipeline.pProgrammable = &recipe;
#define CHECK_PROGRAM( status, label )                                                                                                     \
	if ( !checkResult( sirGraphicsValidateABIPipeline( &pipeline, &caps ), status, label ) )                                               \
	return false
#define CHECK_PROGRAM_CAPS( status, label )                                                                                                \
	if ( !checkResult( sirGraphicsValidateABIPipelineModelCaps( &caps ), status, label ) )                                                 \
	return false
	CHECK_PROGRAM( SIR_GRAPHICS_SUCCESS, "programmable detail baseline" );
	recipe.nFragmentShaderProfile = 102;
	CHECK_PROGRAM( SIR_GRAPHICS_UNSUPPORTED, "programmable fragment profile unsupported" );
	recipe.nFragmentShaderProfile = 101;
	recipe.nVaryings = 9;
	CHECK_PROGRAM( SIR_GRAPHICS_UNSUPPORTED, "programmable varying budget" );
	recipe.nVaryings = 8;
	recipe.nConstantSlots = 17;
	CHECK_PROGRAM( SIR_GRAPHICS_UNSUPPORTED, "programmable constant register budget" );
	recipe.nConstantSlots = 16;
	sirGraphicsBindingDesc_t binding = {};
	binding.nArrayCount = 1;
	binding.eType = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_FRAGMENT;
	pipeline.nBindingCount = 1;
	pipeline.pBindings = &binding;
	CHECK_PROGRAM( SIR_GRAPHICS_SUCCESS, "programmable valid binding" );
	binding.nSpace = 1;
	CHECK_PROGRAM( SIR_GRAPHICS_UNSUPPORTED, "programmable nonzero descriptor register space" );
	binding.nSpace = 0;
	binding.eType = SIR_GRAPHICS_BINDING_STORAGE_TEXTURE;
	CHECK_PROGRAM( SIR_GRAPHICS_UNSUPPORTED, "programmable storage texture forbidden" );
	binding.eType = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	caps.nMaximumVertexShaderProfile = 0;
	CHECK_PROGRAM_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable no vertex shader model" );
	caps.nMaximumVertexShaderProfile = 101;
	caps.nMaximumFragmentShaderProfile = 0;
	CHECK_PROGRAM_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable no fragment shader model" );
	caps.nMaximumFragmentShaderProfile = 101;
	caps.nShaderStageMask |= SIR_GRAPHICS_STAGE_MESH;
	CHECK_PROGRAM_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable mesh forbidden" );
	caps.nShaderStageMask &= ~SIR_GRAPHICS_STAGE_MESH;
	caps.nRequiredFeatureMask |= SIR_GRAPHICS_FEATURE_COMPUTE;
	CHECK_PROGRAM_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable compute features forbidden" );
	caps.nRequiredFeatureMask &= ~SIR_GRAPHICS_FEATURE_COMPUTE;
	caps.nMaximumTextureUnits = 0;
	CHECK_PROGRAM_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable no textures" );
	caps.nMaximumTextureUnits = 4;
	caps.nMaximumVertexAttributes = 0;
	CHECK_PROGRAM_CAPS( SIR_GRAPHICS_INVALID_ARGUMENT, "programmable no vertex inputs" );
	caps.nMaximumVertexAttributes = 16;
	CHECK_PROGRAM_CAPS( SIR_GRAPHICS_SUCCESS, "programmable restored caps" );
#undef CHECK_PROGRAM
#undef CHECK_PROGRAM_CAPS
	return true;
}

int main( void )
{
	if ( !testModern() || !testProgrammable() || !testFixed() || !testStructuralMatrix() || !testModernCommonMatrix() ||
	     !testFixedDetailedMatrix() || !testProgrammableLimitsMatrix() ) {
		fputs( "graphics.abi.pipeline-eras FAILED\n", stderr );
		return 1;
	}
	puts( "PASS: shared ABI v1 modern, programmable and fixed-function recipes and negative cases" );
	return 0;
}
