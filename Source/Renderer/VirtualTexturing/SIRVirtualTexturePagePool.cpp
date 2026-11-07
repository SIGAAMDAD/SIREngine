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

#include <Renderer/VirtualTexturing/SIRVirtualTexturePagePool.h>
#include <Renderer/VirtualTexturing/SIRVirtualTexturePackReader.h>

SIRVirtualTexturePagePool::SIRVirtualTexturePagePool( void ) noexcept = default;

bool SIRVirtualTexturePagePool::init( sirVirtualTexturePagePoolSlot_t *pSlots, uint32_t nSlotCount, uint32_t nPerFamily,
                                      uint32_t *pVirtualToSlot, uint32_t nVirtualPages, uint32_t nDeviceGeneration ) noexcept
{
	if ( m_pSlots != nullptr || pSlots == nullptr || pVirtualToSlot == nullptr || nSlotCount == 0 || nPerFamily == 0 ||
	     nVirtualPages == 0 || nDeviceGeneration == 0 || nPerFamily > UINT32_MAX / SIR_VT_PHYSICAL_COUNT ||
	     nSlotCount != nPerFamily * SIR_VT_PHYSICAL_COUNT ) {
		return false;
	}
	m_pSlots = pSlots;
	m_pVirtualToSlot = pVirtualToSlot;
	m_nSlotCount = nSlotCount;
	m_nPerFamily = nPerFamily;
	m_nVirtualPages = nVirtualPages;
	m_nDeviceGeneration = nDeviceGeneration;
	m_nNextTicket = 0;
	m_nUseClock = 0;
	m_nRejected = 0;
	m_nCancelled = 0;
	for ( uint32_t iPage = 0; iPage < nVirtualPages; ++iPage ) {
		m_pVirtualToSlot[iPage] = SIR_VT_POOL_INVALID_INDEX;
	}
	for ( uint32_t iSlot = 0; iSlot < nSlotCount; ++iSlot ) {
		sirVirtualTexturePagePoolSlot_t slot = {};
		slot.nVirtualPage = SIR_VT_POOL_INVALID_INDEX;
		slot.nPhysicalLayer = iSlot % nPerFamily;
		slot.nFamily = ( uint8_t )( iSlot / nPerFamily );
		slot.eState = SIR_VT_POOL_FREE;
		m_pSlots[iSlot] = slot;
	}
	return true;
}

bool SIRVirtualTexturePagePool::resetCold( void ) noexcept
{
	if ( m_pSlots == nullptr ) {
		return false;
	}
	for ( uint32_t iSlot = 0; iSlot < m_nSlotCount; ++iSlot ) {
		if ( m_pSlots[iSlot].eState != SIR_VT_POOL_FREE ) {
			return false;
		}
	}
	m_pSlots = nullptr;
	m_pVirtualToSlot = nullptr;
	m_nSlotCount = 0;
	m_nPerFamily = 0;
	m_nVirtualPages = 0;
	m_nDeviceGeneration = 0;
	return true;
}

bool SIRVirtualTexturePagePool::checkCompletion( const sirGraphicsCompletionSet_t& completion ) const noexcept
{
	if ( completion.nDeviceGeneration != m_nDeviceGeneration ||
	     ( completion.nQueueMask & ~( ( 1U << SIR_GRAPHICS_QUEUE_COUNT ) - 1U ) ) != 0 ) {
		return false;
	}
	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		if ( ( completion.nQueueMask & ( 1U << iQueue ) ) != 0 && completion.szQueueValues[iQueue] == 0 ) {
			return false;
		}
	}
	return true;
}

bool SIRVirtualTexturePagePool::isComplete( const sirGraphicsCompletionSet_t& pending,
                                            const sirGraphicsCompletionSet_t& completed ) const noexcept
{
	if ( !checkCompletion( completed ) || ( completed.nQueueMask & pending.nQueueMask ) != pending.nQueueMask ) {
		return false;
	}
	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		if ( ( pending.nQueueMask & ( 1U << iQueue ) ) != 0 && completed.szQueueValues[iQueue] < pending.szQueueValues[iQueue] ) {
			return false;
		}
	}
	return true;
}

bool SIRVirtualTexturePagePool::checkReservation( const sirVirtualTexturePagePoolReservation_t& reservation ) const noexcept
{
	if ( m_pSlots == nullptr || reservation.nSlot >= m_nSlotCount || reservation.nVirtualPage >= m_nVirtualPages ) {
		return false;
	}
	const sirVirtualTexturePagePoolSlot_t& slot = m_pSlots[reservation.nSlot];
	return slot.eState == SIR_VT_POOL_RESERVED && slot.nVirtualPage == reservation.nVirtualPage &&
	       slot.nGeneration == reservation.nGeneration && slot.nTicket == reservation.nTicket;
}

