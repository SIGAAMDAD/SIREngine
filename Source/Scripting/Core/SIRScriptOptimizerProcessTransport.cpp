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

#include <Scripting/Core/SIRScriptOptimizerProcessTransport.h>

#include <Core/Platform/SIRScopedLock.h>

#include <string.h>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

SIRScriptOptimizerProcessTransport::SIRScriptOptimizerProcessTransport( void ) noexcept
    : m_Config{}, m_OwnerThread( SIRThread::currentThreadId() ), m_Writer{}, m_Reader{}, m_Mutex{}, m_CV{}, m_bStopping( false ),
      m_bFailed( false ), m_LastActivity{}, m_szRequests{}, m_szReplies{}, m_nRequestHead( 0 ), m_nRequestTail( 0 ), m_nRequestCount( 0 ),
      m_nReplyHead( 0 ), m_nReplyTail( 0 ), m_nReplyCount( 0 ), m_nOutstanding( 0 ), m_bConfigured( false ), m_bRunning( false )
#if defined( _WIN32 )
      ,
      m_hChildProcess( nullptr ), m_hReadPipe( nullptr ), m_hWritePipe( nullptr )
#else
      ,
      m_nChildPid( -1 ), m_nReadFd( -1 ), m_nWriteFd( -1 )
#endif
{
}

SIRScriptOptimizerProcessTransport::~SIRScriptOptimizerProcessTransport( void ) noexcept
{
	stop();
}

sirScriptOptimizationTransportAPI_t SIRScriptOptimizerProcessTransport::getAPI( void ) noexcept
{
	return { SIR_SCRIPT_OPT_TRANSPORT_ABI_REVISION, this, &apiStart, &apiStop, &apiSend, &apiReceive };
}

bool SIRScriptOptimizerProcessTransport::isRunning( void ) const noexcept
{
	return SIRThread::isCurrentThread( m_OwnerThread ) && m_bRunning && !m_bFailed.load( std::memory_order_acquire );
}

bool SIRScriptOptimizerProcessTransport::hasFailed( void ) const noexcept
{
	return m_bFailed.load( std::memory_order_acquire );
}

