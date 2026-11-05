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

#include <Tools/ShaderCooker/MaterialIR/SIRMaterialIR.h>

#include <stdio.h>
#include <stdlib.h>

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static sirMaterialIRNode_t makeNode( sirMaterialIROp_t eOp, sirMaterialIRType_t eType, uint16_t iA = SIR_MATERIAL_IR_NO_INPUT,
                                     uint16_t iB = SIR_MATERIAL_IR_NO_INPUT ) noexcept
{
	sirMaterialIRNode_t node = {};
	node.eOp = eOp;
	node.eType = eType;
	node.iA = iA;
	node.iB = iB;
	node.iC = SIR_MATERIAL_IR_NO_INPUT;
	return node;
}

static void testDeterministicMaps( void ) noexcept
{
	sirMaterialIRNode_t szNodes[5] = {};
	szNodes[0] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[1] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[2] = makeNode( SIR_MATERIAL_IR_STATIC_SWITCH, SIR_MATERIAL_IR_FLOAT4, 0, 1 );
	szNodes[2].nSwitchBit = 1;
	szNodes[3] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 2 );
	szNodes[4] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 0 );
	sirMaterialIRSwitch_t szSwitches[2] = {};
	szSwitches[0].nBit = 1;
	szSwitches[1].nBit = 2;
	szSwitches[1].nRequires = 1;
	sirMaterialIRPass_t szPasses[2] = {};
	szPasses[0] = { SIR_SHADER_PASS_COLOR, 3, SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT, SIR_GRAPHICS_FEATURE_GRAPHICS };
	szPasses[1] = { SIR_SHADER_PASS_DEPTH, 4, SIR_GRAPHICS_STAGE_VERTEX, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 17;
	graph.nSourceHash = 95;
	graph.pNodes = szNodes;
	graph.nNodeCount = 5;
	graph.pSwitches = szSwitches;
	graph.nSwitchCount = 2;
	graph.pPasses = szPasses;
	graph.nPassCount = 2;
	sirMaterialIRCompileOptions_t options = { SIR_GRAPHICS_FEATURE_GRAPHICS, true };
	sirMaterialShaderMapEntry_t szEntries[16] = {};
	sirMaterialIRCompileOutput_t output = { szEntries, 5, 0, 0 };
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	REQUIRE( output.nRequiredCount == 8 );
	REQUIRE( output.nProducedCount == 0 && szEntries[0].nCodeHash == 0 );
	output.nCapacity = 16;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( output.nProducedCount == 8 );
	REQUIRE( szEntries[0].nPermutationMask == 0 );
	REQUIRE( szEntries[1].nPermutationMask == 1 );
	REQUIRE( szEntries[2].nPermutationMask == 3 );
	REQUIRE( szEntries[3].bUberFallback && szEntries[3].nPermutationMask == UINT32_MAX );
	REQUIRE( szEntries[4].ePass == SIR_SHADER_PASS_DEPTH && szEntries[7].bUberFallback );
	REQUIRE( szEntries[0].nCodeHash != szEntries[1].nCodeHash );
	REQUIRE( szEntries[4].nCodeHash == szEntries[5].nCodeHash && szEntries[4].nCodeHash == szEntries[6].nCodeHash );
	const uint64_t nFirstHash = szEntries[0].nCodeHash;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( szEntries[0].nCodeHash == nFirstHash );
	// Node orders and pass hashes remain fixed when switch metadata order differs.
	sirMaterialIRSwitch_t temp = szSwitches[0];
	szSwitches[0] = szSwitches[1];
	szSwitches[1] = temp;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( szEntries[0].nCodeHash == nFirstHash );
	output.nCapacity = 1;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	REQUIRE( output.nRequiredCount == 8 && output.nProducedCount == 0 );
	REQUIRE( szEntries[0].nCodeHash == nFirstHash );
}

