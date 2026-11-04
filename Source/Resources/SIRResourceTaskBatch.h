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

#ifndef __SIRENGINE_SIRRESOURCETASKBATCH_H__
#define __SIRENGINE_SIRRESOURCETASKBATCH_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIRMPSCEventQueue.h>
#include <Resources/SIRResourceGraph.h>

// Worker descriptors contain identity and an expected revision, never
// borrowed canonical payload pointers or read-scope objects.
typedef struct sirResourceTaskWorkItem_s
{
	SIRResourceHandle hResource;
	uint64_t nExpectedRevision;
	uint64_t nTicket;
} sirResourceTaskWorkItem_t;

typedef struct sirResourceTaskCompletion_s
{
	uint64_t nTicket;
	SIRResourceResult eResult;
} sirResourceTaskCompletion_t;

typedef struct sirResourceTaskCallbacks_s
{
	void *pContext;
	// Return true only after the executor accepts responsibility for one
	// eventual completion. A rejected submission must produce no completion.
	bool ( *pfnSubmit )( void *pContext, const sirResourceTaskWorkItem_t &rWork ) noexcept;
} sirResourceTaskCallbacks_t;

typedef struct sirResourceTaskConfig_s
{
	SIRResourceAllocator allocator;
	uint32_t nMetadataDomain;
	uint32_t nMaxTasks;
	uint32_t nMaxEdges;
	uint32_t nMaxInFlight;
	sirResourceTaskCallbacks_t callbacks;
} sirResourceTaskConfig_t;

typedef enum sirResourceTaskState_e : uint8_t
{
	SIR_RESOURCE_TASK_WAITING = 0,
	SIR_RESOURCE_TASK_SUBMITTED,
	SIR_RESOURCE_TASK_SUCCEEDED,
	SIR_RESOURCE_TASK_FAILED,
	SIR_RESOURCE_TASK_SKIPPED
} sirResourceTaskState_t;

typedef struct sirResourceTaskStats_s
{
	uint32_t nTasks;
	uint32_t nEdges;
	uint32_t nInFlight;
	uint32_t nCompleted;
	uint32_t nFailed;
	uint32_t nSkipped;
	uint32_t nSubmitRejected;
	uint32_t nIgnoredCompletions;
	uint32_t nBatchGeneration;
	bool bActive;
	bool bCancelled;
} sirResourceTaskStats_t;

typedef struct sirResourceTaskNode_s sirResourceTaskNode_t;
typedef struct sirResourceTaskEdge_s sirResourceTaskEdge_t;

// A single-writer orchestration bridge to an external job executor.
// Ready tasks may be submitted concurrently; completions arrive through the
// existing Core MPSC ring. Call start/dispatch/drain/cancel only from the GRDG
// writer thread, and stop/join completion producers before destroying this
// object. Cancellation never releases in-flight tasks before they complete.
class SIRResourceTaskBatch
{
private:
	static constexpr uint32_t COMPLETION_CAPACITY = 256;
	SIRResourceGraph *m_pGraph;
	SIRResourceGraphWriter *m_pWriter;
	sirResourceTaskConfig_t m_Config;
	std::thread::id m_OwnerThread;
	sirResourceTaskNode_t *m_pNodes;
	sirResourceTaskEdge_t *m_pEdges;
	SIRResourceDependencyPlanEntry *m_pPlan;
	SIRMPSCEventQueue<sirResourceTaskCompletion_t, COMPLETION_CAPACITY> m_Completions;
	sirResourceTaskStats_t m_Stats;
	uint32_t m_nMaxInFlight;
	bool m_bReady;
	bool m_bDispatching;

	bool isWriterThread( void ) const noexcept;
	bool isComplete( void ) const noexcept;
	void skipPending( void ) noexcept;
	void releaseMetadata( void ) noexcept;
	SIRResourceResult buildEdges( void ) noexcept;
	SIRResourceResult validateWorkItem( const sirResourceTaskWorkItem_t &rWork ) noexcept;

public:
	SIRResourceTaskBatch( SIRResourceGraph &rGraph, const sirResourceTaskConfig_t &rConfig ) noexcept;
	~SIRResourceTaskBatch( void ) noexcept;
	SIRResourceTaskBatch( const SIRResourceTaskBatch & ) = delete;
	SIRResourceTaskBatch &operator=( const SIRResourceTaskBatch & ) = delete;

	bool isReady( void ) const noexcept;
	SIRResourceResult start( SIRResourceHandle hRoot ) noexcept;
	SIRResourceResult dispatchReady( uint32_t nBudget, uint32_t *pSubmitted ) noexcept;
	// Called by workers. FULL means the worker MUST retry while the batch
	// and queue are still alive; never discard an accepted job's completion.
	sirEventEnqueueStatus_t tryPostCompletion( const sirResourceTaskCompletion_t &rCompletion ) noexcept;
	SIRResourceResult drainCompletions( uint32_t nBudget, uint32_t *pConsumed ) noexcept;
	SIRResourceResult cancel( void ) noexcept;
	SIRResourceResult getStats( sirResourceTaskStats_t *pStats ) const noexcept;
};

#endif // __SIRENGINE_SIRRESOURCETASKBATCH_H__
