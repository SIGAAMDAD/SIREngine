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

#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>

#include <limits.h>
#include <new>
#include <stdlib.h>

// Thread-local ownership forbids worker-side blocking waits and teardown.
static thread_local SIRTaskScheduler *s_pRunningScheduler = nullptr;
static thread_local uint32_t s_nRunningWorker = UINT32_MAX;
static constexpr uint32_t SIR_TASK_NONE = UINT32_MAX;

typedef struct sirTaskSlot_s
{
	sirTaskFunction_t pFunction;
	void *pContext;
	uint32_t nGeneration;
	uint32_t nNext;
	uint32_t nPrev;
	uint32_t nQueueWorker;
	uint32_t nIncomingHead;
	uint32_t nOutgoingHead;
	uint32_t nRemaining;
	uint32_t nCascadeNext;
	bool bCascadeQueued;
	sirTaskState_t eState;
	sirTaskPriority_t ePriority;
	sirTaskExecutor_t eExecutor;
	bool bCancellationRequested;
} sirTaskSlot_t;

typedef struct sirTaskEdge_s
{
	uint32_t nParent;
	uint32_t nChild;
	uint32_t nParentPrev;
	uint32_t nParentNext;
	uint32_t nChildPrev;
	uint32_t nChildNext;
} sirTaskEdge_t;

typedef struct sirTaskWorkerContext_s
{
	SIRTaskScheduler *pScheduler;
	uint32_t nWorker;
} sirTaskWorkerContext_t;

static_assert( sizeof( sirTaskSlot_t ) <= 64, "Scheduler task metadata exceeds one cache line" );
static_assert( sizeof( sirTaskQueue_t ) <= 64, "Scheduler queue metadata exceeds one cache line" );

SIRTaskScheduler::SIRTaskScheduler( void ) noexcept
    : m_pMemory( nullptr ), m_pSlots( nullptr ), m_pEdges( nullptr ), m_pLocalQueues( nullptr ), m_pWorkerContexts( nullptr ),
      m_pWorkers( nullptr ), m_OwnerThread(), m_ProviderOwners{}, m_bProviderBound{}, m_GlobalQueues{}, m_nFreeHead( SIR_TASK_NONE ),
      m_nFreeEdgeHead( SIR_TASK_NONE ), m_nDependencyCapacity( 0 ), m_nTaskCapacity( 0 ), m_nWorkerCount( 0 ), m_nReadyCpu( 0 ),
      m_nActive( 0 ), m_nPendingProviders( 0 ), m_nExecutingProviders( 0 ), m_nSubmitted( 0 ), m_nExecuted( 0 ), m_nStealAttempts( 0 ),
      m_nStealSucceeded( 0 ), m_bInitialized( false ), m_bClosing( false ), m_bEverInitialized( false )
{
	for ( uint32_t iQueue = 0; iQueue < SIR_TASK_EXEC_COUNT; ++iQueue ) {
		for ( uint32_t iPriority = 0; iPriority < 3; ++iPriority ) {
			m_GlobalQueues[iQueue].nHead[iPriority] = SIR_TASK_NONE;
			m_GlobalQueues[iQueue].nTail[iPriority] = SIR_TASK_NONE;
		}
	}
}

SIRTaskScheduler::~SIRTaskScheduler()
{
	if ( m_bInitialized && !shutdown() ) {
		abort();
	}
}

