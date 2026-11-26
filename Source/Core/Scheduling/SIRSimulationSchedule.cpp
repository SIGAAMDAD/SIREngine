/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Scheduling/SIRSimulationSchedule.h>
#include <Core/Jobs/SIRTaskScheduler.h>

SIRSimulationSchedule::SIRSimulationSchedule( void ) noexcept
    : m_Graph(), m_pScheduler( nullptr ), m_bInitialized( false ), m_bActive( false )
{
}

bool SIRSimulationSchedule::init( SIRMemoryManager *pMemory, SIRTaskScheduler *pScheduler, const sirTaskGraphConfig_t& config ) noexcept
{
	if ( m_bInitialized || pMemory == nullptr || pScheduler == nullptr || !m_Graph.init( pMemory, config ) ) {
		return false;
	}
	m_pScheduler = pScheduler;
	m_bInitialized = true;
	return true;
}

bool SIRSimulationSchedule::begin( const sirSimulationPassDesc_t *pPasses, uint32_t nPasses, const sirTaskGraphEdge_t *pEdges,
                                   uint32_t nEdges ) noexcept
{
	if ( !m_bInitialized || m_bActive || nPasses == 0 || pPasses == nullptr || ( nEdges != 0 && pEdges == nullptr ) ) {
		return false;
	}
	if ( !m_Graph.reset() ) {
		return false;
	}

	for ( uint32_t iPass = 0; iPass < nPasses; ++iPass ) {
		const sirSimulationPassDesc_t& desc = pPasses[iPass];
		uint32_t nPassIndex = UINT32_MAX;
		if ( !m_Graph.addPass( desc.pass, desc.pAccesses, desc.nAccesses, &nPassIndex ) || nPassIndex != iPass ) {
			m_Graph.reset();
			return false;
		}
	}
	for ( uint32_t iEdge = 0; iEdge < nEdges; ++iEdge ) {
		if ( !m_Graph.addDependency( pEdges[iEdge].nBefore, pEdges[iEdge].nAfter ) ) {
			m_Graph.reset();
			return false;
		}
	}
	if ( m_Graph.compile() != SIRTaskGraphStatus::SIR_TASK_GRAPH_OK || !m_Graph.submit( m_pScheduler ) ) {
		m_Graph.reset();
		return false;
	}
	m_bActive = true;
	return true;
}

bool SIRSimulationSchedule::poll( sirTaskGraphExecutionSummary_t *pSummary ) noexcept
{
	return m_bActive && pSummary != nullptr && m_Graph.queryExecution( pSummary );
}

bool SIRSimulationSchedule::finish( void ) noexcept
{
	if ( !m_bActive || !m_Graph.releaseSubmitted() || !m_Graph.reset() ) {
		return false;
	}
	m_bActive = false;
	return true;
}

bool SIRSimulationSchedule::cancel( void ) noexcept
{
	return m_bActive && m_Graph.cancelExecution();
}

uint32_t SIRSimulationSchedule::pumpMain( uint32_t nMaximum ) noexcept
{
	return m_bActive ? m_pScheduler->pumpMain( nMaximum ) : 0;
}

uint32_t SIRSimulationSchedule::pumpCPU( uint32_t nMaximum ) noexcept
{
	return m_bActive ? m_pScheduler->pumpCPU( nMaximum ) : 0;
}

bool SIRSimulationSchedule::isActive( void ) const noexcept
{
	return m_bActive;
}
