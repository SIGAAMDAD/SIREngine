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

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>

static void sirCheckProcess( bool bCondition, const char *pszMessage )
{
	if ( !bCondition ) {
		std::fprintf( stderr, "Child-process test failed: %s\n", pszMessage );
		std::abort();
	}
}

int main( int argc, char **argv )
{
	if ( argc > 1 ) {
		if ( std::strcmp( argv[1], "--exit-19" ) == 0 ) {
			return 19;
		}
		if ( std::strcmp( argv[1], "--sleep-200" ) == 0 ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
			return 7;
		}
		if ( std::strcmp( argv[1], "--write-marker" ) == 0 ) {
			FILE *pMarker = std::fopen( "child-process-marker.tmp", "wb" );
			if ( pMarker == nullptr ) {
				return 31;
			}
			std::fclose( pMarker );
			return 0;
		}
		if ( std::strcmp( argv[1], "--check-arguments" ) == 0 ) {
			return argc == 4 && std::strcmp( argv[2], "two words" ) == 0 && std::strcmp( argv[3], "quote\"and\\slash" ) == 0 ? 0 : 22;
		}
	}
	const std::string szExecutable = std::filesystem::absolute( argv[0] ).string();
	SIRChildProcess process;
	const char *szExitArgs[] = { "--exit-19" };
	const sirChildProcessLaunch_t exitLaunch = { szExecutable.c_str(), szExitArgs, 1, nullptr };
	sirCheckProcess( process.start( exitLaunch ) == SIR_CHILD_PROCESS_OK, "launch" );
	sirCheckProcess( process.getProcessId() > 0, "process identity" );
	sirCheckProcess( process.start( exitLaunch ) == SIR_CHILD_PROCESS_ALREADY_OPEN, "no duplicate start" );
	uint32_t nExitCode = 0;
	sirCheckProcess( process.wait( &nExitCode ) == SIR_CHILD_PROCESS_OK && nExitCode == 19, "exit code" );
	bool bExited = false;
	sirCheckProcess( process.poll( &bExited, &nExitCode ) == SIR_CHILD_PROCESS_OK && bExited && nExitCode == 19, "cached exit" );
	sirCheckProcess( process.close() == SIR_CHILD_PROCESS_OK, "reap and release" );
	sirCheckProcess( process.wait( nullptr ) == SIR_CHILD_PROCESS_CLOSED, "closed wait" );
	const char *szArgumentArgs[] = { "--check-arguments", "two words", "quote\"and\\slash" };
	const sirChildProcessLaunch_t argumentLaunch = { szExecutable.c_str(), szArgumentArgs, 3, nullptr };
	sirCheckProcess( process.start( argumentLaunch ) == SIR_CHILD_PROCESS_OK, "argument start" );
	sirCheckProcess( process.wait( &nExitCode ) == SIR_CHILD_PROCESS_OK && nExitCode == 0, "argument fidelity" );
	sirCheckProcess( process.close() == SIR_CHILD_PROCESS_OK, "close argument child" );
	const char *szSleepArgs[] = { "--sleep-200" };
	const sirChildProcessLaunch_t sleepLaunch = { szExecutable.c_str(), szSleepArgs, 1, nullptr };
	sirCheckProcess( process.start( sleepLaunch ) == SIR_CHILD_PROCESS_OK, "slow start" );
	sirCheckProcess( process.waitFor( 0, &nExitCode ) == SIR_CHILD_PROCESS_TIMED_OUT, "zero deadline" );
	sirCheckProcess( process.waitFor( 2000, &nExitCode ) == SIR_CHILD_PROCESS_OK && nExitCode == 7, "timed success" );
	sirCheckProcess( process.close() == SIR_CHILD_PROCESS_OK, "close slow child" );
	sirCheckProcess( process.start( sleepLaunch ) == SIR_CHILD_PROCESS_OK, "terminate start" );
	sirCheckProcess( process.terminate() == SIR_CHILD_PROCESS_OK, "terminate and reap" );
	sirCheckProcess( process.close() == SIR_CHILD_PROCESS_OK, "terminate close" );
	const sirChildProcessLaunch_t invalidLaunch = { "/no-such-sir-executable", nullptr, 0, nullptr };
	sirCheckProcess( process.start( invalidLaunch ) == SIR_CHILD_PROCESS_SPAWN_FAILED, "invalid executable" );
	std::filesystem::path pathDirectory = std::filesystem::current_path() / "sir-process-cwd-test";
	std::filesystem::create_directories( pathDirectory );
	const std::string szDirectory = pathDirectory.string();
	const char *szMarkerArgs[] = { "--write-marker" };
	const sirChildProcessLaunch_t cwdLaunch = { szExecutable.c_str(), szMarkerArgs, 1, szDirectory.c_str() };
	sirCheckProcess( process.start( cwdLaunch ) == SIR_CHILD_PROCESS_OK, "working-directory spawn" );
	sirCheckProcess( process.wait( &nExitCode ) == SIR_CHILD_PROCESS_OK && nExitCode == 0, "working-directory child" );
	sirCheckProcess( std::filesystem::exists( pathDirectory / "child-process-marker.tmp" ), "working-directory output" );
	sirCheckProcess( process.close() == SIR_CHILD_PROCESS_OK, "working-directory close" );
	std::filesystem::remove_all( pathDirectory );
	return 0;
}
