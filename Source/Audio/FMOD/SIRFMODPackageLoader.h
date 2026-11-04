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

#ifndef __SIRENGINE_SIRFMODPACKAGELOADER_H__
#define __SIRENGINE_SIRFMODPACKAGELOADER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/FMOD/SIRFMODBankStore.h>
#include <Audio/SIRAudioResources.h>
#include <Core/Containers/SIRContainerResult.h>

// The callback only supplies a bank path for the duration of the synchronous
// loadBank call. Provider keys are cooked, stable IDs, not file paths.
typedef struct sirFMODPackagePathResolver_s
{
	void *pUserData;
	sirAudioResult_t ( *resolve )( void *pUserData, uint64_t nPackageId, uint64_t nProviderKey, const char **pszBankPath ) noexcept;
} sirFMODPackagePathResolver_t;

typedef struct sirFMODPackageEntry_s
{
	uint64_t nPackageId;
	uint8_t eState;   // 0 = absent, 1 = resident, 2 = failed native load with retained handle.
	uint8_t bVisited; // Transient iterative DFS state.
	uint8_t nReserved[6];
	uint32_t nReferences;     // One reference per distinct root acquisition reaching this package.
	uint32_t nRootReferences; // Explicit owner-held leases on this package itself.
} sirFMODPackageEntry_t;

typedef struct sirFMODPackageFrame_s
{
	uint32_t iPackage;
	uint32_t iNextDependency;
} sirFMODPackageFrame_t;

// One owner-thread orchestrator owns the bank store's complete set of banks.
// Each loadPackage call acquires a logical root lease; common dependencies
// are shared through per-package counts. releasePackage retires unused banks
// in reverse dependency order. Partial native unload failure retains ownership.
class SIRFMODPackageLoader
{
private:
	const SIRAudioResourceStore *m_pManifest = nullptr;
	SIRFMODBankStore *m_pBanks = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirFMODPackagePathResolver_t m_Resolver = {};
	sirFMODPackageEntry_t *m_pEntries = nullptr;
	sirFMODPackageFrame_t *m_pFrames = nullptr;
	uint32_t *m_pPlan = nullptr;
	uint32_t *m_pLoadOrder = nullptr;
	uint64_t m_nManifestEpoch = 0;
	uint32_t m_nPackageCount = 0;
	uint32_t m_nLoadedCount = 0;
	uint64_t m_nLastFailedPackageId = 0;
	uint64_t m_nAsyncRootId = 0;
	uint32_t m_nAsyncPlanCount = 0;
	uint32_t m_nAsyncStep = 0;
	uint32_t m_nAsyncOldCount = 0;
	bool m_bAsyncPreloadSamples = false;
	sirAudioResult_t m_eAsyncTerminalResult = SIR_AUDIO_PENDING;

	void clearAsync( void ) noexcept;
	sirAudioResult_t failAsync( sirAudioResult_t eError ) noexcept;

	bool findIndex( uint64_t nPackageId, uint32_t &iPackage ) const noexcept;
	sirAudioResult_t buildPlan( uint32_t iRoot, uint32_t &nPlanCount ) noexcept;
	sirAudioResult_t rollbackTo( uint32_t nOldCount ) noexcept;
	void releaseStorage( void ) noexcept;

public:
	SIRFMODPackageLoader( void ) noexcept = default;
	~SIRFMODPackageLoader( void ) noexcept;
	SIRFMODPackageLoader( const SIRFMODPackageLoader & ) = delete;
	SIRFMODPackageLoader &operator=( const SIRFMODPackageLoader & ) = delete;

	sirAudioResult_t initialize( const SIRAudioResourceStore *pManifest, SIRFMODBankStore *pBanks,
	                             const sirContainerAllocator_t *pAllocator, sirFMODPackagePathResolver_t resolver ) noexcept;
	sirAudioResult_t loadPackage( uint64_t nPackageId, bool bPreloadSamples ) noexcept;
	sirAudioResult_t beginLoadPackage( uint64_t nPackageId, bool bPreloadSamples ) noexcept;
	sirAudioResult_t pollLoadPackage( void ) noexcept;
	sirAudioResult_t cancelLoadPackage( void ) noexcept;
	uint64_t getPendingPackageId( void ) const noexcept;
	sirAudioResult_t releasePackage( uint64_t nPackageId ) noexcept;
	sirAudioResult_t collectUnused( void ) noexcept;
	bool getPackageReferences( uint64_t nPackageId, uint32_t &nReferences, uint32_t &nRootReferences ) const noexcept;
	sirAudioResult_t unloadAll( void ) noexcept;
	sirAudioResult_t shutdown( void ) noexcept;
	bool isReady( void ) const noexcept;
	uint32_t getLoadedCount( void ) const noexcept;
	uint64_t getLastFailedPackageId( void ) const noexcept;
};

#endif // __SIRENGINE_SIRFMODPACKAGELOADER_H__
