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

#include <Core/Memory/Allocators/SIRHandleHeap.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <cstring>
#include <limits>

SIRHandleHeap::SIRHandleHeap( void )
    : m_pSegment( nullptr ), m_pBase( nullptr ), m_pHandles( nullptr ), m_pCold( nullptr ), m_pExtents( nullptr ), m_nCapacity( 0 ),
      m_nCommitted( 0 ), m_nHighWater( 0 ), m_nMaxHandles( 0 ), m_nNextHandle( 0 ), m_nFreeHandleHead( SIR_HANDLE_NONE ), m_nLiveCount( 0 ),
      m_nExtentCapacity( 0 ), m_nNextExtent( 0 ), m_nFreeExtentHead( SIR_HANDLE_NONE ), m_nAddressRoot( SIR_HANDLE_NONE ),
      m_nSizeRoot( SIR_HANDLE_NONE )
{
}

uint64_t SIRHandleHeap::getRequiredMetadataBytes( uint32_t nMaxHandles )
{
	return nMaxHandles == 0 ? 0
	                        : ( ( uint64_t )( nMaxHandles ) ) *
	                                  ( sizeof( sirHandleDesc_t ) + sizeof( sirHandleCold_t ) + sizeof( sirHandleHeapExtent_t ) ) +
	                              sizeof( sirHandleHeapExtent_t ) + 32;
}

bool SIRHandleHeap::init( SIRMemorySegment *pSegment, uint32_t nMaxHandles )
{
	if ( m_pSegment != nullptr || pSegment == nullptr || nMaxHandles == 0 || nMaxHandles == UINT32_MAX ||
	     pSegment->getReservedBytes() > UINT32_MAX - 15 ) {
		return false;
	}
	SIRMemoryManager *pManager = pSegment->getManager();
	const uint64_t nCapacity = ( ( uint64_t )( nMaxHandles ) );
	sirHandleDesc_t *pHandles =
	    ( ( sirHandleDesc_t * )( pManager->allocateMetadata( nCapacity * sizeof( sirHandleDesc_t ), alignof( sirHandleDesc_t ) ) ) );
	sirHandleCold_t *pCold =
	    ( ( sirHandleCold_t * )( pManager->allocateMetadata( nCapacity * sizeof( sirHandleCold_t ), alignof( sirHandleCold_t ) ) ) );
	sirHandleHeapExtent_t *pExtents = ( ( sirHandleHeapExtent_t * )( pManager->allocateMetadata(
	    ( nCapacity + 1 ) * sizeof( sirHandleHeapExtent_t ), alignof( sirHandleHeapExtent_t ) ) ) );
	if ( pHandles == nullptr || pCold == nullptr || pExtents == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < nMaxHandles; ++i ) {
		pHandles[i].nGeneration = 1;
		pHandles[i].nStatePin = 0;
	}
	m_pSegment = pSegment;
	m_pBase = ( ( uint8_t * )( pSegment->getBase() ) );
	m_pHandles = pHandles;
	m_pCold = pCold;
	m_pExtents = pExtents;
	m_nMaxHandles = nMaxHandles;
	m_nExtentCapacity = nMaxHandles + 1;
	m_nCapacity = ( ( uint32_t )( pSegment->getReservedBytes() ) );
	return true;
}

sirHandleDesc_t *SIRHandleHeap::validate( sirMemoryHandle_t handle ) const
{
	if ( m_pHandles == nullptr || handle.nIndex >= m_nNextHandle || handle.nGeneration == 0 ) {
		return nullptr;
	}
	sirHandleDesc_t *pDesc = m_pHandles + handle.nIndex;
	return ( pDesc->nStatePin & 1 ) != 0 && pDesc->nGeneration == handle.nGeneration ? pDesc : nullptr;
}

bool SIRHandleHeap::acquireHandle( uint32_t *pIndex )
{
	if ( m_nFreeHandleHead != SIR_HANDLE_NONE ) {
		*pIndex = m_nFreeHandleHead;
		m_nFreeHandleHead = m_pCold[*pIndex].nNextFree;
		return true;
	}
	if ( m_nNextHandle >= m_nMaxHandles ) {
		return false;
	}
	*pIndex = m_nNextHandle++;
	return true;
}

void SIRHandleHeap::recycleHandle( uint32_t nIndex )
{
	sirHandleDesc_t& desc = m_pHandles[nIndex];
	desc.nStatePin = 0;
	++desc.nGeneration;
	if ( desc.nGeneration != 0 ) {
		m_pCold[nIndex].nNextFree = m_nFreeHandleHead;
		m_nFreeHandleHead = nIndex;
	}
}

