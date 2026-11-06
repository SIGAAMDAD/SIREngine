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

#ifndef __SIRENGINE_SIRHIZBINDINGS_H__
#define __SIRENGINE_SIRHIZBINDINGS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Visibility/SIRHiZCommands.h>

// The owner retains depth/pyramid textures and both pipelines. This module
// creates only dependent views, descriptor sets and a mapped-once upload buffer.
// The descriptor bindings are b0 (constant buffer), t0 (sampled mip view),
// u0 (storage mip view), represented as bindings 0/1/2 in space 0.
// Backend-specific depth-plane view support is intentionally explicit.
typedef struct sirHiZBindingsDesc_s
{
	const sirHiZDispatchPlan_t *pPlan;
	sirHiZDepthInput_t depth;
	sirGraphicsHandle_t hDepthTexture;
	sirGraphicsHandle_t hPyramidTexture;
	sirGraphicsHandle_t hCopyPipeline;
	sirGraphicsHandle_t hReducePipeline;
	uint32_t nParameterAlignment;
} sirHiZBindingsDesc_t;

typedef struct sirHiZBindings_s
{
	sirHiZDispatchPlan_t plan;
	sirGraphicsHandle_t hDepthTexture;
	sirGraphicsHandle_t hPyramidTexture;
	sirGraphicsHandle_t hCopyPipeline;
	sirGraphicsHandle_t hReducePipeline;
	sirGraphicsHandle_t hParameterBuffer;
	sirGraphicsHandle_t hDepthView;
	sirGraphicsHandle_t szSampleViews[32];
	sirGraphicsHandle_t szStorageViews[32];
	sirGraphicsHandle_t szPassSets[32];
	uint32_t nDeviceGeneration;
	uint32_t nPassCount;
	uint32_t nParameterStride;
	bool bMapped;
	bool bReady;
} sirHiZBindings_t;

// A zero-initialized output is required. On an allocation, mapping, or
// descriptor failure, the output retains all previously created handles and
// must be retired. Output bReady is true only after every binding succeeds.
// Currently only D32F input can be created as a sampled depth view here;
// D16/D24S8 depth-as-sampled imports require native backend qualification.
sirGraphicsResult_t sirHiZBindingsCreate( SIRRenderingDevice& device, const sirHiZBindingsDesc_t& desc,
                                          sirHiZBindings_t *pBindings ) noexcept;

// Fill the command recorder from the validated, copied plan and bound handles.
// Does not mutate the caller's input states. Callers must keep bindings stable
// until command recording completes; retired bindings cannot be recorded.
sirGraphicsResult_t sirHiZBindingsGetCommands( const sirHiZBindings_t& bindings, sirHiZCommandResources_t *pCommands ) noexcept;

// May be called repeatedly after partial allocation/retirement failures.
// Failed retirement leaves the outstanding handles in the backing for retry.
// The completion set must include all reader and writer GPU submissions.
sirGraphicsResult_t sirHiZBindingsRetire( SIRRenderingDevice& device, const sirGraphicsCompletionSet_t& completion,
                                          sirHiZBindings_t *pBindings ) noexcept;

// For irrecoverably lost devices, release stale host-side handle records only.
sirGraphicsResult_t sirHiZBindingsDiscardLost( uint32_t nReplacementGeneration, sirHiZBindings_t *pBindings ) noexcept;

#endif // __SIRENGINE_SIRHIZBINDINGS_H__
