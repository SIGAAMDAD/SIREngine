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
#include <Tools/ShaderCooker/Archive/SIRShaderArchive.h>
#include <Tools/ShaderCooker/Recipes/SIRFixedRecipeCooker.h>

#include <stdio.h>
#include <stdlib.h>

#define CHECK( expr )                                                                                                                      \
	do {                                                                                                                                   \
		if ( !( expr ) ) {                                                                                                                 \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr );                                                                   \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static sirGraphicsABIPipelineModelCaps_t basicCaps( sirGraphicsPipelineModel_t eModel ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 10;
	caps.nModel = eModel;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	caps.nMaximumColorAttachments = 1;
	caps.nMaximumSamples = 1;
	caps.nMaximumTextureDimension = 1024;
	caps.nMaximumTextureUnits = 4;
	caps.nMaximumVertexStreams = 4;
	caps.nMaximumVertexAttributes = 4;
	return caps;
}

static void fixedRecipeRoundtrip( void ) noexcept
{
	sirGraphicsABIFixedStage_t stage = {};
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_MODULATE;
	stage.nColorArgument0 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	stage.nColorArgument1 = SIR_GRAPHICS_FIXED_ARG_DIFFUSE;
	sirGraphicsABIFixedLight_t light = {};
	light.fRange = 24.0f;
	sirGraphicsABIFixedRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING;
	recipe.pStages = &stage;
	recipe.pLights = &light;
	recipe.nTextureStageCount = 1;
	recipe.nLightCount = 1;
	uint8_t szCooked[2048] = {};
	uint32_t nCooked = 0;
	CHECK( sirShaderFixedCook( recipe, szCooked, sizeof( szCooked ), &nCooked ) == SIR_SHADER_COOK_OK );
	const sirShaderArchiveStageInput_t input = { SIR_GRAPHICS_SHADER_FIXED_FUNCTION, 0, 123, szCooked, nCooked };
	const sirShaderArchiveBuildDesc_t desc = { 123, SIR_SHADER_ARCHIVE_TARGET_ABI_V1, &input, 1 };
	uint8_t szArchive[4096] = {};
	sirShaderArchiveBuildResult_t built = {};
	CHECK( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), &built ) == SIR_SHADER_COOK_OK );
	sirShaderArchiveStageView_t view = {};
	CHECK( sirShaderArchiveResolve( szArchive, built.nRequiredBytes, 0, &view ) == SIR_SHADER_COOK_OK );
	sirShaderFixedDecoded_t decoded = {};
	CHECK( sirShaderFixedLoad( view.pBytes, view.nByteCount, &decoded ) == SIR_SHADER_COOK_OK );
	CHECK( decoded.recipe.pStages == decoded.szStages && decoded.recipe.pLights == decoded.szLights );
	sirGraphicsABIPipelineModelCaps_t caps = basicCaps( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION );
	caps.nRequiredFeatureMask |= SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	caps.nShaderStageMask = 0;
	caps.nShaderFormatMask = 0;
	caps.nMaximumTextureStages = 1;
	caps.nMaximumFixedLights = 1;
	caps.nFixedFeatureMask = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING;
	caps.nFixedTextureOperationMask = ( 1U << SIR_GRAPHICS_FIXED_OP_MODULATE ) | ( 1U << SIR_GRAPHICS_FIXED_OP_DISABLE );
	caps.nFixedFogModeMask = 1U << SIR_GRAPHICS_FIXED_FOG_NONE;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.pFixed = &decoded.recipe;
	CHECK( sirGraphicsValidateABIPipelineModelCaps( &caps ) == SIR_GRAPHICS_SUCCESS );
	CHECK( sirGraphicsValidateABIPipeline( &pipeline, &caps ) == SIR_GRAPHICS_SUCCESS );
	caps.nMaximumFixedLights = 0;
	CHECK( sirGraphicsValidateABIPipeline( &pipeline, &caps ) == SIR_GRAPHICS_UNSUPPORTED );
}

static void programmableTokensRoundtrip( void ) noexcept
{
	const uint8_t szVs[] = { 0x00, 0x03, 0xfe, 0xff, 0xff, 0xff, 0x00, 0x00 };
	const uint8_t szPs[] = { 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00 };
	const sirShaderArchiveStageInput_t szInputs[2] = {
	    { SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS, SIR_GRAPHICS_STAGE_VERTEX, 12, szVs, sizeof( szVs ) },
	    { SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS, SIR_GRAPHICS_STAGE_FRAGMENT, 13, szPs, sizeof( szPs ) } };
	const sirShaderArchiveBuildDesc_t archive = { 321, SIR_SHADER_ARCHIVE_TARGET_ABI_V1, szInputs, 2 };
	uint8_t szArchive[512] = {};
	sirShaderArchiveBuildResult_t built = {};
	CHECK( sirShaderArchiveBuild( archive, szArchive, sizeof( szArchive ), &built ) == SIR_SHADER_COOK_OK );
	sirShaderArchiveStageView_t vs = {};
	sirShaderArchiveStageView_t ps = {};
	CHECK( sirShaderArchiveResolve( szArchive, built.nRequiredBytes, 0, &vs ) == SIR_SHADER_COOK_OK );
	CHECK( sirShaderArchiveResolve( szArchive, built.nRequiredBytes, 1, &ps ) == SIR_SHADER_COOK_OK );
	sirGraphicsABIPipelineModelCaps_t caps = basicCaps( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE );
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS - 1 );
	caps.nMaximumBindings = 4;
	caps.nMaximumVaryings = 4;
	caps.nMaximumConstantSlots = 4;
	caps.nMaximumVertexShaderProfile = 0x0300;
	caps.nMaximumFragmentShaderProfile = 0x0300;
	sirGraphicsABIProgrammableRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nVertexShaderProfile = 0x0300;
	recipe.nFragmentShaderProfile = 0x0300;
	recipe.nTextureUnits = 1;
	recipe.nVaryings = 1;
	recipe.nConstantSlots = 1;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.pProgrammable = &recipe;
	pipeline.vertexShader = {
	    sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS, vs.pBytes, vs.nByteCount, vs.nBinaryHash, "main" };
	pipeline.pixelShader = {
	    sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS, ps.pBytes, ps.nByteCount, ps.nBinaryHash, "main" };
	CHECK( sirGraphicsValidateABIPipelineModelCaps( &caps ) == SIR_GRAPHICS_SUCCESS );
	CHECK( sirGraphicsValidateABIPipeline( &pipeline, &caps ) == SIR_GRAPHICS_SUCCESS );
	pipeline.vertexShader.pBytes = ps.pBytes;
	CHECK( sirGraphicsValidateABIPipeline( &pipeline, &caps ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	pipeline.vertexShader.pBytes = vs.pBytes;
	pipeline.pixelShader.nFormat = SIR_GRAPHICS_SHADER_DXBC;
	CHECK( sirGraphicsValidateABIPipeline( &pipeline, &caps ) == SIR_GRAPHICS_UNSUPPORTED );
}

int main( void )
{
	fixedRecipeRoundtrip();
	programmableTokensRoundtrip();
	return 0;
}
