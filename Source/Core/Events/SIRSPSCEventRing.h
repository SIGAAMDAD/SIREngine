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

#ifndef __SIRENGINE_SIRSPSCEVENTRING_H__
#define __SIRENGINE_SIRSPSCEVENTRING_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <Core/Platform/SIRAtomic.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

template <typename TPayload>
struct sirEventReadSpan_s
{
	const TPayload *pData;
	uint32_t nCount;
};

typedef struct sirSPSCProducerState_s
{
	SIRAtomic<uint64_t> nTail{ 0 };
	uint64_t nHeadCache;
} sirSPSCProducerState_t;

typedef struct sirSPSCConsumerState_s
{
	SIRAtomic<uint64_t> nHead{ 0 };
	uint64_t nTailCache;
	uint32_t nReserved;
	bool bDraining;
} sirSPSCConsumerState_t;

// Exactly one producer and one consumer. Publish producer/consumer cursors once
// per batch. The producer must not touch the consumer's local cursor or vice
// versa. Payloads remain reserved until a drain callback returns.
// stop() may race with an in-progress publish; join producers before destroy.
template <typename TPayload, uint32_t Capacity = 256>
class SIRSPSCEventRing
{
public:
	static_assert( Capacity > 1 && ( Capacity & ( Capacity - 1 ) ) == 0, "Ring capacity must be a power of two" );
	static_assert( std::is_trivially_copyable<TPayload>::value, "Ring payload must be trivially copyable" );

	SIRSPSCEventRing( void ) noexcept : m_Producer{}, m_Consumer{}, m_bStopped( false )
	{
	}
	SIRSPSCEventRing( const SIRSPSCEventRing& ) = delete;
	SIRSPSCEventRing& operator=( const SIRSPSCEventRing& ) = delete;

	sirEventEnqueueStatus_t tryEnqueue( const TPayload& payload ) noexcept
	{
		if ( m_bStopped.load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
			return SIR_EVENT_ENQUEUE_STOPPED;
		}
		return enqueueBatchUnchecked( &payload, 1 ) == 1 ? SIR_EVENT_ENQUEUE_OK : SIR_EVENT_ENQUEUE_FULL;
	}

	// Returns the accepted prefix length. Zero can mean full, stopped or nCount=0.
	uint32_t enqueueBatch( const TPayload *pPayloads, uint32_t nCount ) noexcept
	{
		if ( pPayloads == nullptr || nCount == 0 || m_bStopped.load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
			return 0;
		}
		return enqueueBatchUnchecked( pPayloads, nCount );
	}

	bool tryDequeue( TPayload& output ) noexcept
	{
		if ( m_Consumer.bDraining ) {
			return false;
		}
		return dequeueBatch( &output, 1 ) == 1;
	}

	uint32_t dequeueBatch( TPayload *pOutput, uint32_t nCount ) noexcept
	{
		if ( pOutput == nullptr || nCount == 0 || m_Consumer.bDraining ) {
			return 0;
		}
		const uint64_t nHead = m_Consumer.nHead.load( SIR_MEMORY_ORDER_RELAXED );
		const uint32_t nAvailable = availableForConsumer( nHead, nCount );
		if ( nAvailable == 0 ) {
			return 0;
		}
		const uint32_t nStart = ( uint32_t )( nHead & ( Capacity - 1 ) );
		const uint32_t nFirst = nAvailable < Capacity - nStart ? nAvailable : Capacity - nStart;
		memcpy( pOutput, &m_Events[nStart], nFirst * sizeof( TPayload ) );
		if ( nFirst < nAvailable ) {
			memcpy( pOutput + nFirst, m_Events, ( nAvailable - nFirst ) * sizeof( TPayload ) );
		}
		m_Consumer.nHead.store( nHead + nAvailable, SIR_MEMORY_ORDER_RELEASE );
		return nAvailable;
	}

	template <typename TCallback>
	uint32_t drainBatch( TCallback callback, uint32_t nBudget = UINT32_MAX ) noexcept
	{
		if ( m_Consumer.bDraining || nBudget == 0 ) {
			return 0;
		}
		m_Consumer.bDraining = true;
		const uint64_t nHead = m_Consumer.nHead.load( SIR_MEMORY_ORDER_RELAXED );
		const uint32_t nAvailable = availableForConsumer( nHead, nBudget );
		for ( uint32_t i = 0; i < nAvailable; ++i ) {
			callback( m_Events[( nHead + i ) & ( Capacity - 1 )] );
		}
		if ( nAvailable != 0 ) {
			m_Consumer.nHead.store( nHead + nAvailable, SIR_MEMORY_ORDER_RELEASE );
		}
		m_Consumer.bDraining = false;
		return nAvailable;
	}