bool SIRTaskScheduler::init( SIRMemoryManager *pMemory, const sirTaskSchedulerConfig_t& config ) noexcept
{
	if ( m_bEverInitialized || pMemory == nullptr || !pMemory->isInitialized() || config.nTaskCapacity == 0 ||
	     config.nTaskCapacity == UINT32_MAX || ( ( uint64_t )config.nTaskCapacity * sizeof( sirTaskSlot_t ) ) > SIZE_MAX ||
	     ( ( uint64_t )config.nWorkerCount * sizeof( SIRThread ) ) > SIZE_MAX ||
	     ( ( uint64_t )config.nWorkerCount * sizeof( sirTaskQueue_t ) ) > SIZE_MAX ||
	     ( ( uint64_t )config.nWorkerCount * sizeof( sirTaskWorkerContext_t ) ) > SIZE_MAX ) {
		return false;
	}

	const uint64_t nEdgeCapacity = config.nDependencyCapacity != 0 ? config.nDependencyCapacity : ( uint64_t )config.nTaskCapacity * 4;
	if ( nEdgeCapacity == 0 || nEdgeCapacity >= UINT32_MAX || nEdgeCapacity > SIZE_MAX / sizeof( sirTaskEdge_t ) ) {
		return false;
	}

	// The backing allocator provides stable, SIR-attributed scheduler metadata.
	// No allocations are performed from worker or dependency hot paths.
	void *pSlotMemory = pMemory->allocateMetadata( ( uint64_t )config.nTaskCapacity * sizeof( sirTaskSlot_t ), alignof( sirTaskSlot_t ),
	                                               SIR_MEMORY_DOMAIN_CORE );
	void *pThreadMemory = config.nWorkerCount == 0 ? nullptr
	                                               : pMemory->allocateMetadata( ( uint64_t )config.nWorkerCount * sizeof( SIRThread ),
	                                                                            alignof( SIRThread ), SIR_MEMORY_DOMAIN_CORE );
	void *pQueueMemory = config.nWorkerCount == 0 ? nullptr
	                                              : pMemory->allocateMetadata( ( uint64_t )config.nWorkerCount * sizeof( sirTaskQueue_t ),
	                                                                           alignof( sirTaskQueue_t ), SIR_MEMORY_DOMAIN_CORE );
	void *pContextMemory = config.nWorkerCount == 0
	                           ? nullptr
	                           : pMemory->allocateMetadata( ( uint64_t )config.nWorkerCount * sizeof( sirTaskWorkerContext_t ),
	                                                        alignof( sirTaskWorkerContext_t ), SIR_MEMORY_DOMAIN_CORE );
	void *pEdgeMemory =
	    pMemory->allocateMetadata( nEdgeCapacity * sizeof( sirTaskEdge_t ), alignof( sirTaskEdge_t ), SIR_MEMORY_DOMAIN_CORE );
	if ( pSlotMemory == nullptr ||
	     ( config.nWorkerCount != 0 && ( pThreadMemory == nullptr || pQueueMemory == nullptr || pContextMemory == nullptr ) ) ||
	     pEdgeMemory == nullptr ) {
		return false;
	}

	m_pMemory = pMemory;
	m_pSlots = ( sirTaskSlot_t * )pSlotMemory;
	m_pEdges = ( sirTaskEdge_t * )pEdgeMemory;
	m_nDependencyCapacity = ( uint32_t )nEdgeCapacity;
	for ( uint32_t iEdge = 0; iEdge < m_nDependencyCapacity; ++iEdge ) {
		m_pEdges[iEdge].nParentNext = iEdge + 1 == m_nDependencyCapacity ? SIR_TASK_NONE : iEdge + 1;
	}
	m_nFreeEdgeHead = 0;
	m_pWorkers = ( SIRThread * )pThreadMemory;
	m_pLocalQueues = ( sirTaskQueue_t * )pQueueMemory;
	m_pWorkerContexts = ( sirTaskWorkerContext_t * )pContextMemory;
	for ( uint32_t iWorker = 0; iWorker < config.nWorkerCount; ++iWorker ) {
		m_pWorkerContexts[iWorker] = { this, iWorker };
		m_pLocalQueues[iWorker].nCriticalStreak = 0;
		m_pLocalQueues[iWorker].nNormalStreak = 0;
		for ( uint32_t iPriority = 0; iPriority < 3; ++iPriority ) {
			m_pLocalQueues[iWorker].nHead[iPriority] = SIR_TASK_NONE;
			m_pLocalQueues[iWorker].nTail[iPriority] = SIR_TASK_NONE;
		}
	}
	m_nTaskCapacity = config.nTaskCapacity;
	m_nWorkerCount = config.nWorkerCount;
	m_OwnerThread = std::this_thread::get_id();
	for ( uint32_t iSlot = 0; iSlot < m_nTaskCapacity; ++iSlot ) {
		sirTaskSlot_t& slot = m_pSlots[iSlot];
		slot.pFunction = nullptr;
		slot.pContext = nullptr;
		slot.nGeneration = 1;
		slot.nNext = iSlot + 1 == m_nTaskCapacity ? SIR_TASK_NONE : iSlot + 1;
		slot.nPrev = SIR_TASK_NONE;
		slot.nQueueWorker = SIR_TASK_NONE;
		slot.nIncomingHead = SIR_TASK_NONE;
		slot.nOutgoingHead = SIR_TASK_NONE;
		slot.nRemaining = 0;
		slot.nCascadeNext = SIR_TASK_NONE;
		slot.bCascadeQueued = false;
		slot.eState = SIR_TASK_INVALID;
		slot.ePriority = SIR_TASK_PRIORITY_NORMAL;
		slot.eExecutor = SIR_TASK_EXEC_CPU;
		slot.bCancellationRequested = false;
	}
	m_nFreeHead = 0;
	m_bInitialized = true;
	m_bEverInitialized = true;
	for ( uint32_t iWorker = 0; iWorker < m_nWorkerCount; ++iWorker ) {
		new ( m_pWorkers + iWorker ) SIRThread();
	}
	for ( uint32_t iWorker = 0; iWorker < m_nWorkerCount; ++iWorker ) {
		if ( !m_pWorkers[iWorker].start( &SIRTaskScheduler::workerEntry, &m_pWorkerContexts[iWorker] ) ) {
			shutdown();
			return false;
		}
	}
	return true;
}

bool SIRTaskScheduler::shutdown( void ) noexcept
{
	if ( !m_bInitialized || s_pRunningScheduler == this || std::this_thread::get_id() != m_OwnerThread ) {
		return false;
	}
	m_Mutex.lock();
	// A provider may have queued GPU/physics/audio commands which still refer
	// to task context and SIR-owned memory. Never invalidate those resources.
	if ( m_nPendingProviders != 0 || m_nExecutingProviders != 0 ) {
		m_Mutex.unlock();
		return false;
	}
	m_bClosing = true;
	for ( uint32_t iSlot = 0; iSlot < m_nTaskCapacity; ++iSlot ) {
		sirTaskSlot_t& slot = m_pSlots[iSlot];
		if ( slot.eState == SIR_TASK_QUEUED || slot.eState == SIR_TASK_BLOCKED ) {
			cancelTreeLocked( iSlot );
		} else if ( slot.eState == SIR_TASK_RUNNING ) {
			slot.bCancellationRequested = true;
		}
	}
	m_Ready.notifyAll();
	m_Finished.notifyAll();
	m_Mutex.unlock();

	bool bJoined = true;
	for ( uint32_t iWorker = 0; iWorker < m_nWorkerCount; ++iWorker ) {
		if ( !m_pWorkers[iWorker].join() ) {
			bJoined = false;
		}
	}
	if ( !bJoined ) {
		return false;
	}
	for ( uint32_t iWorker = 0; iWorker < m_nWorkerCount; ++iWorker ) {
		m_pWorkers[iWorker].~SIRThread();
	}
	m_Mutex.lock();
	m_bInitialized = false;
	m_Finished.notifyAll();
	m_Mutex.unlock();
	return true;
}

