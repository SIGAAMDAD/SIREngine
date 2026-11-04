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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <Core/Platform/SIRChildProcess.h>

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

static uint32_t sirDecodeChildExitStatus( int nStatus ) noexcept
{
	if ( WIFEXITED( nStatus ) ) {
		return ( uint32_t )WEXITSTATUS( nStatus );
	}
	return WIFSIGNALED( nStatus ) ? ( uint32_t )( 128 + WTERMSIG( nStatus ) ) : 255U;
}

SIRChildProcess::SIRChildProcess( void ) noexcept : m_nProcessId( -1 ), m_nExitCode( 0 ), m_bExited( false )
{
}

SIRChildProcess::~SIRChildProcess( void ) noexcept
{
	( void )close();
}

sirChildProcessResult_t SIRChildProcess::start( const sirChildProcessLaunch_t& rLaunch ) noexcept
{
	if ( isOpen() ) {
		return SIR_CHILD_PROCESS_ALREADY_OPEN;
	}
	if ( rLaunch.pszExecutable == nullptr || rLaunch.pszExecutable[0] == '\0' || rLaunch.nArgumentCount > 128 ||
	     ( rLaunch.nArgumentCount != 0 && rLaunch.ppszArguments == nullptr ) ||
	     ( rLaunch.pszWorkingDirectory != nullptr && rLaunch.pszWorkingDirectory[0] == '\0' ) ) {
		return SIR_CHILD_PROCESS_INVALID_ARGUMENT;
	}
	char *pszArguments[130];
	pszArguments[0] = ( char * )rLaunch.pszExecutable;
	for ( uint32_t i = 0; i < rLaunch.nArgumentCount; ++i ) {
		if ( rLaunch.ppszArguments[i] == nullptr ) {
			return SIR_CHILD_PROCESS_INVALID_ARGUMENT;
		}
		pszArguments[i + 1] = ( char * )rLaunch.ppszArguments[i];
	}
	pszArguments[rLaunch.nArgumentCount + 1] = nullptr;

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_t *pActions = nullptr;
	if ( rLaunch.pszWorkingDirectory != nullptr ) {
		if ( posix_spawn_file_actions_init( &actions ) != 0 ) {
			return SIR_CHILD_PROCESS_OS_ERROR;
		}
		pActions = &actions;
		if ( posix_spawn_file_actions_addchdir_np( &actions, rLaunch.pszWorkingDirectory ) != 0 ) {
			( void )posix_spawn_file_actions_destroy( &actions );
			return SIR_CHILD_PROCESS_SPAWN_FAILED;
		}
	}
	pid_t nPid = -1;
	const int nResult = posix_spawnp( &nPid, rLaunch.pszExecutable, pActions, nullptr, pszArguments, environ );
	if ( pActions != nullptr ) {
		( void )posix_spawn_file_actions_destroy( pActions );
	}
	if ( nResult != 0 ) {
		return SIR_CHILD_PROCESS_SPAWN_FAILED;
	}
	m_nProcessId = ( int32_t )nPid;
	m_nExitCode = 0;
	m_bExited = false;
	return SIR_CHILD_PROCESS_OK;
}

sirChildProcessResult_t SIRChildProcess::poll( bool *pbExited, uint32_t *pnExitCode ) noexcept
{
	if ( pbExited == nullptr ) {
		return SIR_CHILD_PROCESS_INVALID_ARGUMENT;
	}
	if ( !isOpen() ) {
		return SIR_CHILD_PROCESS_CLOSED;
	}
	if ( !m_bExited ) {
		int nStatus = 0;
		pid_t nResult = -1;
		do {
			nResult = waitpid( ( pid_t )m_nProcessId, &nStatus, WNOHANG );
		} while ( nResult == -1 && errno == EINTR );
		if ( nResult == -1 ) {
			return SIR_CHILD_PROCESS_OS_ERROR;
		}
		if ( nResult != 0 ) {
			m_nExitCode = sirDecodeChildExitStatus( nStatus );
			m_bExited = true;
		}
	}
	*pbExited = m_bExited;
	if ( m_bExited && pnExitCode != nullptr ) {
		*pnExitCode = m_nExitCode;
	}
	return SIR_CHILD_PROCESS_OK;
}

