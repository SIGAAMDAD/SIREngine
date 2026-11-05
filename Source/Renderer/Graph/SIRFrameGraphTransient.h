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

#ifndef __SIRENGINE_SIRFRAMEGRAPHTRANSIENT_H__
#define __SIRENGINE_SIRFRAMEGRAPHTRANSIENT_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Renderer/Graph/SIRFrameGraph.h>

// Provider-supplied requirements: the graph must never infer native allocation
// size, placement compatibility, physical backing identity or budget headroom.
typedef struct sirFrameGraphTransientDesc_s
{
	uint32_t nResource;
	uint64_t nRequiredBytes;
	uint64_t nAlignmentBytes;
	uint64_t nProviderCompatibilityKey;
	sirGraphicsMemoryClass_t eMemory;
	sirGraphicsFormat_t eFormat;
	uint32_t nSampleCount;
	bool bAllowAlias;
} sirFrameGraphTransientDesc_t;

typedef struct sirFrameGraphTransientInput_s
{
	const sirFrameGraphTransientDesc_t *pDescs;
	uint32_t nDescCount;
	uint64_t nMaxBackingBytes;
} sirFrameGraphTransientInput_t;

// Each slot is a reservation request, not a physical GPU allocation. The
// provider must verify layout/placement, completion sets and actual budgets
// before binding or sharing native backing. No physical bytes are charged here.
typedef struct sirFrameGraphTransientSlot_s
{
	uint32_t nRepresentativeResource;
	uint32_t nLastUse;
	uint64_t nRequiredBytes;
	uint64_t nAlignmentBytes;
	uint64_t nProviderCompatibilityKey;
	sirGraphicsMemoryClass_t eMemory;
	sirGraphicsHandleType_t eType;
	sirGraphicsFormat_t eFormat;
	sirGraphicsQueue_t eQueue;
	uint32_t nSampleCount;
	bool bReuseAllowed;
} sirFrameGraphTransientSlot_t;

typedef struct sirFrameGraphTransientPlan_s
{
	uint32_t nSlotCount;
	uint32_t nResourceCount;
	uint64_t nLogicalBytes;
	uint64_t nRequestedBackingBytes;
	uint32_t szResourceSlot[SIR_FRAMEGRAPH_MAX_RESOURCES];
	sirFrameGraphTransientSlot_t szSlots[SIR_FRAMEGRAPH_MAX_RESOURCES];
} sirFrameGraphTransientPlan_t;

#endif // __SIRENGINE_SIRFRAMEGRAPHTRANSIENT_H__