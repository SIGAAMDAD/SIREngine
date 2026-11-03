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

#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <cstring>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

SIRFreeListAllocator::SIRFreeListAllocator( void )
    : m_pSegment( nullptr ), m_pBase( nullptr ), m_nCommittedBytes( 0 ),
      m_nCapacityBytes( 0 ), m_nIncrementBytes( 0 ),
      m_nNonemptyClasses( 0 ), m_nFreeHeads{}, m_nLastBlock( SIRHeapBlock::NONE ),
      m_nAllocatedBlockBytes( 0 ), m_nAllocationCount( 0 )
{
    for ( uint32_t i = 0; i < SIR_FREE_CLASSES; ++i )
    {
        m_nFreeHeads[i] = SIRHeapBlock::NONE;
    }
}

bool SIRFreeListAllocator::init( SIRMemorySegment *pSegment,
                                 uint32_t nCommitIncrementBytes )
{
    if ( m_pSegment != nullptr || pSegment == nullptr ||
         pSegment->getReservedBytes() >= UINT32_MAX - 15 ||
         pSegment->getReservedBytes() < SIRHeapBlock::MINIMUM )
    {
        return false;
    }
    uint64_t nIncrement = 0;
    {
        const uint64_t nRoundValue = nCommitIncrementBytes != 0 ? nCommitIncrementBytes : pSegment->getPageSize();
        const uint64_t nRoundMultiple = pSegment->getPageSize();
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
        nIncrement = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 )
            ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
            : nRoundValue + nPadding;
    }
    if ( nIncrement > UINT32_MAX )
    {
        return false;
    }
    m_pSegment = pSegment;
    m_pBase = ( ( uint8_t * ) ( pSegment->getBase() ) );
    m_nCapacityBytes = ( ( uint32_t ) ( pSegment->getReservedBytes() ) );
    m_nIncrementBytes = ( ( uint32_t ) ( nIncrement ) );
    return true;
}

sirHeapBlock_t *SIRFreeListAllocator::block( uint32_t nOffset ) const
{
    return ( ( sirHeapBlock_t * ) ( m_pBase + nOffset ) );
}

uint32_t SIRFreeListAllocator::classForSize( uint32_t nBytes )
{
    // Sixteen exact 16-byte bins through 256 bytes, then two bins per
    // power-of-two range. 64 bins cover the full 32-bit offset address space.
    if ( nBytes <= 256 )
    {
        return ( ( nBytes + 15 ) >> 4 ) - 1;
    }
    const uint32_t nValue = nBytes - 1;
#if defined(_MSC_VER)
    unsigned long nLeading = 0;
    _BitScanReverse( &nLeading, nValue );
    const uint32_t nLog = ( ( uint32_t ) ( nLeading ) );
#else
    const uint32_t nLog = 31u - ( ( uint32_t ) ( __builtin_clz( nValue ) ) );
#endif
    const uint32_t nRangeBase = 1u << nLog;
    const uint32_t nSubClass = ( nValue - nRangeBase ) >= ( nRangeBase >> 1 ) ? 1u : 0u;
    return 16u + ( nLog - 8u ) * 2u + nSubClass;
}

uint32_t SIRFreeListAllocator::lowestSetBit( uint64_t nMask )
{
#if defined(_MSC_VER) && defined(_M_X64)
    unsigned long nBit = 0;
    _BitScanForward64( &nBit, nMask );
    return ( ( uint32_t ) ( nBit ) );
#else
    return ( ( uint32_t ) ( __builtin_ctzll( nMask ) ) );
#endif
}

void SIRFreeListAllocator::insertFree( uint32_t nOffset )
{
    sirHeapBlock_t *pBlock = block( nOffset );
    const uint32_t nClass = classForSize( SIRHeapBlock::size( pBlock ) );
    pBlock->nSizeAndFlags |= SIRHeapBlock::FREE;
    pBlock->nPreviousFreeOffset = SIRHeapBlock::NONE;
    pBlock->nNextFreeOffset = m_nFreeHeads[nClass];
    if ( m_nFreeHeads[nClass] != SIRHeapBlock::NONE )
    {
        block( m_nFreeHeads[nClass] )->nPreviousFreeOffset = nOffset;
    }
    m_nFreeHeads[nClass] = nOffset;
    m_nNonemptyClasses |= 1ull << nClass;
}

