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

#ifndef __SIRENGINE_SIREXTERNALEVENTQUEUE_H__
#define __SIRENGINE_SIREXTERNALEVENTQUEUE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

// Caller-owned typed buffer, normally constructed in a SIR memory domain.
// init/shutdown and operations belong to one owner thread. The queue never
// allocates, deallocates or takes ownership of the supplied typed slots.
// Cold sizing is a caller policy; hot dispatch has no allocator dependency.
template <typename TPayload>
class SIRExternalEventQueue
{
public:
	typedef void ( *callback_t )( void *, const TPayload & ) noexcept;
	static_assert( std::is_trivially_copyable<TPayload>::value, "Event payloads must be trivially copyable" );

	SIRExternalEventQueue( void ) noexcept
	    : m_pEvents( nullptr ), m_pContext( nullptr ), m_pCallback( nullptr ), m_nCapacity( 0 ), m_nHead( 0 ), m_nTail( 0 ), m_nCount( 0 ),
	      m_bPumping( false ), m_bClearDuringPump( false )
	{
	}
	SIRExternalEventQueue( const SIRExternalEventQueue & ) = delete;
	SIRExternalEventQueue &operator=( const SIRExternalEventQueue & ) = delete;

	static size_t requiredStorageBytes( uint32_t nCapacity ) noexcept
	{
		if ( ( size_t )nCapacity > SIZE_MAX / sizeof( TPayload ) ) {
			return 0;
		}
		return ( size_t )nCapacity * sizeof( TPayload );
	}

	bool init( TPayload *pEvents, uint32_t nCapacity, void *pContext, callback_t pCallback ) noexcept
	{
		if ( m_pEvents != nullptr || pEvents == nullptr || nCapacity == 0 || pCallback == nullptr || m_bPumping ) {
			return false;
		}
		m_pEvents = pEvents;
		m_nCapacity = nCapacity;
		m_pContext = pContext;
		m_pCallback = pCallback;
		m_nHead = 0;
		m_nTail = 0;
		m_nCount = 0;
		return true;
	}

	bool enqueue( const TPayload &payload ) noexcept
	{
		if ( m_pEvents == nullptr || m_nCount == m_nCapacity ) {
			return false;
		}
		m_pEvents[m_nTail] = payload;
		m_nTail = m_nTail + 1 == m_nCapacity ? 0 : m_nTail + 1;
		++m_nCount;
		return true;
	}

	uint32_t enqueueBatch( const TPayload *pData, uint32_t nCount ) noexcept
	{
		if ( m_pEvents == nullptr || pData == nullptr ) {
			return 0;
		}
		const uint32_t nRoom = m_nCapacity - m_nCount;
		const uint32_t nWritten = nCount < nRoom ? nCount : nRoom;
		if ( nWritten == 0 ) {
			return 0;
		}
		const uint32_t nFirst = nWritten < m_nCapacity - m_nTail ? nWritten : m_nCapacity - m_nTail;
		memcpy( m_pEvents + m_nTail, pData, nFirst * sizeof( TPayload ) );
		if ( nFirst != nWritten ) {
			memcpy( m_pEvents, pData + nFirst, ( nWritten - nFirst ) * sizeof( TPayload ) );
		}
		m_nTail += nWritten;
		if ( m_nTail >= m_nCapacity ) {
			m_nTail -= m_nCapacity;
		}
		m_nCount += nWritten;
		return nWritten;
	}

	uint32_t pump( uint32_t nBudget = UINT32_MAX ) noexcept
	{
		if ( m_pEvents == nullptr || m_bPumping || nBudget == 0 ) {
			return 0;
		}
		m_bPumping = true;
		m_bClearDuringPump = false;
		const uint32_t nLimit = m_nCount < nBudget ? m_nCount : nBudget;
		uint32_t nDelivered = 0;
		while ( nDelivered < nLimit ) {
			const TPayload payload = m_pEvents[m_nHead];
			m_nHead = m_nHead + 1 == m_nCapacity ? 0 : m_nHead + 1;
			--m_nCount;
			m_pCallback( m_pContext, payload );
			++nDelivered;
			if ( m_nCount == 0 || m_bClearDuringPump ) {
				break;
			}
		}
		m_bPumping = false;
		return nDelivered;
	}

	void clear( void ) noexcept
	{
		if ( m_bPumping ) {
			m_bClearDuringPump = true;
		}
		m_nHead = m_nTail;
		m_nCount = 0;
	}

	bool shutdown( void ) noexcept
	{
		if ( m_bPumping ) {
			return false;
		}
		m_pEvents = nullptr;
		m_pContext = nullptr;
		m_pCallback = nullptr;
		m_nCapacity = 0;
		m_nHead = 0;
		m_nTail = 0;
		m_nCount = 0;
		return true;
	}

	uint32_t pendingCount( void ) const noexcept
	{
		return m_nCount;
	}
	uint32_t capacity( void ) const noexcept
	{
		return m_nCapacity;
	}

private:
	TPayload *m_pEvents;
	void *m_pContext;
	callback_t m_pCallback;
	uint32_t m_nCapacity;
	uint32_t m_nHead;
	uint32_t m_nTail;
	uint32_t m_nCount;
	bool m_bPumping;
	bool m_bClearDuringPump;
};

#endif // __SIRENGINE_SIREXTERNALEVENTQUEUE_H__
