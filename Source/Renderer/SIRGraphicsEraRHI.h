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

#ifndef __SIRENGINE_SIRGRAPHICSERARHI_H__
#define __SIRENGINE_SIRGRAPHICSERARHI_H__

#include <SIRPCH.hpp>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/SIRRenderingDevice.h>

// Shared input for the programmable and fixed-function serial draw models.
// The pipeline must already have been created from an admitted cooked recipe.
// Native handles and the arrays remain borrowed. GRDG retains provider leases
// until native completion; no CPU replay is reported as GPU retirement.
typedef struct sirGraphicsEraBinding_s
{
	uint32_t nSlot;
	sirGraphicsHandle_t hSet;
} sirGraphicsEraBinding_t;

typedef struct sirGraphicsEraStream_s
{
	uint32_t nSlot;
	sirGraphicsHandle_t hBuffer;
	uint64_t nOffsetBytes;
	uint32_t nStrideBytes;
} sirGraphicsEraStream_t;

typedef struct sirGraphicsEraDraw_s
{
	sirGraphicsHandle_t hPipeline;
	const sirGraphicsEraBinding_t *pBindings;
	const sirGraphicsEraStream_t *pStreams;
	uint32_t nBindingCount;
	uint32_t nStreamCount;
	sirGraphicsHandle_t hIndexBuffer;
	uint64_t nIndexOffsetBytes;
	uint32_t nIndex32;
	uint32_t nIndexed;
	uint32_t nElementCount;
	uint32_t nFirstElement;
	int32_t nVertexOffset;
} sirGraphicsEraDraw_t;

typedef struct sirGraphicsEraBatch_s
{
	uint32_t nDeviceGeneration;
	sirGraphicsPipelineModel_t eModel;
	const sirGraphicsRenderPass_t *pPass;
	const sirGraphicsViewport_t *pViewport;
	const sirGraphicsRect_t *pScissor;
	const sirGraphicsEraDraw_t *pDraws;
	uint32_t nDrawCount;
} sirGraphicsEraBatch_t;

// Pipeline validation delegates to the one existing ABI v1 validator.
// No independent description of fixed stages, shader profiles or formats.
class SIRGraphicsEraRHI
{
public:
	static sirGraphicsResult_t admitPipeline( const sirGraphicsABIPipeline_t& pipeline,
	                                          const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept;
	static sirGraphicsResult_t admitBatch( const sirGraphicsEraBatch_t& batch, const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept;
	static sirGraphicsResult_t record( const sirGraphicsEraBatch_t& batch, SIRGraphicsCommandList& commands,
	                                   const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept;
	static sirGraphicsResult_t execute( const sirGraphicsEraBatch_t& batch, SIRRenderingDevice& device,
	                                    sirGraphicsCompletion_t *pCompletion ) noexcept;

protected:
private:
};

class SIRGraphicsProgrammableRHI
{
public:
	static sirGraphicsResult_t admitPipeline( const sirGraphicsABIPipeline_t& pipeline,
	                                          const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept;
	static sirGraphicsResult_t execute( const sirGraphicsEraBatch_t& batch, SIRRenderingDevice& device,
	                                    sirGraphicsCompletion_t *pCompletion ) noexcept;

protected:
private:
};

class SIRGraphicsFixedFunctionRHI
{
public:
	static sirGraphicsResult_t admitPipeline( const sirGraphicsABIPipeline_t& pipeline,
	                                          const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept;
	static sirGraphicsResult_t execute( const sirGraphicsEraBatch_t& batch, SIRRenderingDevice& device,
	                                    sirGraphicsCompletion_t *pCompletion ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRGRAPHICSERARHI_H__
