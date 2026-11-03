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

#include <Core/Memory/Allocators/SIRTLSFAllocator.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <cstring>
#if defined( _MSC_VER )
#include <intrin.h>
#endif

SIRTLSFAllocator::SIRTLSFAllocator( void )
    : m_pSegment( nullptr ), m_pBase( nullptr ), m_nCommittedBytes( 0 ), m_nCapacityBytes( 0 ), m_nIncrementBytes( 0 ), m_nPageSize( 0 ),
      m_nLastBlock( SIRHeapBlock::NONE ), m_nAllocatedBlockBytes( 0 ), m_nAllocationCount( 0 ), m_nFirstLevelBitmap( 0 ),
      m_nSecondLevelBitmap{}, m_nFreeHeads{}
{
	for ( uint32_t i = 0; i < 32; ++i ) {
		for ( uint32_t j = 0; j < 16; ++j ) {
			m_nFreeHeads[i][j] = SIRHeapBlock::NONE;
		}
	}
}

bool SIRTLSFAllocator::init( SIRMemorySegment *pSegment, uint32_t nCommitIncrementBytes )
{
	if ( m_pSegment != nullptr || pSegment == nullptr || pSegment->getReservedBytes() >= UINT32_MAX - 15 ||
	     pSegment->getReservedBytes() < SIRHeapBlock::MINIMUM ) {
		return false;
	}
	uint64_t nIncrement = 0;
	{
		const uint64_t nRoundValue = nCommitIncrementBytes != 0 ? nCommitIncrementBytes : pSegment->getPageSize();
		const uint64_t nRoundMultiple = pSegment->getPageSize();
		if ( nRoundMultiple == 0 ) {
			return false;
		}
		const uint64_t nRemainder = nRoundValue % nRoundMultiple;
		const uint64_t nPadding = nRemainder == 0 ? 0 : nRoundMultiple - nRemainder;
		if ( nRoundValue > UINT64_MAX - nPadding ) {
			return false;
		}
		nIncrement = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 ) ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
		                                                                : nRoundValue + nPadding;
	}
	if ( nIncrement > UINT32_MAX ) {
		return false;
	}
	m_pSegment = pSegment;
	m_pBase = ( ( uint8_t * )( pSegment->getBase() ) );
	m_nCapacityBytes = ( ( uint32_t )( pSegment->getReservedBytes() ) );
	m_nIncrementBytes = ( ( uint32_t )( nIncrement ) );
	m_nPageSize = ( ( uint32_t )( pSegment->getPageSize() ) );
	return true;
}

sirHeapBlock_t *SIRTLSFAllocator::block( uint32_t nOffset ) const
{
	return ( ( sirHeapBlock_t * )( m_pBase + nOffset ) );
}

uint32_t SIRTLSFAllocator::firstSet( uint32_t nWord )
{
#if defined( _MSC_VER )
	unsigned long nBit = 0;
	_BitScanForward( &nBit, nWord );
	return ( ( uint32_t )( nBit ) );
#else
	return ( ( uint32_t )( __builtin_ctz( nWord ) ) );
#endif
}

uint32_t SIRTLSFAllocator::highestSet( uint32_t nWord )
{
#if defined( _MSC_VER )
	unsigned long nBit = 0;
	_BitScanReverse( &nBit, nWord );
	return ( ( uint32_t )( nBit ) );
#else
	return 31u - ( ( uint32_t )( __builtin_clz( nWord ) ) );
#endif
}

bool SIRTLSFAllocator::mapSize( uint32_t nSize, bool bRoundUp, uint32_t *pFirstLevel, uint32_t *pSecondLevel )
{
	if ( nSize < 16 || pFirstLevel == nullptr || pSecondLevel == nullptr ) {
		return false;
	}
	// All physical heap blocks have 16-byte granularity. The two-level
	// mapping uses one leading-bit scan and shifts rather than division.
	const uint32_t nUnits = ( nSize >> 4 ) + ( ( nSize & 15u ) != 0 );
	if ( nUnits < 16 ) {
		*pFirstLevel = 0;
		*pSecondLevel = nUnits - 1;
		return true;
	}
	const uint32_t nLog = highestSet( nUnits );
	const uint32_t nShift = nLog - 4;
	const uint32_t nRemainder = nUnits - ( 1u << nLog );
	const uint32_t nMask = ( 1u << nShift ) - 1;
	uint32_t nFirst = nLog - 3;
	uint32_t nSecond = nRemainder >> nShift;
	if ( bRoundUp && ( nRemainder & nMask ) != 0 ) {
		if ( ++nSecond == 16 ) {
			++nFirst;
			nSecond = 0;
		}
	}
	if ( nFirst >= 32 ) {
		return false;
	}
	*pFirstLevel = nFirst;
	*pSecondLevel = nSecond;
	return true;
}