uint32_t SIRHandleHeap::acquireExtent( void )
{
	if ( m_nFreeExtentHead != SIR_HANDLE_NONE ) {
		const uint32_t nIndex = m_nFreeExtentHead;
		m_nFreeExtentHead = m_pExtents[nIndex].address.nLeft;
		return nIndex;
	}
	if ( m_nNextExtent >= m_nExtentCapacity ) {
		return SIR_HANDLE_NONE;
	}
	return m_nNextExtent++;
}

void SIRHandleHeap::recycleExtent( uint32_t nIndex )
{
	m_pExtents[nIndex].address.nLeft = m_nFreeExtentHead;
	m_nFreeExtentHead = nIndex;
}

template <bool bSizeTree>
sirTreeLinks_t& SIRHandleHeap::getLinks( uint32_t nIndex )
{
	return bSizeTree ? m_pExtents[nIndex].size : m_pExtents[nIndex].address;
}

template <bool bSizeTree>
const sirTreeLinks_t& SIRHandleHeap::getLinks( uint32_t nIndex ) const
{
	return bSizeTree ? m_pExtents[nIndex].size : m_pExtents[nIndex].address;
}

template <bool bSizeTree>
uint32_t& SIRHandleHeap::root( void )
{
	return bSizeTree ? m_nSizeRoot : m_nAddressRoot;
}

template <bool bSizeTree>
uint32_t SIRHandleHeap::rootValue( void ) const
{
	return bSizeTree ? m_nSizeRoot : m_nAddressRoot;
}

template <bool bSizeTree>
bool SIRHandleHeap::treeLess( uint32_t nLeft, uint32_t nRight ) const
{
	const sirHandleHeapExtent_t& left = m_pExtents[nLeft];
	const sirHandleHeapExtent_t& right = m_pExtents[nRight];
	if ( bSizeTree && left.nBytes != right.nBytes ) {
		return left.nBytes < right.nBytes;
	}
	return left.nOffset < right.nOffset;
}

template <bool bSizeTree>
uint8_t SIRHandleHeap::treeHeight( uint32_t nNode ) const
{
	return nNode == SIR_HANDLE_NONE ? 0 : getLinks<bSizeTree>( nNode ).nHeight;
}

template <bool bSizeTree>
void SIRHandleHeap::updateHeight( uint32_t nNode )
{
	sirTreeLinks_t& links = getLinks<bSizeTree>( nNode );
	const uint8_t nLeft = treeHeight<bSizeTree>( links.nLeft );
	const uint8_t nRight = treeHeight<bSizeTree>( links.nRight );
	links.nHeight = ( ( uint8_t )( ( nLeft > nRight ? nLeft : nRight ) + 1 ) );
}

template <bool bSizeTree>
void SIRHandleHeap::replaceNode( uint32_t nNode, uint32_t nReplacement )
{
	const uint32_t nParent = getLinks<bSizeTree>( nNode ).nParent;
	if ( nParent == SIR_HANDLE_NONE ) {
		root<bSizeTree>() = nReplacement;
	} else if ( getLinks<bSizeTree>( nParent ).nLeft == nNode ) {
		getLinks<bSizeTree>( nParent ).nLeft = nReplacement;
	} else {
		getLinks<bSizeTree>( nParent ).nRight = nReplacement;
	}
	if ( nReplacement != SIR_HANDLE_NONE ) {
		getLinks<bSizeTree>( nReplacement ).nParent = nParent;
	}
}

template <bool bSizeTree>
uint32_t SIRHandleHeap::rotateLeft( uint32_t nNode )
{
	const uint32_t nNewRoot = getLinks<bSizeTree>( nNode ).nRight;
	const uint32_t nMiddle = getLinks<bSizeTree>( nNewRoot ).nLeft;
	replaceNode<bSizeTree>( nNode, nNewRoot );
	getLinks<bSizeTree>( nNewRoot ).nLeft = nNode;
	getLinks<bSizeTree>( nNode ).nParent = nNewRoot;
	getLinks<bSizeTree>( nNode ).nRight = nMiddle;
	if ( nMiddle != SIR_HANDLE_NONE ) {
		getLinks<bSizeTree>( nMiddle ).nParent = nNode;
	}
	updateHeight<bSizeTree>( nNode );
	updateHeight<bSizeTree>( nNewRoot );
	return nNewRoot;
}

