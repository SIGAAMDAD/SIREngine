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

#ifndef __SIRENGINE_SIRCACHEALLOCATOR_H__
#define __SIRENGINE_SIRCACHEALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <cstdint>

class SIRMemorySegment;

typedef struct sirCacheHandle_s {
    uint32_t nIndex;
    uint32_t nGeneration;
} sirCacheHandle_t;
static_assert( sizeof( sirCacheHandle_t ) == 8, "8-byte cache handle" );

typedef struct sirCachePressureResult_s {
    uint64_t nFreedBlockBytes;
    uint64_t nReclaimedPagesBytes;
    uint32_t nEvicted;
} sirCachePressureResult_t;

// Evictable raw-byte cache with a concrete buddy backing allocator.
// CLOCK eviction never releases pinned entries. All operations single-owner.
typedef struct sirCacheDesc_s {
    uint32_t nOffset;
    union
    {
        uint32_t nBytes;
        uint32_t nNextFree;
    };
    uint32_t nGeneration;
    uint32_t nPinCount;
    uint32_t nActiveSlot;
    uint32_t nFlags;
} sirCacheDesc_t;

class SIRCacheAllocator
{
public:
    SIRCacheAllocator( void );
    SIRCacheAllocator( const SIRCacheAllocator& ) = delete;
    SIRCacheAllocator& operator=( const SIRCacheAllocator& ) = delete;

    static uint64_t getRequiredMetadataBytes( uint64_t nSegmentBytes,
                                               uint64_t nMinimumBlockBytes,
                                               uint32_t nMaxEntries );
    bool init( SIRMemorySegment *pSegment, uint32_t nMaxEntries,
               uint64_t nMinimumBlockBytes = 0 );
    void *alloc( uint32_t nBytes, uint32_t nAlignment, sirCacheHandle_t *pHandle );
    void *resolve( sirCacheHandle_t handle );
    void *pin( sirCacheHandle_t handle );
    bool touch( sirCacheHandle_t handle );
    bool unpin( sirCacheHandle_t handle );
    bool release( sirCacheHandle_t handle );
    bool evictOne( void );
    uint64_t evictUntilFree( uint64_t nBytes );
    bool trimUnusedPages( void );
    sirCachePressureResult_t reclaimCold( uint64_t nTargetCommittedBytes,
                                         uint32_t nMaximumVictims );
    bool shutdown( void );
    uint32_t getLiveCount( void ) const;
    uint64_t getEvictedCount( void ) const;

private:
    // Six 32-bit words instead of the original 32-byte pointer descriptor.
    // nBytes and nNextFree have disjoint live/free lifetimes.

    static_assert( sizeof( sirCacheDesc_t ) == 24, "24-byte cache descriptor" );

    static constexpr uint32_t SIR_CACHE_LIVE = 1;
    static constexpr uint32_t SIR_CACHE_REFERENCED = 2;
    static constexpr uint32_t SIR_CACHE_NONE = UINT32_MAX;

    sirCacheDesc_t *validate( sirCacheHandle_t handle );
    bool evictIndex( uint32_t nIndex, bool bEvicted );
    bool acquireIndex( uint32_t *pIndex );
    void returnIndex( uint32_t nIndex );
    uint8_t *entryAddress( const sirCacheDesc_t& desc ) const;

    SIRBuddyAllocator m_Buddy;
    SIRMemorySegment *m_pSegment;
    sirCacheDesc_t *m_pEntries;
    uint32_t *m_pActiveIndices;
    uint8_t *m_pBase;
    uint32_t m_nEntryCount;
    uint32_t m_nPageSize;
    uint32_t m_nFreshIndex;
    uint32_t m_nFreeHead;
    uint32_t m_nClockHand;
    uint32_t m_nLiveCount;
    uint64_t m_nEvicted;
};

#endif // __SIRENGINE_SIRCACHEALLOCATOR_H__
