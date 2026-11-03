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

#include <Core/Memory/Allocators/SIRBitmapAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <Core/Memory/SIRMemoryUtils.h>
SIRBitmapAllocator::SIRBitmapAllocator( void )
    : m_pSegment( nullptr ), m_pBase( nullptr ), m_pLevels {},
      m_nLevelWords {}, m_nCommittedBytes( 0 ), m_nCommitIncrementBytes( 0 ),
      m_nLevels( 0 ), m_nStride( 0 ), m_nCapacitySlots( 0 ),
      m_nActiveSlots( 0 ), m_nFreeSlots( 0 ), m_nLiveSlots( 0 ),
      m_nHotWord( UINT32_MAX )
{
}

uint32_t SIRBitmapAllocator::findSetBit( uint64_t nWord )
{
#if defined( _MSC_VER )
	unsigned long nBit = 0;
	#if defined( _M_X64 )
	_BitScanForward64( &nBit, nWord );
	#else
	if ( ( (uint32_t)( nWord ) ) != 0 ) {
		_BitScanForward( &nBit, ( (uint32_t)( nWord ) ) );
	} else {
		_BitScanForward( &nBit, ( (uint32_t)( nWord >> 32 ) ) );
		nBit += 32;
	}
	#endif
	return ( (uint32_t)( nBit ) );
#else
	return ( (uint32_t)( __builtin_ctzll( nWord ) ) );
#endif
}

uint32_t SIRBitmapAllocator::highestSetBit( uint64_t nWord )
{
#if defined( _MSC_VER )
	unsigned long nBit = 0;
	#if defined( _M_X64 )
	_BitScanReverse64( &nBit, nWord );
	#else
	if ( ( (uint32_t)( nWord >> 32 ) ) != 0 ) {
		_BitScanReverse( &nBit, ( (uint32_t)( nWord >> 32 ) ) );
		nBit += 32;
	} else {
		_BitScanReverse( &nBit, ( (uint32_t)( nWord ) ) );
	}
	#endif
	return ( (uint32_t)( nBit ) );
#else
	return 63u - ( (uint32_t)( __builtin_clzll( nWord ) ) );
#endif
}

uint64_t SIRBitmapAllocator::getRequiredMetadataBytes( uint64_t nSegmentBytes,
                                                       uint32_t nObjectBytes,
                                                       uint32_t nAlignment )
{
	if ( nObjectBytes == 0 || !SIR_IS_POWER_OF_TWO( nAlignment ) ||
	     nAlignment > 65536 ) {
		return 0;
	}
	uint64_t nStride = 0;
	const uint64_t nMinimum = nObjectBytes > nAlignment ? nObjectBytes : nAlignment;
	if ( !SIR_IS_POWER_OF_TWO( nAlignment ) ||
	     ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
	     ( uint64_t )( nMinimum ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment ) - 1 ) )
	{
	    return 0;
	}
	nStride = SIR_ALIGN_UP( nMinimum, nAlignment );
	if ( nStride == 0 )
	{
	    return 0;
	}
	uint64_t nSlots = nSegmentBytes / nStride;
	if ( nSlots == 0 || nSlots > UINT32_MAX ) {
		return 0;
	}
	uint64_t nBytes = 0;
	do {
		nSlots = SIR_ROUND_UP_MULTIPLE( nSlots, 64ull ) / 64;
		nBytes += nSlots * sizeof( uint64_t );
	} while ( nSlots > 1 );
	return nBytes + 6 * 8; // Alignment headroom between individually allocated arrays.
}