template <bool bSizeTree>
uint32_t SIRHandleHeap::rotateRight( uint32_t nNode )
{
	const uint32_t nNewRoot = getLinks<bSizeTree>( nNode ).nLeft;
	const uint32_t nMiddle = getLinks<bSizeTree>( nNewRoot ).nRight;
	replaceNode<bSizeTree>( nNode, nNewRoot );
	getLinks<bSizeTree>( nNewRoot ).nRight = nNode;
	getLinks<bSizeTree>( nNode ).nParent = nNewRoot;
	getLinks<bSizeTree>( nNode ).nLeft = nMiddle;
	if ( nMiddle != SIR_HANDLE_NONE ) {
		getLinks<bSizeTree>( nMiddle ).nParent = nNode;
	}
	updateHeight<bSizeTree>( nNode );
	updateHeight<bSizeTree>( nNewRoot );
	return nNewRoot;
}

template <bool bSizeTree>
void SIRHandleHeap::rebalance( uint32_t nNode )
{
	while ( nNode != SIR_HANDLE_NONE ) {
		const uint8_t nOldHeight = treeHeight<bSizeTree>( nNode );
		updateHeight<bSizeTree>( nNode );
		const sirTreeLinks_t& links = getLinks<bSizeTree>( nNode );
		const int nBalance = ( ( int )( treeHeight<bSizeTree>( links.nLeft ) ) ) - ( ( int )( treeHeight<bSizeTree>( links.nRight ) ) );
		if ( nBalance > 1 ) {
			const sirTreeLinks_t& left = getLinks<bSizeTree>( links.nLeft );
			if ( treeHeight<bSizeTree>( left.nRight ) > treeHeight<bSizeTree>( left.nLeft ) ) {
				rotateLeft<bSizeTree>( links.nLeft );
			}
			nNode = rotateRight<bSizeTree>( nNode );
		} else if ( nBalance < -1 ) {
			const sirTreeLinks_t& right = getLinks<bSizeTree>( links.nRight );
			if ( treeHeight<bSizeTree>( right.nLeft ) > treeHeight<bSizeTree>( right.nRight ) ) {
				rotateRight<bSizeTree>( links.nRight );
			}
			nNode = rotateLeft<bSizeTree>( nNode );
		}
		if ( nBalance >= -1 && nBalance <= 1 && treeHeight<bSizeTree>( nNode ) == nOldHeight ) {
			return;
		}
		nNode = getLinks<bSizeTree>( nNode ).nParent;
	}
}

template <bool bSizeTree>
void SIRHandleHeap::treeInsert( uint32_t nIndex )
{
	sirTreeLinks_t& links = getLinks<bSizeTree>( nIndex );
	links.nLeft = SIR_HANDLE_NONE;
	links.nRight = SIR_HANDLE_NONE;
	links.nParent = SIR_HANDLE_NONE;
	links.nHeight = 1;
	uint32_t nParent = SIR_HANDLE_NONE;
	uint32_t nCurrent = rootValue<bSizeTree>();
	while ( nCurrent != SIR_HANDLE_NONE ) {
		nParent = nCurrent;
		nCurrent = treeLess<bSizeTree>( nIndex, nCurrent ) ? getLinks<bSizeTree>( nCurrent ).nLeft : getLinks<bSizeTree>( nCurrent ).nRight;
	}
	if ( nParent == SIR_HANDLE_NONE ) {
		root<bSizeTree>() = nIndex;
		return;
	}
	links.nParent = nParent;
	if ( treeLess<bSizeTree>( nIndex, nParent ) ) {
		getLinks<bSizeTree>( nParent ).nLeft = nIndex;
	} else {
		getLinks<bSizeTree>( nParent ).nRight = nIndex;
	}
	rebalance<bSizeTree>( nParent );
}

template <bool bSizeTree>
uint32_t SIRHandleHeap::treeFirst( uint32_t nNode ) const
{
	while ( nNode != SIR_HANDLE_NONE && getLinks<bSizeTree>( nNode ).nLeft != SIR_HANDLE_NONE ) {
		nNode = getLinks<bSizeTree>( nNode ).nLeft;
	}
	return nNode;
}

template <bool bSizeTree>
uint32_t SIRHandleHeap::treeLast( uint32_t nNode ) const
{
	while ( nNode != SIR_HANDLE_NONE && getLinks<bSizeTree>( nNode ).nRight != SIR_HANDLE_NONE ) {
		nNode = getLinks<bSizeTree>( nNode ).nRight;
	}
	return nNode;
}

