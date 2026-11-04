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

#include <new>
#include <Core/Memory/Allocators/SIRFrameAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <Core/Memory/SIRMemoryUtils.h>
sirFrameSlot_s::sirFrameSlot_s( void ) : nEpoch( 0 ), bInFlight( false )
{
}

SIRFrameAllocator::SIRFrameAllocator( void )
    : m_pSlots( nullptr ), m_nLastEpoch( 0 ), m_nCompletedEpoch( 0 ), m_nSlotCount( 0 ), m_nActiveSlot( SIR_FRAME_INVALID_SLOT )
{
}

uint64_t SIRFrameAllocator::getRequiredMetadataBytes( uint32_t nSlotCount )
{
	if ( nSlotCount < 2 || nSlotCount == SIR_FRAME_INVALID_SLOT ) {
		return 0;
	}

	return ( ( uint64_t )( nSlotCount ) ) * sizeof( sirFrameSlot_t );
}

bool SIRFrameAllocator::init( SIRMemorySegment *const *ppSegments, uint32_t nSlotCount, uint64_t nCommitIncrementBytes )
{
	if ( m_pSlots != nullptr || ppSegments == nullptr || getRequiredMetadataBytes( nSlotCount ) == 0 ) {
		return false;
	}

	SIRMemoryManager *pManager = nullptr;
	for ( uint32_t i = 0; i < nSlotCount; ++i ) {
		SIRMemorySegment *pSegment = ppSegments[i];
		if ( pSegment == nullptr || pSegment->getBase() == nullptr || pSegment->getReservedBytes() == 0 || pSegment->getPageSize() == 0 ) {
			return false;
		}

		if ( pManager == nullptr ) {
			pManager = pSegment->getManager();
		}
		if ( pManager == nullptr || pSegment->getManager() != pManager ) {
			return false;
		}

		if ( !SIR_IS_POWER_OF_TWO( pSegment->getPageSize() ) || ( uint64_t )( pSegment->getPageSize() ) > UINTPTR_MAX ||
		     ( uint64_t )( nCommitIncrementBytes != 0 ? nCommitIncrementBytes : pSegment->getPageSize() ) >
		         UINTPTR_MAX - ( ( uint64_t )( pSegment->getPageSize() ) - 1 ) ) {
			return false;
		}

		// Only initialization scans for duplicate segment ownership.
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( ppSegments[j] == pSegment ) {
				return false;
			}
		}
	}

	const uint64_t nBytes = getRequiredMetadataBytes( nSlotCount );
	void *pStorage = pManager->allocateMetadata( nBytes, alignof( sirFrameSlot_t ) );
	if ( pStorage == nullptr ) {
		return false;
	}

	sirFrameSlot_t *pSlots = ( ( sirFrameSlot_t * )( pStorage ) );
	for ( uint32_t i = 0; i < nSlotCount; ++i ) {
		new ( pSlots + i ) sirFrameSlot_t();
		// The segment and granularity were validated above, so Init cannot
		// fail unless the caller violates exclusive segment ownership.
		if ( !pSlots[i].Linear.init( ppSegments[i], nCommitIncrementBytes ) ) {
			for ( uint32_t j = 0; j < i; ++j ) {
				( void )pSlots[j].Linear.shutdown();
				pSlots[j].~sirFrameSlot_t();
			}
			pSlots[i].~sirFrameSlot_t();
			// Bump-only manager metadata remains consumed on failure.
			return false;
		}
	}

	m_pSlots = pSlots;
	m_nSlotCount = nSlotCount;
	m_nLastEpoch = 0;
	m_nCompletedEpoch = 0;
	m_nActiveSlot = SIR_FRAME_INVALID_SLOT;
	return true;
}

bool SIRFrameAllocator::beginEpoch( uint64_t nEpoch, uint64_t nCompletedEpoch )
{
	if ( m_pSlots == nullptr || m_nActiveSlot != SIR_FRAME_INVALID_SLOT || nEpoch == 0 || nEpoch <= m_nLastEpoch ||
	     nCompletedEpoch < m_nCompletedEpoch || nCompletedEpoch >= nEpoch ) {
		return false;
	}

	const uint32_t nSlot = ( ( uint32_t )( nEpoch % m_nSlotCount ) );
	sirFrameSlot_t& slot = m_pSlots[nSlot];
	if ( slot.bInFlight && slot.nEpoch > nCompletedEpoch ) {
		return false;
	}

	// Successful beginEpoch reclaims all completed prior epochs. reset keeps
	// committed pages warm and is constant-time.
	if ( !slot.Linear.reset( false ) ) {
		return false;
	}

	for ( uint32_t i = 0; i < m_nSlotCount; ++i ) {
		if ( m_pSlots[i].bInFlight && m_pSlots[i].nEpoch <= nCompletedEpoch ) {
			m_pSlots[i].bInFlight = false;
		}
	}

	slot.nEpoch = nEpoch;
	slot.bInFlight = false;
	m_nActiveSlot = nSlot;
	m_nLastEpoch = nEpoch;
	m_nCompletedEpoch = nCompletedEpoch;
	return true;
}

