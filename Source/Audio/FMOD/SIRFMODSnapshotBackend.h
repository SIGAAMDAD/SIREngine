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

#ifndef __SIRENGINE_SIRFMODSNAPSHOTBACKEND_H__
#define __SIRENGINE_SIRFMODSNAPSHOTBACKEND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/FMOD/SIRFMODContext.h>
#include <Core/Containers/SIRContainerResult.h>

// Snapshot event descriptions are borrowed from loaded banks. Each active
// EventInstance is owned here until release succeeds. Call only on the Studio
// owner thread and unregister descriptions before unloading their banks.
typedef struct sirFMODSnapshotNativeOps_s
{
	uint32_t nAbiVersion;
	void *pUserData;
	int32_t ( *resolveSnapshot )( void *pUserData, void *pStudio, const char *szPath, void **ppDescription ) noexcept;
	int32_t ( *createInstance )( void *pUserData, void *pDescription, void **ppInstance ) noexcept;
	int32_t ( *startInstance )( void *pUserData, void *pInstance ) noexcept;
	int32_t ( *stopInstance )( void *pUserData, void *pInstance, sirAudioStopMode_t eMode ) noexcept;
	int32_t ( *releaseInstance )( void *pUserData, void *pInstance ) noexcept;
} sirFMODSnapshotNativeOps_t;

typedef struct sirFMODSnapshotRecord_s
{
	uint64_t nSnapshotId;
	void *pDescription;
	void *pInstance;
	uint8_t bStarted;
	uint8_t nReserved[7];
} sirFMODSnapshotRecord_t;

// Fixed-capacity, sorted-ID snapshot registry. Allocation happens only during
// initialize; start/stop/release never allocate outside the FMOD SDK.
class SIRFMODSnapshotBackend
{
private:
	SIRFMODContext *m_pContext = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirFMODSnapshotNativeOps_t m_Ops = {};
	sirFMODSnapshotRecord_t *m_pSnapshots = nullptr;
	uint64_t m_nContextEpoch = 0;
	uint32_t m_nCapacity = 0;
	uint32_t m_nCount = 0;
	int32_t m_nLastNativeResult = 0;
	bool m_bInitialized = false;

	bool isReady( void ) const noexcept;
	bool findSnapshot( uint64_t nSnapshotId, uint32_t &iIndex ) const noexcept;
	sirAudioResult_t releaseInstance( sirFMODSnapshotRecord_t &record, bool bStop, sirAudioStopMode_t eMode ) noexcept;
	void releaseStorage( void ) noexcept;

public:
	SIRFMODSnapshotBackend( void ) noexcept = default;
	~SIRFMODSnapshotBackend( void ) noexcept;
	SIRFMODSnapshotBackend( const SIRFMODSnapshotBackend & ) = delete;
	SIRFMODSnapshotBackend &operator=( const SIRFMODSnapshotBackend & ) = delete;

	sirAudioResult_t configure( SIRFMODContext *pContext, const sirContainerAllocator_t *pAllocator, const sirFMODSnapshotNativeOps_t &ops,
	                            uint32_t nMaxSnapshots ) noexcept;
	sirAudioResult_t initialize( void ) noexcept;
	sirAudioResult_t shutdown( void ) noexcept;
	sirAudioResult_t registerSnapshot( uint64_t nSnapshotId, const char *szPath ) noexcept;
	sirAudioResult_t unregisterSnapshot( uint64_t nSnapshotId ) noexcept;
	sirAudioResult_t startSnapshot( uint64_t nSnapshotId ) noexcept;
	sirAudioResult_t stopSnapshot( uint64_t nSnapshotId, sirAudioStopMode_t eMode ) noexcept;
	uint32_t getSnapshotCount( void ) const noexcept;
	uint32_t getActiveInstanceCount( void ) const noexcept;
	int32_t getLastNativeResult( void ) const noexcept;
};

sirFMODSnapshotNativeOps_t sirGetFMODSnapshotNativeOps( void ) noexcept;

#endif // __SIRENGINE_SIRFMODSNAPSHOTBACKEND_H__