template <bool bSizeTree>
uint32_t SIRHandleHeap::treeNext( uint32_t nIndex ) const
{
	if ( getLinks<bSizeTree>( nIndex ).nRight != SIR_HANDLE_NONE ) {
		return treeFirst<bSizeTree>( getLinks<bSizeTree>( nIndex ).nRight );
	}
	uint32_t nParent = getLinks<bSizeTree>( nIndex ).nParent;
	while ( nParent != SIR_HANDLE_NONE && getLinks<bSizeTree>( nParent ).nRight == nIndex ) {
		nIndex = nParent;
		nParent = getLinks<bSizeTree>( nIndex ).nParent;
	}
	return nParent;
}

template <bool bSizeTree>
void SIRHandleHeap::treeRemove( uint32_t nIndex )
{
	const sirTreeLinks_t links = getLinks<bSizeTree>( nIndex );
	if ( links.nLeft == SIR_HANDLE_NONE || links.nRight == SIR_HANDLE_NONE ) {
		const uint32_t nChild = links.nLeft != SIR_HANDLE_NONE ? links.nLeft : links.nRight;
		replaceNode<bSizeTree>( nIndex, nChild );
		rebalance<bSizeTree>( links.nParent );
		return;
	}

	const uint32_t nSuccessor = treeFirst<bSizeTree>( links.nRight );
	const uint32_t nSuccessorParent = getLinks<bSizeTree>( nSuccessor ).nParent;
	uint32_t nRebalanceFrom = nSuccessor;
	if ( nSuccessorParent != nIndex ) {
		const uint32_t nSuccessorRight = getLinks<bSizeTree>( nSuccessor ).nRight;
		replaceNode<bSizeTree>( nSuccessor, nSuccessorRight );
		getLinks<bSizeTree>( nSuccessor ).nRight = links.nRight;
		getLinks<bSizeTree>( links.nRight ).nParent = nSuccessor;
		nRebalanceFrom = nSuccessorParent;
	}
	replaceNode<bSizeTree>( nIndex, nSuccessor );
	getLinks<bSizeTree>( nSuccessor ).nLeft = links.nLeft;
	getLinks<bSizeTree>( links.nLeft ).nParent = nSuccessor;
	getLinks<bSizeTree>( nSuccessor ).nHeight = links.nHeight;
	rebalance<bSizeTree>( nRebalanceFrom );
}

void SIRHandleHeap::removeExtent( uint32_t nIndex )
{
	treeRemove<true>( nIndex );
	treeRemove<false>( nIndex );
}

bool SIRHandleHeap::insertExtent( uint32_t nOffset, uint32_t nBytes )
{
	if ( nBytes == 0 ) {
		return true;
	}
	uint32_t nPrevious = SIR_HANDLE_NONE;
	uint32_t nNext = SIR_HANDLE_NONE;
	uint32_t nCurrent = m_nAddressRoot;
	while ( nCurrent != SIR_HANDLE_NONE ) {
		if ( m_pExtents[nCurrent].nOffset < nOffset ) {
			nPrevious = nCurrent;
			nCurrent = getLinks<false>( nCurrent ).nRight;
		} else {
			nNext = nCurrent;
			nCurrent = getLinks<false>( nCurrent ).nLeft;
		}
	}

	const bool bMergePrevious =
	    nPrevious != SIR_HANDLE_NONE && ( ( uint64_t )( m_pExtents[nPrevious].nOffset ) ) + m_pExtents[nPrevious].nBytes == nOffset;
	const bool bMergeNext = nNext != SIR_HANDLE_NONE && ( ( uint64_t )( nOffset ) ) + nBytes == m_pExtents[nNext].nOffset;
	if ( bMergePrevious ) {
		// Offset of the left extent stays unchanged. Only its size index
		// must be updated; retain its existing address-tree position.
		treeRemove<true>( nPrevious );
		m_pExtents[nPrevious].nBytes += nBytes;
		if ( bMergeNext ) {
			removeExtent( nNext );
			m_pExtents[nPrevious].nBytes += m_pExtents[nNext].nBytes;
			recycleExtent( nNext );
		}
		treeInsert<true>( nPrevious );
		return true;
	}
	if ( bMergeNext ) {
		removeExtent( nNext );
		m_pExtents[nNext].nOffset = nOffset;
		m_pExtents[nNext].nBytes += nBytes;
		treeInsert<false>( nNext );
		treeInsert<true>( nNext );
		return true;
	}
	const uint32_t nNew = acquireExtent();
	if ( nNew == SIR_HANDLE_NONE ) {
		return false;
	}
	m_pExtents[nNew].nOffset = nOffset;
	m_pExtents[nNew].nBytes = nBytes;
	treeInsert<false>( nNew );
	treeInsert<true>( nNew );
	return true;
}