bool SIRTaskScheduler::isCurrentHandleLocked( sirTaskHandle_t task ) const noexcept
{
	return m_bInitialized && task.nSlot < m_nTaskCapacity && task.nGeneration != 0 &&
	       m_pSlots[task.nSlot].nGeneration == task.nGeneration && m_pSlots[task.nSlot].eState != SIR_TASK_INVALID;
}

void SIRTaskScheduler::enqueueLocked( uint32_t nSlot ) noexcept
{
	sirTaskSlot_t& slot = m_pSlots[nSlot];
	const uint32_t nExecutor = ( uint32_t )slot.eExecutor;
	const uint32_t nPriority = ( uint32_t )slot.ePriority;
	// Tasks created or activated by a worker go to its local queue. External
	// producers use the shared ingress queue, preserving FIFO admission.
	const uint32_t nWorker = nExecutor == SIR_TASK_EXEC_CPU && s_pRunningScheduler == this && s_nRunningWorker < m_nWorkerCount
	                             ? s_nRunningWorker
	                             : SIR_TASK_NONE;
	sirTaskQueue_t& queue = nWorker == SIR_TASK_NONE ? m_GlobalQueues[nExecutor] : m_pLocalQueues[nWorker];
	slot.nQueueWorker = nWorker;
	slot.nPrev = queue.nTail[nPriority];
	slot.nNext = SIR_TASK_NONE;
	if ( slot.nPrev == SIR_TASK_NONE ) {
		queue.nHead[nPriority] = nSlot;
	} else {
		m_pSlots[slot.nPrev].nNext = nSlot;
	}
	queue.nTail[nPriority] = nSlot;
	if ( slot.eExecutor == SIR_TASK_EXEC_CPU ) {
		++m_nReadyCpu;
	}
}

void SIRTaskScheduler::removeLocked( uint32_t nSlot ) noexcept
{
	sirTaskSlot_t& slot = m_pSlots[nSlot];
	const uint32_t nExecutor = ( uint32_t )slot.eExecutor;
	const uint32_t nPriority = ( uint32_t )slot.ePriority;
	sirTaskQueue_t& queue = slot.nQueueWorker == SIR_TASK_NONE ? m_GlobalQueues[nExecutor] : m_pLocalQueues[slot.nQueueWorker];
	if ( slot.nPrev == SIR_TASK_NONE ) {
		queue.nHead[nPriority] = slot.nNext;
	} else {
		m_pSlots[slot.nPrev].nNext = slot.nNext;
	}
	if ( slot.nNext == SIR_TASK_NONE ) {
		queue.nTail[nPriority] = slot.nPrev;
	} else {
		m_pSlots[slot.nNext].nPrev = slot.nPrev;
	}
	slot.nPrev = SIR_TASK_NONE;
	slot.nNext = SIR_TASK_NONE;
	slot.nQueueWorker = SIR_TASK_NONE;
	if ( slot.eExecutor == SIR_TASK_EXEC_CPU ) {
		--m_nReadyCpu;
	}
}

uint32_t SIRTaskScheduler::selectPriorityLocked( sirTaskQueue_t& queue ) noexcept
{
	const bool bCritical = queue.nHead[SIR_TASK_PRIORITY_CRITICAL] != SIR_TASK_NONE;
	const bool bNormal = queue.nHead[SIR_TASK_PRIORITY_NORMAL] != SIR_TASK_NONE;
	const bool bBackground = queue.nHead[SIR_TASK_PRIORITY_BACKGROUND] != SIR_TASK_NONE;
	if ( bCritical && ( queue.nCriticalStreak < 8 || ( !bNormal && !bBackground ) ) ) {
		++queue.nCriticalStreak;
		return SIR_TASK_PRIORITY_CRITICAL;
	}
	if ( bNormal && ( queue.nNormalStreak < 4 || !bBackground ) ) {
		queue.nCriticalStreak = 0;
		++queue.nNormalStreak;
		return SIR_TASK_PRIORITY_NORMAL;
	}
	if ( bBackground ) {
		queue.nCriticalStreak = 0;
		queue.nNormalStreak = 0;
		return SIR_TASK_PRIORITY_BACKGROUND;
	}
	return SIR_TASK_NONE;
}

uint32_t SIRTaskScheduler::dequeueQueueLocked( sirTaskQueue_t& queue, bool bFromTail ) noexcept
{
	const uint32_t nPriority = selectPriorityLocked( queue );
	if ( nPriority == SIR_TASK_NONE ) {
		return SIR_TASK_NONE;
	}
	const uint32_t nSlot = bFromTail ? queue.nTail[nPriority] : queue.nHead[nPriority];
	removeLocked( nSlot );
	return nSlot;
}

