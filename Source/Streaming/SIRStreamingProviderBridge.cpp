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

#include <Streaming/SIRStreamingProviderBridge.h>

#include <limits.h>
#include <new>

typedef struct sirStreamingBridgeSlot_s
{
	uint32_t nTicketGeneration;
	bool bPrepareOutstanding;
	bool bCancelSent;
	bool bActivateOutstanding;
	bool bRetireOutstanding;
} sirStreamingBridgeSlot_t;

SIRStreamingProviderBridge::SIRStreamingProviderBridge( void ) noexcept
    : m_pCoordinator( nullptr ), m_pSlots( nullptr ), m_Callbacks{}, m_nCapacity( 0 ), m_iNextSlot( 0 ), m_bInitialized( false )
{
}

uint64_t SIRStreamingProviderBridge::requiredBytes( uint32_t nCapacity ) noexcept
{
	return static_cast<uint64_t>(nCapacity) * sizeof( sirStreamingBridgeSlot_t );
}

SIRStreamingResult SIRStreamingProviderBridge::initialize( SIRStreamingCoordinator *pCoordinator, void *pStorage, uint64_t nStorageBytes,
                                                             const sirStreamingProviderCallbacks_t& callbacks ) noexcept
{
	if ( m_bInitialized || pCoordinator == nullptr || pStorage == nullptr ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( callbacks.pBeginPrepare == nullptr || callbacks.pCancelPrepare == nullptr ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( callbacks.pBeginActivate == nullptr || callbacks.pBeginRetire == nullptr ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}

	const sirStreamingStats_t stats = pCoordinator->getStats();
	const bool bRunning = stats.nPreparing != 0 || stats.nPrepared != 0 || stats.nActive != 0;
	const bool bRetiring = stats.nRetiring != 0 || stats.nFailed != 0;
	if ( stats.nCapacity == 0 || bRunning || bRetiring ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}

	const uint64_t nRequired = requiredBytes( stats.nCapacity );
	if ( nRequired > SIZE_MAX || nStorageBytes < nRequired || ( reinterpret_cast<uintptr_t>(pStorage) % alignof( sirStreamingBridgeSlot_t ) ) != 0 ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}

	m_pSlots = static_cast<sirStreamingBridgeSlot_t *>(pStorage);
	for ( uint32_t iSlot = 0; iSlot < stats.nCapacity; ++iSlot ) {
		new ( m_pSlots + iSlot ) sirStreamingBridgeSlot_t{};
	}

	m_pCoordinator = pCoordinator;
	m_Callbacks = callbacks;
	m_nCapacity = stats.nCapacity;
	m_iNextSlot = 0;
	m_bInitialized = true;
	return SIRStreamingResult::SIR_STREAMING_OK;
}

SIRStreamingResult SIRStreamingProviderBridge::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIRStreamingResult::SIR_STREAMING_NOT_READY;
	}
	if ( m_pCoordinator->getStats().nOccupied != 0 ) {
		return SIRStreamingResult::SIR_STREAMING_BUSY;
	}
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		const sirStreamingBridgeSlot_t& slot = m_pSlots[iSlot];
		if ( slot.bPrepareOutstanding || slot.bActivateOutstanding || slot.bRetireOutstanding ) {
			return SIRStreamingResult::SIR_STREAMING_BUSY;
		}
	}

	m_pCoordinator = nullptr;
	m_pSlots = nullptr;
	m_Callbacks = {};
	m_nCapacity = 0;
	m_iNextSlot = 0;
	m_bInitialized = false;
	return SIRStreamingResult::SIR_STREAMING_OK;
}

bool SIRStreamingProviderBridge::hasValidEntry( sirStreamingTicket_t ticket ) const noexcept
{
	if ( !m_bInitialized || ticket.nSlot >= m_nCapacity ) {
		return false;
	}
	return m_pSlots[ticket.nSlot].nTicketGeneration == ticket.nGeneration && ticket.nGeneration != 0;
}

