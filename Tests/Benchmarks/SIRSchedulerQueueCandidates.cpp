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

#include "SIRSchedulerQueueCandidates.h"

#include <limits.h>
#include <new>

bool SIRSchedulerQueueCandidate::init( sirSchedulerQueueCandidate_t eKind, uint32_t nWorkers, uint32_t nCapacity ) noexcept
{
	if ( m_pQueues || nWorkers == 0 || nCapacity < 2 || ( nCapacity & ( nCapacity - 1 ) ) != 0 || eKind > SIR_QUEUE_CANDIDATE_CHASE_LEV ) {
		return false;
	}
	std::unique_ptr<sirSchedulerQueueShard_t[]> pQueues( new ( std::nothrow ) sirSchedulerQueueShard_t[nWorkers] );
	if ( !pQueues ) {
		return false;
	}
	for ( uint32_t i = 0; i < nWorkers; ++i ) {
		pQueues[i].pTokens.reset( new ( std::nothrow ) std::atomic<uint64_t>[nCapacity] );
		if ( !pQueues[i].pTokens ) {
			return false;
		}
		for ( uint32_t j = 0; j < nCapacity; ++j ) {
			pQueues[i].pTokens[j].store( 0, std::memory_order_relaxed );
		}
	}
	m_pQueues = std::move( pQueues );
	m_nWorkers = nWorkers;
	m_nCapacity = nCapacity;
	m_eKind = eKind;
	return true;
}

bool SIRSchedulerQueueCandidate::lockedPush( uint32_t nOwner, uint64_t nToken ) noexcept
{
	sirSchedulerQueueShard_t& queue = m_pQueues[nOwner];
	std::unique_lock<std::mutex> lock( m_eKind == SIR_QUEUE_CANDIDATE_GLOBAL_LOCK ? m_GlobalMutex : queue.mutex );
	if ( queue.nCount == m_nCapacity ) {
		return false;
	}
	queue.pTokens[( queue.nHead + queue.nCount ) & ( m_nCapacity - 1 )].store( nToken, std::memory_order_relaxed );
	++queue.nCount;
	return true;
}

bool SIRSchedulerQueueCandidate::lockedPop( uint32_t nOwner, uint64_t *pToken, bool bSteal ) noexcept
{
	sirSchedulerQueueShard_t& queue = m_pQueues[nOwner];
	std::unique_lock<std::mutex> lock( m_eKind == SIR_QUEUE_CANDIDATE_GLOBAL_LOCK ? m_GlobalMutex : queue.mutex );
	if ( queue.nCount == 0 ) {
		return false;
	}
	const uint32_t nIndex = bSteal ? queue.nHead : ( queue.nHead + queue.nCount - 1 ) & ( m_nCapacity - 1 );
	*pToken = queue.pTokens[nIndex].load( std::memory_order_relaxed );
	if ( bSteal ) {
		queue.nHead = ( queue.nHead + 1 ) & ( m_nCapacity - 1 );
	}
	--queue.nCount;
	return true;
}

// Bounded Chase-Lev experiment, using a fixed ring and generation-tagged
// 64-bit tokens. Standard last-element CAS and seq_cst ordering; no resizing,
// foreign pushes, arbitrary removals, or task-slot reclamation.
bool SIRSchedulerQueueCandidate::chasePush( uint32_t nOwner, uint64_t nToken ) noexcept
{
	sirSchedulerQueueShard_t& queue = m_pQueues[nOwner];
	const uint64_t nBottom = queue.nBottom.load( std::memory_order_relaxed );
	const uint64_t nTop = queue.nTop.load( std::memory_order_acquire );
	if ( nBottom < nTop || nBottom - nTop >= m_nCapacity || nBottom == UINT64_MAX ) {
		return false;
	}
	queue.pTokens[nBottom & ( m_nCapacity - 1 )].store( nToken, std::memory_order_relaxed );
	queue.nBottom.store( nBottom + 1, std::memory_order_release );
	return true;
}

bool SIRSchedulerQueueCandidate::chasePop( uint32_t nOwner, uint64_t *pToken ) noexcept
{
	sirSchedulerQueueShard_t& queue = m_pQueues[nOwner];
	const uint64_t nCurrent = queue.nBottom.load( std::memory_order_relaxed );
	if ( nCurrent == 0 ) {
		return false;
	}
	const uint64_t nBottom = nCurrent - 1;
	queue.nBottom.store( nBottom, std::memory_order_seq_cst );
	uint64_t nTop = queue.nTop.load( std::memory_order_seq_cst );
	if ( nTop > nBottom ) {
		queue.nBottom.store( nBottom + 1, std::memory_order_relaxed );
		return false;
	}
	const uint64_t nValue = queue.pTokens[nBottom & ( m_nCapacity - 1 )].load( std::memory_order_relaxed );
	if ( nTop == nBottom ) {
		if ( !queue.nTop.compare_exchange_strong( nTop, nTop + 1, std::memory_order_seq_cst, std::memory_order_relaxed ) ) {
			queue.nBottom.store( nBottom + 1, std::memory_order_relaxed );
			return false;
		}
		queue.nBottom.store( nBottom + 1, std::memory_order_relaxed );
	}
	*pToken = nValue;
	return true;
}

bool SIRSchedulerQueueCandidate::chaseSteal( uint32_t nVictim, uint64_t *pToken ) noexcept
{
	sirSchedulerQueueShard_t& queue = m_pQueues[nVictim];
	const uint64_t nTop = queue.nTop.load( std::memory_order_seq_cst );
	const uint64_t nBottom = queue.nBottom.load( std::memory_order_seq_cst );
	if ( nTop >= nBottom ) {
		return false;
	}
	const uint64_t nValue = queue.pTokens[nTop & ( m_nCapacity - 1 )].load( std::memory_order_relaxed );
	uint64_t nExpected = nTop;
	if ( !queue.nTop.compare_exchange_strong( nExpected, nTop + 1, std::memory_order_seq_cst, std::memory_order_relaxed ) ) {
		return false;
	}
	*pToken = nValue;
	return true;
}

bool SIRSchedulerQueueCandidate::push( uint32_t nOwner, uint64_t nToken ) noexcept
{
	if ( !m_pQueues || nOwner >= m_nWorkers || nToken == 0 ) {
		return false;
	}
	return m_eKind == SIR_QUEUE_CANDIDATE_CHASE_LEV ? chasePush( nOwner, nToken ) : lockedPush( nOwner, nToken );
}

bool SIRSchedulerQueueCandidate::pop( uint32_t nOwner, uint64_t *pToken ) noexcept
{
	if ( !m_pQueues || nOwner >= m_nWorkers || !pToken ) {
		return false;
	}
	return m_eKind == SIR_QUEUE_CANDIDATE_CHASE_LEV ? chasePop( nOwner, pToken ) : lockedPop( nOwner, pToken, false );
}

bool SIRSchedulerQueueCandidate::steal( uint32_t nThief, uint32_t nVictim, uint64_t *pToken ) noexcept
{
	if ( !m_pQueues || nThief >= m_nWorkers || nVictim >= m_nWorkers || nThief == nVictim || !pToken ) {
		return false;
	}
	return m_eKind == SIR_QUEUE_CANDIDATE_CHASE_LEV ? chaseSteal( nVictim, pToken ) : lockedPop( nVictim, pToken, true );
}
