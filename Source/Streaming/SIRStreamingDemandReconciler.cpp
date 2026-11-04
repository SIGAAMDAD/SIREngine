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

#include <Streaming/SIRStreamingDemandReconciler.h>

#include <limits.h>
#include <new>

typedef struct sirStreamingReconcileEntry_s
{
	sirStreamingUnitKey_t key;
	sirStreamingTicket_t ticket;
	bool bOccupied;
	bool bRetiring;
} sirStreamingReconcileEntry_t;

static bool streamingReconcileKeyEqual( const sirStreamingUnitKey_t &a, const sirStreamingUnitKey_t &b ) noexcept
{
	return a.nContainerId == b.nContainerId && a.nUnitId == b.nUnitId && a.nManifestGeneration == b.nManifestGeneration &&
	       a.nVariant == b.nVariant && a.eKind == b.eKind;
}

SIRStreamingDemandReconciler::SIRStreamingDemandReconciler( void ) noexcept
    : m_pCoordinator( nullptr ), m_pEntries( nullptr ), m_nConsumerId( 0 ), m_nConsumerGeneration( 0 ), m_nCapacity( 0 ), m_nLastEpoch( 0 ),
      m_bInitialized( false )
{
}

uint64_t SIRStreamingDemandReconciler::requiredBytes( uint32_t nCapacity ) noexcept
{
	return ( uint64_t )nCapacity * sizeof( sirStreamingReconcileEntry_t );
}

sirStreamingResult_t SIRStreamingDemandReconciler::initialize( SIRStreamingCoordinator *pCoordinator, void *pStorage,
                                                               uint64_t nStorageBytes, uint32_t nCapacity, uint64_t nConsumerId,
                                                               uint32_t nConsumerGeneration ) noexcept
{
	if ( m_bInitialized || pCoordinator == nullptr || pStorage == nullptr || nCapacity == 0 ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( nConsumerId == 0 || nConsumerGeneration == 0 ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( pCoordinator->getStats().nCapacity < nCapacity ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	const uint64_t nBytes = requiredBytes( nCapacity );
	if ( nBytes > SIZE_MAX || nStorageBytes < nBytes || ( ( uintptr_t )pStorage % alignof( sirStreamingReconcileEntry_t ) ) != 0 ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	m_pEntries = ( sirStreamingReconcileEntry_t * )pStorage;
	for ( uint32_t iEntry = 0; iEntry < nCapacity; ++iEntry ) {
		new ( m_pEntries + iEntry ) sirStreamingReconcileEntry_t{};
	}

	m_pCoordinator = pCoordinator;
	m_nConsumerId = nConsumerId;
	m_nConsumerGeneration = nConsumerGeneration;
	m_nCapacity = nCapacity;
	m_nLastEpoch = 0;
	m_bInitialized = true;
	return SIR_STREAMING_OK;
}

uint32_t SIRStreamingDemandReconciler::findEntry( const sirStreamingUnitKey_t &key ) const noexcept
{
	for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
		const sirStreamingReconcileEntry_t &entry = m_pEntries[iEntry];
		if ( entry.bOccupied && streamingReconcileKeyEqual( entry.key, key ) ) {
			return iEntry;
		}
	}
	return m_nCapacity;
}

void SIRStreamingDemandReconciler::clearEntry( uint32_t iEntry ) noexcept
{
	m_pEntries[iEntry] = {};
}

void SIRStreamingDemandReconciler::pruneReleased( void ) noexcept
{
	for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
		sirStreamingReconcileEntry_t &entry = m_pEntries[iEntry];
		if ( !entry.bOccupied ) {
			continue;
		}

		sirStreamingSnapshot_t snapshot = {};
		if ( m_pCoordinator->query( entry.ticket, &snapshot ) == SIR_STREAMING_STALE_TICKET ) {
			clearEntry( iEntry );
		}
	}
}

sirStreamingResult_t SIRStreamingDemandReconciler::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_STREAMING_NOT_READY;
	}

	pruneReleased();
	if ( getTrackedCount() != 0 ) {
		return SIR_STREAMING_BUSY;
	}

	m_pCoordinator = nullptr;
	m_pEntries = nullptr;
	m_nConsumerId = 0;
	m_nConsumerGeneration = 0;
	m_nCapacity = 0;
	m_nLastEpoch = 0;
	m_bInitialized = false;
	return SIR_STREAMING_OK;
}