uint32_t SIRTLSFAllocator::findSuitable( uint32_t nBytes ) const
{
	uint32_t nFirst = 0;
	uint32_t nSecond = 0;
	if ( !mapSize( nBytes, true, &nFirst, &nSecond ) ) {
		return SIRHeapBlock::NONE;
	}
	const uint32_t nMask = 0xffffu << nSecond;
	uint32_t nCandidate = m_nSecondLevelBitmap[nFirst] & nMask;
	if ( nCandidate == 0 ) {
		if ( nFirst == 31 ) {
			return SIRHeapBlock::NONE;
		}
		const uint32_t nNextLevels = m_nFirstLevelBitmap & ( 0xffffffffu << ( nFirst + 1 ) );
		if ( nNextLevels == 0 ) {
			return SIRHeapBlock::NONE;
		}
		nFirst = firstSet( nNextLevels );
		nCandidate = m_nSecondLevelBitmap[nFirst];
	}
	return m_nFreeHeads[nFirst][firstSet( nCandidate )];
}

void SIRTLSFAllocator::insertFree( uint32_t nOffset )
{
	sirHeapBlock_t *pBlock = block( nOffset );
	uint32_t nFirst = 0;
	uint32_t nSecond = 0;
	mapSize( SIRHeapBlock::size( pBlock ), false, &nFirst, &nSecond );
	uint32_t &nHead = m_nFreeHeads[nFirst][nSecond];
	pBlock->nSizeAndFlags |= SIRHeapBlock::FREE;
	pBlock->nPreviousFreeOffset = SIRHeapBlock::NONE;
	pBlock->nNextFreeOffset = nHead;
	if ( nHead != SIRHeapBlock::NONE ) {
		block( nHead )->nPreviousFreeOffset = nOffset;
	}
	nHead = nOffset;
	m_nSecondLevelBitmap[nFirst] |= 1u << nSecond;
	m_nFirstLevelBitmap |= 1u << nFirst;
}

void SIRTLSFAllocator::removeFree( uint32_t nOffset )
{
	sirHeapBlock_t *pBlock = block( nOffset );
	uint32_t nFirst = 0;
	uint32_t nSecond = 0;
	mapSize( SIRHeapBlock::size( pBlock ), false, &nFirst, &nSecond );
	uint32_t &nHead = m_nFreeHeads[nFirst][nSecond];
	if ( pBlock->nPreviousFreeOffset == SIRHeapBlock::NONE ) {
		nHead = pBlock->nNextFreeOffset;
	} else {
		block( pBlock->nPreviousFreeOffset )->nNextFreeOffset = pBlock->nNextFreeOffset;
	}
	if ( pBlock->nNextFreeOffset != SIRHeapBlock::NONE ) {
		block( pBlock->nNextFreeOffset )->nPreviousFreeOffset = pBlock->nPreviousFreeOffset;
	}
	if ( nHead == SIRHeapBlock::NONE ) {
		m_nSecondLevelBitmap[nFirst] &= ~( 1u << nSecond );
		if ( m_nSecondLevelBitmap[nFirst] == 0 ) {
			m_nFirstLevelBitmap &= ~( 1u << nFirst );
		}
	}
}

void SIRTLSFAllocator::updateNextPrev( uint32_t nOffset )
{
	const uint32_t nNext = nOffset + SIRHeapBlock::size( block( nOffset ) );
	if ( nNext < m_nCommittedBytes ) {
		block( nNext )->nPreviousPhysicalBytes = SIRHeapBlock::size( block( nOffset ) );
	}
}

bool SIRTLSFAllocator::requiredBytes( uint32_t nBytes, uint32_t nAlignment, uint32_t *pBytes ) const
{
	if ( pBytes == nullptr || nBytes == 0 || !SIR_IS_POWER_OF_TWO( nAlignment ) || nAlignment > m_nPageSize ) {
		return false;
	}
	const uint64_t nWorst = ( ( uint64_t )( SIRHeapBlock::HEADER ) ) + nBytes + ( nAlignment > 16 ? nAlignment - 16 : 0 );
	const uint64_t nRounded = SIR_ALIGN_UP( nWorst, 16ull );
	if ( nRounded > m_nCapacityBytes ) {
		return false;
	}
	*pBytes = ( ( uint32_t )( nRounded < SIRHeapBlock::MINIMUM ? SIRHeapBlock::MINIMUM : nRounded ) );
	return true;
}

