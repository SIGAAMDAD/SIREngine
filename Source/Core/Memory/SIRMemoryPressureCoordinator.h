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

#ifndef __SIRENGINE_SIRMEMORYPRESSURECOORDINATOR_H__
#define __SIRENGINE_SIRMEMORYPRESSURECOORDINATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRAtomic.h>
#include <Core/Platform/SIRSync.h>

class SIRMemoryManager;
class SIRMemorySegment;

typedef enum sirReclaimOwnerType_e : uint8_t
{
	SIR_RECLAIM_SLAB,
	SIR_RECLAIM_SIZE_CLASSES,
	SIR_RECLAIM_THREAD_SEAT,
	SIR_RECLAIM_BUDDY,
	SIR_RECLAIM_CACHE,
	SIR_RECLAIM_HANDLE,
	SIR_RECLAIM_FREE_LIST,
	SIR_RECLAIM_TLSF,
	SIR_RECLAIM_POOL,
	SIR_RECLAIM_FRAME,
	SIR_RECLAIM_RING,
	SIR_RECLAIM_CACHE_EVICT,
	SIR_RECLAIM_SIZE_CLASSES_PRESSURE,
	SIR_RECLAIM_COMPACT_BUCKET
} sirReclaimOwnerType_t;

typedef struct sirReclaimOwnerConfig_s
{
	void *pAllocator;
	SIRMemorySegment *const *ppSegments;
	uint32_t nSegmentCount;
	uint32_t nOwnerType;
	uint32_t nPriority;
	uint32_t nSeat;
	uint32_t nWarmSlabs;
	uint32_t nMaximumVictims;
} sirReclaimOwnerConfig_t;

typedef struct sirPressureStats_s
{
	uint64_t nRequestedBytes;
	uint64_t nReclaimedBytes;
	uint64_t nUnfulfilledBytes;
	uint64_t nCanceledBytes;
	uint32_t nPendingOwners;
	uint32_t nOwnerCount;
} sirPressureStats_t;

// Cold coordinator: requests are atomic and never invoke allocator functions.
// ONLY the allocator-owning thread calls serviceOwner at a quiescent safepoint.
// All storage comes from the manager's permanent metadata reservation.
typedef struct sirOwner_s
{
	void *pAllocator;
	SIRMemorySegment **ppSegments;
	uint32_t nSegmentCount;
	uint32_t nType;
	uint32_t nSeat;
	uint32_t nWarmSlabs;
	uint32_t nMaximumVictims;
	uint32_t nPriority;
	SIRAtomic<uint64_t> nPendingBytes;
	SIRAtomic<uint32_t> nServicing;
	sirOwner_s( void );
} sirOwner_t;

class SIRMemoryPressureCoordinator
{
public:
	SIRMemoryPressureCoordinator( void );

	bool init( SIRMemoryManager *pManager, uint32_t nOwnerCapacity );
	bool registerOwner( const sirReclaimOwnerConfig_t &config, uint32_t *pIndex );
	bool requestReclaim( uint64_t nTargetBytes );
	uint64_t serviceOwner( uint32_t nIndex );
	uint32_t findNextPendingOwner( uint32_t nMaxPriority = UINT32_MAX ) const;
	sirPressureStats_t getStats( void ) const;
	bool cancelPending( void );
	bool shutdown( void );

private:
	bool trimOwner( sirOwner_t &owner, uint64_t nTargetBytes );
	uint64_t countOwnerCommitted( const sirOwner_t &owner ) const;

	SIRMemoryManager *m_pManager;
	sirOwner_t *m_pOwners;
	uint32_t m_nOwnerCapacity;
	SIRAtomic<uint32_t> m_nOwnerCount;
	SIRAtomic<uint64_t> m_nRequestedBytes;
	SIRAtomic<uint64_t> m_nReclaimedBytes;
	SIRAtomic<uint64_t> m_nCanceledBytes;
	mutable SIRMutex m_RequestMutex;
};

#endif // __SIRENGINE_SIRMEMORYPRESSURECOORDINATOR_H__
