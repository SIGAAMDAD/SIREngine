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

#ifndef __SIRENGINE_SIRDEFERREDEVENTCHANNEL_H__
#define __SIRENGINE_SIRDEFERREDEVENTCHANNEL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREvent.h>
#include <Core/Events/SIREventStream.h>
#include <stdint.h>

// Owner-thread deferred publication. Callback subscription remains independent
// of buffered packet lifetime; a callback may append for the next pump.
template <typename TPayload, uint32_t QueueCapacity = 256, uint32_t ListenerCapacity = 8, bool FilterUnobserved = false>
class SIRDeferredEventChannel
{
public:
	typedef typename SIREvent<TPayload, ListenerCapacity>::callback_t callback_t;
	SIRDeferredEventChannel( void ) noexcept = default;
	SIRDeferredEventChannel( const SIRDeferredEventChannel& ) = delete;
	SIRDeferredEventChannel& operator=( const SIRDeferredEventChannel& ) = delete;

	sirSubscriptionHandle_t subscribe( void *pContext, callback_t pCallback ) noexcept
	{
		return m_Event.subscribe( pContext, pCallback );
	}
	bool unsubscribe( sirSubscriptionHandle_t handle ) noexcept
	{
		return m_Event.unsubscribe( handle );
	}
	bool enqueue( const TPayload& payload ) noexcept
	{
		if constexpr ( FilterUnobserved ) {
			if ( m_Event.subscriberCount() == 0 ) {
				return false;
			}
		}
		return m_Stream.append( payload );
	}
	uint32_t enqueueBatch( const TPayload *pPayloads, uint32_t nCount ) noexcept
	{
		if constexpr ( FilterUnobserved ) {
			if ( m_Event.subscriberCount() == 0 ) {
				return 0;
			}
		}
		return m_Stream.appendBatch( pPayloads, nCount );
	}
	uint32_t pump( void ) noexcept
	{
		return m_Stream.drain( this, []( void *pContext, const TPayload& payload ) noexcept {
			( ( SIRDeferredEventChannel * )pContext )->m_Event.publish( payload );
		} );
	}
	uint32_t pendingCount( void ) const noexcept
	{
		return m_Stream.pendingCount();
	}
	uint32_t subscriberCount( void ) const noexcept
	{
		return m_Event.subscriberCount();
	}
	void clearPending( void ) noexcept
	{
		m_Stream.clear();
	}

private:
	SIREvent<TPayload, ListenerCapacity> m_Event;
	SIREventStream<TPayload, QueueCapacity> m_Stream;
};

#endif // __SIRENGINE_SIRDEFERREDEVENTCHANNEL_H__
