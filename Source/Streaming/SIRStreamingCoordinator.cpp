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
	SIRStreamingStage eStage;
	bool bOccupied;
	bool bCancellationRequested;
} sirStreamingSlot_t;

static bool streamingUnitEquals( const sirStreamingUnitKey_t& a, const sirStreamingUnitKey_t& b ) noexcept
{
	return a.nContainerId == b.nContainerId && a.nUnitId == b.nUnitId && a.nManifestGeneration == b.nManifestGeneration &&
	       a.nVariant == b.nVariant && a.eKind == b.eKind;
}

static bool streamingDemandValid( const sirStreamingDemand_t& demand ) noexcept
{
	const bool bUnitValid =
	    demand.keyUnit.nUnitId != 0 && demand.keyUnit.nManifestGeneration != 0 && demand.keyUnit.eKind < SIRStreamingUnitKind::SIR_STREAMING_UNIT_COUNT;
	const bool bConsumerValid = demand.nConsumerId != 0 && demand.nConsumerGeneration != 0 && demand.nRequestEpoch != 0;
	const bool bTargetValid = demand.eTarget == SIRStreamingTarget::SIR_STREAMING_TARGET_PREPARED || demand.eTarget == SIRStreamingTarget::SIR_STREAMING_TARGET_ACTIVE;

	return bUnitValid && bConsumerValid && bTargetValid;
}

SIRStreamingCoordinator::SIRStreamingCoordinator( void ) noexcept
    : m_pSlots( nullptr ), m_nCapacity( 0 ), m_nOccupied( 0 ), m_bInitialized( false ), m_bStopped( false )
{
}

uint64_t SIRStreamingCoordinator::requiredBytes( uint32_t nCapacity ) noexcept
{
	return static_cast<uint64_t>(nCapacity) * sizeof( sirStreamingSlot_t );
}

SIRStreamingResult SIRStreamingCoordinator::initialize( void *pStorage, uint64_t nStorageBytes, uint32_t nCapacity ) noexcept
{
	const bool bInvalidState = m_bInitialized || m_bStopped;
	const bool bInvalidStorage = pStorage == nullptr || nCapacity == 0;
	if ( bInvalidState || bInvalidStorage ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}
	const uint64_t nRequiredBytes = requiredBytes( nCapacity );
	if ( ( reinterpret_cast<uintptr_t>(pStorage) % alignof( sirStreamingSlot_t ) ) != 0 || nRequiredBytes > SIZE_MAX || nStorageBytes < nRequiredBytes ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}

	m_pSlots = static_cast<sirStreamingSlot_t *>(pStorage);
	m_nCapacity = nCapacity;
	m_nOccupied = 0;
	for ( uint32_t iSlot = 0; iSlot < nCapacity; ++iSlot ) {
		new ( m_pSlots + iSlot ) sirStreamingSlot_t{};
		m_pSlots[iSlot].nGeneration = 1;
	}

	m_bInitialized = true;
	return SIRStreamingResult::SIR_STREAMING_OK;
}

bool SIRStreamingCoordinator::isValid( sirStreamingTicket_t ticket ) const noexcept
{
	const bool bInactive = !m_bInitialized || m_bStopped;
	const bool bInvalidTicket = ticket.nSlot >= m_nCapacity || ticket.nGeneration == 0;
	if ( bInactive || bInvalidTicket ) {
		return false;
	}

	const sirStreamingSlot_t& slot = m_pSlots[ticket.nSlot];
	return slot.bOccupied && slot.nGeneration == ticket.nGeneration;
}

void SIRStreamingCoordinator::releaseSlot( uint32_t iSlot ) noexcept
{
	sirStreamingSlot_t& slot = m_pSlots[iSlot];
	slot.demand = {};
	slot.eStage = SIRStreamingStage::SIR_STREAMING_STAGE_EMPTY;
	slot.bCancellationRequested = false;
	slot.bOccupied = false;
	// Saturated generations are permanently retired rather than wrapping.
	if ( slot.nGeneration != UINT32_MAX ) {
		++slot.nGeneration;
	}
	--m_nOccupied;
}

