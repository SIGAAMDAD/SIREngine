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

#ifndef __SIRENGINE_SIRTASKSCHEDULER_H__
#define __SIRENGINE_SIRTASKSCHEDULER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Jobs/SIRTaskTypes.h>
#include <Core/Platform/SIRSync.h>
#include <Core/Platform/SIRThread.h>
#include <thread>

class SIRMemoryManager;
typedef struct sirTaskSlot_s sirTaskSlot_t;
typedef struct sirTaskEdge_s sirTaskEdge_t;
typedef struct sirTaskQueue_s
{
	uint32_t nHead[3];
	uint32_t nTail[3];
	uint32_t nCriticalStreak;
	uint32_t nNormalStreak;
} sirTaskQueue_t;
typedef struct sirTaskWorkerContext_s sirTaskWorkerContext_t;

typedef struct sirTaskSchedulerConfig_s
{
	uint32_t nTaskCapacity;
	uint32_t nWorkerCount;
	// 0 selects four edges per task slot. Allocation occurs at init only.
	uint32_t nDependencyCapacity = 0;
} sirTaskSchedulerConfig_t;

typedef struct sirTaskSchedulerStats_s
{
	uint64_t nSubmitted;
	uint64_t nExecuted;
	uint64_t nStealAttempts;
	uint64_t nStealSucceeded;
	uint32_t nReadyCpu;
	uint32_t nActive;
	uint32_t nAwaitingProvider;
} sirTaskSchedulerStats_t;

// Core executor with bounded task/prerequisite storage and worker-local queues.
// The memory manager supplies all storage during initialization; task and
// dependency hot paths do not allocate. Queue manipulation is serialized by
// m_Mutex, including local pops and steals (not a lock-free deque).
// Release terminal handles to recycle their generation-checked slots.
class SIRTaskScheduler
{
public:
	SIRTaskScheduler( void ) noexcept;
	~SIRTaskScheduler();
	SIRTaskScheduler( const SIRTaskScheduler & ) = delete;
	SIRTaskScheduler &operator=( const SIRTaskScheduler & ) = delete;

	bool init( SIRMemoryManager *pMemory, const sirTaskSchedulerConfig_t &config ) noexcept;
	// Must be called by the owning initialization thread, outside callbacks.
	bool shutdown( void ) noexcept;
	bool submit( sirTaskFunction_t pFunction, void *pContext, sirTaskPriority_t ePriority, sirTaskExecutor_t eExecutor,
	             sirTaskHandle_t *pHandle ) noexcept;
	// Prerequisites must be previously submitted tasks; successful terminal
	// prerequisites are already satisfied. Failed/cancelled ones cancel the
	// new task. Registration and publication are atomic under the scheduler lock.
	bool submitAfter( sirTaskFunction_t pFunction, void *pContext, sirTaskPriority_t ePriority, sirTaskExecutor_t eExecutor,
	                  const sirTaskHandle_t *pPrerequisites, uint32_t nPrerequisites, sirTaskHandle_t *pHandle ) noexcept;
	// Registers a complete acyclic batch behind one scheduler lock. No task
	// starts before every task and dependency is installed. Rejection leaves
	// capacity and pOutHandles unchanged. Edges must have before < after.
	bool submitBatch( const sirTaskBatchTask_t *pTasks, uint32_t nTasks, const sirTaskBatchEdge_t *pEdges, uint32_t nEdges,
	                  sirTaskHandle_t *pOutHandles ) noexcept;
	bool cancel( sirTaskHandle_t task ) noexcept;
	bool isCancellationRequested( sirTaskHandle_t task ) noexcept;
	sirTaskState_t getState( sirTaskHandle_t task ) noexcept;
	// Blocking wait is valid only from a non-worker thread and for CPU tasks.
	bool waitTask( sirTaskHandle_t task ) noexcept;
	bool releaseTask( sirTaskHandle_t task ) noexcept;
	// Executes owner-affine callbacks only on the initialization thread.
	uint32_t pumpMain( uint32_t nMaximumTasks ) noexcept;
	// Owner-thread CPU fallback for configurations with zero workers.
	uint32_t pumpCPU( uint32_t nMaximumTasks ) noexcept;
	// Each provider registers exactly one callback owner. The caller is the
	// render/physics/audio safe-point thread, never the realtime audio callback.
	bool bindProviderOwner( sirTaskExecutor_t eExecutor ) noexcept;
	uint32_t pumpProvider( sirTaskExecutor_t eExecutor, uint32_t nMaximumTasks ) noexcept;
	// Only the registered provider owner may signal a deferred task. Cancellation
	// keeps provider-owned resources alive until this acknowledgement arrives.
	bool completeProvider( sirTaskHandle_t task, sirTaskResult_t eResult ) noexcept;
	uint32_t getActiveCount( void ) noexcept;
	sirTaskSchedulerStats_t getStats( void ) noexcept;

private:
	static void workerEntry( void *pUserData ) noexcept;
	void workerLoop( uint32_t nWorker ) noexcept;
	bool isCurrentHandleLocked( sirTaskHandle_t task ) const noexcept;
	void enqueueLocked( uint32_t nSlot ) noexcept;
	uint32_t dequeueLocked( sirTaskExecutor_t eExecutor, uint32_t nWorker = UINT32_MAX ) noexcept;
	uint32_t dequeueQueueLocked( sirTaskQueue_t &queue, bool bFromTail ) noexcept;
	uint32_t selectPriorityLocked( sirTaskQueue_t &queue ) noexcept;
	void removeLocked( uint32_t nSlot ) noexcept;
	void finishTask( uint32_t nSlot, sirTaskResult_t eResult ) noexcept;
	void finishTaskLocked( uint32_t nSlot, sirTaskResult_t eResult ) noexcept;
	uint32_t pumpExecutor( sirTaskExecutor_t eExecutor, uint32_t nMaximumTasks ) noexcept;
	void unlinkEdgeLocked( uint32_t nEdge ) noexcept;
	void cancelTreeLocked( uint32_t nSlot ) noexcept;
	void resolveDependentsLocked( uint32_t nSlot, bool bSucceeded ) noexcept;

	SIRMutex m_Mutex;
	SIRConditionVariable m_Ready;
	SIRConditionVariable m_Finished;
	SIRMemoryManager *m_pMemory;
	sirTaskSlot_t *m_pSlots;
	sirTaskEdge_t *m_pEdges;
	sirTaskQueue_t *m_pLocalQueues;
	sirTaskWorkerContext_t *m_pWorkerContexts;
	SIRThread *m_pWorkers;
	std::thread::id m_OwnerThread;
	std::thread::id m_ProviderOwners[3];
	bool m_bProviderBound[3];
	sirTaskQueue_t m_GlobalQueues[SIR_TASK_EXEC_COUNT];
	uint32_t m_nFreeHead;
	uint32_t m_nFreeEdgeHead;
	uint32_t m_nDependencyCapacity;
	uint32_t m_nTaskCapacity;
	uint32_t m_nWorkerCount;
	uint32_t m_nReadyCpu;
	uint32_t m_nActive;
	uint32_t m_nPendingProviders;
	uint32_t m_nExecutingProviders;
	uint64_t m_nSubmitted;
	uint64_t m_nExecuted;
	uint64_t m_nStealAttempts;
	uint64_t m_nStealSucceeded;
	bool m_bInitialized;
	bool m_bClosing;
	bool m_bEverInitialized;
};

#endif // __SIRENGINE_SIRTASKSCHEDULER_H__
