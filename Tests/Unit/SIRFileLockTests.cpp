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

#include <Core/Platform/SIRFileLock.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <stdio.h>

#if defined( __linux__ )
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static bool testFileLockThreadContention( void )
{
	const std::filesystem::path root = std::filesystem::temp_directory_path() /
	                                   ( "sir-file-lock-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	if ( !std::filesystem::create_directory( root ) ) {
		return false;
	}
	const std::string szLockPath = ( root / "shared.lock" ).string();
	SIRFileLock invalid( nullptr );
	SIRFileLock empty( "" );
	if ( invalid.isLocked() || empty.isLocked() ) {
		return false;
	}

	std::atomic<bool> bAttempting{ false };
	std::atomic<bool> bAcquired{ false };
	std::thread competing;
	bool bPremature = false;
	{
		SIRFileLock first( szLockPath.c_str() );
		if ( !first.isLocked() ) {
			return false;
		}
		competing = std::thread( [&] {
			bAttempting.store( true, std::memory_order_release );
			SIRFileLock second( szLockPath.c_str() );
			bAcquired.store( second.isLocked(), std::memory_order_release );
		} );
		while ( !bAttempting.load( std::memory_order_acquire ) ) {
			std::this_thread::yield();
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 40 ) );
		bPremature = bAcquired.load( std::memory_order_acquire );
	}
	competing.join();
	const bool bSuccess = !bPremature && bAcquired.load( std::memory_order_acquire );
	const bool bRemoved = std::filesystem::remove_all( root ) > 0;
	return bSuccess && bRemoved;
}

#if defined( __linux__ )
static bool testFileLockProcessContention( void )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-file-lock-process-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	if ( !std::filesystem::create_directory( root ) ) {
		return false;
	}
	const std::string szLockPath = ( root / "shared.lock" ).string();
	int szPipe[2] = { -1, -1 };
	if ( pipe( szPipe ) != 0 ) {
		return false;
	}

	pid_t nChild = -1;
	int nPremature = -1;
	{
		SIRFileLock parent( szLockPath.c_str() );
		if ( !parent.isLocked() ) {
			( void )close( szPipe[0] );
			( void )close( szPipe[1] );
			return false;
		}
		nChild = fork();
		if ( nChild == 0 ) {
			( void )close( szPipe[0] );
			SIRFileLock child( szLockPath.c_str() );
			const uint8_t nReady = child.isLocked() ? 1 : 0;
			const bool bSent = write( szPipe[1], &nReady, sizeof( nReady ) ) == sizeof( nReady );
			_exit( bSent && nReady == 1 ? 0 : 1 );
		}
		( void )close( szPipe[1] );
		if ( nChild < 0 ) {
			( void )close( szPipe[0] );
			return false;
		}
		pollfd record = { szPipe[0], POLLIN, 0 };
		nPremature = poll( &record, 1, 100 );
	}

	pollfd record = { szPipe[0], POLLIN, 0 };
	const int nReady = poll( &record, 1, 3000 );
	uint8_t nByte = 0;
	const bool bRead = nReady == 1 && read( szPipe[0], &nByte, sizeof( nByte ) ) == sizeof( nByte );
	( void )close( szPipe[0] );
	int nStatus = 0;
	const bool bExited = waitpid( nChild, &nStatus, 0 ) == nChild && WIFEXITED( nStatus ) && WEXITSTATUS( nStatus ) == 0;
	const bool bRemoved = std::filesystem::remove_all( root ) > 0;
	return nPremature == 0 && bRead && nByte == 1 && bExited && bRemoved;
}
#endif

int main( void )
{
	if ( !testFileLockThreadContention() ) {
		fprintf( stderr, "SIRFileLock thread contention: FAIL\n" );
		return 1;
	}
#if defined( __linux__ )
	if ( !testFileLockProcessContention() ) {
		fprintf( stderr, "SIRFileLock process contention: FAIL\n" );
		return 1;
	}
#endif
	puts( "SIRFileLock: PASS" );
	return 0;
}