static void testInvalidGraphs( void ) noexcept
{
	sirMaterialIRNode_t szNodes[3] = {};
	szNodes[0] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT3 );
	szNodes[1] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[2] = makeNode( SIR_MATERIAL_IR_ADD, SIR_MATERIAL_IR_FLOAT4, 0, 1 );
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 2, SIR_GRAPHICS_STAGE_FRAGMENT, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 7;
	graph.nSourceHash = 11;
	graph.pNodes = szNodes;
	graph.nNodeCount = 3;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	sirMaterialShaderMapEntry_t szEntries[8] = {};
	sirMaterialIRCompileOutput_t output = { szEntries, 8, 0, 0 };
	const sirMaterialIRCompileOptions_t options = { SIR_GRAPHICS_FEATURE_GRAPHICS, false };
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szNodes[2] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 2 );
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szNodes[2] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 1 );
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	pass.nRequiredFeatures = 0;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	pass.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_GEOMETRY | SIR_GRAPHICS_STAGE_FRAGMENT;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nRequiredFeatures |= SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_UNSUPPORTED );
	const sirMaterialIRCompileOptions_t withGeometry = { SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER, false };
	REQUIRE( sirMaterialIRCompile( graph, withGeometry, &output ) == SIR_SHADER_COOK_OK );
	pass.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_TESS_CONTROL | SIR_GRAPHICS_STAGE_TESS_EVALUATION;
	pass.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	REQUIRE( sirMaterialIRCompile( graph, withGeometry, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nRequiredFeatures |= SIR_GRAPHICS_FEATURE_TESSELLATION;
	const sirMaterialIRCompileOptions_t withTessellation = { SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_TESSELLATION, false };
	REQUIRE( sirMaterialIRCompile( graph, withTessellation, &output ) == SIR_SHADER_COOK_OK );
	pass.nStageMask = SIR_GRAPHICS_STAGE_MESH | SIR_GRAPHICS_STAGE_FRAGMENT;
	pass.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	REQUIRE( sirMaterialIRCompile( graph, withGeometry, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nRequiredFeatures |= SIR_GRAPHICS_FEATURE_MESH_SHADER;
	const sirMaterialIRCompileOptions_t withMesh = { SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_MESH_SHADER, false };
	REQUIRE( sirMaterialIRCompile( graph, withMesh, &output ) == SIR_SHADER_COOK_OK );
	pass.ePass = SIR_SHADER_PASS_COMPUTE;
	pass.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	const sirMaterialIRCompileOptions_t withCompute = { SIR_GRAPHICS_FEATURE_COMPUTE, false };
	REQUIRE( sirMaterialIRCompile( graph, withCompute, &output ) == SIR_SHADER_COOK_OK );
	pass.ePass = SIR_SHADER_PASS_COLOR;
	pass.nStageMask = SIR_GRAPHICS_STAGE_FRAGMENT;
	pass.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	pass.nStageMask |= SIR_GRAPHICS_STAGE_COMPUTE;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nStageMask = SIR_GRAPHICS_STAGE_FRAGMENT;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	sirMaterialIRPass_t second = pass;
	second.ePass = SIR_SHADER_PASS_COLOR;
	sirMaterialIRPass_t szPasses[2] = { pass, second };
	graph.pPasses = szPasses;
	graph.nPassCount = 2;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	graph.nPassCount = 1;
	graph.pPasses = &pass;
	sirMaterialIRCompileOptions_t unsupported = { 0, false };
	REQUIRE( sirMaterialIRCompile( graph, unsupported, &output ) == SIR_SHADER_COOK_UNSUPPORTED );
	REQUIRE( output.nProducedCount == 0 && output.nRequiredCount == 0 );
	graph.nNodeCount = SIR_MATERIAL_IR_MAX_NODES + 1;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	graph.nNodeCount = 3;
	graph.pNodes = nullptr;
	output.nRequiredCount = 99;
	output.nProducedCount = 99;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	REQUIRE( output.nRequiredCount == 0 && output.nProducedCount == 0 );
}

static void testStaticDynamicSeparation( void ) noexcept
{
	sirMaterialIRNode_t szNodes[3] = {};
	szNodes[0] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[1] = makeNode( SIR_MATERIAL_IR_STATIC_SWITCH, SIR_MATERIAL_IR_FLOAT4, 0, 0 );
	szNodes[1].nSwitchBit = 1;
	szNodes[2] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 1 );
	sirMaterialIRSwitch_t choice = {};
	choice.nBit = 1;
	choice.bDynamic = true;
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 2, SIR_GRAPHICS_STAGE_VERTEX, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 5;
	graph.nSourceHash = 10;
	graph.pNodes = szNodes;
	graph.nNodeCount = 3;
	graph.pSwitches = &choice;
	graph.nSwitchCount = 1;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	sirMaterialShaderMapEntry_t szEntries[2] = {};
	sirMaterialIRCompileOutput_t output = { szEntries, 2, 0, 0 };
	sirMaterialIRCompileOptions_t options = { SIR_GRAPHICS_FEATURE_GRAPHICS, false };
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	choice.bDynamic = false;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	szNodes[1].iC = 0;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szNodes[1].iC = SIR_MATERIAL_IR_NO_INPUT;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( output.nProducedCount == 2 );
	choice.nRequires = 2;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	choice.nRequires = 0;
	choice.nExcludes = 1;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( output.nProducedCount == 1 );
}

static void testSampleOperandValidation( void ) noexcept
{
	sirMaterialIRNode_t szNodes[4] = {};
	szNodes[0] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_TEXTURE2D );
	szNodes[1] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT2 );
	szNodes[2] = makeNode( SIR_MATERIAL_IR_SAMPLE_2D, SIR_MATERIAL_IR_FLOAT4, 0, 1 );
	szNodes[3] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 2 );
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 3, SIR_GRAPHICS_STAGE_FRAGMENT, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 19;
	graph.nSourceHash = 101;
	graph.pNodes = szNodes;
	graph.nNodeCount = 4;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	sirMaterialShaderMapEntry_t szEntries[2] = {};
	sirMaterialIRCompileOutput_t output = { szEntries, 2, 0, 0 };
	const sirMaterialIRCompileOptions_t options = { SIR_GRAPHICS_FEATURE_GRAPHICS, false };
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	szNodes[2].iC = 0;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szNodes[2].iC = SIR_MATERIAL_IR_NO_INPUT;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
}

