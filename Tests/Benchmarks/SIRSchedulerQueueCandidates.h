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

#ifndef __SIRENGINE_SIRSCHEDULERQUEUECANDIDATES_H__
#define __SIRENGINE_SIRSCHEDULERQUEUECANDIDATES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <atomic>
#include <memory>
#include <mutex>
#include <stdint.h>

// Experiment-only, fixed-capacity queue candidates. Not production scheduler
// implementations and not interchangeable with generation-managed task slots.
typedef enum sirSchedulerQueueCandidate_e : uint8_t
{
	SIR_QUEUE_CANDIDATE_GLOBAL_LOCK = 0,
	SIR_QUEUE_CANDIDATE_LOCAL_LOCK = 1,
	SIR_QUEUE_CANDIDATE_CHASE_LEV = 2
} sirSchedulerQueueCandidate_t;

typedef struct alignas( 64 ) sirSchedulerQueueShard_s
{
	std::mutex mutex;
	std::unique_ptr<std::atomic<uint64_t>[]> pTokens;
	std::atomic<uint64_t> nTop{ 0 };
	std::atomic<uint64_t> nBottom{ 0 };
	uint32_t nHead = 0;
	uint32_t nCount = 0;
} sirSchedulerQueueShard_t;

class SIRSchedulerQueueCandidate
{
public:
	SIRSchedulerQueueCandidate( void ) noexcept = default;
	SIRSchedulerQueueCandidate( const SIRSchedulerQueueCandidate & ) = delete;
	SIRSchedulerQueueCandidate &operator=( const SIRSchedulerQueueCandidate & ) = delete;

	// Capacity must be power-of-two. Only shard owner pushes and pops;
	// unrelated threads may steal. The global-lock candidate deliberately
	// serializes all shards; the local-lock variant locks only a victim shard.
	bool init( sirSchedulerQueueCandidate_t eKind, uint32_t nWorkers, uint32_t nCapacity ) noexcept;
	bool push( uint32_t nOwner, uint64_t nToken ) noexcept;
	bool pop( uint32_t nOwner, uint64_t *pToken ) noexcept;
	bool steal( uint32_t nThief, uint32_t nVictim, uint64_t *pToken ) noexcept;
	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}
	uint32_t getWorkerCount( void ) const noexcept
	{
		return m_nWorkers;
	}

private:
	bool lockedPush( uint32_t nOwner, uint64_t nToken ) noexcept;
	bool lockedPop( uint32_t nOwner, uint64_t *pToken, bool bSteal ) noexcept;
	bool chasePush( uint32_t nOwner, uint64_t nToken ) noexcept;
	bool chasePop( uint32_t nOwner, uint64_t *pToken ) noexcept;
	bool chaseSteal( uint32_t nVictim, uint64_t *pToken ) noexcept;

	std::mutex m_GlobalMutex;
	std::unique_ptr<sirSchedulerQueueShard_t[]> m_pQueues;
	uint32_t m_nWorkers = 0;
	uint32_t m_nCapacity = 0;
	sirSchedulerQueueCandidate_t m_eKind = SIR_QUEUE_CANDIDATE_GLOBAL_LOCK;
};

#endif // __SIRENGINE_SIRSCHEDULERQUEUECANDIDATES_H__