uint32_t SIRHandleHeap::findFittingExtent( uint32_t nBytes, uint32_t nAlignment, uint32_t *pUserOffset, uint32_t *pBlockBytes ) const
{
	uint64_t nMinimum = 0;
	if ( !SIR_IS_POWER_OF_TWO( 16 ) || ( uint64_t )( 16 ) > UINTPTR_MAX ||
	     ( uint64_t )( nBytes ) > UINTPTR_MAX - ( ( uint64_t )( 16 ) - 1 ) ) {
		return SIR_HANDLE_NONE;
	}
	nMinimum = SIR_ALIGN_UP( nBytes, 16 );
	if ( m_nSizeRoot == SIR_HANDLE_NONE ) {
		return SIR_HANDLE_NONE;
	}
	uint32_t nCandidate = SIR_HANDLE_NONE;
	uint32_t nCurrent = m_nSizeRoot;
	while ( nCurrent != SIR_HANDLE_NONE ) {
		if ( m_pExtents[nCurrent].nBytes >= nMinimum ) {
			nCandidate = nCurrent;
			nCurrent = getLinks<true>( nCurrent ).nLeft;
		} else {
			nCurrent = getLinks<true>( nCurrent ).nRight;
		}
	}
	for ( ; nCandidate != SIR_HANDLE_NONE; nCandidate = treeNext<true>( nCandidate ) ) {
		uint32_t nUser = 0;
		uint32_t nUsed = 0;
		if ( prepareBlock( m_pExtents[nCandidate].nOffset, nBytes, nAlignment, &nUser, &nUsed ) &&
		     nUsed <= m_pExtents[nCandidate].nBytes ) {
			*pUserOffset = nUser;
			*pBlockBytes = nUsed;
			return nCandidate;
		}
	}
	return SIR_HANDLE_NONE;
}

void SIRHandleHeap::trimLogicalTail( void )
{
	uint32_t nLast = treeLast<false>( m_nAddressRoot );
	while ( nLast != SIR_HANDLE_NONE && ( ( uint64_t )( m_pExtents[nLast].nOffset ) ) + m_pExtents[nLast].nBytes == m_nHighWater ) {
		m_nHighWater = m_pExtents[nLast].nOffset;
		removeExtent( nLast );
		recycleExtent( nLast );
		nLast = treeLast<false>( m_nAddressRoot );
	}
}

bool SIRHandleHeap::prepareBlock( uint32_t nOffset, uint32_t nBytes, uint32_t nAlignment, uint32_t *pUserOffset,
                                  uint32_t *pBlockBytes ) const
{
	uint64_t nAligned = 0;
	if ( !SIR_IS_POWER_OF_TWO( nAlignment ) || ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
	     ( uint64_t )( ( ( uintptr_t )( m_pBase ) ) + nOffset ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment )-1 ) ) {
		return false;
	}
	nAligned = SIR_ALIGN_UP( ( ( uintptr_t )( m_pBase ) ) + nOffset, nAlignment );
	const uint64_t nUserOffset = nAligned - ( ( uintptr_t )( m_pBase ) );
	uint64_t nEnd = 0;
	if ( !SIR_IS_POWER_OF_TWO( 16 ) || ( uint64_t )( 16 ) > UINTPTR_MAX ||
	     ( uint64_t )( nUserOffset + nBytes ) > UINTPTR_MAX - ( ( uint64_t )( 16 ) - 1 ) ) {
		return false;
	}
	nEnd = SIR_ALIGN_UP( nUserOffset + nBytes, 16 );
	if ( nEnd > m_nCapacity ) {
		return false;
	}
	*pUserOffset = ( ( uint32_t )( nUserOffset ) );
	*pBlockBytes = ( ( uint32_t )( nEnd - nOffset ) );
	return true;
}

