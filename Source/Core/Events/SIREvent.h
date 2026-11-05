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

#ifndef __SIRENGINE_SIREVENT_H__
#define __SIRENGINE_SIREVENT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <atomic>

#include <stdint.h>

#include <type_traits>

// Single-owner typed signal. subscribe/unsubscribe/clear and publish must all run
// on the same owning thread. No callbacks are invoked under a registry lock.
// New listeners added during a publication become visible after its outermost return.
// A removed listener is suppressed immediately, including in recursive publications.
template <typename TPayload, uint32_t Capacity = 8>
class SIREvent
{
public:
	typedef void ( *callback_t )( void *pContext, const TPayload& payload ) noexcept;
	static_assert( Capacity > 0, "A SIREvent needs at least one listener slot" );

	SIREvent( void ) noexcept : m_nCount( 0 ), m_nActive( 0 ), m_nDepth( 0 ), m_bDirty( false )
	{
	}
	SIREvent( const SIREvent& ) = delete;
	SIREvent& operator=( const SIREvent& ) = delete;

	sirSubscriptionHandle_t subscribe( void *pContext, callback_t pCallback ) noexcept
	{
		if ( pCallback == nullptr || m_nCount >= Capacity ) {
			return {};
		}
		const uint64_t nToken = s_nNextToken.fetch_add( 1, std::memory_order_relaxed );
		if ( nToken == 0 || nToken == UINT64_MAX ) {
			return {};
		}
		const uint32_t nIndex = m_nCount++;
		m_Callbacks[nIndex] = { pContext, pCallback };
		m_nTokens[nIndex] = nToken;
		m_nStates[nIndex] = m_nDepth == 0 ? SIR_EVENT_ENTRY_ACTIVE : SIR_EVENT_ENTRY_PENDING;
		if ( m_nDepth == 0 ) {
			++m_nActive;
		} else {
			m_bDirty = true;
		}
		return { this, nToken };
	}

	template <typename TObject, void ( TObject::*Method )( const TPayload& ) noexcept>
	sirSubscriptionHandle_t subscribe( TObject *pObject ) noexcept
	{
		return subscribe( pObject,
		                  []( void *pContext, const TPayload& payload ) noexcept { ( ( ( TObject * )pContext )->*Method )( payload ); } );
	}

	template <typename TObject, void ( TObject::*Method )( const TPayload& ) const noexcept>
	sirSubscriptionHandle_t subscribe( const TObject *pObject ) noexcept
	{
		return subscribe( ( void * )pObject, []( void *pContext, const TPayload& payload ) noexcept {
			( ( ( const TObject * )pContext )->*Method )( payload );
		} );
	}

	bool unsubscribe( sirSubscriptionHandle_t handle ) noexcept
	{
		if ( handle.pOwner != this || handle.nToken == 0 ) {
			return false;
		}
		for ( uint32_t i = 0; i < m_nCount; ++i ) {
			if ( m_nTokens[i] == handle.nToken && m_nStates[i] != SIR_EVENT_ENTRY_DEAD ) {
				if ( m_nStates[i] == SIR_EVENT_ENTRY_ACTIVE ) {
					--m_nActive;
				}
				m_nStates[i] = SIR_EVENT_ENTRY_DEAD;
				m_bDirty = true;
				if ( m_nDepth == 0 ) {
					compact();
				}
				return true;
			}
		}
		return false;
	}

	bool contains( sirSubscriptionHandle_t handle ) const noexcept
	{
		if ( handle.pOwner != this || handle.nToken == 0 ) {
			return false;
		}
		for ( uint32_t i = 0; i < m_nCount; ++i ) {
			if ( m_nTokens[i] == handle.nToken && m_nStates[i] != SIR_EVENT_ENTRY_DEAD ) {
				return true;
			}
		}
		return false;
	}

	void clear( void ) noexcept
	{
		for ( uint32_t i = 0; i < m_nCount; ++i ) {
			m_nStates[i] = SIR_EVENT_ENTRY_DEAD;
		}
		m_nActive = 0;
		if ( m_nDepth == 0 ) {
			m_nCount = 0;
			m_bDirty = false;
		} else {
			m_bDirty = true;
		}
	}

	void publish( const TPayload& payload ) noexcept
	{
		if ( m_nActive == 0 ) {
			return;
		}
		++m_nDepth;
		const uint32_t nLimit = m_nCount;
		for ( uint32_t i = 0; i < nLimit; ++i ) {
			if ( m_nStates[i] == SIR_EVENT_ENTRY_ACTIVE ) {
				const callback_t pCallback = m_Callbacks[i].pInvoke;
				void *pContext = m_Callbacks[i].pContext;
				pCallback( pContext, payload );
			}
		}
		--m_nDepth;
		if ( m_nDepth == 0 && m_bDirty ) {
			compact();
		}
	}

	uint32_t subscriberCount( void ) const noexcept
	{
		return m_nActive;
	}
	uint32_t capacity( void ) const noexcept
	{
		return Capacity;
	}

private:
	void compact( void ) noexcept
	{
		uint32_t nWrite = 0;
		for ( uint32_t i = 0; i < m_nCount; ++i ) {
			if ( m_nStates[i] == SIR_EVENT_ENTRY_DEAD ) {
				continue;
			}
			m_Callbacks[nWrite] = m_Callbacks[i];
			m_nTokens[nWrite] = m_nTokens[i];
			m_nStates[nWrite] = SIR_EVENT_ENTRY_ACTIVE;
			++nWrite;
		}
		m_nCount = nWrite;
		m_nActive = nWrite;
		m_bDirty = false;
	}

	sirEventDelegate_t<TPayload> m_Callbacks[Capacity];
	uint64_t m_nTokens[Capacity];
	uint8_t m_nStates[Capacity];
	uint32_t m_nCount;
	uint32_t m_nActive;
	uint32_t m_nDepth;
	bool m_bDirty;
	inline static std::atomic<uint64_t> s_nNextToken{ 1 };
};
#endif // __SIRENGINE_SIREVENT_H__