void SIRFreeListAllocator::removeFree( uint32_t nOffset )
{
    removeFreeAtClass( nOffset, classForSize( SIRHeapBlock::size( block( nOffset ) ) ) );
}

void SIRFreeListAllocator::removeFreeAtClass( uint32_t nOffset, uint32_t nClass )
{
    sirHeapBlock_t *pBlock = block( nOffset );
    if ( pBlock->nPreviousFreeOffset == SIRHeapBlock::NONE )
    {
        m_nFreeHeads[nClass] = pBlock->nNextFreeOffset;
    }
    else
    {
        block( pBlock->nPreviousFreeOffset )->nNextFreeOffset = pBlock->nNextFreeOffset;
    }
    if ( pBlock->nNextFreeOffset != SIRHeapBlock::NONE )
    {
        block( pBlock->nNextFreeOffset )->nPreviousFreeOffset = pBlock->nPreviousFreeOffset;
    }
    if ( m_nFreeHeads[nClass] == SIRHeapBlock::NONE )
    {
        m_nNonemptyClasses &= ~( 1ull << nClass );
    }
}

void SIRFreeListAllocator::updateNextPrev( uint32_t nOffset )
{
    const uint32_t nNext = nOffset + SIRHeapBlock::size( block( nOffset ) );
    if ( nNext < m_nCommittedBytes )
    {
        block( nNext )->nPreviousPhysicalBytes = SIRHeapBlock::size( block( nOffset ) );
    }
}

bool SIRFreeListAllocator::requiredBytes( uint32_t nBytes, uint32_t nAlignment,
                                          uint32_t *pBytes ) const
{
    if ( pBytes == nullptr || nBytes == 0 ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         nAlignment > m_pSegment->getPageSize() )
    {
        return false;
    }
    const uint64_t nWorst = ( ( uint64_t ) ( SIRHeapBlock::HEADER ) ) + nBytes +
                            ( nAlignment > 16 ? nAlignment - 16 : 0 );
    uint64_t nRounded = 0;
    if ( !SIR_IS_POWER_OF_TWO( 16 ) ||
         ( uint64_t )( 16 ) > UINTPTR_MAX ||
         ( uint64_t )( nWorst ) > UINTPTR_MAX - ( ( uint64_t )( 16 ) - 1 ) )
    {
        return false;
    }
    nRounded = SIR_ALIGN_UP( nWorst, 16 );
    if ( nRounded > m_nCapacityBytes )
    {
        return false;
    }
    *pBytes = ( ( uint32_t ) ( nRounded < SIRHeapBlock::MINIMUM ?
                                     SIRHeapBlock::MINIMUM : nRounded ) );
    return true;
}

uint32_t SIRFreeListAllocator::usedBytes( uint32_t nOffset, uint32_t nBytes,
                                          uint32_t nAlignment, uint32_t *pUserOffset ) const
{
    uint64_t nUserAddress = 0;
    if ( !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
         ( uint64_t )( ( ( uintptr_t ) ( m_pBase ) ) + nOffset + 16 ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment ) - 1 ) )
    {
        return 0;
    }
    nUserAddress = SIR_ALIGN_UP( ( ( uintptr_t ) ( m_pBase ) ) + nOffset + 16, nAlignment );
    const uint64_t nUserOffset = nUserAddress - ( ( uintptr_t ) ( m_pBase ) );
    const uint64_t nEnd = nUserOffset + nBytes - nOffset;
    uint64_t nUsed = 0;
    if ( !SIR_IS_POWER_OF_TWO( 16 ) ||
         ( uint64_t )( 16 ) > UINTPTR_MAX ||
         ( uint64_t )( nEnd ) > UINTPTR_MAX - ( ( uint64_t )( 16 ) - 1 ) )
    {
        return 0;
    }
    nUsed = SIR_ALIGN_UP( nEnd, 16 );
    if ( nUsed > UINT32_MAX )
    {
        return 0;
    }
    *pUserOffset = ( ( uint32_t ) ( nUserOffset ) );
    return ( ( uint32_t ) ( nUsed < SIRHeapBlock::MINIMUM ?
                                  SIRHeapBlock::MINIMUM : nUsed ) );
}

