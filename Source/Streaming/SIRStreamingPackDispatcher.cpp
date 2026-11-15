/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Streaming/SIRStreamingPackDispatcher.h>

#include <limits.h>

SIRStreamingPackDispatcher::SIRStreamingPackDispatcher( void ) noexcept : m_Config{}, m_bInitialized( false )
{
}

static bool sirPackUnitEquals( const sirStreamingUnitKey_t& a, const sirStreamingUnitKey_t& b ) noexcept
{
	return a.nContainerId == b.nContainerId && a.nUnitId == b.nUnitId && a.nManifestGeneration == b.nManifestGeneration &&
	       a.nVariant == b.nVariant && a.eKind == b.eKind;
}

const sirStreamingPackBinding_t *SIRStreamingPackDispatcher::resolveBinding( const sirStreamingUnitKey_t& key ) const noexcept
{
	for ( uint32_t iBinding = 0; iBinding < m_Config.nBindings; ++iBinding ) {
		if ( sirPackUnitEquals( key, m_Config.pBindings[iBinding].keyUnit ) ) {
			return m_Config.pBindings + iBinding;
		}
	}
	return nullptr;
}

sirStreamingResult_t SIRStreamingPackDispatcher::initialize( const sirStreamingPackConfig_t& config ) noexcept
{
	if ( m_bInitialized || config.pCoordinator == nullptr || config.pGraph == nullptr || config.pIO == nullptr ||
	     config.pCatalog == nullptr ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( config.pSources == nullptr || config.pBindings == nullptr || config.pWorkspaces == nullptr || config.pLoads == nullptr ||
	     config.pTickets == nullptr ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( config.nSources < config.pCatalog->getShardCount() || config.nBindings == 0 || config.nLoads == 0 || !config.pCatalog->isOpen() ||
	     !config.pGraph->isReady() ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( config.nTicketCapacity == 0 || config.nTicketCapacity != config.pCoordinator->getStats().nCapacity ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( ( config.pfnReserveBudget == nullptr ) != ( config.pfnReleaseBudget == nullptr ) ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( config.pfnReserveBudget != nullptr && config.pBudgetContext == nullptr ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	for ( uint32_t iLoad = 0; iLoad < config.nLoads; ++iLoad ) {
		if ( config.pLoads[iLoad].loader.isActive() || config.pLoads[iLoad].nBudgetReservation != 0 ) {
			return SIR_STREAMING_BUSY;
		}
	}

	for ( uint32_t iTicket = 0; iTicket < config.nTicketCapacity; ++iTicket ) {
		config.pTickets[iTicket] = {};
		config.pTickets[iTicket].iLoad = UINT32_MAX;
	}
	m_Config = config;
	m_bInitialized = true;
	return SIR_STREAMING_OK;
}

bool SIRStreamingPackDispatcher::dispatchTicket( sirStreamingTicket_t ticket, const sirStreamingSnapshot_t& snapshot,
                                                 sirStreamingPackStats_t& stats ) noexcept
{
	const sirStreamingPackBinding_t *pBinding = resolveBinding( snapshot.demand.keyUnit );
	if ( pBinding == nullptr ) {
		if ( m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_PREPARING ) == SIR_STREAMING_OK ) {
			m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_FAILED );
		}
		++stats.nFailed;
		return true;
	}

	SIRResourceHandle hCached = {};
	if ( SIRPackAsyncResourceLoad::findReusable( *m_Config.pGraph, *m_Config.pCatalog, pBinding->keyResource, pBinding->nExpectedRevision,
	                                             &hCached ) == SIR_RESOURCE_RESULT_SUCCESS ) {
		if ( m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ||
		     m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ) {
			++stats.nFailed;
		} else {
			++stats.nCacheHits;
		}
		return true;
	}

	uint32_t iLoad = UINT32_MAX;
	bool bShared = false;
	for ( uint32_t i = 0; i < m_Config.nLoads; ++i ) {
		if ( m_Config.pLoads[i].loader.accepts( pBinding->keyResource, pBinding->nExpectedRevision ) ) {
			iLoad = i;
			bShared = true;
			break;
		}
	}
	if ( iLoad == UINT32_MAX ) {
		for ( uint32_t i = 0; i < m_Config.nLoads; ++i ) {
			if ( !m_Config.pLoads[i].loader.isActive() ) {
				iLoad = i;
				break;
			}
		}
	}
	if ( iLoad == UINT32_MAX ) {
		++stats.nDeferred;
		return false;
	}

	if ( bShared ) {
		if ( !m_Config.pLoads[iLoad].loader.retainSubscriber( pBinding->keyResource, pBinding->nExpectedRevision ) ) {
			++stats.nDeferred;
			return false;
		}
		++stats.nShared;
	} else {
		uint64_t nReservation = 0;
		if ( m_Config.pfnReserveBudget != nullptr ) {
			const sirStreamingPackBudgetDemand_t budgetDemand = { pBinding->keyResource, pBinding->nExpectedRevision,
			                                                      snapshot.demand.nPriority,
			                                                      snapshot.demand.eTarget == SIR_STREAMING_TARGET_ACTIVE };
			if ( !m_Config.pfnReserveBudget( m_Config.pBudgetContext, budgetDemand, &nReservation ) || nReservation == 0 ) {
				++stats.nDeferred;
				return false;
			}
		}
		const sirResourceResult_t eBegin =
		    m_Config.pLoads[iLoad].loader.begin( *m_Config.pGraph, *m_Config.pIO, *m_Config.pCatalog, m_Config.pSources, m_Config.nSources,
			                                     pBinding->keyResource, pBinding->nExpectedRevision, m_Config.pWorkspaces[iLoad] );
		if ( eBegin != SIR_RESOURCE_RESULT_SUCCESS && nReservation != 0 ) {
			m_Config.pfnReleaseBudget( m_Config.pBudgetContext, nReservation );
		}
		if ( eBegin == SIR_RESOURCE_RESULT_OUT_OF_BUDGET || eBegin == SIR_RESOURCE_RESULT_BUSY ) {
			++stats.nDeferred;
			return false;
		}
		if ( eBegin != SIR_RESOURCE_RESULT_SUCCESS ) {
			if ( m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_PREPARING ) == SIR_STREAMING_OK ) {
				m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_FAILED );
			}
			++stats.nFailed;
			return true;
		}
		m_Config.pLoads[iLoad].nBudgetReservation = nReservation;
		++stats.nStarted;
	}

	if ( m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ) {
		m_Config.pLoads[iLoad].loader.withdrawSubscriber();
		++stats.nFailed;
		return false;
	}
	m_Config.pTickets[ticket.nSlot] = { ticket.nGeneration, iLoad, true };
	return true;
}

void SIRStreamingPackDispatcher::processCancellations( sirStreamingPackStats_t& stats ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < m_Config.nTicketCapacity; ++iSlot ) {
		sirStreamingPackTicketState_t& state = m_Config.pTickets[iSlot];
		if ( !state.bSubscribed || state.iLoad >= m_Config.nLoads ) {
			continue;
		}
		const sirStreamingTicket_t ticket = { iSlot, state.nGeneration };
		sirStreamingSnapshot_t snapshot = {};
		if ( m_Config.pCoordinator->query( ticket, &snapshot ) != SIR_STREAMING_OK || !snapshot.bCancellationRequested ) {
			continue;
		}
		state.bSubscribed = false;
		const sirResourceResult_t eWithdraw = m_Config.pLoads[state.iLoad].loader.withdrawSubscriber();
		if ( eWithdraw != SIR_RESOURCE_RESULT_SUCCESS && eWithdraw != SIR_RESOURCE_RESULT_BUSY ) {
			continue;
		}
		++stats.nCancelled;
		if ( !m_Config.pLoads[state.iLoad].loader.isActive() ) {
			finishTicket( ticket, false, true );
		}
	}
}

void SIRStreamingPackDispatcher::finishTicket( sirStreamingTicket_t ticket, bool bSucceeded, bool bCancelled ) noexcept
{
	sirStreamingSnapshot_t snapshot = {};
	if ( m_Config.pCoordinator->query( ticket, &snapshot ) != SIR_STREAMING_OK || snapshot.eStage != SIR_STREAMING_STAGE_PREPARING ) {
		return;
	}
	const sirStreamingStage_t eStage = bSucceeded ? SIR_STREAMING_STAGE_PREPARED : SIR_STREAMING_STAGE_FAILED;
	if ( m_Config.pCoordinator->advance( ticket, eStage ) != SIR_STREAMING_OK ) {
		return;
	}
	if ( bCancelled ) {
		m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_RETIRING );
		m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_EMPTY );
	}
}

