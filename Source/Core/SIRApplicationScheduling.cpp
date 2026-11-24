/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Core/SIRApplication.h>
#include <Core/Platform/SIRPlatformTime.h>

bool SIRApplication::attachTimeline( SIRTimelineCoordinator& timeline ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED || m_pTimeline != nullptr ) {
		return false;
	}
	m_pTimeline = &timeline;
	return true;
}

bool SIRApplication::attachTaskSchedule( SIRSimulationSchedule& schedule ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED || m_pSchedule != nullptr ) {
		return false;
	}
	m_pSchedule = &schedule;
	return true;
}

bool SIRApplication::configureScheduledSystem( uint32_t iSystem, SIRTaskExecutor eExecutor, const sirTaskGraphAccess_t *pAccesses,
                                               uint32_t nAccesses ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED || iSystem >= m_nSystemCount || eExecutor >= SIRTaskExecutor::SIR_TASK_EXEC_COUNT ||
	     nAccesses > SIR_APPLICATION_MAX_SYSTEM_ACCESSES || ( nAccesses != 0 && pAccesses == nullptr ) ) {
		return false;
	}
	if ( eExecutor == SIRTaskExecutor::SIR_TASK_EXEC_AUDIO ) {
		// The audio callback must not execute arbitrary gameplay system code.
		return false;
	}

	for ( uint32_t iAccess = 0; iAccess < nAccesses; ++iAccess ) {
		m_szSystemAccesses[iSystem][iAccess] = pAccesses[iAccess];
	}
	m_szSystemExecutors[iSystem] = eExecutor;
	m_szSystemAccessCounts[iSystem] = nAccesses;
	m_szSystemConfigured[iSystem] = true;
	return true;
}

bool SIRApplication::configureScheduledPriority( uint32_t iSystem, SIRTaskPriority ePriority ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED || iSystem >= m_nSystemCount || ePriority > SIRTaskPriority::SIR_TASK_PRIORITY_BACKGROUND ) {
		return false;
	}
	m_szSystemPriorities[iSystem] = ePriority;
	return true;
}

bool SIRApplication::addScheduledDependency( uint32_t iBefore, uint32_t iAfter ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED || iBefore >= m_nSystemCount || iAfter >= m_nSystemCount || iBefore == iAfter ||
	     m_ExplicitEdges.getSize() == m_ExplicitEdges.getCapacity() ) {
		return false;
	}
	return m_ExplicitEdges.tryEmplaceBack( sirTaskGraphEdge_t{ iBefore, iAfter } ) == SIRContainerResult::SIR_CONTAINER_OK;
}

bool SIRApplication::publishScheduledInput( uint64_t nNextTick, bool *pbPumped ) noexcept
{
	if ( pbPumped == nullptr ) {
		return false;
	}
	const bool bInputBegun = m_pInput == nullptr || m_pInput->begin( nNextTick );
	m_eWindowPumpResult = SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE;
	if ( m_pWindowPump != nullptr ) {
		m_eWindowPumpResult = m_pWindowPump( m_pWindowPumpContext );
	} else if ( m_pInput != nullptr && m_pInputPump != nullptr && !m_pInputPump( *m_pInput, m_pInputPumpContext ) ) {
		m_eWindowPumpResult = SIRAppWindowPumpResult::SIR_APP_WINDOW_STOP;
	}
	*pbPumped = m_eWindowPumpResult == SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE;
	if ( m_eState != SIRAppState::SIR_APP_READY || !bInputBegun ) {
		return false;
	}
	if ( m_pInput != nullptr ) {
		m_pInput->sample();
		if ( !m_pInput->publish() && ( !m_pInput->recover() || !m_pInput->publish() ) ) {
			return false;
		}
	}
	return true;
}

