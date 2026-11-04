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

#ifndef __SIRENGINE_SIRFMODCONTEXT_H__
#define __SIRENGINE_SIRFMODCONTEXT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/SIRAudioTypes.h>

typedef enum sirFMODContextState_e : uint8_t
{
	SIR_FMOD_CONTEXT_EMPTY = 0,
	SIR_FMOD_CONTEXT_CREATED = 1,
	SIR_FMOD_CONTEXT_READY = 2,
	SIR_FMOD_CONTEXT_RELEASE_FAILED = 3
} sirFMODContextState_t;

typedef enum sirFMODContextResult_e : uint8_t
{
	SIR_FMOD_OK = 0,
	SIR_FMOD_INVALID_ARGUMENT = 1,
	SIR_FMOD_BAD_STATE = 2,
	SIR_FMOD_NATIVE_ERROR = 3,
	SIR_FMOD_VERSION_MISMATCH = 4,
	SIR_FMOD_INVALID_DRIVER = 5,
	SIR_FMOD_RELEASE_ERROR = 6
} sirFMODContextResult_t;

typedef struct sirFMODContextConfig_s
{
	int32_t nMaxChannels;
	int32_t iOutputDriver;
	uint8_t bNoSound;
	uint8_t bSynchronousUpdate;
	uint8_t bRightHanded;
	uint8_t nReserved;
} sirFMODContextConfig_t;

// Private FMOD provider bridge. All opaque native objects belong exclusively
// to the Studio owner. The Core System pointer is borrowed, never released.
// Callback return values are native FMOD_RESULT codes; 0 means FMOD_OK.
typedef struct sirFMODNativeOps_s
{
	uint32_t nAbiVersion;
	uint32_t nSDKVersion;
	void *pUserData;
	int32_t ( *createStudio )( void *pUserData, void **ppStudio ) noexcept;
	int32_t ( *getCore )( void *pUserData, void *pStudio, void **ppCore ) noexcept;
	int32_t ( *getVersion )( void *pUserData, void *pCore, uint32_t *pVersion ) noexcept;
	int32_t ( *getDriverCount )( void *pUserData, void *pCore, int32_t *pCount ) noexcept;
	int32_t ( *setDriver )( void *pUserData, void *pCore, int32_t iDriver ) noexcept;
	int32_t ( *setNoSound )( void *pUserData, void *pCore ) noexcept;
	int32_t ( *initializeStudio )( void *pUserData, void *pStudio, const sirFMODContextConfig_t *pConfig ) noexcept;
	int32_t ( *updateStudio )( void *pUserData, void *pStudio ) noexcept;
	int32_t ( *releaseStudio )( void *pUserData, void *pStudio ) noexcept;
} sirFMODNativeOps_t;

// The owner thread exclusively invokes all methods and quiesces callbacks
// before shutdown. Failure to release retains native ownership for retry.
class SIRFMODContext
{
private:
	friend class SIRFMODVoiceBackend;
	friend class SIRFMODBankStore;
	friend class SIRFMODMixerBackend;
	friend class SIRFMODSnapshotBackend;
	sirFMODNativeOps_t m_Ops = {};
	void *m_pStudio = nullptr;
	void *m_pCoreBorrowed = nullptr;
	uint32_t m_nRuntimeVersion = 0;
	uint64_t m_nEpoch = 0;
	int32_t m_nLastNativeResult = 0;
	uint32_t m_nBorrowedBankObjects = 0;
	sirFMODContextState_t m_eState = SIR_FMOD_CONTEXT_EMPTY;

	sirFMODContextResult_t rollback( sirFMODContextResult_t eOriginalError, int32_t nNativeError ) noexcept;
	bool pinBorrowedBankObject( void ) noexcept;
	void unpinBorrowedBankObject( void ) noexcept;

public:
	SIRFMODContext( void ) noexcept = default;
	~SIRFMODContext( void ) noexcept;
	SIRFMODContext( const SIRFMODContext & ) = delete;
	SIRFMODContext &operator=( const SIRFMODContext & ) = delete;

	sirFMODContextResult_t initialize( const sirFMODContextConfig_t &config, const sirFMODNativeOps_t &ops ) noexcept;
	sirFMODContextResult_t update( void ) noexcept;
	sirFMODContextResult_t shutdown( void ) noexcept;
	sirFMODContextResult_t getDriverCount( int32_t &nCount ) noexcept;
	sirFMODContextResult_t changeDriver( int32_t iDriver ) noexcept;
	bool isReady( void ) const noexcept;
	sirFMODContextState_t getState( void ) const noexcept;
	int32_t getLastNativeResult( void ) const noexcept;
	uint32_t getRuntimeVersion( void ) const noexcept;
	uint64_t getEpoch( void ) const noexcept;
	uint32_t getBorrowedBankObjectCount( void ) const noexcept;
};

// Defined only in the SDK-enabled SIRFMODNativeApi.cpp translation unit.
// Does not require an FMOD SDK header at the call site.
sirFMODNativeOps_t sirGetFMODNativeOps( void ) noexcept;

#endif // __SIRENGINE_SIRFMODCONTEXT_H__
