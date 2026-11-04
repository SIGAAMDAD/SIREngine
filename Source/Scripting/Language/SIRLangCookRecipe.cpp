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

#include <Scripting/Language/SIRLangCookRecipe.h>

#include <string.h>

constexpr uint32_t SIR_LANG_COOK_RECIPE_MAX_IMPORTS = 64;
constexpr uint32_t SIR_LANG_COOK_RECIPE_HEADER_BYTES = 108;
constexpr uint32_t SIR_LANG_COOK_RECIPE_IMPORT_BYTES = 40;

static void sirLangCookWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( iByte * 8 ) );
	}
}

static void sirLangCookWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( iByte * 8 ) );
	}
}

static bool sirLangCookNonzeroDigest( const uint8_t szDigest[32] ) noexcept
{
	uint8_t nAccumulated = 0;
	for ( uint32_t iByte = 0; iByte < 32; ++iByte ) {
		nAccumulated |= szDigest[iByte];
	}
	return nAccumulated != 0;
}

sirScriptResult_t SIRLangMakeCookKey( const sirLangCookIdentity_t& identity, const sirLangCookImport_t *pImports,
                                      uint32_t nImportCount, sirScriptCacheKey_t *pKey ) noexcept
{
	if ( pKey == nullptr || identity.nModuleId == 0 || identity.nSchemaId == 0 ||
	     identity.nLayoutDigest == 0 || identity.nBindingDigest == 0 || identity.nTargetId == 0 ||
	     identity.nToolchainId == 0 || nImportCount > SIR_LANG_COOK_RECIPE_MAX_IMPORTS ||
	     ( nImportCount != 0 && pImports == nullptr ) || !sirLangCookNonzeroDigest( identity.szSourceDigest ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}

	// Imports must be the unique, sorted transitive closure. The header's
	// format and VM revisions also invalidate older cached recipes.
	uint64_t nPreviousModule = 0;
	for ( uint32_t iImport = 0; iImport < nImportCount; ++iImport ) {
		const sirLangCookImport_t& dependency = pImports[iImport];
		if ( dependency.nModuleId <= nPreviousModule || dependency.nModuleId == identity.nModuleId ||
		     !sirLangCookNonzeroDigest( dependency.szDigest ) ) {
			return SIR_SCRIPT_INVALID_ARGUMENT;
		}
		nPreviousModule = dependency.nModuleId;
	}

	uint8_t szRecipe[SIR_LANG_COOK_RECIPE_HEADER_BYTES +
	                 SIR_LANG_COOK_RECIPE_IMPORT_BYTES * SIR_LANG_COOK_RECIPE_MAX_IMPORTS] = {};
	memcpy( szRecipe, "SIRCRP01", 8 );
	sirLangCookWrite32( szRecipe + 8, 1 );
	sirLangCookWrite32( szRecipe + 12, SIR_LANG_VM_VERSION );
	sirLangCookWrite64( szRecipe + 16, identity.nModuleId );
	sirLangCookWrite64( szRecipe + 24, identity.nSchemaId );
	sirLangCookWrite64( szRecipe + 32, identity.nLayoutDigest );
	sirLangCookWrite64( szRecipe + 40, identity.nBindingDigest );
	sirLangCookWrite64( szRecipe + 48, identity.nTargetId );
	sirLangCookWrite64( szRecipe + 56, identity.nToolchainId );
	sirLangCookWrite64( szRecipe + 64, identity.nOptimizationFlags );
	memcpy( szRecipe + 72, identity.szSourceDigest, 32 );
	sirLangCookWrite32( szRecipe + 104, nImportCount );

	for ( uint32_t iImport = 0; iImport < nImportCount; ++iImport ) {
		const uint32_t iOffset = SIR_LANG_COOK_RECIPE_HEADER_BYTES + iImport * SIR_LANG_COOK_RECIPE_IMPORT_BYTES;
		sirLangCookWrite64( szRecipe + iOffset, pImports[iImport].nModuleId );
		memcpy( szRecipe + iOffset + 8, pImports[iImport].szDigest, 32 );
	}

	const uint64_t nRecipeBytes = SIR_LANG_COOK_RECIPE_HEADER_BYTES +
	                               ( uint64_t )nImportCount * SIR_LANG_COOK_RECIPE_IMPORT_BYTES;
	return SIRScriptArtifactCache::makeKey( szRecipe, nRecipeBytes, pKey );
}
