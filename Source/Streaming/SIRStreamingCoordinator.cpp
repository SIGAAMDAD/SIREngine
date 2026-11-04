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

#include <Streaming/SIRStreamingCoordinator.h>

#include <limits.h>
#include <new>

typedef struct sirStreamingSlot_s
{
	sirStreamingDemand_t demand;
	uint32_t nGeneration;
	sirStreamingStage_t eStage;
	bool bOccupied;
	bool bCancellationRequested;
} sirStreamingSlot_t;

static bool streamingUnitEquals( const sirStreamingUnitKey_t &a, const sirStreamingUnitKey_t &b ) noexcept
{
	return a.nContainerId == b.nContainerId && a.nUnitId == b.nUnitId && a.nManifestGeneration == b.nManifestGeneration &&
	       a.nVariant == b.nVariant && a.eKind == b.eKind;
}

static bool streamingDemandValid( const sirStreamingDemand_t &demand ) noexcept
{
	const bool bUnitValid =
	    demand.keyUnit.nUnitId != 0 && demand.keyUnit.nManifestGeneration != 0 && demand.keyUnit.eKind < SIR_STREAMING_UNIT_COUNT;
	const bool bConsumerValid = demand.nConsumerId != 0 && demand.nConsumerGeneration != 0 && demand.nRequestEpoch != 0;
	const bool bTargetValid = demand.eTarget == SIR_STREAMING_TARGET_PREPARED || demand.eTarget == SIR_STREAMING_TARGET_ACTIVE;

	return bUnitValid && bConsumerValid && bTargetValid;
}

SIRStreamingCoordinator::SIRStreamingCoordinator( void ) noexcept
    : m_pSlots( nullptr ), m_nCapacity( 0 ), m_nOccupied( 0 ), m_bInitialized( false ), m_bStopped( false )
{
}

uint64_t SIRStreamingCoordinator::requiredBytes( uint32_t nCapacity ) noexcept
{
	return ( uint64_t )nCapacity * sizeof( sirStreamingSlot_t );
}