void SIRVirtualTexturePagePool::releaseSlot( uint32_t nSlot ) noexcept
{
	sirVirtualTexturePagePoolSlot_t& slot = m_pSlots[nSlot];
	if ( slot.nVirtualPage < m_nVirtualPages && m_pVirtualToSlot[slot.nVirtualPage] == nSlot ) {
		m_pVirtualToSlot[slot.nVirtualPage] = SIR_VT_POOL_INVALID_INDEX;
	}
	const uint32_t nLayer = slot.nPhysicalLayer;
	const uint8_t nFamily = slot.nFamily;
	slot = {};
	slot.nVirtualPage = SIR_VT_POOL_INVALID_INDEX;
	slot.nPhysicalLayer = nLayer;
	slot.nFamily = nFamily;
	slot.eState = SIR_VT_POOL_FREE;
}

bool SIRVirtualTexturePagePool::reserve( uint32_t nVirtualPage, sirVirtualTexturePhysicalFamily_t eFamily, uint32_t nGeneration,
                                         sirVirtualTexturePagePoolReservation_t *pReservation ) noexcept
{
	if ( m_pSlots == nullptr || pReservation == nullptr || nVirtualPage >= m_nVirtualPages ||
	     ( uint32_t )eFamily >= SIR_VT_PHYSICAL_COUNT || nGeneration == 0 || m_nNextTicket == UINT64_MAX ) {
		++m_nRejected;
		return false;
	}
	// Never reserve the same virtual page twice across upload, residency or
	// retirement: out-of-order callbacks cannot acquire a second physical alias.
	for ( uint32_t iSlot = 0; iSlot < m_nSlotCount; ++iSlot ) {
		if ( m_pSlots[iSlot].eState != SIR_VT_POOL_FREE && m_pSlots[iSlot].nVirtualPage == nVirtualPage ) {
			++m_nRejected;
			return false;
		}
	}
	const uint32_t nFirst = ( uint32_t )eFamily * m_nPerFamily;
	for ( uint32_t iSlot = nFirst; iSlot < nFirst + m_nPerFamily; ++iSlot ) {
		sirVirtualTexturePagePoolSlot_t& slot = m_pSlots[iSlot];
		if ( slot.eState != SIR_VT_POOL_FREE ) {
			continue;
		}
		slot.eState = SIR_VT_POOL_RESERVED;
		slot.nVirtualPage = nVirtualPage;
		slot.nGeneration = nGeneration;
		slot.nTicket = ++m_nNextTicket;
		slot.nAge = ++m_nUseClock;
		*pReservation = { iSlot, nVirtualPage, nGeneration, slot.nTicket };
		return true;
	}
	++m_nRejected;
	return false;
}

bool SIRVirtualTexturePagePool::commitUpload( const sirVirtualTexturePagePoolReservation_t& reservation,
                                              const sirGraphicsCompletionSet_t& completion ) noexcept
{
	if ( !checkReservation( reservation ) || !checkCompletion( completion ) || completion.nQueueMask == 0 ) {
		return false;
	}
	sirVirtualTexturePagePoolSlot_t& slot = m_pSlots[reservation.nSlot];
	slot.completion = completion;
	slot.eState = SIR_VT_POOL_UPLOADING;
	return true;
}

bool SIRVirtualTexturePagePool::abortUpload( const sirVirtualTexturePagePoolReservation_t& reservation,
                                             const sirGraphicsCompletionSet_t *pCompletion ) noexcept
{
	if ( !checkReservation( reservation ) ||
	     ( pCompletion != nullptr && ( !checkCompletion( *pCompletion ) || pCompletion->nQueueMask == 0 ) ) ) {
		return false;
	}
	if ( pCompletion == nullptr ) {
		releaseSlot( reservation.nSlot );
	} else {
		sirVirtualTexturePagePoolSlot_t& slot = m_pSlots[reservation.nSlot];
		slot.completion = *pCompletion;
		slot.eState = SIR_VT_POOL_RETIRING;
	}
	++m_nCancelled;
	return true;
}

bool SIRVirtualTexturePagePool::beginRetire( uint32_t nVirtualPage, const sirGraphicsCompletionSet_t& completion ) noexcept
{
	if ( m_pSlots == nullptr || nVirtualPage >= m_nVirtualPages || !checkCompletion( completion ) || completion.nQueueMask == 0 ) {
		return false;
	}
	const uint32_t nSlot = m_pVirtualToSlot[nVirtualPage];
	if ( nSlot >= m_nSlotCount || m_pSlots[nSlot].eState != SIR_VT_POOL_RESIDENT ) {
		return false;
	}
	m_pVirtualToSlot[nVirtualPage] = SIR_VT_POOL_INVALID_INDEX;
	sirVirtualTexturePagePoolSlot_t& slot = m_pSlots[nSlot];
	slot.completion = completion;
	slot.eState = SIR_VT_POOL_RETIRING;
	return true;
}

