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

#include <Scripting/Core/SIRScriptScheduler.h>

#include <string.h>

SIRScriptScheduler::SIRScriptScheduler( void ) noexcept : m_nFreeHead( 1 ), m_nSequence( 0 )
{
	memset( m_szSlots, 0, sizeof( m_szSlots ) );
	memset( m_szHeads, 0, sizeof( m_szHeads ) );
	memset( m_szTails, 0, sizeof( m_szTails ) );
	memset( m_szCounts, 0, sizeof( m_szCounts ) );
	for ( uint32_t iSlot = 1; iSlot <= MAX_TASKS; ++iSlot ) {
		m_szSlots[iSlot].nNext = iSlot == MAX_TASKS ? 0 : iSlot + 1;
	}
}

uint32_t SIRScriptScheduler::nextGeneration( uint32_t nGeneration ) noexcept
{
	++nGeneration;
	return nGeneration == 0 ? 1 : nGeneration;
}

void SIRScriptScheduler::release( uint32_t nSlot ) noexcept
{
	sirScriptScheduledSlot_t &slot = m_szSlots[nSlot];
	slot.bActive = false;
	slot.nGeneration = nextGeneration( slot.nGeneration );
	slot.nNext = m_nFreeHead;
	m_nFreeHead = nSlot;
}

sirScriptResult_t SIRScriptScheduler::enqueue( sirScriptPhase_t ePhase, const sirScriptScheduledCall_t &call,
                                               sirScriptTaskHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr || ePhase >= SIR_SCRIPT_PHASE_COUNT ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( call.nDispatchId == 0 || call.nBytes > sizeof( call.szPayload ) || call.nAlignment == 0 ||
	     ( call.nAlignment & ( call.nAlignment - 1 ) ) != 0 || call.nAlignment > alignof( sirScriptScheduledCall_t ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( call.eKind != SIR_SCRIPT_SCHEDULED_EVENT && call.eKind != SIR_SCRIPT_SCHEDULED_INVOKE ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( m_nFreeHead == 0 || m_nSequence == UINT64_MAX ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}

	const uint32_t nSlot = m_nFreeHead;
	sirScriptScheduledSlot_t &slot = m_szSlots[nSlot];
	m_nFreeHead = slot.nNext;
	slot.call = call;
	slot.bActive = true;
	slot.nSequence = ++m_nSequence;
	slot.nGeneration = nextGeneration( slot.nGeneration );
	slot.nNext = 0;
	if ( m_szTails[ePhase] != 0 ) {
		m_szSlots[m_szTails[ePhase]].nNext = nSlot;
	} else {
		m_szHeads[ePhase] = nSlot;
	}
	m_szTails[ePhase] = nSlot;
	++m_szCounts[ePhase];
	*pHandle = { nSlot, slot.nGeneration };
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptScheduler::cancel( sirScriptTaskHandle_t handle ) noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > MAX_TASKS ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	const sirScriptScheduledSlot_t &slot = m_szSlots[handle.nSlot];
	if ( !slot.bActive || slot.nGeneration != handle.nGeneration ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}

	for ( uint32_t iPhase = 0; iPhase < SIR_SCRIPT_PHASE_COUNT; ++iPhase ) {
		uint32_t nPrevious = 0;
		uint32_t nCurrent = m_szHeads[iPhase];
		while ( nCurrent != 0 ) {
			const uint32_t nNext = m_szSlots[nCurrent].nNext;
			if ( nCurrent == handle.nSlot ) {
				if ( nPrevious != 0 ) {
					m_szSlots[nPrevious].nNext = nNext;
				} else {
					m_szHeads[iPhase] = nNext;
				}
				if ( m_szTails[iPhase] == nCurrent ) {
					m_szTails[iPhase] = nPrevious;
				}
				--m_szCounts[iPhase];
				release( nCurrent );
				return SIR_SCRIPT_OK;
			}
			nPrevious = nCurrent;
			nCurrent = nNext;
		}
	}
	return SIR_SCRIPT_INVALID_HANDLE;
}

bool SIRScriptScheduler::dequeue( sirScriptPhase_t ePhase, uint64_t nLastSequence, sirScriptScheduledCall_t *pCall ) noexcept
{
	if ( pCall == nullptr || ePhase >= SIR_SCRIPT_PHASE_COUNT ) {
		return false;
	}
	const uint32_t nSlot = m_szHeads[ePhase];
	if ( nSlot == 0 || m_szSlots[nSlot].nSequence > nLastSequence ) {
		return false;
	}

	sirScriptScheduledSlot_t &slot = m_szSlots[nSlot];
	*pCall = slot.call;
	m_szHeads[ePhase] = slot.nNext;
	if ( m_szHeads[ePhase] == 0 ) {
		m_szTails[ePhase] = 0;
	}
	--m_szCounts[ePhase];
	release( nSlot );
	return true;
}

void SIRScriptScheduler::clear( void ) noexcept
{
	for ( uint32_t iPhase = 0; iPhase < SIR_SCRIPT_PHASE_COUNT; ++iPhase ) {
		while ( m_szHeads[iPhase] != 0 ) {
			const uint32_t nSlot = m_szHeads[iPhase];
			m_szHeads[iPhase] = m_szSlots[nSlot].nNext;
			release( nSlot );
		}
		m_szTails[iPhase] = 0;
		m_szCounts[iPhase] = 0;
	}
}

uint32_t SIRScriptScheduler::pendingCount( sirScriptPhase_t ePhase ) const noexcept
{
	return ePhase < SIR_SCRIPT_PHASE_COUNT ? m_szCounts[ePhase] : 0;
}

uint64_t SIRScriptScheduler::lastSequence( void ) const noexcept
{
	return m_nSequence;
}
