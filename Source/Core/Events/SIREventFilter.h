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

#ifndef __SIRENGINE_SIREVENTFILTER_H__
#define __SIRENGINE_SIREVENTFILTER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

// Optional cold-path filter/forwarding adapter, never part of the default dispatch loop.
template <typename TPayload>
class SIREventFilter
{
public:
	typedef bool ( *predicate_t )( void *pContext, const TPayload& payload ) noexcept;
	typedef void ( *callback_t )( void *pContext, const TPayload& payload ) noexcept;
	SIREventFilter( void *pContext, predicate_t pPredicate, callback_t pCallback, uint32_t nSkip = 0,
	                uint32_t nLimit = UINT32_MAX ) noexcept
	    : m_pContext( pContext ), m_pPredicate( pPredicate ), m_pCallback( pCallback ), m_nSkip( nSkip ), m_nLimit( nLimit ),
	      m_nDelivered( 0 )
	{
	}

	bool publish( const TPayload& payload ) noexcept
	{
		if ( m_pCallback == nullptr || m_nDelivered == m_nLimit || ( m_pPredicate != nullptr && !m_pPredicate( m_pContext, payload ) ) ) {
			return false;
		}
		if ( m_nSkip != 0 ) {
			--m_nSkip;
			return false;
		}
		++m_nDelivered;
		m_pCallback( m_pContext, payload );
		return true;
	}
	uint32_t deliveredCount( void ) const noexcept
	{
		return m_nDelivered;
	}

private:
	void *m_pContext;
	predicate_t m_pPredicate;
	callback_t m_pCallback;
	uint32_t m_nSkip;
	uint32_t m_nLimit;
	uint32_t m_nDelivered;
};

#endif // __SIRENGINE_SIREVENTFILTER_H__