uint32_t SIRTaskScheduler::dequeueLocked( sirTaskExecutor_t eExecutor, uint32_t nWorker ) noexcept
{
	if ( eExecutor != SIR_TASK_EXEC_CPU ) {
		return dequeueQueueLocked( m_GlobalQueues[eExecutor], false );
	}
	if ( nWorker < m_nWorkerCount ) {
		// LIFO improves local cache locality for spawned work. Stealers take
		// oldest tasks from the opposite end of the victim queue.
		uint32_t nTask = dequeueQueueLocked( m_pLocalQueues[nWorker], true );
		if ( nTask != SIR_TASK_NONE ) {
			return nTask;
		}
		nTask = dequeueQueueLocked( m_GlobalQueues[SIR_TASK_EXEC_CPU], false );
		if ( nTask != SIR_TASK_NONE ) {
			return nTask;
		}
		for ( uint32_t iOffset = 1; iOffset < m_nWorkerCount; ++iOffset ) {
			const uint32_t iVictim = ( nWorker + iOffset ) % m_nWorkerCount;
			++m_nStealAttempts;
			nTask = dequeueQueueLocked( m_pLocalQueues[iVictim], false );
			if ( nTask != SIR_TASK_NONE ) {
				++m_nStealSucceeded;
				return nTask;
			}
		}
		return SIR_TASK_NONE;
	}
	// Explicit owner-thread pump, used only when worker count is zero.
	return dequeueQueueLocked( m_GlobalQueues[SIR_TASK_EXEC_CPU], false );
}

bool SIRTaskScheduler::submit( const sirTaskBatchTask_t& task, sirTaskHandle_t *pHandle ) noexcept
{
	return submitAfter( task, nullptr, 0, pHandle );
}

bool SIRTaskScheduler::submitAfter( const sirTaskBatchTask_t& task, const sirTaskHandle_t *pPrerequisites,
                                    uint32_t nPrerequisites, sirTaskHandle_t *pHandle ) noexcept
{
	const sirTaskFunction_t pFunction = task.pFunction;
	void *pContext = task.pContext;
	const sirTaskPriority_t ePriority = task.ePriority;
	const sirTaskExecutor_t eExecutor = task.eExecutor;
	if ( pHandle == nullptr ) {
		return false;
	}
	*pHandle = { SIR_TASK_NONE, 0 };
	if ( pFunction == nullptr || ePriority > SIR_TASK_PRIORITY_BACKGROUND || eExecutor >= SIR_TASK_EXEC_COUNT ||
	     ( nPrerequisites != 0 && pPrerequisites == nullptr ) ) {
		return false;
	}
	m_Mutex.lock();
	if ( !m_bInitialized || m_bClosing || m_nFreeHead == SIR_TASK_NONE || nPrerequisites > m_nDependencyCapacity ) {
		m_Mutex.unlock();
		return false;
	}
	uint32_t nUnresolved = 0;
	bool bFailedPrerequisite = false;
	for ( uint32_t i = 0; i < nPrerequisites; ++i ) {
		if ( !isCurrentHandleLocked( pPrerequisites[i] ) ) {
			m_Mutex.unlock();
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( pPrerequisites[i].nSlot == pPrerequisites[j].nSlot ) {
				m_Mutex.unlock();
				return false;
			}
		}
		const sirTaskState_t eParent = m_pSlots[pPrerequisites[i].nSlot].eState;
		if ( eParent == SIR_TASK_FAILED || eParent == SIR_TASK_CANCELLED ) {
			bFailedPrerequisite = true;
		} else if ( eParent == SIR_TASK_BLOCKED || eParent == SIR_TASK_QUEUED || eParent == SIR_TASK_RUNNING ||
		            eParent == SIR_TASK_AWAITING_PROVIDER ) {
			++nUnresolved;
		}
	}
	uint32_t nAvailable = 0;
	for ( uint32_t nEdge = m_nFreeEdgeHead; nEdge != SIR_TASK_NONE && nAvailable < nUnresolved; nEdge = m_pEdges[nEdge].nParentNext ) {
		++nAvailable;
	}
	if ( !bFailedPrerequisite && nAvailable != nUnresolved ) {
		m_Mutex.unlock();
		return false;
	}
	const uint32_t nSlot = m_nFreeHead;
	sirTaskSlot_t& slot = m_pSlots[nSlot];
	m_nFreeHead = slot.nNext;
	slot.pFunction = pFunction;
	slot.pContext = pContext;
	slot.ePriority = ePriority;
	slot.eExecutor = eExecutor;
	slot.bCancellationRequested = bFailedPrerequisite;
	slot.nIncomingHead = SIR_TASK_NONE;
	slot.nOutgoingHead = SIR_TASK_NONE;
	slot.nRemaining = bFailedPrerequisite ? 0 : nUnresolved;
	slot.nCascadeNext = SIR_TASK_NONE;
	slot.bCascadeQueued = false;
	slot.eState = bFailedPrerequisite ? SIR_TASK_CANCELLED : ( nUnresolved == 0 ? SIR_TASK_QUEUED : SIR_TASK_BLOCKED );
	*pHandle = { nSlot, slot.nGeneration };
	++m_nActive;
	++m_nSubmitted;
	if ( !bFailedPrerequisite ) {
		for ( uint32_t i = 0; i < nPrerequisites; ++i ) {
			const uint32_t nParent = pPrerequisites[i].nSlot;
			const sirTaskState_t eParent = m_pSlots[nParent].eState;
			if ( eParent == SIR_TASK_COMPLETED ) {
				continue;
			}
			const uint32_t nEdge = m_nFreeEdgeHead;
			sirTaskEdge_t& edge = m_pEdges[nEdge];
			m_nFreeEdgeHead = edge.nParentNext;
			edge.nParent = nParent;
			edge.nChild = nSlot;
			edge.nParentPrev = SIR_TASK_NONE;
			edge.nParentNext = m_pSlots[nParent].nOutgoingHead;
			if ( edge.nParentNext != SIR_TASK_NONE ) {
				m_pEdges[edge.nParentNext].nParentPrev = nEdge;
			}
			m_pSlots[nParent].nOutgoingHead = nEdge;
			edge.nChildPrev = SIR_TASK_NONE;
			edge.nChildNext = slot.nIncomingHead;
			if ( edge.nChildNext != SIR_TASK_NONE ) {
				m_pEdges[edge.nChildNext].nChildPrev = nEdge;
			}
			slot.nIncomingHead = nEdge;
		}
		if ( slot.eState == SIR_TASK_QUEUED ) {
			enqueueLocked( nSlot );
			if ( eExecutor == SIR_TASK_EXEC_CPU ) {
				m_Ready.notifyOne();
			}
		}
	} else {
		m_Finished.notifyAll();
	}
	m_Mutex.unlock();
	return true;
}

