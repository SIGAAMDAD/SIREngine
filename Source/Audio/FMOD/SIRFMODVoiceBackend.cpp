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

#include <Audio/FMOD/SIRFMODVoiceBackend.h>
#include <Core/Containers/SIRRawStorage.h>
#include <stdlib.h>
#include <string.h>

SIRFMODVoiceBackend::~SIRFMODVoiceBackend( void ) noexcept
{
	shutdownCallback( this );
}

sirAudioBackendOps_t SIRFMODVoiceBackend::getBackendOps( void ) noexcept
{
	return { 2,
	         initializeCallback,
	         shutdownCallback,
	         submitCallback,
	         drainCallback,
	         updateCallback,
	         capabilitiesCallback,
	         submitSpatialCallback };
}

sirAudioResult_t SIRFMODVoiceBackend::configure( SIRFMODContext *pContext, const sirContainerAllocator_t *pAllocator,
                                                 const sirFMODVoiceNativeOps_t &ops, uint32_t nMaxRegisteredEvents ) noexcept
{
	if ( m_bInitialized || m_pVoices != nullptr || pContext == nullptr || !pContext->isReady() || pAllocator == nullptr ||
	     pAllocator->pfnAllocate == nullptr || pAllocator->pfnDeallocate == nullptr || nMaxRegisteredEvents == 0 || ops.nAbiVersion != 1 ||
	     ops.resolveEvent == nullptr || ops.createInstance == nullptr || ops.startInstance == nullptr || ops.stopInstance == nullptr ||
	     ops.isStopped == nullptr || ops.releaseInstance == nullptr ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	m_pContext = pContext;
	m_pAllocator = pAllocator;
	m_Ops = ops;
	m_nEventCapacity = nMaxRegisteredEvents;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODVoiceBackend::initializeCallback( void *pContext, const sirAudioBackendConfig_t *pConfig ) noexcept
{
	SIRFMODVoiceBackend *pBackend = ( SIRFMODVoiceBackend * )pContext;
	if ( pBackend == nullptr || pConfig == nullptr || pBackend->m_bInitialized || pBackend->m_pVoices != nullptr ||
	     pBackend->m_pContext == nullptr || !pBackend->m_pContext->isReady() || pBackend->m_pAllocator == nullptr ||
	     pConfig->nMaxVoices == 0 || pConfig->nCompletionCapacity < 2 || pConfig->nMaxVoices > UINT32_MAX / 2 ||
	     pConfig->nCompletionCapacity < pConfig->nMaxVoices * 2 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	size_t nVoiceBytes = 0;
	size_t nEventBytes = 0;
	size_t nCompletionBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( pConfig->nMaxVoices, sizeof( sirFMODVoiceSlot_t ), &nVoiceBytes ) ||
	     !SIRRawStorage::checkedMultiply( pBackend->m_nEventCapacity, sizeof( sirFMODRegisteredEvent_t ), &nEventBytes ) ||
	     !SIRRawStorage::checkedMultiply( pConfig->nCompletionCapacity, sizeof( sirAudioCompletion_t ), &nCompletionBytes ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pBackend->m_pAllocator, nVoiceBytes, alignof( sirFMODVoiceSlot_t ), &pMemory ) != SIR_CONTAINER_OK ) {
		return SIR_AUDIO_CAPACITY;
	}
	pBackend->m_pVoices = ( sirFMODVoiceSlot_t * )pMemory;
	pBackend->m_nVoiceCapacity = pConfig->nMaxVoices;
	memset( pBackend->m_pVoices, 0, nVoiceBytes );
	pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pBackend->m_pAllocator, nEventBytes, alignof( sirFMODRegisteredEvent_t ), &pMemory ) !=
	     SIR_CONTAINER_OK ) {
		pBackend->releaseStorage();
		return SIR_AUDIO_CAPACITY;
	}
	pBackend->m_pEvents = ( sirFMODRegisteredEvent_t * )pMemory;
	pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pBackend->m_pAllocator, nCompletionBytes, alignof( sirAudioCompletion_t ), &pMemory ) !=
	     SIR_CONTAINER_OK ) {
		pBackend->releaseStorage();
		return SIR_AUDIO_CAPACITY;
	}
	pBackend->m_pCompletions = ( sirAudioCompletion_t * )pMemory;
	pBackend->m_nCompletionCapacity = pConfig->nCompletionCapacity;
	pBackend->m_iCompletionRead = 0;
	pBackend->m_iCompletionWrite = 0;
	pBackend->m_nCompletions = 0;
	pBackend->m_nReservedCompletions = 0;
	pBackend->m_nEventCount = 0;
	pBackend->m_nContextEpoch = pBackend->m_pContext->getEpoch();
	pBackend->m_nLastNativeResult = 0;
	pBackend->m_bInitialized = true;
	return SIR_AUDIO_OK;
}