static void testReflectionBindingValidation( void ) noexcept
{
	sirMaterialIRNode_t szNodes[2] = {};
	szNodes[0] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[1] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 0 );
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 1, SIR_GRAPHICS_STAGE_FRAGMENT, SIR_GRAPHICS_FEATURE_GRAPHICS };
	uint8_t szBindings[2 * SIR_SHADER_PACKAGE_BINDING_BYTES] = {};
	szBindings[4] = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	szBindings[8] = SIR_SHADER_BINDING_MATERIAL;
	szBindings[9] = SIR_GRAPHICS_STAGE_FRAGMENT;
	szBindings[12] = 1;
	szBindings[16] = 0x42;
	szBindings[24 + 2] = 1;
	szBindings[24 + 4] = SIR_GRAPHICS_BINDING_SAMPLER;
	szBindings[24 + 8] = SIR_SHADER_BINDING_MATERIAL;
	szBindings[24 + 9] = SIR_GRAPHICS_STAGE_FRAGMENT;
	szBindings[24 + 12] = 1;
	szBindings[24 + 16] = 0x43;
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 47;
	graph.nSourceHash = 71;
	graph.pNodes = szNodes;
	graph.nNodeCount = 2;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	graph.pEncodedBindings = szBindings;
	graph.nBindingCount = 2;
	sirMaterialShaderMapEntry_t entry = {};
	sirMaterialIRCompileOutput_t output = { &entry, 1, 0, 0 };
	sirMaterialIRCompileOptions_t options = { SIR_GRAPHICS_FEATURE_GRAPHICS, false };
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( output.nRequiredCount == 1 && output.nProducedCount == 1 );
	const uint64_t nFirstSignature = entry.nReflectionSignature;
	szBindings[24 + 16] = 0x44;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( entry.nReflectionSignature != nFirstSignature );
	szBindings[24 + 16] = 0x43;
	szBindings[24 + 2] = 0;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szBindings[24 + 2] = 1;
	szBindings[2] = 2;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szBindings[2] = 0;
	szBindings[4] = SIR_GRAPHICS_BINDING_SAMPLER + 1;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szBindings[4] = SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE;
	szBindings[8] = SIR_SHADER_BINDING_DRAW + 1;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szBindings[8] = SIR_SHADER_BINDING_MATERIAL;
	szBindings[9] = SIR_GRAPHICS_STAGE_VERTEX;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szBindings[9] = SIR_GRAPHICS_STAGE_FRAGMENT;
	szBindings[12] = 0;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szBindings[12] = 1;
	szBindings[14] = 1;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szBindings[14] = 0;
	szBindings[16] = 0;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	szBindings[16] = 0x42;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( entry.nReflectionSignature == nFirstSignature );
}

static void testStageTopology( void ) noexcept
{
	sirMaterialIRNode_t szNodes[2] = {};
	szNodes[0] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[1] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 0 );
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 1, SIR_GRAPHICS_STAGE_VERTEX, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 42;
	graph.nSourceHash = 98;
	graph.pNodes = szNodes;
	graph.nNodeCount = 2;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	sirMaterialShaderMapEntry_t szEntries[1] = {};
	sirMaterialIRCompileOutput_t output = { szEntries, 1, 0, 0 };
	const sirMaterialIRCompileOptions_t options = { SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_MESH_SHADER |
	                                                    SIR_GRAPHICS_FEATURE_TESSELLATION | SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER,
	                                                false };
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	pass.nRequiredFeatures |= SIR_GRAPHICS_FEATURE_MESH_SHADER;
	pass.nStageMask = SIR_GRAPHICS_STAGE_TASK;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nStageMask = SIR_GRAPHICS_STAGE_MESH | SIR_GRAPHICS_STAGE_TASK;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	pass.nStageMask |= SIR_GRAPHICS_STAGE_VERTEX;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_TESSELLATION;
	pass.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_TESS_CONTROL;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nStageMask = SIR_GRAPHICS_STAGE_TESS_CONTROL | SIR_GRAPHICS_STAGE_TESS_EVALUATION;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nStageMask |= SIR_GRAPHICS_STAGE_VERTEX;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	pass.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER;
	pass.nStageMask = SIR_GRAPHICS_STAGE_GEOMETRY;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	pass.nStageMask |= SIR_GRAPHICS_STAGE_VERTEX;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
}