bool SIRApplication::openTimelineFrame( float fDeltaSeconds ) noexcept
{
	if ( m_pTimeline == nullptr ) {
		return true;
	}
	if ( fDeltaSeconds < 0.0f || fDeltaSeconds > 10.0f ) {
		return false;
	}
	const uint64_t nNowNs = SIRPlatformTime::getMonotonicNanoseconds();
	const uint64_t nDeltaNs = ( uint64_t )( static_cast<double>(fDeltaSeconds) * 1000000000.0 );
	return m_pTimeline->beginFrame( nNowNs, nDeltaNs );
}

bool SIRApplication::finishTimelineFrame( void ) noexcept
{
	return m_pTimeline == nullptr || m_pTimeline->publishFrame();
}

SIRTaskResult SIRApplication::scheduledUpdateEntry( void *pContext, sirTaskHandle_t ) noexcept
{
	sirAppScheduledContext_t *pUpdate = static_cast<sirAppScheduledContext_t *>(pContext);
	if ( pUpdate == nullptr || pUpdate->pSystem == nullptr ) {
		return SIRTaskResult::SIR_TASK_RESULT_FAILURE;
	}
	if ( pUpdate->bFixed ) {
		pUpdate->pSystem->fixedUpdate( pUpdate->fDeltaSeconds );
	} else {
		pUpdate->pSystem->update( pUpdate->fDeltaSeconds );
	}
	return SIRTaskResult::SIR_TASK_RESULT_SUCCESS;
}

bool SIRApplication::beginScheduledTick( float fDeltaSeconds, bool bFixed ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_READY || m_bFrameDispatchInProgress || m_pSchedule == nullptr || m_bScheduledActive ||
	     m_nInitializedSystems == 0 ) {
		return false;
	}
	if ( bFixed && m_pTimeline != nullptr && !m_pTimeline->canAdvanceFixed() ) {
		return false;
	}
	if ( !bFixed && !openTimelineFrame( fDeltaSeconds ) ) {
		return false;
	}
	bool bPumped = true;
	if ( !bFixed && ( !publishScheduledInput( m_nTicks + 1, &bPumped ) || !bPumped ) ) {
		if ( m_pTimeline != nullptr ) {
			m_pTimeline->cancelFrame();
		}
		if ( !bPumped ) {
			++m_nTicks;
		}
		return false;
	}

	// Pass IDs use canonical dependency initialization order; configuration
	// methods still accept registration IDs. This avoids reversing a declared
	// dependency when systems were registered in the opposite order.
	uint32_t szPassForSystem[SIR_APPLICATION_MAX_SYSTEMS];
	memset( szPassForSystem, 0, sizeof( szPassForSystem ) );
	for ( uint32_t iPass = 0; iPass < m_nInitializedSystems; ++iPass ) {
		szPassForSystem[m_szInitializationOrder[iPass]] = iPass;
	}

	uint32_t nEdges = 0;
	for ( uint32_t iPass = 0; iPass < m_nInitializedSystems; ++iPass ) {
		const uint32_t iRegistered = m_szInitializationOrder[iPass];
		sirAppScheduledContext_t& context = m_szScheduledContexts[iPass];
		context.pSystem = m_szSystems[iRegistered];
		context.fDeltaSeconds = fDeltaSeconds;
		context.bFixed = bFixed;

		sirSimulationPassDesc_t& desc = m_szScheduledPasses[iPass];
		desc.pass = { scheduledUpdateEntry, &context, m_szSystemPriorities[iRegistered],
		              m_szSystemConfigured[iRegistered] ? m_szSystemExecutors[iRegistered] : SIRTaskExecutor::SIR_TASK_EXEC_MAIN };
		desc.pAccesses = m_szSystemAccesses[iRegistered];
		desc.nAccesses = m_szSystemConfigured[iRegistered] ? m_szSystemAccessCounts[iRegistered] : 0;

		// Unconfigured systems must preserve the engine's resolved order.
		for ( uint32_t iPriorPass = 0; iPriorPass < iPass; ++iPriorPass ) {
			const uint32_t iPrior = m_szInitializationOrder[iPriorPass];
			if ( !m_szSystemConfigured[iPrior] || !m_szSystemConfigured[iRegistered] ) {
				m_szScheduledEdges[nEdges++] = { iPriorPass, iPass };
			}
		}
	}
	for ( uint32_t iConsumer = 0; iConsumer < m_nSystemCount; ++iConsumer ) {
		const uint32_t nMask = m_szDependencyMasks[iConsumer];
		for ( uint32_t iProvider = 0; iProvider < m_nSystemCount; ++iProvider ) {
			if ( ( nMask & ( 1U << iProvider ) ) != 0 ) {
				m_szScheduledEdges[nEdges++] = { szPassForSystem[iProvider], szPassForSystem[iConsumer] };
			}
		}
	}
	for ( uint32_t iEdge = 0; iEdge < m_ExplicitEdges.getSize(); ++iEdge ) {
		const sirTaskGraphEdge_t& edge = m_ExplicitEdges[iEdge];
		m_szScheduledEdges[nEdges++] = { szPassForSystem[edge.nBefore], szPassForSystem[edge.nAfter] };
	}
	if ( !m_pSchedule->begin( m_szScheduledPasses, m_nInitializedSystems, m_szScheduledEdges, nEdges ) ) {
		if ( !bFixed && m_pTimeline != nullptr ) {
			m_pTimeline->cancelFrame();
		}
		// The input snapshot has already been committed. Do not reuse its tick
		// identity even when scheduler admission fails transactionally.
		if ( !bFixed && m_pInput != nullptr ) {
			++m_nTicks;
		}
		return false;
	}
	m_fScheduledDeltaSeconds = fDeltaSeconds;
	m_bScheduledFixed = bFixed;
	m_bScheduledActive = true;
	return true;
}

