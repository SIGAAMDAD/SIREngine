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

#include <Core/Containers/SIRRawStorage.h>
#include <limits.h>

static SIRAtomic<uint64_t> s_nAudioEpoch = SIRAtomic<uint64_t>( 1 );

SIRAudioSystem::~SIRAudioSystem( void ) noexcept
{
	shutdown();
}

sirAudioResult_t SIRAudioSystem::initialize( const sirAudioSystemConfig_t &config, const sirContainerAllocator_t *pAllocator,
                                             const sirAudioBackendOps_t &backend, void *pBackendContext ) noexcept
{
	if ( m_bInitialized || m_pPlaying != nullptr || backend.nAbiVersion != 1 || backend.initialize == nullptr || backend.shutdown == nullptr ||
	     backend.submit == nullptr || backend.drainCompletions == nullptr || backend.update == nullptr || backend.getCapabilities == nullptr ||
	     pBackendContext == nullptr || config.nMaxPlaying == 0 || config.nMaxPlaying > UINT32_MAX / 2 ||
	     config.nCommandCapacity < 2 || config.nCriticalCapacity < 2 || config.nCompletionCapacity < 2 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	size_t nBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( config.nMaxPlaying, sizeof( sirPlayingSlot_t ), &nBytes ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, nBytes, alignof( sirPlayingSlot_t ), &pMemory ) != SIR_CONTAINER_OK ) {
		return SIR_AUDIO_CAPACITY;
	}
	m_pPlaying = ( sirPlayingSlot_t * )pMemory;
	m_pAllocator = pAllocator;
	m_nPlayingCapacity = config.nMaxPlaying;
	m_nNextSlot = 0;
	memset( m_pPlaying, 0, nBytes );
	if ( !m_Commands.initialize( pAllocator, config.nCommandCapacity ) ||
	     !m_CriticalCommands.initialize( pAllocator, config.nCriticalCapacity ) ) {
		shutdown();
		return SIR_AUDIO_CAPACITY;
	}
	m_Backend = backend;
	m_pBackendContext = pBackendContext;
	const sirAudioBackendConfig_t backendConfig = { config.nMaxPlaying, config.nCompletionCapacity };
	const sirAudioResult_t eResult = m_Backend.initialize( m_pBackendContext, &backendConfig );
	if ( eResult != SIR_AUDIO_OK ) {
		shutdown();
		return eResult;
	}
	m_nEpoch = s_nAudioEpoch.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
	m_nNextSequence.store( 1, SIR_MEMORY_ORDER_RELAXED );
	m_bInitialized = true;
	return SIR_AUDIO_OK;
}

