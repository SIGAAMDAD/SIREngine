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

#include <Audio/FMOD/SIRFMODBankStore.h>
#include <Core/Containers/SIRRawStorage.h>
#include <stdlib.h>
#include <string.h>

SIRFMODBankStore::~SIRFMODBankStore( void ) noexcept
{
	if ( shutdown() != SIR_AUDIO_OK ) {
		// A native bank must never be abandoned while its handle is owned.
		abort();
	}
}

bool SIRFMODBankStore::isReady( void ) const noexcept
{
	return m_bInitialized && m_pContext != nullptr && m_pContext->isReady() && m_pContext->getEpoch() == m_nContextEpoch;
}

bool SIRFMODBankStore::findBank( uint64_t nPackageId, uint32_t &iBank ) const noexcept
{
	for ( uint32_t iCurrent = 0; iCurrent < m_nBankCount; ++iCurrent ) {
		if ( m_pBanks[iCurrent].nPackageId == nPackageId ) {
			iBank = iCurrent;
			return true;
		}
	}
	return false;
}

void SIRFMODBankStore::releaseStorage( void ) noexcept
{
	if ( m_pBanks != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pBanks, ( size_t )m_nBankCapacity * sizeof( sirFMODBankSlot_t ),
		                           alignof( sirFMODBankSlot_t ) );
	}
	if ( m_pEvents != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pEvents, ( size_t )m_nEventCapacity * sizeof( sirFMODBankEvent_t ),
		                           alignof( sirFMODBankEvent_t ) );
	}
	m_pBanks = nullptr;
	m_pEvents = nullptr;
	m_nBankCount = 0;
	m_nEventCount = 0;
	m_nBankCapacity = 0;
	m_nEventCapacity = 0;
	m_pContext = nullptr;
	m_pVoices = nullptr;
	m_pAllocator = nullptr;
	m_Ops = {};
	m_nContextEpoch = 0;
	m_bInitialized = false;
}

