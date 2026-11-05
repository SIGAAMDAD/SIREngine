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
#include <stdint.h>
#include <stdio.h>

static uint32_t s_nShaderData = 0x07230203;
static uint32_t s_nChecks = 0;

static bool check( sirGraphicsResult_t eActual, sirGraphicsResult_t eExpected, const char *pszLabel ) noexcept
{
	++s_nChecks;
	if ( eActual != eExpected ) {
		fprintf( stderr, "%s: expected %u, observed %u\n", pszLabel, ( uint32_t )eExpected, ( uint32_t )eActual );
		return false;
	}
	return true;
}

static sirGraphicsABIPipelineModelCaps_t makeCaps( uint32_t nModel ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 3;
	caps.nModel = nModel;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nMaximumTextureDimension = 4096;
	caps.nMaximumSamples = 1;
	caps.nMaximumColorAttachments = 1;
	caps.nMaximumVertexStreams = 4;
	caps.nMaximumVertexAttributes = 8;
	caps.nMaximumBindings = 8;
	caps.nMaximumTextureUnits = 4;
	caps.nSupportsNPOT = 1;
	if ( nModel == SIR_GRAPHICS_PIPELINE_MODEL_MODERN ) {
		caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
		caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE;
		caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT | SIR_GRAPHICS_STAGE_COMPUTE;
		caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
	} else if ( nModel == SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
		caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
		caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
		caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_GLSL - 1 );
		caps.nMaximumVertexShaderProfile = 101;
		caps.nMaximumFragmentShaderProfile = 101;
		caps.nMaximumVaryings = 8;
		caps.nMaximumConstantSlots = 32;
	} else {
		caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
		caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
		caps.nMaximumTextureStages = 4;
		caps.nMaximumFixedLights = 2;
		caps.nFixedFeatureMask = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING;
		caps.nFixedTextureOperationMask = ( 1U << SIR_GRAPHICS_FIXED_OP_SELECT ) | ( 1U << SIR_GRAPHICS_FIXED_OP_MODULATE );
		caps.nFixedFogModeMask = ( 1U << SIR_GRAPHICS_FIXED_FOG_NONE ) | ( 1U << SIR_GRAPHICS_FIXED_FOG_LINEAR );
	}
	return caps;
}

static sirGraphicsABIPipeline_t makeShaderPipeline( uint32_t nModel ) noexcept
{
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = nModel;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.nSampleCount = 1;
	const uint32_t nFormat = nModel == SIR_GRAPHICS_PIPELINE_MODEL_MODERN ? SIR_GRAPHICS_SHADER_SPIRV : SIR_GRAPHICS_SHADER_GLSL;
	pipeline.vertexShader = { sizeof( sirGraphicsABIShaderCode_t ), nFormat, &s_nShaderData, 4, 1, "main" };
	pipeline.pixelShader = { sizeof( sirGraphicsABIShaderCode_t ), nFormat, &s_nShaderData, 4, 2, "main" };
	return pipeline;
}

static bool testModern( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = makeCaps( SIR_GRAPHICS_PIPELINE_MODEL_MODERN );
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	sirGraphicsABIPipeline_t pipeline = makeShaderPipeline( SIR_GRAPHICS_PIPELINE_MODEL_MODERN );
	pipeline.pModern = &modern;
	if ( !check( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_SUCCESS, "modern caps" ) ||
	     !check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "modern pipeline" ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < 64; ++i ) {
		modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_RAY_TRACING;
		if ( !check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "modern optional feature" ) ) {
			return false;
		}
		modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
		modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
		if ( !check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "modern shader stage" ) ) {
			return false;
		}
		modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	}
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	return check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "stateful modern API" );
}

static bool testProgrammable( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = makeCaps( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE );
	sirGraphicsABIProgrammableRecipe_t programmable = {};
	programmable.nSize = sizeof( programmable );
	programmable.nVertexShaderProfile = 101;
	programmable.nFragmentShaderProfile = 101;
	programmable.nTextureUnits = 2;
	programmable.nVaryings = 4;
	programmable.nConstantSlots = 8;
	sirGraphicsABIPipeline_t pipeline = makeShaderPipeline( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE );
	pipeline.pProgrammable = &programmable;
	if ( !check( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_SUCCESS, "programmable caps" ) ||
	     !check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "programmable pipeline" ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < 64; ++i ) {
		programmable.nTextureUnits = 5;
		if ( !check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "legacy texture limit" ) ) {
			return false;
		}
		programmable.nTextureUnits = 2;
		programmable.nVertexShaderProfile = 102;
		if ( !check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "legacy shader profile" ) ) {
			return false;
		}
		programmable.nVertexShaderProfile = 101;
		pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
		if ( !check( sirGraphicsValidateABIPipelineStructure( &pipeline ), SIR_GRAPHICS_INVALID_ARGUMENT, "legacy compute" ) ) {
			return false;
		}
		pipeline.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	}
	caps.nShaderStageMask |= SIR_GRAPHICS_STAGE_COMPUTE;
	return check( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "legacy compute caps" );
}

static bool testFixed( void ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = makeCaps( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION );
	sirGraphicsABIFixedStage_t stages[5] = {};
	sirGraphicsABIFixedLight_t lights[3] = {};
	for ( uint32_t i = 0; i < 5; ++i ) {
		stages[i].nTextureUnit = i;
		stages[i].nColorOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
		stages[i].nAlphaOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	}
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	fixed.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM;
	fixed.nFogMode = SIR_GRAPHICS_FIXED_FOG_NONE;
	fixed.nAlphaCompare = SIR_GRAPHICS_COMPARE_ALWAYS;
	fixed.fAlphaReference = 0.5f;
	fixed.pStages = stages;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.pFixed = &fixed;
	if ( !check( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_SUCCESS, "fixed caps" ) ||
	     !check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS, "shaderless fixed" ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < 64; ++i ) {
		fixed.nTextureStageCount = 5;
		if ( !check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "fixed stage budget" ) ) {
			return false;
		}
		fixed.nTextureStageCount = 0;
		fixed.nLightCount = 3;
		fixed.pLights = lights;
		if ( !check( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED, "fixed light budget" ) ) {
			return false;
		}
		fixed.nLightCount = 0;
		fixed.pLights = nullptr;
		pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
		if ( !check( sirGraphicsValidateABIPipelineStructure( &pipeline ), SIR_GRAPHICS_INVALID_ARGUMENT, "fixed compute" ) ) {
			return false;
		}
		pipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	}
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	return check( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "fixed explicit queue" );
}

int main( void )
{
	if ( !testModern() || !testProgrammable() || !testFixed() ) {
		return 1;
	}
	printf( "PASS: %u three-era ABI conformance assertions\n", s_nChecks );
	return 0;
}
