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

#if defined( _WIN32 )

#include <Core/Platform/Shared/Diagnostics/SIRPlatformDebug.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <intrin.h>

bool SIRPlatformDebug::isDebuggerAttached( void ) noexcept
{
	return IsDebuggerPresent() != FALSE;
}

void SIRPlatformDebug::breakIfAttached( void ) noexcept
{
	if ( isDebuggerAttached() ) {
		DebugBreak();
	}
}

sirDiagnosticResult SIRPlatformDebug::writeEmergency( const sirEmergencyBytes_t& bytes, uint32_t *pWritten ) noexcept
{
	if ( pWritten != nullptr ) {
		*pWritten = 0;
	}
	if ( bytes.nSize == 0 ) {
		return sirDiagnosticResult::SIR_DIAGNOSTIC_OK;
	}
	if ( bytes.pData == nullptr ) {
		return sirDiagnosticResult::SIR_DIAGNOSTIC_INVALID_ARGUMENT;
	}

	const DWORD nPreviousError = GetLastError();
	const HANDLE hStderr = GetStdHandle( STD_ERROR_HANDLE );
	DWORD nWritten = 0;
	const bool bWritten = hStderr != nullptr && hStderr != INVALID_HANDLE_VALUE &&
	                      WriteFile( hStderr, bytes.pData, static_cast<DWORD>( bytes.nSize ), &nWritten, nullptr ) != FALSE;
	if ( isDebuggerAttached() ) {
		// The debugger mirror accepts text only; the stderr path is byte-exact.
		char szMirror[1024]{};
		const size_t nCopy = bytes.nSize < sizeof( szMirror ) ? bytes.nSize : sizeof( szMirror ) - 1;
		memcpy( szMirror, bytes.pData, nCopy );
		OutputDebugStringA( szMirror );
	}
	SetLastError( nPreviousError );

	if ( pWritten != nullptr ) {
		*pWritten = static_cast<uint32_t>( nWritten );
	}
	if ( !bWritten || nWritten == 0 ) {
		return sirDiagnosticResult::SIR_DIAGNOSTIC_OS_ERROR;
	}
	return nWritten == bytes.nSize ? sirDiagnosticResult::SIR_DIAGNOSTIC_OK : sirDiagnosticResult::SIR_DIAGNOSTIC_PARTIAL;
}

void SIRPlatformDebug::terminateImmediately( void ) noexcept
{
	TerminateProcess( GetCurrentProcess(), 134 );
#if defined( _MSC_VER )
	__fastfail( FAST_FAIL_FATAL_APP_EXIT );
#else
	__builtin_trap();
#endif
}

#endif // defined( _WIN32 )
