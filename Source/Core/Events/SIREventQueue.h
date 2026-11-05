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

#ifndef __SIRENGINE_SIREVENTQUEUE_H__
#define __SIRENGINE_SIREVENTQUEUE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <stdint.h>
#include <type_traits>

template <typename TPayload>
struct sirEventQueueSlot_s
{
	TPayload payload;
	void *pContext;
	void ( *pCallback )( void *, const TPayload& ) noexcept;
	uint32_t nNext;
};

// Six-priority, owner-thread FIFO. The in-flight payload slot stays reserved
// until its callback completes, even if the callback enqueues or clears.
template <typename TPayload, uint32_t Capacity = 256>
class SIREventQueue
{
public:
	typedef void ( *callback_t )( void *pContext, const TPayload& payload ) noexcept;
	static_assert( Capacity > 0, "Queue needs slots" );
	static_assert( std::is_trivially_copyable<TPayload>::value, "Queue payloads must be POD" );

	SIREventQueue( void ) noexcept : m_nFreeHead( 1 ), m_nCount( 0 ), m_bProcessing( false )
	{
		for ( uint32_t i = 1; i <= Capacity; ++i ) {
			m_Slots[i].nNext = i == Capacity ? 0 : i + 1;
		}
	}
	SIREventQueue( const SIREventQueue& ) = delete;
	SIREventQueue& operator=( const SIREventQueue& ) = delete;

	bool enqueue( void *pContext, callback_t pCallback, const TPayload& payload,
	              sirEventPriority_t ePriority = SIR_EVENT_PRIORITY_NORMAL ) noexcept
	{
		if ( pCallback == nullptr || ePriority >= SIR_EVENT_PRIORITY_COUNT || m_nFreeHead == 0 ) {
			return false;
		}
		const uint32_t nIndex = m_nFreeHead;
		m_nFreeHead = m_Slots[nIndex].nNext;
		m_Slots[nIndex].pContext = pContext;
		m_Slots[nIndex].pCallback = pCallback;
		m_Slots[nIndex].payload = payload;
		m_Slots[nIndex].nNext = 0;
		const uint32_t nPriority = ( uint32_t )ePriority;
		if ( m_nTail[nPriority] != 0 ) {
			m_Slots[m_nTail[nPriority]].nNext = nIndex;
		} else {
			m_nHead[nPriority] = nIndex;
		}
		m_nTail[nPriority] = nIndex;
		++m_nCount;
		return true;
	}

	bool processNext( void ) noexcept
	{
		if ( m_bProcessing || m_nCount == 0 ) {
			return false;
		}
		int nPriority = SIR_EVENT_PRIORITY_COUNT - 1;
		while ( nPriority >= 0 && m_nHead[nPriority] == 0 ) {
			--nPriority;
		}
		if ( nPriority < 0 ) {
			return false;
		}
		const uint32_t nIndex = m_nHead[nPriority];
		m_nHead[nPriority] = m_Slots[nIndex].nNext;
		if ( m_nHead[nPriority] == 0 ) {
			m_nTail[nPriority] = 0;
		}
		--m_nCount;
		m_bProcessing = true;
		const callback_t pCallback = m_Slots[nIndex].pCallback;
		void *pContext = m_Slots[nIndex].pContext;
		pCallback( pContext, m_Slots[nIndex].payload );
		m_Slots[nIndex].nNext = m_nFreeHead;
		m_nFreeHead = nIndex;
		m_bProcessing = false;
		return true;
	}

	uint32_t processAll( uint32_t nBudget = UINT32_MAX ) noexcept
	{
		uint32_t nCount = 0;
		while ( nCount < nBudget && processNext() ) {
			++nCount;
		}
		return nCount;
	}

	// May be called from within a callback. The in-flight slot is not on any
	// queue list, so clearing queued work cannot invalidate its payload.
	void clear( void ) noexcept
	{
		for ( uint32_t nPriority = 0; nPriority < SIR_EVENT_PRIORITY_COUNT; ++nPriority ) {
			uint32_t nIndex = m_nHead[nPriority];
			while ( nIndex != 0 ) {
				const uint32_t nNext = m_Slots[nIndex].nNext;
				m_Slots[nIndex].nNext = m_nFreeHead;
				m_nFreeHead = nIndex;
				nIndex = nNext;
			}
			m_nHead[nPriority] = 0;
			m_nTail[nPriority] = 0;
		}
		m_nCount = 0;
	}
	uint32_t count( void ) const noexcept
	{
		return m_nCount;
	}

private:
	sirEventQueueSlot_s<TPayload> m_Slots[Capacity + 1];
	uint32_t m_nHead[SIR_EVENT_PRIORITY_COUNT]{};
	uint32_t m_nTail[SIR_EVENT_PRIORITY_COUNT]{};
	uint32_t m_nFreeHead;
	uint32_t m_nCount;
	bool m_bProcessing;
};

#endif // __SIRENGINE_SIREVENTQUEUE_H__
