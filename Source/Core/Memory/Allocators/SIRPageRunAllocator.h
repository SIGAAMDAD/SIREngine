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

#ifndef __SIRENGINE_SIRPAGERUNALLOCATOR_H__
#define __SIRENGINE_SIRPAGERUNALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>
class SIRMemorySegment;

// Cold large-block alternative to buddy allocation. Metadata is contained in
// the existing manager reservation; page runs are backed by the same segment.
// Not thread-safe: SIRGeneralHeap owns the large-heap mutex.
typedef struct sirFreeRun_s {
    uint32_t nPages;
    uint32_t nNext;
    uint32_t nPrevious;
} sirFreeRun_t;

class SIRPageRunAllocator
{
public:
    SIRPageRunAllocator( void );
    static uint64_t getRequiredMetadataBytes( uint64_t nCapacityBytes,
                                              uint64_t nPageBytes );
    bool init( SIRMemorySegment *pSegment );
    void *alloc( uint64_t nBytes, uint64_t nAlignment = 16 );
    bool free( void *pMemory );
    uint64_t getUsableSize( const void *pMemory ) const;
    bool trimUnusedPages( void );
    uint64_t getAllocatedBytes( void ) const;
    uint32_t getAllocationCount( void ) const;
private:
    static constexpr uint32_t SIR_NONE = UINT32_MAX;

    void unlinkRun( uint32_t nFirst );
    void insertRun( uint32_t nFirst, uint32_t nPages );
    SIRMemorySegment *m_pSegment;
    uint8_t *m_pBase;
    sirFreeRun_t *m_pFreeRuns;
    uint32_t *m_pAllocationPages;
    uint32_t m_nPageCount;
    uint32_t m_nPageBytes;
    uint32_t m_nFreeHead;
    uint32_t m_nAllocationCount;
    uint64_t m_nAllocatedBytes;
};

#endif // __SIRENGINE_SIRPAGERUNALLOCATOR_H__