sirStreamingResult_t SIRStreamingCoordinator::initialize( void *pStorage, uint64_t nStorageBytes, uint32_t nCapacity ) noexcept
{
	if ( m_bInitialized || m_bStopped || pStorage == nullptr || nCapacity == 0 ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	const uint64_t nRequiredBytes = requiredBytes( nCapacity );
	if ( ( ( uintptr_t )pStorage % alignof( sirStreamingSlot_t ) ) != 0 || nRequiredBytes > SIZE_MAX || nStorageBytes < nRequiredBytes ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	m_pSlots = ( sirStreamingSlot_t * )pStorage;
	m_nCapacity = nCapacity;
	m_nOccupied = 0;
	for ( uint32_t iSlot = 0; iSlot < nCapacity; ++iSlot ) {
		new ( m_pSlots + iSlot ) sirStreamingSlot_t{};
		m_pSlots[iSlot].nGeneration = 1;
	}

	m_bInitialized = true;
	return SIR_STREAMING_OK;
}

bool SIRStreamingCoordinator::isValid( sirStreamingTicket_t ticket ) const noexcept
{
	if ( !m_bInitialized || m_bStopped || ticket.nSlot >= m_nCapacity || ticket.nGeneration == 0 ) {
		return false;
	}

	const sirStreamingSlot_t &slot = m_pSlots[ticket.nSlot];
	return slot.bOccupied && slot.nGeneration == ticket.nGeneration;
}

void SIRStreamingCoordinator::releaseSlot( uint32_t iSlot ) noexcept
{
	sirStreamingSlot_t &slot = m_pSlots[iSlot];
	slot.demand = {};
	slot.eStage = SIR_STREAMING_STAGE_EMPTY;
	slot.bCancellationRequested = false;
	slot.bOccupied = false;
	// Saturated generations are permanently retired rather than wrapping.
	if ( slot.nGeneration != UINT32_MAX ) {
		++slot.nGeneration;
	}
	--m_nOccupied;
}

sirStreamingResult_t SIRStreamingCoordinator::submit( const sirStreamingDemand_t &demand, sirStreamingTicket_t *pTicket ) noexcept
{
	if ( !m_bInitialized || m_bStopped ) {
		return SIR_STREAMING_NOT_READY;
	}
	if ( pTicket == nullptr || !streamingDemandValid( demand ) ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		const sirStreamingSlot_t &slot = m_pSlots[iSlot];
		if ( !slot.bOccupied ) {
			continue;
		}
		if ( slot.demand.nConsumerId == demand.nConsumerId && slot.demand.nConsumerGeneration == demand.nConsumerGeneration &&
		     streamingUnitEquals( slot.demand.keyUnit, demand.keyUnit ) ) {
			return SIR_STREAMING_DUPLICATE;
		}
	}

	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		sirStreamingSlot_t &slot = m_pSlots[iSlot];
		if ( slot.bOccupied || slot.nGeneration == UINT32_MAX ) {
			continue;
		}

		slot.demand = demand;
		slot.eStage = SIR_STREAMING_STAGE_QUEUED;
		slot.bOccupied = true;
		slot.bCancellationRequested = false;
		*pTicket = { iSlot, slot.nGeneration };
		++m_nOccupied;
		return SIR_STREAMING_OK;
	}

	return SIR_STREAMING_OUT_OF_CAPACITY;
}

sirStreamingResult_t SIRStreamingCoordinator::query( sirStreamingTicket_t ticket, sirStreamingSnapshot_t *pSnapshot ) const noexcept
{
	if ( pSnapshot == nullptr ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( !isValid( ticket ) ) {
		return SIR_STREAMING_STALE_TICKET;
	}

	const sirStreamingSlot_t &slot = m_pSlots[ticket.nSlot];
	pSnapshot->demand = slot.demand;
	pSnapshot->eStage = slot.eStage;
	pSnapshot->bCancellationRequested = slot.bCancellationRequested;
	return SIR_STREAMING_OK;
}

sirStreamingResult_t SIRStreamingCoordinator::advance( sirStreamingTicket_t ticket, sirStreamingStage_t eNextStage ) noexcept
{
	if ( !isValid( ticket ) ) {
		return SIR_STREAMING_STALE_TICKET;
	}

	sirStreamingSlot_t &slot = m_pSlots[ticket.nSlot];
	const sirStreamingStage_t eCurrent = slot.eStage;
	if ( eCurrent == SIR_STREAMING_STAGE_QUEUED && eNextStage == SIR_STREAMING_STAGE_PREPARING ) {
		slot.eStage = eNextStage;
		return SIR_STREAMING_OK;
	}
	if ( eCurrent == SIR_STREAMING_STAGE_PREPARING && eNextStage == SIR_STREAMING_STAGE_PREPARED ) {
		slot.eStage = eNextStage;
		return SIR_STREAMING_OK;
	}
	if ( eCurrent == SIR_STREAMING_STAGE_PREPARING && eNextStage == SIR_STREAMING_STAGE_FAILED ) {
		slot.eStage = eNextStage;
		return SIR_STREAMING_OK;
	}
	if ( eCurrent == SIR_STREAMING_STAGE_PREPARED && eNextStage == SIR_STREAMING_STAGE_ACTIVE &&
	     slot.demand.eTarget == SIR_STREAMING_TARGET_ACTIVE && !slot.bCancellationRequested ) {
		slot.eStage = eNextStage;
		return SIR_STREAMING_OK;
	}
	const bool bReleasable =
	    eCurrent == SIR_STREAMING_STAGE_PREPARED || eCurrent == SIR_STREAMING_STAGE_ACTIVE || eCurrent == SIR_STREAMING_STAGE_FAILED;
	if ( bReleasable && eNextStage == SIR_STREAMING_STAGE_RETIRING ) {
		slot.eStage = eNextStage;
		return SIR_STREAMING_OK;
	}
	if ( eCurrent == SIR_STREAMING_STAGE_RETIRING && eNextStage == SIR_STREAMING_STAGE_EMPTY ) {
		releaseSlot( ticket.nSlot );
		return SIR_STREAMING_OK;
	}

	return SIR_STREAMING_INVALID_TRANSITION;
}

sirStreamingResult_t SIRStreamingCoordinator::cancel( sirStreamingTicket_t ticket ) noexcept
{
	if ( !isValid( ticket ) ) {
		return SIR_STREAMING_STALE_TICKET;
	}

	sirStreamingSlot_t &slot = m_pSlots[ticket.nSlot];
	if ( slot.eStage == SIR_STREAMING_STAGE_QUEUED ) {
		releaseSlot( ticket.nSlot );
		return SIR_STREAMING_OK;
	}

	// Running operations still own their buffers/fences. Only an owner-provided
	// completion may move PREPARING to PREPARED/FAILED, then to RETIRING.
	slot.bCancellationRequested = true;
	return SIR_STREAMING_OK;
}

sirStreamingStats_t SIRStreamingCoordinator::getStats( void ) const noexcept
{
	sirStreamingStats_t result = {};
	if ( !m_bInitialized || m_bStopped ) {
		return result;
	}

	result.nCapacity = m_nCapacity;
	result.nOccupied = m_nOccupied;
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		const sirStreamingSlot_t &slot = m_pSlots[iSlot];
		if ( !slot.bOccupied ) {
			continue;
		}
		result.nCancellationPending += slot.bCancellationRequested ? 1U : 0U;
		switch ( slot.eStage ) {
		case SIR_STREAMING_STAGE_QUEUED:
			++result.nQueued;
			break;
		case SIR_STREAMING_STAGE_PREPARING:
			++result.nPreparing;
			break;
		case SIR_STREAMING_STAGE_PREPARED:
			++result.nPrepared;
			break;
		case SIR_STREAMING_STAGE_ACTIVE:
			++result.nActive;
			break;
		case SIR_STREAMING_STAGE_RETIRING:
			++result.nRetiring;
			break;
		case SIR_STREAMING_STAGE_FAILED:
			++result.nFailed;
			break;
		default:
			break;
		}
	}

	return result;
}

sirStreamingResult_t SIRStreamingCoordinator::shutdown( void ) noexcept
{
	if ( !m_bInitialized || m_bStopped ) {
		return SIR_STREAMING_NOT_READY;
	}
	if ( m_nOccupied != 0 ) {
		return SIR_STREAMING_BUSY;
	}

	m_bStopped = true;
	m_pSlots = nullptr;
	m_nCapacity = 0;
	return SIR_STREAMING_OK;
}