bool SIRTaskScheduler::submitBatch( const sirTaskBatchDesc_t& batch, sirTaskHandle_t *pOutHandles ) noexcept
{
	const sirTaskBatchTask_t *pTasks = batch.pTasks;
	const uint32_t nTasks = batch.nTasks;
	const sirTaskBatchEdge_t *pEdges = batch.pEdges;
	const uint32_t nEdges = batch.nEdges;
	if ( pTasks == nullptr || pOutHandles == nullptr || nTasks == 0 || ( nEdges != 0 && pEdges == nullptr ) ) {
		return false;
	}
	// Reject malformed batches before acquiring the lock or modifying any slot.
	for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
		const sirTaskBatchTask_t& task = pTasks[iTask];
		if ( task.pFunction == nullptr || task.ePriority > SIR_TASK_PRIORITY_BACKGROUND || task.eExecutor >= SIR_TASK_EXEC_COUNT ) {
			return false;
		}
	}
	for ( uint32_t iEdge = 0; iEdge < nEdges; ++iEdge ) {
		if ( pEdges[iEdge].nBefore >= pEdges[iEdge].nAfter || pEdges[iEdge].nAfter >= nTasks ) {
			return false;
		}
	}

	m_Mutex.lock();
	if ( !m_bInitialized || m_bClosing || nTasks > m_nTaskCapacity - m_nActive || nEdges > m_nDependencyCapacity ) {
		m_Mutex.unlock();
		return false;
	}
	uint32_t nAvailableSlots = 0;
	for ( uint32_t nSlot = m_nFreeHead; nSlot != SIR_TASK_NONE && nAvailableSlots < nTasks; nSlot = m_pSlots[nSlot].nNext ) {
		++nAvailableSlots;
	}
	uint32_t nAvailableEdges = 0;
	for ( uint32_t nEdge = m_nFreeEdgeHead; nEdge != SIR_TASK_NONE && nAvailableEdges < nEdges; nEdge = m_pEdges[nEdge].nParentNext ) {
		++nAvailableEdges;
	}
	if ( nAvailableSlots != nTasks || nAvailableEdges != nEdges ) {
		m_Mutex.unlock();
		return false;
	}

	// pOutHandles is also the temporary slot mapping. The caller must provide
	// distinct, writable storage that does not alias pTasks or pEdges.
	for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
		const uint32_t nSlot = m_nFreeHead;
		sirTaskSlot_t& slot = m_pSlots[nSlot];
		m_nFreeHead = slot.nNext;
		slot.pFunction = pTasks[iTask].pFunction;
		slot.pContext = pTasks[iTask].pContext;
		slot.ePriority = pTasks[iTask].ePriority;
		slot.eExecutor = pTasks[iTask].eExecutor;
		slot.nIncomingHead = SIR_TASK_NONE;
		slot.nOutgoingHead = SIR_TASK_NONE;
		slot.nRemaining = 0;
		slot.nCascadeNext = SIR_TASK_NONE;
		slot.bCascadeQueued = false;
		slot.bCancellationRequested = false;
		slot.nQueueWorker = SIR_TASK_NONE;
		slot.nPrev = SIR_TASK_NONE;
		slot.nNext = SIR_TASK_NONE;
		slot.eState = SIR_TASK_BLOCKED;
		pOutHandles[iTask] = { nSlot, slot.nGeneration };
	}
	for ( uint32_t iEdge = 0; iEdge < nEdges; ++iEdge ) {
		const uint32_t nParent = pOutHandles[pEdges[iEdge].nBefore].nSlot;
		const uint32_t nChild = pOutHandles[pEdges[iEdge].nAfter].nSlot;
		const uint32_t nEdge = m_nFreeEdgeHead;
		sirTaskEdge_t& edge = m_pEdges[nEdge];
		m_nFreeEdgeHead = edge.nParentNext;
		edge.nParent = nParent;
		edge.nChild = nChild;
		edge.nParentPrev = SIR_TASK_NONE;
		edge.nParentNext = m_pSlots[nParent].nOutgoingHead;
		if ( edge.nParentNext != SIR_TASK_NONE ) {
			m_pEdges[edge.nParentNext].nParentPrev = nEdge;
		}
		m_pSlots[nParent].nOutgoingHead = nEdge;
		edge.nChildPrev = SIR_TASK_NONE;
		edge.nChildNext = m_pSlots[nChild].nIncomingHead;
		if ( edge.nChildNext != SIR_TASK_NONE ) {
			m_pEdges[edge.nChildNext].nChildPrev = nEdge;
		}
		m_pSlots[nChild].nIncomingHead = nEdge;
		++m_pSlots[nChild].nRemaining;
	}
	bool bReadyCpu = false;
	for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
		const uint32_t nSlot = pOutHandles[iTask].nSlot;
		if ( m_pSlots[nSlot].nRemaining == 0 ) {
			m_pSlots[nSlot].eState = SIR_TASK_QUEUED;
			enqueueLocked( nSlot );
			bReadyCpu |= m_pSlots[nSlot].eExecutor == SIR_TASK_EXEC_CPU;
		}
	}
	m_nActive += nTasks;
	m_nSubmitted += nTasks;
	if ( bReadyCpu ) {
		m_Ready.notifyAll();
	}
	m_Mutex.unlock();
	return true;
}

