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

#include <Tools/ShaderCooker/Recipes/SIRFixedRecipeCooker.h>

#include <math.h>
#include <string.h>

static void sirFixedWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pBytes[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static uint32_t sirFixedRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

static void sirFixedWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	sirFixedWrite32( pBytes, ( uint32_t )nValue );
	sirFixedWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

static uint64_t sirFixedRead64( const uint8_t *pBytes ) noexcept
{
	return ( uint64_t )sirFixedRead32( pBytes ) | ( ( uint64_t )sirFixedRead32( pBytes + 4 ) << 32 );
}

static void sirFixedWriteFloat( uint8_t *pBytes, float fValue ) noexcept
{
	uint32_t nBits = 0;
	memcpy( &nBits, &fValue, sizeof( nBits ) );
	sirFixedWrite32( pBytes, nBits );
}

static float sirFixedReadFloat( const uint8_t *pBytes ) noexcept
{
	const uint32_t nBits = sirFixedRead32( pBytes );
	float fValue = 0;
	memcpy( &fValue, &nBits, sizeof( fValue ) );
	return fValue;
}

static uint64_t sirFixedHash( const uint8_t *pBytes, uint32_t nBytes ) noexcept
{
	uint64_t nHash = 14695981039346656037ULL;
	for ( uint32_t i = 0; i < nBytes; ++i ) {
		const uint8_t nByte = i >= 20 && i < 28 ? 0 : pBytes[i];
		nHash = ( nHash ^ nByte ) * 1099511628211ULL;
	}
	return nHash;
}

static bool sirFixedFloatValid( float fValue ) noexcept
{
	return isfinite( fValue ) != 0;
}

static bool sirFixedFloatsValid( const float *pValues, uint32_t nCount ) noexcept
{
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( !sirFixedFloatValid( pValues[i] ) ) {
			return false;
		}
	}
	return true;
}

static bool sirFixedRecipeValid( const sirGraphicsABIFixedRecipe_t& recipe ) noexcept
{
	if ( recipe.nSize < sizeof( recipe ) || recipe.nReserved != 0 || recipe.nTextureStageCount > SIR_FIXED_RECIPE_MAX_STAGES ||
	     recipe.nLightCount > SIR_FIXED_RECIPE_MAX_LIGHTS || ( recipe.nTextureStageCount != 0 && recipe.pStages == nullptr ) ||
	     ( recipe.nLightCount != 0 && recipe.pLights == nullptr ) ) {
		return false;
	}
	const uint32_t nAllowedFeatures = ( SIR_GRAPHICS_FIXED_TEXCOORD_GEN << 1 ) - 1U;
	if ( ( recipe.nRequiredFeatures & ~nAllowedFeatures ) != 0 || recipe.nFogMode > SIR_GRAPHICS_FIXED_FOG_EXP2 ||
	     recipe.nAlphaCompare > SIR_GRAPHICS_COMPARE_ALWAYS || !( recipe.fAlphaReference >= 0 && recipe.fAlphaReference <= 1 ) ||
	     recipe.fFogEnd < recipe.fFogStart || recipe.fFogDensity < 0 ||
	     ( recipe.nLightCount != 0 && ( recipe.nRequiredFeatures & SIR_GRAPHICS_FIXED_LIGHTING ) == 0 ) ||
	     ( recipe.nFogMode != SIR_GRAPHICS_FIXED_FOG_NONE && ( recipe.nRequiredFeatures & SIR_GRAPHICS_FIXED_FOG ) == 0 ) ) {
		return false;
	}
	const sirGraphicsABIFixedMaterial_t& mat = recipe.material;
	if ( mat.nReserved != 0 || !sirFixedFloatsValid( mat.szAmbient, 4 ) || !sirFixedFloatsValid( mat.szDiffuse, 4 ) ||
	     !sirFixedFloatsValid( mat.szSpecular, 4 ) || !sirFixedFloatsValid( mat.szEmissive, 4 ) || !sirFixedFloatValid( mat.fPower ) ||
	     mat.fPower < 0 || !sirFixedFloatsValid( recipe.szFogColor, 4 ) || !sirFixedFloatsValid( recipe.szWorld, 16 ) ||
	     !sirFixedFloatsValid( recipe.szView, 16 ) || !sirFixedFloatsValid( recipe.szProjection, 16 ) ||
	     !sirFixedFloatValid( recipe.fAlphaReference ) || !sirFixedFloatValid( recipe.fFogStart ) ||
	     !sirFixedFloatValid( recipe.fFogEnd ) || !sirFixedFloatValid( recipe.fFogDensity ) ) {
		return false;
	}
	for ( uint32_t iStage = 0; iStage < recipe.nTextureStageCount; ++iStage ) {
		const sirGraphicsABIFixedStage_t& stage = recipe.pStages[iStage];
		if ( stage.nTextureUnit >= SIR_FIXED_RECIPE_MAX_STAGES || stage.nCoordinateSet >= 16 ||
		     stage.nColorOperation > SIR_GRAPHICS_FIXED_OP_DOT3 || stage.nAlphaOperation > SIR_GRAPHICS_FIXED_OP_DOT3 ||
		     stage.nColorArgument0 > SIR_GRAPHICS_FIXED_ARG_CONSTANT || stage.nColorArgument1 > SIR_GRAPHICS_FIXED_ARG_CONSTANT ||
		     stage.nAlphaArgument0 > SIR_GRAPHICS_FIXED_ARG_CONSTANT || stage.nAlphaArgument1 > SIR_GRAPHICS_FIXED_ARG_CONSTANT ||
		     !sirFixedFloatsValid( stage.szConstantColor, 4 ) ) {
			return false;
		}
		for ( uint32_t jStage = 0; jStage < iStage; ++jStage ) {
			if ( recipe.pStages[jStage].nTextureUnit == stage.nTextureUnit ) {
				return false;
			}
		}
	}
	for ( uint32_t iLight = 0; iLight < recipe.nLightCount; ++iLight ) {
		const sirGraphicsABIFixedLight_t& light = recipe.pLights[iLight];
		if ( !sirFixedFloatsValid( light.szPosition, 4 ) || !sirFixedFloatsValid( light.szDirection, 4 ) ||
		     !sirFixedFloatsValid( light.szDiffuse, 4 ) || !sirFixedFloatsValid( light.szSpecular, 4 ) ||
		     !sirFixedFloatValid( light.fRange ) || !sirFixedFloatValid( light.fConstantAttenuation ) ||
		     !sirFixedFloatValid( light.fLinearAttenuation ) || !sirFixedFloatValid( light.fQuadraticAttenuation ) || light.fRange < 0 ||
		     light.fConstantAttenuation < 0 || light.fLinearAttenuation < 0 || light.fQuadraticAttenuation < 0 ) {
			return false;
		}
	}
	return true;
}

