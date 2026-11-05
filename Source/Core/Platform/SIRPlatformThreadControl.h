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

#ifndef __SIRENGINE_SIRPLATFORMTHREADCONTROL_H__
#define __SIRENGINE_SIRPLATFORMTHREADCONTROL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

typedef enum sirThreadControlResult_e : uint8_t
{
	SIR_THREAD_CONTROL_OK = 0,
	SIR_THREAD_CONTROL_INVALID_ARGUMENT = 1,
	SIR_THREAD_CONTROL_ACCESS_DENIED = 2,
	SIR_THREAD_CONTROL_UNSUPPORTED = 3,
	SIR_THREAD_CONTROL_OS_ERROR = 4
} sirThreadControlResult_t;

typedef enum sirThreadPriority_e : uint8_t
{
	SIR_THREAD_PRIORITY_BACKGROUND = 0,
	SIR_THREAD_PRIORITY_NORMAL = 1,
	SIR_THREAD_PRIORITY_HIGH = 2
} sirThreadPriority_t;

typedef struct sirThreadAffinity_s
{
	uint64_t nMask;
	uint16_t nGroup;
} sirThreadAffinity_t;

// Controls the calling thread only. Windows supports explicit processor groups;
// Linux requires nGroup == 0 and a mask over CPU indices 0..63.
// Priority is an OS scheduling hint and may require extra privileges.
class SIRPlatformThreadControl
{
public:
	SIRPlatformThreadControl( void ) = delete;
	~SIRPlatformThreadControl( void ) = delete;

	static sirThreadControlResult_t setCurrentName( const char *pszName ) noexcept;
	static sirThreadControlResult_t getCurrentName( char *pszName, uint32_t nCapacity ) noexcept;
	static sirThreadControlResult_t setCurrentAffinity( const sirThreadAffinity_t& rAffinity ) noexcept;
	static sirThreadControlResult_t getCurrentAffinity( sirThreadAffinity_t *pAffinity ) noexcept;
	static sirThreadControlResult_t setCurrentPriority( sirThreadPriority_t ePriority ) noexcept;
};

#endif // __SIRENGINE_SIRPLATFORMTHREADCONTROL_H__
