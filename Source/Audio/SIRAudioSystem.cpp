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

#include <Audio/SIRAudioSystem.h>
#include <Audio/SIRAudioResources.h>

#include <Core/Containers/SIRRawStorage.h>
#include <limits.h>
#include <cmath>

static SIRAtomic<uint64_t> s_nAudioEpoch = SIRAtomic<uint64_t>( 1 );

SIRAudioSystem::~SIRAudioSystem( void ) noexcept
{
	shutdown();
}

sirAudioResult_t SIRAudioSystem::initialize( const sirAudioSystemConfig_t& config, const sirContainerAllocator_t *pAllocator,
                                             const sirAudioBackendOps_t& backend, void *pBackendContext,
                                             const SIRAudioResourceStore *pResources ) noexcept
{
	if ( ( pResources != nullptr && !pResources->isLoaded() ) || m_bInitialized || m_pPlaying != nullptr || backend.nAbiVersion != 2 ||
	     backend.submitSpatial == nullptr || backend.initialize == nullptr || backend.shutdown == nullptr || backend.submit == nullptr ||
	     backend.drainCompletions == nullptr || backend.update == nullptr || backend.getCapabilities == nullptr ||
	     pBackendContext == nullptr || config.nMaxPlaying == 0 || config.nMaxPlaying > UINT32_MAX / 2 || config.nCommandCapacity < 2 ||
	     config.nCriticalCapacity < 2 || config.nCompletionCapacity < 2 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	size_t nBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( config.nMaxPlaying, sizeof( sirAudioPlayingSlot_t ), &nBytes ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, nBytes, alignof( sirAudioPlayingSlot_t ), &pMemory ) != SIR_CONTAINER_OK ) {
		return SIR_AUDIO_CAPACITY;
	}
	m_pPlaying = ( sirAudioPlayingSlot_t * )pMemory;
	m_pAllocator = pAllocator;

	m_nPlayingCapacity = config.nMaxPlaying;
	m_nNextSlot = 0;
	memset( m_pPlaying, 0, nBytes );
	if ( config.nMaxEmitters != 0 ) {
		size_t nEmitterBytes = 0;
		void *pEmitters = nullptr;
		if ( !SIRRawStorage::checkedMultiply( config.nMaxEmitters, sizeof( sirAudioSpatialSlot_t ), &nEmitterBytes ) ||
		     SIRRawStorage::tryAllocate( pAllocator, nEmitterBytes, alignof( sirAudioSpatialSlot_t ), &pEmitters ) != SIR_CONTAINER_OK ) {
			shutdown();
			return SIR_AUDIO_CAPACITY;
		}
		m_pEmitters = ( sirAudioSpatialSlot_t * )pEmitters;
		m_nEmitterCapacity = config.nMaxEmitters;
		memset( m_pEmitters, 0, nEmitterBytes );
	}
	if ( config.nMaxListeners != 0 ) {
		size_t nListenerBytes = 0;
		void *pListeners = nullptr;
		if ( !SIRRawStorage::checkedMultiply( config.nMaxListeners, sizeof( sirAudioSpatialSlot_t ), &nListenerBytes ) ||
		     SIRRawStorage::tryAllocate( pAllocator, nListenerBytes, alignof( sirAudioSpatialSlot_t ), &pListeners ) != SIR_CONTAINER_OK ) {
			shutdown();
			return SIR_AUDIO_CAPACITY;
		}
		m_pListeners = ( sirAudioSpatialSlot_t * )pListeners;
		m_nListenerCapacity = config.nMaxListeners;
		memset( m_pListeners, 0, nListenerBytes );
	}
	if ( !m_Commands.initialize( pAllocator, config.nCommandCapacity ) ||
	     !m_CriticalCommands.initialize( pAllocator, config.nCriticalCapacity ) ) {
		shutdown();
		return SIR_AUDIO_CAPACITY;
	}
	if ( !sirPlatformCaptureContext( &m_Platform ) ) {
		shutdown();
		return SIR_AUDIO_BACKEND_ERROR;
	}
	m_Backend = backend;
	m_pBackendContext = pBackendContext;
	const sirAudioBackendConfig_t backendConfig = { config.nMaxPlaying, config.nCompletionCapacity };
	const sirAudioResult_t eResult = m_Backend.initialize( m_pBackendContext, &backendConfig );
	if ( eResult != SIR_AUDIO_OK ) {
		shutdown();
		return eResult;
	}
	m_pResources = pResources;
	m_nResourceEpoch = pResources == nullptr ? 0 : pResources->getEpoch();
	m_nEpoch = s_nAudioEpoch.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
	m_nNextSequence.store( 1, SIR_MEMORY_ORDER_RELAXED );
	m_bInitialized = true;
	return SIR_AUDIO_OK;
}

