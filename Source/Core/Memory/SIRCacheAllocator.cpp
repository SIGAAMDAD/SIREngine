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

#include <Core/Memory/Allocators/SIRCacheAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <new>

SIRCacheAllocator::SIRCacheAllocator( void )
    : m_Buddy(), m_pSegment( nullptr ), m_pEntries( nullptr ), m_pActiveIndices( nullptr ),
      m_pBase( nullptr ), m_nEntryCount( 0 ), m_nPageSize( 0 ), m_nFreshIndex( 0 ),
      m_nFreeHead( SIR_CACHE_NONE ), m_nClockHand( 0 ),
      m_nLiveCount( 0 ), m_nEvicted( 0 )
{
}

uint64_t SIRCacheAllocator::getRequiredMetadataBytes( uint64_t nSegmentBytes,
                                                      uint64_t nMinimumBlockBytes,
                                                      uint32_t nMaxEntries )
{
	if ( nSegmentBytes > UINT32_MAX || nMaxEntries == 0 ) {
		return 0;
	}
	const uint64_t nBuddy = SIRBuddyAllocator::getRequiredMetadataBytes(
	    nSegmentBytes, nMinimumBlockBytes );
	if ( nBuddy == 0 ) {
		return 0;
	}
	// Entries and dense CLOCK indices live in one permanent metadata allocation.
	return nBuddy + ( (uint64_t)( nMaxEntries ) ) * ( sizeof( sirCacheDesc_t ) + sizeof( uint32_t ) ) + 16;
}

bool SIRCacheAllocator::init( SIRMemorySegment *pSegment, uint32_t nMaxEntries,
                              uint64_t nMinimumBlockBytes )
{
	if ( m_pEntries != nullptr || pSegment == nullptr || nMaxEntries == 0 ||
	     pSegment->getReservedBytes() > UINT32_MAX ) {
		return false;
	}
	SIRMemoryManager *pManager = pSegment->getManager();
	if ( pManager == nullptr ) {
		return false;
	}

	const uint64_t nDescriptorBytes = ( (uint64_t)( nMaxEntries ) ) *
	                                  sizeof( sirCacheDesc_t );
	const uint64_t nTotalBytes = nDescriptorBytes +
	                             ( (uint64_t)( nMaxEntries ) ) * sizeof( uint32_t );
	uint8_t *pMetadata = ( (uint8_t *)( pManager->allocateMetadata( nTotalBytes, alignof( uint32_t ) ) ) );
	if ( pMetadata == nullptr || !m_Buddy.init( pSegment, nMinimumBlockBytes ) ) {
		return false;
	}

	m_pSegment = pSegment;
	m_pEntries = ( (sirCacheDesc_t *)( pMetadata ) );
	m_pActiveIndices = ( (uint32_t *)( pMetadata + nDescriptorBytes ) );
	m_pBase = ( (uint8_t *)( pSegment->getBase() ) );
	m_nEntryCount = nMaxEntries;
	m_nPageSize = ( (uint32_t)( pSegment->getPageSize() ) );
	for ( uint32_t i = 0; i < nMaxEntries; ++i ) {
		m_pEntries[ i ].nGeneration = 1;
	}
	return true;
}

uint8_t *SIRCacheAllocator::entryAddress( const sirCacheDesc_t& desc ) const
{
	return m_pBase + desc.nOffset;
}

sirCacheDesc_t *SIRCacheAllocator::validate( sirCacheHandle_t handle )
{
	if ( m_pEntries == nullptr || handle.nIndex >= m_nFreshIndex ||
	     handle.nGeneration == 0 ) {
		return nullptr;
	}
	sirCacheDesc_t *pDesc = m_pEntries + handle.nIndex;
	return ( pDesc->nFlags & SIR_CACHE_LIVE ) != 0 &&
	               pDesc->nGeneration == handle.nGeneration
	           ? pDesc
	           : nullptr;
}

bool SIRCacheAllocator::acquireIndex( uint32_t *pIndex )
{
	if ( m_nFreeHead != SIR_CACHE_NONE ) {
		*pIndex = m_nFreeHead;
		m_nFreeHead = m_pEntries[ m_nFreeHead ].nNextFree;
		return true;
	}
	if ( m_nFreshIndex == m_nEntryCount ) {
		return false;
	}
	*pIndex = m_nFreshIndex++;
	return true;
}

void SIRCacheAllocator::returnIndex( uint32_t nIndex )
{
	sirCacheDesc_t& desc = m_pEntries[ nIndex ];
	desc.nNextFree = m_nFreeHead;
	m_nFreeHead = nIndex;
}