uint32_t SIRVirtualTexturePagePool::serviceCompletions( const sirGraphicsCompletionSet_t& completed ) noexcept
{
	if ( m_pSlots == nullptr || !checkCompletion( completed ) ) {
		return 0;
	}
	uint32_t nTransitions = 0;
	for ( uint32_t iSlot = 0; iSlot < m_nSlotCount; ++iSlot ) {
		sirVirtualTexturePagePoolSlot_t& slot = m_pSlots[iSlot];
		if ( ( slot.eState != SIR_VT_POOL_UPLOADING && slot.eState != SIR_VT_POOL_RETIRING ) ||
		     !isComplete( slot.completion, completed ) ) {
			continue;
		}
		if ( slot.eState == SIR_VT_POOL_RETIRING ) {
			releaseSlot( iSlot );
		} else {
			slot.eState = SIR_VT_POOL_RESIDENT;
			m_pVirtualToSlot[slot.nVirtualPage] = iSlot;
		}
		++nTransitions;
	}
	return nTransitions;
}

bool SIRVirtualTexturePagePool::findResident( uint32_t nVirtualPage, uint32_t nGeneration, uint32_t *pSlot ) const noexcept
{
	if ( m_pSlots == nullptr || pSlot == nullptr || nVirtualPage >= m_nVirtualPages || nGeneration == 0 ) {
		return false;
	}
	const uint32_t nSlot = m_pVirtualToSlot[nVirtualPage];
	if ( nSlot >= m_nSlotCount || m_pSlots[nSlot].eState != SIR_VT_POOL_RESIDENT || m_pSlots[nSlot].nGeneration != nGeneration ) {
		return false;
	}
	*pSlot = nSlot;
	return true;
}

bool SIRVirtualTexturePagePool::resolveResidentAncestor( const sirVirtualTexturePackView_t& view, uint32_t nVirtualPage,
                                                         uint32_t nGeneration, uint32_t *pResolvedPage, uint16_t *pResolvedMip,
                                                         uint32_t *pSlot ) const noexcept
{
	if ( m_pSlots == nullptr || view.header.nPageCount != m_nVirtualPages || nVirtualPage >= m_nVirtualPages || nGeneration == 0 ||
	     pResolvedPage == nullptr || pResolvedMip == nullptr || pSlot == nullptr ) {
		return false;
	}
	uint32_t nPage = nVirtualPage;
	// An intact V1 package is acyclic. The bound also protects callers passing
	// transient metadata views during file replacement.
	for ( uint32_t iDepth = 0; iDepth < m_nVirtualPages; ++iDepth ) {
		sirVirtualTexturePackPageRecord_t page = {};
		if ( !sirVTPackGetPage( view, nPage, &page ) || page.nVirtualPage != nPage || page.nGeneration != nGeneration ) {
			return false;
		}
		uint32_t nSlot = SIR_VT_POOL_INVALID_INDEX;
		if ( findResident( nPage, nGeneration, &nSlot ) ) {
			*pResolvedPage = nPage;
			*pResolvedMip = page.nMip;
			*pSlot = nSlot;
			return true;
		}
		if ( page.nParentPage == SIR_VT_POOL_INVALID_INDEX || page.nParentPage >= m_nVirtualPages ) {
			return false;
		}
		nPage = page.nParentPage;
	}
	return false;
}

sirVirtualTexturePagePoolStats_t SIRVirtualTexturePagePool::getStats( void ) const noexcept
{
	sirVirtualTexturePagePoolStats_t stats = {};
	stats.nRejected = m_nRejected;
	stats.nCancelled = m_nCancelled;
	for ( uint32_t iSlot = 0; iSlot < m_nSlotCount; ++iSlot ) {
		switch ( m_pSlots[iSlot].eState ) {
		case SIR_VT_POOL_FREE:
			++stats.nFree;
			break;
		case SIR_VT_POOL_RESERVED:
			++stats.nReserved;
			break;
		case SIR_VT_POOL_UPLOADING:
			++stats.nUploading;
			break;
		case SIR_VT_POOL_RESIDENT:
			++stats.nResident;
			break;
		case SIR_VT_POOL_RETIRING:
			++stats.nRetiring;
			break;
		}
	}
	return stats;
}
