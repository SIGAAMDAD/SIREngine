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
	sirProviderCompletionSet_t completed;
	bool bRegistered;
} sirDerivedProviderSlot_t;

typedef struct sirDerivedRepresentationSlot_s
{
	SIRDerivedResourceRegistration registration;
	SIRResourcePinHandle pin;
	sirProviderCompletionSet_t retirement;
	uint32_t nGeneration;
	bool bOccupied;
	bool bRetiring;
	bool bNativeReleased;
	bool bSharedDeviceBacking;
	bool bPendingNativeCreate;
	sirSharedDeviceBackingDesc_t physical;
	SIRResourceInfo source;
	uint64_t nFingerprintLo;
	uint64_t nFingerprintHi;
} sirDerivedRepresentationSlot_t;

typedef struct sirSharedDeviceBorrowSlot_s
{
	SIRResourcePinHandle sourcePin;
	sirProviderCompletionSet_t required;
	sirSharedDeviceView_t issuedView;
	uint32_t nBackingSlot;
	uint32_t nBackingGeneration;
	uint32_t nConsumerId;
	uint32_t nCompletionProviderId;
	uint32_t nGeneration;
	bool bOccupied;
	bool bReleased;
} sirSharedDeviceBorrowSlot_t;

static bool fingerprintSource( const SIRResourceView& rSource, uint64_t& nLo, uint64_t& nHi ) noexcept
{
	SIRResourceInfo info = {};
	if ( rSource.getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS || info.m_nChunkCount == 0 ) {
		return false;
	}
	nLo = ( UINT64_C( 0xcbf29ce484222325 ) ^ info.m_nRevision ) * UINT64_C( 0x100000001b3 );
	nHi = UINT64_C( 0x9e3779b97f4a7c15 ) ^ info.m_nRevision;
	for ( uint32_t iChunk = 0; iChunk < info.m_nChunkCount; ++iChunk ) {
		SIRResourceChunkInfo chunk = {};
		if ( rSource.getChunkInfo( iChunk, &chunk ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return false;
		}
		SIRResourceConstBytes physicalBytes = {};
		if ( rSource.getChunk( chunk.m_Id, &physicalBytes ) != SIR_RESOURCE_RESULT_SUCCESS || physicalBytes.m_pData == nullptr ||
		     physicalBytes.m_nSize != chunk.m_nBytes ) {
			return false;
		}
		// A claimed content digest is insufficient to prove two sources share the canonical backing.
		// Only the same physically interned GRDG chunks may share this device allocation.
		const uint64_t values[] = { chunk.m_ContentKey.m_nHashLo,
		                            chunk.m_ContentKey.m_nHashHi,
		                            chunk.m_nBytes,
		                            chunk.m_ContentKey.m_nFormatId,
		                            chunk.m_ContentKey.m_nEncodingId,
		                            chunk.m_Id.m_nValue,
		                            ( uint64_t )( uintptr_t )physicalBytes.m_pData };
		for ( uint64_t nValue : values ) {
			nLo = ( nLo ^ nValue ) * UINT64_C( 0x100000001b3 );
			nHi ^= nValue + UINT64_C( 0x9e3779b97f4a7c15 ) + ( nHi << 6 ) + ( nHi >> 2 );
		}
	}
	return true;
}

// A hash is a lookup hint, not proof of source-storage identity. Confirm exact
// canonical chunk pointer/identity from the pinned GRDG source before aliasing.
static bool sameCanonicalStorage( SIRResourceGraph& rGraph, const SIRResourceReadScope& rScope, const SIRResourceView& rRequested,
                                  const sirDerivedRepresentationSlot_t& rBacking ) noexcept
{
	SIRResourceView resident;
	if ( rGraph.resolve( rScope, rBacking.registration.m_Key.m_Resource, &resident ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return false;
	}
	SIRResourceInfo a = {}, b = {};
	if ( resident.getInfo( &a ) != SIR_RESOURCE_RESULT_SUCCESS || rRequested.getInfo( &b ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     a.m_eKind != b.m_eKind || a.m_nRevision != b.m_nRevision || a.m_nSchemaVersion != b.m_nSchemaVersion ||
	     a.m_nFormatId != b.m_nFormatId || a.m_nChunkCount != b.m_nChunkCount ) {
		return false;
	}
	for ( uint32_t iChunk = 0; iChunk < a.m_nChunkCount; ++iChunk ) {
		SIRResourceChunkInfo first = {}, second = {};
		if ( resident.getChunkInfo( iChunk, &first ) != SIR_RESOURCE_RESULT_SUCCESS ||
		     rRequested.getChunkInfo( iChunk, &second ) != SIR_RESOURCE_RESULT_SUCCESS || first.m_Id.m_nValue != second.m_Id.m_nValue ||
		     first.m_nBytes != second.m_nBytes || first.m_ContentKey.m_nHashLo != second.m_ContentKey.m_nHashLo ||
		     first.m_ContentKey.m_nHashHi != second.m_ContentKey.m_nHashHi ||
		     first.m_ContentKey.m_nFormatId != second.m_ContentKey.m_nFormatId ||
		     first.m_ContentKey.m_nEncodingId != second.m_ContentKey.m_nEncodingId ) {
			return false;
		}
		SIRResourceConstBytes physicalA = {}, physicalB = {};
		if ( resident.getChunk( first.m_Id, &physicalA ) != SIR_RESOURCE_RESULT_SUCCESS ||
		     rRequested.getChunk( second.m_Id, &physicalB ) != SIR_RESOURCE_RESULT_SUCCESS || physicalA.m_pData != physicalB.m_pData ||
		     physicalA.m_nSize != physicalB.m_nSize ) {
			return false;
		}
	}
	return true;
}

static bool sameBackingStorage( const sirSharedDeviceBackingDesc_t& rA, const sirSharedDeviceBackingDesc_t& rB ) noexcept
{
	return rA.nPhysicalDeviceId == rB.nPhysicalDeviceId && rA.nDeviceGeneration == rB.nDeviceGeneration &&
	       rA.nStorageLayoutId == rB.nStorageLayoutId && rA.nStorageFormatId == rB.nStorageFormatId &&
	       rA.nCommittedBytes == rB.nCommittedBytes;
}

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
      m_pRepresentations( nullptr ), m_pSharedBorrows( nullptr ), m_nSharedBorrowCapacity( 0 ), m_nRepresentationMetadataBytes( 0 ),
      m_nRepresentationMetadataAlignment( 0 ), m_Stats{}, m_nTrackedBytes( 0 ), m_bReady( false )
{
	if ( !rGraph.isReady() || !rConfig.allocator.m_pfnAllocate || !rConfig.allocator.m_pfnFree || !rConfig.nMaxProviders ||
	     !rConfig.nMaxRepresentations || !rConfig.nMaxTrackedBytes ) {
		return;
	}
	const uint64_t nProviderBytes = ( uint64_t )rConfig.nMaxProviders * sizeof( sirDerivedProviderSlot_t );
	const uint64_t nRecordBytes = ( uint64_t )rConfig.nMaxRepresentations * sizeof( sirDerivedRepresentationSlot_t );
	m_nSharedBorrowCapacity = rConfig.nMaxSharedBorrows == 0 ? rConfig.nMaxRepresentations : rConfig.nMaxSharedBorrows;
	const uint64_t nBorrowBytes = ( uint64_t )m_nSharedBorrowCapacity * sizeof( sirSharedDeviceBorrowSlot_t );
	const uint64_t nBorrowAlignment = alignof( sirSharedDeviceBorrowSlot_t );
	if ( nRecordBytes > UINT64_MAX - ( nBorrowAlignment - 1 ) ) {
		return;
	}
	const uint64_t nBorrowOffset = ( nRecordBytes + nBorrowAlignment - 1 ) & ~( nBorrowAlignment - 1 );
	if ( nBorrowBytes > UINT64_MAX - nBorrowOffset ) {
		return;
	}
	m_nRepresentationMetadataBytes = nBorrowOffset + nBorrowBytes;
	m_nRepresentationMetadataAlignment = alignof( sirDerivedRepresentationSlot_t ) > nBorrowAlignment
	                                         ? alignof( sirDerivedRepresentationSlot_t )
	                                         : ( uint32_t )nBorrowAlignment;
	if ( nProviderBytes > SIZE_MAX || m_nRepresentationMetadataBytes > SIZE_MAX ) {
		return;
	}
	m_pProviders = ( sirDerivedProviderSlot_t * )rConfig.allocator.m_pfnAllocate(
	    rConfig.allocator.m_pContext, nProviderBytes, alignof( sirDerivedProviderSlot_t ), rConfig.nMetadataDomain );
	m_pRepresentations = ( sirDerivedRepresentationSlot_t * )rConfig.allocator.m_pfnAllocate(
	    rConfig.allocator.m_pContext, m_nRepresentationMetadataBytes, m_nRepresentationMetadataAlignment, rConfig.nMetadataDomain );
	if ( m_pProviders == nullptr || m_pRepresentations == nullptr ) {
		return;
	}
	m_pSharedBorrows = ( sirSharedDeviceBorrowSlot_t * )( ( uint8_t * )m_pRepresentations + nBorrowOffset );
	memset( m_pProviders, 0, ( size_t )nProviderBytes );
	memset( m_pRepresentations, 0, ( size_t )m_nRepresentationMetadataBytes );
	for ( uint32_t iSlot = 0; iSlot < rConfig.nMaxRepresentations; ++iSlot ) {
		m_pRepresentations[iSlot].nGeneration = 1;
	}
	for ( uint32_t iBorrow = 0; iBorrow < m_nSharedBorrowCapacity; ++iBorrow ) {
		m_pSharedBorrows[iBorrow].nGeneration = 1;
	}
	m_bReady = true;
}

SIRResourceDerivedRegistry::~SIRResourceDerivedRegistry( void ) noexcept
{
	if ( m_pSharedBorrows != nullptr && m_bReady ) {
		for ( uint32_t iBorrow = 0; iBorrow < m_nSharedBorrowCapacity; ++iBorrow ) {
			if ( m_pSharedBorrows[iBorrow].bOccupied ) {
				abort();
			}
		}
	}
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
		m_Config.allocator.m_pfnFree( m_Config.allocator.m_pContext, m_pRepresentations, m_nRepresentationMetadataBytes,
		                              m_nRepresentationMetadataAlignment, m_Config.nMetadataDomain );
		m_pRepresentations = nullptr;
		m_pSharedBorrows = nullptr;
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
	if ( rAccounting.m_eSpace > SIR_MEMORY_SPACE_EXTERNAL || rAccounting.m_eSpace == SIR_MEMORY_SPACE_DEVICE ||
	     rAccounting.m_nCommittedBytes == 0 || rAccounting.m_nResidentBytes > rAccounting.m_nCommittedBytes ||
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
		const sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[iSlot];
		if ( !rSlot.bOccupied ) {
			if ( iFree == UINT32_MAX ) {
				iFree = iSlot;
			}
			continue;
		}
		if ( rSlot.bSharedDeviceBacking && rAccounting.m_eSpace == SIR_MEMORY_SPACE_DEVICE &&
		     sameDerivedHandle( rSlot.registration.m_Key.m_Resource, rRegistration.m_Key.m_Resource ) &&
		     rSlot.registration.m_Key.m_nRevision == rRegistration.m_Key.m_nRevision ) {
			return SIR_RESOURCE_RESULT_BUSY;
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
	sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[iFree];
	rSlot.registration = rRegistration;
	rSlot.pin = pin;
	rSlot.retirement = {};
	rSlot.bOccupied = true;
	rSlot.bRetiring = false;
	rSlot.bNativeReleased = false;
	rSlot.bSharedDeviceBacking = false;
	rSlot.bPendingNativeCreate = false;
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
	sirDerivedProviderSlot_t& rProvider = m_pProviders[iProvider];
	for ( uint32_t iBorrow = 0; iBorrow < m_nSharedBorrowCapacity; ++iBorrow ) {
		sirSharedDeviceBorrowSlot_t& rBorrow = m_pSharedBorrows[iBorrow];
		if ( !rBorrow.bOccupied || !rBorrow.bReleased || rBorrow.nCompletionProviderId != rProvider.nProviderId ||
		     !sirProviderCompletionSatisfied( rBorrow.required, rProvider.completed ) ) {
			continue;
		}
		if ( m_pGraph->releaseProviderPin( rBorrow.sourcePin ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_RESOURCE_RESULT_FAILED;
		}
		rBorrow.bOccupied = false;
		rBorrow.bReleased = false;
		rBorrow.nGeneration = nextDerivedGeneration( rBorrow.nGeneration );
		--m_Stats.nLiveSharedBorrows;
	}
	bool bBusy = false;
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[iSlot];
		if ( !rSlot.bOccupied || !rSlot.bRetiring || rSlot.bPendingNativeCreate ) {
			continue;
		}
		const int32_t iOwnerProvider = findProvider( rSlot.registration.m_Key.m_nProviderId );
		if ( iOwnerProvider < 0 || !sirProviderCompletionSatisfied( rSlot.retirement, m_pProviders[iOwnerProvider].completed ) ) {
			continue;
		}
		bool bBorrowPending = false;
		for ( uint32_t iBorrow = 0; iBorrow < m_nSharedBorrowCapacity; ++iBorrow ) {
			const sirSharedDeviceBorrowSlot_t& rBorrow = m_pSharedBorrows[iBorrow];
			if ( rBorrow.bOccupied && rBorrow.nBackingSlot == iSlot && rBorrow.nBackingGeneration == rSlot.nGeneration ) {
				bBorrowPending = true;
				break;
			}
		}
		if ( bBorrowPending ) {
			continue;
		}
		if ( !rSlot.bNativeReleased ) {
			sirDerivedProviderSlot_t& rOwner = m_pProviders[iOwnerProvider];
			if ( !rOwner.callbacks.pfnReleaseNative( rOwner.callbacks.pContext, rSlot.registration.m_nNativeToken ) ) {
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
                                                                    const sirProviderCompletionSet_t& required ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || hRepresentation.m_nSlot >= m_Config.nMaxRepresentations ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[hRepresentation.m_nSlot];
	if ( !rSlot.bOccupied || rSlot.nGeneration != hRepresentation.m_nGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( rSlot.bRetiring || rSlot.bPendingNativeCreate ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	const int32_t iProvider = findProvider( rSlot.registration.m_Key.m_nProviderId );
	if ( iProvider < 0 || !sirProviderCompletionValid( required ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const sirProviderCompletionSet_t& completed = m_pProviders[iProvider].completed;
	if ( required.nProviderId != completed.nProviderId || required.nProviderGeneration != completed.nProviderGeneration ||
	     required.nLaneCount != completed.nLaneCount ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	rSlot.bRetiring = true;
	rSlot.retirement = required;
	++m_Stats.nPendingRetirements;
	if ( sirProviderCompletionSatisfied( required, completed ) ) {
		( void )collectProvider( ( uint32_t )iProvider );
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
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
	const sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[hRepresentation.m_nSlot];
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

SIRResourceResult SIRResourceDerivedRegistry::checkCurrentSource( const SIRResourceReadScope& rScope,
                                                                  const SIRResourceView& rSource ) const noexcept
{
	SIRResourceInfo source = {};
	if ( rSource.getInfo( &source ) != SIR_RESOURCE_RESULT_SUCCESS || source.m_eState != SIR_RESOURCE_STATE_RESIDENT ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	SIRResourceHandle hCurrent = {};
	if ( m_pGraph->find( rScope, source.m_Key, &hCurrent ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	SIRResourceView current;
	if ( m_pGraph->resolve( rScope, hCurrent, &current ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     current.getRevision() != rSource.getRevision() ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::reserveSharedDeviceBacking( const SIRResourceReadScope& rScope,
                                                                          const SIRResourceView& rSource,
                                                                          const sirSharedDeviceBackingDesc_t& rDesc,
                                                                          sirSharedDeviceBackingReservation_t *pReservation ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !pReservation || !rDesc.nPhysicalDeviceId || !rDesc.nDeviceGeneration || !rDesc.nStorageLayoutId ||
	     !rDesc.nStorageFormatId || !rDesc.nSupportedUsageMask || !rDesc.nProviderId || !rDesc.nCommittedBytes ||
	     rDesc.nResidentBytes > rDesc.nCommittedBytes || !rDesc.nAlignment || ( rDesc.nAlignment & ( rDesc.nAlignment - 1 ) ) != 0 ||
	     findProvider( rDesc.nProviderId ) < 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const SIRResourceResult eSource = checkCurrentSource( rScope, rSource );
	if ( eSource != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eSource;
	}
	SIRResourceInfo info = {};
	( void )rSource.getInfo( &info );
	if ( info.m_nChunkCount == 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const int32_t iOwnerProvider = findProvider( rDesc.nProviderId );
	if ( iOwnerProvider < 0 || m_pProviders[iOwnerProvider].completed.nProviderGeneration != rDesc.nDeviceGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	uint64_t nLo = 0, nHi = 0;
	if ( !fingerprintSource( rSource, nLo, nHi ) ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	uint32_t iFree = UINT32_MAX;
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		const sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[iSlot];
		if ( !rSlot.bOccupied ) {
			if ( iFree == UINT32_MAX ) {
				iFree = iSlot;
			}
			continue;
		}
		if ( !rSlot.bSharedDeviceBacking || rSlot.physical.nPhysicalDeviceId != rDesc.nPhysicalDeviceId ) {
			continue;
		}
		// Switching a physical device generation requires a full drain. No second
		// simultaneous full source allocation may be admitted during reset.
		if ( rSlot.physical.nDeviceGeneration != rDesc.nDeviceGeneration ) {
			return SIR_RESOURCE_RESULT_BUSY;
		}
		if ( rSlot.nFingerprintLo != nLo || rSlot.nFingerprintHi != nHi || rSlot.source.m_eKind != info.m_eKind ||
		     rSlot.source.m_nSchemaVersion != info.m_nSchemaVersion || rSlot.source.m_nFormatId != info.m_nFormatId ) {
			continue;
		}
		if ( !sameCanonicalStorage( *m_pGraph, rScope, rSource, rSlot ) ) {
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		if ( !sameBackingStorage( rSlot.physical, rDesc ) || rSlot.physical.nAlignment < rDesc.nAlignment ||
		     ( rSlot.physical.nSupportedUsageMask & rDesc.nSupportedUsageMask ) != rDesc.nSupportedUsageMask ) {
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		*pReservation = { { iSlot, rSlot.nGeneration }, ( ( uint64_t )rSlot.nGeneration << 32 ) | ( iSlot + 1 ), false };
		return rSlot.bPendingNativeCreate ? SIR_RESOURCE_RESULT_BUSY : SIR_RESOURCE_RESULT_SUCCESS;
	}
	if ( iFree == UINT32_MAX || rDesc.nCommittedBytes > m_Config.nMaxTrackedBytes - m_nTrackedBytes ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	SIRResourcePinHandle sourcePin = {};
	const SIRResourceResult ePin = m_pGraph->pinForProvider( rScope, rSource, rDesc.nProviderId, &sourcePin );
	if ( ePin != SIR_RESOURCE_RESULT_SUCCESS ) {
		return ePin;
	}
	sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[iFree];
	rSlot.registration = {};
	rSlot.registration.m_Key = { rSource.getHandle(), info.m_nRevision, rDesc.nProviderId, 1, 0, 0 };
	rSlot.registration.m_Accounting = { SIR_MEMORY_SPACE_DEVICE, rDesc.nDomainId, rDesc.nCommittedBytes, rDesc.nResidentBytes,
	                                    info.m_nRevision };
	rSlot.pin = sourcePin;
	rSlot.source = info;
	rSlot.physical = rDesc;
	rSlot.nFingerprintLo = nLo;
	rSlot.nFingerprintHi = nHi;
	rSlot.retirement = {};
	rSlot.bOccupied = true;
	rSlot.bRetiring = false;
	rSlot.bNativeReleased = false;
	rSlot.bSharedDeviceBacking = true;
	rSlot.bPendingNativeCreate = true;
	m_nTrackedBytes += rDesc.nCommittedBytes;
	*pReservation = { { iFree, rSlot.nGeneration }, ( ( uint64_t )rSlot.nGeneration << 32 ) | ( iFree + 1 ), true };
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::publishSharedDeviceBacking( SIRDerivedResourceHandle hBacking,
                                                                          uint64_t nNativeToken ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !nNativeToken || hBacking.m_nSlot >= m_Config.nMaxRepresentations ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[hBacking.m_nSlot];
	if ( !rSlot.bOccupied || rSlot.nGeneration != hBacking.m_nGeneration || !rSlot.bSharedDeviceBacking || !rSlot.bPendingNativeCreate ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		if ( iSlot != hBacking.m_nSlot && m_pRepresentations[iSlot].bOccupied && !m_pRepresentations[iSlot].bPendingNativeCreate &&
		     m_pRepresentations[iSlot].registration.m_nNativeToken == nNativeToken ) {
			return SIR_RESOURCE_RESULT_BUSY;
		}
	}
	const uint32_t iSpace = SIR_MEMORY_SPACE_DEVICE;
	if ( rSlot.physical.nCommittedBytes > UINT64_MAX - m_Stats.nCommittedBytes[iSpace] ||
	     rSlot.physical.nResidentBytes > UINT64_MAX - m_Stats.nResidentBytes[iSpace] ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	rSlot.registration.m_nNativeToken = nNativeToken;
	rSlot.bPendingNativeCreate = false;
	m_Stats.nCommittedBytes[iSpace] += rSlot.physical.nCommittedBytes;
	m_Stats.nResidentBytes[iSpace] += rSlot.physical.nResidentBytes;
	if ( m_Stats.nCommittedBytes[iSpace] > m_Stats.nPeakCommittedBytes[iSpace] ) {
		m_Stats.nPeakCommittedBytes[iSpace] = m_Stats.nCommittedBytes[iSpace];
	}
	++m_Stats.nLiveRepresentations;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::cancelSharedDeviceBacking( SIRDerivedResourceHandle hBacking ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || hBacking.m_nSlot >= m_Config.nMaxRepresentations ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[hBacking.m_nSlot];
	if ( !rSlot.bOccupied || rSlot.nGeneration != hBacking.m_nGeneration || !rSlot.bPendingNativeCreate ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( m_pGraph->releaseProviderPin( rSlot.pin ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	m_nTrackedBytes -= rSlot.physical.nCommittedBytes;
	rSlot.bOccupied = false;
	rSlot.bPendingNativeCreate = false;
	rSlot.bSharedDeviceBacking = false;
	rSlot.nGeneration = nextDerivedGeneration( rSlot.nGeneration );
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceDerivedRegistry::acquireSharedDeviceView( const SIRResourceReadScope& rScope, const SIRResourceView& rSource,
                                                                       const sirSharedDeviceViewRequest_t& rRequest,
                                                                       sirSharedDeviceView_t *pView ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !pView || !rRequest.nPhysicalDeviceId || !rRequest.nDeviceGeneration || !rRequest.nStorageLayoutId ||
	     !rRequest.nStorageFormatId || !rRequest.nRequiredUsageMask || !rRequest.nBytes || !rRequest.nExpectedPublicationEpoch ||
	     !rRequest.nConsumerId || !rRequest.nCompletionProviderId || !rRequest.nRequiredAlignment ||
	     ( rRequest.nRequiredAlignment & ( rRequest.nRequiredAlignment - 1 ) ) != 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( m_pGraph->getStats().nPublicationEpoch != rRequest.nExpectedPublicationEpoch ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	const SIRResourceResult eSource = checkCurrentSource( rScope, rSource );
	if ( eSource != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eSource;
	}
	SIRResourceInfo info = {};
	( void )rSource.getInfo( &info );
	if ( info.m_nRevision != rRequest.nExpectedSourceRevision ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	const int32_t iProvider = findProvider( rRequest.nCompletionProviderId );
	if ( iProvider < 0 || m_pProviders[iProvider].completed.nProviderGeneration != rRequest.nDeviceGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	uint64_t nLo = 0, nHi = 0;
	if ( !fingerprintSource( rSource, nLo, nHi ) ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	for ( uint32_t iSlot = 0; iSlot < m_Config.nMaxRepresentations; ++iSlot ) {
		const sirDerivedRepresentationSlot_t& rSlot = m_pRepresentations[iSlot];
		if ( !rSlot.bOccupied || !rSlot.bSharedDeviceBacking || rSlot.bPendingNativeCreate || rSlot.bRetiring ||
		     rSlot.physical.nPhysicalDeviceId != rRequest.nPhysicalDeviceId ||
		     rSlot.physical.nDeviceGeneration != rRequest.nDeviceGeneration || rSlot.nFingerprintLo != nLo || rSlot.nFingerprintHi != nHi ||
		     rSlot.source.m_eKind != info.m_eKind || rSlot.source.m_nFormatId != info.m_nFormatId ||
		     rSlot.source.m_nSchemaVersion != info.m_nSchemaVersion ) {
			continue;
		}
		if ( !sameCanonicalStorage( *m_pGraph, rScope, rSource, rSlot ) ) {
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		if ( rSlot.physical.nStorageLayoutId != rRequest.nStorageLayoutId || rSlot.physical.nStorageFormatId != rRequest.nStorageFormatId ||
		     ( rSlot.physical.nSupportedUsageMask & rRequest.nRequiredUsageMask ) != rRequest.nRequiredUsageMask ||
		     rRequest.nOffset > rSlot.physical.nCommittedBytes || rRequest.nBytes > rSlot.physical.nCommittedBytes - rRequest.nOffset ||
		     rRequest.nRequiredAlignment > rSlot.physical.nAlignment || ( rRequest.nOffset & ( rRequest.nRequiredAlignment - 1 ) ) != 0 ) {
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		for ( uint32_t iBorrow = 0; iBorrow < m_nSharedBorrowCapacity; ++iBorrow ) {
			sirSharedDeviceBorrowSlot_t& rBorrow = m_pSharedBorrows[iBorrow];
			if ( rBorrow.bOccupied ) {
				continue;
			}
			SIRResourcePinHandle pin = {};
			const SIRResourceResult ePin = m_pGraph->pinForProvider( rScope, rSource, rRequest.nCompletionProviderId, &pin );
			if ( ePin != SIR_RESOURCE_RESULT_SUCCESS ) {
				return ePin;
			}
			rBorrow.sourcePin = pin;
			rBorrow.required = {};
			rBorrow.nBackingSlot = iSlot;
			rBorrow.nBackingGeneration = rSlot.nGeneration;
			rBorrow.nConsumerId = rRequest.nConsumerId;
			rBorrow.nCompletionProviderId = rRequest.nCompletionProviderId;
			rBorrow.bOccupied = true;
			rBorrow.bReleased = false;
			++m_Stats.nLiveSharedBorrows;
			*pView = { ( ( uint64_t )rSlot.nGeneration << 32 ) | ( iSlot + 1 ),
			           rSlot.registration.m_nNativeToken,
			           rRequest.nOffset,
			           rRequest.nBytes,
			           info.m_nRevision,
			           rRequest.nDeviceGeneration,
			           rRequest.nRequiredUsageMask,
			           rRequest.nConsumerId,
			           iBorrow,
			           rBorrow.nGeneration,
			           true,
			           rRequest.nCompletionProviderId };
			rBorrow.issuedView = *pView;
			return SIR_RESOURCE_RESULT_SUCCESS;
		}
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	return SIR_RESOURCE_RESULT_NOT_RESIDENT;
}

SIRResourceResult SIRResourceDerivedRegistry::releaseSharedDeviceView( const sirSharedDeviceView_t& rView,
                                                                       const sirProviderCompletionSet_t& required ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || rView.nBorrowSlot >= m_nSharedBorrowCapacity || !sirProviderCompletionValid( required ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	sirSharedDeviceBorrowSlot_t& rBorrow = m_pSharedBorrows[rView.nBorrowSlot];
	// The returned lease is an immutable capability, not just a generation and
	// consumer label. Releasing a modified offset, native token or revision must
	// never retire the actual canonical pin held by this slot.
	const sirSharedDeviceView_t& issued = rBorrow.issuedView;
	if ( !rBorrow.bOccupied || rBorrow.bReleased || rBorrow.nGeneration != rView.nBorrowGeneration ||
	     rBorrow.nConsumerId != rView.nConsumerId ||
	     ( ( uint64_t )rBorrow.nBackingGeneration << 32 ) + rBorrow.nBackingSlot + 1 != rView.nPhysicalBackingId ||
	     rBorrow.nCompletionProviderId != required.nProviderId || issued.nPhysicalBackingId != rView.nPhysicalBackingId ||
	     issued.nNativeResourceToken != rView.nNativeResourceToken || issued.nOffset != rView.nOffset || issued.nBytes != rView.nBytes ||
	     issued.nSourceRevision != rView.nSourceRevision || issued.nDeviceGeneration != rView.nDeviceGeneration ||
	     issued.nApprovedUsageMask != rView.nApprovedUsageMask || issued.nBorrowSlot != rView.nBorrowSlot ||
	     issued.nBorrowGeneration != rView.nBorrowGeneration || issued.bReadOnly != rView.bReadOnly ||
	     issued.nCompletionProviderId != rView.nCompletionProviderId ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	const int32_t iProvider = findProvider( rBorrow.nCompletionProviderId );
	if ( iProvider < 0 || required.nProviderGeneration != m_pProviders[iProvider].completed.nProviderGeneration ||
	     required.nLaneCount != m_pProviders[iProvider].completed.nLaneCount ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	rBorrow.required = required;
	rBorrow.bReleased = true;
	return collectProvider( ( uint32_t )iProvider );
}
