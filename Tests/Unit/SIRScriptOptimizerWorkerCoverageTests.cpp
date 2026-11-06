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

#include <chrono>
#include <filesystem>
#include <stdio.h>
#include <string.h>
#include <string>

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirScriptWorkerCoverageState_s
{
	alignas( 64 ) uint8_t szOutput[128];
	SIRScriptOptimizerWorker *pWorker;
	const sirScriptOptimizationRequest_t *pRequest;
	sirScriptResult_t eReentrantResult;
	uint32_t nMode;
	uint32_t nCompiles;
	uint32_t nReleases;
} sirScriptWorkerCoverageState_t;

static sirScriptResult_t coverageCompile( void *pUser, const sirScriptOptimizerWorkView_t *pView,
                                          sirScriptOptimizerOutput_t *pOutput ) noexcept
{
	sirScriptWorkerCoverageState_t *pState = ( sirScriptWorkerCoverageState_t * )pUser;
	++pState->nCompiles;
	if ( pState->nMode == 6 ) {
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( pState->nMode == 9 ) {
		sirScriptOptimizationReply_t nested = {};
		pState->eReentrantResult = pState->pWorker->process( *pState->pRequest, &nested );
	}
	for ( uint32_t i = 0; i < pView->nInputBytes; ++i ) {
		pState->szOutput[i] = ( ( const uint8_t * )pView->pInput )[i] ^ 0x5a;
	}
	*pOutput = { pState->szOutput, pView->nInputBytes, 64 };
	if ( pState->nMode == 1 ) {
		pOutput->nBytes = 0;
	} else if ( pState->nMode == 2 ) {
		pOutput->nBytes = pView->nInputBytes + 1;
	} else if ( pState->nMode == 3 ) {
		pOutput->nAlignment = 3;
	} else if ( pState->nMode == 4 ) {
		pOutput->pBytes = nullptr;
	} else if ( pState->nMode == 5 ) {
		pOutput->pBytes = pState->szOutput + 1;
	} else if ( pState->nMode == 7 ) {
		pOutput->nAlignment = 0;
	} else if ( pState->nMode == 8 ) {
		pOutput->nAlignment = 8192;
	}
	return SIR_SCRIPT_OK;
}

static void coverageRelease( void *pUser, sirScriptOptimizerOutput_t *pOutput ) noexcept
{
	sirScriptWorkerCoverageState_t *pState = ( sirScriptWorkerCoverageState_t * )pUser;
	++pState->nReleases;
	*pOutput = {};
}

static sirScriptOptimizationRequest_t coverageRequest( const uint8_t szInput[128], uint64_t nJobId,
                                                       sirScriptOptimizerRecipe_t *pRecipe ) noexcept
{
	sirScriptOptimizationRequest_t request = {};
	request.nJobId = nJobId;
	request.artifact = { 41, 99, 1, 4, 52, 74, 21 };
	request.nInputBytes = 128;
	request.eTier = SIR_SCRIPT_OPT_TARGET_CPU;
	sirScriptOptimizerRecipeIngredients_t ingredients = {};
	static constexpr char szDependencies[] = "dependency-closure-worker-coverage";
	static constexpr char szFrontend[] = "frontend-worker-coverage";
	static constexpr char szOptimizer[] = "optimizer-worker-coverage";
	static constexpr char szOptions[] = "options-worker-coverage";
	SIRScriptSHA256::digest( szDependencies, sizeof( szDependencies ), ingredients.szDependencies );
	SIRScriptSHA256::digest( szFrontend, sizeof( szFrontend ), ingredients.szFrontend );
	SIRScriptSHA256::digest( szOptimizer, sizeof( szOptimizer ), ingredients.szOptimizer );
	SIRScriptSHA256::digest( szOptions, sizeof( szOptions ), ingredients.szOptions );
	if ( SIRScriptOptimizationRecipe::build( request, ingredients, szInput, pRecipe ) == SIR_SCRIPT_OK ) {
		request.key = pRecipe->key;
	}
	return request;
}

static bool testDirectWorker( void )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-opt-direct-coverage-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
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

	uint8_t szInput[128] = {};
	for ( uint32_t i = 0; i < sizeof( szInput ); ++i ) {
		szInput[i] = ( uint8_t )i;
	}
	sirScriptOptimizerRecipe_t recipe = {};
	sirScriptOptimizationRequest_t request = coverageRequest( szInput, 1, &recipe );
	SIR_TEST_CHECK( inputs.stage( request, recipe.szBytes, sizeof( recipe.szBytes ), szInput ) == SIR_SCRIPT_OK );

	sirScriptWorkerCoverageState_t state = {};
	sirScriptOptimizerBackendAPI_t backend = { SIR_SCRIPT_OPT_BACKEND_ABI_REVISION, &state, coverageCompile, coverageRelease };
	SIRScriptOptimizerWorker worker;
	sirScriptOptimizationReply_t reply = {};
	SIR_TEST_CHECK( worker.process( request, &reply ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( worker.configure( nullptr, &cache, backend, 128 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( worker.configure( &inputs, nullptr, backend, 128 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( worker.configure( &inputs, &cache, backend, 0 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( worker.configure( &inputs, &cache, backend, ( uint64_t )UINT32_MAX + 1 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	backend.nABIRevision = 0;
	SIR_TEST_CHECK( worker.configure( &inputs, &cache, backend, 128 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	backend.nABIRevision = SIR_SCRIPT_OPT_BACKEND_ABI_REVISION;
	backend.pCompile = nullptr;
	SIR_TEST_CHECK( worker.configure( &inputs, &cache, backend, 128 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	backend.pCompile = coverageCompile;
	backend.pRelease = nullptr;
	SIR_TEST_CHECK( worker.configure( &inputs, &cache, backend, 128 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	backend.pRelease = coverageRelease;
	SIR_TEST_CHECK( worker.configure( &inputs, &cache, backend, 128 ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( worker.configure( &inputs, &cache, backend, 128 ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( worker.process( request, nullptr ) == SIR_SCRIPT_INVALID_ARGUMENT );
	sirScriptOptimizationRequest_t invalid = request;
	invalid.nJobId = 0;
	SIR_TEST_CHECK( worker.process( invalid, &reply ) == SIR_SCRIPT_INVALID_ARGUMENT );

	SIR_TEST_CHECK( worker.process( request, &reply ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( reply.eResult == SIR_SCRIPT_OK && state.nCompiles == 1 && state.nReleases == 1 );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::validateCompletion( request, reply ) == SIR_SCRIPT_OK );
	uint8_t szFetched[128] = {};
	SIR_TEST_CHECK( cache.fetch( reply.cached, szFetched, sizeof( szFetched ) ) == SIR_SCRIPT_OK );
	for ( uint32_t i = 0; i < sizeof( szFetched ); ++i ) {
		SIR_TEST_CHECK( szFetched[i] == ( uint8_t )( szInput[i] ^ 0x5a ) );
	}
	request.nJobId = 2;
	SIR_TEST_CHECK( worker.process( request, &reply ) == SIR_SCRIPT_OK && reply.eResult == SIR_SCRIPT_OK );

	sirScriptOptimizerRecipe_t absentRecipe = {};
	uint8_t szAbsent[128] = {};
	memset( szAbsent, 0x33, sizeof( szAbsent ) );
	sirScriptOptimizationRequest_t absent = coverageRequest( szAbsent, 3, &absentRecipe );
	SIR_TEST_CHECK( worker.process( absent, &reply ) == SIR_SCRIPT_OK && reply.eResult == SIR_SCRIPT_NOT_FOUND );
	const uint32_t nCompilesBeforeFailures = state.nCompiles;
	for ( uint32_t nMode = 1; nMode <= 8; ++nMode ) {
		state.nMode = nMode;
		request.nJobId = 10 + nMode;
		SIR_TEST_CHECK( worker.process( request, &reply ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( reply.eResult == ( nMode == 6 ? SIR_SCRIPT_BACKEND_FAILURE : SIR_SCRIPT_CAPACITY_EXCEEDED ) );
	}
	SIR_TEST_CHECK( state.nCompiles == nCompilesBeforeFailures + 8 );
	SIR_TEST_CHECK( state.nReleases == state.nCompiles );

	state.nMode = 9;
	state.pWorker = &worker;
	state.pRequest = &request;
	request.nJobId = 20;
	SIR_TEST_CHECK( worker.process( request, &reply ) == SIR_SCRIPT_OK && reply.eResult == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( state.eReentrantResult == SIR_SCRIPT_INVALID_ARGUMENT );

	SIRScriptOptimizerWorker smallWorker;
	state.nMode = 0;
	SIR_TEST_CHECK( smallWorker.configure( &inputs, &cache, backend, 64 ) == SIR_SCRIPT_OK );
	request.nJobId = 21;
	SIR_TEST_CHECK( smallWorker.process( request, &reply ) == SIR_SCRIPT_OK && reply.eResult == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( state.nReleases == state.nCompiles );
	SIR_TEST_CHECK( inputs.erase( request.key ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.erase( request.key ) == SIR_SCRIPT_OK );
	std::filesystem::remove_all( root );
	return true;
}

int main( void )
{
	if ( !testDirectWorker() ) {
		return 1;
	}
	puts( "optimizer worker direct coverage: PASS" );
	return 0;
}
