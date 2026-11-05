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

#include <Core/Platform/SIRChildProcess.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <stddef.h>

static bool sirConvertProcessUtf8( const char *pszSource, wchar_t *pDestination, int nCapacity ) noexcept
{
	return pszSource != nullptr && MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszSource, -1, pDestination, nCapacity ) > 0;
}

static bool sirAppendProcessCharacter( wchar_t *pszCommand, size_t nCapacity, size_t *pnCursor, wchar_t c ) noexcept
{
	if ( *pnCursor + 1 >= nCapacity ) {
		return false;
	}
	pszCommand[( *pnCursor )++] = c;
	pszCommand[*pnCursor] = L'\0';
	return true;
}

// Inverse of the Windows argv command-line backslash/quote parsing convention.
static bool sirAppendProcessArgument( wchar_t *pszCommand, size_t nCapacity, size_t *pnCursor, const wchar_t *pszArgument ) noexcept
{
	if ( !sirAppendProcessCharacter( pszCommand, nCapacity, pnCursor, L'"' ) ) {
		return false;
	}
	const wchar_t *pCursor = pszArgument;
	while ( *pCursor != L'\0' ) {
		uint32_t nSlashes = 0;
		while ( *pCursor == L'\\' ) {
			++nSlashes;
			++pCursor;
		}
		if ( *pCursor == L'"' ) {
			for ( uint32_t i = 0; i < nSlashes * 2 + 1; ++i ) {
				if ( !sirAppendProcessCharacter( pszCommand, nCapacity, pnCursor, L'\\' ) ) {
					return false;
				}
			}
			if ( !sirAppendProcessCharacter( pszCommand, nCapacity, pnCursor, *pCursor++ ) ) {
				return false;
			}
		} else {
			const uint32_t nCopies = ( *pCursor == L'\0' ) ? nSlashes * 2 : nSlashes;
			for ( uint32_t i = 0; i < nCopies; ++i ) {
				if ( !sirAppendProcessCharacter( pszCommand, nCapacity, pnCursor, L'\\' ) ) {
					return false;
				}
			}
			if ( *pCursor != L'\0' && !sirAppendProcessCharacter( pszCommand, nCapacity, pnCursor, *pCursor++ ) ) {
				return false;
			}
		}
	}
	return sirAppendProcessCharacter( pszCommand, nCapacity, pnCursor, L'"' );
}

SIRChildProcess::SIRChildProcess( void ) noexcept : m_pProcess( nullptr ), m_nProcessId( 0 ), m_nExitCode( 0 ), m_bExited( false )
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
	wchar_t szExecutable[32768] = {};
	wchar_t szCommand[32768] = {};
	wchar_t szArgument[32768] = {};
	wchar_t szWorkingDirectory[32768] = {};
	if ( !sirConvertProcessUtf8( rLaunch.pszExecutable, szExecutable, 32768 ) ) {
		return SIR_CHILD_PROCESS_INVALID_ARGUMENT;
	}
	if ( rLaunch.pszWorkingDirectory != nullptr && !sirConvertProcessUtf8( rLaunch.pszWorkingDirectory, szWorkingDirectory, 32768 ) ) {
		return SIR_CHILD_PROCESS_INVALID_ARGUMENT;
	}
	const wchar_t *pszCwd = rLaunch.pszWorkingDirectory == nullptr ? nullptr : szWorkingDirectory;
	size_t nCursor = 0;
	if ( !sirAppendProcessArgument( szCommand, 32768, &nCursor, szExecutable ) ) {
		return SIR_CHILD_PROCESS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < rLaunch.nArgumentCount; ++i ) {
		if ( rLaunch.ppszArguments[i] == nullptr || !sirConvertProcessUtf8( rLaunch.ppszArguments[i], szArgument, 32768 ) ||
		     !sirAppendProcessCharacter( szCommand, 32768, &nCursor, L' ' ) ||
		     !sirAppendProcessArgument( szCommand, 32768, &nCursor, szArgument ) ) {
			return SIR_CHILD_PROCESS_INVALID_ARGUMENT;
		}
	}
	STARTUPINFOW startup{};
	startup.cb = sizeof( startup );
	PROCESS_INFORMATION process{};
	if ( !CreateProcessW( szExecutable, szCommand, nullptr, nullptr, FALSE, 0, nullptr, pszCwd, &startup, &process ) ) {
		return SIR_CHILD_PROCESS_SPAWN_FAILED;
	}
	CloseHandle( process.hThread );
	m_pProcess = process.hProcess;
	m_nProcessId = ( uint32_t )process.dwProcessId;
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
		const DWORD nWait = WaitForSingleObject( ( HANDLE )m_pProcess, 0 );
		if ( nWait == WAIT_OBJECT_0 ) {
			DWORD nExitCode = 0;
			if ( !GetExitCodeProcess( ( HANDLE )m_pProcess, &nExitCode ) ) {
				return SIR_CHILD_PROCESS_OS_ERROR;
			}
			m_nExitCode = ( uint32_t )nExitCode;
			m_bExited = true;
		} else if ( nWait != WAIT_TIMEOUT ) {
			return SIR_CHILD_PROCESS_OS_ERROR;
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
	if ( !m_bExited && WaitForSingleObject( ( HANDLE )m_pProcess, INFINITE ) != WAIT_OBJECT_0 ) {
		return SIR_CHILD_PROCESS_OS_ERROR;
	}
	bool bExited = false;
	return poll( &bExited, pnExitCode );
}

sirChildProcessResult_t SIRChildProcess::waitFor( uint32_t nMilliseconds, uint32_t *pnExitCode ) noexcept
{
	if ( !isOpen() ) {
		return SIR_CHILD_PROCESS_CLOSED;
	}
	if ( !m_bExited ) {
		const DWORD nWait = WaitForSingleObject( ( HANDLE )m_pProcess, nMilliseconds == INFINITE ? INFINITE - 1 : nMilliseconds );
		if ( nWait == WAIT_TIMEOUT ) {
			return SIR_CHILD_PROCESS_TIMED_OUT;
		}
		if ( nWait != WAIT_OBJECT_0 ) {
			return SIR_CHILD_PROCESS_OS_ERROR;
		}
	}
	bool bExited = false;
	return poll( &bExited, pnExitCode );
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
	if ( !TerminateProcess( ( HANDLE )m_pProcess, 137 ) ) {
		// If the child exited between poll and termination, that is success.
		return poll( &bExited, nullptr ) == SIR_CHILD_PROCESS_OK && bExited ? SIR_CHILD_PROCESS_OK : SIR_CHILD_PROCESS_OS_ERROR;
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
	if ( !CloseHandle( ( HANDLE )m_pProcess ) ) {
		return SIR_CHILD_PROCESS_OS_ERROR;
	}
	m_pProcess = nullptr;
	m_nProcessId = 0;
	m_nExitCode = 0;
	m_bExited = false;
	return SIR_CHILD_PROCESS_OK;
}

bool SIRChildProcess::isOpen( void ) const noexcept
{
	return m_pProcess != nullptr;
}

uint32_t SIRChildProcess::getProcessId( void ) const noexcept
{
	return isOpen() ? m_nProcessId : 0;
}
