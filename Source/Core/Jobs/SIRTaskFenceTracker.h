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

#ifndef __SIRENGINE_SIRTASKFENCETRACKER_H__
#define __SIRENGINE_SIRTASKFENCETRACKER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Jobs/SIRTaskTypes.h>
#include <thread>

class SIRMemoryManager;
class SIRTaskScheduler;
typedef struct sirTaskFenceRecord_s sirTaskFenceRecord_t;

// Runs on the registered provider safe-point thread. A false return leaves
// the task pending and the finalizer may be retried. A successful finalizer
// must release all provider-side references before dependents can become ready.
typedef bool ( *sirTaskFenceFinalize_t )( void *pContext ) noexcept;

// Tracks monotonic D3D12/Vulkan-style completion fences for deferred provider
// tasks. All methods after init() are single-owner-thread operations. Nothing
// allocates during tracking, cancellation or completion. The provider owns
// native fences, commands and GRDG pins; this tracker owns only task tickets.
class SIRTaskFenceTracker
{
public:
	SIRTaskFenceTracker( void ) noexcept;
	~SIRTaskFenceTracker();
	SIRTaskFenceTracker( const SIRTaskFenceTracker & ) = delete;
	SIRTaskFenceTracker &operator=( const SIRTaskFenceTracker & ) = delete;

	// Must run on the provider thread. Metadata is retained in the SIR domain
	// until its manager is shut down; no hidden heap fallback.
	bool init( SIRMemoryManager *pMemory, SIRTaskScheduler *pScheduler, sirTaskExecutor_t eProvider, uint32_t nCapacity ) noexcept;
	// Refuses teardown while a provider still owns any tracked native work.
	bool shutdown( void ) noexcept;
	bool hasCapacity( void ) const noexcept;
	uint32_t getPendingCount( void ) const noexcept;

	// Register while the callback is RUNNING (before issuing native work), or
	// after it returns PENDING. Values must be nondecreasing and above the last
	// observed completion fence; several tasks may share one fence value.
	bool track( sirTaskHandle_t task, uint64_t nFenceValue, sirTaskFenceFinalize_t pfnFinalize = nullptr,
	            void *pContext = nullptr ) noexcept;
	// Roll back only the latest registration while its callback is still
	// RUNNING, if native submission failed and it will return FAILURE.
	bool untrackLast( sirTaskHandle_t task ) noexcept;
	// Call after observing native fence advancement, at the provider safe
	// point (never from a realtime callback). Acknowledgement makes dependent
	// tasks runnable only after pfnFinalize has returned true.
	bool advanceCompleted( uint64_t nCompletedFence, sirTaskResult_t eResult, uint32_t *pnAcknowledged ) noexcept;
	// Device-loss path. Valid only AFTER all native work has been quiesced;
	// invokes finalizers before failing remaining deferred tasks.
	bool failQuiesced( uint32_t *pnAcknowledged ) noexcept;

private:
	bool isOwner( void ) const noexcept;
	bool drain( uint64_t nCompletedFence, sirTaskResult_t eResult, uint32_t *pnAcknowledged ) noexcept;

	SIRTaskScheduler *m_pScheduler;
	sirTaskFenceRecord_t *m_pRecords;
	std::thread::id m_OwnerThread;
	sirTaskExecutor_t m_eProvider;
	uint64_t m_nLastTrackedFence;
	uint64_t m_nCompletedFence;
	uint32_t m_nCapacity;
	uint32_t m_nHead;
	uint32_t m_nPending;
	bool m_bDraining;
	bool m_bInitialized;
	bool m_bEverInitialized;
};

#endif // __SIRENGINE_SIRTASKFENCETRACKER_H__
