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

#include <chrono>
#include <stdio.h>
#include <string.h>
#include <thread>

#define SIR_TEST_CHECK( condition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool waitForReply( SIRScriptOptimizerProcessTransport *pTransport, uint64_t nExpectedId ) noexcept
{
	const auto nDeadline = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
	while ( std::chrono::steady_clock::now() < nDeadline ) {
		uint8_t szWire[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
		uint32_t nReceived = 0;
		const sirScriptResult_t eResult = pTransport->receive( szWire, sizeof( szWire ), &nReceived );
		if ( eResult == SIR_SCRIPT_OK ) {
			sirScriptOptimizationReply_t reply = {};
			return nReceived == sizeof( szWire ) &&
			       SIRScriptOptimizationProtocol::decodeReply( szWire, nReceived, &reply ) == SIR_SCRIPT_OK &&
			       reply.nJobId == nExpectedId && reply.eResult == SIR_SCRIPT_BACKEND_FAILURE;
		}
		if ( eResult != SIR_SCRIPT_BUSY ) {
			return false;
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	return false;
}

static bool testOutstandingWindow( const char *pszWorker ) noexcept
{
	sirScriptOptimizerProcessConfig_t config = {};
	if ( snprintf( config.szExecutable, sizeof( config.szExecutable ), "%s", pszWorker ) <= 0 ||
	     strlen( pszWorker ) >= sizeof( config.szExecutable ) ) {
		return false;
	}
	memcpy( config.szArguments[0], "normal", sizeof( "normal" ) );
	config.nArgumentCount = 1;
	config.nWatchdogMilliseconds = 0;

	SIRScriptOptimizerProcessTransport transport;
	SIR_TEST_CHECK( transport.configure( config ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.start() == SIR_SCRIPT_OK );

	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	sirScriptOptimizationRequest_t request = {};
	request.artifact = { 1, 2, 3, 4, 5, 6, 7 };
	memset( request.key.szDigest, 0x55, sizeof( request.key.szDigest ) );
	request.nInputBytes = 128;
	request.eTier = SIR_SCRIPT_OPT_BASELINE;

	// Not receiving any replies keeps all sent work in flight even when the
	// background writer has already drained its small pending request queue.
	for ( uint32_t iJob = 1; iJob <= SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES; ++iJob ) {
		request.nJobId = iJob;
		SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( request, szWire ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK );
	}
	request.nJobId = SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES + 1;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( request, szWire ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_BUSY );

	// Consuming one reply frees exactly one slot in the outstanding window.
	SIR_TEST_CHECK( waitForReply( &transport, 1 ) );
	SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_BUSY );
	for ( uint32_t iJob = 2; iJob <= SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES + 1; ++iJob ) {
		SIR_TEST_CHECK( waitForReply( &transport, iJob ) );
	}

	transport.stop();
	return true;
}

int main( int nArgc, char **ppszArgv )
{
	if ( nArgc != 2 || !testOutstandingWindow( ppszArgv[1] ) ) {
		return 1;
	}
	puts( "optimizer outstanding window: PASS" );
	return 0;
}
