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

#include <Core/Memory/Allocators/SIRSlabAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <cstring>
#include <limits>

SIRSlabAllocator::SIRSlabAllocator( void )
    : m_pSegment( nullptr ), m_pDescs( nullptr ), m_pBase( nullptr ),
      m_nSlabBytes( 0 ), m_nStride( 0 ), m_nSlabShift( 0 ),
      m_nStrideShift( 0 ), m_nSlabCount( 0 ),
      m_nSlotsPerSlab( 0 ), m_nNextDormant( 0 ),
      m_nActiveSlab( SIR_SLAB_NONE ), m_nCommittedSlabs( 0 ),
      m_nColdActivations( 0 ),
      m_nLiveSlots( 0 ), m_nEmptyHead( SIR_SLAB_NONE ),
      m_nPartialHead( SIR_SLAB_NONE ), m_nDormantHead( SIR_SLAB_NONE )
      , m_pLiveBits( nullptr )
{
}

uint64_t SIRSlabAllocator::getRequiredMetadataBytes( uint64_t nSegmentBytes,
                                                      uint64_t nSlabBytes,
                                                      uint32_t nObjectBytes,
                                                      uint32_t nAlignment )
{
    if ( nSlabBytes == 0 || nObjectBytes == 0 ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         nAlignment > nSlabBytes || nSegmentBytes / nSlabBytes > UINT32_MAX )
    {
        return 0;
    }
    uint64_t nStride = 0;
    const uint64_t nMinimum = nObjectBytes < 4 ? 4 : nObjectBytes;
    if ( !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
         ( uint64_t )( nMinimum ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment ) - 1 ) )
    {
        return 0;
    }
    nStride = SIR_ALIGN_UP( nMinimum, nAlignment );
    if ( nStride > nSlabBytes )
    {
        return 0;
    }
    const uint64_t nSlabs = nSegmentBytes / nSlabBytes;
    const uint64_t nSlots = nSlabs * ( nSlabBytes / nStride );
    if ( nSlabs == 0 || nSlots > UINT32_MAX )
    {
        return 0;
    }
    uint64_t nBytes = nSlabs * sizeof( sirSlabDesc_t );
    nBytes += ( SIR_ROUND_UP_MULTIPLE( nSlots, 64ull ) / 64 ) * sizeof( uint64_t ) + 8;
    return nBytes;
}

