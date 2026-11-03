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

#ifndef __SIRENGINE_SIRAUDIOSYSTEM_H__
#define __SIRENGINE_SIRAUDIOSYSTEM_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/SIRAudioBackend.h>
#include <Audio/SIRAudioQueue.h>

// The gameplay owner alone calls play/stop/update/shutdown and reads playback
// state. Concurrent producers may submit global/emitter parameter commands;
// they must quiesce before shutdown. Vendor callbacks only enqueue completions.
class SIRAudioSystem
{
private:
	typedef struct sirPlayingSlot_s
	{
		uint64_t nStartSequence;
		uint64_t nStopSequence;
		uint32_t nGeneration;
		sirAudioPlaybackState_t eState;
		sirAudioPlaybackState_t eStateBeforeStop;
		uint8_t nReserved[2];
	} sirPlayingSlot_t;

	sirPlayingSlot_t *m_pPlaying = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirAudioBackendOps_t m_Backend = {};
	void *m_pBackendContext = nullptr;
	SIRAudioQueue<sirAudioCommand_t> m_Commands;
	SIRAudioQueue<sirAudioCommand_t> m_CriticalCommands;
	SIRAtomic<uint64_t> m_nNextSequence = SIRAtomic<uint64_t>( 1 );
	uint64_t m_nEpoch = 0;
	uint32_t m_nPlayingCapacity = 0;
	uint32_t m_nNextSlot = 0;
	bool m_bInitialized = false;

	bool isValid( sirAudioPlayingHandle_t hPlaying ) const noexcept;
	sirAudioResult_t enqueue( sirAudioCommand_t &command, bool bCritical ) noexcept;
	void applyCompletion( const sirAudioCompletion_t &completion ) noexcept;
	void failSubmission( const sirAudioCommand_t &command ) noexcept;
	void drainCompletions( void ) noexcept;

public:
	SIRAudioSystem( void ) noexcept = default;
	~SIRAudioSystem( void ) noexcept;
	SIRAudioSystem( const SIRAudioSystem & ) = delete;
	SIRAudioSystem &operator=( const SIRAudioSystem & ) = delete;

	sirAudioResult_t initialize( const sirAudioSystemConfig_t &config, const sirContainerAllocator_t *pAllocator,
	                             const sirAudioBackendOps_t &backend, void *pBackendContext ) noexcept;
	void shutdown( void ) noexcept;
	sirAudioResult_t playEvent( uint64_t nEventId, sirAudioEmitterHandle_t hEmitter, sirAudioPlayingHandle_t &hPlaying ) noexcept;
	sirAudioResult_t stopEvent( sirAudioPlayingHandle_t hPlaying, sirAudioStopMode_t eMode ) noexcept;
	sirAudioResult_t setParameter( sirAudioPlayingHandle_t hPlaying, uint64_t nParameterId, float fValue ) noexcept;
	sirAudioResult_t setEmitterParameter( sirAudioEmitterHandle_t hEmitter, uint64_t nParameterId, float fValue ) noexcept;
	sirAudioResult_t setGlobalParameter( uint64_t nParameterId, float fValue ) noexcept;
	sirAudioResult_t getPlaybackState( sirAudioPlayingHandle_t hPlaying, sirAudioPlaybackState_t &eState ) noexcept;
	uint32_t update( void ) noexcept;
};

#endif // __SIRENGINE_SIRAUDIOSYSTEM_H__
