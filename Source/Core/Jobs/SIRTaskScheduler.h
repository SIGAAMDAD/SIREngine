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

typedef struct sirTaskSchedulerConfig_s
{
	uint32_t nTaskCapacity;
	uint32_t nWorkerCount;
} sirTaskSchedulerConfig_t;

// First-stage Core executor. Task/worker storage is committed through the
// supplied memory manager and remains valid until that manager shuts down.
// No allocation occurs in submit/cancel/complete/release hot paths.
// A handle must be released after terminal completion to recycle its slot.
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
	bool submit( sirTaskFunction_t pFunction, void *pContext, sirTaskPriority_t ePriority,
	             sirTaskExecutor_t eExecutor, sirTaskHandle_t *pHandle ) noexcept;
	bool cancel( sirTaskHandle_t task ) noexcept;
	bool isCancellationRequested( sirTaskHandle_t task ) noexcept;
	sirTaskState_t getState( sirTaskHandle_t task ) noexcept;
	// Blocking wait is valid only from a non-worker thread and for CPU tasks.
	bool waitTask( sirTaskHandle_t task ) noexcept;
	bool releaseTask( sirTaskHandle_t task ) noexcept;
	// Executes owner-affine callbacks only on the initialization thread.
	uint32_t pumpMain( uint32_t nMaximumTasks ) noexcept;
	uint32_t getActiveCount( void ) noexcept;

private:
	static void workerEntry( void *pUserData ) noexcept;
	void workerLoop( void ) noexcept;
	bool isCurrentHandleLocked( sirTaskHandle_t task ) const noexcept;
	void enqueueLocked( uint32_t nSlot ) noexcept;
	uint32_t dequeueLocked( sirTaskExecutor_t eExecutor ) noexcept;
	void removeLocked( uint32_t nSlot ) noexcept;
	void finishTask( uint32_t nSlot, sirTaskResult_t eResult ) noexcept;

	SIRMutex m_Mutex;
	SIRConditionVariable m_Ready;
	SIRConditionVariable m_Finished;
	SIRMemoryManager *m_pMemory;
	sirTaskSlot_t *m_pSlots;
	uint32_t m_nCriticalStreak[2];
	uint32_t m_nNormalStreak[2];
	SIRThread *m_pWorkers;
	std::thread::id m_OwnerThread;
	uint32_t m_nHead[2][3];
	uint32_t m_nTail[2][3];
	uint32_t m_nFreeHead;
	uint32_t m_nTaskCapacity;
	uint32_t m_nWorkerCount;
	uint32_t m_nReadyCpu;
	uint32_t m_nActive;
	bool m_bInitialized;
	bool m_bClosing;
	bool m_bEverInitialized;
};

#endif // __SIRENGINE_SIRTASKSCHEDULER_H__
