/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIREVENTSCHEDULER_H__
#define __SIRENGINE_SIREVENTSCHEDULER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <stdint.h>
#include <type_traits>

typedef enum sirEventTimerState_e : uint8_t
{
	SIR_EVENT_TIMER_FREE = 0,
	SIR_EVENT_TIMER_QUEUED = 1,
	SIR_EVENT_TIMER_INVOKING = 2,
	SIR_EVENT_TIMER_CANCELLED = 3
} sirEventTimerState_t;

template <typename TPayload>
struct sirEventSchedulerSlot_s
{
	TPayload payload;
	void *pContext = nullptr;
	void ( *pCallback )( void *, const TPayload& ) noexcept = nullptr;
	uint64_t nDue = 0;
	uint64_t nInterval = 0;
	uint64_t nOrder = 0;
	uint32_t nGeneration = 0;
	uint32_t nHeapIndex = 0;
	uint32_t nNextFree = 0;
	sirEventTimerState_t nState = SIR_EVENT_TIMER_FREE;
};

// Owner-thread min-heap, O(1) free-list allocation, O(log N) cancellation.
// Time is supplied by the caller; no wall-clock assumption or scheduler thread.
template <typename TPayload, uint32_t Capacity = 128>
class SIREventScheduler
{
public:
	typedef void ( *callback_t )( void *pContext, const TPayload& payload ) noexcept;
	static_assert( Capacity > 0, "Scheduler needs slots" );
	static_assert( std::is_trivially_copyable<TPayload>::value, "Scheduler payloads must be POD" );

	SIREventScheduler( void ) noexcept : m_nFreeHead( 1 ), m_nCount( 0 ), m_nSequence( 1 ), m_bPumping( false )
	{
		for ( uint32_t i = 1; i <= Capacity; ++i ) {
			m_Slots[i].nNextFree = i == Capacity ? 0 : i + 1;
			m_Slots[i].nGeneration = 1;
		}
	}
	SIREventScheduler( const SIREventScheduler& ) = delete;
	SIREventScheduler& operator=( const SIREventScheduler& ) = delete;

	bool schedule( void *pContext, callback_t pCallback, const TPayload& payload, uint64_t nNow, uint64_t nDelay, uint64_t nInterval,
	               sirEventScheduleHandle_t& handle ) noexcept
	{
		handle = {};
		if ( pCallback == nullptr || m_nFreeHead == 0 || nNow > UINT64_MAX - nDelay || ( nInterval != 0 && nDelay == 0 ) ) {
			return false;
		}
		const uint32_t nIndex = m_nFreeHead;
		sirEventSchedulerSlot_s<TPayload>& slot = m_Slots[nIndex];
		m_nFreeHead = slot.nNextFree;
		slot.payload = payload;
		slot.pContext = pContext;
		slot.pCallback = pCallback;
		slot.nDue = nNow + nDelay;
		slot.nInterval = nInterval;
		slot.nOrder = m_nSequence++;
		slot.nState = SIR_EVENT_TIMER_QUEUED;
		handle = { this, nIndex, slot.nGeneration };
		if ( nDelay == 0 ) {
			// Preserve NomadFramework's immediate, nonrecurring schedule semantics.
			slot.nState = SIR_EVENT_TIMER_INVOKING;
			slot.pCallback( slot.pContext, slot.payload );
			release( nIndex );
			return true;
		}
		heapPush( nIndex );
		return true;
	}

	bool cancel( sirEventScheduleHandle_t handle ) noexcept
	{
		if ( handle.pOwner != this || handle.nSlot == 0 || handle.nSlot > Capacity ) {
			return false;
		}
		sirEventSchedulerSlot_s<TPayload>& slot = m_Slots[handle.nSlot];
		if ( slot.nGeneration != handle.nGeneration || slot.nState == SIR_EVENT_TIMER_FREE || slot.nState == SIR_EVENT_TIMER_CANCELLED ) {
			return false;
		}
		if ( slot.nState == SIR_EVENT_TIMER_INVOKING ) {
			slot.nState = SIR_EVENT_TIMER_CANCELLED;
		} else {
			heapRemove( slot.nHeapIndex );
			release( handle.nSlot );
		}
		return true;
	}

	uint32_t pump( uint64_t nNow, uint32_t nBudget = UINT32_MAX ) noexcept
	{
		if ( m_bPumping ) {
			return 0;
		}
		m_bPumping = true;
		uint32_t nDelivered = 0;
		while ( m_nCount != 0 && nDelivered < nBudget ) {
			const uint32_t nIndex = m_nHeap[0];
			sirEventSchedulerSlot_s<TPayload>& slot = m_Slots[nIndex];
			if ( slot.nDue > nNow ) {
				break;
			}
			heapRemove( 0 );
			const uint64_t nPreviousDue = slot.nDue;
			slot.nState = SIR_EVENT_TIMER_INVOKING;
			slot.pCallback( slot.pContext, slot.payload );
			++nDelivered;
			if ( slot.nState == SIR_EVENT_TIMER_INVOKING && slot.nInterval != 0 && nPreviousDue < UINT64_MAX ) {
				const uint64_t nInterval = slot.nInterval;
				slot.nDue = nPreviousDue > UINT64_MAX - nInterval ? UINT64_MAX : nPreviousDue + nInterval;
				if ( slot.nDue <= nNow ) {
					slot.nDue = nNow > UINT64_MAX - nInterval ? UINT64_MAX : nNow + nInterval;
				}
				if ( slot.nDue <= nNow ) {
					release( nIndex );
				} else {
					slot.nOrder = m_nSequence++;
					slot.nState = SIR_EVENT_TIMER_QUEUED;
					heapPush( nIndex );
				}
			} else {
				release( nIndex );
			}
		}
		m_bPumping = false;
		return nDelivered;
	}

