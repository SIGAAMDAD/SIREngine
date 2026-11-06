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

#ifndef __SIRENGINE_SIRFMODVOICEBACKEND_H__
#define __SIRENGINE_SIRFMODVOICEBACKEND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/FMOD/SIRFMODContext.h>
#include <Audio/SIRAudioBackend.h>
#include <Core/Containers/SIRContainerResult.h>

class SIRFMODMixerBackend;
class SIRFMODSnapshotBackend;

// FMOD SDK objects are provider-private. The native operations are injected so
// that the exact same lifetime controller is testable without licensed binaries.
// All operations are single-owner-thread; no native callback touches these slots.
typedef struct sirFMODVoiceNativeOps_s
{
	uint32_t nAbiVersion;
	void *pUserData;
	int32_t ( *resolveEvent )( void *pUserData, void *pStudio, const char *szPath, void **ppDescription ) noexcept;
	int32_t ( *createInstance )( void *pUserData, void *pDescription, void **ppInstance ) noexcept;
	int32_t ( *startInstance )( void *pUserData, void *pInstance ) noexcept;
	int32_t ( *stopInstance )( void *pUserData, void *pInstance, sirAudioStopMode_t eMode ) noexcept;
	int32_t ( *isStopped )( void *pUserData, void *pInstance, uint8_t *pbStopped ) noexcept;
	int32_t ( *releaseInstance )( void *pUserData, void *pInstance ) noexcept;
	int32_t ( *setInstanceParameter )( void *pUserData, void *pInstance, uint64_t nNativeId, float fValue ) noexcept;
	int32_t ( *setGlobalParameter )( void *pUserData, void *pStudio, uint64_t nNativeId, float fValue ) noexcept;
	int32_t ( *setInstanceSpatial )( void *pUserData, void *pInstance, const sirAudioAttributes_t *pAttributes ) noexcept;
	int32_t ( *setListenerSpatial )( void *pUserData, void *pStudio, uint32_t iListener, const sirAudioAttributes_t *pAttributes ) noexcept;
	int32_t ( *setListenerCount )( void *pUserData, void *pStudio, uint32_t nListeners ) noexcept;
	int32_t ( *setListenerWeight )( void *pUserData, void *pStudio, uint32_t iListener, float fWeight ) noexcept;
	int32_t ( *setInstanceVolume )( void *pUserData, void *pInstance, float fVolume ) noexcept;
	int32_t ( *setInstancePitch )( void *pUserData, void *pInstance, float fPitch ) noexcept;
	int32_t ( *setInstancePaused )( void *pUserData, void *pInstance, uint8_t bPaused ) noexcept;
	int32_t ( *setInstanceTimeline )( void *pUserData, void *pInstance, int32_t nMilliseconds ) noexcept;
	int32_t ( *getInstanceTimeline )( void *pUserData, void *pInstance, int32_t *pnMilliseconds ) noexcept;
} sirFMODVoiceNativeOps_t;

typedef struct sirFMODRegisteredEvent_s
{
	uint64_t nEventId;
	void *pDescription;
} sirFMODRegisteredEvent_t;

typedef struct sirFMODParameterBinding_s
{
	uint64_t nControlId;
	uint64_t nNativeId;
	sirAudioControlScope_t eScope;
	uint8_t nReserved[7];
} sirFMODParameterBinding_t;

typedef struct sirFMODEmitterSpatial_s
{
	sirAudioEmitterHandle_t hEmitter;
	sirAudioAttributes_t attributes;
	uint8_t bHasAttributes;
	uint8_t nReserved[7];
} sirFMODEmitterSpatial_t;

typedef struct sirFMODEmitterParameter_s
{
	sirAudioEmitterHandle_t hEmitter;
	uint64_t nControlId;
	float fValue;
	uint8_t bDirty;
	uint8_t nReserved[3];
} sirFMODEmitterParameter_t;

typedef struct sirFMODVoiceSlot_s
{
	sirAudioPlayingHandle_t hPlaying;
	sirAudioEmitterHandle_t hEmitter;
	void *pInstance;
	uint64_t nEventId;
	uint64_t nStartSequence;
	uint64_t nStopSequence;
	uint8_t bStopRequested;
	uint8_t bStartFailed;
	uint8_t bSpatialDirty;
	uint8_t nReserved[5];
} sirFMODVoiceSlot_t;