bool SIRFreeListAllocator::grow( uint32_t nMinimumBytes )
{
    const uint32_t nTailFree = m_nLastBlock != SIRHeapBlock::NONE &&
                               SIRHeapBlock::isFree( block( m_nLastBlock ) ) ?
                               SIRHeapBlock::size( block( m_nLastBlock ) ) : 0;
    uint64_t nRequired = nMinimumBytes > nTailFree ? nMinimumBytes - nTailFree :
                         m_pSegment->getPageSize();
    const uint32_t nOld = m_nCommittedBytes;
    if ( nRequired > m_nCapacityBytes - nOld )
    {
        return false;
    }
    uint64_t nGrow = 0;
    uint64_t nTarget = 0;
    {
        const uint64_t nRoundValue = nRequired;
        const uint64_t nRoundMultiple = m_nIncrementBytes;
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
        nGrow = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 )
            ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
            : nRoundValue + nPadding;
    }
    if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) ||
         ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
         ( uint64_t )( ( ( uint64_t ) ( nOld ) ) + nGrow ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) )
    {
        return false;
    }
    nTarget = SIR_ALIGN_UP( ( ( uint64_t ) ( nOld ) ) + nGrow, m_pSegment->getPageSize() );
    if ( nTarget > m_nCapacityBytes )
    {
        nTarget = m_nCapacityBytes;
    }
    uint32_t nAdded = ( ( uint32_t ) ( nTarget ) ) - nOld;
    if ( !m_pSegment->commit( nOld, nAdded ) )
    {
        // commit the minimum page-rounded growth when the preferred batch
        // exceeds the remaining global commitment budget.
        {
            const uint64_t nRoundValue = nRequired;
            const uint64_t nRoundMultiple = m_pSegment->getPageSize();
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
            nGrow = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 )
                ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
                : nRoundValue + nPadding;
        }
        if ( nGrow > m_nCapacityBytes - nOld ||
             !m_pSegment->commit( nOld, nGrow ) )
        {
            return false;
        }
        nAdded = ( ( uint32_t ) ( nGrow ) );
        nTarget = ( ( uint64_t ) ( nOld ) ) + nGrow;
    }
    m_nCommittedBytes = ( ( uint32_t ) ( nTarget ) );
    if ( m_nLastBlock != SIRHeapBlock::NONE &&
         SIRHeapBlock::isFree( block( m_nLastBlock ) ) )
    {
        removeFree( m_nLastBlock );
        block( m_nLastBlock )->nSizeAndFlags += nAdded;
        insertFree( m_nLastBlock );
    }
    else
    {
        sirHeapBlock_t *pNew = block( nOld );
        pNew->nPreviousPhysicalBytes = m_nLastBlock == SIRHeapBlock::NONE ? 0 :
                                       SIRHeapBlock::size( block( m_nLastBlock ) );
        pNew->nSizeAndFlags = nAdded;
        m_nLastBlock = nOld;
        insertFree( nOld );
    }
    return true;
}

void *SIRFreeListAllocator::allocateBlock( uint32_t nOffset, uint32_t nUsed,
                                            uint32_t nUserOffset, uint32_t nClass )
{
    sirHeapBlock_t *pBlock = block( nOffset );
    const uint32_t nOldSize = SIRHeapBlock::size( pBlock );
    removeFreeAtClass( nOffset, nClass );
    if ( nOldSize - nUsed >= SIRHeapBlock::MINIMUM )
    {
        const uint32_t nRemainder = nOffset + nUsed;
        sirHeapBlock_t *pRemainder = block( nRemainder );
        pRemainder->nPreviousPhysicalBytes = nUsed;
        pRemainder->nSizeAndFlags = nOldSize - nUsed;
        insertFree( nRemainder );
        if ( m_nLastBlock == nOffset )
        {
            m_nLastBlock = nRemainder;
        }
        pBlock->nSizeAndFlags = nUsed;
        updateNextPrev( nRemainder );
    }
    else
    {
        pBlock->nSizeAndFlags = nOldSize;
    }
    pBlock->nPreviousFreeOffset = nUserOffset - nOffset;
    std::memcpy( m_pBase + nUserOffset - 4, &nOffset, 4 );
    m_nAllocatedBlockBytes += SIRHeapBlock::size( pBlock );
    ++m_nAllocationCount;
    return m_pBase + nUserOffset;
}