uint32_t SIRTLSFAllocator::usedBytes( uint32_t nOffset, uint32_t nBytes, uint32_t nAlignment, uint32_t *pUserOffset ) const
{
	// The base, block offsets and header are all 16-byte aligned. The
	// ordinary path therefore needs no pointer-based alignment calculation.
	if ( nAlignment <= 16 ) {
		*pUserOffset = nOffset + 16;
		const uint64_t nUsed = SIR_ALIGN_UP( ( ( uint64_t )( 16 ) ) + nBytes, 16ull );
		return ( ( uint32_t )( nUsed < SIRHeapBlock::MINIMUM ? SIRHeapBlock::MINIMUM : nUsed ) );
	}
	const uintptr_t nAddress = ( ( uintptr_t )( m_pBase ) ) + nOffset + 16;
	const uint64_t nUser = SIR_ALIGN_UP( nAddress, nAlignment );
	const uint64_t nUsed = SIR_ALIGN_UP( nUser - ( ( uintptr_t )( m_pBase ) ) - nOffset + nBytes, 16ull );
	if ( nUsed > UINT32_MAX ) {
		return 0;
	}
	*pUserOffset = ( ( uint32_t )( nUser - ( ( uintptr_t )( m_pBase ) ) ) );
	return ( ( uint32_t )( nUsed < SIRHeapBlock::MINIMUM ? SIRHeapBlock::MINIMUM : nUsed ) );
}

bool SIRTLSFAllocator::grow( uint32_t nMinimumBytes )
{
	const uint32_t nTailFree = m_nLastBlock != SIRHeapBlock::NONE && SIRHeapBlock::isFree( block( m_nLastBlock ) )
	                               ? SIRHeapBlock::size( block( m_nLastBlock ) )
	                               : 0;
	uint64_t nRequired = nMinimumBytes > nTailFree ? nMinimumBytes - nTailFree : m_pSegment->getPageSize();
	const uint32_t nOld = m_nCommittedBytes;
	if ( nRequired > m_nCapacityBytes - nOld ) {
		return false;
	}
	uint64_t nGrow = 0;
	uint64_t nTarget = 0;
	{
		const uint64_t nRoundValue = nRequired;
		const uint64_t nRoundMultiple = m_nIncrementBytes;
		if ( nRoundMultiple == 0 ) {
			return false;
		}
		const uint64_t nRemainder = nRoundValue % nRoundMultiple;
		const uint64_t nPadding = nRemainder == 0 ? 0 : nRoundMultiple - nRemainder;
		if ( nRoundValue > UINT64_MAX - nPadding ) {
			return false;
		}
		nGrow = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 ) ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
		                                                           : nRoundValue + nPadding;
	}
	if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) || ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
	     ( uint64_t )( ( ( uint64_t )( nOld ) ) + nGrow ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) ) {
		return false;
	}
	nTarget = SIR_ALIGN_UP( ( ( uint64_t )( nOld ) ) + nGrow, m_pSegment->getPageSize() );
	if ( nTarget > m_nCapacityBytes ) {
		nTarget = m_nCapacityBytes;
	}
	uint32_t nAdded = ( ( uint32_t )( nTarget ) ) - nOld;
	if ( !m_pSegment->commit( nOld, nAdded ) ) {
		// commit the minimum page-rounded growth when the preferred batch
		// exceeds the remaining global commitment budget.
		{
			const uint64_t nRoundValue = nRequired;
			const uint64_t nRoundMultiple = m_pSegment->getPageSize();
			if ( nRoundMultiple == 0 ) {
				return false;
			}
			const uint64_t nRemainder = nRoundValue % nRoundMultiple;
			const uint64_t nPadding = nRemainder == 0 ? 0 : nRoundMultiple - nRemainder;
			if ( nRoundValue > UINT64_MAX - nPadding ) {
				return false;
			}
			nGrow = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 ) ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
			                                                           : nRoundValue + nPadding;
		}
		if ( nGrow > m_nCapacityBytes - nOld || !m_pSegment->commit( nOld, nGrow ) ) {
			return false;
		}
		nAdded = ( ( uint32_t )( nGrow ) );
		nTarget = ( ( uint64_t )( nOld ) ) + nGrow;
	}
	m_nCommittedBytes = ( ( uint32_t )( nTarget ) );
	if ( m_nLastBlock != SIRHeapBlock::NONE && SIRHeapBlock::isFree( block( m_nLastBlock ) ) ) {
		removeFree( m_nLastBlock );
		block( m_nLastBlock )->nSizeAndFlags += nAdded;
		insertFree( m_nLastBlock );
	} else {
		sirHeapBlock_t *pNew = block( nOld );
		pNew->nPreviousPhysicalBytes = m_nLastBlock == SIRHeapBlock::NONE ? 0 : SIRHeapBlock::size( block( m_nLastBlock ) );
		pNew->nSizeAndFlags = nAdded;
		m_nLastBlock = nOld;
		insertFree( nOld );
	}
	return true;
}

