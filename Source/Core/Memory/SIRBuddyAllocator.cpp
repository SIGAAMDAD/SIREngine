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

#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryUtils.h>

#include <cstring>
#include <limits>

SIRBuddyAllocator::SIRBuddyAllocator( void )
    : m_pSegment( nullptr ), m_pFreeOrders( nullptr ), m_pAllocatedOrders( nullptr ), m_pCommitTree( nullptr ),
      m_pCommittedLeafBits( nullptr ), m_nMinBlockBytes( 0 ), m_nAllocatedBytes( 0 ), m_nLeafCount( 0 ), m_nAllocationCount( 0 ),
      m_nRootOrder( 0 )
{
}

uint64_t SIRBuddyAllocator::getRequiredMetadataBytes( uint64_t nCapacityBytes, uint64_t nMinimumBlockBytes )
{
	if ( !SIR_IS_POWER_OF_TWO( nCapacityBytes ) || !SIR_IS_POWER_OF_TWO( nMinimumBlockBytes ) || nCapacityBytes < nMinimumBlockBytes ) {
		return 0;
	}

	const uint64_t nLeaves = nCapacityBytes / nMinimumBlockBytes;
	if ( nLeaves > ( std::numeric_limits<uint32_t>::max )() / 2 ) {
		return 0;
	}

	// 2*N free-order nodes, N allocation orders, and two bits for each
	// commitment-tree node. Unchanged 32-bit extent/leaf limits.
	// Additional packed leaf bits provide one-load warm checks for single
	// page blocks; seven bytes cover 64-bit metadata alignment.
#ifdef SIR_BUDDY_EXPERIMENT_PACKED_ORDERS
	const uint64_t nFreeBytes = ( 10 * nLeaves + 7 ) / 8;
#else
	const uint64_t nFreeBytes = 2 * nLeaves;
#endif
	return nFreeBytes + nLeaves + ( nLeaves + 1 ) / 2 + ( SIR_ROUND_UP_MULTIPLE( nLeaves, 64ull ) / 64 ) * sizeof( uint64_t ) + 7;
}

bool SIRBuddyAllocator::init( SIRMemorySegment *pSegment, uint64_t nMinimumBlockBytes )
{
	if ( pSegment == nullptr || m_pSegment != nullptr ) {
		return false;
	}

	const uint64_t nPageSize = pSegment->getPageSize();
	const uint64_t nMinimum = nMinimumBlockBytes != 0 ? nMinimumBlockBytes : nPageSize;
	const uint64_t nCapacity = pSegment->getReservedBytes();
	if ( nMinimum < nPageSize || nMinimum % nPageSize != 0 ) {
		return false;
	}

	const uint64_t nMetadataBytes = getRequiredMetadataBytes( nCapacity, nMinimum );
	if ( nMetadataBytes == 0 ) {
		return false;
	}

	uint8_t *pMetadata =
	    ( ( uint8_t * )( pSegment->getManager()->allocateMetadata( nMetadataBytes, alignof( uint64_t ), pSegment->getDomain() ) ) );
	if ( pMetadata == nullptr ) {
		return false;
	}

	const uint32_t nLeaves = ( ( uint32_t )( nCapacity / nMinimum ) );
	uint8_t nRootOrder = 0;
	for ( uint32_t nRemaining = nLeaves; nRemaining > 1; nRemaining >>= 1 ) {
		++nRootOrder;
	}

	m_pSegment = pSegment;
	m_pFreeOrders = pMetadata;
#ifdef SIR_BUDDY_EXPERIMENT_PACKED_ORDERS
	const uint64_t nFreeBytes = ( 10 * ( ( uint64_t )( nLeaves ) ) + 7 ) / 8;
#else
	const uint64_t nFreeBytes = 2 * ( ( uint64_t )( nLeaves ) );
#endif
	m_pAllocatedOrders = pMetadata + nFreeBytes;
	m_pCommitTree = m_pAllocatedOrders + nLeaves;
	const uint64_t nTreeBytes = ( ( ( uint64_t )( nLeaves ) ) + 1 ) / 2;
	const uintptr_t nBitsetStart = ( ( uintptr_t )( m_pCommitTree ) ) + nTreeBytes;
	const uintptr_t nAlignedBitset = SIR_ALIGN_UP( nBitsetStart, 8ull );
	m_pCommittedLeafBits = ( ( uint64_t * )( nAlignedBitset ) );
	m_nMinBlockBytes = nMinimum;
	m_nLeafCount = nLeaves;
	m_nRootOrder = nRootOrder;

	uint32_t nFirst = 1;
	uint32_t nNodesAtLevel = 1;
	for ( int32_t nOrder = nRootOrder; nOrder >= 0; --nOrder ) {
		for ( uint32_t i = 0; i < nNodesAtLevel; ++i ) {
			writeFreeOrder( nFirst + i, ( ( uint8_t )( nOrder ) ) );
		}
		nFirst += nNodesAtLevel;
		nNodesAtLevel <<= 1;
	}
	std::memset( m_pAllocatedOrders, SIR_BUDDY_FULL, nLeaves );
	return true;
}