SIRAppScheduledState SIRApplication::pollScheduledTick( void ) noexcept
{
	if ( !m_bScheduledActive || m_pSchedule == nullptr ) {
		return SIRAppScheduledState::SIR_APP_SCHEDULE_IDLE;
	}
	static_cast<void>(m_pSchedule->pumpMain( SIR_APPLICATION_MAX_SYSTEMS ));
	static_cast<void>(m_pSchedule->pumpCPU( SIR_APPLICATION_MAX_SYSTEMS ));

	sirTaskGraphExecutionSummary_t summary {};
	if ( !m_pSchedule->poll( &summary ) ) {
		return SIRAppScheduledState::SIR_APP_SCHEDULE_PENDING;
	}
	if ( summary.eState == SIRTaskGraphExecutionState::SIR_TASK_GRAPH_EXEC_PENDING ) {
		return SIRAppScheduledState::SIR_APP_SCHEDULE_PENDING;
	}
	if ( !m_pSchedule->finish() ) {
		return SIRAppScheduledState::SIR_APP_SCHEDULE_PENDING;
	}

	bool bSucceeded = summary.eState == SIRTaskGraphExecutionState::SIR_TASK_GRAPH_EXEC_SUCCEEDED;
	if ( bSucceeded && m_bScheduledFixed && m_pTimeline != nullptr ) {
		bSucceeded = m_pTimeline->completeFixedStep();
	}
	if ( bSucceeded && !m_bScheduledFixed ) {
		bSucceeded = finishTimelineFrame();
		if ( bSucceeded ) {
			++m_nTicks;
			m_fFrameDeltaSeconds = m_fScheduledDeltaSeconds;
			m_dElapsedSeconds += static_cast<double>(m_fScheduledDeltaSeconds);
		}
	}
	if ( !bSucceeded && !m_bScheduledFixed && m_pTimeline != nullptr ) {
		m_pTimeline->cancelFrame();
	}
	m_bScheduledActive = false;
	return bSucceeded ? SIRAppScheduledState::SIR_APP_SCHEDULE_SUCCEEDED : SIRAppScheduledState::SIR_APP_SCHEDULE_FAILED;
}

bool SIRApplication::cancelScheduledTick( void ) noexcept
{
	return m_bScheduledActive && m_pSchedule != nullptr && m_pSchedule->cancel();
}
