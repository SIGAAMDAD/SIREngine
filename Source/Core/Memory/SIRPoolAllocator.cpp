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

#include <Core/Memory/Allocators/SIRPoolAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <cstring>
#include <limits>

SIRPoolAllocator::SIRPoolAllocator( void )
    : m_pSegment( nullptr ), m_pBase( nullptr ), m_nStrideMagic( 0 ),
      m_nCommittedPrefixBytes( 0 ), m_nCommitIncrementBytes( 0 ),
      m_nObjectBytes( 0 ), m_nSlotStride( 0 ), m_nCapacitySlots( 0 ),
      m_nFreshIndex( 0 ), m_nFreeHead( SIR_POOL_INVALID_SLOT ), m_nLiveCount( 0 )
      , m_pLiveBits( nullptr ), m_nLiveBitmapWords( 0 )
{
}

bool SIRPoolAllocator::init( SIRMemorySegment *pSegment, uint32_t nObjectBytes,
                             uint32_t nAlignment, uint64_t nCommitIncrementBytes )
{
    if ( m_pSegment != nullptr || pSegment == nullptr || pSegment->getBase() == nullptr ||
         nObjectBytes == 0 || !SIR_IS_POWER_OF_TWO( nAlignment ) )
    {
        return false;
    }

    const uint64_t nPageSize = pSegment->getPageSize();
    const uint64_t nCapacityBytes = pSegment->getReservedBytes();
    if ( nPageSize == 0 || nCapacityBytes == 0 )
    {
        return false;
    }

    uint64_t nStride = 0;
    const uint64_t nMinimumSlot = nObjectBytes > sizeof( uint32_t ) ?
                                   nObjectBytes : sizeof( uint32_t );
    if ( !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
         ( uint64_t )( nMinimumSlot ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment ) - 1 ) )
    {
        return false;
    }
    nStride = SIR_ALIGN_UP( nMinimumSlot, nAlignment );
    if ( nStride > std::numeric_limits<uint32_t>::max() )
    {
        return false;
    }

    const uintptr_t nBaseAddress = ( ( uintptr_t ) ( pSegment->getBase() ) );
    uint64_t nAlignedAddress = 0;
    if ( !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
         ( uint64_t )( nBaseAddress ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment ) - 1 ) )
    {
        return false;
    }
    nAlignedAddress = SIR_ALIGN_UP( nBaseAddress, nAlignment );
    if ( nAlignedAddress < nBaseAddress )
    {
        return false;
    }

    const uint64_t nFirstOffset = nAlignedAddress - nBaseAddress;
    if ( nFirstOffset >= nCapacityBytes )
    {
        return false;
    }

    uint64_t nCount = ( nCapacityBytes - nFirstOffset ) / nStride;
    if ( nCount > std::numeric_limits<uint32_t>::max() )
    {
        nCount = std::numeric_limits<uint32_t>::max();
    }
    if ( nCount == 0 )
    {
        return false;
    }

    uint64_t nIncrement = 0;
    if ( !SIR_IS_POWER_OF_TWO( nPageSize ) ||
         ( uint64_t )( nPageSize ) > UINTPTR_MAX ||
         ( uint64_t )( nCommitIncrementBytes != 0 ? nCommitIncrementBytes : nPageSize ) > UINTPTR_MAX - ( ( uint64_t )( nPageSize ) - 1 ) )
    {
        return false;
    }
    nIncrement = SIR_ALIGN_UP( nCommitIncrementBytes != 0 ? nCommitIncrementBytes : nPageSize, nPageSize );

    // One bit per slot rejects invalid double frees in every build.
    // Bitmap metadata lives in the manager's already-committed region.
    const uint64_t nWords = SIR_ROUND_UP_MULTIPLE( nCount, 64ull ) / 64;
    SIRMemoryManager *pManager = pSegment->getManager();
    if ( pManager == nullptr ||
         nWords > std::numeric_limits<uint64_t>::max() / sizeof( uint64_t ) )
    {
        return false;
    }

    uint64_t *pLiveBits = ( ( uint64_t * ) ( pManager->allocateMetadata( nWords * sizeof( uint64_t ), alignof( uint64_t ) ) ) );
    if ( pLiveBits == nullptr )
    {
        return false;
    }
    m_pLiveBits = pLiveBits;
    m_nLiveBitmapWords = nWords;

    m_pSegment = pSegment;
    m_pBase = ( ( uint8_t * ) ( ( ( uintptr_t ) ( nAlignedAddress ) )  ) );
    if ( SIR_IS_POWER_OF_TWO( nStride ) )
    {
        uint32_t nShift = 0;
        for ( uint64_t nPower = nStride; nPower > 1; nPower >>= 1 )
        {
            ++nShift;
        }
        m_nStrideMagic = ( 1ull << 63 ) | nShift;
    }
    else
    {
        // Reciprocal floor(2^32 / stride): one multiply, one correction
        // for 32-bit offsets instead of variable integer division.
        m_nStrideMagic = ( 1ull << 32 ) / nStride;
    }
    m_nCommittedPrefixBytes = 0;
    m_nCommitIncrementBytes = nIncrement;
    m_nObjectBytes = nObjectBytes;
    m_nSlotStride = ( ( uint32_t ) ( nStride ) );
    m_nCapacitySlots = ( ( uint32_t ) ( nCount ) );
    m_nFreshIndex = 0;
    m_nFreeHead = SIR_POOL_INVALID_SLOT;
    m_nLiveCount = 0;
    return true;
}