static void sirFixedWriteFloats( uint8_t *pBytes, const float *pFloats, uint32_t nCount ) noexcept
{
	for ( uint32_t i = 0; i < nCount; ++i ) {
		sirFixedWriteFloat( pBytes + i * 4, pFloats[i] );
	}
}

static void sirFixedReadFloats( const uint8_t *pBytes, float *pFloats, uint32_t nCount ) noexcept
{
	for ( uint32_t i = 0; i < nCount; ++i ) {
		pFloats[i] = sirFixedReadFloat( pBytes + i * 4 );
	}
}

static void sirFixedWriteRecipe( const sirGraphicsABIFixedRecipe_t& recipe, uint8_t *pData ) noexcept
{
	uint32_t nOffset = SIR_FIXED_RECIPE_HEADER_BYTES;
	const uint32_t szScalars[3] = { recipe.nRequiredFeatures, recipe.nFogMode, recipe.nAlphaCompare };
	for ( uint32_t i = 0; i < 3; ++i, nOffset += 4 ) {
		sirFixedWrite32( pData + nOffset, szScalars[i] );
	}
	const float szFog[4] = { recipe.fAlphaReference, recipe.fFogStart, recipe.fFogEnd, recipe.fFogDensity };
	sirFixedWriteFloats( pData + nOffset, szFog, 4 );
	nOffset += 16;
	sirFixedWriteFloats( pData + nOffset, recipe.szFogColor, 4 );
	nOffset += 16;
	const float *szMaterial[4] = { recipe.material.szAmbient, recipe.material.szDiffuse, recipe.material.szSpecular,
	                               recipe.material.szEmissive };
	for ( uint32_t i = 0; i < 4; ++i, nOffset += 16 ) {
		sirFixedWriteFloats( pData + nOffset, szMaterial[i], 4 );
	}
	sirFixedWriteFloat( pData + nOffset, recipe.material.fPower );
	nOffset += 4;
	const float *szMatrices[3] = { recipe.szWorld, recipe.szView, recipe.szProjection };
	for ( uint32_t i = 0; i < 3; ++i, nOffset += 64 ) {
		sirFixedWriteFloats( pData + nOffset, szMatrices[i], 16 );
	}
	for ( uint32_t iStage = 0; iStage < recipe.nTextureStageCount; ++iStage ) {
		const sirGraphicsABIFixedStage_t& stage = recipe.pStages[iStage];
		const uint32_t szFields[8] = { stage.nTextureUnit,    stage.nCoordinateSet,  stage.nColorOperation, stage.nAlphaOperation,
		                               stage.nColorArgument0, stage.nColorArgument1, stage.nAlphaArgument0, stage.nAlphaArgument1 };
		for ( uint32_t i = 0; i < 8; ++i, nOffset += 4 ) {
			sirFixedWrite32( pData + nOffset, szFields[i] );
		}
		sirFixedWriteFloats( pData + nOffset, stage.szConstantColor, 4 );
		nOffset += 16;
	}
	for ( uint32_t iLight = 0; iLight < recipe.nLightCount; ++iLight ) {
		const sirGraphicsABIFixedLight_t& light = recipe.pLights[iLight];
		const float *szVectors[4] = { light.szPosition, light.szDirection, light.szDiffuse, light.szSpecular };
		for ( uint32_t i = 0; i < 4; ++i, nOffset += 16 ) {
			sirFixedWriteFloats( pData + nOffset, szVectors[i], 4 );
		}
		const float szAttenuation[4] = { light.fRange, light.fConstantAttenuation, light.fLinearAttenuation, light.fQuadraticAttenuation };
		sirFixedWriteFloats( pData + nOffset, szAttenuation, 4 );
		nOffset += 16;
	}
}

