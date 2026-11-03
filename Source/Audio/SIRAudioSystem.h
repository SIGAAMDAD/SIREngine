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

// All handle mutations, update(), and shutdown() are owner-thread operations.
// Parameter submissions may be made by concurrent producers after init.
// Stop requests are lossless on success and have a reserved critical queue.
typedef struct sirAudioPlayingSlot_s
{
	uint64_t nStartSequence;
	uint64_t nStopSequence;
	uint32_t nGeneration;
	sirAudioPlaybackState_t eState;
	sirAudioPlaybackState_t eStateBeforeStop;
	uint8_t nReserved[2];
	sirAudioEmitterHandle_t hEmitter;
} sirAudioPlayingSlot_t;

typedef enum sirAudioSpatialState_e : uint8_t
{
	SIR_AUDIO_SPATIAL_FREE = 0,
	SIR_AUDIO_SPATIAL_ACTIVE = 1,
	SIR_AUDIO_SPATIAL_RELEASING = 2
} sirAudioSpatialState_t;

typedef struct sirAudioSpatialSlot_s
{
	uint32_t nGeneration;
	uint32_t nActiveVoices;
	sirAudioSpatialState_t eState;
	uint8_t nReserved[3];
} sirAudioSpatialSlot_t;

typedef struct sirAudioPacket_s
{
	sirAudioCommand_t command;
	sirAudioAttributes_t attributes;
} sirAudioPacket_t;

class SIRAudioResourceStore;

class SIRAudioSystem
{
private:
	sirAudioPlayingSlot_t *m_pPlaying = nullptr;
	sirAudioSpatialSlot_t *m_pEmitters = nullptr;
	sirAudioSpatialSlot_t *m_pListeners = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirAudioBackendOps_t m_Backend = {};
	void *m_pBackendContext = nullptr;
	const SIRAudioResourceStore *m_pResources = nullptr;
	uint64_t m_nResourceEpoch = 0;
	SIRAudioQueue<sirAudioPacket_t> m_Commands;
	SIRAudioQueue<sirAudioCommand_t> m_CriticalCommands;
	SIRAtomic<uint64_t> m_nNextSequence = SIRAtomic<uint64_t>( 1 );
	uint64_t m_nEpoch = 0;
	uint32_t m_nPlayingCapacity = 0;
	uint32_t m_nEmitterCapacity = 0;
	uint32_t m_nListenerCapacity = 0;
	uint32_t m_nNextSlot = 0;
	uint32_t m_nNextEmitterSlot = 0;
	uint32_t m_nNextListenerSlot = 0;
	bool m_bInitialized = false;

	bool isValid( sirAudioPlayingHandle_t hPlaying ) const noexcept;
	bool isValidEmitter( sirAudioEmitterHandle_t hEmitter ) const noexcept;
	bool isValidListener( sirAudioListenerHandle_t hListener ) const noexcept;
	void markTerminal( sirAudioPlayingSlot_t &slot, sirAudioPlaybackState_t eState ) noexcept;
	void applyReleaseResult( const sirAudioCommand_t &command, sirAudioResult_t eResult ) noexcept;
	sirAudioResult_t enqueue( sirAudioCommand_t &command, bool bCritical, const sirAudioAttributes_t *pAttributes = nullptr ) noexcept;
	void applyCompletion( const sirAudioCompletion_t &completion ) noexcept;
	void failSubmission( const sirAudioCommand_t &command ) noexcept;
	void drainCompletions( void ) noexcept;

public:
	SIRAudioSystem( void ) noexcept = default;
	~SIRAudioSystem( void ) noexcept;
	SIRAudioSystem( const SIRAudioSystem & ) = delete;
	SIRAudioSystem &operator=( const SIRAudioSystem & ) = delete;

	sirAudioResult_t initialize( const sirAudioSystemConfig_t &config, const sirContainerAllocator_t *pAllocator,
	                             const sirAudioBackendOps_t &backend, void *pBackendContext,
	                             const SIRAudioResourceStore *pResources = nullptr ) noexcept;
	void shutdown( void ) noexcept;
	sirAudioResult_t playEvent( uint64_t nEventId, sirAudioEmitterHandle_t hEmitter, sirAudioPlayingHandle_t &hPlaying ) noexcept;
	sirAudioResult_t stopEvent( sirAudioPlayingHandle_t hPlaying, sirAudioStopMode_t eMode ) noexcept;
	sirAudioResult_t setParameter( sirAudioPlayingHandle_t hPlaying, uint64_t nParameterId, float fValue ) noexcept;
	sirAudioResult_t registerEmitter( sirAudioEmitterHandle_t &hEmitter ) noexcept;
	sirAudioResult_t releaseEmitter( sirAudioEmitterHandle_t hEmitter ) noexcept;
	sirAudioResult_t setEmitterAttributes( sirAudioEmitterHandle_t hEmitter, const sirAudioAttributes_t &attributes ) noexcept;
	sirAudioResult_t registerListener( sirAudioListenerHandle_t &hListener ) noexcept;
	sirAudioResult_t releaseListener( sirAudioListenerHandle_t hListener ) noexcept;
	sirAudioResult_t setListenerAttributes( sirAudioListenerHandle_t hListener, const sirAudioAttributes_t &attributes ) noexcept;
	sirAudioResult_t setEmitterParameter( sirAudioEmitterHandle_t hEmitter, uint64_t nParameterId, float fValue ) noexcept;
	sirAudioResult_t setGlobalParameter( uint64_t nParameterId, float fValue ) noexcept;
	sirAudioResult_t getPlaybackState( sirAudioPlayingHandle_t hPlaying, sirAudioPlaybackState_t &eState ) noexcept;
	uint32_t update( void ) noexcept;
};

#endif // __SIRENGINE_SIRAUDIOSYSTEM_H__
