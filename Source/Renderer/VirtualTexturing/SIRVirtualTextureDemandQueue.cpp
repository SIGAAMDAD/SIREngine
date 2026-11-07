/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureDemandQueue.h>

// Ported from Terrain3D's terrain_3d_vt_demand_bundle_store.cpp slot policy.
// Source: requestCellHot(), selectQueuedLocked(), workerMain() commit state.
// The external scheduler supplies owner-lane serialization and worker joining.
SIRVirtualTextureDemandQueue::SIRVirtualTextureDemandQueue( void ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < SIR_VT_DEMAND_BUNDLE_SLOTS; ++iSlot ) {
		m_szSlots[iSlot].nBundle = UINT32_MAX;
		m_szSlots[iSlot].eState = SIR_VT_BUNDLE_FREE;
	}
}

uint64_t SIRVirtualTextureDemandQueue::getEpoch( void ) const noexcept
{
	return m_nEpoch;
}

int32_t SIRVirtualTextureDemandQueue::findSlot( uint32_t nBundle ) const noexcept
{
	for ( uint32_t iSlot = 0; iSlot < SIR_VT_DEMAND_BUNDLE_SLOTS; ++iSlot ) {
		const sirVirtualTextureDemandSlot_t& slot = m_szSlots[iSlot];
		if ( slot.nEpoch == m_nEpoch && slot.nBundle == nBundle && slot.eState != SIR_VT_BUNDLE_FREE &&
		     slot.eState != SIR_VT_BUNDLE_FAILED ) {
			return ( int32_t )iSlot;
		}
	}
	return -1;
}

void SIRVirtualTextureDemandQueue::advanceUseClock( void ) noexcept
{
	if ( m_nUseClock == UINT64_MAX ) {
		// Rebase ages without changing ordering; with four slots a bounded
		// pairwise rank calculation avoids overflow and any heap allocation.
		uint64_t szRank[SIR_VT_DEMAND_BUNDLE_SLOTS] = {};
		for ( uint32_t i = 0; i < SIR_VT_DEMAND_BUNDLE_SLOTS; ++i ) {
			for ( uint32_t j = 0; j < SIR_VT_DEMAND_BUNDLE_SLOTS; ++j ) {
				if ( m_szSlots[j].nAge < m_szSlots[i].nAge || ( m_szSlots[j].nAge == m_szSlots[i].nAge && j < i ) ) {
					++szRank[i];
				}
			}
		}
		for ( uint32_t i = 0; i < SIR_VT_DEMAND_BUNDLE_SLOTS; ++i ) {
			m_szSlots[i].nAge = szRank[i];
		}
		m_nUseClock = SIR_VT_DEMAND_BUNDLE_SLOTS;
	}
	++m_nUseClock;
}

bool SIRVirtualTextureDemandQueue::isReady( uint32_t nBundle ) const noexcept
{
	const int32_t iSlot = findSlot( nBundle );
	return iSlot >= 0 && m_szSlots[iSlot].eState == SIR_VT_BUNDLE_READY;
}

sirVirtualTextureDemandQueueStats_t SIRVirtualTextureDemandQueue::getStats( void ) const noexcept
{
	sirVirtualTextureDemandQueueStats_t stats = {};
	stats.nCoalesced = m_nCoalesced;
	stats.nEvictions = m_nEvictions;
	stats.nRejected = m_nRejected;
	for ( uint32_t iSlot = 0; iSlot < SIR_VT_DEMAND_BUNDLE_SLOTS; ++iSlot ) {
		const sirVirtualTextureDemandBundleState_t eState = m_szSlots[iSlot].eState;
		stats.nQueued += eState == SIR_VT_BUNDLE_QUEUED ? 1U : 0U;
		stats.nLoading += eState == SIR_VT_BUNDLE_LOADING ? 1U : 0U;
		stats.nReady += eState == SIR_VT_BUNDLE_READY ? 1U : 0U;
	}
	return stats;
}