	// Acquire up to two zero-copy contiguous spans. The data remain reserved
	// until commitRead(). A partial commit releases only the consumed prefix.
	// Only the consumer thread may hold a read lease.
	uint32_t acquireReadSpans( sirEventReadSpan_s<TPayload>& first, sirEventReadSpan_s<TPayload>& second,
	                           uint32_t nBudget = UINT32_MAX ) noexcept
	{
		first = { nullptr, 0 };
		second = { nullptr, 0 };
		if ( m_Consumer.bDraining || nBudget == 0 ) {
			return 0;
		}
		const uint64_t nHead = m_Consumer.nHead.load( SIR_MEMORY_ORDER_RELAXED );
		const uint32_t nAvailable = availableForConsumer( nHead, nBudget );
		if ( nAvailable == 0 ) {
			return 0;
		}
		const uint32_t nStart = ( uint32_t )( nHead & ( Capacity - 1 ) );
		const uint32_t nFirst = nAvailable < Capacity - nStart ? nAvailable : Capacity - nStart;
		first = { &m_Events[nStart], nFirst };
		if ( nFirst != nAvailable ) {
			second = { m_Events, nAvailable - nFirst };
		}
		m_Consumer.nReserved = nAvailable;
		m_Consumer.bDraining = true;
		return nAvailable;
	}

	bool commitRead( uint32_t nConsumed ) noexcept
	{
		if ( !m_Consumer.bDraining || nConsumed > m_Consumer.nReserved ) {
			return false;
		}
		if ( nConsumed != 0 ) {
			const uint64_t nHead = m_Consumer.nHead.load( SIR_MEMORY_ORDER_RELAXED );
			m_Consumer.nHead.store( nHead + nConsumed, SIR_MEMORY_ORDER_RELEASE );
		}
		m_Consumer.nReserved = 0;
		m_Consumer.bDraining = false;
		return true;
	}

	// Contiguous batch callback without copying or per-element dispatch.
	template <typename TCallback>
	uint32_t drainSpans( TCallback callback, uint32_t nBudget = UINT32_MAX ) noexcept
	{
		sirEventReadSpan_s<TPayload> first{};
		sirEventReadSpan_s<TPayload> second{};
		const uint32_t nCount = acquireReadSpans( first, second, nBudget );
		if ( nCount != 0 ) {
			callback( first.pData, first.nCount );
			if ( second.nCount != 0 ) {
				callback( second.pData, second.nCount );
			}
			commitRead( nCount );
		}
		return nCount;
	}

	template <typename TCallback>
	uint32_t drain( TCallback callback, uint32_t nBudget = UINT32_MAX ) noexcept
	{
		return drainBatch( callback, nBudget );
	}

	void stop( void ) noexcept
	{
		m_bStopped.store( true, SIR_MEMORY_ORDER_RELEASE );
	}

	uint64_t approximateCount( void ) const noexcept
	{
		const uint64_t nTail = m_Producer.nTail.load( SIR_MEMORY_ORDER_ACQUIRE );
		const uint64_t nHead = m_Consumer.nHead.load( SIR_MEMORY_ORDER_ACQUIRE );
		return nTail - nHead;
	}

private:
	uint32_t enqueueBatchUnchecked( const TPayload *pPayloads, uint32_t nCount ) noexcept
	{
		const uint64_t nTail = m_Producer.nTail.load( SIR_MEMORY_ORDER_RELAXED );
		uint64_t nUsed = nTail - m_Producer.nHeadCache;
		if ( nUsed >= Capacity || nCount > Capacity - nUsed ) {
			m_Producer.nHeadCache = m_Consumer.nHead.load( SIR_MEMORY_ORDER_ACQUIRE );
			nUsed = nTail - m_Producer.nHeadCache;
		}
		if ( nUsed >= Capacity ) {
			return 0;
		}
		const uint32_t nRoom = Capacity - ( uint32_t )nUsed;
		const uint32_t nWritten = nCount < nRoom ? nCount : nRoom;
		const uint32_t nStart = ( uint32_t )( nTail & ( Capacity - 1 ) );
		const uint32_t nFirst = nWritten < Capacity - nStart ? nWritten : Capacity - nStart;
		memcpy( &m_Events[nStart], pPayloads, nFirst * sizeof( TPayload ) );
		if ( nFirst < nWritten ) {
			memcpy( m_Events, pPayloads + nFirst, ( nWritten - nFirst ) * sizeof( TPayload ) );
		}
		m_Producer.nTail.store( nTail + nWritten, SIR_MEMORY_ORDER_RELEASE );
		return nWritten;
	}

	uint32_t availableForConsumer( uint64_t nHead, uint32_t nLimit ) noexcept
	{
		uint64_t nAvailable = m_Consumer.nTailCache - nHead;
		if ( nAvailable < nLimit ) {
			m_Consumer.nTailCache = m_Producer.nTail.load( SIR_MEMORY_ORDER_ACQUIRE );
			nAvailable = m_Consumer.nTailCache - nHead;
		}
		return nAvailable < nLimit ? ( uint32_t )nAvailable : nLimit;
	}

	alignas( 64 ) sirSPSCProducerState_t m_Producer;
	alignas( 64 ) sirSPSCConsumerState_t m_Consumer;
	SIRAtomic<bool> m_bStopped;
	// Slots are initialized only when published; skip eager payload zeroing.
	TPayload m_Events[Capacity];
};

#endif // __SIRENGINE_SIRSPSCEVENTRING_H__
