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
	uint32_t nDeviceGeneration; // Device generation associated with this provider registration.
	sirProviderCompletionSet_t completed;
	bool bRegistered;
} sirDerivedProviderSlot_t;

// Unified slot for either a representation or a shared backing.
// This allows both to share the same handle space (SIRDerivedResourceHandle).
typedef enum sirUnifiedSlotKind_e : uint8_t
{
	SIR_UNIFIED_SLOT_EMPTY = 0,
	SIR_UNIFIED_SLOT_REPRESENTATION = 1,
	SIR_UNIFIED_SLOT_SHARED_BACKING = 2
} sirUnifiedSlotKind_t;

typedef struct sirUnifiedSlot_s
{
	sirUnifiedSlotKind_t eKind;
	uint32_t nGeneration;
	union
	{
		struct
		{
			SIRDerivedResourceRegistration registration;
			SIRResourcePinHandle pin;
			sirProviderCompletionSet_t retirement;
			bool bOccupied;
			bool bRetiring;
			bool bNativeReleased;
		} representation;
		struct
		{
			SIRResourcePinHandle pin;
			SIRResourceHandle hSource;
			uint64_t nSourceRevision;
			sirSharedDeviceBackingDesc_t desc;
			uint64_t nNativeToken;
			uint64_t nPhysicalBackingId;
			bool bOccupied;
			bool bPublished;
			bool bPendingRetire;
		} sharedBacking;
	} u;
} sirUnifiedSlot_t;

// A borrowed subrange view of a shared backing. Valid only while the
// corresponding provider completion fence has not been satisfied.
typedef struct sirSharedDeviceBorrowSlot_s
{
	uint32_t iBackingSlot;
	uint32_t nGeneration;
	uint64_t nOffset;
	uint64_t nBytes;
	uint64_t nSourceRevision;
	uint32_t nApprovedUsageMask;
	uint32_t nCompletionProviderId;
	uint32_t nCompletionLaneMask;
	uint32_t nDeviceGeneration;
	uint64_t nNativeResourceToken;
	bool bReadOnly;
	bool bActive;
} sirSharedDeviceBorrowSlot_t;

static uint32_t nextDerivedGeneration( uint32_t nGeneration ) noexcept
{
	return nGeneration == UINT32_MAX ? 1 : nGeneration + 1;
}

static bool sameDerivedHandle( const SIRResourceHandle& rA, const SIRResourceHandle& rB ) noexcept
{
	return rA.m_nSlot == rB.m_nSlot && rA.m_nGeneration == rB.m_nGeneration && rA.m_nTypeId == rB.m_nTypeId;
}

static bool sameDerivedKey( const SIRDerivedResourceKey& rA, const SIRDerivedResourceKey& rB ) noexcept
{
	return sameDerivedHandle( rA.m_Resource, rB.m_Resource ) && rA.m_nRevision == rB.m_nRevision && rA.m_nProviderId == rB.m_nProviderId &&
	       rA.m_nRepresentationId == rB.m_nRepresentationId && rA.m_nVariantId == rB.m_nVariantId;
}

SIRResourceDerivedRegistry::SIRResourceDerivedRegistry( SIRResourceGraph& rGraph, const SIRResourceDerivedConfig& rConfig ) noexcept
    : m_pGraph( &rGraph ), m_Config( rConfig ), m_OwnerThread( std::this_thread::get_id() ), m_pProviders( nullptr ),
      m_pUnifiedSlots( nullptr ), m_pSharedBorrows( nullptr ), m_Stats{}, m_nTrackedBytes( 0 ), m_bReady( false )
{
	if ( !rGraph.isReady() || !rConfig.allocator.m_pfnAllocate || !rConfig.allocator.m_pfnFree || !rConfig.nMaxProviders ||
	     !rConfig.nMaxRepresentations || !rConfig.nMaxTrackedBytes || !rConfig.nMaxSharedBorrows ) {
		return;
	}
	const uint64_t nProviderBytes = ( uint64_t )rConfig.nMaxProviders * sizeof( sirDerivedProviderSlot_t );
	const uint64_t nUnifiedBytes = ( uint64_t )rConfig.nMaxRepresentations * sizeof( sirUnifiedSlot_t );
	const uint64_t nBorrowBytes = ( uint64_t )rConfig.nMaxSharedBorrows * sizeof( sirSharedDeviceBorrowSlot_t );
	if ( nProviderBytes > SIZE_MAX || nUnifiedBytes > SIZE_MAX || nBorrowBytes > SIZE_MAX ) {
		return;
	}
	m_pProviders = ( sirDerivedProviderSlot_t * )rConfig.allocator.m_pfnAllocate(
	    rConfig.allocator.m_pContext, nProviderBytes, alignof( sirDerivedProviderSlot_t ), rConfig.nMetadataDomain );
	m_pUnifiedSlots = ( sirUnifiedSlot_t * )rConfig.allocator.m_pfnAllocate(
	    rConfig.allocator.m_pContext, nUnifiedBytes, alignof( sirUnifiedSlot_t ), rConfig.nMetadataDomain );
	m_pSharedBorrows = ( sirSharedDeviceBorrowSlot_t * )rConfig.allocator.m_pfnAllocate(
	    rConfig.allocator.m_pContext, nBorrowBytes, alignof( sirSharedDeviceBorrowSlot_t ), rConfig.nMetadataDomain );
	if ( m_pProviders == nullptr || m_pUnifiedSlots == nullptr || m_pSharedBorrows == nullptr ) {
		return;
	}
	memset( m_pProviders, 0, ( size_t )nProviderBytes );
	memset( m_pUnifiedSlots, 0, ( size_t )nUnifiedBytes );
	memset( m_pSharedBorrows, 0, ( size_t )nBorrowBytes );
	for ( uint32_t iSlot = 0; iSlot < rConfig.nMaxRepresentations; ++iSlot ) {
		m_pUnifiedSlots[iSlot].nGeneration = 1;
	}
	for ( uint32_t iSlot = 0; iSlot < rConfig.nMaxSharedBorrows; ++iSlot ) {
		m_pSharedBorrows[iSlot].nGeneration = 1;
	}
	m_Stats.nMaxSharedBorrows = rConfig.nMaxSharedBorrows;
	m_bReady = true;
}

