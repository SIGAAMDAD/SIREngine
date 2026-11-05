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

#include <Core/Memory/Allocators/SIRMediumPageAllocator.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <cstring>
#include <new>

uint32_t SIRMediumPageAllocator::blockBytes( const sirBlock_t *pBlock )
{
	return pBlock->nSizeFlags & ~15u;
}

bool SIRMediumPageAllocator::isFree( const sirBlock_t *pBlock )
{
	return ( pBlock->nSizeFlags & SIR_FREE ) != 0;
}

SIRMediumPageAllocator::SIRMediumPageAllocator( void )
    : m_pSegment( nullptr ), m_pBase( nullptr ), m_pExtents( nullptr ), m_pLiveStarts( nullptr ), m_nExtentBytes( 0 ), m_nExtentCount( 0 ),
      m_nLive( 0 )
{
}

bool SIRMediumPageAllocator::init( SIRMemorySegment *pSegment, uint32_t nExtentBytes )
{
	if ( m_pSegment != nullptr || pSegment == nullptr || !SIR_IS_POWER_OF_TWO( nExtentBytes ) || nExtentBytes < 4096 ||
	     nExtentBytes < pSegment->getPageSize() || nExtentBytes % pSegment->getPageSize() != 0 || pSegment->getReservedBytes() == 0 ||
	     pSegment->getReservedBytes() % nExtentBytes != 0 || pSegment->getReservedBytes() / nExtentBytes > UINT32_MAX ) {
		return false;
	}
	const uint32_t nCount = ( ( uint32_t )( pSegment->getReservedBytes() / nExtentBytes ) );
	const uint64_t nUnits = pSegment->getReservedBytes() / 16;
	const uint64_t nDescriptorBytes = ( ( uint64_t )( nCount ) ) * sizeof( sirMediumPageExtent_t );
	const uint64_t nBitBytes = ( SIR_ROUND_UP_MULTIPLE( nUnits, 64ull ) / 64 ) * sizeof( uint64_t );
	void *pMetadata = pSegment->getManager()->allocateMetadata( nDescriptorBytes + nBitBytes, alignof( uint64_t ) );
	if ( pMetadata == nullptr ) {
		return false;
	}
	m_pLiveStarts = ( ( uint64_t * )( ( ( uint8_t * )( pMetadata ) ) + nDescriptorBytes ) );
	m_pSegment = pSegment;
	m_pBase = ( ( uint8_t * )( pSegment->getBase() ) );
	m_pExtents = ( ( sirMediumPageExtent_t * )( pMetadata ) );
	m_nExtentCount = nCount;
	m_nExtentBytes = nExtentBytes;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		m_pExtents[i] = { SIR_NONE, 0, 0, false };
	}
	return true;
}

sirBlock_t *SIRMediumPageAllocator::block( uint32_t nExtent, uint32_t nOffset ) const
{
	return ( ( sirBlock_t * )( m_pBase + ( ( uint64_t )( nExtent ) ) * m_nExtentBytes + nOffset ) );
}

bool SIRMediumPageAllocator::ensureExtent( uint32_t nExtent )
{
	sirMediumPageExtent_t& extent = m_pExtents[nExtent];
	if ( extent.bCommitted ) {
		return true;
	}
	if ( !m_pSegment->commit( ( ( uint64_t )( nExtent ) ) * m_nExtentBytes, m_nExtentBytes ) ) {
		return false;
	}
	extent = { 0, m_nExtentBytes, 0, true };
	*block( nExtent, 0 ) = { 0, m_nExtentBytes | SIR_FREE, SIR_NONE, SIR_NONE };
	return true;
}

void SIRMediumPageAllocator::removeFree( uint32_t nExtent, uint32_t nOffset )
{
	sirMediumPageExtent_t& extent = m_pExtents[nExtent];
	sirBlock_t *pBlock = block( nExtent, nOffset );
	if ( pBlock->nPrevFree == SIR_NONE ) {
		extent.nFreeHead = pBlock->nNextFree;
	} else {
		block( nExtent, pBlock->nPrevFree )->nNextFree = pBlock->nNextFree;
	}
	if ( pBlock->nNextFree != SIR_NONE ) {
		block( nExtent, pBlock->nNextFree )->nPrevFree = pBlock->nPrevFree;
	}
}

void SIRMediumPageAllocator::insertFree( uint32_t nExtent, uint32_t nOffset )
{
	sirMediumPageExtent_t& extent = m_pExtents[nExtent];
	sirBlock_t *pBlock = block( nExtent, nOffset );
	pBlock->nSizeFlags |= SIR_FREE;
	pBlock->nNextFree = extent.nFreeHead;
	pBlock->nPrevFree = SIR_NONE;
	if ( extent.nFreeHead != SIR_NONE ) {
		block( nExtent, extent.nFreeHead )->nPrevFree = nOffset;
	}
	extent.nFreeHead = nOffset;
}