bool SIRCacheAllocator::evictIndex( uint32_t nIndex, bool bEvicted )
{
	sirCacheDesc_t& desc = m_pEntries[ nIndex ];
	if ( ( desc.nFlags & SIR_CACHE_LIVE ) == 0 || desc.nPinCount != 0 ||
	     !m_Buddy.free( entryAddress( desc ) ) ) {
		return false;
	}

	const uint32_t nSlot = desc.nActiveSlot;
	const uint32_t nLastIndex = m_pActiveIndices[ m_nLiveCount - 1 ];
	if ( nLastIndex != nIndex ) {
		m_pActiveIndices[ nSlot ] = nLastIndex;
		m_pEntries[ nLastIndex ].nActiveSlot = nSlot;
	}
	--m_nLiveCount;
	if ( m_nClockHand >= m_nLiveCount ) {
		m_nClockHand = 0;
	}

	// The free-list uses nNextFree in place of nBytes. Offset and pin count
	// are inaccessible until the entry is fully initialized on reuse.
	desc.nFlags = 0;
	++desc.nGeneration;
	if ( desc.nGeneration != 0 ) {
		returnIndex( nIndex );
	}
	if ( bEvicted ) {
		++m_nEvicted;
	}
	return true;
}

bool SIRCacheAllocator::evictOne( void )
{
	if ( m_pEntries == nullptr || m_nLiveCount == 0 ) {
		return false;
	}
	// One revolution suffices. Remember the first entry whose reference bit
	// was cleared: it is exactly the entry a second CLOCK revolution would
	// evict if the first revolution finds no unreferenced, unpinned entry.
	const uint32_t nScanCount = m_nLiveCount;
	uint32_t nFirstSecondChance = SIR_CACHE_NONE;
	uint32_t nFirstSecondChanceSlot = 0;
	for ( uint32_t nScan = 0; nScan < nScanCount; ++nScan ) {
		const uint32_t nSlot = m_nClockHand;
		const uint32_t nIndex = m_pActiveIndices[ nSlot ];
		if ( ++m_nClockHand == m_nLiveCount ) {
			m_nClockHand = 0;
		}
		sirCacheDesc_t& desc = m_pEntries[ nIndex ];
		if ( desc.nPinCount != 0 ) {
			continue;
		}
		if ( ( desc.nFlags & SIR_CACHE_REFERENCED ) != 0 ) {
			desc.nFlags &= ~SIR_CACHE_REFERENCED;
			if ( nFirstSecondChance == SIR_CACHE_NONE ) {
				nFirstSecondChance = nIndex;
				nFirstSecondChanceSlot = nSlot;
			}
			continue;
		}
		return evictIndex( nIndex, true );
	}
	if ( nFirstSecondChance == SIR_CACHE_NONE ) {
		return false;
	}
	m_nClockHand = nFirstSecondChanceSlot + 1;
	if ( m_nClockHand == m_nLiveCount ) {
		m_nClockHand = 0;
	}
	return evictIndex( nFirstSecondChance, true );
}

void *SIRCacheAllocator::alloc( uint32_t nBytes, uint32_t nAlignment,
                                sirCacheHandle_t *pHandle )
{
	// Reject requests Buddy can never satisfy before evicting any live data.
	if ( m_pEntries == nullptr || pHandle == nullptr || nBytes == 0 ||
	     nBytes > m_Buddy.getCapacity() ||
	     !SIR_IS_POWER_OF_TWO( nAlignment ) ||
	     nAlignment > m_nPageSize ) {
		return nullptr;
	}

	// reserve a descriptor before asking the buddy for backing. A full handle
	// table cannot consume a buddy block that it has no capacity to describe.
	uint32_t nIndex = SIR_CACHE_NONE;
	if ( !acquireIndex( &nIndex ) ) {
		if ( !evictOne() || !acquireIndex( &nIndex ) ) {
			return nullptr;
		}
	}

	void *pMemory = m_Buddy.alloc( nBytes, nAlignment );
	while ( pMemory == nullptr ) {
		if ( !evictOne() ) {
			returnIndex( nIndex );
			return nullptr;
		}
		pMemory = m_Buddy.alloc( nBytes, nAlignment );
	}

	sirCacheDesc_t& desc = m_pEntries[ nIndex ];
	const uintptr_t nOffset = ( (uintptr_t)( pMemory ) ) -
	                          ( (uintptr_t)( m_pBase ) );
	desc.nOffset = ( (uint32_t)( nOffset ) );
	desc.nBytes = nBytes;
	desc.nPinCount = 0;
	desc.nActiveSlot = m_nLiveCount;
	desc.nFlags = SIR_CACHE_LIVE | SIR_CACHE_REFERENCED;
	m_pActiveIndices[ m_nLiveCount++ ] = nIndex;
	pHandle->nIndex = nIndex;
	pHandle->nGeneration = desc.nGeneration;
	return pMemory;
}

