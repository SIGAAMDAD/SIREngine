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

#ifndef __SIRENGINE_SIRASYNCEVENTCHANNEL_H__
#define __SIRENGINE_SIRASYNCEVENTCHANNEL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREvent.h>
#include <Core/Events/SIRSPSCEventRing.h>
#include <stdint.h>

// A single producer writes typed POD events, and the subscriber owner drains.
// For N producers, give each producer an independent channel; do not share an
// SPSC producer cursor. Request completion belongs to a separate SIR Requests API.
// When FilterUnobserved is enabled, a producer can drop notifications that
// have no registered listeners at publication time. This intentionally
// changes late-subscription delivery semantics; default is lossless buffering.
template <typename TPayload, uint32_t QueueCapacity = 256, uint32_t ListenerCapacity = 8, bool FilterUnobserved = false>
class SIRAsyncEventChannel
{
public:
	typedef typename SIREvent<TPayload, ListenerCapacity>::callback_t callback_t;
	SIRAsyncEventChannel( void ) noexcept = default;
	SIRAsyncEventChannel( const SIRAsyncEventChannel& ) = delete;
	SIRAsyncEventChannel& operator=( const SIRAsyncEventChannel& ) = delete;

	sirSubscriptionHandle_t subscribe( void *pContext, callback_t pCallback ) noexcept
	{
		const sirSubscriptionHandle_t handle = m_Event.subscribe( pContext, pCallback );
		if constexpr ( FilterUnobserved ) {
			if ( handle.isValid() ) {
				m_bObserved.store( true, SIR_MEMORY_ORDER_RELEASE );
			}
		}
		return handle;
	}
	bool unsubscribe( sirSubscriptionHandle_t handle ) noexcept
	{
		const bool bRemoved = m_Event.unsubscribe( handle );
		if constexpr ( FilterUnobserved ) {
			if ( bRemoved ) {
				m_bObserved.store( m_Event.subscriberCount() != 0, SIR_MEMORY_ORDER_RELEASE );
			}
		}
		return bRemoved;
	}
	sirEventEnqueueStatus_t tryPublish( const TPayload& payload ) noexcept
	{
		if constexpr ( FilterUnobserved ) {
			if ( !m_bObserved.load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
				return SIR_EVENT_ENQUEUE_NO_LISTENERS;
			}
		}
		return m_Ring.tryEnqueue( payload );
	}
	uint32_t publishBatch( const TPayload *pPayloads, uint32_t nCount ) noexcept
	{
		if constexpr ( FilterUnobserved ) {
			if ( !m_bObserved.load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
				return 0;
			}
		}
		return m_Ring.enqueueBatch( pPayloads, nCount );
	}
	uint32_t pump( uint32_t nBudget = UINT32_MAX ) noexcept
	{
		return m_Ring.drainBatch( [this]( const TPayload& payload ) noexcept { m_Event.publish( payload ); }, nBudget );
	}
	void stop( void ) noexcept
	{
		m_Ring.stop();
	}
	uint64_t approximatePendingCount( void ) const noexcept
	{
		return m_Ring.approximateCount();
	}
	uint32_t subscriberCount( void ) const noexcept
	{
		return m_Event.subscriberCount();
	}

private:
	SIREvent<TPayload, ListenerCapacity> m_Event;
	SIRSPSCEventRing<TPayload, QueueCapacity> m_Ring;
	SIRAtomic<bool> m_bObserved{ false };
};

#endif // __SIRENGINE_SIRASYNCEVENTCHANNEL_H__
