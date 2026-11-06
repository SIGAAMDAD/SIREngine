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

#ifndef __SIRENGINE_SIRWORLDHLODDEMAND_H__
#define __SIRENGINE_SIRWORLDHLODDEMAND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <World/HLOD/SIRWorldHLOD.h>

// Cancellation revokes an outstanding request; it never evicts a loaded
// geometry resource. Loading, deduplication, and provider lifetimes belong
// to the shared streaming coordinator, GRDG, and renderer.
typedef enum sirWorldHLODDemandAction_e : uint8_t
{
	SIR_WORLD_HLOD_DEMAND_CANCEL = 0,
	SIR_WORLD_HLOD_DEMAND_REQUEST = 1
} sirWorldHLODDemandAction_t;

typedef struct sirWorldHLODDemandOperation_s
{
	sirWorldHLODDemand_t demand;
	sirWorldHLODDemandAction_t eAction;
} sirWorldHLODDemandOperation_t;

// Input arrays are the node-index-ascending unique demands returned by
// consecutive sirWorldHLODPlan calls. Each batch has one world generation;
// the generations of the previous/current batches may differ (teleport/reload).
typedef struct sirWorldHLODDemandDeltaInput_s
{
	const sirWorldHLODDemand_t *pPrevious;
	uint32_t nPreviousCount;
	const sirWorldHLODDemand_t *pCurrent;
	uint32_t nCurrentCount;
} sirWorldHLODDemandDeltaInput_t;

typedef struct sirWorldHLODDemandDeltaOutput_s
{
	sirWorldHLODDemandOperation_t *pOperations;
	uint32_t nCapacity;
	uint32_t *pnOperationCount;
} sirWorldHLODDemandDeltaOutput_t;

// Allocation-free O(previous + current) merge. A changed identity, revision,
// or world generation cancels the old request before issuing the replacement.
// Unchanged requests are left alone. On failure, neither operation bytes nor
// the output count changes. All input/output memory ranges must be disjoint.
// On success, the owner may adopt the current batch as its next previous batch
// only after the streaming coordinator accepts the complete operation list.
sirWorldHLODResult_t sirWorldHLODReconcileDemands( const sirWorldHLODDemandDeltaInput_t& input,
                                                   const sirWorldHLODDemandDeltaOutput_t& output ) noexcept;

#endif // __SIRENGINE_SIRWORLDHLODDEMAND_H__
