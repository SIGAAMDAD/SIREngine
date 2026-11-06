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

#ifndef __SIRENGINE_SIRFMODAUDIOTHREAD_H__
#define __SIRENGINE_SIRFMODAUDIOTHREAD_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/FMOD/SIRFMODRuntime.h>
#include <Audio/SIRAudioQueue.h>
#include <Core/Platform/SIRThread.h>
#include <condition_variable>
#include <mutex>

typedef enum sirFMODThreadOperation_e : uint8_t
{
	SIR_FMOD_THREAD_LOAD_PACKAGE = 1,
	SIR_FMOD_THREAD_RELEASE_PACKAGE = 2,
	SIR_FMOD_THREAD_COLLECT_UNUSED = 3,
	SIR_FMOD_THREAD_RECOVER_OUTPUT = 4,
	SIR_FMOD_THREAD_PLAY_EVENT = 5,
	SIR_FMOD_THREAD_STOP_EVENT = 6,
	SIR_FMOD_THREAD_SET_PARAMETER = 7,
	SIR_FMOD_THREAD_SET_VOLUME = 8,
	SIR_FMOD_THREAD_SET_PITCH = 9,
	SIR_FMOD_THREAD_SET_PAUSED = 10,
	SIR_FMOD_THREAD_SEEK_TIMELINE = 11,
	SIR_FMOD_THREAD_SET_GLOBAL_PARAMETER = 12,
	SIR_FMOD_THREAD_SET_BUS_VOLUME = 13,
	SIR_FMOD_THREAD_SET_BUS_MUTE = 14,
	SIR_FMOD_THREAD_SET_VCA_VOLUME = 15,
	SIR_FMOD_THREAD_START_SNAPSHOT = 16,
	SIR_FMOD_THREAD_STOP_SNAPSHOT = 17,
	SIR_FMOD_THREAD_QUERY_PLAYBACK = 18,
	SIR_FMOD_THREAD_QUERY_SAMPLE = 19
} sirFMODThreadOperation_t;

typedef struct sirFMODThreadRequest_s
{
	sirFMODThreadOperation_t eOperation;
	sirAudioStopMode_t eStopMode;
	uint8_t bFlag;
	uint8_t nReserved[5];
	uint64_t nId;
	uint64_t nArgumentId;
	sirAudioPlayingHandle_t hPlaying;
	sirAudioEmitterHandle_t hEmitter;
	float fValue;
	uint32_t nTimeMilliseconds;
	int32_t iDriver;
} sirFMODThreadRequest_t;

typedef struct sirFMODThreadResult_s
{
	uint64_t nTicket;
	sirAudioPlayingHandle_t hPlaying;
	sirAudioResult_t eResult;
	sirAudioPlaybackState_t ePlaybackState;
	sirFMODSampleState_t eSampleState;
	uint8_t nReserved;
	uint32_t nValue;
} sirFMODThreadResult_t;

typedef struct sirFMODAudioThreadConfig_s
{
	sirFMODRuntimeConfig_t runtime;
	sirFMODRuntimeNativeOps_t nativeOps;
	uint32_t nRequestCapacity;
	uint32_t nIdleSleepMilliseconds;
} sirFMODAudioThreadConfig_t;

typedef struct sirFMODThreadSlot_s
{
	SIRAtomic<uint32_t> nState;
	SIRAtomic<uint64_t> nCancelTicket;
	SIRAtomic<uint64_t> nTicket;
	SIRAtomic<uint8_t> nOperation;
	uint32_t nGeneration;
	sirFMODThreadRequest_t request;
	sirFMODThreadResult_t result;
} sirFMODThreadSlot_t;

// A single OS thread owns every FMOD Studio/Core call, runtime mutation,
// package transaction and logical audio update. Producers never receive
// native FMOD handles. A ticket is retained until its result is polled.
class SIRFMODAudioThread
{
private:
	SIRFMODRuntime m_Runtime;
	SIRThread m_Thread;
	SIRAudioQueue<uint64_t> m_Requests;
	SIRAudioQueue<uint64_t> m_Critical;
	SIRAudioQueue<sirAudioCompletion_t> m_Lifecycle;
	sirFMODThreadSlot_t *m_pSlots = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirFMODAudioThreadConfig_t m_Config = {};
	SIRAtomic<uint32_t> m_nState;
	SIRAtomic<uint32_t> m_nActiveClients;
	SIRAtomic<uint32_t> m_nNextSlot;
	SIRAtomic<uint32_t> m_nWakes;
	SIRAtomic<uint32_t> m_nDroppedLifecycle;
	SIRAtomic<uint32_t> m_nAutoRecoveries;
	SIRAtomic<uint32_t> m_nAutoRecoveryFailures;
	SIRAtomic<uint32_t> m_nWorkerResult;
	SIRAtomic<uint8_t> m_bAccepting;
	SIRAtomic<uint8_t> m_bStop;
	uint32_t m_nCapacity = 0;
	std::mutex m_WaitMutex;
	std::condition_variable m_WaitCondition;

	bool enterClient( void ) noexcept;
	void leaveClient( void ) noexcept;
	void releaseStorage( void ) noexcept;
	void complete( uint64_t nTicket, const sirFMODThreadResult_t& result ) noexcept;
	sirAudioResult_t execute( const sirFMODThreadRequest_t& request, sirFMODThreadResult_t& result ) noexcept;
	void threadMain( void ) noexcept;
	static void threadCallback( void *pContext );
	static void lifecycleCallback( void *pContext, const sirAudioCompletion_t *pCompletion ) noexcept;

public:
	SIRFMODAudioThread( void ) noexcept = default;
	~SIRFMODAudioThread( void ) noexcept;
	SIRFMODAudioThread( const SIRFMODAudioThread& ) = delete;
	SIRFMODAudioThread& operator=( const SIRFMODAudioThread& ) = delete;

	sirAudioResult_t start( const sirFMODAudioThreadConfig_t& config ) noexcept;
	sirAudioResult_t shutdown( void ) noexcept;
	sirAudioResult_t submit( const sirFMODThreadRequest_t& request, uint64_t& nTicket ) noexcept;
	sirAudioResult_t cancel( uint64_t nTicket ) noexcept;
	sirAudioResult_t poll( uint64_t nTicket, sirFMODThreadResult_t& result ) noexcept;
	bool pollLifecycle( sirAudioCompletion_t& completion ) noexcept;
	bool isReady( void ) const noexcept;
	uint32_t getDroppedLifecycleCount( void ) const noexcept;
	uint32_t getAutoRecoveryCount( void ) const noexcept;
	uint32_t getAutoRecoveryFailureCount( void ) const noexcept;
	// Thread-safe memory snapshots; no FMOD calls or native pointers escape.
	sirFMODMemoryBudgetStats_t getEngineMemoryStats( void ) noexcept;
	sirFMODMemoryBudgetStats_t getNativeMemoryStats( void ) noexcept;
};
#endif // __SIRENGINE_SIRFMODAUDIOTHREAD_H__