static void testPortablePipelineRecipes( void ) noexcept
{
	sirMaterialIRNode_t szNodes[4] = {};
	szNodes[0] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[1] = makeNode( SIR_MATERIAL_IR_CONSTANT, SIR_MATERIAL_IR_FLOAT4 );
	szNodes[2] = makeNode( SIR_MATERIAL_IR_STATIC_SWITCH, SIR_MATERIAL_IR_FLOAT4, 0, 1 );
	szNodes[2].nSwitchBit = 1;
	szNodes[3] = makeNode( SIR_MATERIAL_IR_OUTPUT, SIR_MATERIAL_IR_FLOAT4, 2 );
	sirMaterialIRSwitch_t choice = {};
	choice.nBit = 1;
	sirMaterialIRPass_t pass = { SIR_SHADER_PASS_COLOR, 3, SIR_GRAPHICS_STAGE_FRAGMENT, SIR_GRAPHICS_FEATURE_GRAPHICS };
	sirMaterialIRGraph_t graph = {};
	graph.nAssetId = 811;
	graph.nSourceHash = 33;
	graph.pNodes = szNodes;
	graph.nNodeCount = 4;
	graph.pSwitches = &choice;
	graph.nSwitchCount = 1;
	graph.pPasses = &pass;
	graph.nPassCount = 1;
	sirMaterialShaderMapEntry_t szEntries[3] = {};
	sirMaterialIRCompileOptions_t options = { SIR_GRAPHICS_FEATURE_GRAPHICS, true };
	sirMaterialIRCompileOutput_t output = { szEntries, 3, 0, 0 };
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( output.nProducedCount == 3 );
	sirMaterialIRPipelineSettings_t settings = {};
	settings.nPackageManifestHash = 0x102;
	settings.nRenderStateHash = 0x18;
	settings.nColorFormatsHash = 0x38;
	settings.nSampleCount = 4;
	settings.bAlphaMasked = true;
	sirShaderPipelineRecipe_t recipe = {};
	for ( uint32_t iEntry = 0; iEntry < output.nProducedCount; ++iEntry ) {
		REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, szEntries[iEntry], settings, &recipe ) == SIR_SHADER_COOK_OK );
		REQUIRE( recipe.pass.ePass == SIR_SHADER_PASS_COLOR );
		REQUIRE( recipe.pass.nBindingSignature == szEntries[iEntry].nReflectionSignature );
		REQUIRE( recipe.nAssetId == 811 && recipe.nSourceRevision == 33 && recipe.nPackageSchema == SIR_SHADER_PACKAGE_SCHEMA );
		REQUIRE( recipe.nPermutationKey == szEntries[iEntry].nPermutationMask );
		REQUIRE( recipe.pass.nSampleCount == 4 && recipe.pass.bAlphaMasked );
	}
	sirMaterialShaderMapEntry_t broken = szEntries[0];
	broken.nCodeHash ^= 1;
	recipe.nPackageSchema = 93;
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, broken, settings, &recipe ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	REQUIRE( recipe.nPackageSchema == 93 );
	broken = szEntries[0];
	broken.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, broken, settings, &recipe ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	broken = szEntries[0];
	broken.nReflectionSignature ^= 1;
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, broken, settings, &recipe ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	broken = szEntries[0];
	broken.nPermutationMask = 3;
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, broken, settings, &recipe ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	broken = szEntries[2];
	broken.bUberFallback = false;
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, broken, settings, &recipe ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	settings.nSampleCount = 3;
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, szEntries[0], settings, &recipe ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	settings.nSampleCount = 1;
	settings.nPackageManifestHash = 0;
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, szEntries[0], settings, &recipe ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	settings.nPackageManifestHash = 0x102;
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, szEntries[0], settings, nullptr ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	choice.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER;
	options.nAvailableFeatures |= SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER;
	REQUIRE( sirMaterialIRCompile( graph, options, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( ( szEntries[1].nRequiredFeatures & SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER ) != 0 );
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, szEntries[1], settings, &recipe ) == SIR_SHADER_COOK_OK );
	broken = szEntries[1];
	broken.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	REQUIRE( sirMaterialIRBuildPipelineRecipe( graph, broken, settings, &recipe ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}


int main( void )
{
	testDeterministicMaps();
	testInvalidGraphs();
	testStaticDynamicSeparation();
	testSampleOperandValidation();
	testReflectionBindingValidation();
	testStageTopology();
	testPortablePipelineRecipes();
	return 0;
}