void SIRMediumPageAllocator::recomputeLargest( uint32_t nExtent )
{
	uint32_t nLargest = 0;
	for ( uint32_t n = m_pExtents[nExtent].nFreeHead; n != SIR_NONE; n = block( nExtent, n )->nNextFree ) {
		const uint32_t nBytes = blockBytes( block( nExtent, n ) );
		if ( nBytes > nLargest ) {
			nLargest = nBytes;
		}
	}
	m_pExtents[nExtent].nLargest = nLargest;
}

void *SIRMediumPageAllocator::alloc( uint32_t nBytes, uint32_t nAlignment )
{
	if ( m_pSegment == nullptr || nBytes == 0 || !SIR_IS_POWER_OF_TWO( nAlignment ) || nAlignment > m_nExtentBytes ||
	     ( ( uint64_t )( nBytes ) ) + nAlignment + 32 > m_nExtentBytes ) {
		return nullptr;
	}
	// Search already-committed extents before activating a new one.
	// The previous loop eagerly committed the first dormant extent even when
	// a later committed extent had enough space for the request.
	for ( uint32_t nPass = 0; nPass < 2; ++nPass )
		for ( uint32_t i = 0; i < m_nExtentCount; ++i ) {
			sirMediumPageExtent_t& extent = m_pExtents[i];
			const bool bWasCommitted = extent.bCommitted;
			if ( nPass == 0 && !bWasCommitted ) {
				continue;
			}
			if ( nPass == 1 && ( bWasCommitted || !ensureExtent( i ) ) ) {
				continue;
			}
			if ( extent.nLargest < nBytes + 16 ) {
				continue;
			}
			for ( uint32_t n = extent.nFreeHead; n != SIR_NONE; ) {
				sirBlock_t *pBlock = block( i, n );
				const uint32_t nNext = pBlock->nNextFree;
				const uintptr_t nStart = ( ( uintptr_t )( pBlock ) );
				const uintptr_t nUser = SIR_ALIGN_UP( nStart + 16, ( ( uintptr_t )( nAlignment ) ) );
				const uint64_t nUsed = SIR_ALIGN_UP( nUser - nStart + nBytes, 16ull );
				const uint32_t nSize = blockBytes( pBlock );
				if ( nUsed <= nSize ) {
					const uint32_t nPrev = pBlock->nPrevBytes;
					removeFree( i, n );
					uint32_t nFinal = nSize;
					if ( nSize - nUsed >= 32 ) {
						nFinal = ( ( uint32_t )( nUsed ) );
						const uint32_t nTail = n + nFinal;
						*block( i, nTail ) = { nFinal, nSize - nFinal | SIR_FREE, SIR_NONE, SIR_NONE };
						insertFree( i, nTail );
					}
					*pBlock = { nPrev, nFinal, ( ( uint32_t )( nUser - nStart ) ), 0 };
					if ( n + nSize < m_nExtentBytes ) {
						block( i, n + nSize )->nPrevBytes = nSize == nFinal ? nFinal : nSize - nFinal;
					}
					if ( n + nFinal < m_nExtentBytes && nFinal != nSize && n + nSize == m_nExtentBytes ) {
						// The trailing free block terminates this extent.
					}
					const uint32_t nOffset = n;
					std::memcpy( ( ( void * )( nUser - 4 ) ), &nOffset, 4 );
					const uint64_t nIndex = ( ( ( uint64_t )( i ) ) * m_nExtentBytes + n ) / 16;
					m_pLiveStarts[nIndex >> 6] |= uint64_t( 1 ) << ( nIndex & 63 );
					++extent.nLive;
					++m_nLive;
					recomputeLargest( i );
					return ( ( void * )( nUser ) );
				}
				n = nNext;
			}
		}
	return nullptr;
}

