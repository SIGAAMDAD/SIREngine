/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRMEMORYLAYOUTPLANNER_H__
#define __SIRENGINE_SIRMEMORYLAYOUTPLANNER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/SIRMemoryManager.h>

typedef struct sirMemoryLayoutEntry_s
{
	uint64_t nReserveBytes;
	uint64_t nCommitLimitBytes;
	uint64_t nAllocatorMetadataBytes;
	bool bBuddyPowerOfTwo;
	bool bUse32BitOffsets;
} sirMemoryLayoutEntry_t;

typedef struct sirMemoryLayoutResult_s
{
	uint64_t nReservedDataBytes;
	uint64_t nEstimatedMetadataBytes;
	uint64_t nRemainingDataBytes;
	uint32_t nSegmentCount;
} sirMemoryLayoutResult_t;

// Dry run: never reserves, commits, or publishes any address or segment.
class SIRMemoryLayoutPlanner
{
public:
	static uint64_t estimateCompactBucketMetadata( uint64_t nSegmentBytes, uint32_t nSlabBytes, uint32_t nWorkerSeats,
	                                               uint32_t nAlignment );
	static uint64_t estimateCompactBucketColdMetadata( uint64_t nSegmentBytes, uint32_t nSlabBytes, uint32_t nWorkerSeats,
	                                                   uint32_t nAlignment );
	static bool preflight( const sirMemoryConfig_t &config, const sirMemoryBudgetProfile_t &budget, const sirMemoryLayoutEntry_t *pEntries,
	                       uint32_t nEntryCount, sirMemoryLayoutResult_t *pResult );
};

#endif // __SIRENGINE_SIRMEMORYLAYOUTPLANNER_H__