uint8_t *SIRPoolAllocator::slotAddress( uint32_t nIndex ) const
{
    return m_pBase + ( ( uint64_t ) ( nIndex ) ) * m_nSlotStride;
}

bool SIRPoolAllocator::commitToSlotCount( uint32_t nSlots )
{
    if ( m_pSegment == nullptr || nSlots > m_nCapacitySlots )
    {
        return false;
    }
    if ( nSlots == 0 )
    {
        return true;
    }

    const uint64_t nFirstOffset = ( ( uint64_t ) ( m_pBase - ( ( uint8_t * ) ( m_pSegment->getBase() ) )  ) );
    const uint64_t nRequestedEnd = nFirstOffset +
                                   ( ( uint64_t ) ( nSlots ) ) * m_nSlotStride;
    if ( nRequestedEnd <= m_nCommittedPrefixBytes )
    {
        return true;
    }

    uint64_t nTarget = 0;
    {
        const uint64_t nRoundValue = nRequestedEnd;
        const uint64_t nRoundMultiple = m_nCommitIncrementBytes;
        if ( nRoundMultiple == 0 )
        {
            return false;
        }
        const uint64_t nRemainder = nRoundValue % nRoundMultiple;
        const uint64_t nPadding = nRemainder == 0 ? 0 : nRoundMultiple - nRemainder;
        if ( nRoundValue > UINT64_MAX - nPadding )
        {
            return false;
        }
        nTarget = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 )
            ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
            : nRoundValue + nPadding;
    }

    const uint64_t nCapacity = m_pSegment->getReservedBytes();
    if ( nTarget > nCapacity )
    {
        if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) ||
             ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
             ( uint64_t )( nRequestedEnd ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) )
        {
            return false;
        }
        nTarget = SIR_ALIGN_UP( nRequestedEnd, m_pSegment->getPageSize() );
        if ( nTarget > nCapacity )
        {
            return false;
        }
    }

    if ( !m_pSegment->commit( m_nCommittedPrefixBytes,
                              nTarget - m_nCommittedPrefixBytes ) )
    {
        return false;
    }

    m_nCommittedPrefixBytes = nTarget;
    return true;
}

void *SIRPoolAllocator::alloc( void )
{
    if ( m_pSegment == nullptr )
    {
        return nullptr;
    }

    uint32_t nIndex = m_nFreeHead;
    if ( nIndex != SIR_POOL_INVALID_SLOT )
    {
        uint32_t nNext = SIR_POOL_INVALID_SLOT;
        std::memcpy( &nNext, slotAddress( nIndex ), sizeof( nNext ) );
        m_nFreeHead = nNext;
    }
    else
    {
        if ( m_nFreshIndex == m_nCapacitySlots ||
             !commitToSlotCount( m_nFreshIndex + 1 ) )
        {
            return nullptr;
        }
        nIndex = m_nFreshIndex++;
    }

    ++m_nLiveCount;
    const uint64_t nMask = 1ull << ( nIndex % 64 );
    m_pLiveBits[nIndex / 64] |= nMask;
    return slotAddress( nIndex );
}