const sirPlatformContext_t& SIRAudioSystem::platformContext( void ) const noexcept
{
	return m_Platform;
}

void SIRAudioSystem::setLifecycleCallback( sirAudioLifecycleCallback_t callback ) noexcept
{
	// Only the update owner may replace the callback; producers continue
	// to submit commands without retaining pointers to callback state.
	m_LifecycleCallback = callback;
}

void SIRAudioSystem::shutdown( void ) noexcept
{
	// All producers and the update owner must already be quiesced.
	if ( m_pBackendContext != nullptr && m_Backend.shutdown != nullptr ) {
		m_Backend.shutdown( m_pBackendContext );
	}
	m_Backend = {};
	m_pBackendContext = nullptr;
	m_pResources = nullptr;
	m_LifecycleCallback = {};
	m_nResourceEpoch = 0;
	m_bInitialized = false;
	m_nEpoch = 0;
	m_Platform = {};
	m_Commands.shutdown();
	m_CriticalCommands.shutdown();
	if ( m_pEmitters != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pEmitters, ( size_t )m_nEmitterCapacity * sizeof( sirAudioSpatialSlot_t ),
		                           alignof( sirAudioSpatialSlot_t ) );
	}
	if ( m_pListeners != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pListeners, ( size_t )m_nListenerCapacity * sizeof( sirAudioSpatialSlot_t ),
		                           alignof( sirAudioSpatialSlot_t ) );
	}
	m_pEmitters = nullptr;
	m_pListeners = nullptr;
	m_nEmitterCapacity = 0;
	m_nListenerCapacity = 0;
	m_nNextEmitterSlot = 0;
	m_nNextListenerSlot = 0;
	if ( m_pPlaying != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pPlaying, ( size_t )m_nPlayingCapacity * sizeof( sirAudioPlayingSlot_t ),
		                           alignof( sirAudioPlayingSlot_t ) );
	}
	m_pPlaying = nullptr;
	m_pAllocator = nullptr;
	m_nPlayingCapacity = 0;
	m_nNextSlot = 0;
}

bool SIRAudioSystem::isValid( sirAudioPlayingHandle_t hPlaying ) const noexcept
{
	return m_bInitialized && hPlaying.nEpoch == m_nEpoch && hPlaying.nSlot < m_nPlayingCapacity && hPlaying.nGeneration != 0 &&
	       m_pPlaying[hPlaying.nSlot].nGeneration == hPlaying.nGeneration && m_pPlaying[hPlaying.nSlot].eState != SIR_AUDIO_PLAYBACK_FREE;
}

bool SIRAudioSystem::isValidEmitter( sirAudioEmitterHandle_t hEmitter ) const noexcept
{
	return m_bInitialized && hEmitter.nEpoch == m_nEpoch && hEmitter.nSlot < m_nEmitterCapacity && hEmitter.nGeneration != 0 &&
	       m_pEmitters[hEmitter.nSlot].nGeneration == hEmitter.nGeneration &&
	       m_pEmitters[hEmitter.nSlot].eState == SIR_AUDIO_SPATIAL_ACTIVE;
}

bool SIRAudioSystem::isValidListener( sirAudioListenerHandle_t hListener ) const noexcept
{
	return m_bInitialized && hListener.nEpoch == m_nEpoch && hListener.nSlot < m_nListenerCapacity && hListener.nGeneration != 0 &&
	       m_pListeners[hListener.nSlot].nGeneration == hListener.nGeneration &&
	       m_pListeners[hListener.nSlot].eState == SIR_AUDIO_SPATIAL_ACTIVE;
}

