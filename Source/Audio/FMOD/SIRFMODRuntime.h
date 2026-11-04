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

#ifndef __SIRENGINE_SIRFMODRUNTIME_H__
#define __SIRENGINE_SIRFMODRUNTIME_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/FMOD/SIRFMODBankStore.h>
#include <Audio/FMOD/SIRFMODMixerBackend.h>
#include <Audio/FMOD/SIRFMODMemoryBudget.h>
#include <Audio/FMOD/SIRFMODPackageLoader.h>
#include <Audio/FMOD/SIRFMODSnapshotBackend.h>
#include <Audio/FMOD/SIRFMODVoiceBackend.h>
#include <Audio/SIRAudioSystem.h>

// Paths are temporary borrowings, valid until the synchronous register call
// returns. Stable provider keys come from the immutable cooked manifest.
typedef struct sirFMODRuntimeEventResolver_s
{
	void *pUserData;
	sirAudioResult_t ( *resolve )( void *pUserData, uint64_t nEventId, uint64_t nProviderKey, const char **pszEventPath ) noexcept;
} sirFMODRuntimeEventResolver_t;

typedef enum sirFMODRuntimeObjectKind_e : uint8_t
{
	SIR_FMOD_RUNTIME_BUS = 1,
	SIR_FMOD_RUNTIME_VCA = 2,
	SIR_FMOD_RUNTIME_SNAPSHOT = 3
} sirFMODRuntimeObjectKind_t;

// The editor/cooker provides these immutable records beside the existing v1
// audio manifest. The package ID must name the bank supplying this object.
typedef struct sirFMODRuntimeObject_s
{
	uint64_t nObjectId;
	uint64_t nPackageId;
	uint64_t nProviderKey;
	sirFMODRuntimeObjectKind_t eKind;
	uint8_t nReserved[7];
} sirFMODRuntimeObject_t;

typedef struct sirFMODRuntimeObjectResolver_s
{
	void *pUserData;
	sirAudioResult_t ( *resolve )( void *pUserData, sirFMODRuntimeObjectKind_t eKind, uint64_t nObjectId, uint64_t nProviderKey,
	                               const char **pszPath ) noexcept;
} sirFMODRuntimeObjectResolver_t;

typedef struct sirFMODRuntimeNativeOps_s
{
	sirFMODNativeOps_t context;
	sirFMODVoiceNativeOps_t voice;
	sirFMODBankNativeOps_t bank;
	sirFMODMixerNativeOps_t mixer;
	sirFMODSnapshotNativeOps_t snapshot;
} sirFMODRuntimeNativeOps_t;

typedef struct sirFMODRuntimeConfig_s
{
	sirFMODContextConfig_t context;
	sirAudioSystemConfig_t audio;
	const sirContainerAllocator_t *pAllocator;
	const SIRAudioResourceStore *pManifest;
	sirFMODPackagePathResolver_t packageResolver;
	sirFMODRuntimeEventResolver_t eventResolver;
	sirFMODRuntimeObjectResolver_t objectResolver;
	const sirFMODRuntimeObject_t *pObjects;
	uint32_t nObjectCount;
	uint32_t nMaxRegisteredEvents;
	uint32_t nMaxRegisteredParameters;
	uint32_t nMaxEmitterParameters;
	uint32_t nMaxBanks;
	uint32_t nMaxBankEventBindings;
	uint32_t nMaxMixerControls;
	uint32_t nMaxSnapshots;
	uint64_t nEngineMemoryBudgetBytes;
} sirFMODRuntimeConfig_t;

// One audio owner. Quiesce producers before shutdown. Native FMOD objects,
// manifest pointers and temporary path strings never enter portable queues.
// A failed bank retirement retains its registered owner for explicit retry.
class SIRFMODRuntime
{
private:
	SIRFMODMemoryBudget m_Memory;
	SIRFMODContext m_Context;
	SIRFMODVoiceBackend m_Voices;
	SIRAudioSystem m_Audio;
	SIRFMODBankStore m_Banks;
	SIRFMODMixerBackend m_Mixer;
	SIRFMODSnapshotBackend m_Snapshots;
	SIRFMODPackageLoader m_Packages;
	const SIRAudioResourceStore *m_pManifest = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirFMODRuntimeEventResolver_t m_EventResolver = {};
	sirFMODRuntimeObjectResolver_t m_ObjectResolver = {};
	const sirFMODRuntimeObject_t *m_pObjects = nullptr;
	uint8_t *m_pEventStates = nullptr;
	uint8_t *m_pObjectStates = nullptr;
	uint64_t m_nManifestEpoch = 0;
	uint32_t m_nEventCount = 0;
	uint32_t m_nObjectCount = 0;
	int32_t m_nLastNativeResult = 0;
	bool m_bInitialized = false;

	sirAudioResult_t registerNativeParameters( void ) noexcept;
	sirAudioResult_t bindPackageResources( void ) noexcept;
	void rollbackNewBindings( void ) noexcept;
	void commitNewBindings( void ) noexcept;
	void synchronizeReleasedBindings( void ) noexcept;
	void releaseBindingStorage( void ) noexcept;

public:
	SIRFMODRuntime( void ) noexcept = default;
	~SIRFMODRuntime( void ) noexcept;
	SIRFMODRuntime( const SIRFMODRuntime & ) = delete;
	SIRFMODRuntime &operator=( const SIRFMODRuntime & ) = delete;

	sirAudioResult_t initialize( const sirFMODRuntimeConfig_t &config, const sirFMODRuntimeNativeOps_t &ops ) noexcept;
	sirAudioResult_t shutdown( void ) noexcept;
	sirAudioResult_t loadPackage( uint64_t nPackageId, bool bPreloadSamples ) noexcept;
	sirAudioResult_t releasePackage( uint64_t nPackageId ) noexcept;
	sirAudioResult_t collectUnused( void ) noexcept;
	uint32_t update( void ) noexcept;
	sirAudioResult_t getPlayingTimeline( sirAudioPlayingHandle_t hPlaying, uint32_t &nMilliseconds ) noexcept;
	bool isReady( void ) const noexcept;
	SIRAudioSystem &getAudio( void ) noexcept;
	SIRFMODContext &getContext( void ) noexcept;
	SIRFMODMixerBackend &getMixer( void ) noexcept;
	SIRFMODSnapshotBackend &getSnapshots( void ) noexcept;
	SIRFMODBankStore &getBanks( void ) noexcept;
	SIRFMODPackageLoader &getPackages( void ) noexcept;
	int32_t getLastNativeResult( void ) const noexcept;
	sirFMODMemoryBudgetStats_t getMemoryStats( void ) const noexcept;
};

// Defined only in SDK-enabled SIRFMODRuntimeNativeApi.cpp.
sirFMODRuntimeNativeOps_t sirGetFMODRuntimeNativeOps( void ) noexcept;

#endif // __SIRENGINE_SIRFMODRUNTIME_H__
