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

#ifndef __SIRENGINE_SIRSTATICEVENT_H__
#define __SIRENGINE_SIRSTATICEVENT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <stdint.h>

// Immutable after seal: no atomics, tokens, state checks, allocations or
// mutations on the publication path. Seal before exposing the event to readers.
// Subscribers and their targets must outlive all publishing readers.
template <typename TPayload, uint32_t Capacity = 8>
class SIRStaticEvent
{
public:
	typedef void ( *callback_t )( void *, const TPayload& ) noexcept;
	static_assert( Capacity > 0, "Static events need at least one listener" );

	SIRStaticEvent( void ) noexcept : m_nCount( 0 ), m_bSealed( false )
	{
	}
	SIRStaticEvent( const SIRStaticEvent& ) = delete;
	SIRStaticEvent& operator=( const SIRStaticEvent& ) = delete;

	bool subscribe( void *pContext, callback_t pCallback ) noexcept
	{
		if ( m_bSealed || pCallback == nullptr || m_nCount >= Capacity ) {
			return false;
		}
		m_Callbacks[m_nCount++] = { pContext, pCallback };
		return true;
	}

	template <typename TObject, void ( TObject::*Method )( const TPayload& ) noexcept>
	bool subscribe( TObject *pObject ) noexcept
	{
		return subscribe( pObject,
		                  []( void *pContext, const TPayload& payload ) noexcept { ( ( ( TObject * )pContext )->*Method )( payload ); } );
	}

	template <typename TObject, void ( TObject::*Method )( const TPayload& ) const noexcept>
	bool subscribe( const TObject *pObject ) noexcept
	{
		return subscribe( ( void * )pObject, []( void *pContext, const TPayload& payload ) noexcept {
			( ( ( const TObject * )pContext )->*Method )( payload );
		} );
	}

	bool seal( void ) noexcept
	{
		if ( m_bSealed ) {
			return false;
		}
		m_bSealed = true;
		return true;
	}

	bool publish( const TPayload& payload ) const noexcept
	{
		if ( !m_bSealed ) {
			return false;
		}
		for ( uint32_t i = 0; i < m_nCount; ++i ) {
			m_Callbacks[i].pInvoke( m_Callbacks[i].pContext, payload );
		}
		return true;
	}

	uint32_t subscriberCount( void ) const noexcept
	{
		return m_nCount;
	}

private:
	sirEventDelegate_t<TPayload> m_Callbacks[Capacity];
	uint32_t m_nCount;
	bool m_bSealed;
};

#endif // __SIRENGINE_SIRSTATICEVENT_H__