bool SIRSlabAllocator::init( SIRMemorySegment *pSegment, uint32_t nObjectBytes,
                             uint32_t nAlignment, uint64_t nSlabBytes )
{
    if ( m_pSegment != nullptr || pSegment == nullptr || nObjectBytes == 0 ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         nAlignment > pSegment->getPageSize() ||
         nSlabBytes < pSegment->getPageSize() ||
         nSlabBytes % pSegment->getPageSize() != 0 ||
         nSlabBytes > UINT32_MAX )
    {
        return false;
    }
    uint64_t nStride = 0;
    const uint64_t nMinimum = nObjectBytes < 4 ? 4 : nObjectBytes;
    if ( !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
         ( uint64_t )( nMinimum ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment ) - 1 ) )
    {
        return false;
    }
    nStride = SIR_ALIGN_UP( nMinimum, nAlignment );
    if ( nStride > nSlabBytes ||
         nStride > UINT32_MAX )
    {
        return false;
    }
    const uint64_t nSlabs = pSegment->getReservedBytes() / nSlabBytes;
    const uint64_t nSlotsPerSlab = nSlabBytes / nStride;
    if ( nSlabs == 0 || nSlabs > UINT32_MAX ||
         nSlabs * nSlotsPerSlab > UINT32_MAX || nSlotsPerSlab == 0 )
    {
        return false;
    }
    sirSlabDesc_t *pDescs = ( ( sirSlabDesc_t * ) ( pSegment->getManager()->allocateMetadata( nSlabs * sizeof( sirSlabDesc_t ),
                                                  alignof( sirSlabDesc_t ) ) ) );
    if ( pDescs == nullptr )
    {
        return false;
    }
    for ( uint32_t i = 0; i < nSlabs; ++i )
    {
        pDescs[i].nNext = SIR_SLAB_NONE;
        pDescs[i].nPrev = SIR_SLAB_NONE;
        pDescs[i].nFreeHead = SIR_SLAB_NONE;
        pDescs[i].nState = SIR_SLAB_DORMANT;
    }
    const uint64_t nWords = SIR_ROUND_UP_MULTIPLE( nSlabs * nSlotsPerSlab, 64ull ) / 64;
    uint64_t *pLiveBits = ( ( uint64_t * ) ( pSegment->getManager()->allocateMetadata( nWords * sizeof( uint64_t ),
                                                  alignof( uint64_t ) ) ) );
    if ( pLiveBits == nullptr )
    {
        return false;
    }
    m_pLiveBits = pLiveBits;
    m_pSegment = pSegment;
    m_pDescs = pDescs;
    m_pBase = ( ( uint8_t * ) ( pSegment->getBase() ) );
    m_nSlabBytes = nSlabBytes;
    m_nStride = ( ( uint32_t ) ( nStride ) );
    m_nSlabShift = 0;
    m_nStrideShift = 0;
    if ( SIR_IS_POWER_OF_TWO( nSlabBytes ) )
    {
        uint64_t nSize = nSlabBytes;
        while ( nSize > 1 )
        {
            ++m_nSlabShift;
            nSize >>= 1;
        }
    }
    if ( SIR_IS_POWER_OF_TWO( nStride ) )
    {
        uint64_t nSize = nStride;
        while ( nSize > 1 )
        {
            ++m_nStrideShift;
            nSize >>= 1;
        }
    }
    m_nSlabCount = ( ( uint32_t ) ( nSlabs ) );
    m_nSlotsPerSlab = ( ( uint32_t ) ( nSlotsPerSlab ) );
    return true;
}

void SIRSlabAllocator::unlink( uint32_t nIndex )
{
    sirSlabDesc_t& desc = m_pDescs[nIndex];
    uint32_t *pHead = desc.nState == SIR_SLAB_PARTIAL ? &m_nPartialHead :
                       desc.nState == SIR_SLAB_EMPTY ? &m_nEmptyHead : &m_nDormantHead;
    if ( desc.nPrev == SIR_SLAB_NONE )
    {
        *pHead = desc.nNext;
    }
    else
    {
        m_pDescs[desc.nPrev].nNext = desc.nNext;
    }
    if ( desc.nNext != SIR_SLAB_NONE )
    {
        m_pDescs[desc.nNext].nPrev = desc.nPrev;
    }
    desc.nNext = SIR_SLAB_NONE;
    desc.nPrev = SIR_SLAB_NONE;
}

void SIRSlabAllocator::link( uint32_t nIndex, uint32_t nState )
{
    sirSlabDesc_t& desc = m_pDescs[nIndex];
    uint32_t *pHead = nState == SIR_SLAB_PARTIAL ? &m_nPartialHead :
                       nState == SIR_SLAB_EMPTY ? &m_nEmptyHead : &m_nDormantHead;
    desc.nState = nState;
    desc.nPrev = SIR_SLAB_NONE;
    desc.nNext = *pHead;
    if ( *pHead != SIR_SLAB_NONE )
    {
        m_pDescs[*pHead].nPrev = nIndex;
    }
    *pHead = nIndex;
}

