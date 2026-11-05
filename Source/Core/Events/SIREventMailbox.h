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

#ifndef __SIRENGINE_SIREVENTMAILBOX_H__
#define __SIRENGINE_SIREVENTMAILBOX_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIRSPSCEventRing.h>
#include <Core/Events/SIREventTypes.h>
#include <stdint.h>

typedef struct sirEventProducerHandle_s
{
	const void *pOwner;
	uint32_t nSlot;
	uint32_t nGeneration;

	constexpr bool isValid( void ) const noexcept
	{
		return pOwner != nullptr && nSlot != 0 && nGeneration != 0;
	}
} sirEventProducerHandle_t;

// Register all producers on the owner thread, then seal before starting worker
// threads. Each token belongs exclusively to one producer thread; never share
// a producer token across threads. The owner alone pumps the inbox.
// No slot reuse after sealing; stop and join workers before destruction.
template <typename TPayload, uint32_t ProducerCapacity = 8, uint32_t QueueCapacity = 256>
class SIREventMailbox
{
public:
	static_assert( ProducerCapacity > 0, "Mailbox requires producer slots" );
	static_assert( ProducerCapacity <= 65535, "Mailbox producer slot must fit in a 16-bit index" );

	SIREventMailbox( void ) noexcept : m_nRegistered( 0 ), m_nCursor( 0 ), m_bSealed( false ), m_bPumping( false ), m_bStopped( false )
	{
	}
	SIREventMailbox( const SIREventMailbox& ) = delete;
	SIREventMailbox& operator=( const SIREventMailbox& ) = delete;

	sirEventProducerHandle_t registerProducer( void ) noexcept
	{
		if ( m_bSealed || m_nRegistered == ProducerCapacity ) {
			return {};
		}
		const uint32_t nSlot = m_nRegistered++;
		return { this, nSlot + 1, 1 };
	}

	bool seal( void ) noexcept
	{
		if ( m_bSealed || m_nRegistered == 0 ) {
			return false;
		}
		m_bSealed = true;
		return true;
	}

	sirEventEnqueueStatus_t tryPublish( sirEventProducerHandle_t producer, const TPayload& payload ) noexcept
	{
		if ( !validProducer( producer ) || !m_bSealed ) {
			return SIR_EVENT_ENQUEUE_STOPPED;
		}
		return m_Rings[producer.nSlot - 1].tryEnqueue( payload );
	}

	uint32_t publishBatch( sirEventProducerHandle_t producer, const TPayload *pPayloads, uint32_t nCount ) noexcept
	{
		if ( !validProducer( producer ) || !m_bSealed ) {
			return 0;
		}
		return m_Rings[producer.nSlot - 1].enqueueBatch( pPayloads, nCount );
	}

	// Round-robin with a per-producer quantum; avoids starvation during bursts.
	// Each call touches at most one quantum per registered producer.
	template <typename TCallback>
	uint32_t pump( TCallback callback, uint32_t nBudget = UINT32_MAX, uint32_t nQuantum = 32 ) noexcept
	{
		if ( !m_bSealed || m_bPumping || nBudget == 0 || nQuantum == 0 ) {
			return 0;
		}
		m_bPumping = true;
		uint32_t nDelivered = 0;
		const uint32_t nStart = m_nCursor;
		for ( uint32_t i = 0; i < m_nRegistered && nDelivered < nBudget; ++i ) {
			const uint32_t nProducer = ( nStart + i ) % m_nRegistered;
			const uint32_t nRemaining = nBudget - nDelivered;
			const uint32_t nMax = nRemaining < nQuantum ? nRemaining : nQuantum;
			nDelivered += m_Rings[nProducer].drainBatch( callback, nMax );
			m_nCursor = ( nProducer + 1 ) % m_nRegistered;
		}
		m_bPumping = false;
		return nDelivered;
	}

	// Pass contiguous producer-local spans to one batch consumer. The callback
	// receives a zero-based producer index and a borrowed payload range.
	template <typename TCallback>
	uint32_t pumpSpans( TCallback callback, uint32_t nBudget = UINT32_MAX, uint32_t nQuantum = 32 ) noexcept
	{
		if ( !m_bSealed || m_bPumping || nBudget == 0 || nQuantum == 0 ) {
			return 0;
		}
		m_bPumping = true;
		uint32_t nDelivered = 0;
		const uint32_t nStart = m_nCursor;
		for ( uint32_t i = 0; i < m_nRegistered && nDelivered < nBudget; ++i ) {
			const uint32_t nProducer = ( nStart + i ) % m_nRegistered;
			const uint32_t nRemaining = nBudget - nDelivered;
			const uint32_t nMax = nRemaining < nQuantum ? nRemaining : nQuantum;
			nDelivered += m_Rings[nProducer].drainSpans(
			    [&]( const TPayload *pData, uint32_t nCount ) noexcept { callback( nProducer, pData, nCount ); }, nMax );
			m_nCursor = ( nProducer + 1 ) % m_nRegistered;
		}
		m_bPumping = false;
		return nDelivered;
	}

	void stop( void ) noexcept
	{
		m_bStopped.store( true, SIR_MEMORY_ORDER_RELEASE );
		for ( uint32_t i = 0; i < m_nRegistered; ++i ) {
			m_Rings[i].stop();
		}
	}

	uint64_t approximatePendingCount( void ) const noexcept
	{
		uint64_t nCount = 0;
		for ( uint32_t i = 0; i < m_nRegistered; ++i ) {
			nCount += m_Rings[i].approximateCount();
		}
		return nCount;
	}

private:
	bool validProducer( sirEventProducerHandle_t handle ) const noexcept
	{
		return handle.pOwner == this && handle.nSlot != 0 && handle.nSlot <= m_nRegistered && handle.nGeneration == 1 &&
		       !m_bStopped.load( SIR_MEMORY_ORDER_ACQUIRE );
	}

	SIRSPSCEventRing<TPayload, QueueCapacity> m_Rings[ProducerCapacity];
	uint32_t m_nRegistered;
	uint32_t m_nCursor;
	bool m_bSealed;
	bool m_bPumping;
	SIRAtomic<bool> m_bStopped;
};

#endif // __SIRENGINE_SIREVENTMAILBOX_H__
