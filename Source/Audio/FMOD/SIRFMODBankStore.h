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

#ifndef __SIRENGINE_SIRFMODBANKSTORE_H__
#define __SIRENGINE_SIRFMODBANKSTORE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/FMOD/SIRFMODContext.h>
#include <Audio/FMOD/SIRFMODVoiceBackend.h>
#include <Core/Containers/SIRContainerResult.h>

typedef enum sirFMODSampleState_e : uint8_t
{
	SIR_FMOD_SAMPLE_UNLOADED = 0,
	SIR_FMOD_SAMPLE_LOADING = 1,
	SIR_FMOD_SAMPLE_READY = 2,
	SIR_FMOD_SAMPLE_ERROR = 3
} sirFMODSampleState_t;

typedef enum sirFMODBankObjectKind_e : uint8_t
{
	SIR_FMOD_BANK_OBJECT_BUS = 1,
	SIR_FMOD_BANK_OBJECT_VCA = 2,
	SIR_FMOD_BANK_OBJECT_SNAPSHOT = 3
} sirFMODBankObjectKind_t;

// FMOD handles and file operations stay on the audio owner thread. The
// native SDK callback verifies event membership in the specific bank, using
// the provided accounting allocator for temporary cold-path scratch storage.
typedef struct sirFMODBankNativeOps_s
{
	uint32_t nAbiVersion;
	void *pUserData;
	int32_t ( *loadFile )( void *pUserData, void *pStudio, const char *szPath, void **ppBank ) noexcept;
	int32_t ( *unload )( void *pUserData, void *pBank ) noexcept;
	int32_t ( *preloadSamples )( void *pUserData, void *pBank ) noexcept;
	int32_t ( *sampleState )( void *pUserData, void *pBank, sirFMODSampleState_t *pState ) noexcept;
	int32_t ( *containsEvent )( void *pUserData, void *pStudio, void *pBank, const char *szPath, const sirContainerAllocator_t *pAllocator,
	                            uint8_t *pbFound ) noexcept;
	int32_t ( *containsObject )( void *pUserData, void *pStudio, void *pBank, sirFMODBankObjectKind_t eKind, const char *szPath,
	                             const sirContainerAllocator_t *pAllocator, uint8_t *pbFound ) noexcept;
	int32_t ( *loadFileAsync )( void *pUserData, void *pStudio, const char *szPath, void **ppBank ) noexcept;
	int32_t ( *getLoadState )( void *pUserData, void *pBank, sirFMODSampleState_t *pState ) noexcept;
} sirFMODBankNativeOps_t;

typedef struct sirFMODBankSlot_s
{
	uint64_t nPackageId;
	void *pBank;
	uint32_t nBorrowedObjects;
	uint8_t bPreloadRequested;
	uint8_t bUsable;
	uint8_t nReserved[2];
} sirFMODBankSlot_t;

typedef struct sirFMODBankEvent_s
{
	uint64_t nPackageId;
	uint64_t nEventId;
} sirFMODBankEvent_t;

// Owns loaded FMOD bank handles. The voice backend owns event-instance
// handles; this store owns bank-to-event registration and sample requests.
// Shutdown the audio system and this store before destroying Studio context.
class SIRFMODBankStore
{
private:
	SIRFMODContext *m_pContext = nullptr;
	SIRFMODVoiceBackend *m_pVoices = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirFMODBankNativeOps_t m_Ops = {};
	sirFMODBankSlot_t *m_pBanks = nullptr;
	sirFMODBankEvent_t *m_pEvents = nullptr;
	uint64_t m_nContextEpoch = 0;
	uint32_t m_nBankCapacity = 0;
	uint32_t m_nEventCapacity = 0;
	uint32_t m_nBankCount = 0;
	uint32_t m_nEventCount = 0;
	int32_t m_nLastNativeResult = 0;
	bool m_bInitialized = false;

	bool findBank( uint64_t nPackageId, uint32_t& iBank ) const noexcept;
	void eraseBindings( uint64_t nPackageId ) noexcept;
	void releaseStorage( void ) noexcept;

public:
	SIRFMODBankStore( void ) noexcept = default;
	~SIRFMODBankStore( void ) noexcept;
	SIRFMODBankStore( const SIRFMODBankStore& ) = delete;
	SIRFMODBankStore& operator=( const SIRFMODBankStore& ) = delete;

	sirAudioResult_t initialize( SIRFMODContext *pContext, SIRFMODVoiceBackend *pVoices, const sirContainerAllocator_t *pAllocator,
	                             const sirFMODBankNativeOps_t& ops, uint32_t nMaxBanks, uint32_t nMaxEventBindings ) noexcept;
	sirAudioResult_t loadBank( uint64_t nPackageId, const char *szBankPath ) noexcept;
	sirAudioResult_t beginLoadBank( uint64_t nPackageId, const char *szBankPath ) noexcept;
	sirAudioResult_t pollBankLoad( uint64_t nPackageId ) noexcept;
	sirAudioResult_t bindEvent( uint64_t nPackageId, uint64_t nEventId, const char *szEventPath ) noexcept;
	sirAudioResult_t unbindEvent( uint64_t nPackageId, uint64_t nEventId ) noexcept;
	sirAudioResult_t preloadSamples( uint64_t nPackageId ) noexcept;
	sirAudioResult_t getSampleState( uint64_t nPackageId, sirFMODSampleState_t& eState ) noexcept;
	sirAudioResult_t unloadBank( uint64_t nPackageId ) noexcept;
	sirAudioResult_t shutdown( void ) noexcept;
	bool isReady( void ) const noexcept;
	uint32_t getLoadedBankCount( void ) const noexcept;
	bool hasLoadedBank( uint64_t nPackageId ) const noexcept;
	bool isBoundTo( const SIRFMODContext *pContext ) const noexcept;
	sirAudioResult_t containsBankObject( uint64_t nPackageId, sirFMODBankObjectKind_t eKind, const char *szPath ) noexcept;
	sirAudioResult_t pinBankObject( uint64_t nPackageId ) noexcept;
	void unpinBankObject( uint64_t nPackageId ) noexcept;
	uint32_t getBankObjectPinCount( uint64_t nPackageId ) const noexcept;
	uint32_t getBoundEventCount( void ) const noexcept;
	int32_t getLastNativeResult( void ) const noexcept;
};

// Supplied by the SDK-enabled provider translation unit only.
sirFMODBankNativeOps_t sirGetFMODBankNativeOps( void ) noexcept;

#endif // __SIRENGINE_SIRFMODBANKSTORE_H__