bool SIRBitmapAllocator::init( SIRMemorySegment *pSegment, uint32_t nObjectBytes,
                               uint32_t nAlignment, uint64_t nCommitIncrementBytes )
{
	if ( m_pSegment != nullptr || pSegment == nullptr || nObjectBytes == 0 ||
	     !SIR_IS_POWER_OF_TWO( nAlignment ) ||
	     nAlignment > pSegment->getPageSize() ) {
		return false;
	}
	uint64_t nStride = 0;
	if ( !SIR_IS_POWER_OF_TWO( nAlignment ) ||
	     ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
	     ( uint64_t )( nObjectBytes > nAlignment ? nObjectBytes : nAlignment ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment ) - 1 ) )
	{
	    return false;
	}
	nStride = SIR_ALIGN_UP( nObjectBytes > nAlignment ? nObjectBytes : nAlignment, nAlignment );
	if ( nStride > UINT32_MAX )
	{
	    return false;
	}
	const uint64_t nSlots = pSegment->getReservedBytes() / nStride;
	if ( nSlots == 0 || nSlots > UINT32_MAX ) {
		return false;
	}
	const uint64_t nPage = pSegment->getPageSize();
	const uint64_t nIncrement = nCommitIncrementBytes != 0 ? nCommitIncrementBytes : nPage;
	uint64_t nRoundedIncrement = 0;
	{
	    const uint64_t nRoundValue = nIncrement;
	    const uint64_t nRoundMultiple = nPage;
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
	    nRoundedIncrement = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 )
	        ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
	        : nRoundValue + nPadding;
	}
	uint32_t nCount = 0;
	uint64_t nWords = SIR_ROUND_UP_MULTIPLE( nSlots, 64ull ) / 64;
	uint64_t nWordCounts[ SIR_BITMAP_LEVELS ] = {};
	do {
		if ( nCount == SIR_BITMAP_LEVELS || nWords > UINT32_MAX ) {
			return false;
		}
		nWordCounts[ nCount++ ] = nWords;
		nWords = SIR_ROUND_UP_MULTIPLE( nWords, 64ull ) / 64;
	} while ( nWords != 1 || nWordCounts[ nCount - 1 ] != 1 );

	SIRMemoryManager *pManager = pSegment->getManager();
	if ( pManager == nullptr ) {
		return false;
	}
	uint64_t *pArrays[ SIR_BITMAP_LEVELS ] = {};
	for ( uint32_t i = 0; i < nCount; ++i ) {
		pArrays[ i ] = ( (uint64_t *)( pManager->allocateMetadata( nWordCounts[ i ] * sizeof( uint64_t ), alignof( uint64_t ) ) ) );
		if ( pArrays[ i ] == nullptr ) {
			return false;
		}
	}
	m_pSegment = pSegment;
	m_pBase = ( (uint8_t *)( pSegment->getBase() ) );
	for ( uint32_t i = 0; i < nCount; ++i ) {
		m_pLevels[ i ] = pArrays[ i ];
		m_nLevelWords[ i ] = ( (uint32_t)( nWordCounts[ i ] ) );
	}
	m_nLevels = nCount;
	m_nStride = ( (uint32_t)( nStride ) );
	m_nCapacitySlots = ( (uint32_t)( nSlots ) );
	m_nCommitIncrementBytes = nRoundedIncrement;
	return true;
}

void SIRBitmapAllocator::updateWordSummary( uint32_t nWordIndex, bool bAvailable )
{
	uint32_t nIndex = nWordIndex;
	for ( uint32_t i = 1; i < m_nLevels; ++i ) {
		uint64_t& nWord = m_pLevels[ i ][ nIndex >> 6 ];
		const bool bWasEmpty = nWord == 0;
		const uint64_t nMask = 1ull << ( nIndex & 63 );
		if ( bAvailable ) {
			nWord |= nMask;
		} else {
			nWord &= ~nMask;
		}
		if ( bWasEmpty == ( nWord == 0 ) ) {
			break;
		}
		nIndex >>= 6;
	}
}

void SIRBitmapAllocator::changeFreeRange( uint32_t nBegin, uint32_t nEnd,
                                          bool bSetFree )
{
	uint64_t nStart = nBegin;
	while ( nStart < nEnd ) {
		const uint32_t nWordIndex = ( (uint32_t)( nStart >> 6 ) );
		const uint32_t nBit = ( (uint32_t)( nStart & 63 ) );
		const uint32_t nCount = ( (uint32_t)( ( nEnd - nStart ) < ( 64 - nBit ) ? ( nEnd - nStart ) : ( 64 - nBit ) ) );
		const uint64_t nMask = ( nCount == 64 ? UINT64_MAX : ( ( 1ull << nCount ) - 1 ) ) << nBit;
		uint64_t& nWord = m_pLevels[ 0 ][ nWordIndex ];
		const bool bWasEmpty = nWord == 0;
		if ( bSetFree ) {
			nWord |= nMask;
		} else {
			nWord &= ~nMask;
		}
		if ( bWasEmpty != ( nWord == 0 ) ) {
			updateWordSummary( nWordIndex, nWord != 0 );
		}
		nStart += nCount;
	}
}

