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

#ifndef __SIRENGINE_SIRHIZCOMMANDS_H__
#define __SIRENGINE_SIRHIZCOMMANDS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/Visibility/SIRHiZDispatch.h>

// Pipelines, per-mip descriptor sets, textures and constants are owned by the
// caller. Each set must bind the matching 32-byte b0 parameters, t0 source
// view and u0 destination mip view from the checked dispatch plan.
// Command recording changes the source texture to SHADER_READ and all pyramid
// mips to SHADER_READ. Resource lifetime and queue submission stay with the
// native device and caller; this function allocates no GPU resources.
typedef struct sirHiZCommandResources_s
{
	const sirHiZDispatchPlan_t *pPlan;
	const sirGraphicsHandle_t *pPassSets;
	uint32_t nPassSetCount;
	sirGraphicsHandle_t hDepthTexture;
	sirGraphicsHandle_t hPyramidTexture;
	sirGraphicsHandle_t hCopyPipeline;
	sirGraphicsHandle_t hReducePipeline;
	sirGraphicsState_t eDepthBefore;
	sirGraphicsState_t ePyramidBefore;
} sirHiZCommandResources_t;

// All argument validation happens before recording. If a driver call fails,
// the command list has partial commands and must be reset/discarded by caller.
// A successful return means recorded commands, not GPU completion.
sirGraphicsResult_t sirHiZRecordPyramid( SIRGraphicsCommandList& commands, const sirHiZCommandResources_t& resources ) noexcept;

#endif // __SIRENGINE_SIRHIZCOMMANDS_H__
