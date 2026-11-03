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

#ifndef __SIRENGINE_SIRAUDIOBACKEND_H__
#define __SIRENGINE_SIRAUDIOBACKEND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/SIRAudioTypes.h>

// ABI v2 uses portable POD messages; middleware handles remain private.
typedef struct sirAudioBackendOps_s
{
	uint32_t nAbiVersion;
	sirAudioResult_t ( *initialize )( void *pContext, const sirAudioBackendConfig_t *pConfig ) noexcept;
	void ( *shutdown )( void *pContext ) noexcept;
	sirAudioResult_t ( *submit )( void *pContext, const sirAudioCommand_t *pCommands, uint32_t nCount ) noexcept;
	uint32_t ( *drainCompletions )( void *pContext, sirAudioCompletion_t *pOutput, uint32_t nCapacity ) noexcept;
	void ( *update )( void *pContext ) noexcept;
	uint64_t ( *getCapabilities )( const void *pContext ) noexcept;
	// ABI v2: attributes are valid for the duration of this call only.
	sirAudioResult_t ( *submitSpatial )( void *pContext, const sirAudioCommand_t *pCommand,
	                                    const sirAudioAttributes_t *pAttributes ) noexcept;
} sirAudioBackendOps_t;

#endif // __SIRENGINE_SIRAUDIOBACKEND_H__
