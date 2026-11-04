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

#include <Renderer/SIRGraphicsPipelineBridge.h>
#include <Renderer/SIRGraphicsDriverABI.h>
#include <stdio.h>

static uint32_t s_nShaderWords[4] = { 0x07230203, 0, 0, 0 };

static bool checkResult( sirGraphicsResult_t eObserved, sirGraphicsResult_t eExpected, const char *pszCase ) noexcept
{
	if ( eObserved != eExpected ) {
		fprintf( stderr, "%s: expected %u, got %u\n", pszCase, ( uint32_t )eExpected, ( uint32_t )eObserved );
		return false;
	}
	return true;
}

static sirGraphicsABIPipelineModelCaps_t makeGraphicsCaps( uint32_t nModel ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 9;
	caps.nModel = nModel;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
	caps.nMaximumTextureUnits = 8;
	caps.nMaximumVertexAttributes = 16;
	caps.nMaximumVertexStreams = 4;
	caps.nMaximumBindings = 16;
	caps.nMaximumColorAttachments = 1;
	caps.nMaximumSamples = 1;
	caps.nMaximumTextureDimension = 4096;
	return caps;
}

static sirGraphicsPipelineDesc_t makeGraphicsSource( void ) noexcept
{
	sirGraphicsPipelineDesc_t source = {};
	source.eType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	source.nSampleCount = 1;
	source.nColorFormatCount = 1;
	source.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	source.eTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	source.eCullMode = SIR_GRAPHICS_CULL_BACK;
	source.eDepthCompare = SIR_GRAPHICS_COMPARE_LESS_EQUAL;
	source.vertexShader = { SIR_GRAPHICS_SHADER_SPIRV, s_nShaderWords, sizeof( s_nShaderWords ), 101, "main" };
	source.pixelShader = { SIR_GRAPHICS_SHADER_SPIRV, s_nShaderWords, sizeof( s_nShaderWords ), 102, "main" };
	source.szBlendStates[0].bEnabled = true;
	source.szBlendStates[0].eSourceColor = SIR_GRAPHICS_BLEND_SRC_ALPHA;
	source.szBlendStates[0].eDestinationColor = SIR_GRAPHICS_BLEND_ONE_MINUS_SRC_ALPHA;
	source.szBlendStates[0].eSourceAlpha = SIR_GRAPHICS_BLEND_ONE;
	source.szBlendStates[0].eDestinationAlpha = SIR_GRAPHICS_BLEND_ZERO;
	source.szBlendStates[0].eColorOperation = SIR_GRAPHICS_BLEND_ADD;
	source.szBlendStates[0].eAlphaOperation = SIR_GRAPHICS_BLEND_ADD;
	source.szBlendStates[0].nColorWriteMask = 15;
	source.bDepthTest = true;
	source.bDepthWrite = true;
	source.eDepthFormat = SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
	return source;
}

static bool testModern( void ) noexcept
{
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	modern.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	modern.nCookedVariantHash = 0x23ULL;
	sirGraphicsPipelineDesc_t source = makeGraphicsSource();
	source.eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	source.pModern = &modern;
	sirGraphicsBindingDesc_t binding = {};
	binding.nBinding = 2;
	binding.nArrayCount = 1;
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_FRAGMENT;
	binding.eType = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	source.pBindings = &binding;
	source.nBindingCount = 1;

	sirGraphicsABIPipeline_t encoded = {};
	sirGraphicsABIPipelineModelCaps_t caps = makeGraphicsCaps( SIR_GRAPHICS_PIPELINE_MODEL_MODERN );
	if ( !checkResult( sirGraphicsEncodeABIPipeline( source, &encoded ), SIR_GRAPHICS_SUCCESS, "modern bridge" ) ) {
		return false;
	}
	if ( encoded.nSize != sizeof( encoded ) || encoded.nModel != SIR_GRAPHICS_PIPELINE_MODEL_MODERN || encoded.pModern != &modern ||
	     encoded.pBindings != &binding ) {
		return false;
	}
	if ( encoded.vertexShader.nSize != sizeof( sirGraphicsABIShaderCode_t ) ||
	     encoded.vertexShader.nContentHash != source.vertexShader.nContentHash || encoded.vertexShader.pBytes != s_nShaderWords ) {
		return false;
	}
	if ( encoded.nDepthTest != 1 || encoded.nDepthWrite != 1 || encoded.szBlendStates[0].nEnabled != 1 ||
	     encoded.szBlendStates[0].nColorWriteMask != 15 ) {
		return false;
	}
	if ( !checkResult( sirGraphicsValidateABIPipeline( &encoded, &caps ), SIR_GRAPHICS_SUCCESS, "modern native contract" ) ) {
		return false;
	}
	encoded.nSize = 17;
	source.pModern = nullptr;
	if ( !checkResult( sirGraphicsEncodeABIPipeline( source, &encoded ), SIR_GRAPHICS_INVALID_ARGUMENT, "missing modern tag" ) ) {
		return false;
	}
	if ( encoded.nSize != 17 ) {
		return false;
	}
	source.pModern = &modern;
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	if ( !checkResult( sirGraphicsEncodeABIPipeline( source, &encoded ), SIR_GRAPHICS_INVALID_ARGUMENT, "modern stage mismatch" ) ) {
		return false;
	}
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	source.eModel = ( sirGraphicsPipelineModel_t )0;
	return checkResult( sirGraphicsEncodeABIPipeline( source, &encoded ), SIR_GRAPHICS_INVALID_ARGUMENT, "untagged legacy descriptor" );
}

