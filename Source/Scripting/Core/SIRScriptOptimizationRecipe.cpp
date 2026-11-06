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

#include <Scripting/Core/SIRScriptOptimizationRecipe.h>
#include <Scripting/Core/SIRScriptSHA256.h>

#include <string.h>

static constexpr uint8_t s_szRecipeMagic[8] = { 'S', 'I', 'R', 'R', 'C', 'P', '0', '1' };

static void recipeWrite32( uint8_t *pData, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pData[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static void recipeWrite64( uint8_t *pData, uint64_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 8; ++i ) {
		pData[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static bool recipeNonzero( const uint8_t *pData ) noexcept
{
	uint8_t nCombined = 0;
	for ( uint32_t i = 0; i < 32; ++i ) {
		nCombined |= pData[i];
	}
	return nCombined != 0;
}

bool SIRScriptOptimizationRecipe::validRequest( const sirScriptOptimizationRequest_t& request ) noexcept
{
	const sirScriptCodeArtifact_t& artifact = request.artifact;
	return artifact.nBackendId != 0 && artifact.nProgramId != 0 && artifact.nRevision != 0 && artifact.nFormatId != 0 &&
	       artifact.nSchemaFingerprint != 0 && artifact.nTargetId != 0 && artifact.nBuildFingerprint != 0 && request.nInputBytes != 0 &&
	       request.nInputBytes <= UINT32_MAX && request.eTier <= SIR_SCRIPT_OPT_PROFILE_GUIDED && request.nFlags == 0;
}

sirScriptResult_t SIRScriptOptimizationRecipe::build( const sirScriptOptimizationRequest_t& request,
                                                      const sirScriptOptimizerRecipeIngredients_t& ingredients, const void *pInput,
                                                      sirScriptOptimizerRecipe_t *pRecipe ) noexcept
{
	if ( pInput == nullptr || pRecipe == nullptr || !validRequest( request ) || !recipeNonzero( ingredients.szDependencies ) ||
	     !recipeNonzero( ingredients.szFrontend ) || !recipeNonzero( ingredients.szOptimizer ) ||
	     !recipeNonzero( ingredients.szOptions ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	uint8_t *pBytes = pRecipe->szBytes;
	memset( pBytes, 0, RECIPE_BYTES );
	memcpy( pBytes, s_szRecipeMagic, 8 );
	recipeWrite32( pBytes + 8, 1 );
	recipeWrite32( pBytes + 12, RECIPE_BYTES );
	const sirScriptCodeArtifact_t& artifact = request.artifact;
	const uint64_t szFields[7] = { artifact.nBackendId,         artifact.nProgramId, artifact.nRevision,        artifact.nFormatId,
	                               artifact.nSchemaFingerprint, artifact.nTargetId,  artifact.nBuildFingerprint };
	for ( uint32_t i = 0; i < 7; ++i ) {
		recipeWrite64( pBytes + 16 + 8 * i, szFields[i] );
	}
	recipeWrite32( pBytes + 72, ( uint32_t )request.eTier );
	recipeWrite32( pBytes + 76, request.nFlags );
	recipeWrite64( pBytes + 80, request.nInputBytes );
	SIRScriptSHA256::digest( pInput, request.nInputBytes, pBytes + 88 );
	memcpy( pBytes + 120, ingredients.szDependencies, 32 );
	memcpy( pBytes + 152, ingredients.szFrontend, 32 );
	memcpy( pBytes + 184, ingredients.szOptimizer, 32 );
	memcpy( pBytes + 216, ingredients.szOptions, 32 );
	SIRScriptSHA256::digest( pBytes, RECIPE_BYTES, pRecipe->key.szDigest );
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationRecipe::verify( const sirScriptOptimizationRequest_t& request, const void *pInput,
                                                       const void *pRecipe, uint64_t nRecipeBytes ) noexcept
{
	if ( pInput == nullptr || pRecipe == nullptr || nRecipeBytes != RECIPE_BYTES || !validRequest( request ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	const uint8_t *pBytes = ( const uint8_t * )pRecipe;
	if ( memcmp( pBytes, s_szRecipeMagic, 8 ) != 0 || pBytes[8] != 1 || pBytes[9] != 0 || pBytes[10] != 0 || pBytes[11] != 0 ||
	     pBytes[12] != ( uint8_t )RECIPE_BYTES || pBytes[13] != 0 || pBytes[14] != 0 || pBytes[15] != 0 ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	const sirScriptCodeArtifact_t& artifact = request.artifact;
	const uint64_t szFields[7] = { artifact.nBackendId,         artifact.nProgramId, artifact.nRevision,        artifact.nFormatId,
	                               artifact.nSchemaFingerprint, artifact.nTargetId,  artifact.nBuildFingerprint };
	uint8_t szCanonical[8] = {};
	for ( uint32_t i = 0; i < 7; ++i ) {
		recipeWrite64( szCanonical, szFields[i] );
		if ( memcmp( pBytes + 16 + 8 * i, szCanonical, 8 ) != 0 ) {
			return SIR_SCRIPT_TYPE_MISMATCH;
		}
	}
	recipeWrite32( szCanonical, ( uint32_t )request.eTier );
	if ( memcmp( pBytes + 72, szCanonical, 4 ) != 0 ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	recipeWrite32( szCanonical, request.nFlags );
	if ( memcmp( pBytes + 76, szCanonical, 4 ) != 0 ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	recipeWrite64( szCanonical, request.nInputBytes );
	if ( memcmp( pBytes + 80, szCanonical, 8 ) != 0 ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	uint8_t szDigest[32] = {};
	SIRScriptSHA256::digest( pInput, request.nInputBytes, szDigest );
	if ( memcmp( szDigest, pBytes + 88, 32 ) != 0 || !recipeNonzero( pBytes + 120 ) || !recipeNonzero( pBytes + 152 ) ||
	     !recipeNonzero( pBytes + 184 ) || !recipeNonzero( pBytes + 216 ) ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	SIRScriptSHA256::digest( pBytes, RECIPE_BYTES, szDigest );
	return memcmp( szDigest, request.key.szDigest, 32 ) == 0 ? SIR_SCRIPT_OK : SIR_SCRIPT_TYPE_MISMATCH;
}