sirStreamingResult_t SIRStreamingPackDispatcher::pump( uint32_t nMaxDispatches, sirStreamingPackStats_t *pStats ) noexcept
{
	if ( !m_bInitialized || pStats == nullptr ) {
		return SIR_STREAMING_NOT_READY;
	}
	sirStreamingPackStats_t stats = {};
	processCancellations( stats );
	for ( uint32_t iDispatch = 0; iDispatch < nMaxDispatches; ++iDispatch ) {
		uint32_t iBest = UINT32_MAX;
		sirStreamingTicket_t best = {};
		sirStreamingSnapshot_t selected = {};
		for ( uint32_t iTicket = 0; iTicket < m_Config.nTicketCapacity; ++iTicket ) {
			sirStreamingTicket_t ticket = {};
			sirStreamingSnapshot_t snapshot = {};
			if ( m_Config.pCoordinator->querySlot( iTicket, &ticket, &snapshot ) != SIR_STREAMING_OK ||
			     snapshot.eStage != SIR_STREAMING_STAGE_QUEUED ) {
				continue;
			}
			if ( iBest == UINT32_MAX || snapshot.demand.nPriority > selected.demand.nPriority ) {
				iBest = iTicket;
				best = ticket;
				selected = snapshot;
			}
		}
		if ( iBest == UINT32_MAX ) {
			break;
		}
		if ( !dispatchTicket( best, selected, stats ) ) {
			break;
		}
	}
	*pStats = stats;
	return SIR_STREAMING_OK;
}

