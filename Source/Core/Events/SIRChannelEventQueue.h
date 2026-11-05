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

#ifndef __SIRENGINE_SIRCHANNELEVENTQUEUE_H__
#define __SIRENGINE_SIRCHANNELEVENTQUEUE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

// FIFO for one owner thread and one fixed handler. Callback metadata is
// stored once rather than on every event. Snapshot budget prevents a callback
// that continuously appends events from keeping pump() alive indefinitely.
template <typename TPayload, uint32_t Capacity = 256>
class SIRChannelEventQueue
{
public:
	typedef void ( *callback_t )( void *, const TPayload& ) noexcept;
	static_assert( Capacity > 0, "Channel queue requires slots" );
	static_assert( std::is_trivially_copyable<TPayload>::value, "Channel event must be POD" );

	SIRChannelEventQueue( void ) noexcept
	    : m_pContext( nullptr ), m_pCallback( nullptr ), m_nHead( 0 ), m_nTail( 0 ), m_nCount( 0 ), m_bPumping( false ),
	      m_bClearDuringPump( false )
	{
	}
	SIRChannelEventQueue( const SIRChannelEventQueue& ) = delete;
	SIRChannelEventQueue& operator=( const SIRChannelEventQueue& ) = delete;

	bool bind( void *pContext, callback_t pCallback ) noexcept
	{
		if ( m_bPumping || pCallback == nullptr ) {
			return false;
		}
		m_pContext = pContext;
		m_pCallback = pCallback;
		return true;
	}
	bool enqueue( const TPayload& payload ) noexcept
	{
		if ( m_nCount == Capacity ) {
			return false;
		}
		m_Events[m_nTail] = payload;
		m_nTail = ( m_nTail + 1 ) % Capacity;
		++m_nCount;
		return true;
	}
	uint32_t enqueueBatch( const TPayload *pData, uint32_t nCount ) noexcept
	{
		if ( pData == nullptr || nCount == 0 ) {
			return 0;
		}
		const uint32_t nRoom = Capacity - m_nCount;
		const uint32_t nWritten = nCount < nRoom ? nCount : nRoom;
		const uint32_t nFirst = nWritten < Capacity - m_nTail ? nWritten : Capacity - m_nTail;
		memcpy( m_Events + m_nTail, pData, nFirst * sizeof( TPayload ) );
		if ( nFirst < nWritten ) {
			memcpy( m_Events, pData + nFirst, ( nWritten - nFirst ) * sizeof( TPayload ) );
		}
		m_nTail = ( m_nTail + nWritten ) % Capacity;
		m_nCount += nWritten;
		return nWritten;
	}
	uint32_t pump( uint32_t nBudget = UINT32_MAX ) noexcept
	{
		if ( m_bPumping || m_pCallback == nullptr || nBudget == 0 ) {
			return 0;
		}
		m_bPumping = true;
		m_bClearDuringPump = false;
		const uint32_t nLimit = m_nCount < nBudget ? m_nCount : nBudget;
		uint32_t nPumped = 0;
		while ( nPumped < nLimit ) {
			// Copy protects the in-flight payload if callback clears/reuses ring slots.
			const TPayload payload = m_Events[m_nHead];
			m_nHead = ( m_nHead + 1 ) % Capacity;
			--m_nCount;
			m_pCallback( m_pContext, payload );
			++nPumped;
			if ( m_nCount == 0 || m_bClearDuringPump ) {
				break;
			}
		}
		m_bPumping = false;
		return nPumped;
	}
	void clear( void ) noexcept
	{
		if ( m_bPumping ) {
			m_bClearDuringPump = true;
		}
		m_nHead = m_nTail;
		m_nCount = 0;
	}
	uint32_t pendingCount( void ) const noexcept
	{
		return m_nCount;
	}
	static constexpr uint32_t capacity( void ) noexcept
	{
		return Capacity;
	}

private:
	TPayload m_Events[Capacity];
	void *m_pContext;
	callback_t m_pCallback;
	uint32_t m_nHead;
	uint32_t m_nTail;
	uint32_t m_nCount;
	bool m_bPumping;
	bool m_bClearDuringPump;
};

#endif // __SIRENGINE_SIRCHANNELEVENTQUEUE_H__