void *SIRBuddyAllocator::alloc( uint64_t nBytes, uint64_t nAlignment )
{
	if ( m_pSegment == nullptr || nBytes == 0 || !SIR_IS_POWER_OF_TWO( nAlignment ) || nAlignment > m_pSegment->getPageSize() ) {
		return nullptr;
	}

	uint8_t nOrder = 0;
	uint64_t nBlockBytes = m_nMinBlockBytes;
	const uint64_t nCapacity = m_pSegment->getReservedBytes();
	while ( nBlockBytes < nBytes ) {
		if ( nBlockBytes > nCapacity / 2 ) {
			return nullptr;
		}
		nBlockBytes <<= 1;
		++nOrder;
	}
	if ( nOrder > m_nRootOrder || readFreeOrder( 1 ) == SIR_BUDDY_FULL || readFreeOrder( 1 ) < nOrder ) {
		return nullptr;
	}

	uint32_t nNode = 1;
	uint64_t nOffset = 0;
	uint8_t nTraversalOrder = m_nRootOrder;
	while ( nTraversalOrder > nOrder ) {
		--nTraversalOrder;
		const uint32_t nLeft = nNode << 1;
		const uint8_t nLeftOrder = readFreeOrder( nLeft );
		if ( nLeftOrder != SIR_BUDDY_FULL && nLeftOrder >= nOrder ) {
			nNode = nLeft;
		} else {
			nNode = nLeft + 1;
			nOffset += m_nMinBlockBytes << nTraversalOrder;
		}
	}

	// A single minimum-block allocation needs only one packed-bit read.
	// Larger allocations consult the lazy two-bit commitment tree.
	const uint32_t nFirstLeaf = ( ( uint32_t )( nOffset / m_nMinBlockBytes ) );
	const uint32_t nLeafCount = ( ( uint32_t )( nBlockBytes / m_nMinBlockBytes ) );
	bool bWarm = false;
	if ( nOrder <= 6 ) {
		// Aligned buddy blocks of <=64 leaves never cross a bitmap word.
		const uint64_t nMask = ( nLeafCount == 64 ? UINT64_MAX : ( ( 1ull << nLeafCount ) - 1 ) ) << ( nFirstLeaf & 63 );
		bWarm = ( m_pCommittedLeafBits[nFirstLeaf >> 6] & nMask ) == nMask;
	} else {
		bWarm = effectiveCommitState( nNode ) == SIR_COMMIT_FULL;
	}
	if ( !bWarm ) {
		// commit first; failure must not modify free-tree or commitment state.
		if ( !m_pSegment->commit( nOffset, nBlockBytes ) ) {
			return nullptr;
		}
		setCommitState( nNode, SIR_COMMIT_FULL );
		setLeafCommitRange( nFirstLeaf, nLeafCount, true );
	}

	writeFreeOrder( nNode, SIR_BUDDY_FULL );
	m_pAllocatedOrders[nOffset / m_nMinBlockBytes] = nOrder;
	updateParent( nNode, nOrder );
	m_nAllocatedBytes += nBlockBytes;
	++m_nAllocationCount;
	return ( ( uint8_t * )( m_pSegment->getBase() ) ) + nOffset;
}

