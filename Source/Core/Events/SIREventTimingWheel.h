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

#ifndef __SIRENGINE_SIREVENTTIMINGWHEEL_H__
#define __SIRENGINE_SIREVENTTIMINGWHEEL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <stdint.h>
#if defined( _MSC_VER )
#include <intrin.h>
#endif
#include <type_traits>

typedef enum sirEventWheelState_e : uint8_t
{
	SIR_EVENT_WHEEL_FREE = 0,
	SIR_EVENT_WHEEL_QUEUED = 1,
	SIR_EVENT_WHEEL_INVOKING = 2,
	SIR_EVENT_WHEEL_CANCELLED = 3
} sirEventWheelState_t;

template <typename TPayload>
struct sirEventWheelSlot_s
{
	TPayload payload;
	void *pContext = nullptr;
	void ( *pCallback )( void *, const TPayload& ) noexcept = nullptr;
	uint64_t nDue = 0;
	uint64_t nInterval = 0;
	uint32_t nNext = 0;
	uint32_t nPrev = 0;
	uint32_t nGeneration = 1;
	sirEventWheelState_t eState = SIR_EVENT_WHEEL_FREE;
};

// Bounded, single-level owner-thread wheel for dense short-horizon timers.
// Delays and intervals must be strictly below BucketCount; long sparse timers
// continue to use SIREventScheduler's general min-heap. Advancing far ahead
// scans each bucket once and skips missed periodic beats.
// Callbacks are collected before invocation, so cancellation and clear during
// callbacks cannot mutate the iterator's linked list.
template <typename TPayload, uint32_t Capacity = 128, uint32_t BucketCount = 256>
class SIREventTimingWheel
{
public:
	typedef void ( *callback_t )( void *, const TPayload& ) noexcept;
	static_assert( Capacity > 0, "Timing wheel requires storage" );
	static_assert( BucketCount > 1 && ( BucketCount & ( BucketCount - 1 ) ) == 0, "Bucket count must be power of two" );
	static_assert( std::is_trivially_copyable<TPayload>::value, "Timing wheel payloads must be POD" );

	SIREventTimingWheel( void ) noexcept
	    : m_nFreeHead( 1 ), m_nPending( 0 ), m_nClock( 0 ), m_bInitialized( false ), m_bPumping( false ), m_bIncompleteTick( false )
	{
		for ( uint32_t i = 1; i <= Capacity; ++i ) {
			m_Slots[i].nNext = i == Capacity ? 0 : i + 1;
		}
	}
	SIREventTimingWheel( const SIREventTimingWheel& ) = delete;
	SIREventTimingWheel& operator=( const SIREventTimingWheel& ) = delete;

	bool schedule( void *pContext, callback_t pCallback, const TPayload& payload, uint64_t nNow, uint64_t nDelay, uint64_t nInterval,
	               sirEventScheduleHandle_t& handle ) noexcept
	{
		handle = {};
		if ( pCallback == nullptr || m_nFreeHead == 0 || nDelay >= BucketCount || nInterval >= BucketCount ||
		     ( nInterval != 0 && nDelay == 0 ) || nNow > UINT64_MAX - nDelay || ( m_bInitialized && nNow < m_nClock ) ) {
			return false;
		}
		if ( !m_bInitialized ) {
			m_bInitialized = true;
			m_nClock = nNow;
		}
		const uint32_t nIndex = m_nFreeHead;
		sirEventWheelSlot_s<TPayload>& slot = m_Slots[nIndex];
		m_nFreeHead = slot.nNext;
		slot.payload = payload;
		slot.pContext = pContext;
		slot.pCallback = pCallback;
		slot.nDue = nNow + nDelay;
		slot.nInterval = nInterval;
		handle = { this, nIndex, slot.nGeneration };
		if ( nDelay == 0 ) {
			slot.eState = SIR_EVENT_WHEEL_INVOKING;
			slot.pCallback( slot.pContext, slot.payload );
			release( nIndex );
			return true;
		}
		insert( nIndex );
		return true;
	}

