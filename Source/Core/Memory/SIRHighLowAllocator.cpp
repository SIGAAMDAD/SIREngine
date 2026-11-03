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

#include <Core/Memory/Allocators/SIRHighLowAllocator.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryUtils.h>

#include <cstdint>
#include <limits>

SIRHighLowAllocator::SIRHighLowAllocator( void )
    : m_pSegment( nullptr ), m_nLowUsed( 0 ), m_nHighUsed( 0 ),
      m_nLowCommittedEnd( 0 ), m_nHighCommittedBegin( 0 )
{
}

bool SIRHighLowAllocator::init( SIRMemorySegment *pSegment )
{
    if ( pSegment == nullptr || m_pSegment != nullptr )
    {
        return false;
    }
    if ( pSegment->getBase() == nullptr || pSegment->getPageSize() == 0 ||
         pSegment->getReservedBytes() == 0 )
    {
        return false;
    }
    m_pSegment = pSegment;
    m_nHighCommittedBegin = pSegment->getReservedBytes();
    return true;
}

void *SIRHighLowAllocator::allocLow( uint64_t nBytes, uint64_t nAlignment )
{
    if ( m_pSegment == nullptr || nBytes == 0 ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) )
    {
        return nullptr;
    }

    const uint64_t nBase = ( ( uintptr_t ) ( m_pSegment->getBase() ) );
    const uint64_t nCapacity = m_pSegment->getReservedBytes();
    uint64_t nStart = 0;
    if ( nBase > std::numeric_limits<uintptr_t>::max() - m_nLowUsed )
    {
        return nullptr;
    }
    if ( !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
         ( uint64_t )( nBase + m_nLowUsed ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment ) - 1 ) )
    {
        return nullptr;
    }
    nStart = SIR_ALIGN_UP( nBase + m_nLowUsed, nAlignment );
    if ( nStart < nBase )
    {
        return nullptr;
    }

    const uint64_t nOffset = nStart - nBase;
    if ( nOffset > nCapacity - m_nHighUsed ||
         nBytes > nCapacity - m_nHighUsed - nOffset )
    {
        return nullptr;
    }

    const uint64_t nEnd = nOffset + nBytes;
    const uint64_t nPageSize = m_pSegment->getPageSize();
    uint64_t nCommitEnd = 0;
    if ( !SIR_IS_POWER_OF_TWO( nPageSize ) ||
         ( uint64_t )( nPageSize ) > UINTPTR_MAX ||
         ( uint64_t )( nEnd ) > UINTPTR_MAX - ( ( uint64_t )( nPageSize ) - 1 ) )
    {
        return nullptr;
    }
    nCommitEnd = SIR_ALIGN_UP( nEnd, nPageSize );

    // Previously committed low-prefix and high-suffix pages are reusable.
    const uint64_t nColdBegin = m_nLowCommittedEnd;
    const uint64_t nColdEnd = nCommitEnd < m_nHighCommittedBegin ?
                             nCommitEnd : m_nHighCommittedBegin;
    if ( nColdEnd > nColdBegin &&
         !m_pSegment->commit( nColdBegin, nColdEnd - nColdBegin ) )
    {
        return nullptr;
    }

    if ( nCommitEnd > m_nLowCommittedEnd )
    {
        m_nLowCommittedEnd = nCommitEnd;
    }
    m_nLowUsed = nEnd;
    return ( ( void * ) ( ( ( uintptr_t ) ( nStart ) )  ) );
}