void *SIRHandleHeap::alloc( uint32_t nBytes, uint32_t nAlignment, sirMemoryHandle_t *pHandle )
{
	if ( m_pSegment == nullptr || nBytes == 0 || pHandle == nullptr || !SIR_IS_POWER_OF_TWO( nAlignment ) ||
	     nAlignment > m_pSegment->getPageSize() || ( m_nFreeHandleHead == SIR_HANDLE_NONE && m_nNextHandle == m_nMaxHandles ) ) {
		return nullptr;
	}
	uint32_t nBlockOffset = m_nHighWater;
	uint32_t nUserOffset = 0;
	uint32_t nBlockBytes = 0;
	const uint32_t nChosenExtent = findFittingExtent( nBytes, nAlignment, &nUserOffset, &nBlockBytes );
	if ( nChosenExtent != SIR_HANDLE_NONE ) {
		nBlockOffset = m_pExtents[nChosenExtent].nOffset;
	}
	if ( nChosenExtent == SIR_HANDLE_NONE ) {
		if ( !prepareBlock( nBlockOffset, nBytes, nAlignment, &nUserOffset, &nBlockBytes ) ) {
			return nullptr;
		}
		const uint64_t nEnd = ( ( uint64_t )( nBlockOffset ) ) + nBlockBytes;
		if ( nEnd > m_nCommitted ) {
			uint64_t nTarget = 0;
			if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) || ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
			     ( uint64_t )( nEnd ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) ) {
				return nullptr;
			}
			nTarget = SIR_ALIGN_UP( nEnd, m_pSegment->getPageSize() );
			if ( nTarget > m_nCapacity || !m_pSegment->commit( m_nCommitted, nTarget - m_nCommitted ) ) {
				return nullptr;
			}
			m_nCommitted = ( ( uint32_t )( nTarget ) );
		}
		m_nHighWater = ( ( uint32_t )( nEnd ) );
	} else {
		sirHandleHeapExtent_t& extent = m_pExtents[nChosenExtent];
		removeExtent( nChosenExtent );
		if ( nBlockBytes == extent.nBytes ) {
			recycleExtent( nChosenExtent );
		} else {
			extent.nOffset += nBlockBytes;
			extent.nBytes -= nBlockBytes;
			treeInsert<false>( nChosenExtent );
			treeInsert<true>( nChosenExtent );
		}
	}
	uint32_t nHandleIndex = SIR_HANDLE_NONE;
	if ( !acquireHandle( &nHandleIndex ) ) {
		return nullptr;
	}
	sirHandleDesc_t& desc = m_pHandles[nHandleIndex];
	desc.nBlockOffset = nBlockOffset;
	desc.nUserOffset = nUserOffset;
	sirHandleCold_t& cold = m_pCold[nHandleIndex];
	cold.nBlockBytes = nBlockBytes;
	cold.nPayloadBytes = nBytes;
	cold.nAlignment = nAlignment;
	desc.nStatePin = 1;
	++m_nLiveCount;
	pHandle->nIndex = nHandleIndex;
	pHandle->nGeneration = desc.nGeneration;
	return m_pBase + nUserOffset;
}

void *SIRHandleHeap::resolve( sirMemoryHandle_t handle ) const
{
	const sirHandleDesc_t *pDesc = validate( handle );
	return pDesc == nullptr ? nullptr : m_pBase + pDesc->nUserOffset;
}

void *SIRHandleHeap::pin( sirMemoryHandle_t handle )
{
	sirHandleDesc_t *pDesc = validate( handle );
	if ( pDesc == nullptr || ( pDesc->nStatePin >> 1 ) == UINT16_MAX ) {
		return nullptr;
	}
	pDesc->nStatePin += 2;
	return m_pBase + pDesc->nUserOffset;
}

bool SIRHandleHeap::unpin( sirMemoryHandle_t handle )
{
	sirHandleDesc_t *pDesc = validate( handle );
	if ( pDesc == nullptr || ( pDesc->nStatePin >> 1 ) == 0 ) {
		return false;
	}
	pDesc->nStatePin -= 2;
	return true;
}

bool SIRHandleHeap::free( sirMemoryHandle_t handle )
{
	sirHandleDesc_t *pDesc = validate( handle );
	if ( pDesc == nullptr || ( pDesc->nStatePin >> 1 ) != 0 ) {
		return false;
	}
	const uint32_t nOffset = pDesc->nBlockOffset;
	const uint32_t nBytes = m_pCold[handle.nIndex].nBlockBytes;
	if ( ( ( uint64_t )( nOffset ) ) + nBytes == m_nHighWater ) {
		// Tail blocks require no AVL insertion: shrink high water directly,
		// then consume preceding free extents in address order.
		m_nHighWater = nOffset;
	} else if ( !insertExtent( nOffset, nBytes ) ) {
		return false;
	}
	recycleHandle( handle.nIndex );
	--m_nLiveCount;
	trimLogicalTail();
	return true;
}