void SIRBitmapAllocator::setFree( uint32_t nSlot )
{
	uint32_t nIndex = nSlot;
	for ( uint32_t i = 0; i < m_nLevels; ++i ) {
		uint64_t& nWord = m_pLevels[ i ][ nIndex / 64 ];
		const uint64_t nBefore = nWord;
		nWord |= 1ull << ( nIndex % 64 );
		if ( nBefore != 0 ) {
			break;
		}
		nIndex /= 64;
	}
}

bool SIRBitmapAllocator::isFree( uint32_t nSlot ) const
{
	return ( m_pLevels[ 0 ][ nSlot / 64 ] & ( 1ull << ( nSlot % 64 ) ) ) != 0;
}

bool SIRBitmapAllocator::grow( uint32_t nMinimumSlots )
{
	if ( m_pSegment == nullptr || nMinimumSlots > m_nCapacitySlots ) {
		return false;
	}
	const uint64_t nTargetBytes = ( (uint64_t)( nMinimumSlots ) ) * m_nStride;
	uint64_t nCommitEnd = 0;
	{
	    const uint64_t nRoundValue = nTargetBytes;
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
	    nCommitEnd = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 )
	        ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
	        : nRoundValue + nPadding;
	}
	if ( nCommitEnd > m_pSegment->getReservedBytes() ) {
		if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) ||
		     ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
		     ( uint64_t )( nTargetBytes ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) )
		{
		    return false;
		}
		nCommitEnd = SIR_ALIGN_UP( nTargetBytes, m_pSegment->getPageSize() );
		if ( nCommitEnd > m_pSegment->getReservedBytes() )
		{
		    return false;
		}
	}
	if ( nCommitEnd <= m_nCommittedBytes ) {
		return true;
	}
	if ( !m_pSegment->commit( m_nCommittedBytes, nCommitEnd - m_nCommittedBytes ) ) {
		return false;
	}
	uint64_t nActivated = nCommitEnd / m_nStride;
	if ( nActivated > m_nCapacitySlots ) {
		nActivated = m_nCapacitySlots;
	}
	const uint32_t nOldActive = m_nActiveSlots;
	changeFreeRange( nOldActive, ( (uint32_t)( nActivated ) ), true );
	if ( m_nHotWord == UINT32_MAX ) {
		m_nHotWord = nOldActive >> 6;
	}
	m_nFreeSlots += ( (uint32_t)( nActivated ) ) - nOldActive;
	m_nActiveSlots = ( (uint32_t)( nActivated ) );
	m_nCommittedBytes = nCommitEnd;
	return true;
}

bool SIRBitmapAllocator::precommitSlots( uint32_t nSlots )
{
	return nSlots <= m_nActiveSlots || grow( nSlots );
}

void *SIRBitmapAllocator::alloc( void )
{
	if ( m_pSegment == nullptr ) {
		return nullptr;
	}
	if ( m_nFreeSlots == 0 &&
	     ( m_nActiveSlots == m_nCapacitySlots || !grow( m_nActiveSlots + 1 ) ) ) {
		return nullptr;
	}
	uint32_t nWordIndex = m_nHotWord;
	uint64_t nWord = nWordIndex != UINT32_MAX ? m_pLevels[ 0 ][ nWordIndex ] : 0;
	if ( nWord == 0 ) {
		uint32_t nIndex = 0;
		for ( uint32_t i = m_nLevels; i > 0; --i ) {
			const uint64_t nCandidates = m_pLevels[ i - 1 ][ nIndex ];
			if ( nCandidates == 0 ) {
				return nullptr;
			}
			nIndex = nIndex * 64 + findSetBit( nCandidates );
		}
		nWordIndex = nIndex >> 6;
		nWord = m_pLevels[ 0 ][ nWordIndex ];
	}
	const uint32_t nIndex = nWordIndex * 64 + findSetBit( nWord );
	if ( nIndex >= m_nActiveSlots ) {
		return nullptr;
	}
	uint64_t& nFreeWord = m_pLevels[ 0 ][ nWordIndex ];
	nFreeWord &= ~( 1ull << ( nIndex & 63 ) );
	if ( nFreeWord == 0 ) {
		updateWordSummary( nWordIndex, false );
		m_nHotWord = UINT32_MAX;
	} else {
		m_nHotWord = nWordIndex;
	}
	--m_nFreeSlots;
	++m_nLiveSlots;
	return m_pBase + ( (uint64_t)( nIndex ) ) * m_nStride;
}