bool SIRVirtualTextureDemandQueue::requestBundle( uint32_t nBundle, uint8_t nPriority ) noexcept
{
	if ( nBundle == UINT32_MAX ) {
		++m_nRejected;
		return false;
	}
	const int32_t iFound = findSlot( nBundle );
	if ( iFound >= 0 ) {
		sirVirtualTextureDemandSlot_t& slot = m_szSlots[iFound];
		if ( nPriority > slot.nPriority ) {
			slot.nPriority = nPriority;
		}
		advanceUseClock();
		slot.nAge = m_nUseClock;
		++m_nCoalesced;
		return true;
	}

	int32_t iVictim = -1;
	for ( uint32_t iSlot = 0; iSlot < SIR_VT_DEMAND_BUNDLE_SLOTS; ++iSlot ) {
		const sirVirtualTextureDemandSlot_t& slot = m_szSlots[iSlot];
		if ( slot.eState == SIR_VT_BUNDLE_FREE || slot.eState == SIR_VT_BUNDLE_FAILED ) {
			iVictim = ( int32_t )iSlot;
			break;
		}
		if ( slot.eState == SIR_VT_BUNDLE_READY && ( iVictim < 0 || slot.nAge < m_szSlots[iVictim].nAge ) ) {
			iVictim = ( int32_t )iSlot;
		}
	}
	if ( iVictim < 0 || m_nNextSerial == UINT64_MAX ) {
		++m_nRejected;
		return false;
	}
	sirVirtualTextureDemandSlot_t& slot = m_szSlots[iVictim];
	if ( slot.eState == SIR_VT_BUNDLE_READY ) {
		++m_nEvictions;
	}
	advanceUseClock();
	slot.nBundle = nBundle;
	slot.nPriority = nPriority;
	slot.nAge = m_nUseClock;
	slot.nEpoch = m_nEpoch;
	slot.nSerial = ++m_nNextSerial;
	slot.eState = SIR_VT_BUNDLE_QUEUED;
	return true;
}

bool SIRVirtualTextureDemandQueue::beginLoad( sirVirtualTextureDemandTicket_t *pTicket ) noexcept
{
	if ( pTicket == nullptr ) {
		return false;
	}
	*pTicket = {};
	int32_t iSelected = -1;
	for ( uint32_t iSlot = 0; iSlot < SIR_VT_DEMAND_BUNDLE_SLOTS; ++iSlot ) {
		const sirVirtualTextureDemandSlot_t& slot = m_szSlots[iSlot];
		if ( slot.eState != SIR_VT_BUNDLE_QUEUED ) {
			continue;
		}
		if ( iSelected < 0 || slot.nPriority > m_szSlots[iSelected].nPriority ||
		     ( slot.nPriority == m_szSlots[iSelected].nPriority && slot.nAge < m_szSlots[iSelected].nAge ) ) {
			iSelected = ( int32_t )iSlot;
		}
	}
	if ( iSelected < 0 ) {
		return false;
	}
	sirVirtualTextureDemandSlot_t& slot = m_szSlots[iSelected];
	slot.eState = SIR_VT_BUNDLE_LOADING;
	*pTicket = { ( uint32_t )iSelected, slot.nBundle, slot.nEpoch, slot.nSerial };
	return true;
}

bool SIRVirtualTextureDemandQueue::finishLoad( const sirVirtualTextureDemandTicket_t& ticket, bool bSucceeded ) noexcept
{
	if ( ticket.nSlot >= SIR_VT_DEMAND_BUNDLE_SLOTS || ticket.nEpoch != m_nEpoch ) {
		return false;
	}
	sirVirtualTextureDemandSlot_t& slot = m_szSlots[ticket.nSlot];
	if ( slot.eState != SIR_VT_BUNDLE_LOADING || slot.nBundle != ticket.nBundle || slot.nEpoch != ticket.nEpoch ||
	     slot.nSerial != ticket.nSerial ) {
		return false;
	}
	slot.eState = bSucceeded ? SIR_VT_BUNDLE_READY : SIR_VT_BUNDLE_FAILED;
	return true;
}

bool SIRVirtualTextureDemandQueue::resetCold( void ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < SIR_VT_DEMAND_BUNDLE_SLOTS; ++iSlot ) {
		if ( m_szSlots[iSlot].eState == SIR_VT_BUNDLE_LOADING ) {
			return false;
		}
	}
	++m_nEpoch;
	if ( m_nEpoch == 0 ) {
		m_nEpoch = 1;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VT_DEMAND_BUNDLE_SLOTS; ++iSlot ) {
		m_szSlots[iSlot] = {};
		m_szSlots[iSlot].nBundle = UINT32_MAX;
		m_szSlots[iSlot].eState = SIR_VT_BUNDLE_FREE;
	}
	m_nUseClock = 0;
	m_nCoalesced = 0;
	m_nEvictions = 0;
	m_nRejected = 0;
	return true;
}