bool SIRSlabAllocator::activateSlab( uint32_t nIndex )
{
    sirSlabDesc_t& desc = m_pDescs[nIndex];
    if ( desc.nState != SIR_SLAB_DORMANT && desc.nState != SIR_SLAB_DECOMMITTED )
    {
        return false;
    }
    if ( !m_pSegment->commit( ( ( uint64_t ) ( nIndex ) ) * m_nSlabBytes,
                              m_nSlabBytes ) )
    {
        return false;
    }
    if ( desc.nState == SIR_SLAB_DECOMMITTED )
    {
        unlink( nIndex );
    }
    desc.nFresh = 0;
    desc.nLive = 0;
    desc.nFreeHead = SIR_SLAB_NONE;
    link( nIndex, SIR_SLAB_EMPTY );
    ++m_nCommittedSlabs;
    ++m_nColdActivations;
    return true;
}

uint8_t *SIRSlabAllocator::slot( uint32_t nSlab, uint32_t nSlot ) const
{
    return m_pBase + ( ( uint64_t ) ( nSlab ) ) * m_nSlabBytes +
           ( ( uint64_t ) ( nSlot ) ) * m_nStride;
}

void *SIRSlabAllocator::alloc( void )
{
    if ( m_pSegment == nullptr )
    {
        return nullptr;
    }
    uint32_t nSlab = m_nActiveSlab;
    if ( nSlab == SIR_SLAB_NONE )
    {
        nSlab = m_nPartialHead != SIR_SLAB_NONE ? m_nPartialHead : m_nEmptyHead;
        if ( nSlab == SIR_SLAB_NONE )
        {
            nSlab = m_nDormantHead != SIR_SLAB_NONE ?
                    m_nDormantHead : m_nNextDormant;
            if ( nSlab >= m_nSlabCount || !activateSlab( nSlab ) )
            {
                return nullptr;
            }
            if ( nSlab == m_nNextDormant )
            {
                ++m_nNextDormant;
            }
        }
        // Keep a single warm slab out of partial/empty lists. Switching from
        // empty to partial on every alloc/free pair would otherwise relink it.
        unlink( nSlab );
        m_pDescs[nSlab].nState = SIR_SLAB_ACTIVE;
        m_nActiveSlab = nSlab;
    }
    sirSlabDesc_t& desc = m_pDescs[nSlab];
    uint32_t nSlot = desc.nFreeHead;
    if ( nSlot != SIR_SLAB_NONE )
    {
        std::memcpy( &desc.nFreeHead, slot( nSlab, nSlot ), sizeof( uint32_t ) );
    }
    else
    {
        if ( desc.nFresh >= m_nSlotsPerSlab )
        {
            return nullptr;
        }
        nSlot = desc.nFresh++;
    }
    ++desc.nLive;
    ++m_nLiveSlots;
    const uint64_t nFlat = ( ( uint64_t ) ( nSlab ) ) * m_nSlotsPerSlab + nSlot;
    m_pLiveBits[nFlat / 64] |= 1ull << ( nFlat % 64 );
    if ( desc.nLive == m_nSlotsPerSlab )
    {
        // Full slabs do not belong to any list until one slot is freed.
        desc.nState = SIR_SLAB_FULL;
        m_nActiveSlab = SIR_SLAB_NONE;
    }
    return slot( nSlab, nSlot );
}