static bool testProgrammable( void ) noexcept
{
	sirGraphicsABIProgrammableRecipe_t programmable = {};
	programmable.nSize = sizeof( programmable );
	programmable.nVertexShaderProfile = 101;
	programmable.nFragmentShaderProfile = 101;
	programmable.nTextureUnits = 2;
	programmable.nVaryings = 4;
	programmable.nConstantSlots = 12;
	sirGraphicsPipelineDesc_t source = makeGraphicsSource();
	source.eModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	source.pProgrammable = &programmable;
	source.vertexShader.eFormat = SIR_GRAPHICS_SHADER_GLSL;
	source.pixelShader.eFormat = SIR_GRAPHICS_SHADER_GLSL;
	sirGraphicsABIPipeline_t encoded = {};
	sirGraphicsABIPipelineModelCaps_t caps = makeGraphicsCaps( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE );
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_GLSL - 1 );
	caps.nMaximumTextureUnits = 4;
	caps.nMaximumVaryings = 8;
	caps.nMaximumConstantSlots = 32;
	caps.nMaximumVertexShaderProfile = 101;
	caps.nMaximumFragmentShaderProfile = 101;

	if ( !checkResult( sirGraphicsEncodeABIPipeline( source, &encoded ), SIR_GRAPHICS_SUCCESS, "programmable bridge" ) ) {
		return false;
	}
	if ( encoded.pProgrammable != &programmable || encoded.pFixed != nullptr || encoded.pModern != nullptr ) {
		return false;
	}
	if ( !checkResult( sirGraphicsValidateABIPipeline( &encoded, &caps ), SIR_GRAPHICS_SUCCESS, "programmable native contract" ) ) {
		return false;
	}
	programmable.nTextureUnits = 5;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &encoded, &caps ), SIR_GRAPHICS_UNSUPPORTED, "programmable slots" ) ) {
		return false;
	}
	programmable.nTextureUnits = 2;
	sirGraphicsABIModernRecipe_t wrong = {};
	source.pModern = &wrong;
	return checkResult( sirGraphicsEncodeABIPipeline( source, &encoded ), SIR_GRAPHICS_INVALID_ARGUMENT, "conflicting source recipes" );
}

static bool testFixed( void ) noexcept
{
	sirGraphicsABIFixedStage_t stage = {};
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_MODULATE;
	stage.nAlphaOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	fixed.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM;
	fixed.nTextureStageCount = 1;
	fixed.pStages = &stage;
	fixed.nFogMode = SIR_GRAPHICS_FIXED_FOG_NONE;
	fixed.nAlphaCompare = SIR_GRAPHICS_COMPARE_ALWAYS;
	fixed.fAlphaReference = 0.5f;
	sirGraphicsPipelineDesc_t source = {};
	source.eType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	source.eModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	source.pFixed = &fixed;
	source.nColorFormatCount = 1;
	source.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	source.nSampleCount = 1;

	sirGraphicsABIPipeline_t encoded = {};
	sirGraphicsABIPipelineModelCaps_t caps = makeGraphicsCaps( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION );
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	caps.nShaderStageMask = 0;
	caps.nShaderFormatMask = 0;
	caps.nMaximumTextureUnits = 2;
	caps.nMaximumTextureStages = 2;
	caps.nMaximumFixedLights = 2;
	caps.nFixedFeatureMask = SIR_GRAPHICS_FIXED_TRANSFORM;
	caps.nFixedTextureOperationMask = ( 1U << SIR_GRAPHICS_FIXED_OP_SELECT ) | ( 1U << SIR_GRAPHICS_FIXED_OP_MODULATE );
	caps.nFixedFogModeMask = 1U << SIR_GRAPHICS_FIXED_FOG_NONE;

	if ( !checkResult( sirGraphicsEncodeABIPipeline( source, &encoded ), SIR_GRAPHICS_SUCCESS, "fixed bridge" ) ) {
		return false;
	}
	if ( encoded.pFixed != &fixed || encoded.vertexShader.pBytes != nullptr || encoded.pixelShader.pBytes != nullptr ||
	     encoded.computeShader.pBytes != nullptr ) {
		return false;
	}
	if ( !checkResult( sirGraphicsValidateABIPipeline( &encoded, &caps ), SIR_GRAPHICS_SUCCESS, "fixed native contract" ) ) {
		return false;
	}
	fixed.nTextureStageCount = 3;
	if ( !checkResult( sirGraphicsValidateABIPipeline( &encoded, &caps ), SIR_GRAPHICS_UNSUPPORTED, "fixed over budget" ) ) {
		return false;
	}
	fixed.nTextureStageCount = 1;
	source.nBindingCount = 1;
	return checkResult( sirGraphicsEncodeABIPipeline( source, &encoded ), SIR_GRAPHICS_INVALID_ARGUMENT, "fixed descriptor binding" );
}

int main( void )
{
	if ( !testModern() || !testProgrammable() || !testFixed() ) {
		fputs( "graphics.pipeline.bridge FAILED\n", stderr );
		return 1;
	}
	puts( "PASS: source-to-binary pipeline ABI v1 equivalence across three eras" );
	return 0;
}