void SIRFMODVoiceBackend::releaseStorage( void ) noexcept
{
	if ( m_pVoices != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pVoices, ( size_t )m_nVoiceCapacity * sizeof( sirFMODVoiceSlot_t ),
		                           alignof( sirFMODVoiceSlot_t ) );
	}
	if ( m_pEvents != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pEvents, ( size_t )m_nEventCapacity * sizeof( sirFMODRegisteredEvent_t ),
		                           alignof( sirFMODRegisteredEvent_t ) );
	}
	if ( m_pCompletions != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pCompletions, ( size_t )m_nCompletionCapacity * sizeof( sirAudioCompletion_t ),
		                           alignof( sirAudioCompletion_t ) );
	}
	m_pVoices = nullptr;
	m_pEvents = nullptr;
	m_pCompletions = nullptr;
	m_nVoiceCapacity = 0;
	m_nCompletionCapacity = 0;
	m_nEventCount = 0;
	m_nCompletions = 0;
	m_nReservedCompletions = 0;
	m_bInitialized = false;
	m_nContextEpoch = 0;
}

void SIRFMODVoiceBackend::shutdownCallback( void *pContext ) noexcept
{
	SIRFMODVoiceBackend *pBackend = ( SIRFMODVoiceBackend * )pContext;
	if ( pBackend == nullptr ) {
		return;
	}
	if ( pBackend->m_pVoices != nullptr ) {
		for ( uint32_t iVoice = 0; iVoice < pBackend->m_nVoiceCapacity; ++iVoice ) {
			sirFMODVoiceSlot_t &slot = pBackend->m_pVoices[iVoice];
			if ( slot.pInstance == nullptr ) {
				continue;
			}
			// The FMOD native release marks the instance for eventual destruction.
			// The owning Studio context outlives this backend and performs teardown.
			const int32_t nStop = slot.bStartFailed != 0
			                          ? 0
			                          : pBackend->m_Ops.stopInstance( pBackend->m_Ops.pUserData, slot.pInstance, SIR_AUDIO_STOP_IMMEDIATE );
			const int32_t nRelease = pBackend->m_Ops.releaseInstance( pBackend->m_Ops.pUserData, slot.pInstance );
			if ( nStop != 0 || nRelease != 0 ) {
				// BackendOps::shutdown is void. Never free ownership records if a
				// native instance cannot be relinquished safely.
				abort();
			}
			slot.pInstance = nullptr;
		}
	}
	pBackend->releaseStorage();
}

bool SIRFMODVoiceBackend::findEvent( uint64_t nEventId, uint32_t &iEvent ) const noexcept
{
	uint32_t iLow = 0;
	uint32_t iHigh = m_nEventCount;
	while ( iLow < iHigh ) {
		const uint32_t iMid = iLow + ( iHigh - iLow ) / 2;
		if ( m_pEvents[iMid].nEventId < nEventId ) {
			iLow = iMid + 1;
		} else {
			iHigh = iMid;
		}
	}
	iEvent = iLow;
	return iLow < m_nEventCount && m_pEvents[iLow].nEventId == nEventId;
}

