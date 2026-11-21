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

#include <Scripting/Core/SIRScriptIngress.h>

#include <Core/Platform/SIRPlatformProcess.h>

SIRScriptIngress::SIRScriptIngress( void ) noexcept : m_nActiveProducers( 0 ), m_bClosed( false )
{
	for ( uint32_t iPhase = 0; iPhase < SIR_SCRIPT_PHASE_COUNT; ++iPhase ) {
		sirScriptIngressQueue_t& queue = m_szQueues[iPhase];
		queue.nHead = 0;
		queue.nTail.store( 0, SIR_MEMORY_ORDER_RELAXED );
		queue.nCount.store( 0, SIR_MEMORY_ORDER_RELAXED );
		for ( uint32_t iSlot = 0; iSlot < CAPACITY; ++iSlot ) {
			queue.szSlots[iSlot].nSequence.store( iSlot, SIR_MEMORY_ORDER_RELAXED );
		}
	}
}

sirScriptResult_t SIRScriptIngress::tryPost( sirScriptPhase_t ePhase, const sirScriptIngressPacket_t& packet ) noexcept
{
	if ( ePhase >= SIR_SCRIPT_PHASE_COUNT || packet.nEventTypeId == 0 || packet.nBytes > sizeof( packet.szPayload ) ||
	     packet.nAlignment == 0 || ( packet.nAlignment & ( packet.nAlignment - 1 ) ) != 0 ||
	     packet.nAlignment > alignof( sirScriptIngressPacket_t ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( m_bClosed.load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
		return SIR_SCRIPT_BUSY;
	}
	m_nActiveProducers.fetchAdd( 1, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
	if ( m_bClosed.load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
		m_nActiveProducers.fetchSub( 1, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
		return SIR_SCRIPT_BUSY;
	}

	sirScriptIngressQueue_t& queue = m_szQueues[ePhase];
	uint64_t nPosition = queue.nTail.load( SIR_MEMORY_ORDER_RELAXED );
	for ( ;; ) {
		sirScriptIngressSlot_t& slot = queue.szSlots[nPosition & ( CAPACITY - 1 )];
		const uint64_t nSequence = slot.nSequence.load( SIR_MEMORY_ORDER_ACQUIRE );
		const int64_t nDifference = ( int64_t )( nSequence - nPosition );
		if ( nDifference == 0 ) {
			uint64_t nExpected = nPosition;
			if ( queue.nTail.compareExchangeWeak( nExpected, nPosition + 1, SIR_MEMORY_ORDER_RELAXED ) ) {
				queue.nCount.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
				slot.packet = packet;
				slot.nSequence.store( nPosition + 1, SIR_MEMORY_ORDER_RELEASE );
				m_nActiveProducers.fetchSub( 1, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
				return SIR_SCRIPT_OK;
			}
			nPosition = nExpected;
		} else if ( nDifference < 0 ) {
			m_nActiveProducers.fetchSub( 1, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
			return SIR_SCRIPT_CAPACITY_EXCEEDED;
		} else {
			nPosition = queue.nTail.load( SIR_MEMORY_ORDER_RELAXED );
		}
	}
}

bool SIRScriptIngress::tryPop( sirScriptPhase_t ePhase, uint64_t nEndTicket, sirScriptIngressPacket_t *pPacket ) noexcept
{
	if ( ePhase >= SIR_SCRIPT_PHASE_COUNT || pPacket == nullptr ) {
		return false;
	}
	sirScriptIngressQueue_t& queue = m_szQueues[ePhase];
	const uint64_t nPosition = queue.nHead;
	if ( nPosition >= nEndTicket ) {
		return false;
	}
	sirScriptIngressSlot_t& slot = queue.szSlots[nPosition & ( CAPACITY - 1 )];
	if ( slot.nSequence.load( SIR_MEMORY_ORDER_ACQUIRE ) != nPosition + 1 ) {
		return false;
	}
	*pPacket = slot.packet;
	slot.nSequence.store( nPosition + CAPACITY, SIR_MEMORY_ORDER_RELEASE );
	queue.nHead = nPosition + 1;
	queue.nCount.fetchSub( 1, SIR_MEMORY_ORDER_RELAXED );
	return true;
}

uint64_t SIRScriptIngress::snapshotTail( sirScriptPhase_t ePhase ) const noexcept
{
	return ePhase < SIR_SCRIPT_PHASE_COUNT ? m_szQueues[ePhase].nTail.load( SIR_MEMORY_ORDER_ACQUIRE ) : 0;
}

uint32_t SIRScriptIngress::pendingCount( sirScriptPhase_t ePhase ) const noexcept
{
	return ePhase < SIR_SCRIPT_PHASE_COUNT ? m_szQueues[ePhase].nCount.load( SIR_MEMORY_ORDER_RELAXED ) : 0;
}

void SIRScriptIngress::close( void ) noexcept
{
	m_bClosed.store( true, SIR_MEMORY_ORDER_RELEASE );
	while ( m_nActiveProducers.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 ) {
		SIRPlatformProcess::yieldThread();
	}
}

void SIRScriptIngress::clear( void ) noexcept
{
	for ( uint32_t iPhase = 0; iPhase < SIR_SCRIPT_PHASE_COUNT; ++iPhase ) {
		const sirScriptPhase_t ePhase = ( sirScriptPhase_t )iPhase;
		sirScriptIngressPacket_t packet = {};
		const uint64_t nEndTicket = snapshotTail( ePhase );
		while ( tryPop( ePhase, nEndTicket, &packet ) ) {
		}
	}
}
