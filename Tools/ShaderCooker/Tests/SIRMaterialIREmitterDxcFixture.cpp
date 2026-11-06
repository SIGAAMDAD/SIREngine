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

#include <Tools/ShaderCooker/MaterialIR/SIRMaterialIREmitter.h>

#include <stdio.h>
#include <string.h>

static sirMaterialIRNode_t sirEmitterFixtureNode( sirMaterialIROp_t eOp, sirMaterialIRType_t eType ) noexcept
{
	sirMaterialIRNode_t node = {};
	node.eOp = eOp;
	node.eType = eType;
	node.iA = SIR_MATERIAL_IR_NO_INPUT;
	node.iB = SIR_MATERIAL_IR_NO_INPUT;
	node.iC = SIR_MATERIAL_IR_NO_INPUT;
	return node;
}

static bool sirEmitterFixtureWrite( const char *pszDirectory, const char *pszName, const char *pszPrelude, const char *pszBody ) noexcept
{
	char szPath[512] = {};
	const int nPath = snprintf( szPath, sizeof( szPath ), "%s/%s.hlsl", pszDirectory, pszName );
	if ( nPath < 0 || ( uint32_t )nPath >= sizeof( szPath ) ) {
		return false;
	}
	FILE *pFile = fopen( szPath, "wb" );
	if ( pFile == nullptr ) {
		return false;
	}
	const int nWritten =
	    fprintf( pFile, "%s\n%s\nfloat4 main( void ) : SV_Target0\n{\n\treturn sirEvaluateMaterialPass1();\n}\n", pszPrelude, pszBody );
	const bool bSuccess = nWritten > 0 && fflush( pFile ) == 0 && ferror( pFile ) == 0;
	const bool bClosed = fclose( pFile ) == 0;
	return bSuccess && bClosed;
}

static bool sirEmitterFixtureSwitch( const char *pszDirectory, uint32_t nMask, const char *pszName ) noexcept
{
	sirMaterialIRNode_t szNodes[4] = {};
	szNodes[0] = sirEmitterFixtureNode( SIR_MATERIAL_IR_PARAMETER, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[1] = sirEmitterFixtureNode( SIR_MATERIAL_IR_PARAMETER, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[2] = sirEmitterFixtureNode( SIR_MATERIAL_IR_STATIC_SWITCH, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[2].iA = 0;
	szNodes[2].iB = 1;
	szNodes[2].nSwitchBit = 1;
	szNodes[3] = sirEmitterFixtureNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[3].iA = 2;
	sirMaterialIRSwitch_t choice = {};
	choice.nBit = 1;
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 3, SIR_GRAPHICS_STAGE_FRAGMENT, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 301;
	graph.nSourceHash = 11;
	graph.pNodes = szNodes;
	graph.nNodeCount = 4;
	graph.pSwitches = &choice;
	graph.nSwitchCount = 1;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	const char *szSymbols[4] = { "ColorA", "ColorB", nullptr, nullptr };
	sirMaterialIREmitRequest_t request = { &graph, szSymbols, 4, nMask, SIR_GRAPHICS_FEATURE_GRAPHICS, SIR_SHADER_PASS_COLOR, nullptr };
	char szHLSL[2048] = {};
	sirMaterialIREmitOutput_t output = { szHLSL, sizeof( szHLSL ), 0 };
	if ( sirMaterialIREmitHLSL( request, &output ) != SIR_SHADER_COOK_OK ) {
		return false;
	}
	constexpr const char *PRELUDE = "cbuffer MaterialParams : register(b0)\n{\n\tfloat4 ColorA;\n\tfloat4 ColorB;\n};";
	return sirEmitterFixtureWrite( pszDirectory, pszName, PRELUDE, szHLSL );
}

static bool sirEmitterFixtureTexture( const char *pszDirectory ) noexcept
{
	sirMaterialIRNode_t szNodes[4] = {};
	szNodes[0] = sirEmitterFixtureNode( SIR_MATERIAL_IR_PARAMETER, SIR_MATERIAL_IR_TEXTURE2D );
	szNodes[1] = sirEmitterFixtureNode( SIR_MATERIAL_IR_PARAMETER, SIR_MATERIAL_IR_FLOAT2 );
	szNodes[2] = sirEmitterFixtureNode( SIR_MATERIAL_IR_SAMPLE_2D, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[2].iA = 0;
	szNodes[2].iB = 1;
	szNodes[3] = sirEmitterFixtureNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[3].iA = 2;
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 3, SIR_GRAPHICS_STAGE_FRAGMENT, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 302;
	graph.nSourceHash = 12;
	graph.pNodes = szNodes;
	graph.nNodeCount = 4;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	const char *szSymbols[4] = { "AlbedoTexture", "SurfaceUV", nullptr, nullptr };
	sirMaterialIREmitRequest_t request = { &graph, szSymbols, 4, 0, SIR_GRAPHICS_FEATURE_GRAPHICS, SIR_SHADER_PASS_COLOR, "LinearSampler" };
	char szHLSL[2048] = {};
	sirMaterialIREmitOutput_t output = { szHLSL, sizeof( szHLSL ), 0 };
	if ( sirMaterialIREmitHLSL( request, &output ) != SIR_SHADER_COOK_OK ) {
		return false;
	}
	constexpr const char *PRELUDE = "Texture2D<float4> AlbedoTexture : register(t0);\n"
	                                "SamplerState LinearSampler : register(s0);\n"
	                                "cbuffer MaterialParams : register(b0)\n{\n\tfloat2 SurfaceUV;\n};";
	return sirEmitterFixtureWrite( pszDirectory, "texture", PRELUDE, szHLSL );
}

int main( int nArgs, const char *const *pszArgs )
{
	if ( nArgs != 2 || pszArgs[1] == nullptr ) {
		return 2;
	}
	if ( !sirEmitterFixtureSwitch( pszArgs[1], 0, "switch-off" ) || !sirEmitterFixtureSwitch( pszArgs[1], 1, "switch-on" ) ||
	     !sirEmitterFixtureTexture( pszArgs[1] ) ) {
		return 1;
	}
	return 0;
}
