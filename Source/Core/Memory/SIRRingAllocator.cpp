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

#include <Core/Memory/Allocators/SIRRingAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <limits>

SIRRingAllocator::SIRRingAllocator( void )
    : m_pSegment( nullptr ), m_pEntries( nullptr ), m_pBase( nullptr ),
      m_nCapacity( 0 ), m_nHead( 0 ), m_nTail( 0 ), m_nNextSequence( 1 ),
      m_nRetiredSequence( 0 ), m_nCapacityMask( 0 ), m_nCommittedPrefixBytes( 0 ),
      m_nEntryCapacity( 0 ), m_nEntryHead( 0 ), m_nEntryTail( 0 ), m_nCount( 0 )
{
}

uint64_t SIRRingAllocator::getRequiredMetadataBytes( uint32_t nMaxAllocations )
{
    return ( ( uint64_t ) ( nMaxAllocations ) ) * sizeof( sirRingEntry_t );
}

bool SIRRingAllocator::init( SIRMemorySegment *pSegment, uint32_t nMaxAllocations )
{
    if ( m_pSegment != nullptr || pSegment == nullptr || nMaxAllocations == 0 ||
         pSegment->getReservedBytes() == 0 )
    {
        return false;
    }
    sirRingEntry_t *pEntries = ( ( sirRingEntry_t * ) ( pSegment->getManager()->allocateMetadata( getRequiredMetadataBytes( nMaxAllocations ),
                                                  alignof( sirRingEntry_t ) ) ) );
    if ( pEntries == nullptr )
    {
        return false;
    }
    m_pSegment = pSegment;
    m_pEntries = pEntries;
    m_pBase = ( ( uint8_t * ) ( pSegment->getBase() ) );
    m_nCapacity = pSegment->getReservedBytes();
    m_nCapacityMask = SIR_IS_POWER_OF_TWO( m_nCapacity ) ? m_nCapacity - 1 : 0;
    m_nEntryCapacity = nMaxAllocations;
    return true;
}

uint64_t SIRRingAllocator::wrapOffset( uint64_t nOffset ) const
{
    if ( m_nCapacityMask != 0 )
    {
        return nOffset & m_nCapacityMask;
    }
    return nOffset % m_nCapacity;
}

bool SIRRingAllocator::commitThrough( uint64_t nEnd )
{
    if ( nEnd <= m_nCommittedPrefixBytes )
    {
        return true;
    }
    uint64_t nTarget = 0;
    if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) ||
         ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
         ( uint64_t )( nEnd ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) )
    {
        return false;
    }
    nTarget = SIR_ALIGN_UP( nEnd, m_pSegment->getPageSize() );
    if ( nTarget > m_nCapacity ||
         !m_pSegment->commit( m_nCommittedPrefixBytes, nTarget - m_nCommittedPrefixBytes ) )
    {
        return false;
    }
    m_nCommittedPrefixBytes = nTarget;
    return true;
}

bool SIRRingAllocator::precommit( void )
{
    return m_pSegment != nullptr && commitThrough( m_nCapacity );
}