bool SIRPoolAllocator::free( void *pMemory )
{
    if ( m_pSegment == nullptr || pMemory == nullptr || m_nLiveCount == 0 )
    {
        return false;
    }

    const uintptr_t nAddress = ( ( uintptr_t ) ( pMemory ) );
    const uintptr_t nFirstAddress = ( ( uintptr_t ) ( m_pBase ) );
    if ( nAddress < nFirstAddress )
    {
        return false;
    }

    const uint64_t nOffset = nAddress - nFirstAddress;
    uint64_t nIndex = 0;
    if ( ( m_nStrideMagic & ( 1ull << 63 ) ) != 0 )
    {
        const uint32_t nShift = ( ( uint32_t ) ( m_nStrideMagic & 63 ) );
        if ( ( nOffset & ( m_nSlotStride - 1 ) ) != 0 )
        {
            return false;
        }
        nIndex = nOffset >> nShift;
    }
    else if ( nOffset <= UINT32_MAX )
    {
        // floor(x * floor(2^32/d) / 2^32) is at most one below x/d
        // for a 32-bit x. Correct with the remainder; no hardware DIV.
        nIndex = ( nOffset * m_nStrideMagic ) >> 32;
        uint64_t nRemainder = nOffset - nIndex * m_nSlotStride;
        if ( nRemainder >= m_nSlotStride )
        {
            ++nIndex;
            nRemainder -= m_nSlotStride;
        }
        if ( nRemainder != 0 )
        {
            return false;
        }
    }
    else
    {
        // Huge segments may exceed the 32-bit reciprocal domain.
        if ( nOffset % m_nSlotStride != 0 )
        {
            return false;
        }
        nIndex = nOffset / m_nSlotStride;
    }
    if ( nIndex >= m_nFreshIndex )
    {
        return false;
    }
    const uint32_t nSlot = ( ( uint32_t ) ( nIndex ) );
    const uint64_t nMask = 1ull << ( nSlot % 64 );
    uint64_t& nWord = m_pLiveBits[nSlot / 64];
    if ( ( nWord & nMask ) == 0 )
    {
        return false;
    }
    nWord &= ~nMask;

    std::memcpy( slotAddress( nSlot ), &m_nFreeHead, sizeof( m_nFreeHead ) );
    m_nFreeHead = nSlot;
    --m_nLiveCount;
    return true;
}

bool SIRPoolAllocator::precommitSlots( uint32_t nSlots )
{
    return commitToSlotCount( nSlots );
}

bool SIRPoolAllocator::reset( bool bDecommit )
{
    if ( m_pSegment == nullptr || m_nLiveCount != 0 )
    {
        return false;
    }

    if ( bDecommit && m_nCommittedPrefixBytes != 0 )
    {
        if ( !m_pSegment->decommit( 0, m_nCommittedPrefixBytes ) )
        {
            return false;
        }
        m_nCommittedPrefixBytes = 0;
    }

    m_nFreshIndex = 0;
    m_nFreeHead = SIR_POOL_INVALID_SLOT;
    std::memset( m_pLiveBits, 0, ( ( size_t ) ( m_nLiveBitmapWords * sizeof( uint64_t ) ) ) );
    return true;
}

bool SIRPoolAllocator::shutdown( void )
{
    if ( !reset( true ) )
    {
        return false;
    }

    m_pSegment = nullptr;
    m_pBase = nullptr;
    m_nStrideMagic = 0;
    m_nCommittedPrefixBytes = 0;
    m_nCommitIncrementBytes = 0;
    m_nObjectBytes = 0;
    m_nSlotStride = 0;
    m_nCapacitySlots = 0;
    m_nFreshIndex = 0;
    m_nFreeHead = SIR_POOL_INVALID_SLOT;
    m_nLiveCount = 0;
    m_pLiveBits = nullptr;
    m_nLiveBitmapWords = 0;
    return true;
}

uint32_t SIRPoolAllocator::getObjectBytes( void ) const
{
    return m_nObjectBytes;
}

uint32_t SIRPoolAllocator::getSlotStride( void ) const
{
    return m_nSlotStride;
}

uint32_t SIRPoolAllocator::getCapacitySlots( void ) const
{
    return m_nCapacitySlots;
}

uint32_t SIRPoolAllocator::getLiveSlots( void ) const
{
    return m_nLiveCount;
}

uint32_t SIRPoolAllocator::getFreshSlots( void ) const
{
    return m_nFreshIndex;
}

uint64_t SIRPoolAllocator::getCommittedPrefixBytes( void ) const
{
    return m_nCommittedPrefixBytes;
}