void *SIRHighLowAllocator::allocHigh( uint64_t nBytes, uint64_t nAlignment )
{
    if ( m_pSegment == nullptr || nBytes == 0 ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) )
    {
        return nullptr;
    }

    const uint64_t nBase = ( ( uintptr_t ) ( m_pSegment->getBase() ) );
    const uint64_t nCapacity = m_pSegment->getReservedBytes();
    const uint64_t nHighEnd = nCapacity - m_nHighUsed;
    if ( nBytes > nHighEnd )
    {
        return nullptr;
    }

    if ( nBase > std::numeric_limits<uintptr_t>::max() - nHighEnd )
    {
        return nullptr;
    }

    const uint64_t nStartAddress = SIR_ALIGN_DOWN(
        nBase + nHighEnd - nBytes, nAlignment );
    if ( nStartAddress < nBase )
    {
        return nullptr;
    }
    const uint64_t nStart = nStartAddress - nBase;
    if ( nStart < m_nLowUsed )
    {
        return nullptr;
    }

    const uint64_t nPageSize = m_pSegment->getPageSize();
    const uint64_t nCommitBegin = SIR_ALIGN_DOWN( nStart, nPageSize );
    const uint64_t nColdBegin = nCommitBegin > m_nLowCommittedEnd ?
                                nCommitBegin : m_nLowCommittedEnd;
    const uint64_t nColdEnd = m_nHighCommittedBegin;
    if ( nColdEnd > nColdBegin &&
         !m_pSegment->commit( nColdBegin, nColdEnd - nColdBegin ) )
    {
        return nullptr;
    }

    if ( nCommitBegin < m_nHighCommittedBegin )
    {
        m_nHighCommittedBegin = nCommitBegin;
    }
    m_nHighUsed = nCapacity - nStart;
    return ( ( void * ) ( ( ( uintptr_t ) ( nStartAddress ) )  ) );
}

uint64_t SIRHighLowAllocator::getLowMark( void ) const
{
    return m_nLowUsed;
}

uint64_t SIRHighLowAllocator::getHighMark( void ) const
{
    return m_nHighUsed;
}

bool SIRHighLowAllocator::rewindLow( uint64_t nMark )
{
    if ( m_pSegment == nullptr || nMark > m_nLowUsed )
    {
        return false;
    }
    m_nLowUsed = nMark;
    return true;
}

bool SIRHighLowAllocator::rewindHigh( uint64_t nMark )
{
    if ( m_pSegment == nullptr || nMark > m_nHighUsed )
    {
        return false;
    }
    m_nHighUsed = nMark;
    return true;
}

bool SIRHighLowAllocator::reset( void )
{
    if ( m_pSegment == nullptr )
    {
        return false;
    }
    m_nLowUsed = 0;
    m_nHighUsed = 0;
    return true;
}

bool SIRHighLowAllocator::trimUnusedPages( void )
{
    if ( m_pSegment == nullptr )
    {
        return false;
    }

    const uint64_t nPageSize = m_pSegment->getPageSize();
    const uint64_t nCapacity = m_pSegment->getReservedBytes();
    uint64_t nLowerEnd = 0;
    if ( !SIR_IS_POWER_OF_TWO( nPageSize ) ||
         ( uint64_t )( nPageSize ) > UINTPTR_MAX ||
         ( uint64_t )( m_nLowUsed ) > UINTPTR_MAX - ( ( uint64_t )( nPageSize ) - 1 ) )
    {
        return false;
    }
    nLowerEnd = SIR_ALIGN_UP( m_nLowUsed, nPageSize );
    const uint64_t nUpperBegin = SIR_ALIGN_DOWN(
        nCapacity - m_nHighUsed, nPageSize );
    if ( nUpperBegin > nLowerEnd &&
         !m_pSegment->decommit( nLowerEnd, nUpperBegin - nLowerEnd ) )
    {
        return false;
    }

    // Everything outside the live low and high ranges was just reclaimed.
    m_nLowCommittedEnd = nLowerEnd;
    m_nHighCommittedBegin = nUpperBegin;
    return true;
}

bool SIRHighLowAllocator::shutdown( void )
{
    if ( m_pSegment == nullptr || m_nLowUsed != 0 || m_nHighUsed != 0 ||
         !trimUnusedPages() )
    {
        return false;
    }

    m_pSegment = nullptr;
    m_nLowCommittedEnd = 0;
    m_nHighCommittedBegin = 0;
    return true;
}

uint64_t SIRHighLowAllocator::getLowUsedBytes( void ) const
{
    return m_nLowUsed;
}

uint64_t SIRHighLowAllocator::getHighUsedBytes( void ) const
{
    return m_nHighUsed;
}

uint64_t SIRHighLowAllocator::getAvailableBytes( void ) const
{
    if ( m_pSegment == nullptr )
    {
        return 0;
    }
    return m_pSegment->getReservedBytes() - m_nLowUsed - m_nHighUsed;
}
