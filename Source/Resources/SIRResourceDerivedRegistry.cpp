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

#include <Resources/SIRResourceGraph.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct sirDerivedProviderSlot_s
{
	SIRDerivedProviderCallbacks callbacks;
	uint32_t nProviderId;
	uint64_t nCompletedFence;
	bool bRegistered;
} sirDerivedProviderSlot_t;

typedef struct sirDerivedRepresentationSlot_s
{
	SIRDerivedResourceRegistration registration;
	SIRResourcePinHandle pin;
	uint64_t nRetirementFence;
	uint32_t nGeneration;
	bool bOccupied;
	bool bRetiring;
	bool bNativeReleased;
} sirDerivedRepresentationSlot_t;

static uint32_t nextDerivedGeneration( uint32_t nGeneration ) noexcept
{
	return nGeneration == UINT32_MAX ? 1 : nGeneration + 1;
}

static bool sameDerivedHandle( const SIRResourceHandle &rA, const SIRResourceHandle &rB ) noexcept
{
	return rA.m_nSlot == rB.m_nSlot && rA.m_nGeneration == rB.m_nGeneration && rA.m_nTypeId == rB.m_nTypeId;
}

static bool sameDerivedKey( const SIRDerivedResourceKey &rA, const SIRDerivedResourceKey &rB ) noexcept
{
	return sameDerivedHandle( rA.m_Resource, rB.m_Resource ) && rA.m_nRevision == rB.m_nRevision &&
	       rA.m_nProviderId == rB.m_nProviderId && rA.m_nRepresentationId == rB.m_nRepresentationId &&
	       rA.m_nVariantId == rB.m_nVariantId;
}

SIRResourceDerivedRegistry::SIRResourceDerivedRegistry( SIRResourceGraph &rGraph, const SIRResourceDerivedConfig &rConfig ) noexcept
    : m_pGraph( &rGraph ), m_Config( rConfig ), m_OwnerThread( std::this_thread::get_id() ), m_pProviders( nullptr ),
      m_pRepresentations( nullptr ), m_Stats{}, m_nTrackedBytes( 0 ), m_bReady( false )
{
	if ( !rGraph.isReady() || !rConfig.allocator.m_pfnAllocate || !rConfig.allocator.m_pfnFree ||
	     !rConfig.nMaxProviders || !rConfig.nMaxRepresentations || !rConfig.nMaxTrackedBytes ) {
		return;
	}
	const uint64_t nProviderBytes = ( uint64_t )rConfig.nMaxProviders * sizeof( sirDerivedProviderSlot_t );
	const uint64_t nRecordBytes = ( uint64_t )rConfig.nMaxRepresentations * sizeof( sirDerivedRepresentationSlot_t );
	if ( nProviderBytes > SIZE_MAX || nRecordBytes > SIZE_MAX ) {
		return;
	}
	m_pProviders = ( sirDerivedProviderSlot_t * )rConfig.allocator.m_pfnAllocate(
	    rConfig.allocator.m_pContext, nProviderBytes, alignof( sirDerivedProviderSlot_t ), rConfig.nMetadataDomain );
	m_pRepresentations = ( sirDerivedRepresentationSlot_t * )rConfig.allocator.m_pfnAllocate(
	    rConfig.allocator.m_pContext, nRecordBytes, alignof( sirDerivedRepresentationSlot_t ), rConfig.nMetadataDomain );
	if ( m_pProviders == nullptr || m_pRepresentations == nullptr ) {
		return;
	}
	memset( m_pProviders, 0, ( size_t )nProviderBytes );
	memset( m_pRepresentations, 0, ( size_t )nRecordBytes );
	for ( uint32_t iSlot = 0; iSlot < rConfig.nMaxRepresentations; ++iSlot ) {
		m_pRepresentations[iSlot].nGeneration = 1;
	}
	m_bReady = true;
}

SIRResourceDerivedRegistry::~SIRResourceDerivedRegistry( void ) noexcept
{
	if ( m_pRepresentations != nullptr && m_bReady ) {
		for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
			// A live native object must be retired by its provider before shutdown.
			if ( m_pRepresentations[iSlot].bOccupied ) {
				abort();
			}
		}
	}
	releaseMetadata();
}

void SIRResourceDerivedRegistry::releaseMetadata( void ) noexcept
{
	if ( m_pRepresentations != nullptr ) {
		m_Config.allocator.m_pfnFree( m_Config.allocator.m_pContext, m_pRepresentations,
		                            ( uint64_t )m_Config.nMaxRepresentations * sizeof( sirDerivedRepresentationSlot_t ),
		                            alignof( sirDerivedRepresentationSlot_t ), m_Config.nMetadataDomain );
		m_pRepresentations = nullptr;
	}
	if ( m_pProviders != nullptr ) {
		m_Config.allocator.m_pfnFree( m_Config.allocator.m_pContext, m_pProviders,
		                            ( uint64_t )m_Config.nMaxProviders * sizeof( sirDerivedProviderSlot_t ),
		                            alignof( sirDerivedProviderSlot_t ), m_Config.nMetadataDomain );
		m_pProviders = nullptr;
	}
	m_bReady = false;
}

