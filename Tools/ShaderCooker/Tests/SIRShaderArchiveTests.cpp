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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE( condition ) do { if ( !( condition ) ) { fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition ); abort(); } } while ( false )

static void testArchiveDedupAndRoundTrip( void ) noexcept
{
	const uint8_t szSpirv[8] = { 3, 2, 35, 7, 1, 2, 3, 4 };
	const uint8_t szSpirvOther[8] = { 3, 2, 35, 7, 5, 6, 7, 8 };
	sirShaderArchiveStageInput_t szInputs[4] = {};
	szInputs[0] = { SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_STAGE_VERTEX, 15, szSpirv, sizeof( szSpirv ) };
	szInputs[1] = { SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_STAGE_FRAGMENT, 16, szSpirv, sizeof( szSpirv ) };
	szInputs[2] = { SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_STAGE_VERTEX, 17, szSpirvOther, sizeof( szSpirvOther ) };
	szInputs[3] = { SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_STAGE_VERTEX, 15, szSpirv, sizeof( szSpirv ) };
	const sirShaderArchiveBuildDesc_t desc = { 400, SIR_SHADER_ARCHIVE_TARGET_ABI_V1, szInputs, 4 };
	sirShaderArchiveBuildResult_t result = {};
	uint8_t szArchive[512] = {};
	REQUIRE( sirShaderArchiveBuild( desc, nullptr, 0, &result ) == SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	REQUIRE( result.nUniqueCount == 2 );
	REQUIRE( result.nRequiredBytes == SIR_SHADER_ARCHIVE_HEADER_BYTES + 4 * 32 + 2 * 24 + 16 );
	uint8_t szBefore[512] = {};
	memcpy( szBefore, szArchive, sizeof( szArchive ) );
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, result.nRequiredBytes - 1, &result ) ==
	         SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	REQUIRE( memcmp( szBefore, szArchive, sizeof( szArchive ) ) == 0 );
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), &result ) == SIR_SHADER_COOK_OK );
	REQUIRE( result.nManifestHash != 0 );
	REQUIRE( sirShaderArchiveValidate( szArchive, result.nRequiredBytes ) == SIR_SHADER_COOK_OK );

	sirShaderArchiveStageView_t first = {};
	sirShaderArchiveStageView_t second = {};
	sirShaderArchiveStageView_t third = {};
	sirShaderArchiveStageView_t fourth = {};
	REQUIRE( sirShaderArchiveResolve( szArchive, result.nRequiredBytes, 0, &first ) == SIR_SHADER_COOK_OK );
	REQUIRE( sirShaderArchiveResolve( szArchive, result.nRequiredBytes, 1, &second ) == SIR_SHADER_COOK_OK );
	REQUIRE( sirShaderArchiveResolve( szArchive, result.nRequiredBytes, 2, &third ) == SIR_SHADER_COOK_OK );
	REQUIRE( sirShaderArchiveResolve( szArchive, result.nRequiredBytes, 3, &fourth ) == SIR_SHADER_COOK_OK );
	REQUIRE( first.pBytes == second.pBytes && first.pBytes == fourth.pBytes );
	REQUIRE( third.pBytes != first.pBytes );
	REQUIRE( first.nEntryPointHash == 15 && second.nEntryPointHash == 16 );
	REQUIRE( third.nByteCount == 8 && memcmp( third.pBytes, szSpirvOther, 8 ) == 0 );
	REQUIRE( sirShaderArchiveResolve( szArchive, result.nRequiredBytes, 4, &first ) ==
	         SIR_SHADER_COOK_INVALID_ARGUMENT );
	const uint64_t nFirstManifest = result.nManifestHash;
	uint8_t szRepeat[512] = {};
	REQUIRE( sirShaderArchiveBuild( desc, szRepeat, sizeof( szRepeat ), &result ) == SIR_SHADER_COOK_OK );
	REQUIRE( result.nManifestHash == nFirstManifest &&
	         memcmp( szRepeat, szArchive, ( size_t )result.nRequiredBytes ) == 0 );

	// Both the envelope checksum and per-blob hash are verified.
	szRepeat[result.nRequiredBytes - 1] ^= 1;
	REQUIRE( sirShaderArchiveValidate( szRepeat, result.nRequiredBytes ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szRepeat[result.nRequiredBytes - 1] ^= 1;
	szRepeat[28] ^= 1;
	REQUIRE( sirShaderArchiveValidate( szRepeat, result.nRequiredBytes ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szRepeat[28] ^= 1;
	szRepeat[52] = 2;
	REQUIRE( sirShaderArchiveValidate( szRepeat, result.nRequiredBytes ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szRepeat[52] = 1;
	szRepeat[64 + 16] = 255;
	REQUIRE( sirShaderArchiveValidate( szRepeat, result.nRequiredBytes ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}

static void testArchiveBadInputs( void ) noexcept
{
	const uint8_t szBytes[4] = { 3, 2, 35, 7 };
	sirShaderArchiveStageInput_t stage = { SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_STAGE_VERTEX, 13, szBytes, 4 };
	sirShaderArchiveBuildDesc_t desc = { 51, SIR_SHADER_ARCHIVE_TARGET_ABI_V1, &stage, 1 };
	sirShaderArchiveBuildResult_t result = {};
	uint8_t szArchive[512] = {};
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), nullptr ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	desc.nArchiveId = 0;
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), &result ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	desc.nArchiveId = 51;
	desc.nTargetAbiVersion = 2;
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), &result ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	desc.nTargetAbiVersion = 1;
	stage.eFormat = SIR_GRAPHICS_SHADER_FIXED_FUNCTION;
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), &result ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	stage.eFormat = SIR_GRAPHICS_SHADER_SPIRV;
	stage.nStageMask = 3;
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), &result ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	stage.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	stage.pBytes = nullptr;
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), &result ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	stage.pBytes = szBytes;
	stage.nByteCount = 0;
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), &result ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	stage.nByteCount = 4;
	REQUIRE( sirShaderArchiveValidate( nullptr, 0 ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	REQUIRE( sirShaderArchiveResolve( szArchive, 0, 0, nullptr ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	REQUIRE( sirShaderArchiveBuild( desc, szArchive, sizeof( szArchive ), &result ) == SIR_SHADER_COOK_OK );
	REQUIRE( sirShaderArchiveValidate( szArchive, result.nRequiredBytes - 1 ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}

int main( void )
{
	testArchiveDedupAndRoundTrip();
	testArchiveBadInputs();
	return 0;
}