uint64_t SIRBuddyAllocator::getUsableSize( const void *pMemory ) const
{
	if ( m_pSegment == nullptr || pMemory == nullptr ) {
		return 0;
	}
	const uintptr_t nBase = ( ( uintptr_t )( m_pSegment->getBase() ) );
	const uintptr_t nUser = ( ( uintptr_t )( pMemory ) );
	if ( nUser < nBase || nUser - nBase >= m_pSegment->getReservedBytes() ) {
		return 0;
	}
	const uint64_t nOffset = nUser - nBase;
	if ( ( nOffset % m_nMinBlockBytes ) != 0 ) {
		return 0;
	}
	const uint32_t nIndex = ( ( uint32_t )( nOffset / m_nMinBlockBytes ) );
	const uint8_t nOrder = m_pAllocatedOrders[nIndex];
	if ( nOrder > m_nRootOrder ) {
		return 0;
	}
	const uint64_t nSize = m_nMinBlockBytes << nOrder;
	return ( nOffset % nSize ) == 0 ? nSize : 0;
}

bool SIRBuddyAllocator::free( void *pMemory )
{
	if ( m_pSegment == nullptr || pMemory == nullptr ) {
		return false;
	}

	const uintptr_t nBase = ( ( uintptr_t )( m_pSegment->getBase() ) );
	const uintptr_t nAddress = ( ( uintptr_t )( pMemory ) );
	if ( nAddress < nBase || nAddress - nBase >= m_pSegment->getReservedBytes() ) {
		return false;
	}

	const uint64_t nOffset = nAddress - nBase;
	if ( nOffset % m_nMinBlockBytes != 0 ) {
		return false;
	}

	const uint32_t nLeafIndex = ( ( uint32_t )( nOffset / m_nMinBlockBytes ) );
	const uint8_t nOrder = m_pAllocatedOrders[nLeafIndex];
	if ( nOrder == SIR_BUDDY_FULL || nOrder > m_nRootOrder ) {
		return false;
	}

	const uint64_t nBlockBytes = m_nMinBlockBytes << nOrder;
	if ( nOffset % nBlockBytes != 0 ) {
		return false;
	}

	const uint8_t nDepth = m_nRootOrder - nOrder;
	const uint32_t nNode = ( 1u << nDepth ) + ( ( uint32_t )( nOffset / nBlockBytes ) );
	if ( readFreeOrder( nNode ) != SIR_BUDDY_FULL ) {
		return false;
	}

	m_pAllocatedOrders[nLeafIndex] = SIR_BUDDY_FULL;
	writeFreeOrder( nNode, nOrder );
	updateParent( nNode, nOrder );
	m_nAllocatedBytes -= nBlockBytes;
	--m_nAllocationCount;
	return true;
}

uint8_t SIRBuddyAllocator::readFreeOrder( uint32_t nNode ) const
{
#ifdef SIR_BUDDY_EXPERIMENT_PACKED_ORDERS
	const uint64_t nBit = ( ( uint64_t )( nNode ) ) * 5;
	const uint64_t nByte = nBit >> 3;
	const uint32_t nShift = ( ( uint32_t )( nBit & 7 ) );
	uint16_t nValue = m_pFreeOrders[nByte];
	if ( nShift > 3 ) {
		nValue |= ( ( uint16_t )( m_pFreeOrders[nByte + 1] ) ) << 8;
	}
	const uint8_t nPacked = ( ( uint8_t )( ( nValue >> nShift ) & 31 ) );
	return nPacked == 31 ? SIR_BUDDY_FULL : nPacked;
#else
	return m_pFreeOrders[nNode];
#endif
}

