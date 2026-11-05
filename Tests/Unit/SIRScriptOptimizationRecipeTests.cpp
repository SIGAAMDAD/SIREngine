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

#include <stdio.h>
#include <string.h>

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

int main( void )
{
	uint8_t szInput[256] = {};
	for ( uint32_t i = 0; i < sizeof( szInput ); ++i ) {
		szInput[i] = ( uint8_t )( i * 17 + 3 );
	}
	sirScriptOptimizationRequest_t request = {};
	request.nJobId = 17;
	request.artifact = { 10, 20, 30, 40, 50, 60, 70 };
	request.nInputBytes = sizeof( szInput );
	request.eTier = SIR_SCRIPT_OPT_TARGET_CPU;
	sirScriptOptimizerRecipeIngredients_t ingredients = {};
	SIRScriptSHA256::digest( "dependencies", 12, ingredients.szDependencies );
	SIRScriptSHA256::digest( "frontend", 8, ingredients.szFrontend );
	SIRScriptSHA256::digest( "optimizer", 9, ingredients.szOptimizer );
	SIRScriptSHA256::digest( "options", 7, ingredients.szOptions );
	sirScriptOptimizerRecipe_t recipe = {};
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::build( request, ingredients, szInput, &recipe ) == SIR_SCRIPT_OK );
	request.key = recipe.key;
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::verify( request, szInput, recipe.szBytes, sizeof( recipe.szBytes ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::verify( request, szInput, recipe.szBytes, 0 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	// Process-local job identity is deliberately absent from the persistent key.
	sirScriptOptimizationRequest_t second = request;
	second.nJobId = 1000;
	sirScriptOptimizerRecipe_t equivalent = {};
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::build( second, ingredients, szInput, &equivalent ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memcmp( recipe.szBytes, equivalent.szBytes, sizeof( recipe.szBytes ) ) == 0 );
	SIR_TEST_CHECK( memcmp( recipe.key.szDigest, equivalent.key.szDigest, 32 ) == 0 );
	// Both compiler/toolchain configuration and input content are key material.
	ingredients.szOptions[0] ^= 1;
	sirScriptOptimizerRecipe_t different = {};
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::build( request, ingredients, szInput, &different ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memcmp( recipe.key.szDigest, different.key.szDigest, 32 ) != 0 );
	uint8_t szMutated[256] = {};
	memcpy( szMutated, szInput, sizeof( szInput ) );
	szMutated[70] ^= 1;
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::verify( request, szMutated, recipe.szBytes, sizeof( recipe.szBytes ) ) ==
	                SIR_SCRIPT_TYPE_MISMATCH );
	sirScriptOptimizationRequest_t changed = request;
	++changed.artifact.nTargetId;
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::verify( changed, szInput, recipe.szBytes, sizeof( recipe.szBytes ) ) ==
	                SIR_SCRIPT_TYPE_MISMATCH );
	changed = request;
	++changed.artifact.nSchemaFingerprint;
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::verify( changed, szInput, recipe.szBytes, sizeof( recipe.szBytes ) ) ==
	                SIR_SCRIPT_TYPE_MISMATCH );
	uint8_t szDamaged[sizeof( recipe.szBytes )] = {};
	memcpy( szDamaged, recipe.szBytes, sizeof( szDamaged ) );
	szDamaged[185] ^= 1;
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::verify( request, szInput, szDamaged, sizeof( szDamaged ) ) == SIR_SCRIPT_TYPE_MISMATCH );
	memset( ingredients.szOptions, 0, sizeof( ingredients.szOptions ) );
	SIR_TEST_CHECK( SIRScriptOptimizationRecipe::build( request, ingredients, szInput, &different ) == SIR_SCRIPT_INVALID_ARGUMENT );
	puts( "SIREngine optimization recipe PASS" );
	return 0;
}