bool SIRHandleHeap::less( uint32_t nLeft, uint32_t nRight ) const
{
	return m_pHandles[nLeft].nBlockOffset < m_pHandles[nRight].nBlockOffset;
}

uint32_t SIRHandleHeap::mergeSortedRuns( uint32_t nLeft, uint32_t nRight )
{
	uint32_t nHead = SIR_HANDLE_NONE;
	uint32_t *pTail = &nHead;
	while ( nLeft != SIR_HANDLE_NONE && nRight != SIR_HANDLE_NONE ) {
		const bool bTakeLeft = !less( nRight, nLeft );
		const uint32_t nPicked = bTakeLeft ? nLeft : nRight;
		if ( bTakeLeft ) {
			nLeft = m_pCold[nLeft].nNextFree;
		} else {
			nRight = m_pCold[nRight].nNextFree;
		}
		*pTail = nPicked;
		pTail = &m_pCold[nPicked].nNextFree;
	}
	*pTail = nLeft != SIR_HANDLE_NONE ? nLeft : nRight;
	return nHead;
}

uint32_t SIRHandleHeap::sortActive( void )
{
	// Bottom-up intrusive mergesort: the cold next-free field is unused for
	// live handles. This removes all permanent sort-index buffers while
	// preserving stable public handle indices and pin constraints.
	uint32_t nRuns[33];
	for ( uint32_t i = 0; i < 33; ++i ) {
		nRuns[i] = SIR_HANDLE_NONE;
	}
	for ( uint32_t i = 0; i < m_nNextHandle; ++i ) {
		if ( ( m_pHandles[i].nStatePin & 1 ) == 0 ) {
			continue;
		}
		m_pCold[i].nNextFree = SIR_HANDLE_NONE;
		uint32_t nHead = i;
		uint32_t nLevel = 0;
		while ( nLevel < 32 && nRuns[nLevel] != SIR_HANDLE_NONE ) {
			nHead = mergeSortedRuns( nRuns[nLevel], nHead );
			nRuns[nLevel++] = SIR_HANDLE_NONE;
		}
		nRuns[nLevel] = nHead;
	}
	uint32_t nHead = SIR_HANDLE_NONE;
	for ( uint32_t i = 0; i < 33; ++i ) {
		if ( nRuns[i] != SIR_HANDLE_NONE ) {
			nHead = mergeSortedRuns( nRuns[i], nHead );
		}
	}
	return nHead;
}

uint64_t SIRHandleHeap::getCompactionPayloadUpperBound( void ) const
{
	if ( m_pSegment == nullptr ) {
		return 0;
	}
	uint64_t nBytes = 0;
	for ( uint32_t i = 0; i < m_nNextHandle; ++i ) {
		if ( ( m_pHandles[i].nStatePin & 1 ) != 0 && ( m_pHandles[i].nStatePin >> 1 ) == 0 ) {
			nBytes += m_pCold[i].nPayloadBytes;
		}
	}
	return nBytes;
}

bool SIRHandleHeap::compactWithinBudget( uint64_t nMaximumPayloadBytes )
{
	return m_pSegment != nullptr && getCompactionPayloadUpperBound() <= nMaximumPayloadBytes && compact();
}