void *SIRCacheAllocator::resolve( sirCacheHandle_t handle )
{
	sirCacheDesc_t *pDesc = validate( handle );
	if ( pDesc == nullptr ) {
		return nullptr;
	}
	if ( ( pDesc->nFlags & SIR_CACHE_REFERENCED ) == 0 ) {
		pDesc->nFlags |= SIR_CACHE_REFERENCED;
	}
	return entryAddress( *pDesc );
}

void *SIRCacheAllocator::pin( sirCacheHandle_t handle )
{
	sirCacheDesc_t *pDesc = validate( handle );
	if ( pDesc == nullptr || pDesc->nPinCount == UINT32_MAX ) {
		return nullptr;
	}
	++pDesc->nPinCount;
	if ( ( pDesc->nFlags & SIR_CACHE_REFERENCED ) == 0 ) {
		pDesc->nFlags |= SIR_CACHE_REFERENCED;
	}
	return entryAddress( *pDesc );
}

bool SIRCacheAllocator::touch( sirCacheHandle_t handle )
{
	sirCacheDesc_t *pDesc = validate( handle );
	if ( pDesc == nullptr ) {
		return false;
	}
	if ( ( pDesc->nFlags & SIR_CACHE_REFERENCED ) == 0 ) {
		pDesc->nFlags |= SIR_CACHE_REFERENCED;
	}
	return true;
}

bool SIRCacheAllocator::unpin( sirCacheHandle_t handle )
{
	sirCacheDesc_t *pDesc = validate( handle );
	if ( pDesc == nullptr || pDesc->nPinCount == 0 ) {
		return false;
	}
	--pDesc->nPinCount;
	return true;
}

bool SIRCacheAllocator::release( sirCacheHandle_t handle )
{
	return validate( handle ) != nullptr && evictIndex( handle.nIndex, false );
}

uint64_t SIRCacheAllocator::evictUntilFree( uint64_t nBytes )
{
	uint64_t nFreed = 0;
	while ( nFreed < nBytes ) {
		const uint64_t nOldAllocated = m_Buddy.getAllocatedBytes();
		if ( !evictOne() ) {
			break;
		}
		nFreed += nOldAllocated - m_Buddy.getAllocatedBytes();
	}
	return nFreed;
}

sirCachePressureResult_t SIRCacheAllocator::reclaimCold( uint64_t nTargetCommittedBytes, uint32_t nMaximumVictims )
{
	sirCachePressureResult_t result = {};

	if ( m_pEntries == nullptr || nTargetCommittedBytes == 0 || nMaximumVictims == 0 ) {
		return result;
	}
	const uint64_t nInitialCommitted = m_pSegment->getCommittedBytes();
	uint64_t nNextTrimAt = nTargetCommittedBytes;
	for ( uint32_t i = 0; i < nMaximumVictims; ++i ) {
		const uint64_t nOldAllocated = m_Buddy.getAllocatedBytes();
		if ( !evictOne() ) {
			break;
		}
		++result.nEvicted;
		result.nFreedBlockBytes += nOldAllocated - m_Buddy.getAllocatedBytes();
		if ( result.nFreedBlockBytes >= nNextTrimAt ) {
			if ( !m_Buddy.trimUnusedPages() ) {
				break;
			}
			result.nReclaimedPagesBytes = nInitialCommitted - m_pSegment->getCommittedBytes();
			if ( result.nReclaimedPagesBytes >= nTargetCommittedBytes ) {
				return result;
			}
			const uint64_t nMissing = nTargetCommittedBytes - result.nReclaimedPagesBytes;
			nNextTrimAt = result.nFreedBlockBytes + nMissing;
		}
	}
	m_Buddy.trimUnusedPages();
	result.nReclaimedPagesBytes = nInitialCommitted - m_pSegment->getCommittedBytes();
	return result;
}

bool SIRCacheAllocator::trimUnusedPages( void )
{
	return m_pEntries != nullptr && m_Buddy.trimUnusedPages();
}

bool SIRCacheAllocator::shutdown( void )
{
	if ( m_pEntries == nullptr || m_nLiveCount != 0 || !m_Buddy.trimUnusedPages() ) {
		return false;
	}
	m_pSegment = nullptr;
	m_pEntries = nullptr;
	m_pActiveIndices = nullptr;
	m_pBase = nullptr;
	m_nEntryCount = 0;
	m_nPageSize = 0;
	m_nFreshIndex = 0;
	m_nFreeHead = SIR_CACHE_NONE;
	m_nClockHand = 0;
	m_nLiveCount = 0;
	m_nEvicted = 0;
	m_Buddy.~SIRBuddyAllocator();
	new ( &m_Buddy ) SIRBuddyAllocator();
	return true;
}

uint32_t SIRCacheAllocator::getLiveCount( void ) const
{
	return m_nLiveCount;
}
uint64_t SIRCacheAllocator::getEvictedCount( void ) const
{
	return m_nEvicted;
}