bool SIRBitmapAllocator::free( void *pMemory )
{
	if ( m_pSegment == nullptr || pMemory == nullptr || m_nLiveSlots == 0 ) {
		return false;
	}
	const uintptr_t nBase = ( (uintptr_t)( m_pBase ) );
	const uintptr_t nPtr = ( (uintptr_t)( pMemory ) );
	if ( nPtr < nBase ) {
		return false;
	}
	const uint64_t nDelta = nPtr - nBase;
	if ( nDelta % m_nStride != 0 || nDelta / m_nStride >= m_nActiveSlots ) {
		return false;
	}
	const uint32_t nIndex = ( (uint32_t)( nDelta / m_nStride ) );
	if ( isFree( nIndex ) ) {
		return false;
	}
	setFree( nIndex );
	m_nHotWord = nIndex >> 6;
	++m_nFreeSlots;
	--m_nLiveSlots;
	return true;
}

bool SIRBitmapAllocator::trimUnusedPages( void )
{
	if ( m_pSegment == nullptr ) {
		return false;
	}
	// Find the last live slot a word at a time instead of testing slots.
	uint32_t nRequired = 0;
	for ( uint32_t i = ( m_nActiveSlots + 63ull ) / 64; i > 0; --i ) {
		const uint32_t nWordIndex = i - 1;
		const uint32_t nValid = nWordIndex == ( m_nActiveSlots - 1 ) / 64 ? ( ( m_nActiveSlots - 1 ) & 63 ) + 1 : 64;
		const uint64_t nMask = nValid == 64 ? UINT64_MAX : ( 1ull << nValid ) - 1;
		const uint64_t nUsed = ~m_pLevels[ 0 ][ nWordIndex ] & nMask;
		if ( nUsed != 0 ) {
			nRequired = nWordIndex * 64 + highestSetBit( nUsed ) + 1;
			break;
		}
	}
	uint64_t nEnd = 0;
	if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) ||
	     ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
	     ( uint64_t )( ( (uint64_t)( nRequired ) ) * m_nStride ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) )
	{
	    return false;
	}
	nEnd = SIR_ALIGN_UP( ( (uint64_t)( nRequired ) ) * m_nStride, m_pSegment->getPageSize() );
	if ( nEnd == m_nCommittedBytes ) {
		return true;
	}
	if ( !m_pSegment->decommit( nEnd, m_nCommittedBytes - nEnd ) ) {
		return false;
	}
	uint64_t nNewActive = nEnd / m_nStride;
	if ( nNewActive > m_nCapacitySlots ) {
		nNewActive = m_nCapacitySlots;
	}
	changeFreeRange( ( (uint32_t)( nNewActive ) ), m_nActiveSlots, false );
	m_nFreeSlots -= m_nActiveSlots - ( (uint32_t)( nNewActive ) );
	m_nActiveSlots = ( (uint32_t)( nNewActive ) );
	if ( m_nHotWord != UINT32_MAX &&
	     ( m_nHotWord >= ( m_nActiveSlots + 63ull ) / 64 ||
	       m_pLevels[ 0 ][ m_nHotWord ] == 0 ) ) {
		m_nHotWord = UINT32_MAX;
	}
	m_nCommittedBytes = nEnd;
	return true;
}

bool SIRBitmapAllocator::shutdown( void )
{
	if ( m_pSegment == nullptr || m_nLiveSlots != 0 ) {
		return false;
	}
	if ( m_nCommittedBytes != 0 && !m_pSegment->decommit( 0, m_nCommittedBytes ) ) {
		return false;
	}
	m_pSegment = nullptr;
	m_pBase = nullptr;
	m_nCommittedBytes = 0;
	m_nActiveSlots = 0;
	m_nLiveSlots = 0;
	m_nFreeSlots = 0;
	m_nHotWord = UINT32_MAX;
	return true;
}

uint32_t SIRBitmapAllocator::getCapacitySlots( void ) const
{
	return m_nCapacitySlots;
}
uint32_t SIRBitmapAllocator::getActiveSlots( void ) const
{
	return m_nActiveSlots;
}
uint32_t SIRBitmapAllocator::getLiveSlots( void ) const
{
	return m_nLiveSlots;
}
uint32_t SIRBitmapAllocator::getSlotStride( void ) const
{
	return m_nStride;
}
