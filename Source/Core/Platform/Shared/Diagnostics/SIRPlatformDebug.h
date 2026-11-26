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

#ifndef __SIR_PLATFORMDEBUG_H__
#define __SIR_PLATFORMDEBUG_H__

#if defined( SIR_PRAGMA_ONCE_SUPPORTED ) && SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/SIRPCH.h>

enum class sirDiagnosticResult : uint8_t
{
	SIR_DIAGNOSTIC_OK,
	SIR_DIAGNOSTIC_PARTIAL,
	SIR_DIAGNOSTIC_INVALID_ARGUMENT,
	SIR_DIAGNOSTIC_OS_ERROR
};

typedef struct sirEmergencyBytes_s
{
	const char *pData;
	uint32_t nSize;
} sirEmergencyBytes_t;

class SIRPlatformDebug final
{
public:
	explicit SIRPlatformDebug( void ) = delete;
	explicit SIRPlatformDebug( const SIRPlatformDebug& ) = delete;
	explicit SIRPlatformDebug( SIRPlatformDebug&& ) = delete;

	SIRPlatformDebug& operator=( const SIRPlatformDebug& ) = delete;
	SIRPlatformDebug& operator=( SIRPlatformDebug&& ) = delete;

	// Normal-thread operations only. The inherited stderr sink may block.
	[[nodiscard]] static bool isDebuggerAttached( void ) noexcept;
	static void breakIfAttached( void ) noexcept;
	[[nodiscard]] static sirDiagnosticResult writeEmergency( const sirEmergencyBytes_t& bytes, uint32_t *pWritten = nullptr ) noexcept;

	// No CRT cleanup, debugger query, or output on the secondary/fault path.
	[[noreturn]] static void terminateImmediately( void ) noexcept;
};

#endif // __SIR_PLATFORMDEBUG_H__
