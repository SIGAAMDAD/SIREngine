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

#ifndef __SIRENGINE_SIRMEDIUMPAGEALLOCATOR_H__
#define __SIRENGINE_SIRMEDIUMPAGEALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>
class SIRMemorySegment;

// Optional P11 medium-extent experiment. One exclusive segment, one owning
// thread; external metadata stays valid when fully empty pages are decommitted.
typedef struct sirMediumPageExtent_s {
    uint32_t nFreeHead;
    uint32_t nLargest;
    uint32_t nLive;
    bool bCommitted;
} sirMediumPageExtent_t;

typedef struct sirBlock_s {
    uint32_t nPrevBytes;
    uint32_t nSizeFlags;
    uint32_t nNextFree;
    uint32_t nPrevFree;
} sirBlock_t;

class SIRMediumPageAllocator
{
public:
    SIRMediumPageAllocator( void );
    bool init( SIRMemorySegment *pSegment, uint32_t nExtentBytes );
    void *alloc( uint32_t nBytes, uint32_t nAlignment = 16 );
    bool free( void *pMemory );
    uint32_t getUsableSize( const void *pMemory ) const;
    uint64_t trimEmptyPages( uint32_t nMaximumWarmPages );
    bool shutdown( void );

private:


    static constexpr uint32_t SIR_NONE = UINT32_MAX;
    static constexpr uint32_t SIR_FREE = 1;
    static uint32_t blockBytes( const sirBlock_t *pBlock );
    static bool isFree( const sirBlock_t *pBlock );
    sirBlock_t *block( uint32_t nExtent, uint32_t nOffset ) const;
    void insertFree( uint32_t nExtent, uint32_t nOffset );
    void removeFree( uint32_t nExtent, uint32_t nOffset );
    void recomputeLargest( uint32_t nExtent );
    bool ensureExtent( uint32_t nExtent );
    bool resolve( const void *pMemory, uint32_t *pExtent,
                  uint32_t *pOffset, uint32_t *pUsable ) const;
    SIRMemorySegment *m_pSegment;
    uint8_t *m_pBase;
    sirMediumPageExtent_t *m_pExtents;
    uint64_t *m_pLiveStarts;
    uint32_t m_nExtentBytes;
    uint32_t m_nExtentCount;
    uint32_t m_nLive;
};

#endif // __SIRENGINE_SIRMEDIUMPAGEALLOCATOR_H__
