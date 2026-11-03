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

#ifndef __SIRENGINE_SIREVENTPHASEQUEUE_H__
#define __SIRENGINE_SIREVENTPHASEQUEUE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>
#include <type_traits>

typedef struct sirEventTargetRef_s
{
	uint64_t nIdentifier;
	uint32_t nGeneration;
	uint32_t nDomain;

	constexpr bool isValid( void ) const noexcept
	{
		return nIdentifier != 0 && nGeneration != 0;
	}
} sirEventTargetRef_t;

template <typename TPayload, typename TTarget = sirEventTargetRef_t>
struct sirPhasedEvent_s
{
	TTarget target;
	TPayload payload;
	uint32_t nNext;
};

// Owner-thread phase-boundary queue with one shared fixed-capacity slab.
// Each phase holds a FIFO of indices; draining detaches the current FIFO so
// callbacks can queue events for the next pass. Slots being dispatched are
// reserved until callbacks return. Capacity is TOTAL across all phases.
// Target generations are validated against the owning subsystem on delivery.
template <typename TPayload, uint32_t PhaseCount = 4, uint32_t Capacity = 256, typename TTarget = sirEventTargetRef_t>
class SIREventPhaseQueue
{
public:
	typedef void ( *callback_t )( void *, TTarget, const TPayload & ) noexcept;
	typedef bool ( *validator_t )( void *, TTarget ) noexcept;
	static_assert( PhaseCount > 0 && Capacity > 0, "Phased queues require phases and slots" );
	static_assert( std::is_trivially_copyable<TPayload>::value, "Phased queue payload must be POD" );
	static_assert( std::is_same<TTarget, sirEventTargetRef_t>::value || std::is_same<TTarget, sirCompactEventRef_t>::value,
	               "Supported phase targets are full or compact resource refs" );

	SIREventPhaseQueue( void ) noexcept : m_nFreeHead( 1 ), m_nStaleDiscarded( 0 )
	{
		for ( uint32_t i = 1; i <= Capacity; ++i ) {
			m_Events[i].nNext = i == Capacity ? 0 : i + 1;
		}
	}
	SIREventPhaseQueue( const SIREventPhaseQueue & ) = delete;
	SIREventPhaseQueue &operator=( const SIREventPhaseQueue & ) = delete;

	bool enqueue( uint32_t nPhase, const TPayload &payload, TTarget target = {} ) noexcept
	{
		if ( nPhase >= PhaseCount || m_nFreeHead == 0 ) {
			return false;
		}
		if constexpr ( std::is_same<TTarget, sirEventTargetRef_t>::value ) {
			if ( target.nIdentifier == 0 ? ( target.nGeneration != 0 || target.nDomain != 0 ) : target.nGeneration == 0 ) {
				return false;
			}
		} else {
			if ( target.nSlot == 0 ? target.nGeneration != 0 : target.nGeneration == 0 ) {
				return false;
			}
		}
		const uint32_t nIndex = m_nFreeHead;
		sirPhasedEvent_s<TPayload, TTarget> &entry = m_Events[nIndex];
		m_nFreeHead = entry.nNext;
		entry.payload = payload;
		entry.target = target;
		entry.nNext = 0;
		if ( m_nTails[nPhase] != 0 ) {
			m_Events[m_nTails[nPhase]].nNext = nIndex;
		} else {
			m_nHeads[nPhase] = nIndex;
		}
		m_nTails[nPhase] = nIndex;
		++m_nCounts[nPhase];
		return true;
	}

	uint32_t pumpPhase( uint32_t nPhase, void *pContext, callback_t pCallback, validator_t pValidator,
	                    uint32_t nBudget = UINT32_MAX ) noexcept
	{
		if ( nPhase >= PhaseCount || pCallback == nullptr || m_bDraining[nPhase] || nBudget == 0 ) {
			return 0;
		}
		m_bDraining[nPhase] = true;
		uint32_t nIndex = m_nHeads[nPhase];
		const uint32_t nOldTail = m_nTails[nPhase];
		const uint32_t nDetached = m_nCounts[nPhase];
		m_nHeads[nPhase] = 0;
		m_nTails[nPhase] = 0;
		m_nCounts[nPhase] = 0;
		uint32_t nDelivered = 0;
		uint32_t nExamined = 0;
		while ( nIndex != 0 && nExamined < nBudget ) {
			++nExamined;
			sirPhasedEvent_s<TPayload, TTarget> &entry = m_Events[nIndex];
			const uint32_t nNext = entry.nNext;
			if ( entry.target.isValid() && ( pValidator == nullptr || !pValidator( pContext, entry.target ) ) ) {
				++m_nStaleDiscarded;
			} else {
				pCallback( pContext, entry.target, entry.payload );
				++nDelivered;
			}
			entry.nNext = m_nFreeHead;
			m_nFreeHead = nIndex;
			nIndex = nNext;
		}
		if ( nIndex != 0 ) {
			// Old, unprocessed records precede new records enqueued by callbacks.
			m_Events[nOldTail].nNext = m_nHeads[nPhase];
			if ( m_nTails[nPhase] == 0 ) {
				m_nTails[nPhase] = nOldTail;
			}
			m_nHeads[nPhase] = nIndex;
			m_nCounts[nPhase] += nDetached - nExamined;
		}
		m_bDraining[nPhase] = false;
		return nDelivered;
	}

	void clearPending( uint32_t nPhase ) noexcept
	{
		if ( nPhase >= PhaseCount ) {
			return;
		}
		uint32_t nIndex = m_nHeads[nPhase];
		while ( nIndex != 0 ) {
			const uint32_t nNext = m_Events[nIndex].nNext;
			m_Events[nIndex].nNext = m_nFreeHead;
			m_nFreeHead = nIndex;
			nIndex = nNext;
		}
		m_nHeads[nPhase] = 0;
		m_nTails[nPhase] = 0;
		m_nCounts[nPhase] = 0;
	}

	uint32_t pendingCount( uint32_t nPhase ) const noexcept
	{
		return nPhase < PhaseCount ? m_nCounts[nPhase] : 0;
	}
	uint64_t staleDiscardCount( void ) const noexcept
	{
		return m_nStaleDiscarded;
	}

private:
	sirPhasedEvent_s<TPayload, TTarget> m_Events[Capacity + 1];
	uint32_t m_nHeads[PhaseCount]{};
	uint32_t m_nTails[PhaseCount]{};
	uint32_t m_nCounts[PhaseCount]{};
	bool m_bDraining[PhaseCount]{};
	uint32_t m_nFreeHead;
	uint64_t m_nStaleDiscarded;
};

// Phase identity supplies the resource domain, saving eight bytes per record
// when the owning subsystem uses 32-bit resource slots.
template <typename TPayload, uint32_t PhaseCount = 4, uint32_t Capacity = 256>
using SIRCompactPhaseQueue = SIREventPhaseQueue<TPayload, PhaseCount, Capacity, sirCompactEventRef_t>;

#endif // __SIRENGINE_SIREVENTPHASEQUEUE_H__
