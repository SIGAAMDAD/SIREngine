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
#include <cmath>
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
                                                 const sirFMODVoiceNativeOps_t &ops, uint32_t nMaxRegisteredEvents,
                                                 uint32_t nMaxRegisteredParameters, uint32_t nMaxEmitters, uint32_t nMaxListeners ) noexcept
{
	if ( m_bInitialized || m_pVoices != nullptr || pContext == nullptr || !pContext->isReady() || pAllocator == nullptr ||
	     pAllocator->pfnAllocate == nullptr || pAllocator->pfnDeallocate == nullptr || nMaxRegisteredEvents == 0 || ops.nAbiVersion != 3 ||
	     ops.resolveEvent == nullptr || ops.createInstance == nullptr || ops.startInstance == nullptr || ops.stopInstance == nullptr ||
	     ops.isStopped == nullptr || ops.releaseInstance == nullptr ||
	     ( nMaxRegisteredParameters != 0 && ( ops.setInstanceParameter == nullptr || ops.setGlobalParameter == nullptr ) ) ||
	     ( nMaxEmitters != 0 && ops.setInstanceSpatial == nullptr ) ||
	     ( nMaxListeners != 0 &&
	       ( ops.setListenerSpatial == nullptr || ops.setListenerCount == nullptr || ops.setListenerWeight == nullptr ) ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	m_pContext = pContext;
	m_pAllocator = pAllocator;
	m_Ops = ops;
	m_nEventCapacity = nMaxRegisteredEvents;
	m_nParameterCapacity = nMaxRegisteredParameters;
	m_nEmitterCapacity = nMaxEmitters;
	m_nListenerCapacity = nMaxListeners;
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
	size_t nParameterBytes = 0;
	size_t nEmitterBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( pConfig->nMaxVoices, sizeof( sirFMODVoiceSlot_t ), &nVoiceBytes ) ||
	     !SIRRawStorage::checkedMultiply( pBackend->m_nEventCapacity, sizeof( sirFMODRegisteredEvent_t ), &nEventBytes ) ||
	     !SIRRawStorage::checkedMultiply( pConfig->nCompletionCapacity, sizeof( sirAudioCompletion_t ), &nCompletionBytes ) ||
	     !SIRRawStorage::checkedMultiply( pBackend->m_nParameterCapacity, sizeof( sirFMODParameterBinding_t ), &nParameterBytes ) ||
	     !SIRRawStorage::checkedMultiply( pBackend->m_nEmitterCapacity, sizeof( sirFMODEmitterSpatial_t ), &nEmitterBytes ) ) {
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
	if ( nParameterBytes != 0 ) {
		pMemory = nullptr;
		if ( SIRRawStorage::tryAllocate( pBackend->m_pAllocator, nParameterBytes, alignof( sirFMODParameterBinding_t ), &pMemory ) !=
		     SIR_CONTAINER_OK ) {
			pBackend->releaseStorage();
			return SIR_AUDIO_CAPACITY;
		}
		pBackend->m_pParameters = ( sirFMODParameterBinding_t * )pMemory;
	}
	pBackend->m_nParameterCount = 0;
	if ( nEmitterBytes != 0 ) {
		pMemory = nullptr;
		if ( SIRRawStorage::tryAllocate( pBackend->m_pAllocator, nEmitterBytes, alignof( sirFMODEmitterSpatial_t ), &pMemory ) !=
		     SIR_CONTAINER_OK ) {
			pBackend->releaseStorage();
			return SIR_AUDIO_CAPACITY;
		}
		pBackend->m_pEmitters = ( sirFMODEmitterSpatial_t * )pMemory;
		memset( pBackend->m_pEmitters, 0, nEmitterBytes );
	}
	if ( pBackend->m_nListenerCapacity != 0 ) {
		pBackend->m_nLastNativeResult =
		    pBackend->m_Ops.setListenerCount( pBackend->m_Ops.pUserData, pBackend->m_pContext->m_pStudio, pBackend->m_nListenerCapacity );
		if ( pBackend->m_nLastNativeResult != 0 ) {
			pBackend->releaseStorage();
			return SIR_AUDIO_BACKEND_ERROR;
		}
		for ( uint32_t iListener = 0; iListener < pBackend->m_nListenerCapacity; ++iListener ) {
			pBackend->m_nLastNativeResult =
			    pBackend->m_Ops.setListenerWeight( pBackend->m_Ops.pUserData, pBackend->m_pContext->m_pStudio, iListener, 0.0f );
			if ( pBackend->m_nLastNativeResult != 0 ) {
				pBackend->releaseStorage();
				return SIR_AUDIO_BACKEND_ERROR;
			}
		}
	}
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
	if ( m_pEmitters != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pEmitters, ( size_t )m_nEmitterCapacity * sizeof( sirFMODEmitterSpatial_t ),
		                           alignof( sirFMODEmitterSpatial_t ) );
	}
	if ( m_pParameters != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pParameters, ( size_t )m_nParameterCapacity * sizeof( sirFMODParameterBinding_t ),
		                           alignof( sirFMODParameterBinding_t ) );
	}
	if ( m_pCompletions != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pCompletions, ( size_t )m_nCompletionCapacity * sizeof( sirAudioCompletion_t ),
		                           alignof( sirAudioCompletion_t ) );
	}
	m_pVoices = nullptr;
	m_pEvents = nullptr;
	m_pParameters = nullptr;
	m_pEmitters = nullptr;
	m_pCompletions = nullptr;
	m_nVoiceCapacity = 0;
	m_nCompletionCapacity = 0;
	m_nEventCount = 0;
	m_nParameterCount = 0;
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

