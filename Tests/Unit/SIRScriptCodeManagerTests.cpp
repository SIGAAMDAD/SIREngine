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

#include <Scripting/Core/SIRScriptServer.h>

#include <stdio.h>
#include <thread>

#define SIR_TEST_CHECK( condition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirCodeTestProvider_s
{
	alignas( 64 ) uint64_t szData[8];
	uint32_t nAcquires;
	uint32_t nReleases;
	uint32_t nLive;
	bool bBadRevision;
	bool bBadAlignment;
	bool bFailAcquire;
	bool bZeroToken;
	SIRScriptCodeManager *pManager;
	sirScriptResult_t eReentrant;
} sirCodeTestProvider_t;

static sirScriptResult_t acquireFixture( void *pUser, uint64_t, uint64_t nId, uint64_t nRevision, sirScriptProgramView_t *pView,
                                         uint64_t *pToken ) noexcept
{
	sirCodeTestProvider_t *pFixture = ( sirCodeTestProvider_t * )pUser;
	if ( pFixture->pManager != nullptr ) {
		pFixture->eReentrant = pFixture->pManager->shutdown();
	}
	if ( pFixture->bFailAcquire ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	++pFixture->nAcquires;
	*pView = { nId, pFixture->bBadRevision ? nRevision + 1 : nRevision, 77, pFixture->szData, sizeof( pFixture->szData ), 64 };
	if ( pFixture->bBadAlignment ) {
		pView->pData = ( ( unsigned char * )pFixture->szData ) + 1;
	}
	*pToken = pFixture->bZeroToken ? 0 : pFixture->nAcquires;
	if ( *pToken != 0 ) {
		++pFixture->nLive;
	}
	return SIR_SCRIPT_OK;
}

static void releaseFixture( void *pUser, uint64_t ) noexcept
{
	sirCodeTestProvider_t *pFixture = ( sirCodeTestProvider_t * )pUser;
	++pFixture->nReleases;
	--pFixture->nLive;
	if ( pFixture->pManager != nullptr ) {
		pFixture->eReentrant = pFixture->pManager->shutdown();
	}
}

static sirScriptProgramSourceAPI_t fixtureAPI( sirCodeTestProvider_t *pFixture ) noexcept
{
	return { SIR_SCRIPT_PROGRAM_SOURCE_ABI_REVISION, pFixture, acquireFixture, releaseFixture };
}

static sirScriptCodeArtifact_t artifact( const SIRScriptBindingRegistry& registry, uint64_t nRevision ) noexcept
{
	return { 1, 50, nRevision, 77, registry.schemaFingerprint(), 88, 101 + nRevision };
}

static bool testCodeManager( void )
{
	SIRScriptBindingRegistry registry;
	sirCodeTestProvider_t provider = {};
	SIRScriptCodeManager manager( &registry );
	provider.pManager = &manager;
	sirScriptCodeHandle_t first = {};
	SIR_TEST_CHECK( manager.configure( 88, fixtureAPI( &provider ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.configure( 88, fixtureAPI( &provider ) ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( manager.stage( artifact( registry, 1 ), &first ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.stage( artifact( registry, 1 ), nullptr ) == SIR_SCRIPT_INVALID_ARGUMENT );
	sirScriptCodeArtifact_t bad = artifact( registry, 1 );
	bad.nTargetId = 89;
	SIR_TEST_CHECK( manager.stage( bad, &first ) == SIR_SCRIPT_TYPE_MISMATCH );
	bad = artifact( registry, 1 );
	bad.nSchemaFingerprint ^= 123;
	SIR_TEST_CHECK( manager.stage( bad, &first ) == SIR_SCRIPT_TYPE_MISMATCH );
	provider.bBadRevision = true;
	SIR_TEST_CHECK( manager.stage( artifact( registry, 1 ), &first ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( provider.nLive == 0 && provider.nReleases == 1 );
	provider.bBadRevision = false;
	provider.bBadAlignment = true;
	SIR_TEST_CHECK( manager.stage( artifact( registry, 1 ), &first ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( provider.nLive == 0 && provider.nReleases == 2 );
	provider.bBadAlignment = false;
	provider.bFailAcquire = true;
	SIR_TEST_CHECK( manager.stage( artifact( registry, 1 ), &first ) == SIR_SCRIPT_NOT_FOUND );
	provider.bFailAcquire = false;
	SIR_TEST_CHECK( manager.stage( artifact( registry, 1 ), &first ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( provider.eReentrant == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( manager.stage( artifact( registry, 1 ), &first ) == SIR_SCRIPT_DUPLICATE );
	sirScriptCodeHandle_t second = {};
	SIR_TEST_CHECK( manager.stage( artifact( registry, 2 ), &second ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.activate( first ) == SIR_SCRIPT_OK );
	uint64_t nCurrent = 0;
	SIR_TEST_CHECK( manager.activeRevision( 1, 50, &nCurrent ) == SIR_SCRIPT_OK && nCurrent == 1 );
	sirScriptProgramSourceAPI_t source = manager.makeProgramSourceAPI();
	sirScriptProgramView_t view = {};
	uint64_t nLeaseA = 0;
	uint64_t nLeaseB = 0;
	uint64_t nLeaseNew = 0;
	SIR_TEST_CHECK( source.pAcquire( source.pUser, 1, 50, 1, &view, &nLeaseA ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.pAcquire( source.pUser, 1, 50, 1, &view, &nLeaseB ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nLeaseA != nLeaseB && view.nBytes == 64 );
	SIR_TEST_CHECK( source.pAcquire( source.pUser, 2, 50, 1, &view, &nLeaseNew ) == SIR_SCRIPT_NOT_FOUND );
	SIR_TEST_CHECK( manager.shutdown() == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( manager.activate( second ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.stats().nRetiredArtifacts == 1 && manager.stats().nActiveRuntimeLeases == 2 );
	SIR_TEST_CHECK( source.pAcquire( source.pUser, 1, 50, 1, &view, &nLeaseB ) == SIR_SCRIPT_NOT_FOUND );
	source.pRelease( source.pUser, nLeaseA );
	source.pRelease( source.pUser, nLeaseA ); // Double release must not steal the next lease.
	SIR_TEST_CHECK( manager.stats().nActiveRuntimeLeases == 1 );
	SIR_TEST_CHECK( provider.nLive == 2 );
	// Failed acquisitions do not overwrite an existing lease token.
	SIR_TEST_CHECK( source.pAcquire( source.pUser, 1, 50, 2, &view, &nLeaseNew ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.stats().nActiveRuntimeLeases == 2 );
	source.pRelease( source.pUser, nLeaseNew );
	SIR_TEST_CHECK( manager.stats().nActiveRuntimeLeases == 1 );
	SIR_TEST_CHECK( manager.retire( second ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( provider.nLive == 1 );
	SIR_TEST_CHECK( manager.shutdown() == SIR_SCRIPT_BUSY );
	source.pRelease( source.pUser, nLeaseB );
	SIR_TEST_CHECK( manager.stats().nArtifacts == 0 );
	SIR_TEST_CHECK( manager.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( provider.nLive == 0 && provider.nAcquires == provider.nReleases );
	return true;
}

static bool testCapacityAndOwnership( void )
{
	SIRScriptBindingRegistry registry;
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	sirCodeTestProvider_t provider = {};
	SIRScriptCodeManager manager( &registry );
	SIR_TEST_CHECK( manager.configure( 88, fixtureAPI( &provider ) ) == SIR_SCRIPT_OK );
	const sirScriptProgramSourceAPI_t source = manager.makeProgramSourceAPI();
	sirScriptCodeHandle_t handles[32] = {};
	for ( uint32_t i = 0; i < 32; ++i ) {
		sirScriptCodeArtifact_t item = artifact( registry, 1 );
		item.nProgramId = 100 + i;
		SIR_TEST_CHECK( manager.stage( item, &handles[i] ) == SIR_SCRIPT_OK );
	}
	sirScriptCodeArtifact_t overflow = artifact( registry, 1 );
	overflow.nProgramId = 1000;
	sirScriptCodeHandle_t newHandle = {};
	SIR_TEST_CHECK( manager.stage( overflow, &newHandle ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	for ( uint32_t i = 0; i < 32; ++i ) {
		SIR_TEST_CHECK( manager.discard( handles[i] ) == SIR_SCRIPT_OK );
	}
	SIR_TEST_CHECK( manager.stage( overflow, &newHandle ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.discard( handles[0] ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( manager.activate( newHandle ) == SIR_SCRIPT_OK );

	uint64_t szLeases[128] = {};
	sirScriptProgramView_t view = {};
	for ( uint32_t i = 0; i < 128; ++i ) {
		SIR_TEST_CHECK( source.pAcquire( source.pUser, 1, 1000, 1, &view, &szLeases[i] ) == SIR_SCRIPT_OK );
	}
	uint64_t nOverflowLease = 0;
	SIR_TEST_CHECK( source.pAcquire( source.pUser, 1, 1000, 1, &view, &nOverflowLease ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( manager.stats().nActiveRuntimeLeases == 128 );
	sirScriptResult_t eThreadResult = SIR_SCRIPT_OK;
	std::thread worker( [&]() { eThreadResult = source.pAcquire( source.pUser, 1, 1000, 1, &view, &nOverflowLease ); } );
	worker.join();
	SIR_TEST_CHECK( eThreadResult == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( manager.retire( newHandle ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.shutdown() == SIR_SCRIPT_BUSY );
	for ( uint32_t i = 0; i < 128; ++i ) {
		source.pRelease( source.pUser, szLeases[i] );
	}
	SIR_TEST_CHECK( manager.stats().nArtifacts == 0 );
	SIR_TEST_CHECK( manager.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( provider.nLive == 0 && provider.nAcquires == provider.nReleases );
	return true;
}

typedef struct sirCodeTestBackend_s
{
	uint32_t nStarts;
	uint32_t nLoads;
	uint32_t nUnloads;
	uint32_t nCreates;
	uint32_t nDestroys;
	uint32_t nCalls;
	bool bFailNextLoad;
} sirCodeTestBackend_t;

static sirScriptResult_t backendStart( void *pUser ) noexcept
{
	++( ( sirCodeTestBackend_t * )pUser )->nStarts;
	return SIR_SCRIPT_OK;
}
static void backendStop( void * ) noexcept
{
}
static sirScriptResult_t backendLoad( void *pUser, sirScriptProgramView_t view, uint64_t *pToken ) noexcept
{
	sirCodeTestBackend_t *pBackend = ( sirCodeTestBackend_t * )pUser;
	++pBackend->nLoads;
	if ( pBackend->bFailNextLoad ) {
		pBackend->bFailNextLoad = false;
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( view.nFormatId != 77 ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	*pToken = view.nRevision;
	return SIR_SCRIPT_OK;
}
static void backendUnload( void *pUser, uint64_t ) noexcept
{
	++( ( sirCodeTestBackend_t * )pUser )->nUnloads;
}
static sirScriptResult_t backendCreate( void *pUser, uint64_t, uint64_t, uint64_t *pToken ) noexcept
{
	++( ( sirCodeTestBackend_t * )pUser )->nCreates;
	*pToken = 123;
	return SIR_SCRIPT_OK;
}
static sirScriptResult_t backendInitialize( void *, uint64_t ) noexcept
{
	return SIR_SCRIPT_OK;
}
static void backendShutdown( void *, uint64_t ) noexcept
{
}
static void backendDestroy( void *pUser, uint64_t ) noexcept
{
	++( ( sirCodeTestBackend_t * )pUser )->nDestroys;
}
static sirScriptResult_t backendInvoke( void *pUser, const sirScriptInvokeRequest_t * ) noexcept
{
	++( ( sirCodeTestBackend_t * )pUser )->nCalls;
	return SIR_SCRIPT_OK;
}

static bool testRuntimeIntegration( void )
{
	sirCodeTestProvider_t provider = {};
	SIRScriptServer server;
	sirCodeTestBackend_t backend = {};
	SIR_TEST_CHECK( server.bindings().publish() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.codeManager().configure( 88, fixtureAPI( &provider ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.useCodeManagerSource() == SIR_SCRIPT_OK );
	const sirScriptBackendAPI_t api = { SIR_SCRIPT_BACKEND_ABI_REVISION,
	                                    &backend,
	                                    backendStart,
	                                    backendStop,
	                                    backendLoad,
	                                    backendUnload,
	                                    backendCreate,
	                                    backendInitialize,
	                                    backendShutdown,
	                                    backendDestroy,
	                                    backendInvoke };
	SIR_TEST_CHECK( server.runtime().registerBackend( 1, api ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.runtime().startBackend( 1 ) == SIR_SCRIPT_OK );
	sirScriptCodeHandle_t first = {};
	sirScriptCodeHandle_t second = {};
	SIR_TEST_CHECK( server.codeManager().stage( artifact( server.bindings(), 1 ), &first ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.codeManager().activate( first ) == SIR_SCRIPT_OK );
	sirScriptProgramHandle_t program = {};
	backend.bFailNextLoad = true;
	SIR_TEST_CHECK( server.runtime().loadProgram( 1, 50, 1, &program ) == SIR_SCRIPT_BACKEND_FAILURE );
	SIR_TEST_CHECK( server.codeManager().stats().nActiveRuntimeLeases == 0 );
	SIR_TEST_CHECK( server.runtime().loadProgram( 1, 50, 1, &program ) == SIR_SCRIPT_OK );
	sirScriptInstanceHandle_t instance = {};
	SIR_TEST_CHECK( server.runtime().createInstance( program, 9, &instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.codeManager().stage( artifact( server.bindings(), 2 ), &second ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.codeManager().activate( second ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.runtime().loadProgram( 1, 50, 1, &program ) == SIR_SCRIPT_DUPLICATE );
	SIR_TEST_CHECK( server.codeManager().stats().nRetiredArtifacts == 1 );
	SIR_TEST_CHECK( server.runtime().destroyInstance( instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.runtime().unloadProgram( program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.codeManager().stats().nRetiredArtifacts == 0 );
	SIR_TEST_CHECK( provider.nLive == 1 );
	sirScriptProgramHandle_t secondProgram = {};
	SIR_TEST_CHECK( server.runtime().loadProgram( 1, 50, 2, &secondProgram ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( server.runtime().shutdownAll() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.nLoads == 3 && backend.nUnloads == 2 && backend.nDestroys == 1 );
	SIR_TEST_CHECK( server.codeManager().shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( provider.nLive == 0 && provider.nAcquires == provider.nReleases );
	return true;
}

static bool testImplicitServerShutdown( void )
{
	sirCodeTestProvider_t provider = {};
	sirCodeTestBackend_t backend = {};
	{
		SIRScriptServer server;
		SIR_TEST_CHECK( server.bindings().publish() == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( server.codeManager().configure( 88, fixtureAPI( &provider ) ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( server.useCodeManagerSource() == SIR_SCRIPT_OK );
		const sirScriptBackendAPI_t api = { SIR_SCRIPT_BACKEND_ABI_REVISION,
		                                    &backend,
		                                    backendStart,
		                                    backendStop,
		                                    backendLoad,
		                                    backendUnload,
		                                    backendCreate,
		                                    backendInitialize,
		                                    backendShutdown,
		                                    backendDestroy,
		                                    backendInvoke };
		SIR_TEST_CHECK( server.runtime().registerBackend( 1, api ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( server.runtime().startBackend( 1 ) == SIR_SCRIPT_OK );
		sirScriptCodeHandle_t code = {};
		SIR_TEST_CHECK( server.codeManager().stage( artifact( server.bindings(), 7 ), &code ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( server.codeManager().activate( code ) == SIR_SCRIPT_OK );
		sirScriptProgramHandle_t program = {};
		SIR_TEST_CHECK( server.runtime().loadProgram( 1, 50, 7, &program ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( provider.nLive == 1 );
	}
	SIR_TEST_CHECK( provider.nLive == 0 );
	SIR_TEST_CHECK( provider.nAcquires == provider.nReleases );
	SIR_TEST_CHECK( backend.nLoads == 1 && backend.nUnloads == 1 );
	return true;
}

int main( void )
{
	if ( !testCodeManager() || !testCapacityAndOwnership() || !testRuntimeIntegration() || !testImplicitServerShutdown() ) {
		return 1;
	}
	puts( "script code manager PASS" );
	return 0;
}