sirChildProcessResult_t SIRChildProcess::wait( uint32_t *pnExitCode ) noexcept
{
	if ( !isOpen() ) {
		return SIR_CHILD_PROCESS_CLOSED;
	}
	if ( !m_bExited ) {
		int nStatus = 0;
		pid_t nResult;
		do {
			nResult = waitpid( ( pid_t )m_nProcessId, &nStatus, 0 );
		} while ( nResult == -1 && errno == EINTR );
		if ( nResult == -1 ) {
			return SIR_CHILD_PROCESS_OS_ERROR;
		}
		m_nExitCode = sirDecodeChildExitStatus( nStatus );
		m_bExited = true;
	}
	if ( pnExitCode != nullptr ) {
		*pnExitCode = m_nExitCode;
	}
	return SIR_CHILD_PROCESS_OK;
}

sirChildProcessResult_t SIRChildProcess::waitFor( uint32_t nMilliseconds, uint32_t *pnExitCode ) noexcept
{
	if ( !isOpen() ) {
		return SIR_CHILD_PROCESS_CLOSED;
	}
	struct timespec startTime;
	if ( clock_gettime( CLOCK_MONOTONIC, &startTime ) != 0 ) {
		return SIR_CHILD_PROCESS_OS_ERROR;
	}
	const uint64_t nStart = ( uint64_t )startTime.tv_sec * 1000000000ULL + ( uint64_t )startTime.tv_nsec;
	const uint64_t nDuration = ( uint64_t )nMilliseconds * 1000000ULL;
	while ( true ) {
		bool bExited = false;
		const sirChildProcessResult_t eResult = poll( &bExited, pnExitCode );
		if ( eResult != SIR_CHILD_PROCESS_OK ) {
			return eResult;
		}
		if ( bExited ) {
			return SIR_CHILD_PROCESS_OK;
		}
		struct timespec currentTime;
		if ( clock_gettime( CLOCK_MONOTONIC, &currentTime ) != 0 ) {
			return SIR_CHILD_PROCESS_OS_ERROR;
		}
		const uint64_t nNow = ( uint64_t )currentTime.tv_sec * 1000000000ULL + ( uint64_t )currentTime.tv_nsec;
		if ( nNow - nStart >= nDuration ) {
			return SIR_CHILD_PROCESS_TIMED_OUT;
		}
		const uint64_t nRemain = nDuration - ( nNow - nStart );
		struct timespec sleepTime = { 0, ( long )( nRemain > 2000000ULL ? 2000000ULL : nRemain ) };
		while ( nanosleep( &sleepTime, &sleepTime ) != 0 && errno == EINTR ) {
		}
	}
}

sirChildProcessResult_t SIRChildProcess::terminate( void ) noexcept
{
	if ( !isOpen() ) {
		return SIR_CHILD_PROCESS_CLOSED;
	}
	bool bExited = false;
	const sirChildProcessResult_t eState = poll( &bExited, nullptr );
	if ( eState != SIR_CHILD_PROCESS_OK || bExited ) {
		return eState;
	}
	if ( kill( ( pid_t )m_nProcessId, SIGKILL ) != 0 && errno != ESRCH ) {
		return SIR_CHILD_PROCESS_OS_ERROR;
	}
	return wait( nullptr );
}

sirChildProcessResult_t SIRChildProcess::close( void ) noexcept
{
	if ( !isOpen() ) {
		return SIR_CHILD_PROCESS_OK;
	}
	if ( !m_bExited ) {
		const sirChildProcessResult_t eResult = terminate();
		if ( eResult != SIR_CHILD_PROCESS_OK ) {
			return eResult;
		}
	}
	m_nProcessId = -1;
	m_bExited = false;
	m_nExitCode = 0;
	return SIR_CHILD_PROCESS_OK;
}

bool SIRChildProcess::isOpen( void ) const noexcept
{
	return m_nProcessId > 0;
}

uint32_t SIRChildProcess::getProcessId( void ) const noexcept
{
	return isOpen() ? ( uint32_t )m_nProcessId : 0;
}