bool SIRFMODVoiceBackend::findParameter( uint64_t nControlId, uint32_t &iParameter ) const noexcept
{
	uint32_t iLow = 0;
	uint32_t iHigh = m_nParameterCount;
	while ( iLow < iHigh ) {
		const uint32_t iMiddle = iLow + ( iHigh - iLow ) / 2;
		if ( m_pParameters[iMiddle].nControlId < nControlId ) {
			iLow = iMiddle + 1;
		} else {
			iHigh = iMiddle;
		}
	}
	iParameter = iLow;
	return iLow < m_nParameterCount && m_pParameters[iLow].nControlId == nControlId;
}

sirAudioResult_t SIRFMODVoiceBackend::registerParameter( uint64_t nControlId, uint64_t nNativeId, sirAudioControlScope_t eScope ) noexcept
{
	if ( !m_bInitialized || !m_pContext->isReady() || m_pContext->getEpoch() != m_nContextEpoch ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nControlId == 0 || nNativeId == 0 || ( eScope != SIR_AUDIO_CONTROL_PLAYING && eScope != SIR_AUDIO_CONTROL_GLOBAL ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	uint32_t iInsert = 0;
	if ( findParameter( nControlId, iInsert ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_nParameterCount >= m_nParameterCapacity ) {
		return SIR_AUDIO_CAPACITY;
	}
	if ( iInsert < m_nParameterCount ) {
		memmove( m_pParameters + iInsert + 1, m_pParameters + iInsert,
		         ( size_t )( m_nParameterCount - iInsert ) * sizeof( sirFMODParameterBinding_t ) );
	}
	m_pParameters[iInsert] = { nControlId, nNativeId, eScope, {} };
	++m_nParameterCount;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODVoiceBackend::unregisterParameter( uint64_t nControlId ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iParameter = 0;
	if ( !findParameter( nControlId, iParameter ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( iParameter + 1 < m_nParameterCount ) {
		memmove( m_pParameters + iParameter, m_pParameters + iParameter + 1,
		         ( size_t )( m_nParameterCount - iParameter - 1 ) * sizeof( sirFMODParameterBinding_t ) );
	}
	--m_nParameterCount;
	return SIR_AUDIO_OK;
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
	if ( command.eType == SIR_AUDIO_COMMAND_RELEASE_EMITTER ) {
		if ( command.hEmitter.nSlot >= m_nEmitterCapacity || command.hEmitter.nGeneration == 0 || command.hEmitter.nEpoch == 0 ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		sirFMODEmitterSpatial_t &emitter = m_pEmitters[command.hEmitter.nSlot];
		if ( emitter.bHasAttributes != 0 && emitter.hEmitter.nEpoch == command.hEmitter.nEpoch &&
		     emitter.hEmitter.nGeneration == command.hEmitter.nGeneration ) {
			emitter = {};
		}
		return SIR_AUDIO_OK;
	}
	if ( command.eType == SIR_AUDIO_COMMAND_RELEASE_LISTENER ) {
		if ( command.hEmitter.nSlot >= m_nListenerCapacity || command.hEmitter.nGeneration == 0 || command.hEmitter.nEpoch == 0 ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		m_nLastNativeResult = m_Ops.setListenerWeight( m_Ops.pUserData, m_pContext->m_pStudio, command.hEmitter.nSlot, 0.0f );
		return m_nLastNativeResult == 0 ? SIR_AUDIO_OK : SIR_AUDIO_BACKEND_ERROR;
	}
	if ( command.eType == SIR_AUDIO_COMMAND_SET_GLOBAL_PARAMETER ) {
		uint32_t iParameter = 0;
		if ( !findParameter( command.nControlId, iParameter ) || m_pParameters[iParameter].eScope != SIR_AUDIO_CONTROL_GLOBAL ||
		     m_Ops.setGlobalParameter == nullptr ) {
			return SIR_AUDIO_UNSUPPORTED;
		}
		m_nLastNativeResult =
		    m_Ops.setGlobalParameter( m_Ops.pUserData, m_pContext->m_pStudio, m_pParameters[iParameter].nNativeId, command.fValue );
		return m_nLastNativeResult == 0 ? SIR_AUDIO_OK : SIR_AUDIO_BACKEND_ERROR;
	}
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
		slot.hEmitter = command.hEmitter;
		slot.bSpatialDirty = 0;
		slot.nEventId = command.nEventId;
		slot.nStartSequence = command.nSequence;
		slot.nStopSequence = 0;
		slot.bStopRequested = 0;
		slot.bStartFailed = 0;
		if ( command.hEmitter.nEpoch != 0 ) {
			if ( command.hEmitter.nSlot >= m_nEmitterCapacity ) {
				slot.bStartFailed = 1;
				const int32_t nRelease = m_Ops.releaseInstance( m_Ops.pUserData, pInstance );
				if ( nRelease == 0 ) {
					slot = {};
				} else {
					m_nLastNativeResult = nRelease;
				}
				return SIR_AUDIO_INVALID_HANDLE;
			}
			const sirFMODEmitterSpatial_t &emitter = m_pEmitters[command.hEmitter.nSlot];
			if ( emitter.bHasAttributes != 0 && emitter.hEmitter.nGeneration == command.hEmitter.nGeneration &&
			     emitter.hEmitter.nEpoch == command.hEmitter.nEpoch ) {
				m_nLastNativeResult = m_Ops.setInstanceSpatial( m_Ops.pUserData, pInstance, &emitter.attributes );
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
			}
		}
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
			uint32_t iParameter = 0;
			if ( !findParameter( command.nControlId, iParameter ) || m_pParameters[iParameter].eScope != SIR_AUDIO_CONTROL_PLAYING ||
			     m_Ops.setInstanceParameter == nullptr ) {
				return SIR_AUDIO_UNSUPPORTED;
			}
			m_nLastNativeResult =
			    m_Ops.setInstanceParameter( m_Ops.pUserData, slot.pInstance, m_pParameters[iParameter].nNativeId, command.fValue );
			return m_nLastNativeResult == 0 ? SIR_AUDIO_OK : SIR_AUDIO_BACKEND_ERROR;
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

void SIRFMODVoiceBackend::retrySpatial( void ) noexcept
{
	for ( uint32_t iVoice = 0; iVoice < m_nVoiceCapacity; ++iVoice ) {
		sirFMODVoiceSlot_t &slot = m_pVoices[iVoice];
		if ( slot.pInstance == nullptr || slot.bStartFailed != 0 || slot.bSpatialDirty == 0 || slot.hEmitter.nSlot >= m_nEmitterCapacity ) {
			continue;
		}
		const sirFMODEmitterSpatial_t &emitter = m_pEmitters[slot.hEmitter.nSlot];
		if ( emitter.bHasAttributes == 0 || emitter.hEmitter.nGeneration != slot.hEmitter.nGeneration ||
		     emitter.hEmitter.nEpoch != slot.hEmitter.nEpoch ) {
			continue;
		}
		m_nLastNativeResult = m_Ops.setInstanceSpatial( m_Ops.pUserData, slot.pInstance, &emitter.attributes );
		if ( m_nLastNativeResult == 0 ) {
			slot.bSpatialDirty = 0;
		}
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
	pBackend->retrySpatial();
	pBackend->pollVoices();
}

uint64_t SIRFMODVoiceBackend::capabilitiesCallback( const void * ) noexcept
{
	return 0;
}

sirAudioResult_t SIRFMODVoiceBackend::submitSpatialCallback( void *pContext, const sirAudioCommand_t *pCommand,
                                                             const sirAudioAttributes_t *pAttributes ) noexcept
{
	SIRFMODVoiceBackend *pBackend = ( SIRFMODVoiceBackend * )pContext;
	if ( pBackend == nullptr || !pBackend->m_bInitialized || pBackend->m_pContext == nullptr || !pBackend->m_pContext->isReady() ||
	     pBackend->m_pContext->getEpoch() != pBackend->m_nContextEpoch ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( pCommand == nullptr || pAttributes == nullptr ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	const float *pVectors[4] = { pAttributes->fPosition, pAttributes->fVelocity, pAttributes->fForward, pAttributes->fUp };
	for ( uint32_t iVector = 0; iVector < 4; ++iVector ) {
		for ( uint32_t iComponent = 0; iComponent < 3; ++iComponent ) {
			if ( !std::isfinite( pVectors[iVector][iComponent] ) ) {
				return SIR_AUDIO_INVALID_ARGUMENT;
			}
		}
	}
	if ( pCommand->hEmitter.nGeneration == 0 || pCommand->hEmitter.nEpoch == 0 ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( pCommand->eType == SIR_AUDIO_COMMAND_SET_LISTENER_ATTRIBUTES ) {
		if ( pCommand->hEmitter.nSlot >= pBackend->m_nListenerCapacity ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		pBackend->m_nLastNativeResult = pBackend->m_Ops.setListenerSpatial( pBackend->m_Ops.pUserData, pBackend->m_pContext->m_pStudio,
		                                                                    pCommand->hEmitter.nSlot, pAttributes );
		if ( pBackend->m_nLastNativeResult != 0 ) {
			return SIR_AUDIO_BACKEND_ERROR;
		}
		pBackend->m_nLastNativeResult =
		    pBackend->m_Ops.setListenerWeight( pBackend->m_Ops.pUserData, pBackend->m_pContext->m_pStudio, pCommand->hEmitter.nSlot, 1.0f );
		return pBackend->m_nLastNativeResult == 0 ? SIR_AUDIO_OK : SIR_AUDIO_BACKEND_ERROR;
	}
	if ( pCommand->eType != SIR_AUDIO_COMMAND_SET_ATTRIBUTES || pCommand->hEmitter.nSlot >= pBackend->m_nEmitterCapacity ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	sirFMODEmitterSpatial_t &emitter = pBackend->m_pEmitters[pCommand->hEmitter.nSlot];
	emitter.hEmitter = pCommand->hEmitter;
	emitter.attributes = *pAttributes;
	emitter.bHasAttributes = 1;
	sirAudioResult_t eResult = SIR_AUDIO_OK;
	for ( uint32_t iVoice = 0; iVoice < pBackend->m_nVoiceCapacity; ++iVoice ) {
		sirFMODVoiceSlot_t &slot = pBackend->m_pVoices[iVoice];
		if ( slot.pInstance == nullptr || slot.bStartFailed != 0 || slot.hEmitter.nSlot != pCommand->hEmitter.nSlot ||
		     slot.hEmitter.nGeneration != pCommand->hEmitter.nGeneration || slot.hEmitter.nEpoch != pCommand->hEmitter.nEpoch ) {
			continue;
		}
		pBackend->m_nLastNativeResult = pBackend->m_Ops.setInstanceSpatial( pBackend->m_Ops.pUserData, slot.pInstance, pAttributes );
		slot.bSpatialDirty = pBackend->m_nLastNativeResult != 0 ? 1 : 0;
		if ( pBackend->m_nLastNativeResult != 0 ) {
			eResult = SIR_AUDIO_BACKEND_ERROR;
		}
	}
	return eResult;
}

bool SIRFMODVoiceBackend::isInitialized( void ) const noexcept
{
	return m_bInitialized;
}

bool SIRFMODVoiceBackend::isEventInUse( uint64_t nEventId ) const noexcept
{
	for ( uint32_t iVoice = 0; iVoice < m_nVoiceCapacity; ++iVoice ) {
		if ( m_pVoices[iVoice].pInstance != nullptr && m_pVoices[iVoice].nEventId == nEventId ) {
			return true;
		}
	}
	return false;
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