SIRStreamingResult SIRStreamingProviderBridge::beginRetirement( sirStreamingTicket_t ticket, sirStreamingBridgeSlot_t& slot,
                                                                  sirStreamingBridgeStats_t& stats ) noexcept
{
	if ( slot.bActivateOutstanding || slot.bPrepareOutstanding ) {
		++stats.nDeferred;
		return SIRStreamingResult::SIR_STREAMING_BUSY;
	}
	if ( !m_Callbacks.pBeginRetire( m_Callbacks.pContext, ticket ) ) {
		++stats.nDeferred;
		return SIRStreamingResult::SIR_STREAMING_BUSY;
	}

	const SIRStreamingResult eResult = m_pCoordinator->advance( ticket, SIRStreamingStage::SIR_STREAMING_STAGE_RETIRING );
	if ( eResult == SIRStreamingResult::SIR_STREAMING_OK ) {
		slot.bRetireOutstanding = true;
		++stats.nRetirementDispatched;
	}
	return eResult;
}

void SIRStreamingProviderBridge::processSlot( sirStreamingTicket_t ticket, const sirStreamingSnapshot_t& snapshot,
                                              sirStreamingBridgeStats_t& stats ) noexcept
{
	sirStreamingBridgeSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( slot.nTicketGeneration != ticket.nGeneration ) {
		// A caller that bypasses this bridge cannot erase outstanding provider
		// ownership merely by recycling a coordinator ticket.
		if ( slot.bPrepareOutstanding || slot.bActivateOutstanding || slot.bRetireOutstanding ) {
			++stats.nDeferred;
			return;
		}
		slot = {};
		slot.nTicketGeneration = ticket.nGeneration;
	}

	if ( snapshot.eStage == SIRStreamingStage::SIR_STREAMING_STAGE_QUEUED ) {
		if ( !m_Callbacks.pBeginPrepare( m_Callbacks.pContext, ticket, &snapshot.demand ) ) {
			++stats.nDeferred;
			return;
		}
		if ( m_pCoordinator->advance( ticket, SIRStreamingStage::SIR_STREAMING_STAGE_PREPARING ) == SIRStreamingResult::SIR_STREAMING_OK ) {
			slot.bPrepareOutstanding = true;
			++stats.nPrepareDispatched;
		}
		return;
	}

	if ( snapshot.eStage == SIRStreamingStage::SIR_STREAMING_STAGE_PREPARING ) {
		if ( snapshot.bCancellationRequested && !slot.bCancelSent ) {
			if ( m_Callbacks.pCancelPrepare( m_Callbacks.pContext, ticket ) ) {
				slot.bCancelSent = true;
				++stats.nCancellationDispatched;
			} else {
				++stats.nDeferred;
			}
		}
		return;
	}

	if ( snapshot.eStage == SIRStreamingStage::SIR_STREAMING_STAGE_PREPARED ) {
		if ( snapshot.bCancellationRequested ) {
			beginRetirement( ticket, slot, stats );
			return;
		}
		if ( snapshot.demand.eTarget == SIRStreamingTarget::SIR_STREAMING_TARGET_ACTIVE && !slot.bActivateOutstanding ) {
			if ( m_Callbacks.pBeginActivate( m_Callbacks.pContext, ticket ) ) {
				slot.bActivateOutstanding = true;
				++stats.nActivationDispatched;
			} else {
				++stats.nDeferred;
			}
		}
		return;
	}

	if ( snapshot.eStage == SIRStreamingStage::SIR_STREAMING_STAGE_ACTIVE && snapshot.bCancellationRequested ) {
		beginRetirement( ticket, slot, stats );
		return;
	}
	if ( snapshot.eStage == SIRStreamingStage::SIR_STREAMING_STAGE_FAILED ) {
		beginRetirement( ticket, slot, stats );
	}
}

SIRStreamingResult SIRStreamingProviderBridge::pump( uint32_t nMaximumSlots, sirStreamingBridgeStats_t *pStats ) noexcept
{
	if ( !m_bInitialized ) {
		return SIRStreamingResult::SIR_STREAMING_NOT_READY;
	}
	if ( nMaximumSlots == 0 || pStats == nullptr ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}

	sirStreamingBridgeStats_t stats {};
	const uint32_t nVisits = nMaximumSlots < m_nCapacity ? nMaximumSlots : m_nCapacity;
	for ( uint32_t iVisit = 0; iVisit < nVisits; ++iVisit ) {
		sirStreamingTicket_t ticket {};
		sirStreamingSnapshot_t snapshot {};
		const SIRStreamingResult eQuery = m_pCoordinator->querySlot( m_iNextSlot, &ticket, &snapshot );
		m_iNextSlot = m_iNextSlot + 1U == m_nCapacity ? 0U : m_iNextSlot + 1U;
		++stats.nVisited;
		if ( eQuery == SIRStreamingResult::SIR_STREAMING_OK ) {
			processSlot( ticket, snapshot, stats );
		}
	}

	*pStats = stats;
	return SIRStreamingResult::SIR_STREAMING_OK;
}