void SIRBuddyAllocator::writeFreeOrder( uint32_t nNode, uint8_t nOrder )
{
#ifdef SIR_BUDDY_EXPERIMENT_PACKED_ORDERS
	const uint64_t nBit = ( ( uint64_t )( nNode ) ) * 5;
	const uint64_t nByte = nBit >> 3;
	const uint32_t nShift = ( ( uint32_t )( nBit & 7 ) );
	const uint16_t nMask = ( ( uint16_t )( 31u << nShift ) );
	const uint16_t nOrderBits = ( ( uint16_t )( ( nOrder == SIR_BUDDY_FULL ? 31u : nOrder ) << nShift ) );
	uint16_t nValue = m_pFreeOrders[nByte];
	if ( nShift > 3 ) {
		nValue |= ( ( uint16_t )( m_pFreeOrders[nByte + 1] ) ) << 8;
	}
	nValue = ( ( uint16_t )( ( nValue & ~nMask ) | nOrderBits ) );
	m_pFreeOrders[nByte] = ( ( uint8_t )( nValue ) );
	if ( nShift > 3 ) {
		m_pFreeOrders[nByte + 1] = ( ( uint8_t )( nValue >> 8 ) );
	}
#else
	m_pFreeOrders[nNode] = nOrder;
#endif
}

void SIRBuddyAllocator::updateParent( uint32_t nNode, uint8_t nChildOrder )
{
	while ( nNode > 1 ) {
		const uint32_t nParent = nNode / 2;
		const uint8_t nLeft = readFreeOrder( nParent * 2 );
		const uint8_t nRight = readFreeOrder( nParent * 2 + 1 );
		uint8_t nParentOrder = SIR_BUDDY_FULL;
		if ( nLeft == nChildOrder && nRight == nChildOrder ) {
			nParentOrder = nChildOrder + 1;
		} else if ( nLeft == SIR_BUDDY_FULL ) {
			nParentOrder = nRight;
		} else if ( nRight == SIR_BUDDY_FULL ) {
			nParentOrder = nLeft;
		} else {
			nParentOrder = nLeft > nRight ? nLeft : nRight;
		}

		// An unchanged aggregate cannot modify any higher ancestor.
		if ( readFreeOrder( nParent ) == nParentOrder ) {
			return;
		}
		writeFreeOrder( nParent, nParentOrder );
		nNode = nParent;
		++nChildOrder;
	}
}

uint8_t SIRBuddyAllocator::readCommitState( uint32_t nNode ) const
{
	const uint32_t nBit = nNode * 2;
	return ( ( uint8_t )( ( m_pCommitTree[nBit / 8] >> ( nBit % 8 ) ) & 3u ) );
}

void SIRBuddyAllocator::writeCommitState( uint32_t nNode, uint8_t nState )
{
	const uint32_t nBit = nNode * 2;
	const uint8_t nMask = ( ( uint8_t )( 3u << ( nBit % 8 ) ) );
	uint8_t &nWord = m_pCommitTree[nBit / 8];
	nWord = ( ( uint8_t )( ( nWord & ~nMask ) | ( nState << ( nBit % 8 ) ) ) );
}

uint8_t SIRBuddyAllocator::effectiveCommitState( uint32_t nNode ) const
{
	// A parent marked FULL/NONE lazily overrides its descendants.
	uint32_t nHighest = 1;
	while ( nHighest <= nNode / 2 ) {
		nHighest <<= 1;
	}
	uint32_t nCurrent = 1;
	for ( uint32_t nBit = nHighest >> 1; nBit != 0; nBit >>= 1 ) {
		const uint8_t nState = readCommitState( nCurrent );
		if ( nState != SIR_COMMIT_MIXED ) {
			return nState;
		}
		nCurrent = ( nCurrent << 1 ) | ( ( nNode & nBit ) != 0 ? 1u : 0u );
	}
	return readCommitState( nCurrent );
}

