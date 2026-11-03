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

#ifndef __SIRENGINE_SIRFRAMEALLOCATOR_H__
#define __SIRENGINE_SIRFRAMEALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRLinearAllocator.h>
#include <cstdint>

class SIRMemorySegment;

// Fixed-depth rotating linear arenas. Each slot exclusively owns a segment.
// The caller provides monotonically increasing epoch/completion numbers.
// No frame is reused until its previous epoch has completed.
// All mutable operations require a single owner thread.
typedef struct sirFrameSlot_s {
    SIRLinearAllocator Linear;
    uint64_t nEpoch;
    bool bInFlight;

    sirFrameSlot_s( void );
} sirFrameSlot_t;

class SIRFrameAllocator
{
public:
    SIRFrameAllocator( void );
    SIRFrameAllocator( const SIRFrameAllocator& ) = delete;
    SIRFrameAllocator& operator=( const SIRFrameAllocator& ) = delete;
    SIRFrameAllocator( SIRFrameAllocator&& ) = delete;
    SIRFrameAllocator& operator=( SIRFrameAllocator&& ) = delete;

    // The slot descriptors live in SIRMemoryManager's precommitted metadata.
    static uint64_t getRequiredMetadataBytes( uint32_t nSlotCount );

    bool init( SIRMemorySegment *const *ppSegments, uint32_t nSlotCount,
               uint64_t nCommitIncrementBytes = 0 );
    bool beginEpoch( uint64_t nEpoch, uint64_t nCompletedEpoch );
    void *alloc( uint64_t nBytes, uint64_t nAlignment = 16 );
    bool precommitSlot( uint32_t nSlot, uint64_t nBytes );
    bool submitEpoch( uint64_t nEpoch );
    bool releaseCompleted( uint64_t nCompletedEpoch );
    bool trimIdle( void );
    bool trimIdleRetaining( uint64_t nWarmBytes );
    bool shutdown( void );

    uint32_t getSlotCount( void ) const;
    uint64_t getCurrentEpoch( void ) const;
    uint64_t getLastCompletedEpoch( void ) const;
    uint64_t getActiveUsedBytes( void ) const;

private:


    static constexpr uint32_t SIR_FRAME_INVALID_SLOT = UINT32_MAX;

    sirFrameSlot_t *m_pSlots;
    uint64_t m_nLastEpoch;
    uint64_t m_nCompletedEpoch;
    uint32_t m_nSlotCount;
    uint32_t m_nActiveSlot;
};

#endif // __SIRENGINE_SIRFRAMEALLOCATOR_H__