SIRStreamingResult SIRStreamingProviderBridge::acknowledgePrepared( sirStreamingTicket_t ticket, bool bSucceeded ) noexcept
{
	if ( !hasValidEntry( ticket ) ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}
	sirStreamingBridgeSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( !slot.bPrepareOutstanding ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_TRANSITION;
	}

	sirStreamingSnapshot_t snapshot {};
	if ( m_pCoordinator->query( ticket, &snapshot ) != SIRStreamingResult::SIR_STREAMING_OK ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}
	if ( snapshot.eStage != SIRStreamingStage::SIR_STREAMING_STAGE_PREPARING ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_TRANSITION;
	}

	const SIRStreamingStage eNext = bSucceeded ? SIRStreamingStage::SIR_STREAMING_STAGE_PREPARED : SIRStreamingStage::SIR_STREAMING_STAGE_FAILED;
	const SIRStreamingResult eResult = m_pCoordinator->advance( ticket, eNext );
	if ( eResult == SIRStreamingResult::SIR_STREAMING_OK ) {
		slot.bPrepareOutstanding = false;
	}
	return eResult;
}

SIRStreamingResult SIRStreamingProviderBridge::acknowledgeActivated( sirStreamingTicket_t ticket, bool bSucceeded ) noexcept
{
	if ( !hasValidEntry( ticket ) ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}
	sirStreamingBridgeSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( !slot.bActivateOutstanding ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_TRANSITION;
	}

	sirStreamingSnapshot_t snapshot {};
	if ( m_pCoordinator->query( ticket, &snapshot ) != SIRStreamingResult::SIR_STREAMING_OK ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}
	if ( snapshot.eStage != SIRStreamingStage::SIR_STREAMING_STAGE_PREPARED ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_TRANSITION;
	}

	// An activation may finish after cancellation or target downgrade.
	// The backend must retire its activated representation before this unit
	// can be prepared again at the weaker target state.
	if ( bSucceeded && snapshot.demand.eTarget != SIRStreamingTarget::SIR_STREAMING_TARGET_ACTIVE ) {
		const SIRStreamingResult eCancel = m_pCoordinator->cancel( ticket );
		if ( eCancel != SIRStreamingResult::SIR_STREAMING_OK ) {
			return eCancel;
		}
		slot.bActivateOutstanding = false;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	if ( bSucceeded && snapshot.bCancellationRequested ) {
		slot.bActivateOutstanding = false;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	const SIRStreamingStage eNext = bSucceeded ? SIRStreamingStage::SIR_STREAMING_STAGE_ACTIVE : SIRStreamingStage::SIR_STREAMING_STAGE_FAILED;
	const SIRStreamingResult eResult = m_pCoordinator->advance( ticket, eNext );
	if ( eResult == SIRStreamingResult::SIR_STREAMING_OK ) {
		slot.bActivateOutstanding = false;
	}
	return eResult;
}

SIRStreamingResult SIRStreamingProviderBridge::acknowledgeRetired( sirStreamingTicket_t ticket ) noexcept
{
	if ( !hasValidEntry( ticket ) ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}
	sirStreamingBridgeSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( !slot.bRetireOutstanding ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_TRANSITION;
	}

	sirStreamingSnapshot_t snapshot {};
	if ( m_pCoordinator->query( ticket, &snapshot ) != SIRStreamingResult::SIR_STREAMING_OK ) {
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}
	if ( snapshot.eStage != SIRStreamingStage::SIR_STREAMING_STAGE_RETIRING ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_TRANSITION;
	}

	const SIRStreamingResult eResult = m_pCoordinator->advance( ticket, SIRStreamingStage::SIR_STREAMING_STAGE_EMPTY );
	if ( eResult == SIRStreamingResult::SIR_STREAMING_OK ) {
		slot = {};
	}
	return eResult;
}