SIRResourceDerivedRegistry::~SIRResourceDerivedRegistry( void ) noexcept
{
	if ( m_pUnifiedSlots != nullptr && m_bReady ) {
		for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
			if ( m_pUnifiedSlots[iSlot].eKind != SIR_UNIFIED_SLOT_EMPTY ) {
				abort();
			}
		}
	}
	if ( m_pSharedBorrows != nullptr && m_bReady ) {
		for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxSharedBorrows; ++iSlot ) {
			if ( m_pSharedBorrows[iSlot].bActive ) {
				abort();
			}
		}
	}
	releaseMetadata();
}

void SIRResourceDerivedRegistry::releaseMetadata( void ) noexcept
{
	if ( m_pSharedBorrows != nullptr ) {
		m_Config.allocator.m_pfnFree( m_Config.allocator.m_pContext, m_pSharedBorrows,
		                              ( uint64_t )m_Config.nMaxSharedBorrows * sizeof( sirSharedDeviceBorrowSlot_t ),
		                              alignof( sirSharedDeviceBorrowSlot_t ), m_Config.nMetadataDomain );
		m_pSharedBorrows = nullptr;
	}
	if ( m_pUnifiedSlots != nullptr ) {
		m_Config.allocator.m_pfnFree( m_Config.allocator.m_pContext, m_pUnifiedSlots,
		                              ( uint64_t )m_Config.nMaxRepresentations * sizeof( sirUnifiedSlot_t ),
		                              alignof( sirUnifiedSlot_t ), m_Config.nMetadataDomain );
		m_pUnifiedSlots = nullptr;
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
                                                                const SIRDerivedProviderCallbacks& rCallbacks ) noexcept
{
	return registerProvider( nProviderId, 1, 1, rCallbacks );
}

SIRResourceResult SIRResourceDerivedRegistry::registerProvider( uint32_t nProviderId, uint32_t nProviderGeneration, uint32_t nLaneCount,
                                                                const SIRDerivedProviderCallbacks& rCallbacks ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !nProviderId || !nProviderGeneration || nLaneCount == 0 || nLaneCount > SIR_PROVIDER_MAX_LANES ||
	     !rCallbacks.pfnReleaseNative ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( findProvider( nProviderId ) >= 0 ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	for ( uint32_t iProvider = 0; iProvider < m_Config.nMaxProviders; ++iProvider ) {
		sirDerivedProviderSlot_t& rProvider = m_pProviders[iProvider];
		if ( !rProvider.bRegistered ) {
			rProvider.callbacks = rCallbacks;
			rProvider.nProviderId = nProviderId;
			rProvider.nDeviceGeneration = nProviderGeneration;
			rProvider.completed = { nProviderId, nProviderGeneration, ( 1u << nLaneCount ) - 1u, nLaneCount, {} };
			rProvider.bRegistered = true;
			++m_Stats.nRegisteredProviders;
			return SIR_RESOURCE_RESULT_SUCCESS;
		}
	}
	return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
}

SIRResourceResult SIRResourceDerivedRegistry::registerRepresentation( const SIRResourceReadScope& rScope, const SIRResourceView& rSource,
                                                                      const SIRDerivedResourceRegistration& rRegistration,
                                                                      SIRDerivedResourceHandle *pHandle ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !pHandle || !rRegistration.m_nNativeToken || !rRegistration.m_Key.m_nRevision ||
	     !rRegistration.m_Key.m_nRepresentationId || rRegistration.m_Key.m_nReserved || rRegistration.m_Key.m_Resource.m_nReserved ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const SIRDerivedResourceAccounting& rAccounting = rRegistration.m_Accounting;
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
	// Check for duplicate key or native token.
	int32_t iFree = -1;
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		const sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[iSlot];
		if ( rSlot.eKind == SIR_UNIFIED_SLOT_EMPTY ) {
			if ( iFree == -1 ) {
				iFree = ( int32_t )iSlot;
			}
			continue;
		}
		if ( rSlot.eKind == SIR_UNIFIED_SLOT_REPRESENTATION ) {
			if ( sameDerivedKey( rSlot.u.representation.registration.m_Key, rRegistration.m_Key ) ||
			     ( rSlot.u.representation.registration.m_Key.m_nProviderId == rRegistration.m_Key.m_nProviderId &&
			       rSlot.u.representation.registration.m_nNativeToken == rRegistration.m_nNativeToken ) ) {
				return SIR_RESOURCE_RESULT_BUSY;
			}
		} else if ( rSlot.eKind == SIR_UNIFIED_SLOT_SHARED_BACKING ) {
			if ( rSlot.u.sharedBacking.desc.nProviderId == rRegistration.m_Key.m_nProviderId &&
			     rSlot.u.sharedBacking.nNativeToken == rRegistration.m_nNativeToken ) {
				return SIR_RESOURCE_RESULT_BUSY;
			}
		}
	}
	if ( iFree == -1 ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	SIRResourcePinHandle pin = {};
	const SIRResourceResult ePin = m_pGraph->pinForProvider( rScope, rSource, rRegistration.m_Key.m_nProviderId, &pin );
	if ( ePin != SIR_RESOURCE_RESULT_SUCCESS ) {
		return ePin;
	}
	sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[iFree];
	rSlot.eKind = SIR_UNIFIED_SLOT_REPRESENTATION;
	rSlot.u.representation.registration = rRegistration;
	rSlot.u.representation.pin = pin;
	rSlot.u.representation.retirement = {};
	rSlot.u.representation.bOccupied = true;
	rSlot.u.representation.bRetiring = false;
	rSlot.u.representation.bNativeReleased = false;
	m_nTrackedBytes += rAccounting.m_nCommittedBytes;
	m_Stats.nCommittedBytes[iSpace] += rAccounting.m_nCommittedBytes;
	m_Stats.nResidentBytes[iSpace] += rAccounting.m_nResidentBytes;
	if ( m_Stats.nCommittedBytes[iSpace] > m_Stats.nPeakCommittedBytes[iSpace] ) {
		m_Stats.nPeakCommittedBytes[iSpace] = m_Stats.nCommittedBytes[iSpace];
	}
	++m_Stats.nLiveRepresentations;
	*pHandle = { ( uint32_t )iFree, rSlot.nGeneration };
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::collectProvider( uint32_t iProvider ) noexcept
{
	sirDerivedProviderSlot_t& rProvider = m_pProviders[iProvider];
	bool bBusy = false;
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[iSlot];
		if ( rSlot.eKind != SIR_UNIFIED_SLOT_REPRESENTATION ) continue;
		if ( !rSlot.u.representation.bOccupied || !rSlot.u.representation.bRetiring ||
		     rSlot.u.representation.registration.m_Key.m_nProviderId != rProvider.nProviderId ||
		     !sirProviderCompletionSatisfied( rSlot.u.representation.retirement, rProvider.completed ) ) {
			continue;
		}
		if ( !rSlot.u.representation.bNativeReleased ) {
			if ( !rProvider.callbacks.pfnReleaseNative( rProvider.callbacks.pContext, rSlot.u.representation.registration.m_nNativeToken ) ) {
				bBusy = true;
				continue;
			}
			rSlot.u.representation.bNativeReleased = true;
		}
		if ( m_pGraph->releaseProviderPin( rSlot.u.representation.pin ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_RESOURCE_RESULT_FAILED;
		}
		const uint32_t iSpace = ( uint32_t )rSlot.u.representation.registration.m_Accounting.m_eSpace;
		const uint64_t nCommitted = rSlot.u.representation.registration.m_Accounting.m_nCommittedBytes;
		const uint64_t nResident = rSlot.u.representation.registration.m_Accounting.m_nResidentBytes;
		m_Stats.nCommittedBytes[iSpace] -= nCommitted;
		m_Stats.nResidentBytes[iSpace] -= nResident;
		m_nTrackedBytes -= nCommitted;
		--m_Stats.nLiveRepresentations;
		--m_Stats.nPendingRetirements;
		rSlot.u.representation.bOccupied = false;
		rSlot.u.representation.bRetiring = false;
		rSlot.u.representation.bNativeReleased = false;
		rSlot.eKind = SIR_UNIFIED_SLOT_EMPTY;
		rSlot.nGeneration = nextDerivedGeneration( rSlot.nGeneration );
	}
	return bBusy ? SIR_RESOURCE_RESULT_BUSY : SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::retireRepresentation( SIRDerivedResourceHandle hRepresentation,
                                                                    const sirProviderCompletionSet_t& required ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || hRepresentation.m_nSlot >= m_Config.nMaxRepresentations ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[hRepresentation.m_nSlot];
	if ( rSlot.eKind == SIR_UNIFIED_SLOT_EMPTY || rSlot.nGeneration != hRepresentation.m_nGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( rSlot.eKind == SIR_UNIFIED_SLOT_REPRESENTATION ) {
		if ( rSlot.u.representation.bRetiring ) {
			return SIR_RESOURCE_RESULT_BUSY;
		}
		const int32_t iProvider = findProvider( rSlot.u.representation.registration.m_Key.m_nProviderId );
		if ( iProvider < 0 || !sirProviderCompletionValid( required ) ) {
			return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		const sirProviderCompletionSet_t& completed = m_pProviders[iProvider].completed;
		if ( required.nProviderId != completed.nProviderId || required.nProviderGeneration != completed.nProviderGeneration ||
		     required.nLaneCount != completed.nLaneCount ) {
			return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		rSlot.u.representation.bRetiring = true;
		rSlot.u.representation.retirement = required;
		++m_Stats.nPendingRetirements;
		if ( sirProviderCompletionSatisfied( required, completed ) ) {
			( void )collectProvider( ( uint32_t )iProvider );
		}
		return SIR_RESOURCE_RESULT_SUCCESS;
	} else if ( rSlot.eKind == SIR_UNIFIED_SLOT_SHARED_BACKING ) {
		// Shared backing retirement is handled by retireSharedBacking.
		if ( !rSlot.u.sharedBacking.bPublished ) {
			return SIR_RESOURCE_RESULT_BUSY; // Not published yet.
		}
		if ( rSlot.u.sharedBacking.bPendingRetire ) {
			return SIR_RESOURCE_RESULT_BUSY; // Already pending retirement.
		}
		rSlot.u.sharedBacking.bPendingRetire = true;
		return retireSharedBacking( hRepresentation.m_nSlot, required );
	}
	return SIR_RESOURCE_RESULT_INVALID_HANDLE;
}

SIRResourceResult SIRResourceDerivedRegistry::updateProviderCompletion( const sirProviderCompletionSet_t& observed ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	if ( !sirProviderCompletionValid( observed ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const int32_t iProvider = findProvider( observed.nProviderId );
	if ( iProvider < 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	sirProviderCompletionSet_t& completed = m_pProviders[iProvider].completed;
	if ( completed.nProviderGeneration != observed.nProviderGeneration || completed.nLaneCount != observed.nLaneCount ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	for ( uint32_t iLane = 0; iLane < completed.nLaneCount; ++iLane ) {
		if ( ( observed.nLaneMask & ( 1u << iLane ) ) != 0 && observed.szCompletedValues[iLane] < completed.szCompletedValues[iLane] ) {
			return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t iLane = 0; iLane < completed.nLaneCount; ++iLane ) {
		if ( ( observed.nLaneMask & ( 1u << iLane ) ) != 0 ) {
			completed.szCompletedValues[iLane] = observed.szCompletedValues[iLane];
		}
	}
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
	const sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[hRepresentation.m_nSlot];
	if ( rSlot.eKind == SIR_UNIFIED_SLOT_EMPTY || rSlot.nGeneration != hRepresentation.m_nGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( rSlot.eKind == SIR_UNIFIED_SLOT_REPRESENTATION ) {
		*pAccounting = rSlot.u.representation.registration.m_Accounting;
		return SIR_RESOURCE_RESULT_SUCCESS;
	}
	return SIR_RESOURCE_RESULT_TYPE_MISMATCH;
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

// Helper: find an existing shared backing matching the source and descriptor.
int32_t SIRResourceDerivedRegistry::findSharedBacking( const SIRResourceView& rSource, const sirSharedDeviceBackingDesc_t& rDesc ) const noexcept
{
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		const sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[iSlot];
		if ( rSlot.eKind != SIR_UNIFIED_SLOT_SHARED_BACKING ) continue;
		if ( !rSlot.u.sharedBacking.bOccupied ) continue;
		if ( !sameDerivedHandle( rSlot.u.sharedBacking.hSource, rSource.getHandle() ) ) continue;
		if ( rSlot.u.sharedBacking.nSourceRevision != rSource.getRevision() ) continue;
		if ( rSlot.u.sharedBacking.desc.nPhysicalDeviceId != rDesc.nPhysicalDeviceId ) continue;
		if ( rSlot.u.sharedBacking.desc.nDeviceGeneration != rDesc.nDeviceGeneration ) continue;
		if ( rSlot.u.sharedBacking.desc.nStorageLayoutId != rDesc.nStorageLayoutId ) continue;
		if ( rSlot.u.sharedBacking.desc.nSupportedUsageMask != rDesc.nSupportedUsageMask ) continue;
		if ( rSlot.u.sharedBacking.desc.nFormatId != rDesc.nFormatId ) continue;
		if ( rSlot.u.sharedBacking.desc.nAlignment != rDesc.nAlignment ) continue;
		return ( int32_t )iSlot;
	}
	return -1;
}

// Helper: find a free unified slot for the given kind (representation or shared backing).
int32_t SIRResourceDerivedRegistry::findFreeUnifiedSlot( sirUnifiedSlotKind_t eKind ) const noexcept
{
	( void )eKind; // Currently we just find any empty slot
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		if ( m_pUnifiedSlots[iSlot].eKind == SIR_UNIFIED_SLOT_EMPTY ) {
			return ( int32_t )iSlot;
		}
	}
	return -1;
}

// Helper: find a free shared borrow slot.
int32_t SIRResourceDerivedRegistry::findFreeSharedBorrow( void ) const noexcept
{
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxSharedBorrows; ++iSlot ) {
		if ( !m_pSharedBorrows[iSlot].bActive ) {
			return ( int32_t )iSlot;
		}
	}
	return -1;
}

// Helper: retire a shared backing after all borrows are released and completion satisfied.
SIRResourceResult SIRResourceDerivedRegistry::retireSharedBacking( uint32_t iBacking, const sirProviderCompletionSet_t& required ) noexcept
{
	if ( iBacking >= m_Config.nMaxRepresentations ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[iBacking];
	if ( rSlot.eKind != SIR_UNIFIED_SLOT_SHARED_BACKING || !rSlot.u.sharedBacking.bOccupied || !rSlot.u.sharedBacking.bPublished || !rSlot.u.sharedBacking.bPendingRetire ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	const int32_t iProvider = findProvider( rSlot.u.sharedBacking.desc.nProviderId );
	if ( iProvider < 0 || !sirProviderCompletionValid( required ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const sirProviderCompletionSet_t& completed = m_pProviders[iProvider].completed;
	if ( required.nProviderId != completed.nProviderId || required.nProviderGeneration != completed.nProviderGeneration ||
	     required.nLaneCount != completed.nLaneCount ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( !sirProviderCompletionSatisfied( required, completed ) ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	// Release native token via provider.
	if ( !m_pProviders[iProvider].callbacks.pfnReleaseNative( m_pProviders[iProvider].callbacks.pContext, rSlot.u.sharedBacking.nNativeToken ) ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	// Release the GRDG pin.
	if ( m_pGraph->releaseProviderPin( rSlot.u.sharedBacking.pin ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	// Update stats.
	const uint32_t iSpace = ( uint32_t )SIR_MEMORY_SPACE_DEVICE;
	const uint64_t nCommitted = rSlot.u.sharedBacking.desc.nCommittedBytes;
	m_Stats.nCommittedBytes[iSpace] -= nCommitted;
	m_Stats.nResidentBytes[iSpace] -= nCommitted;
	m_nTrackedBytes -= nCommitted;
	// Clear slot.
	rSlot.eKind = SIR_UNIFIED_SLOT_EMPTY;
	rSlot.nGeneration = nextDerivedGeneration( rSlot.nGeneration );
	return SIR_RESOURCE_RESULT_SUCCESS;
}

// Public: reserve a shared device backing for a canonical source.
SIRResourceResult SIRResourceDerivedRegistry::reserveSharedDeviceBacking( const SIRResourceReadScope& rScope, const SIRResourceView& rSource,
                                                                        const sirSharedDeviceBackingDesc_t& rDesc,
                                                                        sirSharedDeviceBackingReservation_t *pReservation ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !pReservation || !rScope.isActive() || !rSource.isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( rDesc.nPhysicalDeviceId == 0 || rDesc.nDeviceGeneration == 0 || rDesc.nStorageLayoutId == 0 ||
	     rDesc.nFormatId == 0 || rDesc.nCommittedBytes == 0 || rDesc.nProviderId == 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( rDesc.nAlignment == 0 || ( rDesc.nAlignment & ( rDesc.nAlignment - 1 ) ) != 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	if ( rDesc.nCommittedBytes > m_Config.nMaxTrackedBytes - m_nTrackedBytes ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	const int32_t iProvider = findProvider( rDesc.nProviderId );
	if ( iProvider < 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const uint32_t nProviderDeviceGeneration = m_pProviders[iProvider].nDeviceGeneration;
	if ( nProviderDeviceGeneration != rDesc.nDeviceGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	// Check for existing backing with identical identity.
	int32_t iExisting = findSharedBacking( rSource, rDesc );
	if ( iExisting >= 0 ) {
		sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[iExisting];
		if ( !rSlot.u.sharedBacking.bPublished ) {
			return SIR_RESOURCE_RESULT_BUSY;
		}
		pReservation->hBacking = { ( uint32_t )iExisting, rSlot.nGeneration };
		pReservation->nPhysicalBackingId = rSlot.u.sharedBacking.nPhysicalBackingId;
		pReservation->bNeedsNativeCreation = false;
		return SIR_RESOURCE_RESULT_BUSY; // Signal that backing already exists.
	}
	// Pin the source in GRDG for this provider.
	SIRResourcePinHandle pin = {};
	const SIRResourceResult ePin = m_pGraph->pinForProvider( rScope, rSource, rDesc.nProviderId, &pin );
	if ( ePin != SIR_RESOURCE_RESULT_SUCCESS ) {
		return ePin;
	}
	// Allocate a new backing slot.
	int32_t iFree = findFreeUnifiedSlot( SIR_UNIFIED_SLOT_SHARED_BACKING );
	if ( iFree < 0 ) {
		m_pGraph->releaseProviderPin( pin );
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[iFree];
	rSlot.eKind = SIR_UNIFIED_SLOT_SHARED_BACKING;
	rSlot.u.sharedBacking.pin = pin;
	rSlot.u.sharedBacking.hSource = rSource.getHandle();
	rSlot.u.sharedBacking.nSourceRevision = rSource.getRevision();
	rSlot.u.sharedBacking.desc = rDesc;
	rSlot.u.sharedBacking.nNativeToken = 0;
	rSlot.u.sharedBacking.nPhysicalBackingId = ( ( uint64_t )rDesc.nPhysicalDeviceId << 48 ) | ( ( uint64_t )rDesc.nDeviceGeneration << 32 ) | ( ( uint64_t )rDesc.nStorageLayoutId << 16 ) | rDesc.nFormatId;
	rSlot.u.sharedBacking.bOccupied = true;
	rSlot.u.sharedBacking.bPublished = false;
	rSlot.u.sharedBacking.bPendingRetire = false;
	// Update stats.
	const uint32_t iSpace = ( uint32_t )SIR_MEMORY_SPACE_DEVICE;
	const uint64_t nCommitted = rDesc.nCommittedBytes;
	if ( nCommitted > UINT64_MAX - m_Stats.nCommittedBytes[iSpace] || nCommitted > UINT64_MAX - m_Stats.nResidentBytes[iSpace] ) {
		m_pGraph->releaseProviderPin( pin );
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	m_Stats.nCommittedBytes[iSpace] += nCommitted;
	m_Stats.nResidentBytes[iSpace] += nCommitted;
	if ( m_Stats.nCommittedBytes[iSpace] > m_Stats.nPeakCommittedBytes[iSpace] ) {
		m_Stats.nPeakCommittedBytes[iSpace] = m_Stats.nCommittedBytes[iSpace];
	}
	m_nTrackedBytes += nCommitted;
	++m_Stats.nLiveRepresentations;
	pReservation->hBacking = { ( uint32_t )iFree, rSlot.nGeneration };
	pReservation->nPhysicalBackingId = rSlot.u.sharedBacking.nPhysicalBackingId;
	pReservation->bNeedsNativeCreation = true;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

// Public: publish the native token for a reserved backing.
SIRResourceResult SIRResourceDerivedRegistry::publishSharedDeviceBacking( SIRDerivedResourceHandle hBacking, uint64_t nNativeToken ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !nNativeToken || hBacking.m_nSlot >= m_Config.nMaxRepresentations ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[hBacking.m_nSlot];
	if ( rSlot.eKind != SIR_UNIFIED_SLOT_SHARED_BACKING || !rSlot.u.sharedBacking.bOccupied || rSlot.nGeneration != hBacking.m_nGeneration || rSlot.u.sharedBacking.bPublished ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	const int32_t iProvider = findProvider( rSlot.u.sharedBacking.desc.nProviderId );
	if ( iProvider < 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	// Verify the native token is unique for this provider.
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		const sirUnifiedSlot_t& rOther = m_pUnifiedSlots[iSlot];
		if ( rOther.eKind == SIR_UNIFIED_SLOT_SHARED_BACKING && rOther.u.sharedBacking.bOccupied && rOther.u.sharedBacking.bPublished &&
		     rOther.u.sharedBacking.desc.nProviderId == rSlot.u.sharedBacking.desc.nProviderId &&
		     rOther.u.sharedBacking.nNativeToken == nNativeToken ) {
			return SIR_RESOURCE_RESULT_BUSY;
		}
	}
	rSlot.u.sharedBacking.nNativeToken = nNativeToken;
	rSlot.u.sharedBacking.bPublished = true;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

// Public: acquire a borrowed view of a shared backing.
SIRResourceResult SIRResourceDerivedRegistry::acquireSharedDeviceView( const SIRResourceReadScope& rScope, const SIRResourceView& rSource,
                                                                       const sirSharedDeviceViewRequest_t& rRequest,
                                                                       sirSharedDeviceView_t *pView ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !pView || !rScope.isActive() || !rSource.isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( rRequest.nExpectedPublicationEpoch == 0 || rRequest.nBytes == 0 ||
	     rRequest.nPhysicalDeviceId == 0 || rRequest.nDeviceGeneration == 0 ||
	     rRequest.nCompletionProviderId == 0 || rRequest.nCompletionLaneMask == 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	// Validate epoch and revision against GRDG state.
	const uint64_t nCurrentEpoch = m_pGraph->getStats().nPublicationEpoch;
	if ( rRequest.nExpectedPublicationEpoch != nCurrentEpoch ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	if ( rRequest.nExpectedSourceRevision != 0 && rRequest.nExpectedSourceRevision != rSource.getRevision() ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	// Find the backing by matching key identity fields.
	int32_t iBacking = -1;
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		const sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[iSlot];
		if ( rSlot.eKind != SIR_UNIFIED_SLOT_SHARED_BACKING ) continue;
		if ( !rSlot.u.sharedBacking.bOccupied || !rSlot.u.sharedBacking.bPublished ) continue;
		if ( rSlot.u.sharedBacking.desc.nPhysicalDeviceId != rRequest.nPhysicalDeviceId ) continue;
		if ( rSlot.u.sharedBacking.desc.nDeviceGeneration != rRequest.nDeviceGeneration ) continue;
		if ( rSlot.u.sharedBacking.desc.nStorageLayoutId != rRequest.nStorageLayoutId ) continue;
		if ( rSlot.u.sharedBacking.desc.nSupportedUsageMask != rRequest.nSupportedUsageMask ) continue;
		if ( rSlot.u.sharedBacking.desc.nFormatId != rRequest.nFormatId ) continue;
		if ( rSlot.u.sharedBacking.desc.nProviderId != rRequest.nCompletionProviderId ) continue;
		if ( rSlot.u.sharedBacking.nSourceRevision != rSource.getRevision() ) continue;
		iBacking = ( int32_t )iSlot;
		break;
	}
	if ( iBacking < 0 ) return SIR_RESOURCE_RESULT_STALE_REVISION;

	const sirUnifiedSlot_t& rBacking = m_pUnifiedSlots[iBacking];
	if ( rRequest.nOffset + rRequest.nBytes > rBacking.u.sharedBacking.desc.nCommittedBytes ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	if ( ( rRequest.nApprovedUsageMask & ~rBacking.u.sharedBacking.desc.nSupportedUsageMask ) != 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	// Acquire a borrow slot.
	int32_t iBorrow = findFreeSharedBorrow();
	if ( iBorrow < 0 ) return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	sirSharedDeviceBorrowSlot_t& rBorrow = m_pSharedBorrows[iBorrow];
	rBorrow.iBackingSlot = ( uint32_t )iBacking;
	rBorrow.nGeneration = rBacking.nGeneration;
	rBorrow.nOffset = rRequest.nOffset;
	rBorrow.nBytes = rRequest.nBytes;
	rBorrow.nSourceRevision = rSource.getRevision();
	rBorrow.nApprovedUsageMask = rRequest.nApprovedUsageMask;
	rBorrow.nCompletionProviderId = rRequest.nCompletionProviderId;
	rBorrow.nCompletionLaneMask = rRequest.nCompletionLaneMask;
	rBorrow.nDeviceGeneration = rRequest.nDeviceGeneration;
	rBorrow.nNativeResourceToken = rBacking.u.sharedBacking.nNativeToken;
	rBorrow.bReadOnly = ( rRequest.nApprovedUsageMask & 0x1 ) == 0; // Assume bit 0 = write
	rBorrow.bActive = true;
	// Fill view.
	pView->nPhysicalBackingId = rBacking.u.sharedBacking.nPhysicalBackingId;
	pView->nNativeResourceToken = rBacking.u.sharedBacking.nNativeToken;
	pView->nOffset = rRequest.nOffset;
	pView->nBytes = rRequest.nBytes;
	pView->nSourceRevision = rSource.getRevision();
	pView->nDeviceGeneration = rRequest.nDeviceGeneration;
	pView->nApprovedUsageMask = rRequest.nApprovedUsageMask;
	pView->nCompletionProviderId = rRequest.nCompletionProviderId;
	pView->nCompletionLaneMask = rRequest.nCompletionLaneMask;
	pView->bReadOnly = rBorrow.bReadOnly;
	++m_Stats.nLiveSharedBorrows;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

// Public: release a borrowed view.
SIRResourceResult SIRResourceDerivedRegistry::releaseSharedDeviceView( const sirSharedDeviceView_t& rView, const sirProviderCompletionSet_t& required ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	if ( !sirProviderCompletionValid( required ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	// Find the matching borrow slot.
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxSharedBorrows; ++iSlot ) {
		sirSharedDeviceBorrowSlot_t& rBorrow = m_pSharedBorrows[iSlot];
		if ( !rBorrow.bActive ) continue;
		if ( rBorrow.nOffset != rView.nOffset ) continue;
		if ( rBorrow.nBytes != rView.nBytes ) continue;
		if ( rBorrow.nSourceRevision != rView.nSourceRevision ) continue;
		if ( rBorrow.nDeviceGeneration != rView.nDeviceGeneration ) continue;
		if ( rBorrow.nApprovedUsageMask != rView.nApprovedUsageMask ) continue;
		if ( rBorrow.nCompletionProviderId != rView.nCompletionProviderId ) continue;
		if ( rBorrow.nCompletionLaneMask != rView.nCompletionLaneMask ) continue;
		if ( rBorrow.nNativeResourceToken != rView.nNativeResourceToken ) continue;
		if ( rBorrow.bReadOnly != rView.bReadOnly ) continue;
		// Verify provider completion.
		const int32_t iProvider = findProvider( rView.nCompletionProviderId );
		if ( iProvider < 0 ) {
			return SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		const sirProviderCompletionSet_t& completed = m_pProviders[iProvider].completed;
		if ( required.nProviderId != completed.nProviderId || required.nProviderGeneration != completed.nProviderGeneration ||
		     required.nLaneCount != completed.nLaneCount ) {
			return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		if ( !sirProviderCompletionSatisfied( required, completed ) ) {
			return SIR_RESOURCE_RESULT_BUSY;
		}
		// Release the borrow.
		rBorrow.bActive = false;
		rBorrow.nGeneration = nextDerivedGeneration( rBorrow.nGeneration );
		--m_Stats.nLiveSharedBorrows;
		return SIR_RESOURCE_RESULT_SUCCESS;
	}
	return SIR_RESOURCE_RESULT_INVALID_HANDLE;
}

// Public: cancel a reserved but unpublished backing.
SIRResourceResult SIRResourceDerivedRegistry::cancelSharedDeviceBacking( SIRDerivedResourceHandle hBacking ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || hBacking.m_nSlot >= m_Config.nMaxRepresentations ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	sirUnifiedSlot_t& rSlot = m_pUnifiedSlots[hBacking.m_nSlot];
	if ( rSlot.eKind != SIR_UNIFIED_SLOT_SHARED_BACKING || !rSlot.u.sharedBacking.bOccupied || rSlot.nGeneration != hBacking.m_nGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( rSlot.u.sharedBacking.bPublished ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	// Release the GRDG pin.
	if ( m_pGraph->releaseProviderPin( rSlot.u.sharedBacking.pin ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	// Update stats.
	const uint32_t iSpace = ( uint32_t )SIR_MEMORY_SPACE_DEVICE;
	const uint64_t nCommitted = rSlot.u.sharedBacking.desc.nCommittedBytes;
	if ( m_Stats.nCommittedBytes[iSpace] < nCommitted || m_Stats.nResidentBytes[iSpace] < nCommitted ||
	     m_nTrackedBytes < nCommitted ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	m_Stats.nCommittedBytes[iSpace] -= nCommitted;
	m_Stats.nResidentBytes[iSpace] -= nCommitted;
	m_nTrackedBytes -= nCommitted;
	--m_Stats.nLiveRepresentations;
	// Clear slot.
	rSlot.eKind = SIR_UNIFIED_SLOT_EMPTY;
	rSlot.nGeneration = nextDerivedGeneration( rSlot.nGeneration );
	return SIR_RESOURCE_RESULT_SUCCESS;
}