bool SIRMediumPageAllocator::resolve( const void *pMemory, uint32_t *pExtent, uint32_t *pOffset, uint32_t *pUsable ) const
{
	if ( m_pSegment == nullptr || pMemory == nullptr ) {
		return false;
	}
	const uintptr_t nBase = ( ( uintptr_t )( m_pBase ) );
	const uintptr_t nUser = ( ( uintptr_t )( pMemory ) );
	if ( nUser < nBase + 16 || nUser - nBase >= m_pSegment->getReservedBytes() ) {
		return false;
	}
	const uint64_t nGlobal = nUser - nBase;
	const uint32_t nExtent = ( ( uint32_t )( nGlobal / m_nExtentBytes ) );
	const uint32_t nInner = ( ( uint32_t )( nGlobal % m_nExtentBytes ) );
	if ( !m_pExtents[nExtent].bCommitted || nInner < 16 || m_pExtents[nExtent].nLive == 0 ) {
		return false;
	}
	uint32_t nOffset = SIR_NONE;
	std::memcpy( &nOffset, ( ( const void * )( nUser - 4 ) ), 4 );
	if ( nOffset % 16 != 0 || nOffset > m_nExtentBytes - 32 || nOffset >= nInner ) {
		return false;
	}
	const uint64_t nIndex = ( ( ( uint64_t )( nExtent ) ) * m_nExtentBytes + nOffset ) / 16;
	if ( ( m_pLiveStarts[nIndex >> 6] & ( uint64_t( 1 ) << ( nIndex & 63 ) ) ) == 0 ) {
		return false;
	}
	const sirBlock_t *pBlock = block( nExtent, nOffset );
	const uint32_t nSize = blockBytes( pBlock );
	if ( isFree( pBlock ) || nSize < 32 || nSize > m_nExtentBytes - nOffset || nOffset + pBlock->nNextFree != nInner ||
	     nInner >= nOffset + nSize ) {
		return false;
	}
	if ( pExtent != nullptr ) {
		*pExtent = nExtent;
	}
	if ( pOffset != nullptr ) {
		*pOffset = nOffset;
	}
	if ( pUsable != nullptr ) {
		*pUsable = nOffset + nSize - nInner;
	}
	return true;
}

uint32_t SIRMediumPageAllocator::getUsableSize( const void *pMemory ) const
{
	uint32_t nSize = 0;
	return resolve( pMemory, nullptr, nullptr, &nSize ) ? nSize : 0;
}

bool SIRMediumPageAllocator::free( void *pMemory )
{
	uint32_t nExtent = 0, nOffset = 0;
	if ( !resolve( pMemory, &nExtent, &nOffset, nullptr ) ) {
		return false;
	}
	sirMediumPageExtent_t& extent = m_pExtents[nExtent];
	const uint64_t nIndex = ( ( ( uint64_t )( nExtent ) ) * m_nExtentBytes + nOffset ) / 16;
	m_pLiveStarts[nIndex >> 6] &= ~( uint64_t( 1 ) << ( nIndex & 63 ) );
	sirBlock_t *pBlock = block( nExtent, nOffset );
	uint32_t nSize = blockBytes( pBlock );
	const uint32_t nPrev = pBlock->nPrevBytes;
	const uint32_t nNext = nOffset + nSize;
	if ( nNext < m_nExtentBytes && isFree( block( nExtent, nNext ) ) ) {
		nSize += blockBytes( block( nExtent, nNext ) );
		removeFree( nExtent, nNext );
	}
	if ( nPrev != 0 && isFree( block( nExtent, nOffset - nPrev ) ) ) {
		const uint32_t nBefore = nOffset - nPrev;
		removeFree( nExtent, nBefore );
		nOffset = nBefore;
		pBlock = block( nExtent, nOffset );
		nSize += nPrev;
	}
	pBlock->nSizeFlags = nSize;
	if ( nOffset + nSize < m_nExtentBytes ) {
		block( nExtent, nOffset + nSize )->nPrevBytes = nSize;
	}
	insertFree( nExtent, nOffset );
	--extent.nLive;
	--m_nLive;
	recomputeLargest( nExtent );
	return true;
}

uint64_t SIRMediumPageAllocator::trimEmptyPages( uint32_t nMaximumWarmPages )
{
	if ( m_pSegment == nullptr ) {
		return 0;
	}
	uint64_t nReleased = 0;
	uint32_t nWarm = 0;
	for ( uint32_t i = 0; i < m_nExtentCount; ++i ) {
		sirMediumPageExtent_t& extent = m_pExtents[i];
		if ( !extent.bCommitted || extent.nLive != 0 ) {
			continue;
		}
		if ( ++nWarm <= nMaximumWarmPages ) {
			continue;
		}
		if ( m_pSegment->decommit( ( ( uint64_t )( i ) ) * m_nExtentBytes, m_nExtentBytes ) ) {
			extent = { SIR_NONE, 0, 0, false };
			nReleased += m_nExtentBytes;
		}
	}
	return nReleased;
}

bool SIRMediumPageAllocator::shutdown( void )
{
	if ( m_nLive != 0 ) {
		return false;
	}
	m_pSegment = nullptr;
	m_pBase = nullptr;
	m_pExtents = nullptr;
	m_pLiveStarts = nullptr;
	return true;
}