bool SIRStreamingPackDispatcher::ownsCompletion( const sirIOCompletion_t& completion ) const noexcept
{
	if ( !m_bInitialized ) {
		return false;
	}
	for ( uint32_t iLoad = 0; iLoad < m_Config.nLoads; ++iLoad ) {
		if ( m_Config.pLoads[iLoad].loader.ownsCompletion( completion ) ) {
			return true;
		}
	}
	return false;
}

sirStreamingResult_t SIRStreamingPackDispatcher::complete( const sirIOCompletion_t& completion, SIRResourceHandle *pPublished ) noexcept
{
	if ( !m_bInitialized || pPublished == nullptr ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	for ( uint32_t iLoad = 0; iLoad < m_Config.nLoads; ++iLoad ) {
		SIRPackAsyncResourceLoad& loader = m_Config.pLoads[iLoad].loader;
		if ( !loader.ownsCompletion( completion ) ) {
			continue;
		}
		const sirResourceResult_t eResult = loader.complete( completion, pPublished );
		if ( eResult == SIR_RESOURCE_RESULT_BUSY ) {
			return SIR_STREAMING_OK;
		}
		const bool bSuccess = eResult == SIR_RESOURCE_RESULT_SUCCESS;
		const uint64_t nReservation = m_Config.pLoads[iLoad].nBudgetReservation;
		if ( nReservation != 0 ) {
			m_Config.pfnReleaseBudget( m_Config.pBudgetContext, nReservation );
			m_Config.pLoads[iLoad].nBudgetReservation = 0;
		}
		for ( uint32_t iTicket = 0; iTicket < m_Config.nTicketCapacity; ++iTicket ) {
			sirStreamingPackTicketState_t& state = m_Config.pTickets[iTicket];
			if ( state.iLoad != iLoad || state.nGeneration == 0 ) {
				continue;
			}
			const sirStreamingTicket_t ticket = { iTicket, state.nGeneration };
			const bool bCancelled = !state.bSubscribed;
			finishTicket( ticket, bSuccess && !bCancelled, bCancelled );
			state = {};
			state.iLoad = UINT32_MAX;
		}
		return bSuccess ? SIR_STREAMING_OK : SIR_STREAMING_INVALID_TRANSITION;
	}
	return SIR_STREAMING_STALE_TICKET;
}

sirStreamingResult_t SIRStreamingPackDispatcher::retirePrepared( sirStreamingTicket_t ticket ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_STREAMING_NOT_READY;
	}
	if ( m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ) {
		return SIR_STREAMING_INVALID_TRANSITION;
	}
	return m_Config.pCoordinator->advance( ticket, SIR_STREAMING_STAGE_EMPTY );
}

uint32_t SIRStreamingPackDispatcher::getActiveLoadCount( void ) const noexcept
{
	uint32_t nActive = 0;
	if ( m_bInitialized ) {
		for ( uint32_t iLoad = 0; iLoad < m_Config.nLoads; ++iLoad ) {
			nActive += m_Config.pLoads[iLoad].loader.isActive() ? 1U : 0U;
		}
	}
	return nActive;
}

sirStreamingResult_t SIRStreamingPackDispatcher::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_STREAMING_NOT_READY;
	}
	if ( getActiveLoadCount() != 0 ) {
		return SIR_STREAMING_BUSY;
	}
	for ( uint32_t iLoad = 0; iLoad < m_Config.nLoads; ++iLoad ) {
		if ( m_Config.pLoads[iLoad].nBudgetReservation != 0 ) {
			return SIR_STREAMING_BUSY;
		}
	}
	m_Config = {};
	m_bInitialized = false;
	return SIR_STREAMING_OK;
}