bool SIRResourceDerivedRegistry::isReady( void ) const noexcept
{
	return m_bReady;
}

bool SIRResourceDerivedRegistry::isWriterThread( void ) const noexcept
{
	return m_OwnerThread == std::this_thread::get_id();
}

int32_t SIRResourceDerivedRegistry::findProvider( uint32_t nProviderId ) const noexcept
{
	for ( uint32_t iProvider = 0; iProvider < m_Config.nMaxProviders; ++iProvider ) {
		if ( m_pProviders[iProvider].bRegistered && m_pProviders[iProvider].nProviderId == nProviderId ) {
			return ( int32_t )iProvider;
		}
	}
	return -1;
}

SIRResourceResult SIRResourceDerivedRegistry::registerProvider( uint32_t nProviderId,
                                                                const SIRDerivedProviderCallbacks &rCallbacks ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !nProviderId || !rCallbacks.pfnReleaseNative ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( findProvider( nProviderId ) >= 0 ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	for ( uint32_t iProvider = 0; iProvider < m_Config.nMaxProviders; ++iProvider ) {
		sirDerivedProviderSlot_t &rProvider = m_pProviders[iProvider];
		if ( !rProvider.bRegistered ) {
			rProvider.callbacks = rCallbacks;
			rProvider.nProviderId = nProviderId;
			rProvider.nCompletedFence = 0;
			rProvider.bRegistered = true;
			++m_Stats.nRegisteredProviders;
			return SIR_RESOURCE_RESULT_SUCCESS;
		}
	}
	return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
}

SIRResourceResult SIRResourceDerivedRegistry::registerRepresentation( const SIRResourceReadScope &rScope,
                                                                      const SIRResourceView &rSource,
                                                                      const SIRDerivedResourceRegistration &rRegistration,
                                                                      SIRDerivedResourceHandle *pHandle ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !pHandle || !rRegistration.m_nNativeToken || !rRegistration.m_Key.m_nRevision ||
	     !rRegistration.m_Key.m_nRepresentationId || rRegistration.m_Key.m_nReserved ||
	     rRegistration.m_Key.m_Resource.m_nReserved ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const SIRDerivedResourceAccounting &rAccounting = rRegistration.m_Accounting;
	if ( rAccounting.m_eSpace > SIR_MEMORY_SPACE_EXTERNAL || rAccounting.m_nCommittedBytes == 0 ||
	     rAccounting.m_nResidentBytes > rAccounting.m_nCommittedBytes ||
	     rAccounting.m_nSourceRevision != rRegistration.m_Key.m_nRevision ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const int32_t iProvider = findProvider( rRegistration.m_Key.m_nProviderId );
	if ( iProvider < 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	SIRResourceInfo sourceInfo = {};
	if ( rSource.getInfo( &sourceInfo ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     !sameDerivedHandle( rSource.getHandle(), rRegistration.m_Key.m_Resource ) ||
	     rSource.getRevision() != rRegistration.m_Key.m_nRevision ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	const uint32_t iSpace = ( uint32_t )rAccounting.m_eSpace;
	if ( rAccounting.m_nCommittedBytes > m_Config.nMaxTrackedBytes - m_nTrackedBytes ||
	     rAccounting.m_nCommittedBytes > UINT64_MAX - m_Stats.nCommittedBytes[iSpace] ||
	     rAccounting.m_nResidentBytes > UINT64_MAX - m_Stats.nResidentBytes[iSpace] ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	uint32_t iFree = UINT32_MAX;
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		const sirDerivedRepresentationSlot_t &rSlot = m_pRepresentations[iSlot];
		if ( !rSlot.bOccupied ) {
			if ( iFree == UINT32_MAX ) {
				iFree = iSlot;
			}
			continue;
		}
		if ( sameDerivedKey( rSlot.registration.m_Key, rRegistration.m_Key ) ||
		     ( rSlot.registration.m_Key.m_nProviderId == rRegistration.m_Key.m_nProviderId &&
		       rSlot.registration.m_nNativeToken == rRegistration.m_nNativeToken ) ) {
			// A given provider's native allocation must only be counted once.
			return SIR_RESOURCE_RESULT_BUSY;
		}
	}
	if ( iFree == UINT32_MAX ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	SIRResourcePinHandle pin = {};
	const SIRResourceResult ePin = m_pGraph->pinForProvider( rScope, rSource, rRegistration.m_Key.m_nProviderId, &pin );
	if ( ePin != SIR_RESOURCE_RESULT_SUCCESS ) {
		return ePin;
	}
	sirDerivedRepresentationSlot_t &rSlot = m_pRepresentations[iFree];
	rSlot.registration = rRegistration;
	rSlot.pin = pin;
	rSlot.nRetirementFence = 0;
	rSlot.bOccupied = true;
	rSlot.bRetiring = false;
	rSlot.bNativeReleased = false;
	m_nTrackedBytes += rAccounting.m_nCommittedBytes;
	m_Stats.nCommittedBytes[iSpace] += rAccounting.m_nCommittedBytes;
	m_Stats.nResidentBytes[iSpace] += rAccounting.m_nResidentBytes;
	if ( m_Stats.nCommittedBytes[iSpace] > m_Stats.nPeakCommittedBytes[iSpace] ) {
		m_Stats.nPeakCommittedBytes[iSpace] = m_Stats.nCommittedBytes[iSpace];
	}
	++m_Stats.nLiveRepresentations;
	*pHandle = { iFree, rSlot.nGeneration };
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::collectProvider( uint32_t iProvider ) noexcept
{
	sirDerivedProviderSlot_t &rProvider = m_pProviders[iProvider];
	bool bBusy = false;
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		sirDerivedRepresentationSlot_t &rSlot = m_pRepresentations[iSlot];
		if ( !rSlot.bOccupied || !rSlot.bRetiring || rSlot.registration.m_Key.m_nProviderId != rProvider.nProviderId ||
		     rSlot.nRetirementFence > rProvider.nCompletedFence ) {
			continue;
		}
		if ( !rSlot.bNativeReleased ) {
			if ( !rProvider.callbacks.pfnReleaseNative( rProvider.callbacks.pContext, rSlot.registration.m_nNativeToken ) ) {
				bBusy = true;
				continue;
			}
			rSlot.bNativeReleased = true;
		}
		if ( m_pGraph->releaseProviderPin( rSlot.pin ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_RESOURCE_RESULT_FAILED;
		}
		const uint32_t iSpace = ( uint32_t )rSlot.registration.m_Accounting.m_eSpace;
		const uint64_t nCommitted = rSlot.registration.m_Accounting.m_nCommittedBytes;
		const uint64_t nResident = rSlot.registration.m_Accounting.m_nResidentBytes;
		m_Stats.nCommittedBytes[iSpace] -= nCommitted;
		m_Stats.nResidentBytes[iSpace] -= nResident;
		m_nTrackedBytes -= nCommitted;
		--m_Stats.nLiveRepresentations;
		--m_Stats.nPendingRetirements;
		rSlot.bOccupied = false;
		rSlot.bRetiring = false;
		rSlot.bNativeReleased = false;
		rSlot.nGeneration = nextDerivedGeneration( rSlot.nGeneration );
	}
	return bBusy ? SIR_RESOURCE_RESULT_BUSY : SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::retireRepresentation( SIRDerivedResourceHandle hRepresentation,
                                                                    uint64_t nCompletionFence ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || hRepresentation.m_nSlot >= m_Config.nMaxRepresentations ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	sirDerivedRepresentationSlot_t &rSlot = m_pRepresentations[hRepresentation.m_nSlot];
	if ( !rSlot.bOccupied || rSlot.nGeneration != hRepresentation.m_nGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( rSlot.bRetiring ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	rSlot.bRetiring = true;
	rSlot.nRetirementFence = nCompletionFence;
	++m_Stats.nPendingRetirements;
	const int32_t iProvider = findProvider( rSlot.registration.m_Key.m_nProviderId );
	if ( iProvider >= 0 && nCompletionFence <= m_pProviders[iProvider].nCompletedFence ) {
		// Retirement acceptance is durable even if native release needs retry.
		( void )collectProvider( ( uint32_t )iProvider );
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::updateCompletedFence( uint32_t nProviderId, uint64_t nCompletedFence ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	const int32_t iProvider = findProvider( nProviderId );
	if ( iProvider < 0 || nCompletedFence < m_pProviders[iProvider].nCompletedFence ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	m_pProviders[iProvider].nCompletedFence = nCompletedFence;
	return collectProvider( ( uint32_t )iProvider );
}

SIRResourceResult SIRResourceDerivedRegistry::getAccounting( SIRDerivedResourceHandle hRepresentation,
                                                              SIRDerivedResourceAccounting *pAccounting ) const noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !pAccounting || hRepresentation.m_nSlot >= m_Config.nMaxRepresentations ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const sirDerivedRepresentationSlot_t &rSlot = m_pRepresentations[hRepresentation.m_nSlot];
	if ( !rSlot.bOccupied || rSlot.nGeneration != hRepresentation.m_nGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	*pAccounting = rSlot.registration.m_Accounting;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::getStats( SIRResourceDerivedStats *pStats ) const noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !pStats ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	*pStats = m_Stats;
	return SIR_RESOURCE_RESULT_SUCCESS;
}
