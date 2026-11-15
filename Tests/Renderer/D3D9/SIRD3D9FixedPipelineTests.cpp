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
#include <Renderer/Drivers/D3D9/SIRD3D9FixedPipelines.h>
#include <stdlib.h>

static void sirRequire( bool bResult )
{
	if ( !bResult ) {
		abort();
	}
}

int main( void )
{
	sirD3D9FixedPipelines_t pool = {};
	sirD3D9FixedPipelinesInitialize( &pool, 4, 4, 4 );
	sirGraphicsABIFixedStage_t stage = {};
	stage.nTextureUnit = 0;
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_MODULATE;
	stage.nAlphaOperation = SIR_GRAPHICS_FIXED_OP_SELECT;
	stage.nColorArgument0 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	stage.nColorArgument1 = SIR_GRAPHICS_FIXED_ARG_DIFFUSE;
	stage.nAlphaArgument0 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	stage.nAlphaArgument1 = SIR_GRAPHICS_FIXED_ARG_DIFFUSE;
	sirGraphicsABIFixedRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nTextureStageCount = 1;
	recipe.pStages = &stage;
	recipe.nAlphaCompare = SIR_GRAPHICS_COMPARE_ALWAYS;
	recipe.fAlphaReference = 0.5f;
	sirGraphicsABIPipeline_t desc = {};
	desc.nSize = sizeof( desc );
	desc.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	desc.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	desc.nSampleCount = 1;
	desc.nTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	desc.nCullMode = SIR_GRAPHICS_CULL_BACK;
	desc.szBlendStates[0].nEnabled = 1;
	desc.szBlendStates[0].nSourceColor = SIR_GRAPHICS_BLEND_SRC_ALPHA;
	desc.szBlendStates[0].nDestinationColor = SIR_GRAPHICS_BLEND_ONE_MINUS_SRC_ALPHA;
	sirGraphicsVertexAttribute_t attributes[2] = {};
	attributes[0].nLocation = 0;
	attributes[0].nStrideBytes = 24;
	attributes[0].eFormat = SIR_GRAPHICS_FORMAT_RGBA32_FLOAT;
	attributes[1].nLocation = 2;
	attributes[1].nOffsetBytes = 16;
	attributes[1].nStrideBytes = 24;
	attributes[1].eFormat = SIR_GRAPHICS_FORMAT_RG32_FLOAT;
	desc.pAttributes = attributes;
	desc.nAttributeCount = 2;
	desc.pFixed = &recipe;
	sirGraphicsHandle_t handle = {};
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &handle ) == SIR_GRAPHICS_SUCCESS );
	const sirD3D9FixedPipeline_t *pFound = nullptr;
	sirRequire( sirD3D9FixedPipelineLookup( &pool, handle, &pFound ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( pFound->recipe.pStages == pFound->stages );
	sirRequire( pFound->nFVF == ( 0x004U | 0x100U ) && pFound->nVertexStride == 24 );
	sirRequire( pFound->nCullMode == SIR_GRAPHICS_CULL_BACK && pFound->blend.nSourceColor == SIR_GRAPHICS_BLEND_SRC_ALPHA );
	stage.nColorOperation = SIR_GRAPHICS_FIXED_OP_DISABLE;
	sirRequire( pFound->stages[0].nColorOperation == SIR_GRAPHICS_FIXED_OP_MODULATE );
	sirRequire( sirD3D9FixedPipelineRetire( &pool, handle ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( sirD3D9FixedPipelineLookup( &pool, handle, &pFound ) == SIR_GRAPHICS_INVALID_HANDLE );
	desc.pFixed = &recipe;
	sirGraphicsHandle_t next = {};
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( next.nGeneration != handle.nGeneration );
	sirD3D9FixedPipelinesInvalidate( &pool, 5 );
	sirRequire( sirD3D9FixedPipelineLookup( &pool, next, &pFound ) == SIR_GRAPHICS_INVALID_HANDLE );
	desc.nDepthTest = 1;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.nDepthTest = 0;
	desc.nDepthWrite = 1;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.nDepthWrite = 0;
	desc.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	// A sampled texture with no vertex coordinates must be rejected.
	desc.nAttributeCount = 1;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.nAttributeCount = 2;
	stage.nCoordinateSet = 1;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	stage.nCoordinateSet = 0;
	// A diffuse-only recipe is valid without texture coordinates.
	stage.nColorArgument0 = SIR_GRAPHICS_FIXED_ARG_DIFFUSE;
	stage.nAlphaArgument0 = SIR_GRAPHICS_FIXED_ARG_DIFFUSE;
	desc.nAttributeCount = 1;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( sirD3D9FixedPipelineRetire( &pool, next ) == SIR_GRAPHICS_SUCCESS );
	// Native XYZ transform uses the existing float4 position stream,
	// consuming only XYZ while preserving the API-defined stride.
	recipe.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( sirD3D9FixedPipelineLookup( &pool, next, &pFound ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( pFound->nFVF == 0x002U );
	sirRequire( sirD3D9FixedPipelineRetire( &pool, next ) == SIR_GRAPHICS_SUCCESS );
	desc.nAttributeCount = 2;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( sirD3D9FixedPipelineLookup( &pool, next, &pFound ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( pFound->bNativeDeclaration && pFound->nElementCount == 2 );
	sirRequire( sirD3D9FixedPipelineRetire( &pool, next ) == SIR_GRAPHICS_SUCCESS );
	desc.nAttributeCount = 1;
	recipe.nRequiredFeatures |= SIR_GRAPHICS_FIXED_CPU_TRANSFORM;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	recipe.nRequiredFeatures = SIR_GRAPHICS_FIXED_LIGHTING;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	// Each primitive topology has its own count/validation semantics.
	recipe.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM;
	desc.nAttributeCount = 1;
	const uint32_t aTopologies[] = { SIR_GRAPHICS_TOPOLOGY_POINTS, SIR_GRAPHICS_TOPOLOGY_LINES, SIR_GRAPHICS_TOPOLOGY_LINE_STRIP,
	                                 SIR_GRAPHICS_TOPOLOGY_TRIANGLES, SIR_GRAPHICS_TOPOLOGY_TRIANGLE_STRIP };
	for ( uint32_t nTopology : aTopologies ) {
		desc.nTopology = nTopology;
		sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_SUCCESS );
		sirRequire( sirD3D9FixedPipelineRetire( &pool, next ) == SIR_GRAPHICS_SUCCESS );
	}
	desc.nTopology = SIR_GRAPHICS_TOPOLOGY_PATCHES;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	// Hardware fixed lighting requires a transformed stream with a normal.
	sirGraphicsVertexAttribute_t litAttributes[4] = {};
	litAttributes[0] = attributes[0];
	litAttributes[0].nStrideBytes = 44;
	litAttributes[1].nLocation = 3;
	litAttributes[1].nOffsetBytes = 16;
	litAttributes[1].nStrideBytes = 44;
	litAttributes[1].eFormat = SIR_GRAPHICS_FORMAT_RGBA32_FLOAT;
	litAttributes[2].nLocation = 1;
	litAttributes[2].nOffsetBytes = 32;
	litAttributes[2].nStrideBytes = 44;
	litAttributes[2].eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	litAttributes[3].nLocation = 2;
	litAttributes[3].nOffsetBytes = 36;
	litAttributes[3].nStrideBytes = 44;
	litAttributes[3].eFormat = SIR_GRAPHICS_FORMAT_RG32_FLOAT;
	desc.pAttributes = litAttributes;
	desc.nAttributeCount = 4;
	recipe.nRequiredFeatures = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING;
	sirGraphicsABIFixedLight_t light = {};
	light.fRange = 100.0f;
	recipe.nLightCount = 1;
	recipe.pLights = &light;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( sirD3D9FixedPipelineLookup( &pool, next, &pFound ) == SIR_GRAPHICS_SUCCESS );
	sirRequire( pFound->bNativeDeclaration && pFound->nElementCount == 4 && pFound->recipe.pLights == pFound->lights );
	sirRequire( pFound->nFVF == ( 0x002U | 0x010U | 0x040U | 0x100U ) );
	sirRequire( sirD3D9FixedPipelineRetire( &pool, next ) == SIR_GRAPHICS_SUCCESS );
	litAttributes[1].nOffsetBytes = 20;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	litAttributes[1].nOffsetBytes = 16;
	litAttributes[1].nLocation = 4;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	litAttributes[1].nLocation = 3;
	recipe.nLightCount = 0;
	recipe.pLights = nullptr;
	desc.nAttributeCount = 1;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.pAttributes = attributes;
	recipe.nRequiredFeatures = SIR_GRAPHICS_FIXED_TEXCOORD_GEN;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	recipe.nRequiredFeatures = 0;
	desc.nAttributeCount = 2;
	stage.nColorArgument0 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	stage.nAlphaArgument0 = SIR_GRAPHICS_FIXED_ARG_TEXTURE;
	recipe.nTextureStageCount = 5;
	sirRequire( sirD3D9FixedPipelineCreate( &pool, &desc, &next ) == SIR_GRAPHICS_UNSUPPORTED );
	return 0;
}
