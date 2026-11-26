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

#ifndef __SIR_SIMULATIONSCHEDULE_H__
#define __SIR_SIMULATIONSCHEDULE_H__
#include <Core/SIRPCH.h>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <Core/Jobs/SIRTaskGraph.h>

class SIRMemoryManager;
class SIRTaskScheduler;

typedef struct sirSimulationPassDesc_s
{
	sirTaskGraphPass_t pass;
	const sirTaskGraphAccess_t *pAccesses;
	uint32_t nAccesses;
} sirSimulationPassDesc_t;

// One owner builds a dependency graph, then the existing scheduler executes it.
// Provider-affine work completes at registered safe points, not on this owner.
// Caller-owned pass contexts must remain alive through finish() or cancellation.
class SIRSimulationSchedule
{
public:
	SIRSimulationSchedule( void ) noexcept;
	SIRSimulationSchedule( const SIRSimulationSchedule& ) = delete;
	SIRSimulationSchedule& operator=( const SIRSimulationSchedule& ) = delete;

	bool init( SIRMemoryManager *pMemory, SIRTaskScheduler *pScheduler, const sirTaskGraphConfig_t& config ) noexcept;
	bool begin( const sirSimulationPassDesc_t *pPasses, uint32_t nPasses, const sirTaskGraphEdge_t *pEdges, uint32_t nEdges ) noexcept;
	bool poll( sirTaskGraphExecutionSummary_t *pSummary ) noexcept;
	bool finish( void ) noexcept;
	bool cancel( void ) noexcept;
	uint32_t pumpMain( uint32_t nMaximum ) noexcept;
	uint32_t pumpCPU( uint32_t nMaximum ) noexcept;
	bool isActive( void ) const noexcept;

private:
	SIRTaskGraph m_Graph;
	SIRTaskScheduler *m_pScheduler;
	bool m_bInitialized;
	bool m_bActive;
};
#endif // __SIR_SIMULATIONSCHEDULE_H__
