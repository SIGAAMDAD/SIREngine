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

#ifndef __SIRENGINE_SIRMPSCEVENTQUEUE_H__
#define __SIRENGINE_SIRMPSCEVENTQUEUE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <Core/Platform/SIRAtomic.h>
#include <stdint.h>
#include <type_traits>

template <typename TPayload>
struct sirMPSCEventSlot_s
{
	SIRAtomic<uint64_t> nSequence;
	TPayload payload;
	sirMPSCEventSlot_s( void ) noexcept : nSequence( 0 )
	{
	}
};

// Optional shared ingress for concurrent producers and exactly one consumer.
// Bounded sequence-tagged queue: producers CAS a slot reservation, then publish
// the initialized payload using a release sequence store. A full queue returns
// immediately; no unbounded producer ticket or heap allocation is required.
// Call stop(), join every producer, drain and then destroy.
template <typename TPayload, uint32_t Capacity = 256>
class SIRMPSCEventQueue
{
public:
	static_assert( Capacity > 1 && ( Capacity & ( Capacity - 1 ) ) == 0, "MPSC capacity must be a power of two" );
	static_assert( std::is_trivially_copyable<TPayload>::value, "MPSC payload must be trivially copyable" );

	SIRMPSCEventQueue( void ) noexcept : m_nHead( 0 ), m_nTail( 0 ), m_bStopped( false ), m_bDraining( false )
	{
		for ( uint32_t i = 0; i < Capacity; ++i ) {
			m_Slots[i].nSequence.store( i, SIR_MEMORY_ORDER_RELAXED );
		}
	}
	SIRMPSCEventQueue( const SIRMPSCEventQueue & ) = delete;
	SIRMPSCEventQueue &operator=( const SIRMPSCEventQueue & ) = delete;

	sirEventEnqueueStatus_t tryEnqueue( const TPayload &payload ) noexcept
	{
		if ( m_bStopped.load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
			return SIR_EVENT_ENQUEUE_STOPPED;
		}
		uint64_t nPosition = m_nTail.load( SIR_MEMORY_ORDER_RELAXED );
		for ( ;; ) {
			sirMPSCEventSlot_s<TPayload> &slot = m_Slots[nPosition & ( Capacity - 1 )];
			const uint64_t nSequence = slot.nSequence.load( SIR_MEMORY_ORDER_ACQUIRE );
			const int64_t nDifference = ( int64_t )( nSequence - nPosition );
			if ( nDifference == 0 ) {
				uint64_t nExpected = nPosition;
				if ( m_nTail.compareExchangeWeak( nExpected, nPosition + 1, SIR_MEMORY_ORDER_RELAXED ) ) {
					slot.payload = payload;
					slot.nSequence.store( nPosition + 1, SIR_MEMORY_ORDER_RELEASE );
					return SIR_EVENT_ENQUEUE_OK;
				}
				nPosition = nExpected;
			} else if ( nDifference < 0 ) {
				return SIR_EVENT_ENQUEUE_FULL;
			} else {
				nPosition = m_nTail.load( SIR_MEMORY_ORDER_RELAXED );
			}
		}
	}

	bool tryDequeue( TPayload &output ) noexcept
	{
		if ( m_bDraining ) {
			return false;
		}
		return dequeueOne( output );
	}

	template <typename TCallback>
	uint32_t drain( TCallback callback, uint32_t nBudget = UINT32_MAX ) noexcept
	{
		if ( m_bDraining || nBudget == 0 ) {
			return 0;
		}
		m_bDraining = true;
		uint32_t nCount = 0;
		TPayload payload;
		while ( nCount < nBudget && dequeueOne( payload ) ) {
			callback( payload );
			++nCount;
		}
		m_bDraining = false;
		return nCount;
	}

	void stop( void ) noexcept
	{
		m_bStopped.store( true, SIR_MEMORY_ORDER_RELEASE );
	}

private:
	bool dequeueOne( TPayload &output ) noexcept
	{
		const uint64_t nPosition = m_nHead;
		sirMPSCEventSlot_s<TPayload> &slot = m_Slots[nPosition & ( Capacity - 1 )];
		if ( slot.nSequence.load( SIR_MEMORY_ORDER_ACQUIRE ) != nPosition + 1 ) {
			return false;
		}
		output = slot.payload;
		slot.nSequence.store( nPosition + Capacity, SIR_MEMORY_ORDER_RELEASE );
		m_nHead = nPosition + 1;
		return true;
	}

	alignas( 64 ) uint64_t m_nHead;
	alignas( 64 ) SIRAtomic<uint64_t> m_nTail;
	alignas( 64 ) SIRAtomic<bool> m_bStopped;
	bool m_bDraining;
	sirMPSCEventSlot_s<TPayload> m_Slots[Capacity];
};

#endif // __SIRENGINE_SIRMPSCEVENTQUEUE_H__