SIRScriptResult SIRScriptOptimizerProcessTransport::configure( const sirScriptOptimizerProcessConfig_t& config ) noexcept
{
	if ( !SIRThread::isCurrentThread( m_OwnerThread ) ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( m_bRunning ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	if ( config.nArgumentCount > SIR_SCRIPT_OPT_MAX_ARGUMENTS || config.szExecutable[0] == '\0' ||
	     memchr( config.szExecutable, '\0', sizeof( config.szExecutable ) ) == nullptr ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < config.nArgumentCount; ++i ) {
		if ( memchr( config.szArguments[i], '\0', sizeof( config.szArguments[i] ) ) == nullptr ) {
			return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
		}
	}
	m_Config = config;
	m_bConfigured = true;
	return SIRScriptResult::SIR_SCRIPT_OK;
}

SIRScriptResult SIRScriptOptimizerProcessTransport::start( void ) noexcept
{
	if ( !SIRThread::isCurrentThread( m_OwnerThread ) ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured || m_bRunning ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	m_bStopping.store( false, std::memory_order_release );
	m_bFailed.store( false, std::memory_order_release );
	{
		SIRScopedLock<SIRMutex> guard( m_Mutex );
		m_nRequestHead = m_nRequestTail = m_nRequestCount = 0;
		m_nReplyHead = m_nReplyTail = m_nReplyCount = 0;
	}
	m_nOutstanding = 0;
	m_LastActivity = std::chrono::steady_clock::now();
	if ( !spawn() ) {
		closePipes();
		return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
	}
	m_bRunning = true;
	if ( !m_Writer.start( &SIRScriptOptimizerProcessTransport::writerEntry, this ) ||
	     !m_Reader.start( &SIRScriptOptimizerProcessTransport::readerEntry, this ) ) {
		stop();
		return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
	}
	return SIRScriptResult::SIR_SCRIPT_OK;
}

void SIRScriptOptimizerProcessTransport::stop( void ) noexcept
{
	if ( !m_bRunning && !m_Writer.isJoinable() && !m_Reader.isJoinable() ) {
		return;
	}
	m_bStopping.store( true, std::memory_order_release );
	m_CV.notifyAll();
	// Terminate the child before cancelling synchronous I/O. Otherwise an I/O
	// thread may enter ReadFile after the cancellation and block indefinitely.
	terminateChild();
#if defined( _WIN32 )
	// Interrupt any remaining Win32 pipe operations before joining threads.
	if ( m_Writer.isJoinable() ) {
		static_cast<void>(m_Writer.cancelSynchronousIo());
	}
	if ( m_Reader.isJoinable() ) {
		static_cast<void>(m_Reader.cancelSynchronousIo());
	}
#endif
	if ( m_Writer.isJoinable() ) {
		m_Writer.join();
	}
	if ( m_Reader.isJoinable() ) {
		m_Reader.join();
	}
	closePipes();
	{
		SIRScopedLock<SIRMutex> guard( m_Mutex );
		m_nRequestHead = m_nRequestTail = m_nRequestCount = 0;
		m_nReplyHead = m_nReplyTail = m_nReplyCount = 0;
	}
	m_nOutstanding = 0;
	m_bRunning = false;
}

SIRScriptResult SIRScriptOptimizerProcessTransport::send( const uint8_t *pWire, uint32_t nBytes ) noexcept
{
	if ( !SIRThread::isCurrentThread( m_OwnerThread ) ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( !m_bRunning ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	if ( pWire == nullptr || nBytes != SIRScriptOptimizationProtocol::REQUEST_BYTES ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( m_bFailed.load( std::memory_order_acquire ) ) {
		return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
	}
	{
		SIRScopedLock<SIRMutex> guard( m_Mutex );
		// Bound work that has been sent but not yet consumed by the owner.
		// Writer-side queue drainage alone does not free reply capacity.
		if ( m_nRequestCount == SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES || m_nOutstanding == SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES ) {
			return SIRScriptResult::SIR_SCRIPT_BUSY;
		}
		memcpy( m_szRequests[m_nRequestTail], pWire, nBytes );
		m_nRequestTail = ( m_nRequestTail + 1 ) % SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES;
		++m_nRequestCount;
	}
	++m_nOutstanding;
	m_LastActivity = std::chrono::steady_clock::now();
	m_CV.notifyAll();
	return SIRScriptResult::SIR_SCRIPT_OK;
}

SIRScriptResult SIRScriptOptimizerProcessTransport::receive( uint8_t *pWire, uint32_t nCapacity, uint32_t *pReceived ) noexcept
{
	if ( !SIRThread::isCurrentThread( m_OwnerThread ) ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( !m_bRunning ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	if ( pWire == nullptr || pReceived == nullptr || nCapacity < SIRScriptOptimizationProtocol::REPLY_BYTES ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pReceived = 0;
	{
		SIRScopedLock<SIRMutex> guard( m_Mutex );
		if ( m_nReplyCount != 0 ) {
			memcpy( pWire, m_szReplies[m_nReplyHead], SIRScriptOptimizationProtocol::REPLY_BYTES );
			m_nReplyHead = ( m_nReplyHead + 1 ) % SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES;
			--m_nReplyCount;
			*pReceived = SIRScriptOptimizationProtocol::REPLY_BYTES;
			if ( m_nOutstanding != 0 ) {
				--m_nOutstanding;
			}
			m_LastActivity = std::chrono::steady_clock::now();
			m_CV.notifyAll();
			return SIRScriptResult::SIR_SCRIPT_OK;
		}
	}
	if ( m_bFailed.load( std::memory_order_acquire ) ) {
		return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( m_nOutstanding > 0 && m_Config.nWatchdogMilliseconds > 0 ) {
		const auto nElapsed = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - m_LastActivity );
		if ( nElapsed.count() >= m_Config.nWatchdogMilliseconds ) {
			failTransport();
			return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
		}
	}
	return SIRScriptResult::SIR_SCRIPT_BUSY;
}

void SIRScriptOptimizerProcessTransport::failTransport( void ) noexcept
{
	if ( !m_bStopping.load( std::memory_order_acquire ) ) {
		m_bFailed.store( true, std::memory_order_release );
		m_CV.notifyAll();
	}
}

void SIRScriptOptimizerProcessTransport::writerEntry( void *pUserData ) noexcept
{
	static_cast<SIRScriptOptimizerProcessTransport *>(pUserData)->writerMain();
}

void SIRScriptOptimizerProcessTransport::readerEntry( void *pUserData ) noexcept
{
	static_cast<SIRScriptOptimizerProcessTransport *>(pUserData)->readerMain();
}

void SIRScriptOptimizerProcessTransport::writerMain( void ) noexcept
{
#if !defined( _WIN32 )
	// A dead worker must yield EPIPE, never terminate the host via SIGPIPE.
	sigset_t block;
	sigemptyset( &block );
	sigaddset( &block, SIGPIPE );
	static_cast<void>(pthread_sigmask( SIG_BLOCK, &block, nullptr ));
#endif
	uint8_t szFrame[SIRScriptOptimizationProtocol::REQUEST_BYTES];
	memset( szFrame, 0, sizeof( szFrame ) );
	for ( ;; ) {
		{
			SIRScopedLock<SIRMutex> guard( m_Mutex );
			while ( !m_bStopping.load() && !m_bFailed.load() && m_nRequestCount == 0 ) {
				m_CV.wait( m_Mutex );
			}
			if ( m_bStopping.load() || m_bFailed.load() ) {
				return;
			}
			memcpy( szFrame, m_szRequests[m_nRequestHead], sizeof( szFrame ) );
			m_nRequestHead = ( m_nRequestHead + 1 ) % SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES;
			--m_nRequestCount;
		}
		if ( !writeFrame( szFrame, sizeof( szFrame ) ) ) {
			failTransport();
			return;
		}
	}
}

void SIRScriptOptimizerProcessTransport::readerMain( void ) noexcept
{
	uint8_t szFrame[SIRScriptOptimizationProtocol::REPLY_BYTES];
	memset( szFrame, 0, sizeof( szFrame ) );
	for ( ;; ) {
		if ( m_bStopping.load( std::memory_order_acquire ) ) {
			return;
		}
		if ( !readFrame( szFrame, sizeof( szFrame ) ) ) {
			failTransport();
			return;
		}
		SIRScopedLock<SIRMutex> guard( m_Mutex );
		while ( !m_bStopping.load() && m_nReplyCount == SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES ) {
			m_CV.wait( m_Mutex );
		}
		if ( m_bStopping.load() ) {
			return;
		}
		memcpy( m_szReplies[m_nReplyTail], szFrame, sizeof( szFrame ) );
		m_nReplyTail = ( m_nReplyTail + 1 ) % SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES;
		++m_nReplyCount;
	}
}

SIRScriptResult SIRScriptOptimizerProcessTransport::apiStart( void *pUser ) noexcept
{
	return static_cast<SIRScriptOptimizerProcessTransport *>(pUser)->start();
}

void SIRScriptOptimizerProcessTransport::apiStop( void *pUser ) noexcept
{
	static_cast<SIRScriptOptimizerProcessTransport *>(pUser)->stop();
}

SIRScriptResult SIRScriptOptimizerProcessTransport::apiSend( void *pUser, const uint8_t *pWire, uint32_t nBytes ) noexcept
{
	return static_cast<SIRScriptOptimizerProcessTransport *>(pUser)->send( pWire, nBytes );
}

SIRScriptResult SIRScriptOptimizerProcessTransport::apiReceive( void *pUser, uint8_t *pWire, uint32_t nCapacity,
                                                                  uint32_t *pReceived ) noexcept
{
	return static_cast<SIRScriptOptimizerProcessTransport *>(pUser)->receive( pWire, nCapacity, pReceived );
}

#if !defined( _WIN32 )
static bool scriptTransportSetCloseOnExec( int nFd ) noexcept
{
	return fcntl( nFd, F_SETFD, FD_CLOEXEC ) == 0;
}

bool SIRScriptOptimizerProcessTransport::spawn( void ) noexcept
{
	int szInput[2] = { -1, -1 };
	int szOutput[2] = { -1, -1 };
	if ( pipe( szInput ) != 0 ) {
		return false;
	}
	if ( pipe( szOutput ) != 0 ) {
		close( szInput[0] );
		close( szInput[1] );
		return false;
	}
	bool bSuccess = scriptTransportSetCloseOnExec( szInput[0] ) && scriptTransportSetCloseOnExec( szInput[1] ) &&
	                scriptTransportSetCloseOnExec( szOutput[0] ) && scriptTransportSetCloseOnExec( szOutput[1] );
	posix_spawn_file_actions_t actions;
	bool bActionsInitialized = false;
	if ( bSuccess ) {
		bActionsInitialized = posix_spawn_file_actions_init( &actions ) == 0;
		bSuccess = bActionsInitialized;
	}
	if ( bSuccess ) {
		bSuccess = posix_spawn_file_actions_adddup2( &actions, szInput[0], STDIN_FILENO ) == 0 &&
		           posix_spawn_file_actions_adddup2( &actions, szOutput[1], STDOUT_FILENO ) == 0 &&
		           posix_spawn_file_actions_addclose( &actions, szInput[0] ) == 0 &&
		           posix_spawn_file_actions_addclose( &actions, szInput[1] ) == 0 &&
		           posix_spawn_file_actions_addclose( &actions, szOutput[0] ) == 0 &&
		           posix_spawn_file_actions_addclose( &actions, szOutput[1] ) == 0;
	}
	if ( bSuccess ) {
		char *szArguments[SIR_SCRIPT_OPT_MAX_ARGUMENTS + 2] = {};
		szArguments[0] = m_Config.szExecutable;
		for ( uint32_t i = 0; i < m_Config.nArgumentCount; ++i ) {
			szArguments[i + 1] = m_Config.szArguments[i];
		}
		pid_t nPid = -1;
		bSuccess = posix_spawnp( &nPid, m_Config.szExecutable, &actions, nullptr, szArguments, environ ) == 0;
		if ( bSuccess ) {
			m_nChildPid = static_cast<int>(nPid);
		}
	}
	if ( bActionsInitialized ) {
		static_cast<void>(posix_spawn_file_actions_destroy( &actions ));
	}
	close( szInput[0] );
	close( szOutput[1] );
	if ( bSuccess ) {
		m_nWriteFd = szInput[1];
		m_nReadFd = szOutput[0];
	} else {
		close( szInput[1] );
		close( szOutput[0] );
	}
	return bSuccess;
}

void SIRScriptOptimizerProcessTransport::terminateChild( void ) noexcept
{
	if ( m_nChildPid > 0 ) {
		int nStatus = 0;
		if ( waitpid( static_cast<pid_t>(m_nChildPid), &nStatus, WNOHANG ) == 0 ) {
			static_cast<void>(kill( static_cast<pid_t>(m_nChildPid), SIGKILL ));
			while ( waitpid( static_cast<pid_t>(m_nChildPid), &nStatus, 0 ) < 0 && errno == EINTR ) {
			}
		}
		m_nChildPid = -1;
	}
}

void SIRScriptOptimizerProcessTransport::closePipes( void ) noexcept
{
	if ( m_nReadFd >= 0 ) {
		close( m_nReadFd );
		m_nReadFd = -1;
	}
	if ( m_nWriteFd >= 0 ) {
		close( m_nWriteFd );
		m_nWriteFd = -1;
	}
}

bool SIRScriptOptimizerProcessTransport::writeFrame( const uint8_t *pData, uint32_t nBytes ) noexcept
{
	uint32_t nOffset = 0;
	while ( nOffset < nBytes ) {
		const ssize_t nWritten = write( m_nWriteFd, pData + nOffset, nBytes - nOffset );
		if ( nWritten < 0 && errno == EINTR ) {
			continue;
		}
		if ( nWritten <= 0 ) {
			return false;
		}
		nOffset += static_cast<uint32_t>(nWritten);
	}
	return true;
}

bool SIRScriptOptimizerProcessTransport::readFrame( uint8_t *pData, uint32_t nBytes ) noexcept
{
	uint32_t nOffset = 0;
	while ( nOffset < nBytes ) {
		const ssize_t nRead = read( m_nReadFd, pData + nOffset, nBytes - nOffset );
		if ( nRead < 0 && errno == EINTR ) {
			continue;
		}
		if ( nRead <= 0 ) {
			return false;
		}
		nOffset += static_cast<uint32_t>(nRead);
	}
	return true;
}
#else
// Win32 anonymous pipes are blocking; both directions run exclusively on
// dedicated threads and never block the scripting owner lane.
static bool scriptTransportAppendWindowsArgument( char *pszBuffer, size_t nCapacity, size_t *pOffset, const char *pszArgument ) noexcept
{
	if ( *pOffset + 2 >= nCapacity ) {
		return false;
	}
	pszBuffer[( *pOffset )++] = '"';
	uint32_t nBackslashes = 0;
	for ( const char *p = pszArgument;; ++p ) {
		if ( *p == '\\' ) {
			++nBackslashes;
			continue;
		}
		const bool bQuote = *p == '"';
		const uint32_t nCount = nBackslashes * ( bQuote || *p == 0 ? 2 : 1 ) + ( bQuote ? 1 : 0 );
		if ( *pOffset + nCount + 3 >= nCapacity ) {
			return false;
		}
		for ( uint32_t i = 0; i < nCount; ++i ) {
			pszBuffer[( *pOffset )++] = '\\';
		}
		nBackslashes = 0;
		if ( *p == 0 ) {
			pszBuffer[( *pOffset )++] = '"';
			pszBuffer[*pOffset] = 0;
			return true;
		}
		pszBuffer[( *pOffset )++] = *p;
	}
}

bool SIRScriptOptimizerProcessTransport::spawn( void ) noexcept
{
	SECURITY_ATTRIBUTES security = { sizeof( SECURITY_ATTRIBUTES ), nullptr, TRUE };
	HANDLE hInputRead = nullptr, hInputWrite = nullptr, hOutputRead = nullptr, hOutputWrite = nullptr;
	if ( !CreatePipe( &hInputRead, &hInputWrite, &security, 0 ) ) {
		return false;
	}
	if ( !CreatePipe( &hOutputRead, &hOutputWrite, &security, 0 ) ) {
		CloseHandle( hInputRead );
		CloseHandle( hInputWrite );
		return false;
	}
	bool bSuccess =
	    SetHandleInformation( hInputWrite, HANDLE_FLAG_INHERIT, 0 ) && SetHandleInformation( hOutputRead, HANDLE_FLAG_INHERIT, 0 );
	char szCommandLine[4096];
	memset( szCommandLine, 0, sizeof( szCommandLine ) );
	size_t nOffset = 0;
	if ( bSuccess ) {
		bSuccess = scriptTransportAppendWindowsArgument( szCommandLine, sizeof( szCommandLine ), &nOffset, m_Config.szExecutable );
		for ( uint32_t i = 0; bSuccess && i < m_Config.nArgumentCount; ++i ) {
			szCommandLine[nOffset++] = ' ';
			bSuccess = scriptTransportAppendWindowsArgument( szCommandLine, sizeof( szCommandLine ), &nOffset, m_Config.szArguments[i] );
		}
	}
	PROCESS_INFORMATION process {};
	STARTUPINFOA startup {};
	startup.cb = sizeof( startup );
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = hInputRead;
	startup.hStdOutput = hOutputWrite;
	startup.hStdError = GetStdHandle( STD_ERROR_HANDLE );
	if ( bSuccess ) {
		bSuccess = CreateProcessA( m_Config.szExecutable, szCommandLine, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
		                           &startup, &process ) != 0;
	}
	CloseHandle( hInputRead );
	CloseHandle( hOutputWrite );
	if ( bSuccess ) {
		m_hChildProcess = process.hProcess;
		CloseHandle( process.hThread );
		m_hWritePipe = hInputWrite;
		m_hReadPipe = hOutputRead;
	} else {
		CloseHandle( hInputWrite );
		CloseHandle( hOutputRead );
	}
	return bSuccess;
}

void SIRScriptOptimizerProcessTransport::terminateChild( void ) noexcept
{
	if ( m_hChildProcess != nullptr ) {
		DWORD nExit = STILL_ACTIVE;
		if ( GetExitCodeProcess( static_cast<HANDLE>(m_hChildProcess), &nExit ) && nExit == STILL_ACTIVE ) {
			static_cast<void>(TerminateProcess( static_cast<HANDLE>(m_hChildProcess), 1 ));
		}
		static_cast<void>(WaitForSingleObject( static_cast<HANDLE>(m_hChildProcess), 5000 ));
		CloseHandle( static_cast<HANDLE>(m_hChildProcess) );
		m_hChildProcess = nullptr;
	}
}

void SIRScriptOptimizerProcessTransport::closePipes( void ) noexcept
{
	if ( m_hReadPipe != nullptr ) {
		CloseHandle( static_cast<HANDLE>(m_hReadPipe) );
		m_hReadPipe = nullptr;
	}
	if ( m_hWritePipe != nullptr ) {
		CloseHandle( static_cast<HANDLE>(m_hWritePipe) );
		m_hWritePipe = nullptr;
	}
}

bool SIRScriptOptimizerProcessTransport::writeFrame( const uint8_t *pData, uint32_t nBytes ) noexcept
{
	uint32_t nOffset = 0;
	while ( nOffset < nBytes ) {
		DWORD nWritten = 0;
		if ( !WriteFile( static_cast<HANDLE>(m_hWritePipe), pData + nOffset, nBytes - nOffset, &nWritten, nullptr ) || nWritten == 0 ) {
			return false;
		}
		nOffset += nWritten;
	}
	return true;
}

bool SIRScriptOptimizerProcessTransport::readFrame( uint8_t *pData, uint32_t nBytes ) noexcept
{
	uint32_t nOffset = 0;
	while ( nOffset < nBytes ) {
		DWORD nRead = 0;
		if ( !ReadFile( static_cast<HANDLE>(m_hReadPipe), pData + nOffset, nBytes - nOffset, &nRead, nullptr ) || nRead == 0 ) {
			return false;
		}
		nOffset += nRead;
	}
	return true;
}
#endif
