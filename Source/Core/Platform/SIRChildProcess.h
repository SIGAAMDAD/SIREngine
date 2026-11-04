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

#ifndef __SIRENGINE_SIRCHILDPROCESS_H__
#define __SIRENGINE_SIRCHILDPROCESS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

typedef enum sirChildProcessResult_e : uint8_t
{
	SIR_CHILD_PROCESS_OK = 0,
	SIR_CHILD_PROCESS_INVALID_ARGUMENT = 1,
	SIR_CHILD_PROCESS_ALREADY_OPEN = 2,
	SIR_CHILD_PROCESS_CLOSED = 3,
	SIR_CHILD_PROCESS_RUNNING = 4,
	SIR_CHILD_PROCESS_TIMED_OUT = 5,
	SIR_CHILD_PROCESS_SPAWN_FAILED = 6,
	SIR_CHILD_PROCESS_OS_ERROR = 7
} sirChildProcessResult_t;

typedef struct sirChildProcessLaunch_s
{
	const char *pszExecutable;
	const char *const *ppszArguments;
	uint32_t nArgumentCount;
	const char *pszWorkingDirectory;
} sirChildProcessLaunch_t;

// Owns a child process. close() and destruction terminate a still-running
// child and reap/wait for it; no detached zombies remain on Linux.
// Call close() after observing an exit before reusing this object.
// Environment is inherited. Standard stream redirection will be provided
// by a later integration with SIRPlatformPipe after its branch is merged.
class SIRChildProcess final
{
private:
#if defined( _WIN32 )
	void *m_pProcess;
	uint32_t m_nProcessId;
#else
	int32_t m_nProcessId;
#endif
	uint32_t m_nExitCode;
	bool m_bExited;

public:
	SIRChildProcess( void ) noexcept;
	~SIRChildProcess( void ) noexcept;
	SIRChildProcess( const SIRChildProcess& ) = delete;
	SIRChildProcess& operator=( const SIRChildProcess& ) = delete;

	sirChildProcessResult_t start( const sirChildProcessLaunch_t& rLaunch ) noexcept;
	sirChildProcessResult_t poll( bool *pbExited, uint32_t *pnExitCode ) noexcept;
	sirChildProcessResult_t wait( uint32_t *pnExitCode ) noexcept;
	sirChildProcessResult_t waitFor( uint32_t nMilliseconds, uint32_t *pnExitCode ) noexcept;
	sirChildProcessResult_t terminate( void ) noexcept;
	sirChildProcessResult_t close( void ) noexcept;
	bool isOpen( void ) const noexcept;
	uint32_t getProcessId( void ) const noexcept;
};

#endif // __SIRENGINE_SIRCHILDPROCESS_H__