bool SIRSlabAllocator::free( void *pMemory )
{
    if ( m_pSegment == nullptr || pMemory == nullptr || m_nLiveSlots == 0 )
    {
        return false;
    }
    const uintptr_t nBase = ( ( uintptr_t ) ( m_pBase ) );
    const uintptr_t nPtr = ( ( uintptr_t ) ( pMemory ) );
    if ( nPtr < nBase || ( ( uint64_t ) ( nPtr - nBase ) ) >=
                          ( ( uint64_t ) ( m_nSlabCount ) ) * m_nSlabBytes )
    {
        return false;
    }
    const uint64_t nOffset = nPtr - nBase;
    const uint32_t nSlab = m_nSlabShift != 0 ?
                            ( ( uint32_t ) ( nOffset >> m_nSlabShift ) ) :
                            ( ( uint32_t ) ( nOffset / m_nSlabBytes ) );
    const uint64_t nInside = m_nSlabShift != 0 ?
                             nOffset & ( m_nSlabBytes - 1 ) :
                             nOffset % m_nSlabBytes;
    uint32_t nSlot = 0;
    if ( m_nStrideShift != 0 )
    {
        if ( ( nInside & ( m_nStride - 1 ) ) != 0 )
        {
            return false;
        }
        nSlot = ( ( uint32_t ) ( nInside >> m_nStrideShift ) );
    }
    else
    {
        if ( nInside % m_nStride != 0 )
        {
            return false;
        }
        nSlot = ( ( uint32_t ) ( nInside / m_nStride ) );
    }
    if ( nSlot >= m_pDescs[nSlab].nFresh )
    {
        return false;
    }
    sirSlabDesc_t& desc = m_pDescs[nSlab];
    if ( desc.nState != SIR_SLAB_PARTIAL && desc.nState != SIR_SLAB_FULL &&
         desc.nState != SIR_SLAB_ACTIVE )
    {
        return false;
    }
    // Validate liveness before modifying the intrusive free list in every build.
    const uint64_t nFlat = ( ( uint64_t ) ( nSlab ) ) * m_nSlotsPerSlab + nSlot;
    const uint64_t nMask = 1ull << ( nFlat % 64 );
    if ( ( m_pLiveBits[nFlat / 64] & nMask ) == 0 )
    {
        return false;
    }
    m_pLiveBits[nFlat / 64] &= ~nMask;
    const uint32_t nOldState = desc.nState;
    std::memcpy( slot( nSlab, nSlot ), &desc.nFreeHead, sizeof( uint32_t ) );
    desc.nFreeHead = nSlot;
    --desc.nLive;
    --m_nLiveSlots;
    if ( nOldState == SIR_SLAB_ACTIVE )
    {
        // Retain an empty active slab so immediate reuse is just a free-list pop.
        return true;
    }
    if ( nOldState == SIR_SLAB_FULL )
    {
        link( nSlab, desc.nLive == 0 ? SIR_SLAB_EMPTY : SIR_SLAB_PARTIAL );
    }
    else if ( desc.nLive == 0 )
    {
        unlink( nSlab );
        link( nSlab, SIR_SLAB_EMPTY );
    }
    return true;
}

bool SIRSlabAllocator::precommitSlabs( uint32_t nSlabCount )
{
    if ( m_pSegment == nullptr || nSlabCount > m_nSlabCount )
    {
        return false;
    }
    while ( m_nCommittedSlabs < nSlabCount )
    {
        const uint32_t nIndex = m_nDormantHead != SIR_SLAB_NONE ?
                                m_nDormantHead : m_nNextDormant;
        if ( nIndex == m_nSlabCount || !activateSlab( nIndex ) )
        {
            return false;
        }
        if ( nIndex == m_nNextDormant )
        {
            ++m_nNextDormant;
        }
    }
    return true;
}

bool SIRSlabAllocator::trimEmptySlabs( void )
{
    return trimEmptySlabsTo( 0 );
}

bool SIRSlabAllocator::trimEmptySlabsTo( uint32_t nKeepWarm )
{
    if ( m_pSegment == nullptr )
    {
        return false;
    }
    if ( m_nActiveSlab != SIR_SLAB_NONE &&
         m_pDescs[m_nActiveSlab].nLive == 0 )
    {
        const uint32_t nEmpty = m_nActiveSlab;
        m_nActiveSlab = SIR_SLAB_NONE;
        link( nEmpty, SIR_SLAB_EMPTY );
    }
    uint32_t nWarm = 0;
    for ( uint32_t i = m_nEmptyHead; i != SIR_SLAB_NONE;
          i = m_pDescs[i].nNext )
    {
        ++nWarm;
    }
    while ( nWarm > nKeepWarm && m_nEmptyHead != SIR_SLAB_NONE )
    {
        const uint32_t nIndex = m_nEmptyHead;
        if ( !m_pSegment->decommit( ( ( uint64_t ) ( nIndex ) ) * m_nSlabBytes,
                                    m_nSlabBytes ) )
        {
            return false;
        }
        unlink( nIndex );
        m_pDescs[nIndex].nFresh = 0;
        m_pDescs[nIndex].nFreeHead = SIR_SLAB_NONE;
        link( nIndex, SIR_SLAB_DECOMMITTED );
        --m_nCommittedSlabs;
        --nWarm;
    }
    return true;
}

