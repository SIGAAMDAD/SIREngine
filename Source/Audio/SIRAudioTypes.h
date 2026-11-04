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

#ifndef __SIRENGINE_SIRAUDIOTYPES_H__
#define __SIRENGINE_SIRAUDIOTYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirAudioResult_e : uint8_t
{
	SIR_AUDIO_OK = 0,
	SIR_AUDIO_PENDING = 1,
	SIR_AUDIO_INVALID_HANDLE = 2,
	SIR_AUDIO_CAPACITY = 3,
	SIR_AUDIO_UNSUPPORTED = 4,
	SIR_AUDIO_BACKEND_ERROR = 5,
	SIR_AUDIO_INVALID_ARGUMENT = 6,
	SIR_AUDIO_NOT_INITIALIZED = 7,
	SIR_AUDIO_BUSY = 8
} sirAudioResult_t;

typedef enum sirAudioPlaybackState_e : uint8_t
{
	SIR_AUDIO_PLAYBACK_FREE = 0,
	SIR_AUDIO_PLAYBACK_QUEUED = 1,
	SIR_AUDIO_PLAYBACK_PLAYING = 2,
	SIR_AUDIO_PLAYBACK_STOPPING = 3,
	SIR_AUDIO_PLAYBACK_FINISHED = 4,
	SIR_AUDIO_PLAYBACK_FAILED = 5
} sirAudioPlaybackState_t;

typedef enum sirAudioControlScope_e : uint8_t
{
	SIR_AUDIO_CONTROL_GLOBAL = 0,
	SIR_AUDIO_CONTROL_EMITTER = 1,
	SIR_AUDIO_CONTROL_PLAYING = 2
} sirAudioControlScope_t;

typedef enum sirAudioControlType_e : uint8_t
{
	SIR_AUDIO_CONTROL_FLOAT = 0,
	SIR_AUDIO_CONTROL_BOOLEAN = 1,
	SIR_AUDIO_CONTROL_DISCRETE = 2
} sirAudioControlType_t;

typedef enum sirAudioCommandType_e : uint8_t
{
	SIR_AUDIO_COMMAND_PLAY = 1,
	SIR_AUDIO_COMMAND_STOP = 2,
	SIR_AUDIO_COMMAND_SET_PARAMETER = 3,
	SIR_AUDIO_COMMAND_SET_EMITTER_PARAMETER = 4,
	SIR_AUDIO_COMMAND_SET_GLOBAL_PARAMETER = 5,
	SIR_AUDIO_COMMAND_SET_ATTRIBUTES = 6,
	SIR_AUDIO_COMMAND_SHUTDOWN = 7,
	SIR_AUDIO_COMMAND_RELEASE_EMITTER = 8,
	SIR_AUDIO_COMMAND_RELEASE_LISTENER = 9,
	SIR_AUDIO_COMMAND_SET_LISTENER_ATTRIBUTES = 10,
	SIR_AUDIO_COMMAND_SET_BUS_VOLUME = 11,
	SIR_AUDIO_COMMAND_SET_BUS_MUTE = 12,
	SIR_AUDIO_COMMAND_SET_VCA_VOLUME = 13,
	SIR_AUDIO_COMMAND_START_SNAPSHOT = 14,
	SIR_AUDIO_COMMAND_STOP_SNAPSHOT = 15,
	SIR_AUDIO_COMMAND_SET_PLAYING_VOLUME = 16,
	SIR_AUDIO_COMMAND_SET_PLAYING_PITCH = 17,
	SIR_AUDIO_COMMAND_SET_PLAYING_PAUSED = 18,
	SIR_AUDIO_COMMAND_SEEK_PLAYING_TIMELINE = 19
} sirAudioCommandType_t;

typedef enum sirAudioCompletionType_e : uint8_t
{
	SIR_AUDIO_COMPLETION_STARTED = 1,
	SIR_AUDIO_COMPLETION_FINISHED = 2,
	SIR_AUDIO_COMPLETION_STOPPED = 3,
	SIR_AUDIO_COMPLETION_FAILED = 4
} sirAudioCompletionType_t;

typedef enum sirAudioStopMode_e : uint8_t
{
	SIR_AUDIO_STOP_ALLOW_FADE = 0,
	SIR_AUDIO_STOP_IMMEDIATE = 1
} sirAudioStopMode_t;

typedef struct sirAudioPlayingHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nEpoch;
} sirAudioPlayingHandle_t;

typedef struct sirAudioEmitterHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nEpoch;
} sirAudioEmitterHandle_t;

typedef struct sirAudioListenerHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nEpoch;
} sirAudioListenerHandle_t;

typedef struct sirAudioAttributes_s
{
	float fPosition[3];
	float fVelocity[3];
	float fForward[3];
	float fUp[3];
} sirAudioAttributes_t;

typedef struct sirAudioCommand_s
{
	uint64_t nSequence;
	uint64_t nEventId;
	uint64_t nControlId;
	sirAudioPlayingHandle_t hPlaying;
	sirAudioEmitterHandle_t hEmitter;
	float fValue;
	sirAudioCommandType_t eType;
	sirAudioStopMode_t eStopMode;
	uint8_t nReserved[2];
} sirAudioCommand_t;

typedef struct sirAudioCompletion_s
{
	sirAudioPlayingHandle_t hPlaying;
	uint64_t nSequence;
	sirAudioResult_t eResult;
	sirAudioCompletionType_t eType;
	uint8_t nReserved[6];
} sirAudioCompletion_t;

typedef struct sirAudioBackendConfig_s
{
	uint32_t nMaxVoices;
	uint32_t nCompletionCapacity;
} sirAudioBackendConfig_t;

typedef struct sirAudioSystemConfig_s
{
	uint32_t nMaxPlaying;
	uint32_t nMaxEmitters;
	uint32_t nCommandCapacity;
	uint32_t nCriticalCapacity;
	uint32_t nCompletionCapacity;
	uint32_t nMaxListeners;
} sirAudioSystemConfig_t;

typedef struct sirAudioControlBinding_s
{
	uint64_t nControlId;
	uint64_t nProviderControlId;
	float fDefault;
	float fMinimum;
	float fMaximum;
	sirAudioControlScope_t eScope;
	sirAudioControlType_t eType;
	uint8_t bSupported;
	uint8_t nReserved;
} sirAudioControlBinding_t;

// Wave A wire-message layout: fixed, pointer-free, and stable across the
// supported Windows/Linux 64-bit ABI. Any change requires a version bump.
static_assert( sizeof( sirAudioPlayingHandle_t ) == 16, "Audio handle ABI changed" );
static_assert( sizeof( sirAudioCommand_t ) == 64, "Audio command ABI changed" );
static_assert( sizeof( sirAudioCompletion_t ) == 32, "Audio completion ABI changed" );

#endif // __SIRENGINE_SIRAUDIOTYPES_H__
