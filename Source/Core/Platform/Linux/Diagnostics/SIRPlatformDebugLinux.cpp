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

#if defined( __linux__ )

#include <Core/Platform/Shared/Diagnostics/SIRPlatformDebug.h>

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

bool SIRPlatformDebug::isDebuggerAttached( void ) noexcept
{
	const int32_t nFd = open( "/proc/self/status", O_RDONLY | O_CLOEXEC );
	if ( nFd < 0 ) {
		return false;
	}

	char szStatus[1024]{};
	const ssize_t nRead = read( nFd, szStatus, sizeof( szStatus ) - 1 );
	close( nFd );
	if ( nRead <= 0 ) {
		return false;
	}

	const char *pTracer = strstr( szStatus, "\nTracerPid:" );
	if ( pTracer == nullptr ) {
		return false;
	}
	pTracer += sizeof( "\nTracerPid:" ) - 1;
	while ( *pTracer == ' ' || *pTracer == '\t' ) {
		++pTracer;
	}
	return *pTracer >= '1' && *pTracer <= '9';
}

void SIRPlatformDebug::breakIfAttached( void ) noexcept
{
	if ( isDebuggerAttached() ) {
		raise( SIGTRAP );
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

	// A broken redirection must not turn a nonfatal ensure into SIGPIPE.
	// Preserve both the calling thread's mask and any previously pending signal.
	sigset_t signal{};
	sigemptyset( &signal );
	sigaddset( &signal, SIGPIPE );
	sigset_t previous{};
	if ( pthread_sigmask( SIG_BLOCK, &signal, &previous ) != 0 ) {
		return sirDiagnosticResult::SIR_DIAGNOSTIC_OS_ERROR;
	}
	const int32_t nPreviousError = errno;
	sigset_t pending{};
	const bool bWasPending = sigpending( &pending ) != 0 || sigismember( &pending, SIGPIPE ) == 1;
	const ssize_t nWritten = write( STDERR_FILENO, bytes.pData, bytes.nSize );
	if ( nWritten < 0 && errno == EPIPE && !bWasPending ) {
		const timespec timeout{};
		while ( sigtimedwait( &signal, nullptr, &timeout ) < 0 && errno == EINTR ) {
		}
	}
	pthread_sigmask( SIG_SETMASK, &previous, nullptr );
	errno = nPreviousError;

	if ( nWritten <= 0 ) {
		return sirDiagnosticResult::SIR_DIAGNOSTIC_OS_ERROR;
	}
	if ( pWritten != nullptr ) {
		*pWritten = static_cast<uint32_t>( nWritten );
	}
	return static_cast<uint64_t>( nWritten ) == bytes.nSize ? sirDiagnosticResult::SIR_DIAGNOSTIC_OK : sirDiagnosticResult::SIR_DIAGNOSTIC_PARTIAL;
}

void SIRPlatformDebug::terminateImmediately( void ) noexcept
{
	_exit( 134 );
}

#endif // defined( __linux__ )
