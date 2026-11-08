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

#include <Tools/ShaderCooker/Archive/SIRShaderArchive.h>
#include <Tools/ShaderCooker/Recipes/SIRFixedRecipeCooker.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static sirGraphicsABIFixedRecipe_t makeFixed( sirGraphicsABIFixedStage_t *pStages, sirGraphicsABIFixedLight_t *pLight ) noexcept
{
	sirGraphicsABIFixedRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING | SIR_GRAPHICS_FIXED_FOG;
	recipe.nTextureStageCount = 2;
	recipe.nLightCount = 1;
	recipe.pStages = pStages;
	recipe.pLights = pLight;
	recipe.nFogMode = SIR_GRAPHICS_FIXED_FOG_LINEAR;
	recipe.nAlphaCompare = SIR_GRAPHICS_COMPARE_LESS;
	recipe.fAlphaReference = 0.5f;
	recipe.fFogStart = 2.0f;
	recipe.fFogEnd = 100.0f;
	recipe.fFogDensity = 0.125f;
	recipe.szFogColor[0] = 0.125f;
	recipe.szFogColor[1] = 0.25f;
	recipe.szFogColor[2] = 0.5f;
	recipe.szFogColor[3] = 1.0f;
	recipe.material.szDiffuse[0] = 1.0f;
	recipe.material.szSpecular[0] = 0.3f;
	recipe.material.szEmissive[3] = 0.5f;
	recipe.material.fPower = 16.0f;
	for ( uint32_t i = 0; i < 16; ++i ) {
		recipe.szWorld[i] = ( i % 5 == 0 ) ? 1.0f : 0.0f;
		recipe.szView[i] = ( i % 5 == 0 ) ? 1.0f : 0.0f;
		recipe.szProjection[i] = ( i % 5 == 0 ) ? 1.0f : 0.0f;
	}
	pStages[0].nTextureUnit = 0;
	pStages[0].nColorOperation = SIR_GRAPHICS_FIXED_OP_MODULATE;
	pStages[0].nColorArgument0 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	pStages[0].nColorArgument1 = SIR_GRAPHICS_FIXED_ARG_DIFFUSE;
	pStages[0].nAlphaOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	pStages[0].szConstantColor[3] = 1.0f;
	pStages[1].nTextureUnit = 1;
	pStages[1].nCoordinateSet = 1;
	pStages[1].nColorOperation = SIR_GRAPHICS_FIXED_OP_ADD;
	pStages[1].nAlphaOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	pStages[1].szConstantColor[0] = 0.75f;
	pLight->szPosition[3] = 1.0f;
	pLight->szDiffuse[0] = 1.0f;
	pLight->szDirection[2] = -1.0f;
	pLight->fRange = 32.0f;
	pLight->fConstantAttenuation = 1.0f;
	return recipe;
}