void SIRAudioSystem::markTerminal( sirAudioPlayingSlot_t& slot, sirAudioPlaybackState_t eState ) noexcept
{
	if ( slot.eState != SIR_AUDIO_PLAYBACK_FINISHED && slot.eState != SIR_AUDIO_PLAYBACK_FAILED && slot.hEmitter.nEpoch == m_nEpoch &&
	     slot.hEmitter.nSlot < m_nEmitterCapacity ) {
		sirAudioSpatialSlot_t& emitter = m_pEmitters[slot.hEmitter.nSlot];
		if ( emitter.nGeneration == slot.hEmitter.nGeneration && emitter.nActiveVoices != 0 ) {
			--emitter.nActiveVoices;
		}
	}
	slot.eState = eState;
}

void SIRAudioSystem::applyReleaseResult( const sirAudioCommand_t& command, sirAudioResult_t eResult ) noexcept
{
	if ( command.eType != SIR_AUDIO_COMMAND_RELEASE_EMITTER && command.eType != SIR_AUDIO_COMMAND_RELEASE_LISTENER ) {
		return;
	}
	const bool bEmitter = command.eType == SIR_AUDIO_COMMAND_RELEASE_EMITTER;
	sirAudioSpatialSlot_t *pSlots = bEmitter ? m_pEmitters : m_pListeners;
	const uint32_t nCapacity = bEmitter ? m_nEmitterCapacity : m_nListenerCapacity;
	if ( command.hEmitter.nEpoch != m_nEpoch || command.hEmitter.nSlot >= nCapacity ) {
		return;
	}
	sirAudioSpatialSlot_t& slot = pSlots[command.hEmitter.nSlot];
	if ( slot.nGeneration == command.hEmitter.nGeneration && slot.eState == SIR_AUDIO_SPATIAL_RELEASING ) {
		slot.eState = ( eResult == SIR_AUDIO_OK ) ? SIR_AUDIO_SPATIAL_FREE : SIR_AUDIO_SPATIAL_ACTIVE;
	}
}

static bool isValidAudioAttributes( const sirAudioAttributes_t& attributes ) noexcept
{
	const float *pVectors[4] = { attributes.fPosition, attributes.fVelocity, attributes.fForward, attributes.fUp };
	for ( uint32_t iVector = 0; iVector < 4; ++iVector ) {
		for ( uint32_t iComponent = 0; iComponent < 3; ++iComponent ) {
			if ( !std::isfinite( pVectors[iVector][iComponent] ) ) {
				return false;
			}
		}
	}
	return true;
}

sirAudioResult_t SIRAudioSystem::enqueue( sirAudioCommand_t& command, bool bCritical, const sirAudioAttributes_t *pAttributes ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	command.nSequence = m_nNextSequence.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
	if ( bCritical ) {
		return m_CriticalCommands.tryPush( command ) ? SIR_AUDIO_PENDING : SIR_AUDIO_CAPACITY;
	}
	sirAudioPacket_t packet = {};
	packet.command = command;
	if ( pAttributes != nullptr ) {
		packet.attributes = *pAttributes;
	}
	return m_Commands.tryPush( packet ) ? SIR_AUDIO_PENDING : SIR_AUDIO_CAPACITY;
}