bool SIRHandleHeap::compact( void )
{
	if ( m_pSegment == nullptr ) {
		return false;
	}
	const uint32_t nSortHead = sortActive();
	// Simulate the exact placement algorithm before altering any payload or
	// live descriptor. The rebuild cannot partially consume extent metadata.
	uint32_t nPreflightCursor = 0;
	uint32_t nPreflightGaps = 0;
	uint32_t nPreviousEnd = 0;
	for ( uint32_t i = nSortHead; i != SIR_HANDLE_NONE; i = m_pCold[i].nNextFree ) {
		const sirHandleDesc_t& desc = m_pHandles[i];
		const sirHandleCold_t& cold = m_pCold[i];
		const uint64_t nOldEnd = ( ( uint64_t )( desc.nBlockOffset ) ) + cold.nBlockBytes;
		if ( desc.nBlockOffset < nPreviousEnd || nOldEnd > m_nHighWater || desc.nUserOffset < desc.nBlockOffset ||
		     ( ( uint64_t )( desc.nUserOffset ) ) + cold.nPayloadBytes > nOldEnd ) {
			return false;
		}
		nPreviousEnd = ( ( uint32_t )( nOldEnd ) );
		if ( ( desc.nStatePin >> 1 ) == 0 ) {
			uint32_t nUser = 0;
			uint32_t nBlock = 0;
			if ( prepareBlock( nPreflightCursor, cold.nPayloadBytes, cold.nAlignment, &nUser, &nBlock ) &&
			     ( ( uint64_t )( nPreflightCursor ) ) + nBlock <= nOldEnd ) {
				nPreflightCursor += nBlock;
				continue;
			}
		}
		if ( desc.nBlockOffset > nPreflightCursor ) {
			++nPreflightGaps;
		}
		nPreflightCursor = ( ( uint32_t )( nOldEnd ) );
	}
	if ( nPreflightGaps > m_nExtentCapacity ) {
		return false;
	}

	uint32_t nCursor = 0;
	for ( uint32_t i = nSortHead; i != SIR_HANDLE_NONE; i = m_pCold[i].nNextFree ) {
		sirHandleDesc_t& desc = m_pHandles[i];
		sirHandleCold_t& cold = m_pCold[i];
		const uint32_t nOldEnd = desc.nBlockOffset + cold.nBlockBytes;
		if ( ( desc.nStatePin >> 1 ) != 0 ) {
			nCursor = nOldEnd;
			continue;
		}
		uint32_t nNewUser = 0;
		uint32_t nNewBytes = 0;
		if ( !prepareBlock( nCursor, cold.nPayloadBytes, cold.nAlignment, &nNewUser, &nNewBytes ) ||
		     ( ( uint64_t )( nCursor ) ) + nNewBytes > nOldEnd ) {
			nCursor = nOldEnd;
			continue;
		}
		if ( nNewUser != desc.nUserOffset ) {
			std::memmove( m_pBase + nNewUser, m_pBase + desc.nUserOffset, cold.nPayloadBytes );
		}
		desc.nBlockOffset = nCursor;
		cold.nBlockBytes = nNewBytes;
		desc.nUserOffset = nNewUser;
		nCursor += nNewBytes;
	}
	// Cold rebuild of both extent indexes after moving payloads.
	m_nNextExtent = 0;
	m_nFreeExtentHead = SIR_HANDLE_NONE;
	m_nAddressRoot = SIR_HANDLE_NONE;
	m_nSizeRoot = SIR_HANDLE_NONE;
	uint32_t nEnd = 0;
	for ( uint32_t i = nSortHead; i != SIR_HANDLE_NONE; i = m_pCold[i].nNextFree ) {
		const sirHandleDesc_t& desc = m_pHandles[i];
		const sirHandleCold_t& cold = m_pCold[i];
		if ( desc.nBlockOffset > nEnd && !insertExtent( nEnd, desc.nBlockOffset - nEnd ) ) {
			return false;
		}
		nEnd = desc.nBlockOffset + cold.nBlockBytes;
	}
	m_nHighWater = nEnd;
	return true;
}

bool SIRHandleHeap::trimUnusedPages( void )
{
	if ( m_pSegment == nullptr ) {
		return false;
	}
	uint64_t nTarget = 0;
	if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) || ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
	     ( uint64_t )( m_nHighWater ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) ) {
		return false;
	}
	nTarget = SIR_ALIGN_UP( m_nHighWater, m_pSegment->getPageSize() );
	if ( nTarget == m_nCommitted ) {
		return true;
	}
	if ( !m_pSegment->decommit( nTarget, m_nCommitted - nTarget ) ) {
		return false;
	}
	m_nCommitted = ( ( uint32_t )( nTarget ) );
	return true;
}

bool SIRHandleHeap::shutdown( void )
{
	if ( m_pSegment == nullptr || m_nLiveCount != 0 || !trimUnusedPages() ) {
		return false;
	}
	m_pSegment = nullptr;
	m_pBase = nullptr;
	m_pHandles = nullptr;
	m_pCold = nullptr;
	m_pExtents = nullptr;
	return true;
}

uint32_t SIRHandleHeap::getLiveCount( void ) const
{
	return m_nLiveCount;
}
uint32_t SIRHandleHeap::getCommittedBytes( void ) const
{
	return m_nCommitted;
}
uint32_t SIRHandleHeap::getLargestFreeRange( void ) const
{
	uint32_t nLargest = m_nCapacity - m_nHighWater;
	const uint32_t nExtent = treeLast<true>( m_nSizeRoot );
	if ( nExtent != SIR_HANDLE_NONE && m_pExtents[nExtent].nBytes > nLargest ) {
		nLargest = m_pExtents[nExtent].nBytes;
	}
	return nLargest;
}
