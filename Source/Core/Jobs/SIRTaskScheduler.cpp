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
static constexpr uint32_t SIR_TASK_NONE = UINT32_MAX;

typedef struct sirTaskSlot_s
{
	sirTaskFunction_t pFunction;
	void *pContext;
	uint32_t nGeneration;
	uint32_t nNext;
	uint32_t nPrev;
	sirTaskState_t eState;
	sirTaskPriority_t ePriority;
	sirTaskExecutor_t eExecutor;
	bool bCancellationRequested;
} sirTaskSlot_t;

SIRTaskScheduler::SIRTaskScheduler( void ) noexcept
    : m_pMemory( nullptr ), m_pSlots( nullptr ), m_nCriticalStreak{}, m_nNormalStreak{}, m_pWorkers( nullptr ), m_OwnerThread(),
      m_nHead{}, m_nTail{}, m_nFreeHead( SIR_TASK_NONE ), m_nTaskCapacity( 0 ), m_nWorkerCount( 0 ), m_nReadyCpu( 0 ),
      m_nActive( 0 ), m_bInitialized( false ), m_bClosing( false ), m_bEverInitialized( false )
{
	for ( uint32_t iExecutor = 0; iExecutor < 2; ++iExecutor ) {
		for ( uint32_t iPriority = 0; iPriority < 3; ++iPriority ) {
			m_nHead[iExecutor][iPriority] = SIR_TASK_NONE;
			m_nTail[iExecutor][iPriority] = SIR_TASK_NONE;
		}
	}
}

SIRTaskScheduler::~SIRTaskScheduler()
{
	if ( m_bInitialized && !shutdown() ) {
		abort();
	}
}