SIRStreamingResult SIRStreamingCoordinator::submit( const sirStreamingDemand_t& demand, sirStreamingTicket_t *pTicket ) noexcept
{
	if ( !m_bInitialized || m_bStopped ) {
		return SIRStreamingResult::SIR_STREAMING_NOT_READY;
	}
	if ( pTicket == nullptr || !streamingDemandValid( demand ) ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}

	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		const sirStreamingSlot_t& slot = m_pSlots[iSlot];
		if ( !slot.bOccupied ) {
			continue;
		}
		if ( slot.demand.nConsumerId == demand.nConsumerId && slot.demand.nConsumerGeneration == demand.nConsumerGeneration &&
		     streamingUnitEquals( slot.demand.keyUnit, demand.keyUnit ) ) {
			return SIRStreamingResult::SIR_STREAMING_DUPLICATE;
		}
	}

	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		sirStreamingSlot_t& slot = m_pSlots[iSlot];
		if ( slot.bOccupied || slot.nGeneration == UINT32_MAX ) {
			continue;
		}

		slot.demand = demand;
		slot.eStage = SIRStreamingStage::SIR_STREAMING_STAGE_QUEUED;
		slot.bOccupied = true;
		slot.bCancellationRequested = false;
		*pTicket = { iSlot, slot.nGeneration };
		++m_nOccupied;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}

	return SIRStreamingResult::SIR_STREAMING_OUT_OF_CAPACITY;
}