void *SIRFrameAllocator::alloc( uint64_t nBytes, uint64_t nAlignment )
{
	if ( m_pSlots == nullptr || m_nActiveSlot == SIR_FRAME_INVALID_SLOT ) {
		return nullptr;
	}
	return m_pSlots[m_nActiveSlot].Linear.alloc( nBytes, nAlignment );
}

bool SIRFrameAllocator::precommitSlot( uint32_t nSlot, uint64_t nBytes )
{
	return m_pSlots != nullptr && nSlot < m_nSlotCount && m_pSlots[nSlot].Linear.precommit( nBytes );
}

bool SIRFrameAllocator::submitEpoch( uint64_t nEpoch )
{
	if ( m_pSlots == nullptr || m_nActiveSlot == SIR_FRAME_INVALID_SLOT || m_pSlots[m_nActiveSlot].nEpoch != nEpoch ) {
		return false;
	}

	m_pSlots[m_nActiveSlot].bInFlight = true;
	m_nActiveSlot = SIR_FRAME_INVALID_SLOT;
	return true;
}

bool SIRFrameAllocator::releaseCompleted( uint64_t nCompletedEpoch )
{
	if ( m_pSlots == nullptr || nCompletedEpoch < m_nCompletedEpoch || nCompletedEpoch > m_nLastEpoch ||
	     ( m_nActiveSlot != SIR_FRAME_INVALID_SLOT && nCompletedEpoch >= m_pSlots[m_nActiveSlot].nEpoch ) ) {
		return false;
	}

	for ( uint32_t i = 0; i < m_nSlotCount; ++i ) {
		if ( m_pSlots[i].bInFlight && m_pSlots[i].nEpoch <= nCompletedEpoch ) {
			m_pSlots[i].bInFlight = false;
		}
	}
	m_nCompletedEpoch = nCompletedEpoch;
	return true;
}

bool SIRFrameAllocator::trimIdle( void )
{
	return trimIdleRetaining( 0 );
}

bool SIRFrameAllocator::trimIdleRetaining( uint64_t nWarmBytes )
{
	if ( m_pSlots == nullptr ) {
		return false;
	}

	bool bSuccess = true;
	for ( uint32_t i = 0; i < m_nSlotCount; ++i ) {
		if ( i == m_nActiveSlot || m_pSlots[i].bInFlight ) {
			continue;
		}
		SIRLinearAllocator& linear = m_pSlots[i].Linear;
		const uint64_t nKeep = nWarmBytes < linear.getCapacity() ? nWarmBytes : linear.getCapacity();
		if ( !linear.reset( false ) || !linear.trimTo( nKeep ) ) {
			bSuccess = false;
		}
	}
	return bSuccess;
}

bool SIRFrameAllocator::shutdown( void )
{
	if ( m_pSlots == nullptr || m_nActiveSlot != SIR_FRAME_INVALID_SLOT ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nSlotCount; ++i ) {
		if ( m_pSlots[i].bInFlight ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < m_nSlotCount; ++i ) {
		if ( !m_pSlots[i].Linear.reset( false ) || !m_pSlots[i].Linear.shutdown() ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < m_nSlotCount; ++i ) {
		m_pSlots[i].~sirFrameSlot_t();
	}
	m_pSlots = nullptr;
	m_nSlotCount = 0;
	m_nLastEpoch = 0;
	m_nCompletedEpoch = 0;
	m_nActiveSlot = SIR_FRAME_INVALID_SLOT;
	return true;
}

uint32_t SIRFrameAllocator::getSlotCount( void ) const
{
	return m_nSlotCount;
}

uint64_t SIRFrameAllocator::getCurrentEpoch( void ) const
{
	return m_pSlots != nullptr && m_nActiveSlot != SIR_FRAME_INVALID_SLOT ? m_pSlots[m_nActiveSlot].nEpoch : 0;
}

uint64_t SIRFrameAllocator::getLastCompletedEpoch( void ) const
{
	return m_nCompletedEpoch;
}

uint64_t SIRFrameAllocator::getActiveUsedBytes( void ) const
{
	return m_pSlots != nullptr && m_nActiveSlot != SIR_FRAME_INVALID_SLOT ? m_pSlots[m_nActiveSlot].Linear.getUsedBytes() : 0;
}