sirStreamingResult_t SIRStreamingDemandReconciler::findTicket( const sirStreamingUnitKey_t &key,
                                                               sirStreamingTicket_t *pTicket ) const noexcept
{
	if ( !m_bInitialized ) {
		return SIR_STREAMING_NOT_READY;
	}
	if ( pTicket == nullptr ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	const uint32_t iEntry = findEntry( key );
	if ( iEntry == m_nCapacity ) {
		return SIR_STREAMING_NOT_READY;
	}

	const sirStreamingReconcileEntry_t &entry = m_pEntries[iEntry];
	sirStreamingSnapshot_t snapshot = {};
	if ( m_pCoordinator->query( entry.ticket, &snapshot ) != SIR_STREAMING_OK ) {
		return SIR_STREAMING_STALE_TICKET;
	}

	*pTicket = entry.ticket;
	return SIR_STREAMING_OK;
}

uint32_t SIRStreamingDemandReconciler::getTrackedCount( void ) const noexcept
{
	if ( !m_bInitialized ) {
		return 0;
	}

	uint32_t nTracked = 0;
	for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
		nTracked += m_pEntries[iEntry].bOccupied ? 1U : 0U;
	}
	return nTracked;
}

sirStreamingResult_t SIRStreamingDemandReconciler::validateSet( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount,
                                                                uint32_t nRequestEpoch ) const noexcept
{
	if ( nRequestEpoch == 0 || nRequestEpoch <= m_nLastEpoch ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( nDemandCount > m_nCapacity ) {
		return SIR_STREAMING_OUT_OF_CAPACITY;
	}
	if ( nDemandCount != 0 && pDemands == nullptr ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	for ( uint32_t iDemand = 0; iDemand < nDemandCount; ++iDemand ) {
		const sirStreamingDemand_t &demand = pDemands[iDemand];
		if ( demand.nConsumerId != m_nConsumerId || demand.nConsumerGeneration != m_nConsumerGeneration ) {
			return SIR_STREAMING_INVALID_ARGUMENT;
		}
		if ( demand.nRequestEpoch != nRequestEpoch || demand.keyUnit.nUnitId == 0 || demand.keyUnit.nManifestGeneration == 0 ) {
			return SIR_STREAMING_INVALID_ARGUMENT;
		}
		if ( demand.keyUnit.eKind >= SIR_STREAMING_UNIT_COUNT || demand.eTarget < SIR_STREAMING_TARGET_PREPARED ||
		     demand.eTarget > SIR_STREAMING_TARGET_ACTIVE ) {
			return SIR_STREAMING_INVALID_ARGUMENT;
		}
		for ( uint32_t iPrevious = 0; iPrevious < iDemand; ++iPrevious ) {
			if ( streamingReconcileKeyEqual( demand.keyUnit, pDemands[iPrevious].keyUnit ) ) {
				return SIR_STREAMING_DUPLICATE;
			}
		}
	}

	return SIR_STREAMING_OK;
}

void SIRStreamingDemandReconciler::retireMissing( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount,
                                                  sirStreamingReconcileStats_t &stats ) noexcept
{
	for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
		sirStreamingReconcileEntry_t &entry = m_pEntries[iEntry];
		if ( !entry.bOccupied || entry.bRetiring ) {
			continue;
		}
		bool bWanted = false;
		for ( uint32_t iDemand = 0; iDemand < nDemandCount; ++iDemand ) {
			if ( streamingReconcileKeyEqual( entry.key, pDemands[iDemand].keyUnit ) ) {
				bWanted = true;
				break;
			}
		}
		if ( bWanted ) {
			continue;
		}

		const sirStreamingResult_t eCancel = m_pCoordinator->cancel( entry.ticket );
		if ( eCancel == SIR_STREAMING_OK || eCancel == SIR_STREAMING_STALE_TICKET ) {
			entry.bRetiring = true;
			++stats.nCancelled;
		}
	}
}

void SIRStreamingDemandReconciler::refreshCurrent( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount,
                                                   sirStreamingReconcileStats_t &stats ) noexcept
{
	for ( uint32_t iDemand = 0; iDemand < nDemandCount; ++iDemand ) {
		const sirStreamingDemand_t &demand = pDemands[iDemand];
		const uint32_t iEntry = findEntry( demand.keyUnit );
		if ( iEntry == m_nCapacity ) {
			continue;
		}

		sirStreamingReconcileEntry_t &entry = m_pEntries[iEntry];
		if ( entry.bRetiring ) {
			++stats.nDeferred;
			continue;
		}
		const sirStreamingResult_t eUpdate = m_pCoordinator->updateDemand( entry.ticket, demand );
		if ( eUpdate == SIR_STREAMING_OK ) {
			++stats.nUpdated;
		} else if ( eUpdate == SIR_STREAMING_BUSY ) {
			m_pCoordinator->cancel( entry.ticket );
			entry.bRetiring = true;
			++stats.nCancelled;
			++stats.nDeferred;
		} else {
			++stats.nDeferred;
		}
	}
}

void SIRStreamingDemandReconciler::submitMissing( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount,
                                                  sirStreamingReconcileStats_t &stats ) noexcept
{
	for ( uint32_t iDemand = 0; iDemand < nDemandCount; ++iDemand ) {
		const sirStreamingDemand_t &demand = pDemands[iDemand];
		if ( findEntry( demand.keyUnit ) != m_nCapacity ) {
			continue;
		}

		uint32_t iFree = m_nCapacity;
		for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
			if ( !m_pEntries[iEntry].bOccupied ) {
				iFree = iEntry;
				break;
			}
		}
		if ( iFree == m_nCapacity ) {
			++stats.nDeferred;
			continue;
		}

		sirStreamingTicket_t ticket = {};
		if ( m_pCoordinator->submit( demand, &ticket ) != SIR_STREAMING_OK ) {
			++stats.nDeferred;
			continue;
		}

		sirStreamingReconcileEntry_t &entry = m_pEntries[iFree];
		entry.key = demand.keyUnit;
		entry.ticket = ticket;
		entry.bOccupied = true;
		entry.bRetiring = false;
		++stats.nSubmitted;
	}
}

sirStreamingResult_t SIRStreamingDemandReconciler::reconcile( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount,
                                                              uint32_t nRequestEpoch, sirStreamingReconcileStats_t *pStats ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_STREAMING_NOT_READY;
	}
	if ( pStats == nullptr ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	const sirStreamingResult_t eValidation = validateSet( pDemands, nDemandCount, nRequestEpoch );
	if ( eValidation != SIR_STREAMING_OK ) {
		return eValidation;
	}

	pruneReleased();
	sirStreamingReconcileStats_t stats = {};
	stats.nRequestEpoch = nRequestEpoch;

	retireMissing( pDemands, nDemandCount, stats );
	pruneReleased();
	refreshCurrent( pDemands, nDemandCount, stats );
	submitMissing( pDemands, nDemandCount, stats );

	stats.nTracked = getTrackedCount();
	for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
		stats.nRetirePending += m_pEntries[iEntry].bRetiring ? 1U : 0U;
	}
	m_nLastEpoch = nRequestEpoch;
	*pStats = stats;
	return SIR_STREAMING_OK;
}
