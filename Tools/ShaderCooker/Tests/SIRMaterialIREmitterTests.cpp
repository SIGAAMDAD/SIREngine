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
#include <stdlib.h>
#include <string.h>

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static sirMaterialIRNode_t makeNode( sirMaterialIROp_t eOp, sirMaterialIRType_t eType, uint16_t iA, uint16_t iB ) noexcept
{
	sirMaterialIRNode_t node = {};
	node.eOp = eOp;
	node.eType = eType;
	node.iA = iA;
	node.iB = iB;
	node.iC = SIR_MATERIAL_IR_NO_INPUT;
	return node;
}

static void testStaticSwitchPruningAndBounds( void ) noexcept
{
	sirMaterialIRNode_t szNodes[5] = {};
	szNodes[0] = makeNode( SIR_MATERIAL_IR_PARAMETER, SIR_MATERIAL_IR_FLOAT4, SIR_MATERIAL_IR_NO_INPUT, SIR_MATERIAL_IR_NO_INPUT );
	szNodes[1] = makeNode( SIR_MATERIAL_IR_PARAMETER, SIR_MATERIAL_IR_FLOAT4, SIR_MATERIAL_IR_NO_INPUT, SIR_MATERIAL_IR_NO_INPUT );
	szNodes[2] = makeNode( SIR_MATERIAL_IR_STATIC_SWITCH, SIR_MATERIAL_IR_FLOAT4, 0, 1 );
	szNodes[2].nSwitchBit = 1;
	szNodes[3] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 2, SIR_MATERIAL_IR_NO_INPUT );
	szNodes[4] = makeNode( SIR_MATERIAL_IR_PARAMETER, SIR_MATERIAL_IR_FLOAT4, SIR_MATERIAL_IR_NO_INPUT, SIR_MATERIAL_IR_NO_INPUT );
	sirMaterialIRSwitch_t choice = {};
	choice.nBit = 1;
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 3, SIR_GRAPHICS_STAGE_FRAGMENT, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 71;
	graph.nSourceHash = 45;
	graph.pNodes = szNodes;
	graph.nNodeCount = 5;
	graph.pSwitches = &choice;
	graph.nSwitchCount = 1;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	const char *szSymbols[5] = { "ColorA", "ColorB", nullptr, nullptr, nullptr };
	sirMaterialIREmitRequest_t request = { &graph, szSymbols, 5, 0, SIR_GRAPHICS_FEATURE_GRAPHICS, SIR_SHADER_PASS_COLOR, nullptr };
	char szText[1024] = {};
	sirMaterialIREmitOutput_t output = { szText, sizeof( szText ), 0 };
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( strstr( szText, "float4 sirEvaluateMaterialPass1( void )" ) != nullptr );
	REQUIRE( strstr( szText, "ColorB" ) != nullptr );
	REQUIRE( strstr( szText, "ColorA" ) == nullptr );
	REQUIRE( strstr( szText, "sirValue2 = sirValue1" ) != nullptr );
	const uint32_t nRequired = output.nRequiredBytes;
	output.nCapacity = 1;
	szText[0] = '!';
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	REQUIRE( output.nRequiredBytes == nRequired && szText[0] == '!' );
	output.nCapacity = sizeof( szText );
	request.nPermutationMask = 1;
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( strstr( szText, "ColorA" ) != nullptr );
	REQUIRE( strstr( szText, "ColorB" ) == nullptr );
	request.nPermutationMask = 2;
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	request.nPermutationMask = 0;
	szSymbols[1] = "Color.Bad";
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szSymbols[1] = "sirValue3";
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szSymbols[1] = "ColorB";
	request.nSymbolCount = 4;
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	request.nSymbolCount = 5;
	request.ePass = SIR_SHADER_PASS_DEPTH;
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_UNSUPPORTED );
}

static void testTextureSamplingFallback( void ) noexcept
{
	sirMaterialIRNode_t szNodes[4] = {};
	szNodes[0] = makeNode( SIR_MATERIAL_IR_PARAMETER, SIR_MATERIAL_IR_TEXTURE2D, SIR_MATERIAL_IR_NO_INPUT, SIR_MATERIAL_IR_NO_INPUT );
	szNodes[1] = makeNode( SIR_MATERIAL_IR_PARAMETER, SIR_MATERIAL_IR_FLOAT2, SIR_MATERIAL_IR_NO_INPUT, SIR_MATERIAL_IR_NO_INPUT );
	szNodes[2] = makeNode( SIR_MATERIAL_IR_SAMPLE_2D, SIR_MATERIAL_IR_FLOAT4, 0, 1 );
	szNodes[3] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 2, SIR_MATERIAL_IR_NO_INPUT );
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 3, SIR_GRAPHICS_STAGE_FRAGMENT, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 72;
	graph.nSourceHash = 46;
	graph.pNodes = szNodes;
	graph.nNodeCount = 4;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	const char *szSymbols[4] = { "AlbedoTexture", "SurfaceUV", nullptr, nullptr };
	sirMaterialIREmitRequest_t request = { &graph, szSymbols, 4, 0, SIR_GRAPHICS_FEATURE_GRAPHICS, SIR_SHADER_PASS_COLOR, "LinearSampler" };
	char szText[2048] = {};
	sirMaterialIREmitOutput_t output = { szText, sizeof( szText ), 0 };
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( strstr( szText, "AlbedoTexture.SampleLevel( LinearSampler, sirValue1, 0.0 )" ) != nullptr );
	request.pszSampler = nullptr;
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_UNSUPPORTED );
	request.pszSampler = "Not.Valid";
	REQUIRE( sirMaterialIREmitHLSL( request, &output ) == SIR_SHADER_COOK_UNSUPPORTED );
	request.pszSampler = "LinearSampler";
	REQUIRE( sirMaterialIREmitHLSL( request, nullptr ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}

int main( void )
{
	testStaticSwitchPruningAndBounds();
	testTextureSamplingFallback();
	return 0;
}