void SIRBuddyAllocator::setCommitState( uint32_t nNode, uint8_t nState )
{
	// Push lazy uniform state only along the path being changed.
	uint32_t nHighest = 1;
	while ( nHighest <= nNode / 2 ) {
		nHighest <<= 1;
	}
	uint32_t nCurrent = 1;
	for ( uint32_t nBit = nHighest >> 1; nBit != 0; nBit >>= 1 ) {
		const uint8_t nPrevious = readCommitState( nCurrent );
		if ( nPrevious != SIR_COMMIT_MIXED ) {
			writeCommitState( nCurrent << 1, nPrevious );
			writeCommitState( ( nCurrent << 1 ) + 1, nPrevious );
			writeCommitState( nCurrent, SIR_COMMIT_MIXED );
		}
		nCurrent = ( nCurrent << 1 ) | ( ( nNode & nBit ) != 0 ? 1u : 0u );
	}
	writeCommitState( nNode, nState );
	for ( nCurrent = nNode >> 1; nCurrent != 0; nCurrent >>= 1 ) {
		const uint8_t nLeft = readCommitState( nCurrent << 1 );
		const uint8_t nRight = readCommitState( ( nCurrent << 1 ) + 1 );
		writeCommitState( nCurrent, nLeft == nRight ? nLeft : SIR_COMMIT_MIXED );
	}
}

void SIRBuddyAllocator::setLeafCommitRange( uint32_t nFirst, uint32_t nCount, bool bCommitted )
{
	uint32_t nIndex = nFirst;
	const uint32_t nLast = nFirst + nCount;
	while ( nIndex < nLast ) {
		const uint32_t nBit = nIndex & 63;
		const uint32_t nSpan = ( nLast - nIndex ) < ( 64 - nBit ) ? ( nLast - nIndex ) : ( 64 - nBit );
		const uint64_t nMask = ( nSpan == 64 ? UINT64_MAX : ( ( 1ull << nSpan ) - 1 ) ) << nBit;
		uint64_t &nWord = m_pCommittedLeafBits[nIndex >> 6];
		nWord = bCommitted ? ( nWord | nMask ) : ( nWord & ~nMask );
		nIndex += nSpan;
	}
}

bool SIRBuddyAllocator::trimNode( uint32_t nNode, uint8_t nOrder, uint64_t nOffset )
{
	const uint8_t nFreeOrder = readFreeOrder( nNode );
	if ( nFreeOrder == SIR_BUDDY_FULL || effectiveCommitState( nNode ) == SIR_COMMIT_NONE ) {
		return true;
	}

	const uint64_t nBlockBytes = m_nMinBlockBytes << nOrder;
	if ( nFreeOrder == nOrder ) {
		if ( !m_pSegment->decommit( nOffset, nBlockBytes ) ) {
			return false;
		}
		setCommitState( nNode, SIR_COMMIT_NONE );
		setLeafCommitRange( ( ( uint32_t )( nOffset / m_nMinBlockBytes ) ), ( ( uint32_t )( nBlockBytes / m_nMinBlockBytes ) ), false );
		return true;
	}
	if ( nOrder == 0 ) {
		return false;
	}
	return trimNode( nNode * 2, nOrder - 1, nOffset ) && trimNode( nNode * 2 + 1, nOrder - 1, nOffset + nBlockBytes / 2 );
}

bool SIRBuddyAllocator::trimUnusedPages( void )
{
	return m_pSegment != nullptr && trimNode( 1, m_nRootOrder, 0 );
}

uint64_t SIRBuddyAllocator::getAllocatedBytes( void ) const
{
	return m_nAllocatedBytes;
}

uint32_t SIRBuddyAllocator::getAllocationCount( void ) const
{
	return m_nAllocationCount;
}

uint64_t SIRBuddyAllocator::getMinimumBlockBytes( void ) const
{
	return m_nMinBlockBytes;
}

uint64_t SIRBuddyAllocator::getCapacity( void ) const
{
	return m_pSegment != nullptr ? m_pSegment->getReservedBytes() : 0;
}
