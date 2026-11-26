/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_AUDIOSYSTEM_H__
#define __SIR_AUDIOSYSTEM_H__

#include <Core/SIRPCH.h>
#include <atomic>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Diagnostics/SIRPlatformContext.h>
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
	SIRAudioPlaybackState eState;
	SIRAudioPlaybackState eStateBeforeStop;
	uint8_t nReserved[2];
	sirAudioEmitterHandle_t hEmitter;
} sirAudioPlayingSlot_t;

enum class SIRAudioSpatialState : uint8_t {
	SIR_AUDIO_SPATIAL_FREE = 0,
	SIR_AUDIO_SPATIAL_ACTIVE = 1,
	SIR_AUDIO_SPATIAL_RELEASING = 2
};

typedef struct sirAudioSpatialSlot_s
{
	uint32_t nGeneration;
	uint32_t nActiveVoices;
	SIRAudioSpatialState eState;
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
	sirAudioBackendOps_t m_Backend {};
	void *m_pBackendContext = nullptr;
	const SIRAudioResourceStore *m_pResources = nullptr;
	sirAudioLifecycleCallback_t m_LifecycleCallback {};
	uint64_t m_nResourceEpoch = 0;
	SIRAudioQueue<sirAudioPacket_t> m_Commands;
	SIRAudioQueue<sirAudioCommand_t> m_CriticalCommands;
	std::atomic<uint64_t> m_nNextSequence = std::atomic<uint64_t>( 1 );
	uint64_t m_nEpoch = 0;
	uint32_t m_nPlayingCapacity = 0;
	uint32_t m_nEmitterCapacity = 0;
	uint32_t m_nListenerCapacity = 0;
	uint32_t m_nNextSlot = 0;
	uint32_t m_nNextEmitterSlot = 0;
	uint32_t m_nNextListenerSlot = 0;
	sirPlatformContext_t m_Platform {};
	bool m_bInitialized = false;

	bool isValid( sirAudioPlayingHandle_t hPlaying ) const noexcept;
	bool isValidEmitter( sirAudioEmitterHandle_t hEmitter ) const noexcept;
	bool isValidListener( sirAudioListenerHandle_t hListener ) const noexcept;
	void markTerminal( sirAudioPlayingSlot_t& slot, SIRAudioPlaybackState eState ) noexcept;
	void applyReleaseResult( const sirAudioCommand_t& command, SIRAudioResult eResult ) noexcept;
	SIRAudioResult enqueue( sirAudioCommand_t& command, bool bCritical, const sirAudioAttributes_t *pAttributes = nullptr ) noexcept;
	bool applyCompletion( const sirAudioCompletion_t& completion ) noexcept;
	void failSubmission( const sirAudioCommand_t& command ) noexcept;
	void drainCompletions( void ) noexcept;

public:
	SIRAudioSystem( void ) noexcept = default;
	~SIRAudioSystem( void ) noexcept;
	SIRAudioSystem( const SIRAudioSystem& ) = delete;
	SIRAudioSystem& operator=( const SIRAudioSystem& ) = delete;

	SIRAudioResult initialize( const sirAudioSystemConfig_t& config, const sirContainerAllocator_t *pAllocator,
	                             const sirAudioBackendOps_t& backend, void *pBackendContext,
	                             const SIRAudioResourceStore *pResources = nullptr ) noexcept;
	const sirPlatformContext_t& platformContext( void ) const noexcept;
	void shutdown( void ) noexcept;
	void setLifecycleCallback( sirAudioLifecycleCallback_t callback ) noexcept;
	SIRAudioResult playEvent( uint64_t nEventId, sirAudioEmitterHandle_t hEmitter, sirAudioPlayingHandle_t& hPlaying ) noexcept;
	SIRAudioResult stopEvent( sirAudioPlayingHandle_t hPlaying, SIRAudioStopMode eMode ) noexcept;
	SIRAudioResult setParameter( sirAudioPlayingHandle_t hPlaying, uint64_t nParameterId, float fValue ) noexcept;
	SIRAudioResult setPlayingVolume( sirAudioPlayingHandle_t hPlaying, float fVolume ) noexcept;
	SIRAudioResult setPlayingPitch( sirAudioPlayingHandle_t hPlaying, float fPitch ) noexcept;
	SIRAudioResult setPlayingPaused( sirAudioPlayingHandle_t hPlaying, bool bPaused ) noexcept;
	SIRAudioResult seekPlayingTimeline( sirAudioPlayingHandle_t hPlaying, uint32_t nMilliseconds ) noexcept;
	SIRAudioResult registerEmitter( sirAudioEmitterHandle_t& hEmitter ) noexcept;
	SIRAudioResult releaseEmitter( sirAudioEmitterHandle_t hEmitter ) noexcept;
	SIRAudioResult setEmitterAttributes( sirAudioEmitterHandle_t hEmitter, const sirAudioAttributes_t& attributes ) noexcept;
	SIRAudioResult registerListener( sirAudioListenerHandle_t& hListener ) noexcept;
	SIRAudioResult releaseListener( sirAudioListenerHandle_t hListener ) noexcept;
	SIRAudioResult setListenerAttributes( sirAudioListenerHandle_t hListener, const sirAudioAttributes_t& attributes ) noexcept;
	SIRAudioResult setEmitterParameter( sirAudioEmitterHandle_t hEmitter, uint64_t nParameterId, float fValue ) noexcept;
	SIRAudioResult setGlobalParameter( uint64_t nParameterId, float fValue ) noexcept;
	SIRAudioResult setBusVolume( uint64_t nBusId, float fVolume ) noexcept;
	SIRAudioResult setBusMute( uint64_t nBusId, bool bMute ) noexcept;
	SIRAudioResult setVCAVolume( uint64_t nVCAId, float fVolume ) noexcept;
	SIRAudioResult startSnapshot( uint64_t nSnapshotId ) noexcept;
	SIRAudioResult stopSnapshot( uint64_t nSnapshotId, SIRAudioStopMode eMode ) noexcept;
	SIRAudioResult getPlaybackState( sirAudioPlayingHandle_t hPlaying, SIRAudioPlaybackState& eState ) noexcept;
	uint32_t update( void ) noexcept;
};

#endif // __SIR_AUDIOSYSTEM_H__