void *SIRTLSFAllocator::allocateBlock( uint32_t nOffset, uint32_t nUsed, uint32_t nUserOffset )
{
	sirHeapBlock_t *pBlock = block( nOffset );
	const uint32_t nOldSize = SIRHeapBlock::size( pBlock );
	removeFree( nOffset );
	if ( nOldSize - nUsed >= SIRHeapBlock::MINIMUM ) {
		const uint32_t nRemainder = nOffset + nUsed;
		sirHeapBlock_t *pRemainder = block( nRemainder );
		pRemainder->nPreviousPhysicalBytes = nUsed;
		pRemainder->nSizeAndFlags = nOldSize - nUsed;
		insertFree( nRemainder );
		if ( m_nLastBlock == nOffset ) {
			m_nLastBlock = nRemainder;
		}
		pBlock->nSizeAndFlags = nUsed;
		updateNextPrev( nRemainder );
	} else {
		pBlock->nSizeAndFlags = nOldSize;
	}
	pBlock->nPreviousFreeOffset = nUserOffset - nOffset;
	std::memcpy( m_pBase + nUserOffset - 4, &nOffset, 4 );
	m_nAllocatedBlockBytes += SIRHeapBlock::size( pBlock );
	++m_nAllocationCount;
	return m_pBase + nUserOffset;
}

void *SIRTLSFAllocator::alloc( uint32_t nBytes, uint32_t nAlignment )
{
	if ( m_pSegment == nullptr ) {
		return nullptr;
	}
	uint32_t nWorst = 0;
	if ( !requiredBytes( nBytes, nAlignment, &nWorst ) ) {
		return nullptr;
	}
	uint32_t nIndex = findSuitable( nWorst );
	if ( nIndex == SIRHeapBlock::NONE ) {
		if ( !grow( nWorst ) ) {
			return nullptr;
		}
		nIndex = findSuitable( nWorst );
	}
	if ( nIndex == SIRHeapBlock::NONE ) {
		return nullptr;
	}
	uint32_t nUserOffset = 0;
	const uint32_t nUsed = nAlignment <= 16 ? ( nUserOffset = nIndex + 16, nWorst ) : usedBytes( nIndex, nBytes, nAlignment, &nUserOffset );
	if ( nUsed == 0 || nUsed > SIRHeapBlock::size( block( nIndex ) ) ) {
		return nullptr;
	}
	return allocateBlock( nIndex, nUsed, nUserOffset );
}

bool SIRTLSFAllocator::free( void *pMemory )
{
	if ( m_pSegment == nullptr || pMemory == nullptr || m_nAllocationCount == 0 ) {
		return false;
	}
	const uintptr_t nBase = ( ( uintptr_t )( m_pBase ) );
	const uintptr_t nUser = ( ( uintptr_t )( pMemory ) );
	if ( nUser < nBase + 16 || nUser >= nBase + m_nCommittedBytes ) {
		return false;
	}
	uint32_t nOffset = SIRHeapBlock::NONE;
	std::memcpy( &nOffset, m_pBase + ( nUser - nBase ) - 4, 4 );
	if ( nOffset % 16 != 0 || nOffset >= m_nCommittedBytes - 16 ) {
		return false;
	}
	sirHeapBlock_t *pBlock = block( nOffset );
	const uint32_t nSize = SIRHeapBlock::size( pBlock );
	if ( SIRHeapBlock::isFree( pBlock ) || nSize < SIRHeapBlock::MINIMUM || nSize > m_nCommittedBytes - nOffset ||
	     nOffset + pBlock->nPreviousFreeOffset != nUser - nBase ) {
		return false;
	}
	m_nAllocatedBlockBytes -= nSize;
	--m_nAllocationCount;
	const uint32_t nAfter = nOffset + nSize;
	if ( nAfter < m_nCommittedBytes && SIRHeapBlock::isFree( block( nAfter ) ) ) {
		const uint32_t nSizeAfter = SIRHeapBlock::size( block( nAfter ) );
		removeFree( nAfter );
		pBlock->nSizeAndFlags += nSizeAfter;
		if ( m_nLastBlock == nAfter ) {
			m_nLastBlock = nOffset;
		}
	}
	if ( pBlock->nPreviousPhysicalBytes != 0 ) {
		const uint32_t nBefore = nOffset - pBlock->nPreviousPhysicalBytes;
		if ( SIRHeapBlock::isFree( block( nBefore ) ) ) {
			sirHeapBlock_t *pPrevious = block( nBefore );
			removeFree( nBefore );
			pPrevious->nSizeAndFlags = SIRHeapBlock::size( pPrevious ) + SIRHeapBlock::size( pBlock );
			if ( m_nLastBlock == nOffset ) {
				m_nLastBlock = nBefore;
			}
			nOffset = nBefore;
		}
	}
	updateNextPrev( nOffset );
	insertFree( nOffset );
	return true;
}

