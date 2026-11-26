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

#ifndef __SIR_FMODMIXERBACKEND_H__
#define __SIR_FMODMIXERBACKEND_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/FMOD/SIRFMODContext.h>
#include <Core/Containers/SIRContainerResult.h>

class SIRFMODBankStore;

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

enum class SIRFMODMixerControlKind : uint8_t {
	SIR_FMOD_MIXER_BUS = 1,
	SIR_FMOD_MIXER_VCA = 2
};

typedef struct sirFMODMixerControl_s
{
	uint64_t nControlId;
	uint64_t nPackageId;
	void *pNative;
	SIRFMODBankStore *pBankStore;
	SIRFMODMixerControlKind eKind;
	uint8_t nReserved[7];
} sirFMODMixerControl_t;

// Fixed-capacity registry. No allocations occur in register, lookup or update.
// Native FMOD calls remain on the same owner thread as SIRFMODContext::update.
class SIRFMODMixerBackend
{
private:
	SIRFMODContext *m_pContext = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirFMODMixerNativeOps_t m_Ops {};
	sirFMODMixerControl_t *m_pControls = nullptr;
	uint64_t m_nContextEpoch = 0;
	uint32_t m_nCapacity = 0;
	uint32_t m_nCount = 0;
	int32_t m_nLastNativeResult = 0;
	bool m_bInitialized = false;

	bool isReady( void ) const noexcept;
	bool findControl( uint64_t nControlId, uint32_t& iIndex ) const noexcept;
	SIRAudioResult registerControl( uint64_t nControlId, const char *szPath, SIRFMODMixerControlKind eKind,
	                                  SIRFMODBankStore *pBankStore = nullptr, uint64_t nPackageId = 0 ) noexcept;
	SIRAudioResult setVolume( uint64_t nControlId, float fVolume, SIRFMODMixerControlKind eKind ) noexcept;

public:
	SIRFMODMixerBackend( void ) noexcept = default;
	~SIRFMODMixerBackend( void ) noexcept;
	SIRFMODMixerBackend( const SIRFMODMixerBackend& ) = delete;
	SIRFMODMixerBackend& operator=( const SIRFMODMixerBackend& ) = delete;

	SIRAudioResult configure( SIRFMODContext *pContext, const sirContainerAllocator_t *pAllocator, const sirFMODMixerNativeOps_t& ops,
	                            uint32_t nMaxControls ) noexcept;
	SIRAudioResult initialize( void ) noexcept;
	void shutdown( void ) noexcept;
	SIRAudioResult registerBus( uint64_t nControlId, const char *szPath ) noexcept;
	SIRAudioResult registerVCA( uint64_t nControlId, const char *szPath ) noexcept;
	SIRAudioResult registerBusFromBank( uint64_t nControlId, uint64_t nPackageId, SIRFMODBankStore *pBankStore,
	                                      const char *szPath ) noexcept;
	SIRAudioResult registerVCAFromBank( uint64_t nControlId, uint64_t nPackageId, SIRFMODBankStore *pBankStore,
	                                      const char *szPath ) noexcept;
	SIRAudioResult unregisterControl( uint64_t nControlId ) noexcept;
	SIRAudioResult setBusVolume( uint64_t nControlId, float fVolume ) noexcept;
	SIRAudioResult setBusMute( uint64_t nControlId, bool bMute ) noexcept;
	SIRAudioResult setVCAVolume( uint64_t nControlId, float fVolume ) noexcept;
	uint32_t getControlCount( void ) const noexcept;
	bool isBoundTo( const SIRFMODContext *pContext ) const noexcept;
	int32_t getLastNativeResult( void ) const noexcept;
};

sirFMODMixerNativeOps_t sirGetFMODMixerNativeOps( void ) noexcept;

#endif // __SIR_FMODMIXERBACKEND_H__