class SIRFMODVoiceBackend
{
private:
	SIRFMODContext *m_pContext = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirFMODVoiceNativeOps_t m_Ops = {};
	SIRFMODMixerBackend *m_pMixerBackend = nullptr;
	SIRFMODSnapshotBackend *m_pSnapshotBackend = nullptr;
	sirFMODRegisteredEvent_t *m_pEvents = nullptr;
	sirFMODParameterBinding_t *m_pParameters = nullptr;
	sirFMODEmitterSpatial_t *m_pEmitters = nullptr;
	sirFMODEmitterParameter_t *m_pEmitterParameters = nullptr;
	sirFMODVoiceSlot_t *m_pVoices = nullptr;
	sirAudioCompletion_t *m_pCompletions = nullptr;
	uint64_t m_nContextEpoch = 0;
	uint32_t m_nEventCapacity = 0;
	uint32_t m_nEventCount = 0;
	uint32_t m_nParameterCapacity = 0;
	uint32_t m_nParameterCount = 0;
	uint32_t m_nEmitterCapacity = 0;
	uint32_t m_nEmitterParameterCapacity = 0;
	uint32_t m_nEmitterParameterCount = 0;
	uint32_t m_nListenerCapacity = 0;
	uint32_t m_nVoiceCapacity = 0;
	uint32_t m_nCompletionCapacity = 0;
	uint32_t m_iCompletionRead = 0;
	uint32_t m_iCompletionWrite = 0;
	uint32_t m_nCompletions = 0;
	uint32_t m_nReservedCompletions = 0;
	int32_t m_nLastNativeResult = 0;
	bool m_bInitialized = false;

	static sirAudioResult_t initializeCallback( void *pContext, const sirAudioBackendConfig_t *pConfig ) noexcept;
	static void shutdownCallback( void *pContext ) noexcept;
	static sirAudioResult_t submitCallback( void *pContext, const sirAudioCommand_t *pCommands, uint32_t nCount ) noexcept;
	static uint32_t drainCallback( void *pContext, sirAudioCompletion_t *pOutput, uint32_t nCapacity ) noexcept;
	static void updateCallback( void *pContext ) noexcept;
	static uint64_t capabilitiesCallback( const void *pContext ) noexcept;
	static sirAudioResult_t submitSpatialCallback( void *pContext, const sirAudioCommand_t *pCommand,
	                                               const sirAudioAttributes_t *pAttributes ) noexcept;

	bool findEvent( uint64_t nEventId, uint32_t& iEvent ) const noexcept;
	bool findParameter( uint64_t nControlId, uint32_t& iParameter ) const noexcept;
	bool writeCompletion( const sirAudioCompletion_t& completion ) noexcept;
	void pollVoices( void ) noexcept;
	void retrySpatial( void ) noexcept;
	void retryEmitterParameters( void ) noexcept;
	void clearEmitterParameters( sirAudioEmitterHandle_t hEmitter ) noexcept;
	sirAudioResult_t updateEmitterParameter( const sirAudioCommand_t& command ) noexcept;
	sirAudioResult_t applyEmitterParametersToNewInstance( void *pInstance, sirAudioEmitterHandle_t hEmitter ) noexcept;
	void releaseStorage( void ) noexcept;
	sirAudioResult_t submitOne( const sirAudioCommand_t& command ) noexcept;

public:
	SIRFMODVoiceBackend( void ) noexcept = default;
	~SIRFMODVoiceBackend( void ) noexcept;
	SIRFMODVoiceBackend( const SIRFMODVoiceBackend& ) = delete;
	SIRFMODVoiceBackend& operator=( const SIRFMODVoiceBackend& ) = delete;

	sirAudioResult_t configure( SIRFMODContext *pContext, const sirContainerAllocator_t *pAllocator, const sirFMODVoiceNativeOps_t& ops,
	                            uint32_t nMaxRegisteredEvents, uint32_t nMaxRegisteredParameters = 0, uint32_t nMaxEmitters = 0,
	                            uint32_t nMaxListeners = 0, uint32_t nMaxEmitterParameters = 0 ) noexcept;
	// Borrowed controllers must remain alive through SIRAudioSystem::shutdown().
	// Bind them after their initialize() and before this voice backend initializes.
	sirAudioResult_t bindControllers( SIRFMODMixerBackend *pMixer, SIRFMODSnapshotBackend *pSnapshots ) noexcept;
	sirAudioResult_t registerEvent( uint64_t nEventId, const char *szPath ) noexcept;
	sirAudioResult_t unregisterEvent( uint64_t nEventId ) noexcept;
	sirAudioResult_t registerParameter( uint64_t nControlId, uint64_t nNativeId, sirAudioControlScope_t eScope ) noexcept;
	sirAudioResult_t unregisterParameter( uint64_t nControlId ) noexcept;
	bool isInitialized( void ) const noexcept;
	bool isEventInUse( uint64_t nEventId ) const noexcept;
	uint32_t getActiveVoiceCount( void ) const noexcept;
	int32_t getLastNativeResult( void ) const noexcept;
	sirAudioResult_t getPlayingTimeline( sirAudioPlayingHandle_t hPlaying, uint32_t& nMilliseconds ) noexcept;
	static sirAudioBackendOps_t getBackendOps( void ) noexcept;
};

// Implemented by the FMOD SDK-gated provider translation unit only.
sirFMODVoiceNativeOps_t sirGetFMODVoiceNativeOps( void ) noexcept;

#endif // __SIRENGINE_SIRFMODVOICEBACKEND_H__
