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

#include <Audio/SIRAudioMockBackend.h>

SIRAudioMockBackend::SIRAudioMockBackend( const sirContainerAllocator_t *pAllocator ) noexcept : m_pAllocator( pAllocator )
{
}

sirAudioBackendOps_t SIRAudioMockBackend::getOps( void ) noexcept
{
	return { 1, initializeCallback, shutdownCallback, submitCallback, drainCallback, updateCallback, capabilitiesCallback };
}

sirAudioResult_t SIRAudioMockBackend::initializeCallback( void *pContext, const sirAudioBackendConfig_t *pConfig ) noexcept
{
	SIRAudioMockBackend *pBackend = ( SIRAudioMockBackend * )pContext;
	if ( pBackend == nullptr || pConfig == nullptr || pBackend->m_bInitialized ||
	     !pBackend->m_Completions.initialize( pBackend->m_pAllocator, pConfig->nCompletionCapacity ) ) {
		return SIR_AUDIO_CAPACITY;
	}
	pBackend->m_bInitialized = true;
	pBackend->m_nSubmitted = 0;
	return SIR_AUDIO_OK;
}

void SIRAudioMockBackend::shutdownCallback( void *pContext ) noexcept
{
	SIRAudioMockBackend *pBackend = ( SIRAudioMockBackend * )pContext;
	if ( pBackend != nullptr ) {
		pBackend->m_bInitialized = false;
		pBackend->m_bFailNextPlay = false;
		pBackend->m_Completions.shutdown();
	}
}

sirAudioResult_t SIRAudioMockBackend::submitCallback( void *pContext, const sirAudioCommand_t *pCommands, uint32_t nCount ) noexcept
{
	SIRAudioMockBackend *pBackend = ( SIRAudioMockBackend * )pContext;
	if ( pBackend == nullptr || !pBackend->m_bInitialized || ( pCommands == nullptr && nCount != 0 ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	for ( uint32_t iCommand = 0; iCommand < nCount; ++iCommand ) {
		const sirAudioCommand_t &command = pCommands[iCommand];
		++pBackend->m_nSubmitted;
		if ( command.eType != SIR_AUDIO_COMMAND_PLAY && command.eType != SIR_AUDIO_COMMAND_STOP ) {
			continue;
		}
		sirAudioCompletion_t completion = {};
		completion.hPlaying = command.hPlaying;
		completion.nSequence = command.nSequence;
		if ( command.eType == SIR_AUDIO_COMMAND_STOP ) {
			completion.eType = SIR_AUDIO_COMPLETION_STOPPED;
			completion.eResult = SIR_AUDIO_OK;
		} else if ( pBackend->m_bFailNextPlay ) {
			pBackend->m_bFailNextPlay = false;
			completion.eType = SIR_AUDIO_COMPLETION_FAILED;
			completion.eResult = SIR_AUDIO_BACKEND_ERROR;
		} else {
			completion.eType = SIR_AUDIO_COMPLETION_STARTED;
			completion.eResult = SIR_AUDIO_OK;
		}
		if ( !pBackend->m_Completions.tryPush( completion ) ) {
			return SIR_AUDIO_CAPACITY;
		}
	}
	return SIR_AUDIO_OK;
}

uint32_t SIRAudioMockBackend::drainCallback( void *pContext, sirAudioCompletion_t *pOutput, uint32_t nCapacity ) noexcept
{
	SIRAudioMockBackend *pBackend = ( SIRAudioMockBackend * )pContext;
	if ( pBackend == nullptr || pOutput == nullptr ) {
		return 0;
	}
	uint32_t nCount = 0;
	while ( nCount < nCapacity && pBackend->m_Completions.tryPop( pOutput[nCount] ) ) {
		++nCount;
	}
	return nCount;
}

void SIRAudioMockBackend::updateCallback( void * ) noexcept
{
}

uint64_t SIRAudioMockBackend::capabilitiesCallback( const void * ) noexcept
{
	return 0;
}

void SIRAudioMockBackend::failNextPlay( void ) noexcept
{
	m_bFailNextPlay = true;
}

uint64_t SIRAudioMockBackend::getSubmittedCount( void ) const noexcept
{
	return m_nSubmitted;
}
