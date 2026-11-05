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
#include <Scripting/Core/SIRScriptBindingRegistry.h>

#include <chrono>
#include <stdio.h>
#include <string.h>
#include <thread>

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static sirScriptOptimizerProcessConfig_t restartConfig( const char *pszWorker, const char *pszMode ) noexcept
{
	sirScriptOptimizerProcessConfig_t config = {};
	( void )snprintf( config.szExecutable, sizeof( config.szExecutable ), "%s", pszWorker );
	( void )snprintf( config.szArguments[0], sizeof( config.szArguments[0] ), "%s", pszMode );
	config.nArgumentCount = 1;
	config.nWatchdogMilliseconds = 1000;
	return config;
}

static sirScriptOptimizationRequest_t restartRequest( uint32_t nRevision ) noexcept
{
	sirScriptOptimizationRequest_t request = {};
	request.artifact = { 1, 2, nRevision, 4, 5, 6, 7 };
	memset( request.key.szDigest, ( int )nRevision, sizeof( request.key.szDigest ) );
	request.nInputBytes = 128;
	request.eTier = SIR_SCRIPT_OPT_BASELINE;
	return request;
}

static bool testCrashAndRestart( const char *pszWorker ) noexcept
{
	SIR_TEST_CHECK( pszWorker != nullptr && strlen( pszWorker ) < sizeof( sirScriptOptimizerProcessConfig_t::szExecutable ) );
	SIRScriptOptimizerProcessTransport transport;
	SIR_TEST_CHECK( transport.configure( restartConfig( pszWorker, "truncate" ) ) == SIR_SCRIPT_OK );
	SIRScriptCachedProgramSource source;
	SIRScriptBindingRegistry registry;
	SIRScriptCodeManager manager( &registry );
	SIRScriptOptimizationCoordinator coordinator;
	SIR_TEST_CHECK( coordinator.configure( transport.getAPI(), &source, &manager ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );

	sirScriptOptimizationJobHandle_t hDispatched = {};
	sirScriptOptimizationJobHandle_t hPending = {};
	SIR_TEST_CHECK( coordinator.submit( restartRequest( 11 ), false, &hDispatched ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.submit( restartRequest( 12 ), false, &hPending ) == SIR_SCRIPT_OK );
	sirScriptOptimizationPumpStats_t stats = {};
	SIR_TEST_CHECK( coordinator.pump( 1, 0, &stats ) == SIR_SCRIPT_OK && stats.nSent == 1 );
	const auto nDeadline = std::chrono::steady_clock::now() + std::chrono::seconds( 4 );
	bool bFailed = false;
	while ( std::chrono::steady_clock::now() < nDeadline ) {
		const sirScriptResult_t ePump = coordinator.pump( 0, 1, &stats );
		if ( ePump == SIR_SCRIPT_BACKEND_FAILURE ) {
			bFailed = true;
			break;
		}
		SIR_TEST_CHECK( ePump == SIR_SCRIPT_OK );
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	SIR_TEST_CHECK( bFailed && transport.hasFailed() );
	sirScriptOptimizationJobInfo_t info = {};
	SIR_TEST_CHECK( coordinator.query( hDispatched, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_FAILED &&
	                info.eResult == SIR_SCRIPT_BACKEND_FAILURE && !info.bAwaitingReply );
	const uint64_t nOriginalId = info.nJobId;
	SIR_TEST_CHECK( coordinator.query( hPending, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_QUEUED &&
	                info.nJobId == nOriginalId + 1 );

	SIR_TEST_CHECK( coordinator.retryFailed( hDispatched ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.query( hDispatched, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_QUEUED &&
	                info.nJobId > nOriginalId );
	SIR_TEST_CHECK( transport.configure( restartConfig( pszWorker, "normal" ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 2, 0, &stats ) == SIR_SCRIPT_OK && stats.nSent == 2 );

	const auto nReplyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds( 4 );
	bool bBothCompleted = false;
	while ( std::chrono::steady_clock::now() < nReplyDeadline ) {
		SIR_TEST_CHECK( coordinator.pump( 0, 2, &stats ) == SIR_SCRIPT_OK );
		sirScriptOptimizationJobInfo_t dispatched = {};
		sirScriptOptimizationJobInfo_t pending = {};
		SIR_TEST_CHECK( coordinator.query( hDispatched, &dispatched ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( coordinator.query( hPending, &pending ) == SIR_SCRIPT_OK );
		if ( dispatched.eState == SIR_SCRIPT_JOB_FAILED && pending.eState == SIR_SCRIPT_JOB_FAILED ) {
			SIR_TEST_CHECK( dispatched.eResult == SIR_SCRIPT_BACKEND_FAILURE && pending.eResult == SIR_SCRIPT_BACKEND_FAILURE );
			bBothCompleted = true;
			break;
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	SIR_TEST_CHECK( bBothCompleted && !transport.hasFailed() );
	SIR_TEST_CHECK( coordinator.release( hDispatched ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.release( hPending ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.shutdown() == SIR_SCRIPT_OK );
	return true;
}

int main( int nArguments, char **pszArguments )
{
	if ( nArguments != 2 || !testCrashAndRestart( pszArguments[1] ) ) {
		return 1;
	}
	puts( "optimizer subprocess crash/restart: PASS" );
	return 0;
}
