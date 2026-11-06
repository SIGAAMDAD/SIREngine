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

static sirScriptOptimizerProcessConfig_t fixtureConfig( const char *pszExecutable, const char *pszMode, uint32_t nWatchdog ) noexcept
{
	sirScriptOptimizerProcessConfig_t config = {};
	( void )snprintf( config.szExecutable, sizeof( config.szExecutable ), "%s", pszExecutable );
	( void )snprintf( config.szArguments[0], sizeof( config.szArguments[0] ), "%s", pszMode );
	config.nArgumentCount = 1;
	config.nWatchdogMilliseconds = nWatchdog;
	return config;
}

static sirScriptOptimizationRequest_t fixtureRequest( uint64_t nJobId ) noexcept
{
	sirScriptOptimizationRequest_t request = {};
	request.nJobId = nJobId;
	request.artifact = { 1, 2, 3, 4, 5, 6, 7 };
	memset( request.key.szDigest, 77, sizeof( request.key.szDigest ) );
	request.nInputBytes = 128;
	request.eTier = SIR_SCRIPT_OPT_BASELINE;
	return request;
}

static bool waitReply( SIRScriptOptimizerProcessTransport *pTransport, uint64_t nJobId ) noexcept
{
	const auto until = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
	uint8_t szReply[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
	for ( ; std::chrono::steady_clock::now() < until; ) {
		uint32_t nReceived = 0;
		const sirScriptResult_t eResult = pTransport->receive( szReply, sizeof( szReply ), &nReceived );
		if ( eResult == SIR_SCRIPT_OK ) {
			if ( nReceived != sizeof( szReply ) ) {
				return false;
			}
			sirScriptOptimizationReply_t reply = {};
			return SIRScriptOptimizationProtocol::decodeReply( szReply, nReceived, &reply ) == SIR_SCRIPT_OK && reply.nJobId == nJobId &&
			       reply.eResult == SIR_SCRIPT_BACKEND_FAILURE;
		}
		if ( eResult != SIR_SCRIPT_BUSY ) {
			return false;
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	return false;
}

static bool testConfiguration( const char *pszExecutable ) noexcept
{
	SIRScriptOptimizerProcessTransport transport;
	SIR_TEST_CHECK( transport.start() == SIR_SCRIPT_WRONG_STATE );
	sirScriptOptimizerProcessConfig_t config = fixtureConfig( pszExecutable, "normal", 0 );
	config.nArgumentCount = SIR_SCRIPT_OPT_MAX_ARGUMENTS + 1;
	SIR_TEST_CHECK( transport.configure( config ) == SIR_SCRIPT_INVALID_ARGUMENT );
	memset( config.szExecutable, 'x', sizeof( config.szExecutable ) );
	config.nArgumentCount = 0;
	SIR_TEST_CHECK( transport.configure( config ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( transport.configure( fixtureConfig( "/a/path/that/is/not/a/real/worker", "normal", 0 ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.start() == SIR_SCRIPT_BACKEND_FAILURE );
	SIR_TEST_CHECK( transport.configure( fixtureConfig( pszExecutable, "normal", 0 ) ) == SIR_SCRIPT_OK );
	return true;
}

static bool testFramedTransport( const char *pszExecutable ) noexcept
{
	SIRScriptOptimizerProcessTransport transport;
	SIR_TEST_CHECK( transport.configure( fixtureConfig( pszExecutable, "split", 2000 ) ) == SIR_SCRIPT_OK );
	const sirScriptOptimizationTransportAPI_t api = transport.getAPI();
	SIR_TEST_CHECK( api.nABIRevision == SIR_SCRIPT_OPT_TRANSPORT_ABI_REVISION );
	SIR_TEST_CHECK( api.pStart( api.pUser ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( api.pStart( api.pUser ) == SIR_SCRIPT_WRONG_STATE );
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( fixtureRequest( 11 ), szWire ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( api.pSend( api.pUser, szWire, sizeof( szWire ) - 1 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( api.pSend( api.pUser, szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK );
	sirScriptResult_t eCrossThread = SIR_SCRIPT_OK;
	std::thread thread( [&] { eCrossThread = transport.send( szWire, sizeof( szWire ) ); } );
	thread.join();
	SIR_TEST_CHECK( eCrossThread == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( waitReply( &transport, 11 ) );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( fixtureRequest( 12 ), szWire ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( waitReply( &transport, 12 ) );
	api.pStop( api.pUser );
	SIR_TEST_CHECK( !transport.isRunning() );
	SIR_TEST_CHECK( transport.start() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( fixtureRequest( 13 ), szWire ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( waitReply( &transport, 13 ) );

	// Force CR, LF and Ctrl-Z in both request and reply IDs. Windows CRT
	// text-mode pipes otherwise rewrite or truncate these wire messages.
	constexpr uint64_t BINARY_CONTROL_JOB_ID = UINT64_C( 0x1A0A0D0A );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( fixtureRequest( BINARY_CONTROL_JOB_ID ), szWire ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( waitReply( &transport, BINARY_CONTROL_JOB_ID ) );
	transport.stop();
	return true;
}

static bool testProcessFailures( const char *pszExecutable ) noexcept
{
	SIRScriptOptimizerProcessTransport transport;
	sirScriptOptimizerProcessConfig_t config = fixtureConfig( pszExecutable, "exit", 500 );
	SIR_TEST_CHECK( transport.configure( config ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.start() == SIR_SCRIPT_OK );
	const auto until = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
	bool bFailed = false;
	for ( ; std::chrono::steady_clock::now() < until; ) {
		uint8_t szBuffer[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
		uint32_t nReceived = 0;
		if ( transport.receive( szBuffer, sizeof( szBuffer ), &nReceived ) == SIR_SCRIPT_BACKEND_FAILURE ) {
			bFailed = true;
			break;
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	SIR_TEST_CHECK( bFailed );
	transport.stop();
	SIR_TEST_CHECK( transport.configure( fixtureConfig( pszExecutable, "truncate", 500 ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.start() == SIR_SCRIPT_OK );
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( fixtureRequest( 21 ), szWire ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK );
	bFailed = false;
	const auto nSecondDeadline = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
	for ( auto now = std::chrono::steady_clock::now(); now < nSecondDeadline; now = std::chrono::steady_clock::now() ) {
		uint8_t szReply[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
		uint32_t nBytes = 0;
		if ( transport.receive( szReply, sizeof( szReply ), &nBytes ) == SIR_SCRIPT_BACKEND_FAILURE ) {
			bFailed = true;
			break;
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	SIR_TEST_CHECK( bFailed );
	transport.stop();
	return true;
}

static bool testWatchdog( const char *pszExecutable ) noexcept
{
	SIRScriptOptimizerProcessTransport transport;
	SIR_TEST_CHECK( transport.configure( fixtureConfig( pszExecutable, "stall", 120 ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.start() == SIR_SCRIPT_OK );
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( fixtureRequest( 31 ), szWire ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK );
	const auto until = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
	bool bTimedOut = false;
	for ( ; std::chrono::steady_clock::now() < until; ) {
		uint8_t szReply[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
		uint32_t nReceived = 0;
		if ( transport.receive( szReply, sizeof( szReply ), &nReceived ) == SIR_SCRIPT_BACKEND_FAILURE ) {
			bTimedOut = true;
			break;
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
	}
	SIR_TEST_CHECK( bTimedOut );
	SIR_TEST_CHECK( transport.hasFailed() );
	transport.stop();
	return true;
}

static bool testCoordinator( const char *pszExecutable ) noexcept
{
	SIRScriptOptimizerProcessTransport transport;
	SIR_TEST_CHECK( transport.configure( fixtureConfig( pszExecutable, "normal", 2000 ) ) == SIR_SCRIPT_OK );
	SIRScriptCachedProgramSource source;
	SIRScriptBindingRegistry registry;
	SIRScriptCodeManager manager( &registry );
	SIRScriptOptimizationCoordinator coordinator;
	SIR_TEST_CHECK( coordinator.configure( transport.getAPI(), &source, &manager ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );
	sirScriptOptimizationJobHandle_t job = {};
	SIR_TEST_CHECK( coordinator.submit( fixtureRequest( 0 ), false, &job ) == SIR_SCRIPT_OK );
	const auto until = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
	bool bComplete = false;
	for ( ; std::chrono::steady_clock::now() < until; ) {
		sirScriptOptimizationPumpStats_t stats = {};
		const sirScriptResult_t ePump = coordinator.pump( 1, 1, &stats );
		if ( ePump != SIR_SCRIPT_OK ) {
			fprintf( stderr, "optimizer coordinator pump: %u, transport failed: %u, sent: %u, received: %u, failures: %u\n",
			         ( unsigned )ePump, ( unsigned )transport.hasFailed(), stats.nSent, stats.nReceived, stats.nFailed );
		}
		SIR_TEST_CHECK( ePump == SIR_SCRIPT_OK );
		sirScriptOptimizationJobInfo_t info = {};
		SIR_TEST_CHECK( coordinator.query( job, &info ) == SIR_SCRIPT_OK );
		if ( info.eState == SIR_SCRIPT_JOB_FAILED ) {
			SIR_TEST_CHECK( info.eResult == SIR_SCRIPT_BACKEND_FAILURE );
			bComplete = true;
			break;
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	SIR_TEST_CHECK( bComplete );
	SIR_TEST_CHECK( coordinator.release( job ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.shutdown() == SIR_SCRIPT_OK );
	return true;
}

static bool testStopCancelsBlockedIO( const char *pszExecutable ) noexcept
{
	SIRScriptOptimizerProcessTransport transport;
	for ( uint32_t nAttempt = 0; nAttempt < 3; ++nAttempt ) {
		SIR_TEST_CHECK( transport.configure( fixtureConfig( pszExecutable, "stall", 0 ) ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( transport.start() == SIR_SCRIPT_OK );
		uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
		SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( fixtureRequest( 41 + nAttempt ), szWire ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK );
		std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
		const auto started = std::chrono::steady_clock::now();
		transport.stop();
		SIR_TEST_CHECK( std::chrono::steady_clock::now() - started < std::chrono::seconds( 5 ) );
	}
	return true;
}

int main( int nArguments, char **pszArguments )
{
	if ( nArguments < 2 ) {
		fprintf( stderr, "Expected subprocess fixture executable path\n" );
		return 2;
	}
	if ( !testConfiguration( pszArguments[1] ) || !testFramedTransport( pszArguments[1] ) || !testProcessFailures( pszArguments[1] ) ||
	     !testWatchdog( pszArguments[1] ) || !testStopCancelsBlockedIO( pszArguments[1] ) || !testCoordinator( pszArguments[1] ) ) {
		return 1;
	}
	fprintf( stdout, "optimizer process transport tests passed\n" );
	return 0;
}