	void clear( void ) noexcept
	{
		// Cannot reclaim an in-flight callback's slot until it returns.
		for ( uint32_t i = 1; i <= Capacity; ++i ) {
			if ( m_Slots[i].nState == SIR_EVENT_TIMER_QUEUED ) {
				m_Slots[i].nState = SIR_EVENT_TIMER_FREE;
				if ( ++m_Slots[i].nGeneration == 0 ) {
					++m_Slots[i].nGeneration;
				}
			} else if ( m_Slots[i].nState == SIR_EVENT_TIMER_INVOKING ) {
				m_Slots[i].nState = SIR_EVENT_TIMER_CANCELLED;
			}
		}
		m_nCount = 0;
		m_nFreeHead = 0;
		for ( uint32_t i = Capacity; i > 0; --i ) {
			if ( m_Slots[i].nState == SIR_EVENT_TIMER_FREE ) {
				m_Slots[i].nNextFree = m_nFreeHead;
				m_nFreeHead = i;
			}
		}
	}
	uint32_t pendingCount( void ) const noexcept
	{
		return m_nCount;
	}

private:
	bool less( uint32_t nA, uint32_t nB ) const noexcept
	{
		const sirEventSchedulerSlot_s<TPayload>& a = m_Slots[nA];
		const sirEventSchedulerSlot_s<TPayload>& b = m_Slots[nB];
		return a.nDue < b.nDue || ( a.nDue == b.nDue && a.nOrder < b.nOrder );
	}
	void swapHeap( uint32_t nA, uint32_t nB ) noexcept
	{
		const uint32_t nX = m_nHeap[nA];
		const uint32_t nY = m_nHeap[nB];
		m_nHeap[nA] = nY;
		m_nHeap[nB] = nX;
		m_Slots[nX].nHeapIndex = nB;
		m_Slots[nY].nHeapIndex = nA;
	}
	void siftUp( uint32_t nPosition ) noexcept
	{
		while ( nPosition != 0 ) {
			const uint32_t nParent = ( nPosition - 1 ) / 2;
			if ( !less( m_nHeap[nPosition], m_nHeap[nParent] ) ) {
				break;
			}
			swapHeap( nPosition, nParent );
			nPosition = nParent;
		}
	}
	void siftDown( uint32_t nPosition ) noexcept
	{
		for ( ;; ) {
			const uint32_t nLeft = nPosition * 2 + 1;
			if ( nLeft >= m_nCount ) {
				break;
			}
			const uint32_t nRight = nLeft + 1;
			const uint32_t nMin = nRight < m_nCount && less( m_nHeap[nRight], m_nHeap[nLeft] ) ? nRight : nLeft;
			if ( !less( m_nHeap[nMin], m_nHeap[nPosition] ) ) {
				break;
			}
			swapHeap( nPosition, nMin );
			nPosition = nMin;
		}
	}
	void heapPush( uint32_t nIndex ) noexcept
	{
		const uint32_t nPosition = m_nCount++;
		m_nHeap[nPosition] = nIndex;
		m_Slots[nIndex].nHeapIndex = nPosition;
		siftUp( nPosition );
	}
	void heapRemove( uint32_t nPosition ) noexcept
	{
		const uint32_t nLast = m_nHeap[--m_nCount];
		if ( nPosition != m_nCount ) {
			m_nHeap[nPosition] = nLast;
			m_Slots[nLast].nHeapIndex = nPosition;
			if ( nPosition != 0 && less( m_nHeap[nPosition], m_nHeap[( nPosition - 1 ) / 2] ) ) {
				siftUp( nPosition );
			} else {
				siftDown( nPosition );
			}
		}
	}
	void release( uint32_t nIndex ) noexcept
	{
		sirEventSchedulerSlot_s<TPayload>& slot = m_Slots[nIndex];
		slot.nState = SIR_EVENT_TIMER_FREE;
		if ( ++slot.nGeneration == 0 ) {
			++slot.nGeneration;
		}
		slot.nNextFree = m_nFreeHead;
		m_nFreeHead = nIndex;
	}

	sirEventSchedulerSlot_s<TPayload> m_Slots[Capacity + 1];
	uint32_t m_nHeap[Capacity]{};
	uint32_t m_nFreeHead;
	uint32_t m_nCount;
	uint64_t m_nSequence;
	bool m_bPumping;
};

#endif // __SIRENGINE_SIREVENTSCHEDULER_H__