void *SIRRingAllocator::alloc( uint32_t nBytes, uint32_t nAlignment,
                               uint64_t *pSequence )
{
    if ( m_pSegment == nullptr || nBytes == 0 || pSequence == nullptr ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         nAlignment > m_pSegment->getPageSize() ||
         nBytes > m_nCapacity || m_nCount == m_nEntryCapacity ||
         m_nNextSequence == UINT64_MAX )
    {
        return nullptr;
    }
    if ( m_nCount == 0 )
    {
        m_nHead = 0;
        m_nTail = 0;
    }
    // Segment bases are page aligned; requested alignment is at most a page.
    const uint64_t nOffset = wrapOffset( m_nHead );
    const uint64_t nAlignedOffset = SIR_ALIGN_UP( nOffset, nAlignment );
    const bool bFits = nAlignedOffset <= m_nCapacity &&
                       nBytes <= m_nCapacity - nAlignedOffset;
    const uint64_t nPadding = bFits ? nAlignedOffset - nOffset :
                              m_nCapacity - nOffset;
    if ( m_nHead > UINT64_MAX - nPadding ||
         m_nHead + nPadding > UINT64_MAX - nBytes )
    {
        return nullptr;
    }
    const uint64_t nStart = m_nHead + nPadding;
    const uint64_t nEnd = nStart + nBytes;
    if ( nEnd - m_nTail > m_nCapacity )
    {
        return nullptr;
    }
    const uint64_t nPhysicalEnd = wrapOffset( nStart ) + nBytes;
    if ( !commitThrough( nPhysicalEnd ) )
    {
        return nullptr;
    }
    m_pEntries[m_nEntryTail].nEnd = nEnd;
    if ( ++m_nEntryTail == m_nEntryCapacity )
    {
        m_nEntryTail = 0;
    }
    ++m_nCount;
    m_nHead = nEnd;
    *pSequence = m_nNextSequence++;
    return m_pBase + wrapOffset( nStart );
}

bool SIRRingAllocator::retireThrough( uint64_t nSequence )
{
    if ( m_pSegment == nullptr || nSequence < m_nRetiredSequence ||
         nSequence >= m_nNextSequence )
    {
        return false;
    }
    // Outstanding sequences are contiguous, so one descriptor load retires
    // an entire completed batch instead of traversing every retired entry.
    if ( m_nCount != 0 )
    {
        const uint64_t nOldest = m_nNextSequence - m_nCount;
        if ( nSequence >= nOldest )
        {
            const uint32_t nToRetire = ( ( uint32_t ) ( ( nSequence - nOldest + 1 ) > m_nCount ?
                m_nCount : nSequence - nOldest + 1 ) );
            const uint64_t nLastUnwrapped = ( ( uint64_t ) ( m_nEntryHead ) ) +
                                             nToRetire - 1;
            const uint32_t nLast = ( ( uint32_t ) ( nLastUnwrapped >= m_nEntryCapacity ?
                nLastUnwrapped - m_nEntryCapacity : nLastUnwrapped ) );
            m_nTail = m_pEntries[nLast].nEnd;
            const uint64_t nNext = ( ( uint64_t ) ( m_nEntryHead ) ) + nToRetire;
            m_nEntryHead = ( ( uint32_t ) ( nNext >= m_nEntryCapacity ? nNext - m_nEntryCapacity : nNext ) );
            m_nCount -= nToRetire;
        }
    }
    m_nRetiredSequence = nSequence;
    if ( m_nCount == 0 )
    {
        m_nHead = 0;
        m_nTail = 0;
    }
    return true;
}

bool SIRRingAllocator::trimIfEmpty( void )
{
    if ( m_pSegment == nullptr || m_nCount != 0 )
    {
        return false;
    }
    if ( m_nCommittedPrefixBytes != 0 &&
         !m_pSegment->decommit( 0, m_nCommittedPrefixBytes ) )
    {
        return false;
    }
    m_nCommittedPrefixBytes = 0;
    return true;
}

bool SIRRingAllocator::shutdown( void )
{
    if ( !trimIfEmpty() )
    {
        return false;
    }
    m_pSegment = nullptr;
    m_pEntries = nullptr;
    m_pBase = nullptr;
    m_nCapacity = 0;
    m_nCapacityMask = 0;
    m_nCommittedPrefixBytes = 0;
    m_nHead = 0;
    m_nTail = 0;
    m_nNextSequence = 1;
    m_nRetiredSequence = 0;
    m_nEntryCapacity = 0;
    m_nEntryHead = 0;
    m_nEntryTail = 0;
    m_nCount = 0;
    return true;
}

uint64_t SIRRingAllocator::getFreeBytes( void ) const
{
    return m_pSegment == nullptr ? 0 : m_nCapacity - ( m_nHead - m_nTail );
}
uint64_t SIRRingAllocator::getLastSequence( void ) const { return m_nNextSequence - 1; }
uint32_t SIRRingAllocator::getOutstandingCount( void ) const { return m_nCount; }
