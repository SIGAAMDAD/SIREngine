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
#ifndef __SIRENGINE_SIRD3D9FIXEDPIPELINES_H__
#define __SIRENGINE_SIRD3D9FIXEDPIPELINES_H__
#include <SIRPCH.hpp>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <Renderer/SIRGraphicsDriverABI.h>

#define SIR_D3D9_MAX_FIXED_PIPELINES 16U
#define SIR_D3D9_MAX_FIXED_STAGES 8U
#define SIR_D3D9_MAX_FIXED_LIGHTS 8U

typedef struct sirD3D9FixedPipeline_s
{
	sirGraphicsABIFixedRecipe_t recipe;
	sirGraphicsABIFixedStage_t stages[SIR_D3D9_MAX_FIXED_STAGES];
	sirGraphicsABIFixedLight_t lights[SIR_D3D9_MAX_FIXED_LIGHTS];
	sirGraphicsABIBlendState_t blend;
	uint32_t nCullMode;
	uint32_t nDepthTest;
	uint32_t nDepthWrite;
	uint32_t nDepthCompare;
	uint32_t nFVF;
	uint32_t nElementCount;
	sirGraphicsVertexAttribute_t elements[4];
	bool bNativeDeclaration;
	uint32_t nTopology;
	uint32_t nVertexStride;
	uint32_t nGeneration;
	bool bLive;
} sirD3D9FixedPipeline_t;

typedef struct sirD3D9FixedPipelines_s
{
	sirD3D9FixedPipeline_t slots[SIR_D3D9_MAX_FIXED_PIPELINES];
	uint32_t nDeviceGeneration;
	uint32_t nMaximumTextureStages;
	uint32_t nMaximumTextures;
	uint32_t nMaximumLights;
} sirD3D9FixedPipelines_t;

void sirD3D9FixedPipelinesInitialize( sirD3D9FixedPipelines_t *pPool, uint32_t nDeviceGeneration, uint32_t nTextureStages,
                                      uint32_t nTextures ) noexcept;
sirGraphicsResult_t sirD3D9FixedPipelineCreate( sirD3D9FixedPipelines_t *pPool, const sirGraphicsABIPipeline_t *pDesc,
                                                sirGraphicsHandle_t *pHandle ) noexcept;
sirGraphicsResult_t sirD3D9FixedPipelineLookup( const sirD3D9FixedPipelines_t *pPool, sirGraphicsHandle_t hHandle,
                                                const sirD3D9FixedPipeline_t **ppPipeline ) noexcept;
sirGraphicsResult_t sirD3D9FixedPipelineRetire( sirD3D9FixedPipelines_t *pPool, sirGraphicsHandle_t hHandle ) noexcept;
void sirD3D9FixedPipelinesInvalidate( sirD3D9FixedPipelines_t *pPool, uint32_t nDeviceGeneration ) noexcept;
#endif // __SIRENGINE_SIRD3D9FIXEDPIPELINES_H__