void SIRTaskScheduler::unlinkEdgeLocked( uint32_t nEdge ) noexcept
{
	sirTaskEdge_t& edge = m_pEdges[nEdge];
	sirTaskSlot_t& parent = m_pSlots[edge.nParent];
	sirTaskSlot_t& child = m_pSlots[edge.nChild];
	if ( edge.nParentPrev == SIR_TASK_NONE ) {
		parent.nOutgoingHead = edge.nParentNext;
	} else {
		m_pEdges[edge.nParentPrev].nParentNext = edge.nParentNext;
	}
	if ( edge.nParentNext != SIR_TASK_NONE ) {
		m_pEdges[edge.nParentNext].nParentPrev = edge.nParentPrev;
	}
	if ( edge.nChildPrev == SIR_TASK_NONE ) {
		child.nIncomingHead = edge.nChildNext;
	} else {
		m_pEdges[edge.nChildPrev].nChildNext = edge.nChildNext;
	}
	if ( edge.nChildNext != SIR_TASK_NONE ) {
		m_pEdges[edge.nChildNext].nChildPrev = edge.nChildPrev;
	}
	--child.nRemaining;
	edge.nParentNext = m_nFreeEdgeHead;
	m_nFreeEdgeHead = nEdge;
}

void SIRTaskScheduler::cancelTreeLocked( uint32_t nSlot ) noexcept
{
	// Iterative queue avoids recursion on long dependency chains.
	uint32_t nHead = nSlot;
	uint32_t nTail = nSlot;
	m_pSlots[nSlot].bCascadeQueued = true;
	m_pSlots[nSlot].nCascadeNext = SIR_TASK_NONE;
	while ( nHead != SIR_TASK_NONE ) {
		const uint32_t nCurrent = nHead;
		sirTaskSlot_t& slot = m_pSlots[nCurrent];
		nHead = slot.nCascadeNext;
		slot.nCascadeNext = SIR_TASK_NONE;
		slot.bCascadeQueued = false;
		if ( slot.eState != SIR_TASK_BLOCKED && slot.eState != SIR_TASK_QUEUED ) {
			continue;
		}
		if ( slot.eState == SIR_TASK_QUEUED ) {
			removeLocked( nCurrent );
		}
		slot.eState = SIR_TASK_CANCELLED;
		slot.bCancellationRequested = true;
		while ( slot.nIncomingHead != SIR_TASK_NONE ) {
			unlinkEdgeLocked( slot.nIncomingHead );
		}
		while ( slot.nOutgoingHead != SIR_TASK_NONE ) {
			const uint32_t nEdge = slot.nOutgoingHead;
			const uint32_t nChild = m_pEdges[nEdge].nChild;
			unlinkEdgeLocked( nEdge );
			sirTaskSlot_t& child = m_pSlots[nChild];
			if ( !child.bCascadeQueued && ( child.eState == SIR_TASK_BLOCKED || child.eState == SIR_TASK_QUEUED ) ) {
				child.bCascadeQueued = true;
				child.nCascadeNext = SIR_TASK_NONE;
				if ( nHead == SIR_TASK_NONE ) {
					nHead = nChild;
				} else {
					m_pSlots[nTail].nCascadeNext = nChild;
				}
				nTail = nChild;
			}
		}
	}
	m_Finished.notifyAll();
}

void SIRTaskScheduler::resolveDependentsLocked( uint32_t nSlot, bool bSucceeded ) noexcept
{
	while ( m_pSlots[nSlot].nOutgoingHead != SIR_TASK_NONE ) {
		const uint32_t nEdge = m_pSlots[nSlot].nOutgoingHead;
		const uint32_t nChild = m_pEdges[nEdge].nChild;
		unlinkEdgeLocked( nEdge );
		if ( !bSucceeded ) {
			cancelTreeLocked( nChild );
		} else if ( m_pSlots[nChild].nRemaining == 0 && m_pSlots[nChild].eState == SIR_TASK_BLOCKED ) {
			m_pSlots[nChild].eState = SIR_TASK_QUEUED;
			enqueueLocked( nChild );
			if ( m_pSlots[nChild].eExecutor == SIR_TASK_EXEC_CPU ) {
				m_Ready.notifyOne();
			}
		}
	}
}

bool SIRTaskScheduler::cancel( sirTaskHandle_t task ) noexcept
{
	m_Mutex.lock();
	if ( !isCurrentHandleLocked( task ) ) {
		m_Mutex.unlock();
		return false;
	}
	sirTaskSlot_t& slot = m_pSlots[task.nSlot];
	if ( slot.eState == SIR_TASK_QUEUED || slot.eState == SIR_TASK_BLOCKED ) {
		cancelTreeLocked( task.nSlot );
	} else if ( slot.eState == SIR_TASK_RUNNING || slot.eState == SIR_TASK_AWAITING_PROVIDER ) {
		slot.bCancellationRequested = true;
	} else {
		m_Mutex.unlock();
		return false;
	}
	m_Mutex.unlock();
	return true;
}

bool SIRTaskScheduler::isCancellationRequested( sirTaskHandle_t task ) noexcept
{
	m_Mutex.lock();
	const bool bCanceled = isCurrentHandleLocked( task ) && m_pSlots[task.nSlot].bCancellationRequested;
	m_Mutex.unlock();
	return bCanceled;
}