void *SIRFreeListAllocator::alloc( uint32_t nBytes, uint32_t nAlignment )
{
    if ( m_pSegment == nullptr )
    {
        return nullptr;
    }
    uint32_t nWorst = 0;
    if ( !requiredBytes( nBytes, nAlignment, &nWorst ) )
    {
        return nullptr;
    }
    // The 16 exact bins can use their head without scanning or alignment math.
    // Coarse classes retain block-by-block checks for suitability.
    const uint32_t nMinimum = nAlignment <= 16 ? nWorst :
        ( ( uint32_t ) SIR_ALIGN_UP( ( ( uint64_t ) nBytes ) + 16ull, 16ull ) );
    const uint32_t nFirstClass = classForSize( nMinimum < SIRHeapBlock::MINIMUM ?
                                               SIRHeapBlock::MINIMUM : nMinimum );
    for ( uint32_t nAttempt = 0; nAttempt < 2; ++nAttempt )
    {
        // Common exact-bin requests resolve at the class head in constant time.
        if ( nAlignment <= 16 && nFirstClass < 16 )
        {
            const uint32_t nHead = m_nFreeHeads[nFirstClass];
            if ( nHead != SIRHeapBlock::NONE )
            {
                return allocateBlock( nHead, nMinimum, nHead + 16, nFirstClass );
            }
        }
        uint64_t nClasses = m_nNonemptyClasses & ( UINT64_MAX << nFirstClass );
        if ( nAlignment <= 16 && nFirstClass < 16 )
        {
            nClasses &= ~( 1ull << nFirstClass );
        }
        while ( nClasses != 0 )
        {
            const uint32_t nClass = lowestSetBit( nClasses );
            for ( uint32_t nIndex = m_nFreeHeads[nClass];
                  nIndex != SIRHeapBlock::NONE;
                  nIndex = block( nIndex )->nNextFreeOffset )
            {
                uint32_t nUserOffset = 0;
                const uint32_t nUsed = nAlignment <= 16 ?
                    ( nUserOffset = nIndex + 16, nMinimum ) :
                    usedBytes( nIndex, nBytes, nAlignment, &nUserOffset );
                if ( nUsed != 0 && nUsed <= SIRHeapBlock::size( block( nIndex ) ) )
                {
                    return allocateBlock( nIndex, nUsed, nUserOffset, nClass );
                }
            }
            nClasses &= nClasses - 1;
        }
        if ( nAttempt != 0 || !grow( nWorst ) )
        {
            break;
        }
    }
    return nullptr;
}

bool SIRFreeListAllocator::free( void *pMemory )
{
    if ( m_pSegment == nullptr || pMemory == nullptr || m_nAllocationCount == 0 )
    {
        return false;
    }
    const uintptr_t nBase = ( ( uintptr_t ) ( m_pBase ) );
    const uintptr_t nUser = ( ( uintptr_t ) ( pMemory ) );
    if ( nUser < nBase + 16 || nUser >= nBase + m_nCommittedBytes )
    {
        return false;
    }
    uint32_t nOffset = SIRHeapBlock::NONE;
    std::memcpy( &nOffset, m_pBase + ( nUser - nBase ) - 4, 4 );
    if ( nOffset % 16 != 0 || nOffset >= m_nCommittedBytes - 16 )
    {
        return false;
    }
    sirHeapBlock_t *pBlock = block( nOffset );
    const uint32_t nSize = SIRHeapBlock::size( pBlock );
    if ( SIRHeapBlock::isFree( pBlock ) || nSize < SIRHeapBlock::MINIMUM ||
         nSize > m_nCommittedBytes - nOffset ||
         nOffset + pBlock->nPreviousFreeOffset != nUser - nBase )
    {
        return false;
    }
    m_nAllocatedBlockBytes -= nSize;
    --m_nAllocationCount;
    const uint32_t nAfter = nOffset + nSize;
    if ( nAfter < m_nCommittedBytes && SIRHeapBlock::isFree( block( nAfter ) ) )
    {
        const uint32_t nSizeAfter = SIRHeapBlock::size( block( nAfter ) );
        removeFreeAtClass( nAfter, classForSize( nSizeAfter ) );
        pBlock->nSizeAndFlags += nSizeAfter;
        if ( m_nLastBlock == nAfter )
        {
            m_nLastBlock = nOffset;
        }
    }
    if ( pBlock->nPreviousPhysicalBytes != 0 )
    {
        const uint32_t nBefore = nOffset - pBlock->nPreviousPhysicalBytes;
        if ( SIRHeapBlock::isFree( block( nBefore ) ) )
        {
            sirHeapBlock_t *pPrevious = block( nBefore );
            const uint32_t nPreviousSize = SIRHeapBlock::size( pPrevious );
            removeFreeAtClass( nBefore, classForSize( nPreviousSize ) );
            pPrevious->nSizeAndFlags = nPreviousSize + SIRHeapBlock::size( pBlock );
            if ( m_nLastBlock == nOffset )
            {
                m_nLastBlock = nBefore;
            }
            nOffset = nBefore;
        }
    }
    updateNextPrev( nOffset );
    insertFree( nOffset );
    return true;
}