sirAudioResult_t SIRAudioSystem::playEvent( uint64_t nEventId, sirAudioEmitterHandle_t hEmitter,
                                            sirAudioPlayingHandle_t& hPlaying ) noexcept
{
	hPlaying = {};
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( hEmitter.nEpoch == 0 ) {
		if ( hEmitter.nSlot != 0 || hEmitter.nGeneration != 0 ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
	} else if ( !isValidEmitter( hEmitter ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( nEventId == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_pResources != nullptr ) {
		if ( m_pResources->getEpoch() != m_nResourceEpoch ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		sirAudioEventRecord_t event = {};
		if ( !m_pResources->findEvent( nEventId, event ) ) {
			return SIR_AUDIO_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t nAttempt = 0; nAttempt < m_nPlayingCapacity; ++nAttempt ) {
		const uint32_t iSlot = ( m_nNextSlot + nAttempt ) % m_nPlayingCapacity;
		sirAudioPlayingSlot_t& slot = m_pPlaying[iSlot];
		if ( slot.eState != SIR_AUDIO_PLAYBACK_FREE && slot.eState != SIR_AUDIO_PLAYBACK_FINISHED &&
		     slot.eState != SIR_AUDIO_PLAYBACK_FAILED ) {
			continue;
		}
		if ( slot.nGeneration == UINT32_MAX ) {
			continue;
		}
		const sirAudioPlayingHandle_t candidate = { iSlot, slot.nGeneration + 1, m_nEpoch };
		sirAudioCommand_t command = {};
		command.eType = SIR_AUDIO_COMMAND_PLAY;
		command.nEventId = nEventId;
		command.hEmitter = hEmitter;
		command.hPlaying = candidate;
		const sirAudioResult_t eResult = enqueue( command, false );
		if ( eResult != SIR_AUDIO_PENDING ) {
			return eResult;
		}
		slot.hEmitter = hEmitter;
		if ( hEmitter.nEpoch != 0 ) {
			++m_pEmitters[hEmitter.nSlot].nActiveVoices;
		}
		slot.nGeneration = candidate.nGeneration;
		slot.nStartSequence = command.nSequence;
		slot.nStopSequence = 0;
		slot.eState = SIR_AUDIO_PLAYBACK_QUEUED;
		m_nNextSlot = iSlot + 1 == m_nPlayingCapacity ? 0 : iSlot + 1;
		hPlaying = candidate;
		return SIR_AUDIO_PENDING;
	}
	return SIR_AUDIO_CAPACITY;
}

sirAudioResult_t SIRAudioSystem::stopEvent( sirAudioPlayingHandle_t hPlaying, sirAudioStopMode_t eMode ) noexcept
{
	if ( !isValid( hPlaying ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FINISHED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FAILED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_STOPPING ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_STOP;
	command.hPlaying = hPlaying;
	command.eStopMode = eMode;
	const sirAudioResult_t eResult = enqueue( command, true );
	if ( eResult == SIR_AUDIO_PENDING ) {
		sirAudioPlayingSlot_t& slot = m_pPlaying[hPlaying.nSlot];
		slot.nStopSequence = command.nSequence;
		slot.eStateBeforeStop = slot.eState;
		slot.eState = SIR_AUDIO_PLAYBACK_STOPPING;
	}
	return eResult;
}

sirAudioResult_t SIRAudioSystem::setParameter( sirAudioPlayingHandle_t hPlaying, uint64_t nParameterId, float fValue ) noexcept
{
	if ( !isValid( hPlaying ) || m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FINISHED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FAILED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_STOPPING ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( nParameterId == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_pResources != nullptr ) {
		if ( m_pResources->getEpoch() != m_nResourceEpoch ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		const sirAudioResult_t eValidation = m_pResources->validateControl( nParameterId, SIR_AUDIO_CONTROL_PLAYING, fValue );
		if ( eValidation != SIR_AUDIO_OK ) {
			return eValidation;
		}
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_PARAMETER;
	command.hPlaying = hPlaying;
	command.nControlId = nParameterId;
	command.fValue = fValue;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::setPlayingVolume( sirAudioPlayingHandle_t hPlaying, float fVolume ) noexcept
{
	if ( !isValid( hPlaying ) || m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FINISHED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FAILED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_STOPPING ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( !std::isfinite( fVolume ) || fVolume < 0.0f || fVolume > 4.0f ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_PLAYING_VOLUME;
	command.hPlaying = hPlaying;
	command.fValue = fVolume;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::setPlayingPitch( sirAudioPlayingHandle_t hPlaying, float fPitch ) noexcept
{
	if ( !isValid( hPlaying ) || m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FINISHED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FAILED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_STOPPING ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( !std::isfinite( fPitch ) || fPitch <= 0.0f || fPitch > 4.0f ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_PLAYING_PITCH;
	command.hPlaying = hPlaying;
	command.fValue = fPitch;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::setPlayingPaused( sirAudioPlayingHandle_t hPlaying, bool bPaused ) noexcept
{
	if ( !isValid( hPlaying ) || m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FINISHED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FAILED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_STOPPING ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_PLAYING_PAUSED;
	command.hPlaying = hPlaying;
	command.fValue = bPaused ? 1.0f : 0.0f;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::seekPlayingTimeline( sirAudioPlayingHandle_t hPlaying, uint32_t nMilliseconds ) noexcept
{
	if ( !isValid( hPlaying ) || m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FINISHED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FAILED ||
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_STOPPING ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( nMilliseconds > 0x7FFFFFFFU ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SEEK_PLAYING_TIMELINE;
	command.hPlaying = hPlaying;
	command.nControlId = nMilliseconds;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::registerEmitter( sirAudioEmitterHandle_t& hEmitter ) noexcept
{
	hEmitter = {};
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	for ( uint32_t nAttempt = 0; nAttempt < m_nEmitterCapacity; ++nAttempt ) {
		const uint32_t iSlot = ( m_nNextEmitterSlot + nAttempt ) % m_nEmitterCapacity;
		sirAudioSpatialSlot_t& slot = m_pEmitters[iSlot];
		if ( slot.eState != SIR_AUDIO_SPATIAL_FREE || slot.nGeneration == UINT32_MAX ) {
			continue;
		}
		++slot.nGeneration;
		slot.nActiveVoices = 0;
		slot.eState = SIR_AUDIO_SPATIAL_ACTIVE;
		hEmitter = { iSlot, slot.nGeneration, m_nEpoch };
		m_nNextEmitterSlot = iSlot + 1 == m_nEmitterCapacity ? 0 : iSlot + 1;
		return SIR_AUDIO_OK;
	}
	return SIR_AUDIO_CAPACITY;
}

sirAudioResult_t SIRAudioSystem::releaseEmitter( sirAudioEmitterHandle_t hEmitter ) noexcept
{
	if ( !isValidEmitter( hEmitter ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	sirAudioSpatialSlot_t& slot = m_pEmitters[hEmitter.nSlot];
	if ( slot.nActiveVoices != 0 ) {
		return SIR_AUDIO_BUSY;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_RELEASE_EMITTER;
	command.hEmitter = hEmitter;
	const sirAudioResult_t eResult = enqueue( command, true );
	if ( eResult == SIR_AUDIO_PENDING ) {
		slot.eState = SIR_AUDIO_SPATIAL_RELEASING;
	}
	return eResult;
}

sirAudioResult_t SIRAudioSystem::setEmitterAttributes( sirAudioEmitterHandle_t hEmitter, const sirAudioAttributes_t& attributes ) noexcept
{
	if ( !isValidEmitter( hEmitter ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( !isValidAudioAttributes( attributes ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_ATTRIBUTES;
	command.hEmitter = hEmitter;
	return enqueue( command, false, &attributes );
}

sirAudioResult_t SIRAudioSystem::registerListener( sirAudioListenerHandle_t& hListener ) noexcept
{
	hListener = {};
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	for ( uint32_t nAttempt = 0; nAttempt < m_nListenerCapacity; ++nAttempt ) {
		const uint32_t iSlot = ( m_nNextListenerSlot + nAttempt ) % m_nListenerCapacity;
		sirAudioSpatialSlot_t& slot = m_pListeners[iSlot];
		if ( slot.eState != SIR_AUDIO_SPATIAL_FREE || slot.nGeneration == UINT32_MAX ) {
			continue;
		}
		++slot.nGeneration;
		slot.eState = SIR_AUDIO_SPATIAL_ACTIVE;
		hListener = { iSlot, slot.nGeneration, m_nEpoch };
		m_nNextListenerSlot = iSlot + 1 == m_nListenerCapacity ? 0 : iSlot + 1;
		return SIR_AUDIO_OK;
	}
	return SIR_AUDIO_CAPACITY;
}

sirAudioResult_t SIRAudioSystem::releaseListener( sirAudioListenerHandle_t hListener ) noexcept
{
	if ( !isValidListener( hListener ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_RELEASE_LISTENER;
	command.hEmitter = { hListener.nSlot, hListener.nGeneration, hListener.nEpoch };
	const sirAudioResult_t eResult = enqueue( command, true );
	if ( eResult == SIR_AUDIO_PENDING ) {
		m_pListeners[hListener.nSlot].eState = SIR_AUDIO_SPATIAL_RELEASING;
	}
	return eResult;
}

sirAudioResult_t SIRAudioSystem::setListenerAttributes( sirAudioListenerHandle_t hListener,
                                                        const sirAudioAttributes_t& attributes ) noexcept
{
	if ( !isValidListener( hListener ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( !isValidAudioAttributes( attributes ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_LISTENER_ATTRIBUTES;
	command.hEmitter = { hListener.nSlot, hListener.nGeneration, hListener.nEpoch };
	return enqueue( command, false, &attributes );
}

sirAudioResult_t SIRAudioSystem::setEmitterParameter( sirAudioEmitterHandle_t hEmitter, uint64_t nParameterId, float fValue ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( !isValidEmitter( hEmitter ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( nParameterId == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_pResources != nullptr ) {
		if ( m_pResources->getEpoch() != m_nResourceEpoch ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		const sirAudioResult_t eValidation = m_pResources->validateControl( nParameterId, SIR_AUDIO_CONTROL_EMITTER, fValue );
		if ( eValidation != SIR_AUDIO_OK ) {
			return eValidation;
		}
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_EMITTER_PARAMETER;
	command.hEmitter = hEmitter;
	command.nControlId = nParameterId;
	command.fValue = fValue;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::setGlobalParameter( uint64_t nParameterId, float fValue ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nParameterId == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_pResources != nullptr ) {
		if ( m_pResources->getEpoch() != m_nResourceEpoch ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		const sirAudioResult_t eValidation = m_pResources->validateControl( nParameterId, SIR_AUDIO_CONTROL_GLOBAL, fValue );
		if ( eValidation != SIR_AUDIO_OK ) {
			return eValidation;
		}
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_GLOBAL_PARAMETER;
	command.nControlId = nParameterId;
	command.fValue = fValue;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::setBusVolume( uint64_t nBusId, float fVolume ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nBusId == 0 || !std::isfinite( fVolume ) || fVolume < 0.0f || fVolume > 1.0f ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_BUS_VOLUME;
	command.nControlId = nBusId;
	command.fValue = fVolume;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::setBusMute( uint64_t nBusId, bool bMute ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nBusId == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_BUS_MUTE;
	command.nControlId = nBusId;
	command.fValue = bMute ? 1.0f : 0.0f;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::setVCAVolume( uint64_t nVCAId, float fVolume ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nVCAId == 0 || !std::isfinite( fVolume ) || fVolume < 0.0f || fVolume > 1.0f ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_VCA_VOLUME;
	command.nControlId = nVCAId;
	command.fValue = fVolume;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::startSnapshot( uint64_t nSnapshotId ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nSnapshotId == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_START_SNAPSHOT;
	command.nEventId = nSnapshotId;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::stopSnapshot( uint64_t nSnapshotId, sirAudioStopMode_t eMode ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nSnapshotId == 0 || ( eMode != SIR_AUDIO_STOP_ALLOW_FADE && eMode != SIR_AUDIO_STOP_IMMEDIATE ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_STOP_SNAPSHOT;
	command.nEventId = nSnapshotId;
	command.eStopMode = eMode;
	return enqueue( command, true );
}

sirAudioResult_t SIRAudioSystem::getPlaybackState( sirAudioPlayingHandle_t hPlaying, sirAudioPlaybackState_t& eState ) noexcept
{
	if ( !isValid( hPlaying ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	eState = m_pPlaying[hPlaying.nSlot].eState;
	return SIR_AUDIO_OK;
}

bool SIRAudioSystem::applyCompletion( const sirAudioCompletion_t& completion ) noexcept
{
	if ( !isValid( completion.hPlaying ) ) {
		return false;
	}
	sirAudioPlayingSlot_t& slot = m_pPlaying[completion.hPlaying.nSlot];
	if ( completion.eType == SIR_AUDIO_COMPLETION_STARTED ) {
		if ( completion.nSequence == slot.nStartSequence ) {
			if ( slot.eState == SIR_AUDIO_PLAYBACK_QUEUED ) {
				slot.eState = SIR_AUDIO_PLAYBACK_PLAYING;
				return true;
			}
			if ( slot.eState == SIR_AUDIO_PLAYBACK_STOPPING && slot.eStateBeforeStop == SIR_AUDIO_PLAYBACK_QUEUED ) {
				slot.eStateBeforeStop = SIR_AUDIO_PLAYBACK_PLAYING;
				return true;
			}
		}
	} else if ( completion.eType == SIR_AUDIO_COMPLETION_FAILED ) {
		if ( completion.nSequence == slot.nStartSequence &&
		     ( slot.eState == SIR_AUDIO_PLAYBACK_QUEUED || slot.eState == SIR_AUDIO_PLAYBACK_STOPPING ) ) {
			markTerminal( slot, SIR_AUDIO_PLAYBACK_FAILED );
			return true;
		}
		if ( completion.nSequence == slot.nStopSequence && slot.eState == SIR_AUDIO_PLAYBACK_STOPPING ) {
			slot.eState = slot.eStateBeforeStop;
			slot.nStopSequence = 0;
			return true;
		}
	} else if ( completion.eType == SIR_AUDIO_COMPLETION_STOPPED ) {
		if ( completion.nSequence == slot.nStopSequence && slot.eState == SIR_AUDIO_PLAYBACK_STOPPING ) {
			markTerminal( slot, SIR_AUDIO_PLAYBACK_FINISHED );
			return true;
		}
	} else if ( completion.eType == SIR_AUDIO_COMPLETION_FINISHED ) {
		if ( completion.nSequence == slot.nStartSequence &&
		     ( slot.eState == SIR_AUDIO_PLAYBACK_PLAYING || slot.eState == SIR_AUDIO_PLAYBACK_STOPPING ) ) {
			markTerminal( slot, SIR_AUDIO_PLAYBACK_FINISHED );
			return true;
		}
	}
	return false;
}

void SIRAudioSystem::failSubmission( const sirAudioCommand_t& command ) noexcept
{
	if ( !isValid( command.hPlaying ) ) {
		return;
	}
	sirAudioPlayingSlot_t& slot = m_pPlaying[command.hPlaying.nSlot];
	if ( command.eType == SIR_AUDIO_COMMAND_PLAY && slot.nStartSequence == command.nSequence ) {
		markTerminal( slot, SIR_AUDIO_PLAYBACK_FAILED );
	} else if ( command.eType == SIR_AUDIO_COMMAND_STOP && slot.eState == SIR_AUDIO_PLAYBACK_STOPPING &&
	            slot.nStopSequence == command.nSequence ) {
		slot.eState = slot.eStateBeforeStop;
		slot.nStopSequence = 0;
	}
}

void SIRAudioSystem::drainCompletions( void ) noexcept
{
	sirAudioCompletion_t completions[16] = {};
	uint32_t nCount = 0;
	do {
		nCount = m_Backend.drainCompletions( m_pBackendContext, completions, 16 );
		if ( nCount > 16 ) {
			nCount = 16;
		}
		for ( uint32_t iCompletion = 0; iCompletion < nCount; ++iCompletion ) {
			const sirAudioCompletion_t& completion = completions[iCompletion];
			if ( applyCompletion( completion ) && m_LifecycleCallback.pfnNotify != nullptr ) {
				m_LifecycleCallback.pfnNotify( m_LifecycleCallback.pUserData, &completion );
			}
		}
	} while ( nCount == 16 );
}

uint32_t SIRAudioSystem::update( void ) noexcept
{
	if ( !m_bInitialized ) {
		return 0;
	}
	uint32_t nProcessed = 0;
	sirAudioPacket_t packet = {};
	sirAudioCommand_t command = {};
	// A finite normal-queue budget prevents continuously producing threads from
	// keeping this update loop alive indefinitely. Play is always dispatched
	// before the critical stop created by the same logical owner.
	for ( uint32_t nIndex = 0; nIndex < m_Commands.getCapacity() && m_Commands.tryPop( packet ); ++nIndex ) {
		command = packet.command;
		const bool bSpatial =
		    command.eType == SIR_AUDIO_COMMAND_SET_ATTRIBUTES || command.eType == SIR_AUDIO_COMMAND_SET_LISTENER_ATTRIBUTES;
		const sirAudioResult_t eResult = bSpatial ? m_Backend.submitSpatial( m_pBackendContext, &command, &packet.attributes )
		                                          : m_Backend.submit( m_pBackendContext, &command, 1 );
		if ( eResult != SIR_AUDIO_OK && eResult != SIR_AUDIO_PENDING ) {
			failSubmission( command );
		}
		// The mock backend and real SDK adapters must never silently drop an
		// accepted completion. Drain between dispatches to bound queue pressure.
		drainCompletions();
		++nProcessed;
	}
	for ( uint32_t nIndex = 0; nIndex < m_CriticalCommands.getCapacity() && m_CriticalCommands.tryPop( command ); ++nIndex ) {
		const sirAudioResult_t eResult = m_Backend.submit( m_pBackendContext, &command, 1 );
		if ( eResult != SIR_AUDIO_OK && eResult != SIR_AUDIO_PENDING ) {
			failSubmission( command );
		}
		applyReleaseResult( command, eResult );
		drainCompletions();
		++nProcessed;
	}
	m_Backend.update( m_pBackendContext );
	drainCompletions();
	return nProcessed;
}