	bool cancel( sirEventScheduleHandle_t handle ) noexcept
	{
		if ( handle.pOwner != this || handle.nSlot == 0 || handle.nSlot > Capacity ) {
			return false;
		}
		sirEventWheelSlot_s<TPayload>& slot = m_Slots[handle.nSlot];
		if ( slot.nGeneration != handle.nGeneration || slot.eState == SIR_EVENT_WHEEL_FREE || slot.eState == SIR_EVENT_WHEEL_CANCELLED ) {
			return false;
		}
		if ( slot.eState == SIR_EVENT_WHEEL_QUEUED ) {
			unlink( handle.nSlot );
			release( handle.nSlot );
		} else {
			slot.eState = SIR_EVENT_WHEEL_CANCELLED;
		}
		return true;
	}

	uint32_t pump( uint64_t nNow, uint32_t nBudget = UINT32_MAX ) noexcept
	{
		if ( m_bPumping || nBudget == 0 || ( m_bInitialized && nNow < m_nClock ) ) {
			return 0;
		}
		if ( !m_bInitialized || ( m_nPending == 0 && !m_bIncompleteTick ) ) {
			m_bInitialized = true;
			m_nClock = nNow;
			return 0;
		}
		if ( nNow == m_nClock && !m_bIncompleteTick ) {
			return 0;
		}
		m_bPumping = true;
		const uint32_t nMax = nBudget < Capacity ? nBudget : Capacity;
		uint32_t szDue[Capacity];
		uint32_t nCollected = 0;
		uint64_t nTick = m_bIncompleteTick ? m_nClock : m_nClock + 1;
		if ( nNow - nTick >= BucketCount ) {
			nTick = nNow - ( BucketCount - 1 );
		}
		m_bIncompleteTick = false;
		for ( ;; ) {
			uint64_t nOccupiedTick = 0;
			if ( !nextOccupiedTick( nTick, nNow, nOccupiedTick ) ) {
				m_nClock = nNow;
				break;
			}
			nTick = nOccupiedTick;
			const uint32_t nBucket = ( uint32_t )( nTick & ( BucketCount - 1 ) );
			uint32_t nIndex = m_nHeads[nBucket];
			while ( nIndex != 0 && nCollected < nMax ) {
				const uint32_t nNext = m_Slots[nIndex].nNext;
				if ( m_Slots[nIndex].nDue <= nNow ) {
					unlink( nIndex );
					m_Slots[nIndex].eState = SIR_EVENT_WHEEL_INVOKING;
					szDue[nCollected++] = nIndex;
				}
				nIndex = nNext;
			}
			m_nClock = nTick;
			if ( nCollected == nMax ) {
				m_bIncompleteTick = true;
				break;
			}
			if ( nTick == nNow ) {
				break;
			}
			++nTick;
		}
		uint32_t nInvoked = 0;
		for ( uint32_t i = 0; i < nCollected; ++i ) {
			const uint32_t nIndex = szDue[i];
			sirEventWheelSlot_s<TPayload>& slot = m_Slots[nIndex];
			if ( slot.eState == SIR_EVENT_WHEEL_INVOKING ) {
				slot.pCallback( slot.pContext, slot.payload );
				++nInvoked;
			}
			if ( slot.eState == SIR_EVENT_WHEEL_INVOKING && slot.nInterval != 0 && nNow <= UINT64_MAX - slot.nInterval ) {
				slot.nDue = nNow + slot.nInterval;
				insert( nIndex );
			} else {
				release( nIndex );
			}
		}
		m_bPumping = false;
		return nInvoked;
	}

	void clear( void ) noexcept
	{
		for ( uint32_t i = 0; i < BucketCount; ++i ) {
			m_nHeads[i] = 0;
			m_nTails[i] = 0;
		}
		for ( uint32_t i = 0; i < ( BucketCount + 63 ) / 64; ++i ) {
			m_nOccupied[i] = 0;
		}
		m_nPending = 0;
		m_nFreeHead = 0;
		for ( uint32_t i = Capacity; i > 0; --i ) {
			sirEventWheelSlot_s<TPayload>& slot = m_Slots[i];
			if ( slot.eState == SIR_EVENT_WHEEL_INVOKING ) {
				slot.eState = SIR_EVENT_WHEEL_CANCELLED;
			} else if ( slot.eState == SIR_EVENT_WHEEL_QUEUED || slot.eState == SIR_EVENT_WHEEL_FREE ) {
				slot.eState = SIR_EVENT_WHEEL_FREE;
				if ( ++slot.nGeneration == 0 ) {
					++slot.nGeneration;
				}
				slot.nNext = m_nFreeHead;
				m_nFreeHead = i;
			}
		}
	}