void SIRAudioSystem::shutdown( void ) noexcept
{
	// Producers and the update owner are quiesced before teardown.
	if ( m_pBackendContext != nullptr && m_Backend.shutdown != nullptr ) {
		m_Backend.shutdown( m_pBackendContext );
	}
	m_Backend = {};
	m_pBackendContext = nullptr;
	m_bInitialized = false;
	m_nEpoch = 0;
	m_Commands.shutdown();
	m_CriticalCommands.shutdown();
	if ( m_pPlaying != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pPlaying, ( size_t )m_nPlayingCapacity * sizeof( sirPlayingSlot_t ),
	                                  alignof( sirPlayingSlot_t ) );
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

sirAudioResult_t SIRAudioSystem::enqueue( sirAudioCommand_t &command, bool bCritical ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	command.nSequence = m_nNextSequence.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
	return ( bCritical ? m_CriticalCommands.tryPush( command ) : m_Commands.tryPush( command ) ) ? SIR_AUDIO_PENDING : SIR_AUDIO_CAPACITY;
}

sirAudioResult_t SIRAudioSystem::playEvent( uint64_t nEventId, sirAudioEmitterHandle_t hEmitter,
                                            sirAudioPlayingHandle_t &hPlaying ) noexcept
{
	hPlaying = {};
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nEventId == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	for ( uint32_t nAttempt = 0; nAttempt < m_nPlayingCapacity; ++nAttempt ) {
		const uint32_t iSlot = ( m_nNextSlot + nAttempt ) % m_nPlayingCapacity;
		sirPlayingSlot_t &slot = m_pPlaying[iSlot];
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
		slot.nGeneration = candidate.nGeneration;
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
	     m_pPlaying[hPlaying.nSlot].eState == SIR_AUDIO_PLAYBACK_FAILED ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_STOP;
	command.hPlaying = hPlaying;
	command.eStopMode = eMode;
	const sirAudioResult_t eResult = enqueue( command, true );
	if ( eResult == SIR_AUDIO_PENDING ) {
		m_pPlaying[hPlaying.nSlot].eState = SIR_AUDIO_PLAYBACK_STOPPING;
	}
	return eResult;
}

sirAudioResult_t SIRAudioSystem::setParameter( sirAudioPlayingHandle_t hPlaying, uint64_t nParameterId, float fValue ) noexcept
{
	if ( !isValid( hPlaying ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( nParameterId == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_PARAMETER;
	command.hPlaying = hPlaying;
	command.nControlId = nParameterId;
	command.fValue = fValue;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::setEmitterParameter( sirAudioEmitterHandle_t hEmitter, uint64_t nParameterId, float fValue ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nParameterId == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
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
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_SET_GLOBAL_PARAMETER;
	command.nControlId = nParameterId;
	command.fValue = fValue;
	return enqueue( command, false );
}

sirAudioResult_t SIRAudioSystem::getPlaybackState( sirAudioPlayingHandle_t hPlaying, sirAudioPlaybackState_t &eState ) noexcept
{
	if ( !isValid( hPlaying ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	eState = m_pPlaying[hPlaying.nSlot].eState;
	return SIR_AUDIO_OK;
}

void SIRAudioSystem::applyCompletion( const sirAudioCompletion_t &completion ) noexcept
{
	if ( !isValid( completion.hPlaying ) ) {
		return;
	}
	sirPlayingSlot_t &slot = m_pPlaying[completion.hPlaying.nSlot];
	if ( completion.eType == SIR_AUDIO_COMPLETION_STARTED ) {
		if ( slot.eState == SIR_AUDIO_PLAYBACK_QUEUED ) {
			slot.eState = SIR_AUDIO_PLAYBACK_PLAYING;
		}
	} else if ( completion.eType == SIR_AUDIO_COMPLETION_FAILED ) {
		slot.eState = SIR_AUDIO_PLAYBACK_FAILED;
	} else if ( completion.eType == SIR_AUDIO_COMPLETION_FINISHED || completion.eType == SIR_AUDIO_COMPLETION_STOPPED ) {
		slot.eState = SIR_AUDIO_PLAYBACK_FINISHED;
	}
}

uint32_t SIRAudioSystem::update( void ) noexcept
{
	if ( !m_bInitialized ) {
		return 0;
	}
	uint32_t nProcessed = 0;
	sirAudioCommand_t command = {};
	// Start commands are dispatched before critical stop commands in the
	// same update, so play-then-stop does not race native event creation.
	while ( m_Commands.tryPop( command ) ) {
		const sirAudioResult_t eResult = m_Backend.submit( m_pBackendContext, &command, 1 );
		if ( eResult != SIR_AUDIO_OK && eResult != SIR_AUDIO_PENDING ) {
			const sirAudioCompletion_t failure = { command.hPlaying, command.nSequence, eResult, SIR_AUDIO_COMPLETION_FAILED, {} };
			applyCompletion( failure );
		}
		++nProcessed;
	}
	while ( m_CriticalCommands.tryPop( command ) ) {
		const sirAudioResult_t eResult = m_Backend.submit( m_pBackendContext, &command, 1 );
		if ( eResult != SIR_AUDIO_OK && eResult != SIR_AUDIO_PENDING ) {
			const sirAudioCompletion_t failure = { command.hPlaying, command.nSequence, eResult, SIR_AUDIO_COMPLETION_FAILED, {} };
			applyCompletion( failure );
		}
		++nProcessed;
	}
	m_Backend.update( m_pBackendContext );
	sirAudioCompletion_t completions[16] = {};
	uint32_t nCount = 0;
	do {
		nCount = m_Backend.drainCompletions( m_pBackendContext, completions, 16 );
		if ( nCount > 16 ) {
			nCount = 16;
		}
		for ( uint32_t iCompletion = 0; iCompletion < nCount; ++iCompletion ) {
			applyCompletion( completions[iCompletion] );
		}
	} while ( nCount == 16 );
	return nProcessed;
}