sirShaderCookResult_t sirShaderFixedCook( const sirGraphicsABIFixedRecipe_t& recipe, void *pDestination, uint32_t nCapacity,
                                          uint32_t *pRequiredBytes ) noexcept
{
	if ( pRequiredBytes == nullptr || !sirFixedRecipeValid( recipe ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const uint32_t nRequired = SIR_FIXED_RECIPE_BASE_BYTES + recipe.nTextureStageCount * SIR_FIXED_RECIPE_STAGE_BYTES +
	                           recipe.nLightCount * SIR_FIXED_RECIPE_LIGHT_BYTES;
	*pRequiredBytes = nRequired;
	if ( pDestination == nullptr || nCapacity < nRequired ) {
		return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
	}
	uint8_t *pData = ( uint8_t * )pDestination;
	// No writes occur before full validation and capacity admission.
	sirFixedWrite32( pData, SIR_FIXED_RECIPE_MAGIC );
	sirFixedWrite32( pData + 4, SIR_FIXED_RECIPE_VERSION );
	sirFixedWrite32( pData + 8, nRequired );
	sirFixedWrite32( pData + 12, recipe.nTextureStageCount );
	sirFixedWrite32( pData + 16, recipe.nLightCount );
	sirFixedWrite64( pData + 20, 0 );
	sirFixedWriteRecipe( recipe, pData );
	sirFixedWrite64( pData + 20, sirFixedHash( pData, nRequired ) );
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirShaderFixedLoad( const void *pBytes, uint32_t nBytes, sirShaderFixedDecoded_t *pOut ) noexcept
{
	if ( pBytes == nullptr || pOut == nullptr || nBytes < SIR_FIXED_RECIPE_BASE_BYTES || nBytes > SIR_FIXED_RECIPE_MAX_BYTES ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const uint8_t *pData = ( const uint8_t * )pBytes;
	const uint32_t nStages = sirFixedRead32( pData + 12 );
	const uint32_t nLights = sirFixedRead32( pData + 16 );
	if ( sirFixedRead32( pData ) != SIR_FIXED_RECIPE_MAGIC || sirFixedRead32( pData + 4 ) != SIR_FIXED_RECIPE_VERSION ||
	     sirFixedRead32( pData + 8 ) != nBytes || nStages > SIR_FIXED_RECIPE_MAX_STAGES || nLights > SIR_FIXED_RECIPE_MAX_LIGHTS ||
	     SIR_FIXED_RECIPE_BASE_BYTES + nStages * SIR_FIXED_RECIPE_STAGE_BYTES + nLights * SIR_FIXED_RECIPE_LIGHT_BYTES != nBytes ||
	     sirFixedRead64( pData + 20 ) != sirFixedHash( pData, nBytes ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	sirShaderFixedDecoded_t decoded = {};
	sirGraphicsABIFixedRecipe_t& recipe = decoded.recipe;
	recipe.nSize = sizeof( recipe );
	recipe.nTextureStageCount = nStages;
	recipe.nLightCount = nLights;
	uint32_t nOffset = SIR_FIXED_RECIPE_HEADER_BYTES;
	recipe.nRequiredFeatures = sirFixedRead32( pData + nOffset );
	nOffset += 4;
	recipe.nFogMode = sirFixedRead32( pData + nOffset );
	nOffset += 4;
	recipe.nAlphaCompare = sirFixedRead32( pData + nOffset );
	nOffset += 4;
	float szFog[4] = {};
	sirFixedReadFloats( pData + nOffset, szFog, 4 );
	recipe.fAlphaReference = szFog[0];
	recipe.fFogStart = szFog[1];
	recipe.fFogEnd = szFog[2];
	recipe.fFogDensity = szFog[3];
	nOffset += 16;
	sirFixedReadFloats( pData + nOffset, recipe.szFogColor, 4 );
	nOffset += 16;
	float *szMaterial[4] = { recipe.material.szAmbient, recipe.material.szDiffuse, recipe.material.szSpecular, recipe.material.szEmissive };
	for ( uint32_t i = 0; i < 4; ++i, nOffset += 16 ) {
		sirFixedReadFloats( pData + nOffset, szMaterial[i], 4 );
	}
	recipe.material.fPower = sirFixedReadFloat( pData + nOffset );
	nOffset += 4;
	float *szMatrices[3] = { recipe.szWorld, recipe.szView, recipe.szProjection };
	for ( uint32_t i = 0; i < 3; ++i, nOffset += 64 ) {
		sirFixedReadFloats( pData + nOffset, szMatrices[i], 16 );
	}
	for ( uint32_t iStage = 0; iStage < nStages; ++iStage ) {
		sirGraphicsABIFixedStage_t& stage = decoded.szStages[iStage];
		uint32_t *szFields[8] = { &stage.nTextureUnit,    &stage.nCoordinateSet,  &stage.nColorOperation, &stage.nAlphaOperation,
		                          &stage.nColorArgument0, &stage.nColorArgument1, &stage.nAlphaArgument0, &stage.nAlphaArgument1 };
		for ( uint32_t i = 0; i < 8; ++i, nOffset += 4 ) {
			*szFields[i] = sirFixedRead32( pData + nOffset );
		}
		sirFixedReadFloats( pData + nOffset, stage.szConstantColor, 4 );
		nOffset += 16;
	}
	for ( uint32_t iLight = 0; iLight < nLights; ++iLight ) {
		sirGraphicsABIFixedLight_t& light = decoded.szLights[iLight];
		float *szVectors[4] = { light.szPosition, light.szDirection, light.szDiffuse, light.szSpecular };
		for ( uint32_t i = 0; i < 4; ++i, nOffset += 16 ) {
			sirFixedReadFloats( pData + nOffset, szVectors[i], 4 );
		}
		float szAttenuation[4] = {};
		sirFixedReadFloats( pData + nOffset, szAttenuation, 4 );
		light.fRange = szAttenuation[0];
		light.fConstantAttenuation = szAttenuation[1];
		light.fLinearAttenuation = szAttenuation[2];
		light.fQuadraticAttenuation = szAttenuation[3];
		nOffset += 16;
	}
	recipe.pStages = nStages != 0 ? decoded.szStages : nullptr;
	recipe.pLights = nLights != 0 ? decoded.szLights : nullptr;
	if ( !sirFixedRecipeValid( decoded.recipe ) ) {
		// The decoded stack object has not been written into the caller yet.
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	decoded.nContentHash = sirFixedRead64( pData + 20 );
	// Commit all decoded contents as one operation after the validation gate.
	// Fix the pointers only after the copy, never leave stack addresses in output.
	memcpy( pOut->szStages, decoded.szStages, sizeof( decoded.szStages ) );
	memcpy( pOut->szLights, decoded.szLights, sizeof( decoded.szLights ) );
	pOut->recipe = decoded.recipe;
	pOut->recipe.pStages = nStages != 0 ? pOut->szStages : nullptr;
	pOut->recipe.pLights = nLights != 0 ? pOut->szLights : nullptr;
	pOut->nContentHash = decoded.nContentHash;
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirShaderFixedValidate( const void *pBytes, uint32_t nBytes ) noexcept
{
	sirShaderFixedDecoded_t decoded = {};
	return sirShaderFixedLoad( pBytes, nBytes, &decoded );
}