uint32_t SIRFreeListAllocator::getUsableSize( const void *pMemory ) const
{
    if ( m_pSegment == nullptr || pMemory == nullptr || m_nAllocationCount == 0 )
    {
        return 0;
    }
    const uintptr_t nBase = ( ( uintptr_t ) ( m_pBase ) );
    const uintptr_t nUser = ( ( uintptr_t ) ( pMemory ) );
    if ( nUser < nBase + SIRHeapBlock::HEADER ||
         nUser >= nBase + m_nCommittedBytes )
    {
        return 0;
    }
    uint32_t nOffset = SIRHeapBlock::NONE;
    std::memcpy( &nOffset, m_pBase + ( nUser - nBase ) - 4, sizeof( nOffset ) );
    if ( ( nOffset & 15 ) != 0 || nOffset > m_nCommittedBytes - SIRHeapBlock::HEADER )
    {
        return 0;
    }
    const sirHeapBlock_t *pBlock = block( nOffset );
    const uint32_t nSize = SIRHeapBlock::size( pBlock );
    const uint32_t nUserOffset = ( ( uint32_t ) ( nUser - nBase ) );
    if ( SIRHeapBlock::isFree( pBlock ) || nSize < SIRHeapBlock::MINIMUM ||
         nSize > m_nCommittedBytes - nOffset ||
         nOffset + pBlock->nPreviousFreeOffset != nUserOffset ||
         nUserOffset >= nOffset + nSize )
    {
        return 0;
    }
    return nOffset + nSize - nUserOffset;
}

bool SIRFreeListAllocator::trimUnusedPages( void )
{
    if ( m_pSegment == nullptr )
    {
        return false;
    }
    if ( m_nLastBlock == SIRHeapBlock::NONE ||
         !SIRHeapBlock::isFree( block( m_nLastBlock ) ) )
    {
        return true;
    }
    const uint32_t nStart = m_nLastBlock;
    uint64_t nKeep = 0;
    const uint64_t nKeepStart = nStart % m_pSegment->getPageSize() == 0 ? nStart :
                                ( ( uint64_t ) ( nStart ) ) + SIRHeapBlock::MINIMUM;
    if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) ||
         ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
         ( uint64_t )( nKeepStart ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) )
    {
        return false;
    }
    nKeep = SIR_ALIGN_UP( nKeepStart, m_pSegment->getPageSize() );
    if ( nKeep >= m_nCommittedBytes )
    {
        return true;
    }
    const uint32_t nPreviousSize = block( nStart )->nPreviousPhysicalBytes;
    // unlink before decommit: free-list links may reside in the pages released.
    removeFree( nStart );
    if ( !m_pSegment->decommit( nKeep, m_nCommittedBytes - nKeep ) )
    {
        insertFree( nStart );
        return false;
    }
    if ( nKeep == nStart )
    {
        m_nLastBlock = nStart == 0 ? SIRHeapBlock::NONE : nStart - nPreviousSize;
    }
    else
    {
        block( nStart )->nSizeAndFlags = ( ( uint32_t ) ( nKeep - nStart ) );
        insertFree( nStart );
    }
    m_nCommittedBytes = ( ( uint32_t ) ( nKeep ) );
    return true;
}

bool SIRFreeListAllocator::shutdown( void )
{
    if ( m_pSegment == nullptr || m_nAllocationCount != 0 || !trimUnusedPages() )
    {
        return false;
    }
    if ( m_nCommittedBytes != 0 )
    {
        return false;
    }
    m_pSegment = nullptr;
    m_pBase = nullptr;
    return true;
}

uint32_t SIRFreeListAllocator::getAllocationCount( void ) const { return m_nAllocationCount; }
uint32_t SIRFreeListAllocator::getCommittedBytes( void ) const { return m_nCommittedBytes; }
uint32_t SIRFreeListAllocator::getAllocatedBlockBytes( void ) const { return m_nAllocatedBlockBytes; }