	uint32_t pendingCount( void ) const noexcept
	{
		return m_nPending;
	}

private:
	// At most one wheel revolution is relevant. Find occupied buckets in
	// 64-bit groups rather than advancing through empty ticks.
	bool nextOccupiedTick( uint64_t nStart, uint64_t nEnd, uint64_t& nFound ) const noexcept
	{
		uint64_t nDistance = nEnd - nStart;
		if ( nDistance >= BucketCount ) {
			nDistance = BucketCount - 1;
		}
		uint32_t nRemaining = ( uint32_t )nDistance + 1;
		uint32_t nAdvanced = 0;
		while ( nRemaining != 0 ) {
			const uint32_t nBucket = ( uint32_t )( ( nStart + nAdvanced ) & ( BucketCount - 1 ) );
			const uint32_t nShift = nBucket & 63;
			const uint32_t nGroupLimit = BucketCount - nBucket < 64 - nShift ? BucketCount - nBucket : 64 - nShift;
			const uint32_t nStep = nRemaining < nGroupLimit ? nRemaining : nGroupLimit;
			uint64_t nBits = m_nOccupied[nBucket >> 6] >> nShift;
			if ( nStep != 64 ) {
				nBits &= ( UINT64_C( 1 ) << nStep ) - 1;
			}
			if ( nBits != 0 ) {
#if defined( _MSC_VER )
				unsigned long nBit = 0;
				_BitScanForward64( &nBit, nBits );
				nFound = nStart + nAdvanced + nBit;
#else
				nFound = nStart + nAdvanced + ( uint32_t )__builtin_ctzll( nBits );
#endif
				return true;
			}
			nRemaining -= nStep;
			nAdvanced += nStep;
		}
		return false;
	}

	void insert( uint32_t nIndex ) noexcept
	{
		sirEventWheelSlot_s<TPayload>& slot = m_Slots[nIndex];
		const uint32_t nBucket = ( uint32_t )( slot.nDue & ( BucketCount - 1 ) );
		slot.nNext = 0;
		slot.nPrev = m_nTails[nBucket];
		if ( m_nTails[nBucket] != 0 ) {
			m_Slots[m_nTails[nBucket]].nNext = nIndex;
		} else {
			m_nHeads[nBucket] = nIndex;
		}
		m_nTails[nBucket] = nIndex;
		m_nOccupied[nBucket >> 6] |= UINT64_C( 1 ) << ( nBucket & 63 );
		slot.eState = SIR_EVENT_WHEEL_QUEUED;
		++m_nPending;
	}
	void unlink( uint32_t nIndex ) noexcept
	{
		sirEventWheelSlot_s<TPayload>& slot = m_Slots[nIndex];
		const uint32_t nBucket = ( uint32_t )( slot.nDue & ( BucketCount - 1 ) );
		if ( slot.nPrev != 0 ) {
			m_Slots[slot.nPrev].nNext = slot.nNext;
		} else {
			m_nHeads[nBucket] = slot.nNext;
		}
		if ( slot.nNext != 0 ) {
			m_Slots[slot.nNext].nPrev = slot.nPrev;
		} else {
			m_nTails[nBucket] = slot.nPrev;
		}
		if ( m_nHeads[nBucket] == 0 ) {
			m_nOccupied[nBucket >> 6] &= ~( UINT64_C( 1 ) << ( nBucket & 63 ) );
		}
		--m_nPending;
	}
	void release( uint32_t nIndex ) noexcept
	{
		sirEventWheelSlot_s<TPayload>& slot = m_Slots[nIndex];
		slot.eState = SIR_EVENT_WHEEL_FREE;
		if ( ++slot.nGeneration == 0 ) {
			++slot.nGeneration;
		}
		slot.nNext = m_nFreeHead;
		m_nFreeHead = nIndex;
	}

	sirEventWheelSlot_s<TPayload> m_Slots[Capacity + 1];
	uint32_t m_nHeads[BucketCount]{};
	uint32_t m_nTails[BucketCount]{};
	uint64_t m_nOccupied[( BucketCount + 63 ) / 64]{};
	uint32_t m_nFreeHead;
	uint32_t m_nPending;
	uint64_t m_nClock;
	bool m_bInitialized;
	bool m_bPumping;
	bool m_bIncompleteTick;
};

#endif // __SIRENGINE_SIREVENTTIMINGWHEEL_H__