static void testCookLoadAndArchive( void ) noexcept
{
	sirGraphicsABIFixedStage_t szStages[2] = {};
	sirGraphicsABIFixedLight_t light = {};
	sirGraphicsABIFixedRecipe_t recipe = makeFixed( szStages, &light );
	uint32_t nRequired = 0;
	uint8_t szBinary[SIR_FIXED_RECIPE_MAX_BYTES] = {};
	uint8_t szRepeat[SIR_FIXED_RECIPE_MAX_BYTES] = {};
	REQUIRE( sirShaderFixedCook( recipe, nullptr, 0, &nRequired ) == SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	REQUIRE( nRequired == SIR_FIXED_RECIPE_BASE_BYTES + 2 * SIR_FIXED_RECIPE_STAGE_BYTES + SIR_FIXED_RECIPE_LIGHT_BYTES );
	memset( szBinary, 0xA5, sizeof( szBinary ) );
	REQUIRE( sirShaderFixedCook( recipe, szBinary, nRequired - 1, &nRequired ) == SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	for ( uint32_t i = 0; i < nRequired; ++i ) {
		REQUIRE( szBinary[i] == 0xA5 );
	}
	REQUIRE( sirShaderFixedCook( recipe, szBinary, sizeof( szBinary ), &nRequired ) == SIR_SHADER_COOK_OK );
	REQUIRE( sirShaderFixedCook( recipe, szRepeat, sizeof( szRepeat ), &nRequired ) == SIR_SHADER_COOK_OK );
	REQUIRE( memcmp( szRepeat, szBinary, nRequired ) == 0 );
	REQUIRE( sirShaderFixedValidate( szBinary, nRequired ) == SIR_SHADER_COOK_OK );
	sirShaderFixedDecoded_t decoded = {};
	REQUIRE( sirShaderFixedLoad( szBinary, nRequired, &decoded ) == SIR_SHADER_COOK_OK );
	REQUIRE( decoded.recipe.pStages == decoded.szStages && decoded.recipe.pLights == decoded.szLights );
	REQUIRE( decoded.recipe.nTextureStageCount == 2 && decoded.recipe.nLightCount == 1 );
	REQUIRE( decoded.szStages[1].szConstantColor[0] == 0.75f );
	REQUIRE( decoded.szLights[0].fRange == 32.0f );
	REQUIRE( decoded.recipe.szProjection[10] == 1.0f );
	REQUIRE( decoded.recipe.nFogMode == SIR_GRAPHICS_FIXED_FOG_LINEAR );
	REQUIRE( decoded.recipe.material.fPower == 16.0f );
	const uint8_t szSpirv[8] = { 3, 2, 35, 7, 1, 2, 3, 4 };
	const sirShaderArchiveStageInput_t szInputs[3] = { { SIR_GRAPHICS_SHADER_FIXED_FUNCTION, 0, 100, szBinary, nRequired },
	                                                   { SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_STAGE_VERTEX, 101, szSpirv, 8 },
	                                                   { SIR_GRAPHICS_SHADER_FIXED_FUNCTION, 0, 102, szBinary, nRequired } };
	const sirShaderArchiveBuildDesc_t archiveDesc = { 1234, SIR_SHADER_ARCHIVE_TARGET_ABI_V1, szInputs, 3 };
	sirShaderArchiveBuildResult_t built = {};
	REQUIRE( sirShaderArchiveBuild( archiveDesc, nullptr, 0, &built ) == SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	REQUIRE( built.nUniqueCount == 2 );
	uint8_t szArchive[8192] = {};
	REQUIRE( sirShaderArchiveBuild( archiveDesc, szArchive, sizeof( szArchive ), &built ) == SIR_SHADER_COOK_OK );
	REQUIRE( sirShaderArchiveValidate( szArchive, built.nRequiredBytes ) == SIR_SHADER_COOK_OK );
	sirShaderArchiveStageView_t fixedView = {};
	sirShaderArchiveStageView_t vertexView = {};
	sirShaderArchiveStageView_t duplicateView = {};
	REQUIRE( sirShaderArchiveResolve( szArchive, built.nRequiredBytes, 0, &fixedView ) == SIR_SHADER_COOK_OK );
	REQUIRE( sirShaderArchiveResolve( szArchive, built.nRequiredBytes, 1, &vertexView ) == SIR_SHADER_COOK_OK );
	REQUIRE( sirShaderArchiveResolve( szArchive, built.nRequiredBytes, 2, &duplicateView ) == SIR_SHADER_COOK_OK );
	REQUIRE( fixedView.eFormat == SIR_GRAPHICS_SHADER_FIXED_FUNCTION && fixedView.nStageMask == 0 );
	REQUIRE( duplicateView.pBytes == fixedView.pBytes && fixedView.nByteCount == nRequired );
	REQUIRE( vertexView.nStageMask == SIR_GRAPHICS_STAGE_VERTEX && vertexView.eFormat == SIR_GRAPHICS_SHADER_SPIRV );
	sirShaderFixedDecoded_t archived = {};
	REQUIRE( sirShaderFixedLoad( fixedView.pBytes, fixedView.nByteCount, &archived ) == SIR_SHADER_COOK_OK );
	REQUIRE( archived.recipe.nLightCount == 1 && archived.szLights[0].fRange == 32.0f );
	const sirShaderArchiveStageInput_t badFixed = { SIR_GRAPHICS_SHADER_FIXED_FUNCTION, SIR_GRAPHICS_STAGE_VERTEX, 100, szBinary,
	                                                nRequired };
	const sirShaderArchiveBuildDesc_t badDesc = { 1234, SIR_SHADER_ARCHIVE_TARGET_ABI_V1, &badFixed, 1 };
	REQUIRE( sirShaderArchiveBuild( badDesc, szArchive, sizeof( szArchive ), &built ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}

static void testMalformedAndLimits( void ) noexcept
{
	sirGraphicsABIFixedStage_t szStages[2] = {};
	sirGraphicsABIFixedLight_t light = {};
	sirGraphicsABIFixedRecipe_t recipe = makeFixed( szStages, &light );
	uint8_t szBinary[SIR_FIXED_RECIPE_MAX_BYTES] = {};
	uint32_t nRequired = 0;
	REQUIRE( sirShaderFixedCook( recipe, szBinary, sizeof( szBinary ), &nRequired ) == SIR_SHADER_COOK_OK );
	uint8_t szCorrupt[SIR_FIXED_RECIPE_MAX_BYTES] = {};
	memcpy( szCorrupt, szBinary, nRequired );
	szCorrupt[nRequired - 1] ^= 1;
	REQUIRE( sirShaderFixedValidate( szCorrupt, nRequired ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	REQUIRE( sirShaderFixedValidate( szBinary, nRequired - 1 ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	REQUIRE( sirShaderFixedLoad( szBinary, nRequired, nullptr ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	REQUIRE( sirShaderFixedCook( recipe, szBinary, sizeof( szBinary ), nullptr ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szStages[1].nTextureUnit = 0;
	REQUIRE( sirShaderFixedCook( recipe, szBinary, sizeof( szBinary ), &nRequired ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szStages[1].nTextureUnit = 1;
	szStages[0].szConstantColor[0] = NAN;
	REQUIRE( sirShaderFixedCook( recipe, szBinary, sizeof( szBinary ), &nRequired ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szStages[0].szConstantColor[0] = 0;
	recipe.nTextureStageCount = SIR_FIXED_RECIPE_MAX_STAGES + 1;
	REQUIRE( sirShaderFixedCook( recipe, szBinary, sizeof( szBinary ), &nRequired ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	recipe.nTextureStageCount = 2;
	recipe.fFogDensity = -1;
	REQUIRE( sirShaderFixedCook( recipe, szBinary, sizeof( szBinary ), &nRequired ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	recipe.fFogDensity = 0.125f;
	recipe.szWorld[5] = INFINITY;
	REQUIRE( sirShaderFixedCook( recipe, szBinary, sizeof( szBinary ), &nRequired ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}

int main( void )
{
	testCookLoadAndArchive();
	testMalformedAndLimits();
	return 0;
}