uint32_t SIRTLSFAllocator::getUsableSize( const void *pMemory ) const
{
	if ( m_pSegment == nullptr || pMemory == nullptr || m_nAllocationCount == 0 ) {
		return 0;
	}
	const uintptr_t nBase = ( ( uintptr_t )( m_pBase ) );
	const uintptr_t nUser = ( ( uintptr_t )( pMemory ) );
	if ( nUser < nBase + SIRHeapBlock::HEADER || nUser >= nBase + m_nCommittedBytes ) {
		return 0;
	}
	uint32_t nOffset = SIRHeapBlock::NONE;
	std::memcpy( &nOffset, m_pBase + ( nUser - nBase ) - 4, sizeof( nOffset ) );
	if ( ( nOffset & 15 ) != 0 || nOffset > m_nCommittedBytes - SIRHeapBlock::HEADER ) {
		return 0;
	}
	const sirHeapBlock_t *pBlock = block( nOffset );
	const uint32_t nSize = SIRHeapBlock::size( pBlock );
	const uint32_t nUserOffset = ( ( uint32_t )( nUser - nBase ) );
	if ( SIRHeapBlock::isFree( pBlock ) || nSize < SIRHeapBlock::MINIMUM || nSize > m_nCommittedBytes - nOffset ||
	     nOffset + pBlock->nPreviousFreeOffset != nUserOffset || nUserOffset >= nOffset + nSize ) {
		return 0;
	}
	return nOffset + nSize - nUserOffset;
}

bool SIRTLSFAllocator::trimUnusedPages( void )
{
	if ( m_pSegment == nullptr ) {
		return false;
	}
	if ( m_nLastBlock == SIRHeapBlock::NONE || !SIRHeapBlock::isFree( block( m_nLastBlock ) ) ) {
		return true;
	}
	const uint32_t nStart = m_nLastBlock;
	uint64_t nKeep = 0;
	const uint64_t nKeepStart = nStart % m_pSegment->getPageSize() == 0 ? nStart : ( ( uint64_t )( nStart ) ) + SIRHeapBlock::MINIMUM;
	if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) || ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
	     ( uint64_t )( nKeepStart ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) ) {
		return false;
	}
	nKeep = SIR_ALIGN_UP( nKeepStart, m_pSegment->getPageSize() );
	if ( nKeep >= m_nCommittedBytes ) {
		return true;
	}
	const uint32_t nPreviousSize = block( nStart )->nPreviousPhysicalBytes;
	// unlink before decommit: free-list links may reside in the pages released.
	removeFree( nStart );
	if ( !m_pSegment->decommit( nKeep, m_nCommittedBytes - nKeep ) ) {
		insertFree( nStart );
		return false;
	}
	if ( nKeep == nStart ) {
		m_nLastBlock = nStart == 0 ? SIRHeapBlock::NONE : nStart - nPreviousSize;
	} else {
		block( nStart )->nSizeAndFlags = ( ( uint32_t )( nKeep - nStart ) );
		insertFree( nStart );
	}
	m_nCommittedBytes = ( ( uint32_t )( nKeep ) );
	return true;
}

bool SIRTLSFAllocator::shutdown( void )
{
	if ( m_pSegment == nullptr || m_nAllocationCount != 0 || !trimUnusedPages() ) {
		return false;
	}
	if ( m_nCommittedBytes != 0 ) {
		return false;
	}
	m_pSegment = nullptr;
	m_pBase = nullptr;
	return true;
}

uint32_t SIRTLSFAllocator::getAllocationCount( void ) const
{
	return m_nAllocationCount;
}
uint32_t SIRTLSFAllocator::getCommittedBytes( void ) const
{
	return m_nCommittedBytes;
}
uint32_t SIRTLSFAllocator::getAllocatedBlockBytes( void ) const
{
	return m_nAllocatedBlockBytes;
}