SIRStreamingResult SIRStreamingCoordinator::updateDemand( sirStreamingTicket_t ticket, const sirStreamingDemand_t& demand ) noexcept
{
	if ( !isValid( ticket ) ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}
	if ( !streamingDemandValid( demand ) ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}

	sirStreamingSlot_t& slot = m_pSlots[ticket.nSlot];
	const bool bSameOwner = slot.demand.nConsumerId == demand.nConsumerId && slot.demand.nConsumerGeneration == demand.nConsumerGeneration;
	if ( !bSameOwner || !streamingUnitEquals( slot.demand.keyUnit, demand.keyUnit ) ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( demand.nRequestEpoch < slot.demand.nRequestEpoch ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( slot.bCancellationRequested || slot.eStage == SIRStreamingStage::SIR_STREAMING_STAGE_RETIRING ) {
		return SIRStreamingResult::SIR_STREAMING_BUSY;
	}
	if ( slot.eStage == SIRStreamingStage::SIR_STREAMING_STAGE_ACTIVE && demand.eTarget != SIRStreamingTarget::SIR_STREAMING_TARGET_ACTIVE ) {
		return SIRStreamingResult::SIR_STREAMING_BUSY;
	}

	slot.demand = demand;
	return SIRStreamingResult::SIR_STREAMING_OK;
}

SIRStreamingResult SIRStreamingCoordinator::query( sirStreamingTicket_t ticket, sirStreamingSnapshot_t *pSnapshot ) const noexcept
{
	if ( pSnapshot == nullptr ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( !isValid( ticket ) ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}

	const sirStreamingSlot_t& slot = m_pSlots[ticket.nSlot];
	pSnapshot->demand = slot.demand;
	pSnapshot->eStage = slot.eStage;
	pSnapshot->bCancellationRequested = slot.bCancellationRequested;
	return SIRStreamingResult::SIR_STREAMING_OK;
}

SIRStreamingResult SIRStreamingCoordinator::querySlot( uint32_t iSlot, sirStreamingTicket_t *pTicket,
                                                         sirStreamingSnapshot_t *pSnapshot ) const noexcept
{
	if ( !m_bInitialized || m_bStopped ) {
		return SIRStreamingResult::SIR_STREAMING_NOT_READY;
	}
	if ( pTicket == nullptr || pSnapshot == nullptr || iSlot >= m_nCapacity ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}

	const sirStreamingSlot_t& slot = m_pSlots[iSlot];
	if ( !slot.bOccupied ) {
		return SIRStreamingResult::SIR_STREAMING_NOT_READY;
	}

	*pTicket = { iSlot, slot.nGeneration };
	pSnapshot->demand = slot.demand;
	pSnapshot->eStage = slot.eStage;
	pSnapshot->bCancellationRequested = slot.bCancellationRequested;
	return SIRStreamingResult::SIR_STREAMING_OK;
}

SIRStreamingResult SIRStreamingCoordinator::advance( sirStreamingTicket_t ticket, SIRStreamingStage eNextStage ) noexcept
{
	if ( !isValid( ticket ) ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}

	sirStreamingSlot_t& slot = m_pSlots[ticket.nSlot];
	const SIRStreamingStage eCurrent = slot.eStage;
	if ( eCurrent == SIRStreamingStage::SIR_STREAMING_STAGE_QUEUED && eNextStage == SIRStreamingStage::SIR_STREAMING_STAGE_PREPARING ) {
		slot.eStage = eNextStage;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	if ( eCurrent == SIRStreamingStage::SIR_STREAMING_STAGE_PREPARING && eNextStage == SIRStreamingStage::SIR_STREAMING_STAGE_PREPARED ) {
		slot.eStage = eNextStage;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	if ( eCurrent == SIRStreamingStage::SIR_STREAMING_STAGE_PREPARING && eNextStage == SIRStreamingStage::SIR_STREAMING_STAGE_FAILED ) {
		slot.eStage = eNextStage;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	if ( eCurrent == SIRStreamingStage::SIR_STREAMING_STAGE_PREPARED && eNextStage == SIRStreamingStage::SIR_STREAMING_STAGE_FAILED ) {
		slot.eStage = eNextStage;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	const bool bTargetActive = slot.demand.eTarget == SIRStreamingTarget::SIR_STREAMING_TARGET_ACTIVE && !slot.bCancellationRequested;
	if ( eCurrent == SIRStreamingStage::SIR_STREAMING_STAGE_PREPARED && eNextStage == SIRStreamingStage::SIR_STREAMING_STAGE_ACTIVE && bTargetActive ) {
		slot.eStage = eNextStage;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	const bool bReleasable =
	    eCurrent == SIRStreamingStage::SIR_STREAMING_STAGE_PREPARED || eCurrent == SIRStreamingStage::SIR_STREAMING_STAGE_ACTIVE || eCurrent == SIRStreamingStage::SIR_STREAMING_STAGE_FAILED;
	if ( bReleasable && eNextStage == SIRStreamingStage::SIR_STREAMING_STAGE_RETIRING ) {
		slot.eStage = eNextStage;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	if ( eCurrent == SIRStreamingStage::SIR_STREAMING_STAGE_RETIRING && eNextStage == SIRStreamingStage::SIR_STREAMING_STAGE_EMPTY ) {
		releaseSlot( ticket.nSlot );
		return SIRStreamingResult::SIR_STREAMING_OK;
	}

	return SIRStreamingResult::SIR_STREAMING_INVALID_TRANSITION;
}

SIRStreamingResult SIRStreamingCoordinator::cancel( sirStreamingTicket_t ticket ) noexcept
{
	if ( !isValid( ticket ) ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}

	sirStreamingSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( slot.eStage == SIRStreamingStage::SIR_STREAMING_STAGE_QUEUED ) {
		releaseSlot( ticket.nSlot );
		return SIRStreamingResult::SIR_STREAMING_OK;
	}

	// Running operations still own their buffers/fences. Only an owner-provided
	// completion may move PREPARING to PREPARED/FAILED, then to RETIRING.
	slot.bCancellationRequested = true;
	return SIRStreamingResult::SIR_STREAMING_OK;
}

sirStreamingStats_t SIRStreamingCoordinator::getStats( void ) const noexcept
{
	sirStreamingStats_t result {};
	if ( !m_bInitialized || m_bStopped ) {
		return result;
	}

	result.nCapacity = m_nCapacity;
	result.nOccupied = m_nOccupied;
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		const sirStreamingSlot_t& slot = m_pSlots[iSlot];
		if ( !slot.bOccupied ) {
			continue;
		}
		result.nCancellationPending += slot.bCancellationRequested ? 1U : 0U;
		switch ( slot.eStage ) {
		case SIRStreamingStage::SIR_STREAMING_STAGE_QUEUED:
			++result.nQueued;
			break;
		case SIRStreamingStage::SIR_STREAMING_STAGE_PREPARING:
			++result.nPreparing;
			break;
		case SIRStreamingStage::SIR_STREAMING_STAGE_PREPARED:
			++result.nPrepared;
			break;
		case SIRStreamingStage::SIR_STREAMING_STAGE_ACTIVE:
			++result.nActive;
			break;
		case SIRStreamingStage::SIR_STREAMING_STAGE_RETIRING:
			++result.nRetiring;
			break;
		case SIRStreamingStage::SIR_STREAMING_STAGE_FAILED:
			++result.nFailed;
			break;
		default:
			break;
		}
	}

	return result;
}

SIRStreamingResult SIRStreamingCoordinator::shutdown( void ) noexcept
{
	if ( !m_bInitialized || m_bStopped ) {
		return SIRStreamingResult::SIR_STREAMING_NOT_READY;
	}
	if ( m_nOccupied != 0 ) {
		return SIRStreamingResult::SIR_STREAMING_BUSY;
	}

	m_bStopped = true;
	m_pSlots = nullptr;
	m_nCapacity = 0;
	return SIRStreamingResult::SIR_STREAMING_OK;
}
