/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureResidency.h>

static constexpr uint32_t SIR_VT_EMPTY_BUCKET = 0;

static bool sirVTKeysEqual( const sirVirtualTexturePageKey_t& lhs, const sirVirtualTexturePageKey_t& rhs ) noexcept
{
	return lhs.source.m_nAssetId == rhs.source.m_nAssetId && lhs.source.m_nTypeId == rhs.source.m_nTypeId &&
	       lhs.source.m_nVariantId == rhs.source.m_nVariantId && lhs.nRevision == rhs.nRevision && lhs.nX == rhs.nX && lhs.nY == rhs.nY &&
	       lhs.nMip == rhs.nMip && lhs.nLayer == rhs.nLayer;
}

static uint64_t sirVTMixHash( uint64_t nHash, uint64_t nValue ) noexcept
{
	nHash ^= nValue;
	nHash *= UINT64_C( 1099511628211 );
	return nHash;
}

static uint32_t sirVTHashKey( const sirVirtualTexturePageKey_t& key ) noexcept
{
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	nHash = sirVTMixHash( nHash, key.source.m_nAssetId );
	nHash = sirVTMixHash( nHash, key.source.m_nTypeId );
	nHash = sirVTMixHash( nHash, key.source.m_nVariantId );
	nHash = sirVTMixHash( nHash, key.nRevision );
	nHash = sirVTMixHash( nHash, key.nX );
	nHash = sirVTMixHash( nHash, key.nY );
	nHash = sirVTMixHash( nHash, key.nMip );
	nHash = sirVTMixHash( nHash, key.nLayer );
	return ( uint32_t )( nHash ^ ( nHash >> 32U ) );
}

uint32_t SIRVirtualTextureResidency::getCapacity( void ) const noexcept
{
	return m_storage.nSlotCapacity;
}

uint32_t SIRVirtualTextureResidency::getActiveCount( void ) const noexcept
{
	return m_nActive;
}

uint32_t SIRVirtualTextureResidency::getResidentCount( void ) const noexcept
{
	return m_nResident;
}

uint32_t SIRVirtualTextureResidency::getRetiringCount( void ) const noexcept
{
	return m_nRetiring;
}

