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

#include <Scripting/Core/SIRScriptOptimizerWorker.h>
#include <Scripting/Core/SIRScriptOptimizationRecipe.h>
#include <Scripting/Core/SIRScriptSHA256.h>
#include <Scripting/Core/SIRScriptOptimizerProcessTransport.h>

#include <chrono>
#include <filesystem>
#include <stdio.h>
#include <string.h>
#include <string>
#include <thread>

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static sirScriptOptimizationRequest_t testMakeRequest( const void *pInput, uint64_t nBytes, uint64_t nJobId,
                                                       sirScriptOptimizerRecipe_t *pRecipe ) noexcept
{
	sirScriptOptimizationRequest_t request = {};
	request.nJobId = nJobId;
	request.artifact = { 41, 99, 1, 4, 52, 74, 21 };
	request.nInputBytes = nBytes;
	request.eTier = SIR_SCRIPT_OPT_TARGET_CPU;
	sirScriptOptimizerRecipeIngredients_t ingredients = {};
	static constexpr char szDependencies[] = "full-dependency-closure-v1";
	static constexpr char szFrontend[] = "frontend-and-compiler-version-v1";
	static constexpr char szOptimizer[] = "optimizer-toolchain-v1";
	static constexpr char szOptions[] = "target-feature-and-optimization-options-v1";
	SIRScriptSHA256::digest( szDependencies, sizeof( szDependencies ), ingredients.szDependencies );
	SIRScriptSHA256::digest( szFrontend, sizeof( szFrontend ), ingredients.szFrontend );
	SIRScriptSHA256::digest( szOptimizer, sizeof( szOptimizer ), ingredients.szOptimizer );
	SIRScriptSHA256::digest( szOptions, sizeof( szOptions ), ingredients.szOptions );
	( void )SIRScriptOptimizationRecipe::build( request, ingredients, pInput, pRecipe );
	request.key = pRecipe->key;
	return request;
}

