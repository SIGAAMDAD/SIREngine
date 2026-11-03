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

#ifndef __SIRENGINE_SIRTHREADLOCALALLOCATOR_H__
#define __SIRENGINE_SIRTHREADLOCALALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRAtomic.h>
#include <Core/Memory/Allocators/SIRSlabAllocator.h>
#include <Core/Memory/Allocators/SIRSizeClassAllocator.h>
#include <cstdint>

class SIRMemoryManager;
class SIRMemorySegment;

typedef struct sirThreadSeatToken_s
{
	uint32_t nSeat;
	uint32_t nGeneration;
} sirThreadSeatToken_t;

typedef struct sirThreadAllocatorStats_s
{
	uint64_t nLivePayloadBytes;
	uint64_t nRemotePendingBytes;
	uint64_t nRefills;
	uint64_t nAllocationFailures;
	uint64_t nRemoteAccepted;
	uint64_t nRemoteRejected;
	uint64_t nRemoteDrained;
	uint64_t nAllocatedCount;
	uint32_t nActiveSeats;
} sirThreadAllocatorStats_t;

// Per-seat ownership, not an allocation on the OS thread's heap.
// Eight exclusive segments per seat; caller must preflight the whole layout.
typedef struct sirRemoteFree_s
{
	void *pMemory;
	uint32_t nBytes;
	uint32_t nAlignment;
} sirRemoteFree_t;

typedef struct sirSeat_s
{
	SIRSlabAllocator Classes[SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT];
	SIRMemorySegment *pSegments[SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT];
	SIRAtomic<uint8_t> *pSlotClaims[SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT];
	sirRemoteFree_t *pInbox;
	SIRAtomicFlag InboxLock;
	SIRAtomic<uint32_t> nState;
	SIRAtomic<uint32_t> nGeneration;
	uint64_t nOwnerThread;
	uint64_t nLiveBytes;
	uint64_t nPendingBytes;
	uint64_t nAllocations;
	uint64_t nAllocationFailures;
	uint32_t nInboxCount;
	uint32_t nInboxInFlight;
	uint32_t nInitialized;
	sirSeat_s( void );
} sirSeat_t;

class SIRThreadLocalAllocator
{
public:
	static constexpr uint32_t SIR_CLASSES = SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT;

	SIRThreadLocalAllocator( void );
	bool init( SIRMemoryManager *pManager, SIRMemorySegment *const *ppSegments, uint32_t nSeats, uint32_t nInboxCapacity = 128,
	           uint64_t nSlabBytes = 16384, uint32_t nAlignment = 16 );
	bool attachThread( uint32_t nSeat );
	bool detachThread( void );
	sirThreadSeatToken_t getSeatToken( void ) const;
	void *alloc( uint32_t nBytes, uint32_t nAlignment = 16 );
	bool free( void *pMemory, uint32_t nBytes, uint32_t nAlignment = 16 );
	bool queueRemoteFree( sirThreadSeatToken_t token, void *pMemory, uint32_t nBytes, uint32_t nAlignment = 16 );
	uint32_t drainRemote( uint32_t nMaximum = UINT32_MAX );
	bool trimSeat( uint32_t nSeat, uint32_t nWarmPerClass = 0 );
	bool shutdown( void );
	sirThreadAllocatorStats_t getStats( void ) const;
	uint32_t getSeatCount( void ) const;

private:
	bool ownsThread( const sirSeat_t &seat ) const;
	bool freeLocal( sirSeat_t &seat, void *pMemory, uint32_t nBytes, bool bRemote = false );
	static void lockInbox( sirSeat_t &seat );
	static void unlockInbox( sirSeat_t &seat );

	SIRMemoryManager *m_pManager;
	sirSeat_t *m_pSeats;
	uint32_t m_nSeatCount;
	uint32_t m_nInboxCapacity;
	uint32_t m_nAlignment;
	SIRAtomic<uint64_t> m_nRemoteAccepted;
	SIRAtomic<uint64_t> m_nRemoteRejected;
	SIRAtomic<uint64_t> m_nRemoteDrained;
};

#endif // __SIRENGINE_SIRTHREADLOCALALLOCATOR_H__