bool SIRTaskScheduler::init( SIRMemoryManager *pMemory, const sirTaskSchedulerConfig_t &config ) noexcept
{
	if ( m_bEverInitialized || pMemory == nullptr || !pMemory->isInitialized() || config.nTaskCapacity == 0 ||
	     config.nWorkerCount == 0 || config.nTaskCapacity == UINT32_MAX ||
	     ( ( uint64_t )config.nTaskCapacity * sizeof( sirTaskSlot_t ) ) > SIZE_MAX ||
	     ( ( uint64_t )config.nWorkerCount * sizeof( SIRThread ) ) > SIZE_MAX ) {
		return false;
	}

	void *pSlotMemory = pMemory->allocateMetadata( ( uint64_t )config.nTaskCapacity * sizeof( sirTaskSlot_t ),
	                                            alignof( sirTaskSlot_t ), SIR_MEMORY_DOMAIN_CORE );
	void *pThreadMemory = pMemory->allocateMetadata( ( uint64_t )config.nWorkerCount * sizeof( SIRThread ), alignof( SIRThread ),
	                                              SIR_MEMORY_DOMAIN_CORE );
	if ( pSlotMemory == nullptr || pThreadMemory == nullptr ) {
		return false;
	}

	m_pMemory = pMemory;
	m_pSlots = ( sirTaskSlot_t * )pSlotMemory;
	m_pWorkers = ( SIRThread * )pThreadMemory;
	m_nTaskCapacity = config.nTaskCapacity;
	m_nWorkerCount = config.nWorkerCount;
	m_OwnerThread = std::this_thread::get_id();
	for ( uint32_t iSlot = 0; iSlot < m_nTaskCapacity; ++iSlot ) {
		sirTaskSlot_t &slot = m_pSlots[iSlot];
		slot.pFunction = nullptr;
		slot.pContext = nullptr;
		slot.nGeneration = 1;
		slot.nNext = iSlot + 1 == m_nTaskCapacity ? SIR_TASK_NONE : iSlot + 1;
		slot.nPrev = SIR_TASK_NONE;
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
		if ( !m_pWorkers[iWorker].start( &SIRTaskScheduler::workerEntry, this ) ) {
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
	m_bClosing = true;
	for ( uint32_t iSlot = 0; iSlot < m_nTaskCapacity; ++iSlot ) {
		sirTaskSlot_t &slot = m_pSlots[iSlot];
		if ( slot.eState == SIR_TASK_QUEUED ) {
			removeLocked( iSlot );
			slot.eState = SIR_TASK_CANCELLED;
			slot.bCancellationRequested = true;
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
	sirTaskSlot_t &slot = m_pSlots[nSlot];
	const uint32_t nExecutor = ( uint32_t )slot.eExecutor;
	const uint32_t nPriority = ( uint32_t )slot.ePriority;
	slot.nPrev = m_nTail[nExecutor][nPriority];
	slot.nNext = SIR_TASK_NONE;
	if ( slot.nPrev == SIR_TASK_NONE ) {
		m_nHead[nExecutor][nPriority] = nSlot;
	} else {
		m_pSlots[slot.nPrev].nNext = nSlot;
	}
	m_nTail[nExecutor][nPriority] = nSlot;
	if ( slot.eExecutor == SIR_TASK_EXEC_CPU ) {
		++m_nReadyCpu;
	}
}

void SIRTaskScheduler::removeLocked( uint32_t nSlot ) noexcept
{
	sirTaskSlot_t &slot = m_pSlots[nSlot];
	const uint32_t nExecutor = ( uint32_t )slot.eExecutor;
	const uint32_t nPriority = ( uint32_t )slot.ePriority;
	if ( slot.nPrev == SIR_TASK_NONE ) {
		m_nHead[nExecutor][nPriority] = slot.nNext;
	} else {
		m_pSlots[slot.nPrev].nNext = slot.nNext;
	}
	if ( slot.nNext == SIR_TASK_NONE ) {
		m_nTail[nExecutor][nPriority] = slot.nPrev;
	} else {
		m_pSlots[slot.nNext].nPrev = slot.nPrev;
	}
	slot.nPrev = SIR_TASK_NONE;
	slot.nNext = SIR_TASK_NONE;
	if ( slot.eExecutor == SIR_TASK_EXEC_CPU ) {
		--m_nReadyCpu;
	}
}

uint32_t SIRTaskScheduler::dequeueLocked( sirTaskExecutor_t eExecutor ) noexcept
{
	const uint32_t nExecutor = ( uint32_t )eExecutor;
	const bool bCritical = m_nHead[nExecutor][SIR_TASK_PRIORITY_CRITICAL] != SIR_TASK_NONE;
	const bool bNormal = m_nHead[nExecutor][SIR_TASK_PRIORITY_NORMAL] != SIR_TASK_NONE;
	const bool bBackground = m_nHead[nExecutor][SIR_TASK_PRIORITY_BACKGROUND] != SIR_TASK_NONE;
	uint32_t nPriority = SIR_TASK_PRIORITY_BACKGROUND;
	if ( bCritical && ( m_nCriticalStreak[nExecutor] < 8 || ( !bNormal && !bBackground ) ) ) {
		nPriority = SIR_TASK_PRIORITY_CRITICAL;
		++m_nCriticalStreak[nExecutor];
	} else if ( bNormal && ( m_nNormalStreak[nExecutor] < 4 || !bBackground ) ) {
		nPriority = SIR_TASK_PRIORITY_NORMAL;
		m_nCriticalStreak[nExecutor] = 0;
		++m_nNormalStreak[nExecutor];
	} else if ( bBackground ) {
		m_nCriticalStreak[nExecutor] = 0;
		m_nNormalStreak[nExecutor] = 0;
	} else if ( bCritical ) {
		nPriority = SIR_TASK_PRIORITY_CRITICAL;
		m_nCriticalStreak[nExecutor] = 1;
	} else if ( bNormal ) {
		nPriority = SIR_TASK_PRIORITY_NORMAL;
		m_nCriticalStreak[nExecutor] = 0;
		m_nNormalStreak[nExecutor] = 1;
	} else {
		return SIR_TASK_NONE;
	}
	const uint32_t nSlot = m_nHead[nExecutor][nPriority];
	removeLocked( nSlot );
	return nSlot;
}

bool SIRTaskScheduler::submit( sirTaskFunction_t pFunction, void *pContext, sirTaskPriority_t ePriority,
                               sirTaskExecutor_t eExecutor, sirTaskHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return false;
	}
	*pHandle = { SIR_TASK_NONE, 0 };
	if ( pFunction == nullptr || ePriority > SIR_TASK_PRIORITY_BACKGROUND || eExecutor > SIR_TASK_EXEC_MAIN ) {
		return false;
	}
	m_Mutex.lock();
	if ( !m_bInitialized || m_bClosing || m_nFreeHead == SIR_TASK_NONE ) {
		m_Mutex.unlock();
		return false;
	}
	const uint32_t nSlot = m_nFreeHead;
	sirTaskSlot_t &slot = m_pSlots[nSlot];
	m_nFreeHead = slot.nNext;
	slot.pFunction = pFunction;
	slot.pContext = pContext;
	slot.ePriority = ePriority;
	slot.eExecutor = eExecutor;
	slot.eState = SIR_TASK_QUEUED;
	slot.bCancellationRequested = false;
	*pHandle = { nSlot, slot.nGeneration };
	++m_nActive;
	enqueueLocked( nSlot );
	if ( eExecutor == SIR_TASK_EXEC_CPU ) {
		m_Ready.notifyOne();
	}
	m_Mutex.unlock();
	return true;
}

bool SIRTaskScheduler::cancel( sirTaskHandle_t task ) noexcept
{
	m_Mutex.lock();
	if ( !isCurrentHandleLocked( task ) ) {
		m_Mutex.unlock();
		return false;
	}
	sirTaskSlot_t &slot = m_pSlots[task.nSlot];
	if ( slot.eState == SIR_TASK_QUEUED ) {
		removeLocked( task.nSlot );
		slot.eState = SIR_TASK_CANCELLED;
		slot.bCancellationRequested = true;
		m_Finished.notifyAll();
	} else if ( slot.eState == SIR_TASK_RUNNING ) {
		slot.bCancellationRequested = true;
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

bool SIRTaskScheduler::waitTask( sirTaskHandle_t task ) noexcept
{
	if ( s_pRunningScheduler == this ) {
		return false;
	}
	m_Mutex.lock();
	if ( !isCurrentHandleLocked( task ) || m_pSlots[task.nSlot].eExecutor != SIR_TASK_EXEC_CPU ) {
		m_Mutex.unlock();
		return false;
	}
	while ( isCurrentHandleLocked( task ) &&
	        ( m_pSlots[task.nSlot].eState == SIR_TASK_QUEUED || m_pSlots[task.nSlot].eState == SIR_TASK_RUNNING ) ) {
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
	sirTaskSlot_t &slot = m_pSlots[task.nSlot];
	if ( slot.eState == SIR_TASK_RUNNING || slot.eState == SIR_TASK_QUEUED ) {
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

void SIRTaskScheduler::finishTask( uint32_t nSlot, sirTaskResult_t eResult ) noexcept
{
	m_Mutex.lock();
	sirTaskSlot_t &slot = m_pSlots[nSlot];
	slot.eState = slot.bCancellationRequested ? SIR_TASK_CANCELLED
	                                         : ( eResult == SIR_TASK_RESULT_SUCCESS ? SIR_TASK_COMPLETED : SIR_TASK_FAILED );
	m_Finished.notifyAll();
	m_Mutex.unlock();
}

uint32_t SIRTaskScheduler::pumpMain( uint32_t nMaximumTasks ) noexcept
{
	if ( std::this_thread::get_id() != m_OwnerThread || s_pRunningScheduler == this ) {
		return 0;
	}
	uint32_t nExecuted = 0;
	for ( ; nExecuted < nMaximumTasks; ++nExecuted ) {
		m_Mutex.lock();
		if ( !m_bInitialized || m_bClosing ) {
			m_Mutex.unlock();
			break;
		}
		const uint32_t nSlot = dequeueLocked( SIR_TASK_EXEC_MAIN );
		if ( nSlot == SIR_TASK_NONE ) {
			m_Mutex.unlock();
			break;
		}
		sirTaskSlot_t &slot = m_pSlots[nSlot];
		slot.eState = SIR_TASK_RUNNING;
		const sirTaskFunction_t pFunction = slot.pFunction;
		void *pContext = slot.pContext;
		const sirTaskHandle_t task = { nSlot, slot.nGeneration };
		m_Mutex.unlock();
		SIRTaskScheduler *pPrevious = s_pRunningScheduler;
		s_pRunningScheduler = this;
		const sirTaskResult_t eResult = pFunction( pContext, task );
		s_pRunningScheduler = pPrevious;
		finishTask( nSlot, eResult );
	}
	return nExecuted;
}

void SIRTaskScheduler::workerEntry( void *pUserData ) noexcept
{
	( ( SIRTaskScheduler * )pUserData )->workerLoop();
}

void SIRTaskScheduler::workerLoop( void ) noexcept
{
	s_pRunningScheduler = this;
	for ( ;; ) {
		m_Mutex.lock();
		while ( !m_bClosing && m_nReadyCpu == 0 ) {
			m_Ready.wait( m_Mutex );
		}
		if ( m_bClosing ) {
			m_Mutex.unlock();
			break;
		}
		const uint32_t nSlot = dequeueLocked( SIR_TASK_EXEC_CPU );
		sirTaskSlot_t &slot = m_pSlots[nSlot];
		slot.eState = SIR_TASK_RUNNING;
		const sirTaskFunction_t pFunction = slot.pFunction;
		void *pContext = slot.pContext;
		const sirTaskHandle_t task = { nSlot, slot.nGeneration };
		m_Mutex.unlock();
		const sirTaskResult_t eResult = pFunction( pContext, task );
		finishTask( nSlot, eResult );
	}
	s_pRunningScheduler = nullptr;
}

uint32_t SIRTaskScheduler::getActiveCount( void ) noexcept
{
	m_Mutex.lock();
	const uint32_t nCount = m_nActive;
	m_Mutex.unlock();
	return nCount;
}
