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

#ifndef __SIRENGINE_SIRFMODMIXERBACKEND_H__
#define __SIRENGINE_SIRFMODMIXERBACKEND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/FMOD/SIRFMODContext.h>
#include <Core/Containers/SIRContainerResult.h>

// FMOD objects are borrowed from the context's Studio system. The caller
// owns one thread and must unload banks only after removing their bindings.
typedef struct sirFMODMixerNativeOps_s
{
	uint32_t nAbiVersion;
	void *pUserData;
	int32_t ( *resolveBus )( void *pUserData, void *pStudio, const char *szPath, void **ppBus ) noexcept;
	int32_t ( *resolveVCA )( void *pUserData, void *pStudio, const char *szPath, void **ppVCA ) noexcept;
	int32_t ( *setBusVolume )( void *pUserData, void *pBus, float fVolume ) noexcept;
	int32_t ( *setBusMute )( void *pUserData, void *pBus, uint8_t bMute ) noexcept;
	int32_t ( *setVCAVolume )( void *pUserData, void *pVCA, float fVolume ) noexcept;
} sirFMODMixerNativeOps_t;

typedef enum sirFMODMixerControlKind_e : uint8_t
{
	SIR_FMOD_MIXER_BUS = 1,
	SIR_FMOD_MIXER_VCA = 2
} sirFMODMixerControlKind_t;

typedef struct sirFMODMixerControl_s
{
	uint64_t nControlId;
	void *pNative;
	sirFMODMixerControlKind_t eKind;
	uint8_t nReserved[7];
} sirFMODMixerControl_t;

// Fixed-capacity registry. No allocations occur in register, lookup or update.
// Native FMOD calls remain on the same owner thread as SIRFMODContext::update.
class SIRFMODMixerBackend
{
private:
	SIRFMODContext *m_pContext = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirFMODMixerNativeOps_t m_Ops = {};
	sirFMODMixerControl_t *m_pControls = nullptr;
	uint64_t m_nContextEpoch = 0;
	uint32_t m_nCapacity = 0;
	uint32_t m_nCount = 0;
	int32_t m_nLastNativeResult = 0;
	bool m_bInitialized = false;

	bool isReady( void ) const noexcept;
	bool findControl( uint64_t nControlId, uint32_t &iIndex ) const noexcept;
	sirAudioResult_t registerControl( uint64_t nControlId, const char *szPath, sirFMODMixerControlKind_t eKind ) noexcept;
	sirAudioResult_t setVolume( uint64_t nControlId, float fVolume, sirFMODMixerControlKind_t eKind ) noexcept;

public:
	SIRFMODMixerBackend( void ) noexcept = default;
	~SIRFMODMixerBackend( void ) noexcept;
	SIRFMODMixerBackend( const SIRFMODMixerBackend & ) = delete;
	SIRFMODMixerBackend &operator=( const SIRFMODMixerBackend & ) = delete;

	sirAudioResult_t configure( SIRFMODContext *pContext, const sirContainerAllocator_t *pAllocator, const sirFMODMixerNativeOps_t &ops,
	                            uint32_t nMaxControls ) noexcept;
	sirAudioResult_t initialize( void ) noexcept;
	void shutdown( void ) noexcept;
	sirAudioResult_t registerBus( uint64_t nControlId, const char *szPath ) noexcept;
	sirAudioResult_t registerVCA( uint64_t nControlId, const char *szPath ) noexcept;
	sirAudioResult_t unregisterControl( uint64_t nControlId ) noexcept;
	sirAudioResult_t setBusVolume( uint64_t nControlId, float fVolume ) noexcept;
	sirAudioResult_t setBusMute( uint64_t nControlId, bool bMute ) noexcept;
	sirAudioResult_t setVCAVolume( uint64_t nControlId, float fVolume ) noexcept;
	uint32_t getControlCount( void ) const noexcept;
	bool isBoundTo( const SIRFMODContext *pContext ) const noexcept;
	int32_t getLastNativeResult( void ) const noexcept;
};

sirFMODMixerNativeOps_t sirGetFMODMixerNativeOps( void ) noexcept;

#endif // __SIRENGINE_SIRFMODMIXERBACKEND_H__
