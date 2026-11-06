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

#include <Renderer/Shaders/SIRShaderMaterialManifest.h>

#include <stdio.h>
#include <string.h>

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( false )

static bool testCanonicalManifest( void ) noexcept
{
	sirShaderMaterialPassRecord_t szPasses[2] = {};
	szPasses[0] = { SIR_SHADER_PASS_SHADOW, 1, SIR_GRAPHICS_FEATURE_GRAPHICS, 0x77, 0x31 };
	szPasses[1] = { SIR_SHADER_PASS_COLOR, 0, SIR_GRAPHICS_FEATURE_GRAPHICS, 0x22, 0x25 };
	sirShaderMaterialManifestRequest_t request = { 0x11, 0x28, szPasses, 2, 2 };
	uint32_t nRequiredBytes = 0;
	REQUIRE( sirShaderBuildMaterialManifest( request, nullptr, 0, &nRequiredBytes ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	REQUIRE( nRequiredBytes == SIR_SHADER_MATERIAL_HEADER_BYTES + 2 * SIR_SHADER_MATERIAL_PASS_BYTES );
	uint8_t szManifest[128] = {};
	uint8_t szReordered[128] = {};
	REQUIRE( sirShaderBuildMaterialManifest( request, szManifest, sizeof( szManifest ), &nRequiredBytes ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( sirShaderValidateMaterial( szManifest, nRequiredBytes ) == SIR_GRAPHICS_SUCCESS );
	sirShaderMaterialHeader_t header = {};
	REQUIRE( sirShaderDecodeMaterialHeader( szManifest, nRequiredBytes, &header ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( header.nAssetId == 0x11 && header.nSourceRevision == 0x28 && header.nDependencyCount == 2 );
	REQUIRE( header.nPassCount == 2 && header.nTotalBytes == nRequiredBytes && header.nManifestHash != 0 );
	sirShaderMaterialPassRecord_t pass = {};
	REQUIRE( sirShaderDecodeMaterialPass( szManifest + SIR_SHADER_MATERIAL_HEADER_BYTES, SIR_SHADER_MATERIAL_PASS_BYTES, &pass ) ==
	         SIR_GRAPHICS_SUCCESS );
	REQUIRE( pass.nPass == SIR_SHADER_PASS_COLOR && pass.iShaderDependency == 0 && pass.nPermutationKey == 0x22 );
	REQUIRE( sirShaderDecodeMaterialPass( szManifest + SIR_SHADER_MATERIAL_HEADER_BYTES + SIR_SHADER_MATERIAL_PASS_BYTES,
	                                      SIR_SHADER_MATERIAL_PASS_BYTES, &pass ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( pass.nPass == SIR_SHADER_PASS_SHADOW && pass.iShaderDependency == 1 && pass.nRenderStateHash == 0x31 );
	uint64_t nManifestHash = 0;
	REQUIRE( sirShaderBuildMaterialHash( header, szManifest + SIR_SHADER_MATERIAL_HEADER_BYTES,
	                                     nRequiredBytes - SIR_SHADER_MATERIAL_HEADER_BYTES, &nManifestHash ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( nManifestHash == header.nManifestHash );
	const sirShaderMaterialPassRecord_t first = szPasses[0];
	szPasses[0] = szPasses[1];
	szPasses[1] = first;
	REQUIRE( sirShaderBuildMaterialManifest( request, szReordered, sizeof( szReordered ), nullptr ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( memcmp( szManifest, szReordered, nRequiredBytes ) == 0 );
	szReordered[SIR_SHADER_MATERIAL_HEADER_BYTES + 8] ^= 0x10;
	REQUIRE( sirShaderValidateMaterial( szReordered, nRequiredBytes ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	return true;
}

static bool testInvalidInputsLeaveBytesUntouched( void ) noexcept
{
	sirShaderMaterialPassRecord_t szPasses[2] = {};
	szPasses[0] = { SIR_SHADER_PASS_COLOR, 0, SIR_GRAPHICS_FEATURE_GRAPHICS, 1, 2 };
	szPasses[1] = { SIR_SHADER_PASS_DEPTH, 0, SIR_GRAPHICS_FEATURE_GRAPHICS, 3, 4 };
	sirShaderMaterialManifestRequest_t request = { 7, 8, szPasses, 2, 1 };
	uint8_t szOutput[128] = {};
	memset( szOutput, 0xA5, sizeof( szOutput ) );
	uint32_t nRequiredBytes = 999;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, 1, &nRequiredBytes ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	REQUIRE( nRequiredBytes == 112 );
	for ( uint32_t iByte = 0; iByte < sizeof( szOutput ); ++iByte ) {
		REQUIRE( szOutput[iByte] == 0xA5 );
	}
	nRequiredBytes = 999;
	szPasses[1].nPass = SIR_SHADER_PASS_COLOR;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), &nRequiredBytes ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( nRequiredBytes == 999 && szOutput[0] == 0xA5 );
	szPasses[1].nPass = SIR_SHADER_PASS_DEPTH;
	szPasses[1].iShaderDependency = 1;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	szPasses[1].iShaderDependency = 0;
	szPasses[0].nRequiredFeatures = 1ULL << 63;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	szPasses[0].nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	request.nAssetId = 0;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	request.nAssetId = 7;
	request.nSourceRevision = 0;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	request.nSourceRevision = 8;
	request.nPassCount = 0;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	request.nPassCount = SIR_SHADER_MATERIAL_MAX_PASSES + 1;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	request.nPassCount = 2;
	request.nDependencyCount = SIR_SHADER_MATERIAL_MAX_DEPENDENCIES + 1;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	request.nDependencyCount = 1;
	request.pPasses = nullptr;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( szOutput[0] == 0xA5 && szOutput[sizeof( szOutput ) - 1] == 0xA5 );
	return true;
}

static bool testFixedFunctionWithoutShaderDependency( void ) noexcept
{
	sirShaderMaterialPassRecord_t pass = { SIR_SHADER_PASS_COLOR, SIR_SHADER_MATERIAL_NO_SHADER, SIR_GRAPHICS_FEATURE_FIXED_FUNCTION, 0,
	                                       9 };
	sirShaderMaterialManifestRequest_t request = { 19, 21, &pass, 1, 0 };
	uint8_t szOutput[80] = {};
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( sirShaderValidateMaterial( szOutput, sizeof( szOutput ) ) == SIR_GRAPHICS_SUCCESS );
	pass.nPass = SIR_SHADER_PASS_COMPUTE;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	pass.nPass = SIR_SHADER_PASS_COLOR;
	pass.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	REQUIRE( sirShaderBuildMaterialManifest( request, szOutput, sizeof( szOutput ), nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	return true;
}

int main( void )
{
	if ( !testCanonicalManifest() || !testInvalidInputsLeaveBytesUntouched() || !testFixedFunctionWithoutShaderDependency() ) {
		fprintf( stderr, "SIRShaderMaterialManifestTests failed\n" );
		return 1;
	}
	puts( "SIRShaderMaterialManifestTests passed" );
	return 0;
}