static bool testReceive( SIRScriptOptimizerProcessTransport *pTransport, sirScriptOptimizationReply_t *pReply )
{
	for ( uint32_t i = 0; i < 1000; ++i ) {
		uint8_t szWire[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
		uint32_t nRead = 0;
		const sirScriptResult_t eResult = pTransport->receive( szWire, sizeof( szWire ), &nRead );
		if ( eResult == SIR_SCRIPT_OK ) {
			return SIRScriptOptimizationProtocol::decodeReply( szWire, nRead, pReply ) == SIR_SCRIPT_OK;
		}
		if ( eResult != SIR_SCRIPT_BUSY ) {
			return false;
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
	}
	return false;
}

static bool testSend( SIRScriptOptimizerProcessTransport *pTransport, const sirScriptOptimizationRequest_t& request )
{
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	return SIRScriptOptimizationProtocol::encodeRequest( request, szWire ) == SIR_SCRIPT_OK &&
	       pTransport->send( szWire, sizeof( szWire ) ) == SIR_SCRIPT_OK;
}

static bool testWorkerPipeline( const char *pszWorker, const char *pszBackend )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-opt-real-worker-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	const std::filesystem::path inputRoot = root / "inputs";
	const std::filesystem::path cacheRoot = root / "cache";
	SIR_TEST_CHECK( std::filesystem::create_directories( inputRoot ) );
	SIR_TEST_CHECK( std::filesystem::create_directories( cacheRoot ) );

	const std::string szInputRoot = inputRoot.string();
	const std::string szCacheRoot = cacheRoot.string();
	SIRScriptOptimizerInputStore inputs;
	SIRScriptArtifactCache cache;
	SIR_TEST_CHECK( inputs.configure( szInputRoot.c_str(), 4096, 1024 ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.configure( szCacheRoot.c_str(), 4096 ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( inputs.configure( szInputRoot.c_str(), 4096, 1024 ) == SIR_SCRIPT_WRONG_STATE );

	uint8_t szInput[128] = {};
	for ( uint32_t i = 0; i < sizeof( szInput ); ++i ) {
		szInput[i] = ( uint8_t )i;
	}
	sirScriptOptimizerRecipe_t canonical = {};
	sirScriptOptimizationRequest_t request = testMakeRequest( szInput, sizeof( szInput ), 1, &canonical );
	SIR_TEST_CHECK( inputs.stage( request, canonical.szBytes, sizeof( canonical.szBytes ), szInput ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( inputs.stage( request, canonical.szBytes, sizeof( canonical.szBytes ), szInput ) == SIR_SCRIPT_DUPLICATE );

	sirScriptOptimizerInputBuffers_t buffers = {};
	uint8_t szRecipeRead[512] = {};
	uint8_t szInputRead[128] = {};
	buffers = { szRecipeRead, sizeof( szRecipeRead ), szInputRead, sizeof( szInputRead ), 0 };
	SIR_TEST_CHECK( inputs.fetch( request, &buffers ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( buffers.nRecipeBytes == sizeof( canonical.szBytes ) );
	SIR_TEST_CHECK( memcmp( szRecipeRead, canonical.szBytes, buffers.nRecipeBytes ) == 0 );
	SIR_TEST_CHECK( memcmp( szInputRead, szInput, sizeof( szInput ) ) == 0 );
	buffers.nInputCapacity = 4;
	SIR_TEST_CHECK( inputs.fetch( request, &buffers ) == SIR_SCRIPT_INVALID_ARGUMENT );
	buffers.nInputCapacity = sizeof( szInputRead );

	uint8_t szChanged[128] = {};
	memcpy( szChanged, szInput, sizeof( szChanged ) );
	szChanged[5] ^= 0xf1;
	SIR_TEST_CHECK( inputs.stage( request, canonical.szBytes, sizeof( canonical.szBytes ), szChanged ) == SIR_SCRIPT_TYPE_MISMATCH );
	sirScriptOptimizationRequest_t mismatch = request;
	++mismatch.artifact.nTargetId;
	SIR_TEST_CHECK( inputs.inspect( mismatch, &buffers.nRecipeBytes ) == SIR_SCRIPT_TYPE_MISMATCH );

	SIRScriptOptimizerProcessTransport transport;
	sirScriptOptimizerProcessConfig_t config = {};
	SIR_TEST_CHECK( strlen( pszWorker ) < sizeof( config.szExecutable ) );
	strcpy( config.szExecutable, pszWorker );
	SIR_TEST_CHECK( szInputRoot.length() < sizeof( config.szArguments[0] ) );
	SIR_TEST_CHECK( szCacheRoot.length() < sizeof( config.szArguments[1] ) );
	SIR_TEST_CHECK( strlen( pszBackend ) < sizeof( config.szArguments[2] ) );
	strcpy( config.szArguments[0], szInputRoot.c_str() );
	strcpy( config.szArguments[1], szCacheRoot.c_str() );
	strcpy( config.szArguments[2], pszBackend );
	config.nArgumentCount = 3;
	config.nWatchdogMilliseconds = 3000;
	SIR_TEST_CHECK( transport.configure( config ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.start() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( testSend( &transport, request ) );
	sirScriptOptimizationReply_t reply = {};
	SIR_TEST_CHECK( testReceive( &transport, &reply ) );
	SIR_TEST_CHECK( reply.eResult == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::validateCompletion( request, reply ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( reply.cached.nBytes == sizeof( szInput ) && reply.cached.nAlignment == 64 );
	alignas( 64 ) uint8_t szOutput[128] = {};
	SIR_TEST_CHECK( cache.fetch( reply.cached, szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_OK );
	for ( uint32_t i = 0; i < sizeof( szOutput ); ++i ) {
		SIR_TEST_CHECK( szOutput[i] == ( uint8_t )( szInput[sizeof( szInput ) - 1 - i] ^ 0x5a ) );
	}
	// The same recipe is reusable under a different IPC job identity.
	request.nJobId = 2;
	SIR_TEST_CHECK( testSend( &transport, request ) );
	SIR_TEST_CHECK( testReceive( &transport, &reply ) );
	SIR_TEST_CHECK( reply.nJobId == 2 && reply.eResult == SIR_SCRIPT_OK );

	// An unstaged recipe must return a terminal job failure without killing the process.
	sirScriptOptimizerRecipe_t absentRecipe = {};
	uint8_t szAbsent[128] = {};
	memset( szAbsent, 0x77, sizeof( szAbsent ) );
	sirScriptOptimizationRequest_t absent = testMakeRequest( szAbsent, sizeof( szAbsent ), 3, &absentRecipe );
	SIR_TEST_CHECK( testSend( &transport, absent ) );
	SIR_TEST_CHECK( testReceive( &transport, &reply ) );
	SIR_TEST_CHECK( reply.nJobId == 3 && reply.eResult == SIR_SCRIPT_NOT_FOUND );

	// A damaged input must be rejected before the compiler backend executes.
	for ( const auto& entry : std::filesystem::directory_iterator( inputRoot ) ) {
		const std::filesystem::path path = entry.path();
		if ( path.extension() == ".sirinput" ) {
			FILE *pFile = fopen( path.string().c_str(), "r+b" );
			SIR_TEST_CHECK( pFile != nullptr );
			SIR_TEST_CHECK( fseek( pFile, -1, SEEK_END ) == 0 );
			uint8_t nCorrupt = 0xde;
			SIR_TEST_CHECK( fwrite( &nCorrupt, 1, 1, pFile ) == 1 );
			SIR_TEST_CHECK( fclose( pFile ) == 0 );
		}
	}
	request.nJobId = 4;
	SIR_TEST_CHECK( testSend( &transport, request ) );
	SIR_TEST_CHECK( testReceive( &transport, &reply ) );
	SIR_TEST_CHECK( reply.nJobId == 4 && reply.eResult == SIR_SCRIPT_TYPE_MISMATCH );
	// A corrupt record is repaired by an idempotent staging call with exact source.
	SIR_TEST_CHECK( inputs.stage( request, canonical.szBytes, sizeof( canonical.szBytes ), szInput ) == SIR_SCRIPT_OK );
	request.nJobId = 5;
	SIR_TEST_CHECK( testSend( &transport, request ) );
	SIR_TEST_CHECK( testReceive( &transport, &reply ) );
	SIR_TEST_CHECK( reply.nJobId == 5 && reply.eResult == SIR_SCRIPT_OK );

	// Compilation failure must not publish an artifact and must not kill the process.
	uint8_t szCompileFailure[128] = {};
	memcpy( szCompileFailure, szInput, sizeof( szInput ) );
	szCompileFailure[0] = 0xff;
	sirScriptOptimizerRecipe_t failureRecipe = {};
	sirScriptOptimizationRequest_t failure = testMakeRequest( szCompileFailure, sizeof( szCompileFailure ), 6, &failureRecipe );
	SIR_TEST_CHECK( inputs.stage( failure, failureRecipe.szBytes, sizeof( failureRecipe.szBytes ), szCompileFailure ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( testSend( &transport, failure ) );
	SIR_TEST_CHECK( testReceive( &transport, &reply ) );
	SIR_TEST_CHECK( reply.nJobId == 6 && reply.eResult == SIR_SCRIPT_BACKEND_FAILURE );
	SIR_TEST_CHECK( cache.erase( failure.key ) == SIR_SCRIPT_NOT_FOUND );
	SIR_TEST_CHECK( inputs.erase( failure.key ) == SIR_SCRIPT_OK );

	sirScriptResult_t eOtherThread = SIR_SCRIPT_OK;
	std::thread workerThread( [&inputs, &request, &eOtherThread] {
		uint64_t nRecipeBytes = 0;
		eOtherThread = inputs.inspect( request, &nRecipeBytes );
	} );
	workerThread.join();
	SIR_TEST_CHECK( eOtherThread == SIR_SCRIPT_BUSY );
	transport.stop();
	SIR_TEST_CHECK( inputs.erase( request.key ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.erase( request.key ) == SIR_SCRIPT_OK );
	std::filesystem::remove_all( root );
	return true;
}

int main( int nArguments, char **pszArguments )
{
	if ( nArguments != 3 ) {
		return 2;
	}
	if ( !testWorkerPipeline( pszArguments[1], pszArguments[2] ) ) {
		return 1;
	}
	puts( "SIREngine optimizer worker PASS" );
	return 0;
}
