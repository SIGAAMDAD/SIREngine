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

#include <Core/Platform/SIRAtomic.h>
#include <Core/Memory/Allocators/SIRJobScratchAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <new>

sirJobSlot_s::sirJobSlot_s( void )
    : nState( 0 ), nPeakUsage( 0 ), nGeneration( 1 )
{
}
SIRJobScratchAllocator::SIRJobScratchAllocator( void )
    : m_pSlots( nullptr ), m_nSlotCount( 0 ), m_nPeakUsage( 0 )
{
}

bool SIRJobScratchAllocator::init( SIRMemorySegment *const *ppSegments,
                                   uint32_t nSlots )
{
    if ( m_pSlots != nullptr || ppSegments == nullptr || nSlots == 0 )
    {
        return false;
    }
    SIRMemoryManager *pManager = ppSegments[0] != nullptr ?
                                 ppSegments[0]->getManager() : nullptr;
    if ( pManager == nullptr )
    {
        return false;
    }
    for ( uint32_t i = 0; i < nSlots; ++i )
    {
        if ( ppSegments[i] == nullptr || ppSegments[i]->getManager() != pManager )
        {
            return false;
        }
        for ( uint32_t j = 0; j < i; ++j )
        {
            if ( ppSegments[i] == ppSegments[j] )
            {
                return false;
            }
        }
    }
    sirJobSlot_t *pSlots = ( ( sirJobSlot_t * ) ( pManager->allocateMetadata(
        ( ( uint64_t ) ( nSlots ) ) * sizeof( sirJobSlot_t ), alignof( sirJobSlot_t ) ) ) );
    if ( pSlots == nullptr )
    {
        return false;
    }
    for ( uint32_t i = 0; i < nSlots; ++i )
    {
        new ( pSlots + i ) sirJobSlot_t();
        if ( !pSlots[i].Linear.init( ppSegments[i] ) )
        {
            return false;
        }
    }
    m_pSlots = pSlots;
    m_nSlotCount = nSlots;
    return true;
}

sirJobScratchToken_t SIRJobScratchAllocator::beginJob( uint32_t nSlot )
{
    if ( m_pSlots == nullptr || nSlot >= m_nSlotCount )
    {
        return { UINT32_MAX, 0 };
    }
    sirJobSlot_t& slot = m_pSlots[nSlot];
    uint32_t nExpected = 0;
    if ( !slot.nState.compareExchangeStrong( nExpected, 1,
               SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) )
    {
        return { UINT32_MAX, 0 };
    }
    slot.nPeakUsage.store( 0, SIR_MEMORY_ORDER_RELAXED );
    return { nSlot, slot.nGeneration };
}

bool SIRJobScratchAllocator::isActive( sirJobScratchToken_t token ) const
{
    return m_pSlots != nullptr && token.nSlot < m_nSlotCount &&
           m_pSlots[token.nSlot].nState.load( SIR_MEMORY_ORDER_ACQUIRE ) == 1 &&
           m_pSlots[token.nSlot].nGeneration == token.nGeneration;
}

void *SIRJobScratchAllocator::alloc( sirJobScratchToken_t token,
                                     uint64_t nBytes, uint64_t nAlignment )
{
    if ( !isActive( token ) )
    {
        return nullptr;
    }
    sirJobSlot_t& slot = m_pSlots[token.nSlot];
    void *pMemory = slot.Linear.alloc( nBytes, nAlignment );
    if ( pMemory != nullptr )
    {
        const uint64_t nUsed = slot.Linear.getUsedBytes();
        if ( nUsed > slot.nPeakUsage.load( SIR_MEMORY_ORDER_RELAXED ) )
        {
            // Only the job owner writes its slot peak.
            slot.nPeakUsage.store( nUsed, SIR_MEMORY_ORDER_RELAXED );
        }
    }
    return pMemory;
}

uint64_t SIRJobScratchAllocator::mark( sirJobScratchToken_t token ) const
{
    return isActive( token ) ? m_pSlots[token.nSlot].Linear.getMark() : UINT64_MAX;
}

bool SIRJobScratchAllocator::rewind( sirJobScratchToken_t token, uint64_t nMark )
{
    return isActive( token ) && m_pSlots[token.nSlot].Linear.rewind( nMark );
}

bool SIRJobScratchAllocator::precommit( sirJobScratchToken_t token,
                                        uint64_t nBytes )
{
    return isActive( token ) && m_pSlots[token.nSlot].Linear.precommit( nBytes );
}

bool SIRJobScratchAllocator::completeJob( sirJobScratchToken_t token,
                                           bool bDecommit )
{
    return finishJob( token, bDecommit ? 0 : UINT64_MAX );
}

bool SIRJobScratchAllocator::completeJobRetaining( sirJobScratchToken_t token,
                                                    uint64_t nWarmBytes )
{
    return finishJob( token, nWarmBytes );
}

bool SIRJobScratchAllocator::finishJob( sirJobScratchToken_t token,
                                        uint64_t nWarmBytes )
{
    if ( !isActive( token ) )
    {
        return false;
    }
    sirJobSlot_t& slot = m_pSlots[token.nSlot];
    if ( !slot.Linear.reset( false ) )
    {
        return false;
    }
    if ( nWarmBytes != UINT64_MAX )
    {
        const uint64_t nKeep = nWarmBytes < slot.Linear.getCapacity() ?
                               nWarmBytes : slot.Linear.getCapacity();
        if ( !slot.Linear.trimTo( nKeep ) )
        {
            return false;
        }
    }
    const uint64_t nCompletedPeak = slot.nPeakUsage.load( SIR_MEMORY_ORDER_RELAXED );
    uint64_t nOld = m_nPeakUsage.load( SIR_MEMORY_ORDER_RELAXED );
    while ( nCompletedPeak > nOld &&
            !m_nPeakUsage.compareExchangeWeak( nOld, nCompletedPeak,
                SIR_MEMORY_ORDER_RELAXED ) )
    {
    }
    ++slot.nGeneration;
    slot.nState.store( 0, SIR_MEMORY_ORDER_RELEASE );
    return true;
}

bool SIRJobScratchAllocator::shutdown( void )
{
    if ( m_pSlots == nullptr )
    {
        return false;
    }
    for ( uint32_t i = 0; i < m_nSlotCount; ++i )
    {
        if ( m_pSlots[i].nState.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 )
        {
            return false;
        }
    }
    for ( uint32_t i = 0; i < m_nSlotCount; ++i )
    {
        if ( !m_pSlots[i].Linear.shutdown() )
        {
            return false;
        }
        m_pSlots[i].~sirJobSlot_t();
    }
    m_pSlots = nullptr;
    m_nSlotCount = 0;
    return true;
}

uint64_t SIRJobScratchAllocator::getPeakUsageBytes( void ) const
{
    uint64_t nPeak = m_nPeakUsage.load( SIR_MEMORY_ORDER_ACQUIRE );
    for ( uint32_t i = 0; i < m_nSlotCount; ++i )
    {
        if ( m_pSlots[i].nState.load( SIR_MEMORY_ORDER_ACQUIRE ) == 1 )
        {
            const uint64_t nActive = m_pSlots[i].nPeakUsage.load( SIR_MEMORY_ORDER_RELAXED );
            if ( nActive > nPeak )
            {
                nPeak = nActive;
            }
        }
    }
    return nPeak;
}