bool SIRSlabAllocator::shutdown( void )
{
    if ( m_pSegment == nullptr || m_nLiveSlots != 0 )
    {
        return false;
    }
    if ( !trimEmptySlabs() )
    {
        return false;
    }
    m_pSegment = nullptr;
    m_pDescs = nullptr;
    m_pBase = nullptr;
    m_nSlabCount = 0;
    m_nActiveSlab = SIR_SLAB_NONE;
    m_nCommittedSlabs = 0;
    m_pLiveBits = nullptr;
    return true;
}

uint32_t SIRSlabAllocator::getLiveSlots( void ) const { return m_nLiveSlots; }
uint32_t SIRSlabAllocator::getSlotsPerSlab( void ) const { return m_nSlotsPerSlab; }
uint32_t SIRSlabAllocator::getSlabCount( void ) const { return m_nSlabCount; }
uint32_t SIRSlabAllocator::getCommittedSlabs( void ) const { return m_nCommittedSlabs; }

bool SIRSlabAllocator::containsSlotAddress( const void *pMemory ) const
{
    if ( m_pBase == nullptr || pMemory == nullptr )
    {
        return false;
    }
    const uintptr_t nBase = ( ( uintptr_t ) ( m_pBase ) );
    const uintptr_t nPointer = ( ( uintptr_t ) ( pMemory ) );
    if ( nPointer < nBase )
    {
        return false;
    }
    const uint64_t nOffset = nPointer - nBase;
    if ( nOffset >= ( ( uint64_t ) ( m_nSlabCount ) ) * m_nSlabBytes )
    {
        return false;
    }
    const uint64_t nInside = nOffset % m_nSlabBytes;
    return nInside % m_nStride == 0 &&
           nInside / m_nStride < m_nSlotsPerSlab;
}


bool SIRSlabAllocator::getSlotIndex( const void *pMemory, uint64_t *pIndex ) const
{
    if ( pIndex == nullptr || !containsSlotAddress( pMemory ) )
    {
        return false;
    }
    const uint64_t nOffset = ( ( uintptr_t ) ( pMemory ) ) -
                              ( ( uintptr_t ) ( m_pBase ) );
    const uint32_t nSlab = ( ( uint32_t ) ( nOffset / m_nSlabBytes ) );
    const uint32_t nSlot = ( ( uint32_t ) ( nOffset % m_nSlabBytes / m_nStride ) );
    if ( nSlot >= m_pDescs[nSlab].nFresh )
    {
        return false;
    }
    *pIndex = ( ( uint64_t ) ( nSlab ) ) * m_nSlotsPerSlab + nSlot;
    return true;
}

uint64_t SIRSlabAllocator::getSlotIndexCapacity( void ) const
{
    return ( ( uint64_t ) ( m_nSlabCount ) ) * m_nSlotsPerSlab;
}

bool SIRSlabAllocator::owns( const void *pMemory ) const
{
    if ( !containsSlotAddress( pMemory ) )
    {
        return false;
    }
    const uint64_t nOffset = ( ( uintptr_t ) ( pMemory ) ) -
                             ( ( uintptr_t ) ( m_pBase ) );
    const uint32_t nSlab = ( ( uint32_t ) ( nOffset / m_nSlabBytes ) );
    return nOffset % m_nSlabBytes / m_nStride < m_pDescs[nSlab].nFresh &&
           m_pDescs[nSlab].nLive != 0;
}

uint64_t SIRSlabAllocator::getColdActivations( void ) const
{
    return m_nColdActivations;
}