sirGraphicsResult_t SIRVirtualTextureResidency::initialize( const sirVirtualTextureStorage_t& storage ) noexcept
{
	if ( m_storage.pSlots != nullptr && storage.nDeviceGeneration == m_storage.nDeviceGeneration ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( storage.pSlots == nullptr || storage.pBuckets == nullptr || storage.nDeviceGeneration == 0 || storage.nSlotCapacity == 0 ||
	     storage.nSlotCapacity > UINT32_MAX / 2U || storage.nBucketCapacity < storage.nSlotCapacity * 2U ||
	     ( storage.nBucketCapacity & ( storage.nBucketCapacity - 1U ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	m_storage = storage;
	m_nTouchSerial = 0;
	m_nActive = 0;
	m_nResident = 0;
	m_nRetiring = 0;
	m_nFreeHead = 0;

	for ( uint32_t iSlot = 0; iSlot < storage.nSlotCapacity; ++iSlot ) {
		storage.pSlots[iSlot] = {};
		storage.pSlots[iSlot].nGeneration = 1;
		storage.pSlots[iSlot].nNextFree = iSlot + 1U < storage.nSlotCapacity ? iSlot + 1U : UINT32_MAX;
	}
	for ( uint32_t iBucket = 0; iBucket < storage.nBucketCapacity; ++iBucket ) {
		storage.pBuckets[iBucket] = SIR_VT_EMPTY_BUCKET;
	}
	return SIR_GRAPHICS_SUCCESS;
}

uint32_t SIRVirtualTextureResidency::findBucket( const sirVirtualTexturePageKey_t& key ) const noexcept
{
	if ( m_storage.pBuckets == nullptr ) {
		return UINT32_MAX;
	}

	const uint32_t nMask = m_storage.nBucketCapacity - 1U;
	uint32_t iBucket = sirVTHashKey( key ) & nMask;
	for ( uint32_t nProbe = 0; nProbe < m_storage.nBucketCapacity; ++nProbe ) {
		const uint32_t nEntry = m_storage.pBuckets[iBucket];
		if ( nEntry == SIR_VT_EMPTY_BUCKET ) {
			return UINT32_MAX;
		}
		if ( sirVTKeysEqual( m_storage.pSlots[nEntry - 1U].key, key ) ) {
			return iBucket;
		}
		iBucket = ( iBucket + 1U ) & nMask;
	}
	return UINT32_MAX;
}

uint32_t SIRVirtualTextureResidency::findInsertionBucket( const sirVirtualTexturePageKey_t& key ) const noexcept
{
	const uint32_t nMask = m_storage.nBucketCapacity - 1U;
	uint32_t iBucket = sirVTHashKey( key ) & nMask;
	for ( uint32_t nProbe = 0; nProbe < m_storage.nBucketCapacity; ++nProbe ) {
		if ( m_storage.pBuckets[iBucket] == SIR_VT_EMPTY_BUCKET ) {
			return iBucket;
		}
		iBucket = ( iBucket + 1U ) & nMask;
	}
	return UINT32_MAX;
}

bool SIRVirtualTextureResidency::isHandleValid( sirVirtualTextureHandle_t handle ) const noexcept
{
	return m_storage.pSlots != nullptr && handle.nSlot < m_storage.nSlotCapacity && handle.nGeneration != 0 &&
	       handle.nDeviceGeneration == m_storage.nDeviceGeneration && m_storage.pSlots[handle.nSlot].nGeneration == handle.nGeneration &&
	       m_storage.pSlots[handle.nSlot].eState != SIR_VT_FREE;
}

sirGraphicsResult_t SIRVirtualTextureResidency::reserve( const sirVirtualTexturePageKey_t& key, sirVirtualTextureHandle_t *pHandle,
                                                         bool *pCreated ) noexcept
{
	if ( pHandle == nullptr || pCreated == nullptr || key.source.m_nAssetId == 0 || key.nRevision == 0 || m_storage.pSlots == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	const uint32_t iExisting = findBucket( key );
	if ( iExisting != UINT32_MAX ) {
		const uint32_t iSlot = m_storage.pBuckets[iExisting] - 1U;
		*pHandle = { iSlot, m_storage.pSlots[iSlot].nGeneration, m_storage.nDeviceGeneration };
		*pCreated = false;
		return SIR_GRAPHICS_SUCCESS;
	}

	const uint32_t iSlot = m_nFreeHead;
	if ( iSlot == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const uint32_t iBucket = findInsertionBucket( key );
	if ( iBucket == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	sirVirtualTextureSlot_t& slot = m_storage.pSlots[iSlot];
	m_nFreeHead = slot.nNextFree;
	slot.nNextFree = UINT32_MAX;
	slot.key = key;
	slot.eState = SIR_VT_REQUESTED;
	slot.nLastTouch = 0;
	slot.completion = {};
	m_storage.pBuckets[iBucket] = iSlot + 1U;
	++m_nActive;
	*pHandle = { iSlot, slot.nGeneration, m_storage.nDeviceGeneration };
	*pCreated = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVirtualTextureResidency::find( const sirVirtualTexturePageKey_t& key,
                                                      sirVirtualTextureHandle_t *pHandle ) const noexcept
{
	if ( pHandle == nullptr || m_storage.pBuckets == nullptr || key.source.m_nAssetId == 0 || key.nRevision == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t iBucket = findBucket( key );
	if ( iBucket == UINT32_MAX ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const uint32_t iSlot = m_storage.pBuckets[iBucket] - 1U;
	const sirVirtualTextureSlot_t& slot = m_storage.pSlots[iSlot];
	if ( slot.eState != SIR_VT_RESIDENT ) {
		return SIR_GRAPHICS_BUSY;
	}
	*pHandle = { iSlot, slot.nGeneration, m_storage.nDeviceGeneration };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVirtualTextureResidency::queryState( sirVirtualTextureHandle_t handle,
                                                            sirVirtualTextureState_t *pState ) const noexcept
{
	if ( pState == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !isHandleValid( handle ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	*pState = m_storage.pSlots[handle.nSlot].eState;
	return SIR_GRAPHICS_SUCCESS;
}

bool SIRVirtualTextureResidency::isCompletionValid( const sirGraphicsCompletionSet_t& completion, bool bRequireWork ) const noexcept
{
	constexpr uint32_t SIR_QUEUE_MASK = ( 1U << SIR_GRAPHICS_QUEUE_COUNT ) - 1U;
	if ( completion.nDeviceGeneration != m_storage.nDeviceGeneration || ( completion.nQueueMask & ~SIR_QUEUE_MASK ) != 0 ||
	     ( bRequireWork && completion.nQueueMask == 0 ) ) {
		return false;
	}
	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		if ( ( completion.nQueueMask & ( 1U << iQueue ) ) != 0 && completion.szQueueValues[iQueue] == 0 ) {
			return false;
		}
	}
	return true;
}

bool SIRVirtualTextureResidency::completionReached( const sirGraphicsCompletionSet_t& required,
                                                    const sirGraphicsCompletionSet_t& completed ) const noexcept
{
	if ( ( completed.nQueueMask & required.nQueueMask ) != required.nQueueMask ) {
		return false;
	}
	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		if ( ( required.nQueueMask & ( 1U << iQueue ) ) != 0 && completed.szQueueValues[iQueue] < required.szQueueValues[iQueue] ) {
			return false;
		}
	}
	return true;
}

sirGraphicsResult_t SIRVirtualTextureResidency::submitUpload( sirVirtualTextureHandle_t handle,
                                                              const sirGraphicsCompletionSet_t& completion ) noexcept
{
	if ( !isHandleValid( handle ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !isCompletionValid( completion, true ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVirtualTextureSlot_t& slot = m_storage.pSlots[handle.nSlot];
	if ( slot.eState != SIR_VT_REQUESTED ) {
		return SIR_GRAPHICS_BUSY;
	}
	slot.completion = completion;
	slot.eState = SIR_VT_UPLOADING;
	return SIR_GRAPHICS_SUCCESS;
}

void SIRVirtualTextureResidency::removeMapping( const sirVirtualTexturePageKey_t& key ) noexcept
{
	uint32_t iHole = findBucket( key );
	if ( iHole == UINT32_MAX ) {
		return;
	}

	const uint32_t nMask = m_storage.nBucketCapacity - 1U;
	uint32_t iNext = ( iHole + 1U ) & nMask;
	while ( m_storage.pBuckets[iNext] != SIR_VT_EMPTY_BUCKET ) {
		const uint32_t nEntry = m_storage.pBuckets[iNext];
		const uint32_t iHome = sirVTHashKey( m_storage.pSlots[nEntry - 1U].key ) & nMask;
		const uint32_t nCurrentDistance = ( iNext - iHome ) & nMask;
		const uint32_t nShiftedDistance = ( iHole - iHome ) & nMask;
		if ( nCurrentDistance > nShiftedDistance ) {
			m_storage.pBuckets[iHole] = nEntry;
			iHole = iNext;
		}
		iNext = ( iNext + 1U ) & nMask;
	}
	m_storage.pBuckets[iHole] = SIR_VT_EMPTY_BUCKET;
}

void SIRVirtualTextureResidency::releaseSlot( uint32_t nSlot ) noexcept
{
	sirVirtualTextureSlot_t& slot = m_storage.pSlots[nSlot];
	// Poison the slot on generation exhaustion rather than alias an old handle.
	const uint32_t nGeneration = slot.nGeneration == UINT32_MAX ? 0 : slot.nGeneration + 1U;
	slot = {};
	slot.nGeneration = nGeneration;
	slot.eState = SIR_VT_FREE;
	slot.nNextFree = UINT32_MAX;
	if ( nGeneration != 0 ) {
		slot.nNextFree = m_nFreeHead;
		m_nFreeHead = nSlot;
	}
}

sirGraphicsResult_t SIRVirtualTextureResidency::collect( const sirGraphicsCompletionSet_t& completed, uint32_t *pTransitions ) noexcept
{
	if ( pTransitions == nullptr || m_storage.pSlots == nullptr || !isCompletionValid( completed, false ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	uint32_t nTransitions = 0;
	for ( uint32_t iSlot = 0; iSlot < m_storage.nSlotCapacity; ++iSlot ) {
		sirVirtualTextureSlot_t& slot = m_storage.pSlots[iSlot];
		if ( !completionReached( slot.completion, completed ) ) {
			continue;
		}
		if ( slot.eState == SIR_VT_UPLOADING ) {
			slot.eState = SIR_VT_RESIDENT;
			slot.nLastTouch = ++m_nTouchSerial;
			++m_nResident;
			++nTransitions;
		} else if ( slot.eState == SIR_VT_RETIRING ) {
			releaseSlot( iSlot );
			--m_nRetiring;
			++nTransitions;
		}
	}
	*pTransitions = nTransitions;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVirtualTextureResidency::retire( sirVirtualTextureHandle_t handle,
                                                        const sirGraphicsCompletionSet_t& lastUse ) noexcept
{
	if ( !isHandleValid( handle ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !isCompletionValid( lastUse, true ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	sirVirtualTextureSlot_t& slot = m_storage.pSlots[handle.nSlot];
	if ( slot.eState != SIR_VT_RESIDENT && slot.eState != SIR_VT_UPLOADING ) {
		return SIR_GRAPHICS_BUSY;
	}

	removeMapping( slot.key );
	if ( slot.eState == SIR_VT_RESIDENT ) {
		--m_nResident;
	}
	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		if ( ( lastUse.nQueueMask & ( 1U << iQueue ) ) != 0 ) {
			if ( lastUse.szQueueValues[iQueue] > slot.completion.szQueueValues[iQueue] ) {
				slot.completion.szQueueValues[iQueue] = lastUse.szQueueValues[iQueue];
			}
		}
	}
	slot.completion.nQueueMask |= lastUse.nQueueMask;
	slot.completion.nDeviceGeneration = m_storage.nDeviceGeneration;
	slot.eState = SIR_VT_RETIRING;
	--m_nActive;
	++m_nRetiring;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVirtualTextureResidency::cancel( sirVirtualTextureHandle_t handle ) noexcept
{
	if ( !isHandleValid( handle ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirVirtualTextureSlot_t& slot = m_storage.pSlots[handle.nSlot];
	if ( slot.eState != SIR_VT_REQUESTED ) {
		return SIR_GRAPHICS_BUSY;
	}
	removeMapping( slot.key );
	releaseSlot( handle.nSlot );
	--m_nActive;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVirtualTextureResidency::touch( sirVirtualTextureHandle_t handle ) noexcept
{
	if ( !isHandleValid( handle ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirVirtualTextureSlot_t& slot = m_storage.pSlots[handle.nSlot];
	if ( slot.eState != SIR_VT_RESIDENT ) {
		return SIR_GRAPHICS_BUSY;
	}
	slot.nLastTouch = ++m_nTouchSerial;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVirtualTextureResidency::findOldest( sirVirtualTextureHandle_t *pHandle ) const noexcept
{
	if ( pHandle == nullptr || m_storage.pSlots == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint32_t iOldest = UINT32_MAX;
	uint64_t nOldest = UINT64_MAX;
	for ( uint32_t iSlot = 0; iSlot < m_storage.nSlotCapacity; ++iSlot ) {
		const sirVirtualTextureSlot_t& slot = m_storage.pSlots[iSlot];
		if ( slot.eState == SIR_VT_RESIDENT && slot.nLastTouch < nOldest ) {
			iOldest = iSlot;
			nOldest = slot.nLastTouch;
		}
	}
	if ( iOldest == UINT32_MAX ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	*pHandle = { iOldest, m_storage.pSlots[iOldest].nGeneration, m_storage.nDeviceGeneration };
	return SIR_GRAPHICS_SUCCESS;
}