sirAudioResult_t SIRFMODBankStore::initialize( SIRFMODContext *pContext, SIRFMODVoiceBackend *pVoices,
                                               const sirContainerAllocator_t *pAllocator, const sirFMODBankNativeOps_t &ops,
                                               uint32_t nMaxBanks, uint32_t nMaxEventBindings ) noexcept
{
	if ( m_bInitialized || m_pBanks != nullptr || pContext == nullptr || !pContext->isReady() || pVoices == nullptr ||
	     !pVoices->isInitialized() || !SIRRawStorage::isValidAllocator( pAllocator ) || nMaxBanks == 0 || nMaxEventBindings == 0 ||
	     ops.nAbiVersion != 1 || ops.loadFile == nullptr || ops.unload == nullptr || ops.preloadSamples == nullptr ||
	     ops.sampleState == nullptr || ops.containsEvent == nullptr ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	size_t nBankBytes = 0;
	size_t nEventBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( nMaxBanks, sizeof( sirFMODBankSlot_t ), &nBankBytes ) ||
	     !SIRRawStorage::checkedMultiply( nMaxEventBindings, sizeof( sirFMODBankEvent_t ), &nEventBytes ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	void *pBankMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, nBankBytes, alignof( sirFMODBankSlot_t ), &pBankMemory ) != SIR_CONTAINER_OK ) {
		return SIR_AUDIO_CAPACITY;
	}
	void *pEventMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, nEventBytes, alignof( sirFMODBankEvent_t ), &pEventMemory ) != SIR_CONTAINER_OK ) {
		SIRRawStorage::deallocate( pAllocator, pBankMemory, nBankBytes, alignof( sirFMODBankSlot_t ) );
		return SIR_AUDIO_CAPACITY;
	}

	m_pBanks = ( sirFMODBankSlot_t * )pBankMemory;
	m_pEvents = ( sirFMODBankEvent_t * )pEventMemory;
	memset( m_pBanks, 0, nBankBytes );
	memset( m_pEvents, 0, nEventBytes );
	m_pContext = pContext;
	m_pVoices = pVoices;
	m_pAllocator = pAllocator;
	m_Ops = ops;
	m_nBankCapacity = nMaxBanks;
	m_nEventCapacity = nMaxEventBindings;
	m_nContextEpoch = pContext->getEpoch();
	m_nLastNativeResult = 0;
	m_bInitialized = true;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODBankStore::loadBank( uint64_t nPackageId, const char *szBankPath ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nPackageId == 0 || szBankPath == nullptr || szBankPath[0] == '\0' ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	uint32_t iBank = 0;
	if ( findBank( nPackageId, iBank ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_nBankCount == m_nBankCapacity ) {
		return SIR_AUDIO_CAPACITY;
	}

	void *pBank = nullptr;
	m_nLastNativeResult = m_Ops.loadFile( m_Ops.pUserData, m_pContext->m_pStudio, szBankPath, &pBank );
	if ( m_nLastNativeResult != 0 || pBank == nullptr ) {
		// A native load must return a null handle on failure. A nonnull one
		// is quarantined in a slot so the caller can explicitly unload it.
		if ( pBank != nullptr ) {
			m_pBanks[m_nBankCount++] = { nPackageId, pBank, 0, 0, 0, {} };
		}
		return SIR_AUDIO_BACKEND_ERROR;
	}

	m_pBanks[m_nBankCount++] = { nPackageId, pBank, 0, 0, 1, {} };
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODBankStore::bindEvent( uint64_t nPackageId, uint64_t nEventId, const char *szEventPath ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nEventId == 0 || szEventPath == nullptr || szEventPath[0] == '\0' ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	uint32_t iBank = 0;
	if ( !findBank( nPackageId, iBank ) || m_pBanks[iBank].bUsable == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_nEventCount == m_nEventCapacity ) {
		return SIR_AUDIO_CAPACITY;
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEventCount; ++iEvent ) {
		if ( m_pEvents[iEvent].nEventId == nEventId ) {
			return SIR_AUDIO_INVALID_ARGUMENT;
		}
	}

	uint8_t bFound = 0;
	m_nLastNativeResult =
	    m_Ops.containsEvent( m_Ops.pUserData, m_pContext->m_pStudio, m_pBanks[iBank].pBank, szEventPath, m_pAllocator, &bFound );
	if ( m_nLastNativeResult != 0 ) {
		return SIR_AUDIO_BACKEND_ERROR;
	}
	if ( bFound == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	const sirAudioResult_t eResult = m_pVoices->registerEvent( nEventId, szEventPath );
	if ( eResult != SIR_AUDIO_OK ) {
		return eResult;
	}
	m_pEvents[m_nEventCount++] = { nPackageId, nEventId };
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODBankStore::unbindEvent( uint64_t nPackageId, uint64_t nEventId ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEventCount; ++iEvent ) {
		if ( m_pEvents[iEvent].nPackageId == nPackageId && m_pEvents[iEvent].nEventId == nEventId ) {
			if ( m_pVoices->isInitialized() ) {
				const sirAudioResult_t eResult = m_pVoices->unregisterEvent( nEventId );
				if ( eResult != SIR_AUDIO_OK ) {
					return eResult;
				}
			}
			if ( iEvent + 1 < m_nEventCount ) {
				memmove( m_pEvents + iEvent, m_pEvents + iEvent + 1,
				         ( size_t )( m_nEventCount - iEvent - 1 ) * sizeof( sirFMODBankEvent_t ) );
			}
			--m_nEventCount;
			return SIR_AUDIO_OK;
		}
	}
	return SIR_AUDIO_INVALID_ARGUMENT;
}

sirAudioResult_t SIRFMODBankStore::preloadSamples( uint64_t nPackageId ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iBank = 0;
	if ( !findBank( nPackageId, iBank ) || m_pBanks[iBank].bUsable == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_pBanks[iBank].bPreloadRequested != 0 ) {
		return SIR_AUDIO_OK;
	}
	m_nLastNativeResult = m_Ops.preloadSamples( m_Ops.pUserData, m_pBanks[iBank].pBank );
	if ( m_nLastNativeResult != 0 ) {
		return SIR_AUDIO_BACKEND_ERROR;
	}
	m_pBanks[iBank].bPreloadRequested = 1;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODBankStore::getSampleState( uint64_t nPackageId, sirFMODSampleState_t &eState ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iBank = 0;
	if ( !findBank( nPackageId, iBank ) || m_pBanks[iBank].bUsable == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_pBanks[iBank].bPreloadRequested == 0 ) {
		eState = SIR_FMOD_SAMPLE_UNLOADED;
		return SIR_AUDIO_OK;
	}
	m_nLastNativeResult = m_Ops.sampleState( m_Ops.pUserData, m_pBanks[iBank].pBank, &eState );
	return m_nLastNativeResult == 0 ? SIR_AUDIO_OK : SIR_AUDIO_BACKEND_ERROR;
}

void SIRFMODBankStore::eraseBindings( uint64_t nPackageId ) noexcept
{
	uint32_t iEvent = 0;
	while ( iEvent < m_nEventCount ) {
		if ( m_pEvents[iEvent].nPackageId != nPackageId ) {
			++iEvent;
			continue;
		}
		if ( m_pVoices->isInitialized() ) {
			// Already checked all live instances before native bank unload.
			const sirAudioResult_t eResult = m_pVoices->unregisterEvent( m_pEvents[iEvent].nEventId );
			if ( eResult != SIR_AUDIO_OK && eResult != SIR_AUDIO_INVALID_ARGUMENT ) {
				abort();
			}
		}
		if ( iEvent + 1 < m_nEventCount ) {
			memmove( m_pEvents + iEvent, m_pEvents + iEvent + 1, ( size_t )( m_nEventCount - iEvent - 1 ) * sizeof( sirFMODBankEvent_t ) );
		}
		--m_nEventCount;
	}
}

sirAudioResult_t SIRFMODBankStore::unloadBank( uint64_t nPackageId ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iBank = 0;
	if ( !findBank( nPackageId, iBank ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	// Exact bank owners can retire independently. Unscoped registrations
	// retain the conservative context-wide guard until their owner is known.
	if ( m_pContext->getUnscopedBankObjectCount() != 0 || m_pBanks[iBank].nBorrowedObjects != 0 ) {
		return SIR_AUDIO_BUSY;
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEventCount; ++iEvent ) {
		if ( m_pEvents[iEvent].nPackageId == nPackageId && m_pVoices->isEventInUse( m_pEvents[iEvent].nEventId ) ) {
			return SIR_AUDIO_BUSY;
		}
	}

	m_nLastNativeResult = m_Ops.unload( m_Ops.pUserData, m_pBanks[iBank].pBank );
	if ( m_nLastNativeResult != 0 ) {
		return SIR_AUDIO_BACKEND_ERROR;
	}

	eraseBindings( nPackageId );
	if ( iBank + 1 < m_nBankCount ) {
		memmove( m_pBanks + iBank, m_pBanks + iBank + 1, ( size_t )( m_nBankCount - iBank - 1 ) * sizeof( sirFMODBankSlot_t ) );
	}
	--m_nBankCount;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODBankStore::shutdown( void ) noexcept
{
	if ( m_pBanks == nullptr ) {
		return SIR_AUDIO_OK;
	}
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEventCount; ++iEvent ) {
		if ( m_pVoices->isEventInUse( m_pEvents[iEvent].nEventId ) ) {
			return SIR_AUDIO_BUSY;
		}
	}

	while ( m_nBankCount != 0 ) {
		const uint64_t nPackageId = m_pBanks[m_nBankCount - 1].nPackageId;
		const sirAudioResult_t eResult = unloadBank( nPackageId );
		if ( eResult != SIR_AUDIO_OK ) {
			return eResult;
		}
	}
	releaseStorage();
	return SIR_AUDIO_OK;
}

bool SIRFMODBankStore::isBoundTo( const SIRFMODContext *pContext ) const noexcept
{
	return m_pContext == pContext && isReady();
}

sirAudioResult_t SIRFMODBankStore::pinBankObject( uint64_t nPackageId ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iBank = 0;
	if ( !findBank( nPackageId, iBank ) || m_pBanks[iBank].bUsable == 0 ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( m_pBanks[iBank].nBorrowedObjects == UINT32_MAX ) {
		return SIR_AUDIO_CAPACITY;
	}
	++m_pBanks[iBank].nBorrowedObjects;
	return SIR_AUDIO_OK;
}

void SIRFMODBankStore::unpinBankObject( uint64_t nPackageId ) noexcept
{
	uint32_t iBank = 0;
	if ( !isReady() || !findBank( nPackageId, iBank ) || m_pBanks[iBank].nBorrowedObjects == 0 ) {
		abort();
	}
	--m_pBanks[iBank].nBorrowedObjects;
}

uint32_t SIRFMODBankStore::getBankObjectPinCount( uint64_t nPackageId ) const noexcept
{
	uint32_t iBank = 0;
	return isReady() && findBank( nPackageId, iBank ) ? m_pBanks[iBank].nBorrowedObjects : 0;
}

bool SIRFMODBankStore::hasLoadedBank( uint64_t nPackageId ) const noexcept
{
	uint32_t iBank = 0;
	return isReady() && findBank( nPackageId, iBank ) && m_pBanks[iBank].bUsable != 0;
}

uint32_t SIRFMODBankStore::getLoadedBankCount( void ) const noexcept
{
	return m_nBankCount;
}

uint32_t SIRFMODBankStore::getBoundEventCount( void ) const noexcept
{
	return m_nEventCount;
}

int32_t SIRFMODBankStore::getLastNativeResult( void ) const noexcept
{
	return m_nLastNativeResult;
}