sirTaskState_t SIRTaskScheduler::getState( sirTaskHandle_t task ) noexcept
{
	m_Mutex.lock();
	const sirTaskState_t eState = isCurrentHandleLocked( task ) ? m_pSlots[task.nSlot].eState : SIR_TASK_INVALID;
	m_Mutex.unlock();
	return eState;
}

bool SIRTaskScheduler::queryTask( sirTaskHandle_t task, sirTaskState_t *pState, sirTaskExecutor_t *pExecutor ) noexcept
{
	if ( pState == nullptr || pExecutor == nullptr ) {
		return false;
	}
	m_Mutex.lock();
	if ( !isCurrentHandleLocked( task ) ) {
		m_Mutex.unlock();
		return false;
	}
	const sirTaskSlot_t& slot = m_pSlots[task.nSlot];
	*pState = slot.eState;
	*pExecutor = slot.eExecutor;
	m_Mutex.unlock();
	return true;
}

bool SIRTaskScheduler::waitTask( sirTaskHandle_t task ) noexcept
{
	if ( s_pRunningScheduler == this || m_nWorkerCount == 0 ) {
		return false;
	}
	m_Mutex.lock();
	if ( !isCurrentHandleLocked( task ) || m_pSlots[task.nSlot].eExecutor != SIR_TASK_EXEC_CPU ) {
		m_Mutex.unlock();
		return false;
	}
	while ( isCurrentHandleLocked( task ) &&
	        ( m_pSlots[task.nSlot].eState == SIR_TASK_QUEUED || m_pSlots[task.nSlot].eState == SIR_TASK_BLOCKED ||
	          m_pSlots[task.nSlot].eState == SIR_TASK_RUNNING ) ) {
		m_Finished.wait( m_Mutex );
	}
	const bool bTerminal = isCurrentHandleLocked( task );
	m_Mutex.unlock();
	return bTerminal;
}

bool SIRTaskScheduler::releaseTask( sirTaskHandle_t task ) noexcept
{
	m_Mutex.lock();
	if ( !isCurrentHandleLocked( task ) ) {
		m_Mutex.unlock();
		return false;
	}
	sirTaskSlot_t& slot = m_pSlots[task.nSlot];
	if ( slot.eState == SIR_TASK_RUNNING || slot.eState == SIR_TASK_QUEUED || slot.eState == SIR_TASK_BLOCKED ||
	     slot.eState == SIR_TASK_AWAITING_PROVIDER || slot.nIncomingHead != SIR_TASK_NONE || slot.nOutgoingHead != SIR_TASK_NONE ) {
		m_Mutex.unlock();
		return false;
	}
	slot.eState = SIR_TASK_INVALID;
	slot.pFunction = nullptr;
	slot.pContext = nullptr;
	slot.nGeneration = slot.nGeneration == UINT32_MAX ? 1 : slot.nGeneration + 1;
	slot.nNext = m_nFreeHead;
	slot.nPrev = SIR_TASK_NONE;
	m_nFreeHead = task.nSlot;
	--m_nActive;
	m_Finished.notifyAll();
	m_Mutex.unlock();
	return true;
}

void SIRTaskScheduler::finishTaskLocked( uint32_t nSlot, sirTaskResult_t eResult ) noexcept
{
	sirTaskSlot_t& slot = m_pSlots[nSlot];
	slot.eState =
	    slot.bCancellationRequested ? SIR_TASK_CANCELLED : ( eResult == SIR_TASK_RESULT_SUCCESS ? SIR_TASK_COMPLETED : SIR_TASK_FAILED );
	++m_nExecuted;
	resolveDependentsLocked( nSlot, slot.eState == SIR_TASK_COMPLETED && !m_bClosing );
	m_Finished.notifyAll();
}

void SIRTaskScheduler::finishTask( uint32_t nSlot, sirTaskResult_t eResult ) noexcept
{
	m_Mutex.lock();
	finishTaskLocked( nSlot, eResult );
	m_Mutex.unlock();
}

uint32_t SIRTaskScheduler::pumpMain( uint32_t nMaximumTasks ) noexcept
{
	return pumpExecutor( SIR_TASK_EXEC_MAIN, nMaximumTasks );
}

uint32_t SIRTaskScheduler::pumpCPU( uint32_t nMaximumTasks ) noexcept
{
	if ( m_nWorkerCount != 0 ) {
		return 0;
	}
	return pumpExecutor( SIR_TASK_EXEC_CPU, nMaximumTasks );
}

bool SIRTaskScheduler::bindProviderOwner( sirTaskExecutor_t eExecutor ) noexcept
{
	if ( eExecutor < SIR_TASK_EXEC_RENDER || eExecutor >= SIR_TASK_EXEC_COUNT || s_pRunningScheduler == this ) {
		return false;
	}
	m_Mutex.lock();
	const uint32_t nProvider = ( uint32_t )eExecutor - SIR_TASK_EXEC_RENDER;
	const bool bAllowed =
	    m_bInitialized && !m_bClosing && ( !m_bProviderBound[nProvider] || m_ProviderOwners[nProvider] == std::this_thread::get_id() );
	if ( bAllowed ) {
		m_bProviderBound[nProvider] = true;
		m_ProviderOwners[nProvider] = std::this_thread::get_id();
	}
	m_Mutex.unlock();
	return bAllowed;
}

uint32_t SIRTaskScheduler::pumpProvider( sirTaskExecutor_t eExecutor, uint32_t nMaximumTasks ) noexcept
{
	if ( eExecutor < SIR_TASK_EXEC_RENDER || eExecutor >= SIR_TASK_EXEC_COUNT ) {
		return 0;
	}
	return pumpExecutor( eExecutor, nMaximumTasks );
}

