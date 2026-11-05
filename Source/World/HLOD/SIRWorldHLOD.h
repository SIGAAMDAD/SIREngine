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

#ifndef __SIRENGINE_SIRWORLDHLOD_H__
#define __SIRENGINE_SIRWORLDHLOD_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRResourceTypes.h>

// Flat parent-first tree; direct siblings occupy [firstChild, firstChild + childCount).
// Mesh data and canonical resource identity are owned by GRDG, never this selector.
typedef struct sirWorldHLODNode_s
{
	SIRResourceKey keyGeometry;
	uint64_t nSourceRevision;
	uint32_t nFirstChild;
	uint32_t nChildCount;
} sirWorldHLODNode_t;

typedef struct sirWorldHLODResidency_s
{
	uint64_t nSourceRevision;
	uint32_t nWorldGeneration;
	bool bReady;
} sirWorldHLODResidency_t;

// Transient caller-owned scratch, with one entry per node.
typedef struct sirWorldHLODScratch_s
{
	uint32_t nParentReferences;
	uint8_t bCanCover;
	uint8_t bUseChildren;
	uint8_t bActive;
	uint8_t bRequest;
} sirWorldHLODScratch_t;

typedef struct sirWorldHLODInput_s
{
	const sirWorldHLODNode_t *pNodes;
	const sirWorldHLODResidency_t *pResidency;
	const float *pProjectedError;
	uint32_t nNodeCount;
	uint32_t nWorldGeneration;
	float fMaxProjectedError;
} sirWorldHLODInput_t;

// Hysteresis state is owned by the world caller. Each byte is derived from a
// previously successful cut: bActive && bUseChildren in the returned scratch.
// Reset history on a new manifest, even if the world generation is unchanged.
typedef struct sirWorldHLODHistory_s
{
	const uint8_t *pPreviouslySplit;
	uint32_t nWorldGeneration;
	float fCoarsenProjectedError;
} sirWorldHLODHistory_t;

// Only a request descriptor: a streaming coordinator performs admission, deduplication
// and loading. The source revision must still be checked at actual publication.
typedef struct sirWorldHLODDemand_s
{
	SIRResourceKey keyGeometry;
	uint64_t nSourceRevision;
	uint32_t nWorldGeneration;
	uint32_t nNodeIndex;
} sirWorldHLODDemand_t;

typedef struct sirWorldHLODOutput_s
{
	sirWorldHLODScratch_t *pScratch;
	uint32_t nScratchCapacity;
	uint32_t *pSelectedNodes;
	uint32_t nSelectedCapacity;
	uint32_t *pnSelectedCount;
	sirWorldHLODDemand_t *pDemands;
	uint32_t nDemandCapacity;
	uint32_t *pnDemandCount;
} sirWorldHLODOutput_t;

typedef enum sirWorldHLODResult_e : uint8_t
{
	SIR_WORLD_HLOD_OK = 0,
	SIR_WORLD_HLOD_INVALID_ARGUMENT = 1,
	SIR_WORLD_HLOD_INVALID_HIERARCHY = 2,
	SIR_WORLD_HLOD_NO_RESIDENT_CUT = 3,
	SIR_WORLD_HLOD_CAPACITY = 4
} sirWorldHLODResult_t;

// Returns one coherent cut: a parent or its complete resident child cover, never both.
// Uncovered desired children become bounded demands. Output counts and arrays remain
// unchanged on failure; scratch may change. All input/output spans must not overlap.
// Caller must provide immutable residency for the duration of the call.
sirWorldHLODResult_t sirWorldHLODPlan( const sirWorldHLODInput_t& input, sirWorldHLODOutput_t& output ) noexcept;
// Historical state only affects desired refinement. Resident-parent fallback
// and atomic demand/output guarantees are identical to sirWorldHLODPlan.
sirWorldHLODResult_t sirWorldHLODPlanHysteretic( const sirWorldHLODInput_t& input, const sirWorldHLODHistory_t& history,
                                                 sirWorldHLODOutput_t& output ) noexcept;

#endif // __SIRENGINE_SIRWORLDHLOD_H__
