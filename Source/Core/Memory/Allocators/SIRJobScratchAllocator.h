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

#ifndef __SIRENGINE_SIRJOBSCRATCHALLOCATOR_H__
#define __SIRENGINE_SIRJOBSCRATCHALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRAtomic.h>
#include <Core/Memory/Allocators/SIRLinearAllocator.h>
#include <cstdint>

class SIRMemorySegment;
class SIRMemoryManager;

typedef struct sirJobScratchToken_s {
    uint32_t nSlot;
    uint32_t nGeneration;
} sirJobScratchToken_t;

// Job owns the token, not the OS worker or fiber. A job must serialize its
// alloc/rewind and completion when migrating from one thread to another.
typedef struct sirJobSlot_s {
    SIRLinearAllocator Linear;
    SIRAtomic<uint32_t> nState;
    SIRAtomic<uint64_t> nPeakUsage;
    uint32_t nGeneration;
    sirJobSlot_s( void );
} sirJobSlot_t;

class SIRJobScratchAllocator
{
public:
    SIRJobScratchAllocator( void );
    bool init( SIRMemorySegment *const *ppSegments, uint32_t nSlots );
    sirJobScratchToken_t beginJob( uint32_t nSlot );
    void *alloc( sirJobScratchToken_t token, uint64_t nBytes,
                 uint64_t nAlignment = 16 );
    uint64_t mark( sirJobScratchToken_t token ) const;
    bool rewind( sirJobScratchToken_t token, uint64_t nMark );
    bool precommit( sirJobScratchToken_t token, uint64_t nBytes );
    bool completeJob( sirJobScratchToken_t token, bool bDecommit = false );
    bool completeJobRetaining( sirJobScratchToken_t token, uint64_t nWarmBytes );
    bool shutdown( void );
    uint64_t getPeakUsageBytes( void ) const;

private:

    bool isActive( sirJobScratchToken_t token ) const;
    bool finishJob( sirJobScratchToken_t token, uint64_t nWarmBytes );
    sirJobSlot_t *m_pSlots;
    uint32_t m_nSlotCount;
    SIRAtomic<uint64_t> m_nPeakUsage;
};

#endif // __SIRENGINE_SIRJOBSCRATCHALLOCATOR_H__