bool SIRTaskScheduler::completeProvider( sirTaskHandle_t task, sirTaskResult_t eResult ) noexcept
{
	if ( eResult != SIR_TASK_RESULT_SUCCESS && eResult != SIR_TASK_RESULT_FAILURE ) {
		return false;
	}
	m_Mutex.lock();
	if ( !isCurrentHandleLocked( task ) ) {
		m_Mutex.unlock();
		return false;
	}
	sirTaskSlot_t& slot = m_pSlots[task.nSlot];
	const uint32_t nExecutor = ( uint32_t )slot.eExecutor;
	if ( nExecutor < SIR_TASK_EXEC_RENDER || nExecutor >= SIR_TASK_EXEC_COUNT || slot.eState != SIR_TASK_AWAITING_PROVIDER ||
	     !m_bProviderBound[nExecutor - SIR_TASK_EXEC_RENDER] ||
	     m_ProviderOwners[nExecutor - SIR_TASK_EXEC_RENDER] != std::this_thread::get_id() ) {
		m_Mutex.unlock();
		return false;
	}
	--m_nPendingProviders;
	finishTaskLocked( task.nSlot, eResult );
	m_Mutex.unlock();
	return true;
}

uint32_t SIRTaskScheduler::pumpExecutor( sirTaskExecutor_t eExecutor, uint32_t nMaximumTasks ) noexcept
{
	if ( s_pRunningScheduler == this ) {
		return 0;
	}
	m_Mutex.lock();
	const bool bOwner = eExecutor < SIR_TASK_EXEC_RENDER
	                        ? std::this_thread::get_id() == m_OwnerThread
	                        : eExecutor < SIR_TASK_EXEC_COUNT && m_bProviderBound[eExecutor - SIR_TASK_EXEC_RENDER] &&
	                              std::this_thread::get_id() == m_ProviderOwners[eExecutor - SIR_TASK_EXEC_RENDER];
	m_Mutex.unlock();
	if ( !bOwner ) {
		return 0;
	}
	uint32_t nExecuted = 0;
	for ( ; nExecuted < nMaximumTasks; ++nExecuted ) {
		m_Mutex.lock();
		if ( !m_bInitialized || m_bClosing ) {
			m_Mutex.unlock();
			break;
		}
		const uint32_t nSlot = dequeueLocked( eExecutor );
		if ( nSlot == SIR_TASK_NONE ) {
			m_Mutex.unlock();
			break;
		}
		sirTaskSlot_t& slot = m_pSlots[nSlot];
		slot.eState = SIR_TASK_RUNNING;
		const sirTaskFunction_t pFunction = slot.pFunction;
		void *pContext = slot.pContext;
		const sirTaskHandle_t task = { nSlot, slot.nGeneration };
		const bool bProvider = eExecutor >= SIR_TASK_EXEC_RENDER;
		if ( bProvider ) {
			++m_nExecutingProviders;
		}
		m_Mutex.unlock();
		SIRTaskScheduler *pPrevious = s_pRunningScheduler;
		s_pRunningScheduler = this;
		const sirTaskResult_t eResult = pFunction( pContext, task );
		s_pRunningScheduler = pPrevious;
		if ( bProvider ) {
			m_Mutex.lock();
			if ( eResult == SIR_TASK_RESULT_PENDING ) {
				slot.eState = SIR_TASK_AWAITING_PROVIDER;
				++m_nPendingProviders;
			} else {
				finishTaskLocked( nSlot, eResult );
			}
			--m_nExecutingProviders;
			m_Mutex.unlock();
		} else {
			finishTask( nSlot, eResult );
		}
	}
	return nExecuted;
}

void SIRTaskScheduler::workerEntry( void *pUserData ) noexcept
{
	sirTaskWorkerContext_t *pContext = ( sirTaskWorkerContext_t * )pUserData;
	pContext->pScheduler->workerLoop( pContext->nWorker );
}

void SIRTaskScheduler::workerLoop( uint32_t nWorker ) noexcept
{
	s_pRunningScheduler = this;
	s_nRunningWorker = nWorker;
	for ( ;; ) {
		m_Mutex.lock();
		while ( !m_bClosing && m_nReadyCpu == 0 ) {
			m_Ready.wait( m_Mutex );
		}
		if ( m_bClosing ) {
			m_Mutex.unlock();
			break;
		}
		const uint32_t nSlot = dequeueLocked( SIR_TASK_EXEC_CPU, nWorker );
		sirTaskSlot_t& slot = m_pSlots[nSlot];
		slot.eState = SIR_TASK_RUNNING;
		const sirTaskFunction_t pFunction = slot.pFunction;
		void *pContext = slot.pContext;
		const sirTaskHandle_t task = { nSlot, slot.nGeneration };
		m_Mutex.unlock();
		const sirTaskResult_t eResult = pFunction( pContext, task );
		finishTask( nSlot, eResult );
	}
	s_nRunningWorker = SIR_TASK_NONE;
	s_pRunningScheduler = nullptr;
}

uint32_t SIRTaskScheduler::getActiveCount( void ) noexcept
{
	m_Mutex.lock();
	const uint32_t nCount = m_nActive;
	m_Mutex.unlock();
	return nCount;
}

sirTaskSchedulerStats_t SIRTaskScheduler::getStats( void ) noexcept
{
	m_Mutex.lock();
	const sirTaskSchedulerStats_t stats = { m_nSubmitted, m_nExecuted, m_nStealAttempts,   m_nStealSucceeded,
	                                        m_nReadyCpu,  m_nActive,   m_nPendingProviders };
	m_Mutex.unlock();
	return stats;
}
