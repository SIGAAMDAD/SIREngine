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

#ifndef __SIRENGINE_SIRWORLDHLODGRDG_H__
#define __SIRENGINE_SIRWORLDHLODGRDG_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <World/HLOD/SIRWorldHLOD.h>

class SIRResourceGraph;
class SIRResourceReadScope;

// Render-provider readiness is separate from GRDG CPU resource residency.
// The provider owns the underlying geometry allocation and its completion fence.
// The revision fields bind the provider's exact allocation to cooked HLOD and
// published GRDG geometry; world/device generations reject stale callbacks.
typedef struct sirWorldHLODProviderReady_s
{
	uint64_t nSourceRevision;
	uint64_t nResourceRevision;
	uint32_t nWorldGeneration;
	uint32_t nDeviceGeneration;
	bool bReady;
} sirWorldHLODProviderReady_t;

typedef struct sirWorldHLODGRDGInput_s
{
	const SIRResourceGraph *pGraph;
	const SIRResourceReadScope *pScope;
	const sirWorldHLODNode_t *pNodes;
	const sirWorldHLODProviderReady_t *pProviderReady;
	uint32_t nNodeCount;
	uint32_t nWorldGeneration;
	uint32_t nDeviceGeneration;
} sirWorldHLODGRDGInput_t;

typedef struct sirWorldHLODGRDGOutput_s
{
	sirWorldHLODResidency_t *pResidency;
	uint32_t nCapacity;
} sirWorldHLODGRDGOutput_t;

// Read-only bridge from canonical GRDG identity/revisions and provider readiness
// into the CPU HLOD cut planner. Never requests, loads, copies, or pins geometry.
// The caller owns a stable provider snapshot and a live GRDG read scope for the
// call, and must revalidate or pin selected resources before GPU submission.
// Invalid arguments/capacity leave the output array unchanged.
sirWorldHLODResult_t sirWorldHLODSnapshotGRDG( const sirWorldHLODGRDGInput_t& input, const sirWorldHLODGRDGOutput_t& output ) noexcept;

#endif // __SIRENGINE_SIRWORLDHLODGRDG_H__
