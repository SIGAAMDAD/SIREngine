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

#ifndef __SIRENGINE_SIRAUDIOMOCKBACKEND_H__
#define __SIRENGINE_SIRAUDIOMOCKBACKEND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/SIRAudioBackend.h>
#include <Audio/SIRAudioQueue.h>

// Test-only provider; no vendor SDK, I/O, audio device or event synthesis.
class SIRAudioMockBackend
{
private:
	SIRAudioQueue<sirAudioCompletion_t> m_Completions;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	bool m_bInitialized = false;
	bool m_bFailNextPlay = false;
	bool m_bFailNextStop = false;
	uint64_t m_nSubmitted = 0;

	static sirAudioResult_t initializeCallback( void *pContext, const sirAudioBackendConfig_t *pConfig ) noexcept;
	static void shutdownCallback( void *pContext ) noexcept;
	static sirAudioResult_t submitCallback( void *pContext, const sirAudioCommand_t *pCommands, uint32_t nCount ) noexcept;
	static uint32_t drainCallback( void *pContext, sirAudioCompletion_t *pOutput, uint32_t nCapacity ) noexcept;
	static void updateCallback( void *pContext ) noexcept;
	static uint64_t capabilitiesCallback( const void *pContext ) noexcept;

public:
	explicit SIRAudioMockBackend( const sirContainerAllocator_t *pAllocator ) noexcept;
	static sirAudioBackendOps_t getOps( void ) noexcept;
	void failNextPlay( void ) noexcept;
	void failNextStop( void ) noexcept;
	bool injectCompletion( const sirAudioCompletion_t &completion ) noexcept;
	uint64_t getSubmittedCount( void ) const noexcept;
};

#endif // __SIRENGINE_SIRAUDIOMOCKBACKEND_H__