sirAudioResult_t SIRFMODVoiceBackend::registerEvent( uint64_t nEventId, const char *szPath ) noexcept
{
	if ( !m_bInitialized || !m_pContext->isReady() || m_pContext->getEpoch() != m_nContextEpoch ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nEventId == 0 || szPath == nullptr || szPath[0] == '\0' ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	uint32_t iInsert = 0;
	if ( findEvent( nEventId, iInsert ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_nEventCount >= m_nEventCapacity ) {
		return SIR_AUDIO_CAPACITY;
	}
	void *pDescription = nullptr;
	m_nLastNativeResult = m_Ops.resolveEvent( m_Ops.pUserData, m_pContext->m_pStudio, szPath, &pDescription );
	if ( m_nLastNativeResult != 0 || pDescription == nullptr ) {
		return SIR_AUDIO_BACKEND_ERROR;
	}
	if ( iInsert < m_nEventCount ) {
		memmove( m_pEvents + iInsert + 1, m_pEvents + iInsert, ( size_t )( m_nEventCount - iInsert ) * sizeof( sirFMODRegisteredEvent_t ) );
	}
	m_pEvents[iInsert] = { nEventId, pDescription };
	++m_nEventCount;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODVoiceBackend::unregisterEvent( uint64_t nEventId ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iEvent = 0;
	if ( !findEvent( nEventId, iEvent ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	for ( uint32_t iVoice = 0; iVoice < m_nVoiceCapacity; ++iVoice ) {
		if ( m_pVoices[iVoice].pInstance != nullptr && m_pVoices[iVoice].nEventId == nEventId ) {
			return SIR_AUDIO_BUSY;
		}
	}
	if ( iEvent + 1 < m_nEventCount ) {
		memmove( m_pEvents + iEvent, m_pEvents + iEvent + 1,
		         ( size_t )( m_nEventCount - iEvent - 1 ) * sizeof( sirFMODRegisteredEvent_t ) );
	}
	--m_nEventCount;
	return SIR_AUDIO_OK;
}

bool SIRFMODVoiceBackend::writeCompletion( const sirAudioCompletion_t &completion ) noexcept
{
	if ( m_nCompletions >= m_nCompletionCapacity ) {
		return false;
	}
	m_pCompletions[m_iCompletionWrite] = completion;
	m_iCompletionWrite = ( m_iCompletionWrite + 1 ) % m_nCompletionCapacity;
	++m_nCompletions;
	return true;
}

sirAudioResult_t SIRFMODVoiceBackend::submitOne( const sirAudioCommand_t &command ) noexcept
{
	if ( command.eType == SIR_AUDIO_COMMAND_PLAY ) {
		if ( command.hPlaying.nSlot >= m_nVoiceCapacity || command.hPlaying.nGeneration == 0 || command.hPlaying.nEpoch == 0 ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		sirFMODVoiceSlot_t &slot = m_pVoices[command.hPlaying.nSlot];
		if ( slot.pInstance != nullptr ) {
			return SIR_AUDIO_CAPACITY;
		}
		uint32_t iEvent = 0;
		if ( !findEvent( command.nEventId, iEvent ) ) {
			return SIR_AUDIO_INVALID_ARGUMENT;
		}
		// Reserve both the start and terminal records before creating a voice.
		// Later commands can never consume a live instance's terminal credit.
		if ( ( uint64_t )m_nCompletions + m_nReservedCompletions + 2 > m_nCompletionCapacity ) {
			return SIR_AUDIO_CAPACITY;
		}
		void *pInstance = nullptr;
		m_nLastNativeResult = m_Ops.createInstance( m_Ops.pUserData, m_pEvents[iEvent].pDescription, &pInstance );
		if ( m_nLastNativeResult != 0 || pInstance == nullptr ) {
			if ( pInstance != nullptr ) {
				// On a failed create with a nonnull native instance, retain ownership
				// until release has succeeded. Do not discard the slot on failure.
				slot.pInstance = pInstance;
				slot.bStartFailed = 1;
			}
			return SIR_AUDIO_BACKEND_ERROR;
		}
		slot.pInstance = pInstance;
		slot.hPlaying = command.hPlaying;
		slot.nEventId = command.nEventId;
		slot.nStartSequence = command.nSequence;
		slot.nStopSequence = 0;
		slot.bStopRequested = 0;
		slot.bStartFailed = 0;
		m_nLastNativeResult = m_Ops.startInstance( m_Ops.pUserData, pInstance );
		if ( m_nLastNativeResult != 0 ) {
			slot.bStartFailed = 1;
			const int32_t nRelease = m_Ops.releaseInstance( m_Ops.pUserData, pInstance );
			if ( nRelease == 0 ) {
				slot = {};
			} else {
				m_nLastNativeResult = nRelease;
			}
			return SIR_AUDIO_BACKEND_ERROR;
		}
		++m_nReservedCompletions;
		sirAudioCompletion_t completion = {};
		completion.hPlaying = command.hPlaying;
		completion.nSequence = command.nSequence;
		completion.eResult = SIR_AUDIO_OK;
		completion.eType = SIR_AUDIO_COMPLETION_STARTED;
		if ( !writeCompletion( completion ) ) {
			abort();
		}
		return SIR_AUDIO_OK;
	}
	if ( command.eType == SIR_AUDIO_COMMAND_STOP || command.eType == SIR_AUDIO_COMMAND_SET_PARAMETER ) {
		if ( command.hPlaying.nSlot >= m_nVoiceCapacity ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		sirFMODVoiceSlot_t &slot = m_pVoices[command.hPlaying.nSlot];
		if ( slot.pInstance == nullptr || slot.bStartFailed != 0 || slot.hPlaying.nGeneration != command.hPlaying.nGeneration ||
		     slot.hPlaying.nEpoch != command.hPlaying.nEpoch ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		if ( command.eType == SIR_AUDIO_COMMAND_SET_PARAMETER ) {
			return SIR_AUDIO_UNSUPPORTED;
		}
		if ( slot.bStopRequested != 0 ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		m_nLastNativeResult = m_Ops.stopInstance( m_Ops.pUserData, slot.pInstance, command.eStopMode );
		if ( m_nLastNativeResult != 0 ) {
			return SIR_AUDIO_BACKEND_ERROR;
		}
		slot.nStopSequence = command.nSequence;
		slot.bStopRequested = 1;
		return SIR_AUDIO_PENDING;
	}
	return SIR_AUDIO_UNSUPPORTED;
}

sirAudioResult_t SIRFMODVoiceBackend::submitCallback( void *pContext, const sirAudioCommand_t *pCommands, uint32_t nCount ) noexcept
{
	SIRFMODVoiceBackend *pBackend = ( SIRFMODVoiceBackend * )pContext;
	if ( pBackend == nullptr || !pBackend->m_bInitialized || pBackend->m_pContext == nullptr || !pBackend->m_pContext->isReady() ||
	     pBackend->m_pContext->getEpoch() != pBackend->m_nContextEpoch || ( nCount != 0 && pCommands == nullptr ) ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	// The portable engine currently submits exactly one command at a time.
	// Refuse batches to preserve a single result per command, with no partial success.
	if ( nCount != 1 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	return pBackend->submitOne( pCommands[0] );
}

uint32_t SIRFMODVoiceBackend::drainCallback( void *pContext, sirAudioCompletion_t *pOutput, uint32_t nCapacity ) noexcept
{
	SIRFMODVoiceBackend *pBackend = ( SIRFMODVoiceBackend * )pContext;
	if ( pBackend == nullptr || !pBackend->m_bInitialized || ( nCapacity != 0 && pOutput == nullptr ) ) {
		return 0;
	}
	uint32_t nOut = 0;
	while ( nOut < nCapacity && pBackend->m_nCompletions != 0 ) {
		pOutput[nOut] = pBackend->m_pCompletions[pBackend->m_iCompletionRead];
		pBackend->m_iCompletionRead = ( pBackend->m_iCompletionRead + 1 ) % pBackend->m_nCompletionCapacity;
		--pBackend->m_nCompletions;
		++nOut;
	}
	return nOut;
}

void SIRFMODVoiceBackend::pollVoices( void ) noexcept
{
	for ( uint32_t iVoice = 0; iVoice < m_nVoiceCapacity; ++iVoice ) {
		sirFMODVoiceSlot_t &slot = m_pVoices[iVoice];
		if ( slot.pInstance == nullptr ) {
			continue;
		}
		if ( slot.bStartFailed != 0 ) {
			m_nLastNativeResult = m_Ops.releaseInstance( m_Ops.pUserData, slot.pInstance );
			if ( m_nLastNativeResult == 0 ) {
				slot = {};
			}
			continue;
		}
		uint8_t bStopped = 0;
		m_nLastNativeResult = m_Ops.isStopped( m_Ops.pUserData, slot.pInstance, &bStopped );
		if ( m_nLastNativeResult != 0 || bStopped == 0 ) {
			continue;
		}
		m_nLastNativeResult = m_Ops.releaseInstance( m_Ops.pUserData, slot.pInstance );
		if ( m_nLastNativeResult != 0 ) {
			continue;
		}
		sirAudioCompletion_t completion = {};
		completion.hPlaying = slot.hPlaying;
		completion.nSequence = slot.bStopRequested != 0 ? slot.nStopSequence : slot.nStartSequence;
		completion.eResult = SIR_AUDIO_OK;
		completion.eType = slot.bStopRequested != 0 ? SIR_AUDIO_COMPLETION_STOPPED : SIR_AUDIO_COMPLETION_FINISHED;
		--m_nReservedCompletions;
		if ( !writeCompletion( completion ) ) {
			abort();
		}
		slot = {};
	}
}

void SIRFMODVoiceBackend::updateCallback( void *pContext ) noexcept
{
	SIRFMODVoiceBackend *pBackend = ( SIRFMODVoiceBackend * )pContext;
	if ( pBackend == nullptr || !pBackend->m_bInitialized || !pBackend->m_pContext->isReady() ||
	     pBackend->m_pContext->getEpoch() != pBackend->m_nContextEpoch ) {
		return;
	}
	const sirFMODContextResult_t eResult = pBackend->m_pContext->update();
	if ( eResult != SIR_FMOD_OK ) {
		pBackend->m_nLastNativeResult = pBackend->m_pContext->getLastNativeResult();
		return;
	}
	pBackend->pollVoices();
}

uint64_t SIRFMODVoiceBackend::capabilitiesCallback( const void * ) noexcept
{
	return 0;
}

sirAudioResult_t SIRFMODVoiceBackend::submitSpatialCallback( void *, const sirAudioCommand_t *, const sirAudioAttributes_t * ) noexcept
{
	return SIR_AUDIO_UNSUPPORTED;
}

uint32_t SIRFMODVoiceBackend::getActiveVoiceCount( void ) const noexcept
{
	uint32_t nCount = 0;
	for ( uint32_t iVoice = 0; iVoice < m_nVoiceCapacity; ++iVoice ) {
		if ( m_pVoices[iVoice].pInstance != nullptr ) {
			++nCount;
		}
	}
	return nCount;
}

int32_t SIRFMODVoiceBackend::getLastNativeResult( void ) const noexcept
{
	return m_nLastNativeResult;
}
