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
#include <Renderer/Drivers/D3D9/SIRD3D9Validation.h>
#include <string.h>

void sirD3D9FixedPipelinesInitialize( sirD3D9FixedPipelines_t *pPool, uint32_t nDeviceGeneration, uint32_t nTextureStages,
                                      uint32_t nTextures ) noexcept
{
	if ( pPool == nullptr ) {
		return;
	}
	*pPool = {};
	pPool->nDeviceGeneration = nDeviceGeneration;
	pPool->nMaximumTextureStages = nTextureStages < SIR_D3D9_MAX_FIXED_STAGES ? nTextureStages : SIR_D3D9_MAX_FIXED_STAGES;
	pPool->nMaximumTextures = nTextures < SIR_D3D9_MAX_FIXED_STAGES ? nTextures : SIR_D3D9_MAX_FIXED_STAGES;
	pPool->nMaximumLights = SIR_D3D9_MAX_FIXED_LIGHTS;
	for ( uint32_t i = 0; i < SIR_D3D9_MAX_FIXED_PIPELINES; ++i ) {
		pPool->slots[i].nGeneration = 1;
	}
}

sirGraphicsResult_t sirD3D9FixedPipelineCreate( sirD3D9FixedPipelines_t *pPool, const sirGraphicsABIPipeline_t *pDesc,
                                                sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pPool == nullptr || pDesc == nullptr || pHandle == nullptr || pPool->nDeviceGeneration == 0 ||
	     pDesc->nSize < sizeof( sirGraphicsABIPipeline_t ) || pDesc->nModel != SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ||
	     pDesc->pFixed == nullptr || pDesc->nReserved != 0 || pDesc->nType != SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION ||
	     pDesc->nBindingCount != 0 || pDesc->nColorFormatCount > 1 || pDesc->nSampleCount != 1 ||
	     pDesc->nTopology > SIR_GRAPHICS_TOPOLOGY_TRIANGLE_STRIP || pDesc->vertexShader.nByteCount != 0 ||
	     pDesc->pixelShader.nByteCount != 0 || pDesc->computeShader.nByteCount != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// The ABI position and optional normal are float4 streams. FVF cannot
	// describe their padded offsets; an explicit D3D9 vertex declaration is
	// required whenever fixed vertex transforms are enabled.
	const sirGraphicsABIFixedRecipe_t *pRecipe = pDesc->pFixed;
	const bool bNativeTransform = ( pRecipe->nRequiredFeatures & SIR_GRAPHICS_FIXED_TRANSFORM ) != 0;
	const bool bLighting = ( pRecipe->nRequiredFeatures & SIR_GRAPHICS_FIXED_LIGHTING ) != 0 || pRecipe->nLightCount != 0;
	if ( pDesc->nAttributeCount == 0 || pDesc->nAttributeCount > 4 || pDesc->pAttributes == nullptr ||
	     ( pRecipe->nRequiredFeatures & ( SIR_GRAPHICS_FIXED_CPU_TRANSFORM | SIR_GRAPHICS_FIXED_TEXCOORD_GEN ) ) != 0 ||
	     ( bLighting && !bNativeTransform ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	uint32_t nFVF = bNativeTransform ? 0x002U : 0x004U;
	uint32_t nVertexStride = 0;
	uint32_t nPreviousEnd = 0;
	bool bNormal = false;
	bool bDiffuse = false;
	bool bTexcoord = false;
	for ( uint32_t j = 0; j < pDesc->nAttributeCount; ++j ) {
		const sirGraphicsVertexAttribute_t *pAttr = &pDesc->pAttributes[j];
		if ( pAttr->nBufferSlot != 0 || pAttr->nStrideBytes == 0 || ( nVertexStride != 0 && pAttr->nStrideBytes != nVertexStride ) ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		nVertexStride = pAttr->nStrideBytes;
		uint32_t nSize = 0;
		if ( j == 0 && pAttr->nLocation == 0 && pAttr->nOffsetBytes == 0 && pAttr->eFormat == SIR_GRAPHICS_FORMAT_RGBA32_FLOAT ) {
			nSize = 16;
		} else if ( bNativeTransform && !bNormal && pAttr->nLocation == 3 && pAttr->eFormat == SIR_GRAPHICS_FORMAT_RGBA32_FLOAT ) {
			// XYZ normal, padded to four floats by the shared vertex format.
			nSize = 16;
			bNormal = true;
			nFVF |= 0x010U; // D3DFVF_NORMAL
		} else if ( !bDiffuse && pAttr->nLocation == 1 && pAttr->eFormat == SIR_GRAPHICS_FORMAT_BGRA8_UNORM ) {
			nSize = 4;
			bDiffuse = true;
			nFVF |= 0x040U; // D3DFVF_DIFFUSE
		} else if ( !bTexcoord && pAttr->nLocation == 2 && pAttr->eFormat == SIR_GRAPHICS_FORMAT_RG32_FLOAT ) {
			nSize = 8;
			bTexcoord = true;
			nFVF |= 0x100U; // D3DFVF_TEX1
		} else {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pAttr->nOffsetBytes < nPreviousEnd || pAttr->nOffsetBytes > UINT32_MAX - nSize ||
		     pAttr->nOffsetBytes + nSize > nVertexStride || pAttr->nOffsetBytes > UINT16_MAX ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		nPreviousEnd = pAttr->nOffsetBytes + nSize;
		// Packed FVF streams are legal only when their offsets follow exactly.
		if ( !bNativeTransform &&
		     ( ( j == 0 && pAttr->nOffsetBytes != 0 ) || ( j != 0 && pAttr->nOffsetBytes != nPreviousEnd - nSize ) ) ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	if ( bLighting && !bNormal ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !bNativeTransform ) {
		uint32_t nPackedEnd = 16;
		for ( uint32_t j = 1; j < pDesc->nAttributeCount; ++j ) {
			if ( pDesc->pAttributes[j].nOffsetBytes != nPackedEnd ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			nPackedEnd += pDesc->pAttributes[j].eFormat == SIR_GRAPHICS_FORMAT_BGRA8_UNORM ? 4U : 8U;
		}
	}
	if ( pDesc->nDepthTest != 0 || pDesc->nDepthWrite != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGraphicsResult_t eValid =
	    sirD3D9ValidateFixedRecipe( pRecipe, pPool->nMaximumTextureStages, pPool->nMaximumTextures, pPool->nMaximumLights );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	// The v1 fixed vertex layouts expose at most TEXCOORD0 and no
	// automatic coordinate generator. Reject recipes that would sample
	// missing coordinates or silently select another coordinate set.
	for ( uint32_t i = 0; i < pRecipe->nTextureStageCount; ++i ) {
		const sirGraphicsABIFixedStage_t *pStage = &pRecipe->pStages[i];
		if ( pStage->nCoordinateSet != 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		const bool bSamplesTexture =
		    ( pStage->nColorOperation != SIR_GRAPHICS_FIXED_OP_DISABLE && ( pStage->nColorArgument0 == SIR_GRAPHICS_FIXED_ARG_TEXTURE ||
			                                                                pStage->nColorArgument1 == SIR_GRAPHICS_FIXED_ARG_TEXTURE ) ) ||
		    ( pStage->nAlphaOperation != SIR_GRAPHICS_FIXED_OP_DISABLE &&
			  ( pStage->nAlphaArgument0 == SIR_GRAPHICS_FIXED_ARG_TEXTURE || pStage->nAlphaArgument1 == SIR_GRAPHICS_FIXED_ARG_TEXTURE ) );
		if ( bSamplesTexture && !bTexcoord ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	for ( uint32_t i = 0; i < SIR_D3D9_MAX_FIXED_PIPELINES; ++i ) {
		sirD3D9FixedPipeline_t *pSlot = &pPool->slots[i];
		if ( pSlot->bLive || pSlot->nGeneration == UINT32_MAX ) {
			continue;
		}
		const uint32_t nGeneration = pSlot->nGeneration;
		*pSlot = {};
		pSlot->nGeneration = nGeneration;
		pSlot->recipe = *pRecipe;
		if ( pRecipe->nTextureStageCount != 0 ) {
			memcpy( pSlot->stages, pRecipe->pStages, pRecipe->nTextureStageCount * sizeof( pSlot->stages[0] ) );
			pSlot->recipe.pStages = pSlot->stages;
		} else {
			pSlot->recipe.pStages = nullptr;
		}
		if ( pRecipe->nLightCount != 0 ) {
			memcpy( pSlot->lights, pRecipe->pLights, pRecipe->nLightCount * sizeof( pSlot->lights[0] ) );
			pSlot->recipe.pLights = pSlot->lights;
		} else {
			pSlot->recipe.pLights = nullptr;
		}
		pSlot->blend = pDesc->szBlendStates[0];
		pSlot->nCullMode = pDesc->nCullMode;
		pSlot->nDepthTest = pDesc->nDepthTest;
		pSlot->nDepthWrite = pDesc->nDepthWrite;
		pSlot->nDepthCompare = pDesc->nDepthCompare;
		pSlot->nTopology = pDesc->nTopology;
		pSlot->nFVF = nFVF;
		pSlot->bNativeDeclaration = bNativeTransform;
		pSlot->nElementCount = pDesc->nAttributeCount;
		memcpy( pSlot->elements, pDesc->pAttributes, pDesc->nAttributeCount * sizeof( pSlot->elements[0] ) );
		pSlot->nVertexStride = nVertexStride;
		pSlot->bLive = true;
		*pHandle = { i, nGeneration, pPool->nDeviceGeneration, SIR_GRAPHICS_HANDLE_PIPELINE };
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t sirD3D9FixedPipelineLookup( const sirD3D9FixedPipelines_t *pPool, sirGraphicsHandle_t hHandle,
                                                const sirD3D9FixedPipeline_t **ppPipeline ) noexcept
{
	if ( pPool == nullptr || ppPipeline == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppPipeline = nullptr;
	if ( hHandle.eType != SIR_GRAPHICS_HANDLE_PIPELINE || hHandle.nDeviceGeneration != pPool->nDeviceGeneration ||
	     hHandle.nSlot >= SIR_D3D9_MAX_FIXED_PIPELINES ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirD3D9FixedPipeline_t *pSlot = &pPool->slots[hHandle.nSlot];
	if ( !pSlot->bLive || pSlot->nGeneration != hHandle.nGeneration ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	*ppPipeline = pSlot;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9FixedPipelineRetire( sirD3D9FixedPipelines_t *pPool, sirGraphicsHandle_t hHandle ) noexcept
{
	const sirD3D9FixedPipeline_t *pSlot = nullptr;
	const sirGraphicsResult_t eLookup = sirD3D9FixedPipelineLookup( pPool, hHandle, &pSlot );
	if ( eLookup != SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	sirD3D9FixedPipeline_t *pWritable = &pPool->slots[hHandle.nSlot];
	pWritable->bLive = false;
	++pWritable->nGeneration;
	return SIR_GRAPHICS_SUCCESS;
}

void sirD3D9FixedPipelinesInvalidate( sirD3D9FixedPipelines_t *pPool, uint32_t nDeviceGeneration ) noexcept
{
	if ( pPool == nullptr ) {
		return;
	}
	const uint32_t nStages = pPool->nMaximumTextureStages;
	const uint32_t nTextures = pPool->nMaximumTextures;
	const uint32_t nLights = pPool->nMaximumLights;
	sirD3D9FixedPipelinesInitialize( pPool, nDeviceGeneration, nStages, nTextures );
	pPool->nMaximumLights = nLights;
}
