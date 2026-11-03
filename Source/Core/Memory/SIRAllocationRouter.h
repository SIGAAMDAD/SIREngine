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

#ifndef __SIRENGINE_SIRALLOCATIONROUTER_H__
#define __SIRENGINE_SIRALLOCATIONROUTER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRJobScratchAllocator.h>
#include <Core/Platform/SIRAtomic.h>

class SIRSizeClassAllocator;
class SIRReclaimableSizeClassAllocator;
class SIRFreeListAllocator;
class SIRTLSFAllocator;
class SIRBuddyAllocator;
class SIRFrameAllocator;

typedef enum sirAllocationLifetime_e : uint8_t
{
	SIR_LIFETIME_ARBITRARY,
	SIR_LIFETIME_JOB,
	SIR_LIFETIME_FRAME
} sirAllocationLifetime_t;

typedef enum sirRoutingBacking_e : uint8_t
{
	SIR_ROUTE_NONE,
	SIR_ROUTE_POOL,
	SIR_ROUTE_RECLAIMABLE,
	SIR_ROUTE_FREE_LIST,
	SIR_ROUTE_TLSF,
	SIR_ROUTE_BUDDY,
	SIR_ROUTE_JOB,
	SIR_ROUTE_FRAME
} sirRoutingBacking_t;

typedef struct sirRoutedAllocation_s
{
	void *pMemory;
	uint32_t nBytes;
	uint32_t nBacking;
} sirRoutedAllocation_t;

typedef struct sirAllocationRouterConfig_s
{
	SIRSizeClassAllocator *pPoolClasses;
	SIRReclaimableSizeClassAllocator *pReclaimableClasses;
	SIRFreeListAllocator *pFreeList;
	SIRTLSFAllocator *pTLSF;
	SIRBuddyAllocator *pBuddy;
	SIRJobScratchAllocator *pJobs;
	SIRFrameAllocator *pFrames;
	uint32_t nSmallMaximumBytes;
	uint32_t nLargeMinimumBytes;
	bool bUseReclaimableSmall;
	bool bUseTLSFMedium;
} sirAllocationRouterConfig_t;

// Optional, fixed dispatch. Backend access follows each backend's ownership
// contract; atomic route statistics alone do not serialize backend mutation.
// No allocation-owner discovery or heap fallback.
// Job/frame lifetime allocations are reclaimed only by their epoch owner.
class SIRAllocationRouter
{
public:
	SIRAllocationRouter( void );
	bool init( const sirAllocationRouterConfig_t& config );
	bool alloc( uint32_t nBytes, uint32_t nAlignment,
	            sirRoutedAllocation_t *pResult,
	            sirAllocationLifetime_t nLifetime = SIR_LIFETIME_ARBITRARY,
	            sirJobScratchToken_t job = { UINT32_MAX, 0 } );
	bool free( sirRoutedAllocation_t *pAllocation );
	uint64_t getRoutedCount( sirRoutingBacking_t backing ) const;

private:
	sirAllocationRouterConfig_t m_Config;
	SIRAtomic<uint64_t> m_nCounts[ SIR_ROUTE_FRAME + 1 ];
	uintptr_t m_nOwnerToken;
	bool m_bInitialized;
};

#endif // __SIRENGINE_SIRALLOCATIONROUTER_H__
