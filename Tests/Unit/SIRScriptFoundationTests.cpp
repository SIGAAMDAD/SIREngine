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

#include <Scripting/Core/SIRScriptBindingRegistry.h>
#include <Scripting/Core/SIRScriptRuntime.h>
#include <Scripting/Core/SIRScriptIngress.h>
#include <Scripting/Core/SIRScriptServer.h>

#include <stdio.h>
#include <new>
#include <string.h>
#include <thread>
#include <atomic>

#define SIR_TEST_CHECK( condition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirScriptTestProgramSource_s
{
	uint64_t nPayload;
	uint32_t nAcquires;
	uint32_t nReleases;
	uint32_t nActive;
	bool bFailNextAcquire;
	bool bBadRevision;
	bool bBadAlignment;
	bool bZeroToken;
} sirScriptTestProgramSource_t;

static sirScriptResult_t testAcquireProgram( void *pContext, uint64_t, uint64_t nProgramId, uint64_t nRevision,
                                             sirScriptProgramView_t *pView, uint64_t *pToken ) noexcept
{
	sirScriptTestProgramSource_t *pSource = ( sirScriptTestProgramSource_t * )pContext;
	++pSource->nAcquires;
	if ( pSource->bFailNextAcquire ) {
		pSource->bFailNextAcquire = false;
		return SIR_SCRIPT_NOT_FOUND;
	}
	pView->nProgramId = nProgramId;
	pView->nRevision = pSource->bBadRevision ? nRevision + 1 : nRevision;
	pView->nFormatId = 100;
	pView->pData = &pSource->nPayload;
	pView->nBytes = sizeof( pSource->nPayload );
	pView->nAlignment = pSource->bBadAlignment ? 3 : alignof( uint64_t );
	*pToken = pSource->bZeroToken ? 0 : pSource->nAcquires;
	if ( *pToken != 0 ) {
		++pSource->nActive;
	}
	return SIR_SCRIPT_OK;
}

static void testReleaseProgram( void *pContext, uint64_t ) noexcept
{
	sirScriptTestProgramSource_t *pSource = ( sirScriptTestProgramSource_t * )pContext;
	++pSource->nReleases;
	--pSource->nActive;
}

static sirScriptProgramSourceAPI_t testProgramSourceAPI( sirScriptTestProgramSource_t *pSource ) noexcept
{
	sirScriptProgramSourceAPI_t api = {};
	api.nABIRevision = SIR_SCRIPT_PROGRAM_SOURCE_ABI_REVISION;
	api.pUser = pSource;
	api.pAcquire = testAcquireProgram;
	api.pRelease = testReleaseProgram;
	return api;
}

typedef struct sirScriptTestBackend_s
{
	uint32_t nStarts;
	uint32_t nStops;
	uint32_t nLoads;
	uint32_t nUnloads;
	uint32_t nCreates;
	uint32_t nInitializes;
	uint32_t nShutdowns;
	uint32_t nDestroys;
	uint32_t nInvokes;
	uint32_t nLastEvent;
	uint32_t nLargeMarker;
	uint32_t nPayloadsAtInvoke;
	uint64_t nEventSum;
	bool bPostFromInvoke;
	bool bWrongInvokeThread;
	std::thread::id ownerThread;
	sirScriptResult_t eReentrantStatus;
	bool bFailNextInit;
	bool bFailNextLoad;
	bool bBadProgramView;
	uint32_t nActiveLeasesAtUnload;
	sirScriptTestProgramSource_t *pSource;
	SIRScriptRuntime *pRuntime;
	sirScriptSubscriptionHandle_t subscription;
	sirScriptInstanceHandle_t scheduledInstance;
	sirScriptTaskHandle_t taskToCancel;
	sirScriptTaskHandle_t taskSpawned;
	sirScriptResult_t eCancelStatus;
	sirScriptResult_t eEnqueueStatus;
	sirScriptResult_t eShutdownEnqueueStatus;
	bool bEnqueueDuringShutdown;
} sirScriptTestBackend_t;

static sirScriptResult_t testStart( void *pContext ) noexcept
{
	sirScriptTestBackend_t *pBackend = ( sirScriptTestBackend_t * )pContext;
	++pBackend->nStarts;
	if ( pBackend->pRuntime != nullptr ) {
		pBackend->eReentrantStatus = pBackend->pRuntime->startBackend( 1 );
	}
	return SIR_SCRIPT_OK;
}

static void testStop( void *pContext ) noexcept
{
	++( ( sirScriptTestBackend_t * )pContext )->nStops;
}

static sirScriptResult_t testLoad( void *pContext, sirScriptProgramView_t view, uint64_t *pToken ) noexcept
{
	sirScriptTestBackend_t *pBackend = ( sirScriptTestBackend_t * )pContext;
	++pBackend->nLoads;
	if ( view.nFormatId != 100 || view.nBytes != sizeof( uint64_t ) || view.pData == nullptr ) {
		pBackend->bBadProgramView = true;
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	if ( pBackend->bFailNextLoad ) {
		pBackend->bFailNextLoad = false;
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	*pToken = view.nProgramId ^ ( view.nRevision << 32 );
	return SIR_SCRIPT_OK;
}

static void testUnload( void *pContext, uint64_t ) noexcept
{
	sirScriptTestBackend_t *pBackend = ( sirScriptTestBackend_t * )pContext;
	++pBackend->nUnloads;
	if ( pBackend->pSource != nullptr ) {
		pBackend->nActiveLeasesAtUnload = pBackend->pSource->nActive;
	}
}

static sirScriptResult_t testCreate( void *pContext, uint64_t, uint64_t nOwner, uint64_t *pToken ) noexcept
{
	++( ( sirScriptTestBackend_t * )pContext )->nCreates;
	*pToken = nOwner;
	return SIR_SCRIPT_OK;
}

static sirScriptResult_t testInitialize( void *pContext, uint64_t ) noexcept
{
	sirScriptTestBackend_t *pBackend = ( sirScriptTestBackend_t * )pContext;
	++pBackend->nInitializes;
	if ( pBackend->bFailNextInit ) {
		pBackend->bFailNextInit = false;
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	return SIR_SCRIPT_OK;
}

static void testShutdown( void *pContext, uint64_t ) noexcept
{
	sirScriptTestBackend_t *pBackend = ( sirScriptTestBackend_t * )pContext;
	++pBackend->nShutdowns;
	if ( pBackend->bEnqueueDuringShutdown && pBackend->pRuntime != nullptr ) {
		uint32_t nValue = 99;
		sirScriptTaskHandle_t task = {};
		pBackend->eShutdownEnqueueStatus = pBackend->pRuntime->enqueueEvent( 25, { &nValue, 4, 4 }, SIR_SCRIPT_PHASE_EARLY, &task );
	}
}

static void testDestroy( void *pContext, uint64_t ) noexcept
{
	++( ( sirScriptTestBackend_t * )pContext )->nDestroys;
}

static sirScriptResult_t testInvoke( void *pContext, const sirScriptInvokeRequest_t *pRequest ) noexcept
{
	if ( pRequest == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	const uint64_t nMethod = pRequest->nMethodId;
	const sirScriptReadFrame_t arguments = pRequest->arguments;
	sirScriptTestBackend_t *pBackend = ( sirScriptTestBackend_t * )pContext;
	++pBackend->nInvokes;
	if ( nMethod == 77 && arguments.nBytes == sizeof( uint32_t ) ) {
		pBackend->nLastEvent = *( const uint32_t * )arguments.pData;
		// Disposing a subscription while dispatching must suppress later events.
		if ( pBackend->pRuntime != nullptr && pBackend->subscription.nSlot != 0 ) {
			pBackend->pRuntime->unsubscribe( pBackend->subscription );
			pBackend->subscription = {};
		}
	}
	if ( nMethod == 91 && arguments.nBytes == 512 ) {
		pBackend->nLargeMarker = ( ( const unsigned char * )arguments.pData )[400];
		if ( pBackend->pRuntime != nullptr ) {
			pBackend->nPayloadsAtInvoke = pBackend->pRuntime->queryMemoryFootprint().nLiveExternalPayloads;
		}
	}
	if ( nMethod == 90 ) {
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( nMethod == 88 && arguments.nBytes == sizeof( uint32_t ) ) {
		pBackend->nLastEvent = *( const uint32_t * )arguments.pData;
		pBackend->nEventSum += pBackend->nLastEvent;
		if ( pBackend->ownerThread != std::thread::id{} && std::this_thread::get_id() != pBackend->ownerThread ) {
			pBackend->bWrongInvokeThread = true;
		}
		if ( pBackend->bPostFromInvoke && pBackend->pRuntime != nullptr ) {
			pBackend->bPostFromInvoke = false;
			uint32_t nFollowup = 777;
			pBackend->eEnqueueStatus = pBackend->pRuntime->postEvent( 25, { &nFollowup, 4, 4 }, SIR_SCRIPT_PHASE_EARLY );
		}
	}
	if ( nMethod == 89 && pBackend->pRuntime != nullptr ) {
		pBackend->eCancelStatus = pBackend->pRuntime->cancelScheduled( pBackend->taskToCancel );
		uint32_t nFollowup = 9001;
		pBackend->eEnqueueStatus = pBackend->pRuntime->enqueueEvent( 25, { &nFollowup, sizeof( nFollowup ), alignof( uint32_t ) },
		                                                             SIR_SCRIPT_PHASE_EARLY, &pBackend->taskSpawned );
	}
	return SIR_SCRIPT_OK;
}

static sirScriptBackendAPI_t testBackendAPI( sirScriptTestBackend_t *pBackend ) noexcept
{
	sirScriptBackendAPI_t api = {};
	api.nABIRevision = SIR_SCRIPT_BACKEND_ABI_REVISION;
	api.pUser = pBackend;
	api.pStart = testStart;
	api.pStop = testStop;
	api.pLoadProgram = testLoad;
	api.pUnloadProgram = testUnload;
	api.pCreateInstance = testCreate;
	api.pInitializeInstance = testInitialize;
	api.pShutdownInstance = testShutdown;
	api.pDestroyInstance = testDestroy;
	api.pInvoke = testInvoke;
	return api;
}

static sirScriptResult_t testNative( void *, sirScriptReadFrame_t arguments, sirScriptWriteFrame_t result ) noexcept
{
	*( uint32_t * )result.pData = *( const uint32_t * )arguments.pData + 4;
	return SIR_SCRIPT_OK;
}

static bool testBindings( void ) noexcept
{
	SIRScriptBindingRegistry registry;
	const sirScriptTypeDescriptor_t type = { 10, sizeof( uint32_t ), alignof( uint32_t ),
	                                         SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS };
	const sirScriptFunctionDescriptor_t function = { 11, 12, 10, 10, 0, 1 };
	SIR_TEST_CHECK( registry.registerType( type ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerType( type ) == SIR_SCRIPT_DUPLICATE );
	SIR_TEST_CHECK( registry.registerType( { 99, 8, 3, 0 } ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( registry.registerType( { 99, 8, 8, 0x80000000U } ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( registry.registerType( { 99, 3, 4, 0 } ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( registry.registerFunction( function, testNative, nullptr ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerFunction( function, testNative, nullptr ) == SIR_SCRIPT_DUPLICATE );
	SIR_TEST_CHECK( registry.registerEvent( { 25, 10 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerType( { 30, 4, 4, 0 } ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_WRONG_STATE );

	uint32_t nArgument = 15;
	uint32_t nResult = 0;
	const sirScriptReadFrame_t arguments = { &nArgument, sizeof( nArgument ), alignof( uint32_t ) };
	const sirScriptWriteFrame_t result = { &nResult, sizeof( nResult ), alignof( uint32_t ) };
	SIR_TEST_CHECK( registry.invokeNative( { 11, 12, 1, arguments, result } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nResult == 19 );
	SIR_TEST_CHECK( registry.invokeNative( { 11, 55, 1, arguments, result } ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( registry.invokeNative( { 11, 12, 2, arguments, result } ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( registry.invokeNative( { 11, 12, 1, { nullptr, 4, 4 }, result } ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( registry.invokeNative( { 11, 12, 1, { &nArgument, 8, 4 }, result } ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( registry.validateEvent( 25, arguments ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.validateEvent( 25, { nullptr, 0, 1 } ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( registry.validateEvent( 26, arguments ) == SIR_SCRIPT_NOT_FOUND );
	return true;
}

static bool testSchemaFingerprint( void ) noexcept
{
	SIRScriptBindingRegistry first;
	SIRScriptBindingRegistry second;
	SIRScriptBindingRegistry changed;
	SIR_TEST_CHECK( first.schemaFingerprint() == 0 );
	SIRScriptBindingRegistry *szRegistries[] = { &first, &second, &changed };
	for ( SIRScriptBindingRegistry *pRegistry : szRegistries ) {
		SIR_TEST_CHECK( pRegistry->registerType( { 10, 4, 4, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) ==
		                SIR_SCRIPT_OK );
	}
	SIR_TEST_CHECK( first.registerType( { 20, 8, 8, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( second.registerType( { 20, 8, 8, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( changed.registerType( { 20, 8, 8, SIR_SCRIPT_TYPE_PLAIN_BYTES } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( first.registerFunction( { 101, 201, 10, 20, 0, 1 }, testNative, nullptr ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( first.registerFunction( { 201, 301, 20, 10, 1, 2 }, testNative, nullptr ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( second.registerFunction( { 201, 301, 20, 10, 1, 2 }, testNative, nullptr ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( second.registerFunction( { 101, 201, 10, 20, 0, 1 }, testNative, nullptr ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( changed.registerFunction( { 101, 201, 10, 20, 0, 1 }, testNative, nullptr ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( changed.registerFunction( { 201, 301, 20, 10, 1, 2 }, testNative, nullptr ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( first.registerEvent( { 25, 20 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( first.registerEvent( { 26, 10 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( second.registerEvent( { 26, 10 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( second.registerEvent( { 25, 20 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( changed.registerEvent( { 25, 20 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( changed.registerEvent( { 26, 10 } ) == SIR_SCRIPT_OK );
	for ( SIRScriptBindingRegistry *pRegistry : szRegistries ) {
		SIR_TEST_CHECK( pRegistry->publish() == SIR_SCRIPT_OK );
	}
	SIR_TEST_CHECK( first.schemaFingerprint() != 0 );
	SIR_TEST_CHECK( first.schemaFingerprint() == second.schemaFingerprint() );
	SIR_TEST_CHECK( first.schemaFingerprint() != changed.schemaFingerprint() );
	return true;
}

static bool testRuntime( void ) noexcept
{
	SIRScriptServer server;
	sirScriptTestProgramSource_t source = {};
	source.nPayload = 0x123456789ABCDEF0ULL;
	SIR_TEST_CHECK( server.runtime().setProgramSource( testProgramSourceAPI( &source ) ) == SIR_SCRIPT_OK );
	SIRScriptBindingRegistry &registry = server.bindings();
	SIR_TEST_CHECK( registry.registerType( { 10, sizeof( uint32_t ), alignof( uint32_t ),
	                                         SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 25, 10 } ) == SIR_SCRIPT_OK );
	SIRScriptRuntime &runtime = server.runtime();
	sirScriptTestBackend_t backend = {};
	backend.pRuntime = &runtime;
	const sirScriptBackendAPI_t api = testBackendAPI( &backend );
	sirScriptBackendAPI_t mismatched = api;
	mismatched.nABIRevision = 999;
	SIR_TEST_CHECK( runtime.registerBackend( 1, mismatched ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( runtime.registerBackend( 1, api ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.registerBackend( 1, api ) == SIR_SCRIPT_DUPLICATE );
	SIR_TEST_CHECK( runtime.startBackend( 1 ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.startBackend( 1 ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.eReentrantStatus == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( runtime.startBackend( 1 ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( runtime.unregisterBackend( 1 ) == SIR_SCRIPT_BUSY );

	sirScriptProgramHandle_t program = {};
	SIR_TEST_CHECK( runtime.loadProgram( 1, 123, 1, &program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( program.nSlot != 0 && program.nGeneration != 0 );
	SIR_TEST_CHECK( runtime.loadProgram( 1, 123, 1, &program ) == SIR_SCRIPT_DUPLICATE );
	SIR_TEST_CHECK( runtime.createInstance( program, 99, nullptr ) == SIR_SCRIPT_INVALID_ARGUMENT );

	backend.bFailNextInit = true;
	sirScriptInstanceHandle_t failed = {};
	SIR_TEST_CHECK( runtime.createInstance( program, 99, &failed ) == SIR_SCRIPT_BACKEND_FAILURE );
	SIR_TEST_CHECK( backend.nDestroys == 1 && backend.nShutdowns == 0 );
	SIR_TEST_CHECK( failed.nSlot == 0 );

	sirScriptInstanceHandle_t instance = {};
	SIR_TEST_CHECK( runtime.createInstance( program, 99, &instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( instance.nSlot != 0 );
	SIR_TEST_CHECK( runtime.unloadProgram( program ) == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( runtime.stopBackend( 1 ) == SIR_SCRIPT_BUSY );

	sirScriptSubscriptionHandle_t subscription = {};
	SIR_TEST_CHECK( runtime.subscribe( instance, 999, 77, &subscription ) == SIR_SCRIPT_NOT_FOUND );
	SIR_TEST_CHECK( runtime.subscribe( instance, 25, 77, &subscription ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.subscribe( instance, 25, 77, &backend.subscription ) == SIR_SCRIPT_DUPLICATE );
	backend.subscription = subscription;
	uint32_t nValue = 314;
	SIR_TEST_CHECK( runtime.publishEvent( 25, { &nValue, 4, 4 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.nInvokes == 1 && backend.nLastEvent == 314 );
	SIR_TEST_CHECK( runtime.publishEvent( 25, { &nValue, 4, 4 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.nInvokes == 1 );
	SIR_TEST_CHECK( runtime.unsubscribe( subscription ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.publishEvent( 25, { nullptr, 0, 1 } ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( runtime.invoke( instance, 1234, { nullptr, 4, 4 }, { nullptr, 0, 1 } ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( runtime.invoke( instance, 1234, { nullptr, 0, 1 }, { nullptr, 0, 1 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.destroyInstance( instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.nShutdowns == 1 && backend.nDestroys == 2 );
	SIR_TEST_CHECK( runtime.destroyInstance( instance ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.invoke( instance, 1234, {}, {} ) == SIR_SCRIPT_INVALID_HANDLE );

	sirScriptInstanceHandle_t reused = {};
	SIR_TEST_CHECK( runtime.createInstance( program, 101, &reused ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( reused.nSlot == instance.nSlot && reused.nGeneration != instance.nGeneration );
	SIR_TEST_CHECK( runtime.destroyInstance( reused ) == SIR_SCRIPT_OK );
	const sirScriptProgramHandle_t oldProgram = program;
	SIR_TEST_CHECK( runtime.unloadProgram( program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.unloadProgram( oldProgram ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.stopBackend( 1 ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.nStarts == 1 && backend.nStops == 1 );
	SIR_TEST_CHECK( backend.nLoads == 1 && backend.nUnloads == 1 );
	SIR_TEST_CHECK( runtime.unregisterBackend( 1 ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.startBackend( 1 ) == SIR_SCRIPT_NOT_FOUND );
	return true;
}

static bool testScheduler( void ) noexcept
{
	SIRScriptServer server;
	sirScriptTestProgramSource_t source = {};
	source.nPayload = 0x123456789ABCDEF0ULL;
	SIR_TEST_CHECK( server.runtime().setProgramSource( testProgramSourceAPI( &source ) ) == SIR_SCRIPT_OK );
	SIRScriptBindingRegistry &registry = server.bindings();
	SIRScriptRuntime &runtime = server.runtime();
	SIR_TEST_CHECK( registry.registerType( { 10, 4, 4, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerType( { 11, 512, 16, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerType( { 12, 128, 128, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 25, 10 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 26, 11 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 27, 12 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerType( { 13, 8, 8, 0 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 28, 13 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );

	sirScriptTestBackend_t backend = {};
	backend.pRuntime = &runtime;
	SIR_TEST_CHECK( runtime.registerBackend( 1, testBackendAPI( &backend ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.startBackend( 1 ) == SIR_SCRIPT_OK );
	sirScriptProgramHandle_t program = {};
	sirScriptInstanceHandle_t instance = {};
	SIR_TEST_CHECK( runtime.loadProgram( 1, 123, 1, &program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.createInstance( program, 101, &instance ) == SIR_SCRIPT_OK );
	backend.scheduledInstance = instance;
	sirScriptSubscriptionHandle_t subscription = {};
	SIR_TEST_CHECK( runtime.subscribe( instance, 25, 88, &subscription ) == SIR_SCRIPT_OK );

	uint32_t nValue = 400;
	sirScriptTaskHandle_t first = {};
	SIR_TEST_CHECK( runtime.enqueueEvent( 25, { &nValue, 4, 4 }, SIR_SCRIPT_PHASE_EARLY, &first ) == SIR_SCRIPT_OK );
	nValue = 0;
	uint32_t nProcessed = 0;
	uint32_t nStale = 0;
	SIR_TEST_CHECK( runtime.pendingCount( SIR_SCRIPT_PHASE_EARLY ) == 1 );
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 0, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 0 && runtime.pendingCount( SIR_SCRIPT_PHASE_EARLY ) == 1 );
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 1, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 1 && nStale == 0 && backend.nLastEvent == 400 );
	SIR_TEST_CHECK( runtime.cancelScheduled( first ) == SIR_SCRIPT_INVALID_HANDLE );

	uint32_t nLate = 777;
	sirScriptTaskHandle_t late = {};
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 88, 10, { &nLate, 4, 4 }, SIR_SCRIPT_PHASE_LATE, &late ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 20, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 0 );
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_LATE, 20, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 1 && backend.nLastEvent == 777 );
	SIR_TEST_CHECK( runtime.cancelScheduled( late ) == SIR_SCRIPT_INVALID_HANDLE );

	uint32_t nMethodArg = 500;
	sirScriptTaskHandle_t trigger = {};
	sirScriptTaskHandle_t victim = {};
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 89, 10, { &nMethodArg, 4, 4 }, SIR_SCRIPT_PHASE_EARLY, &trigger ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 88, 10, { &nMethodArg, 4, 4 }, SIR_SCRIPT_PHASE_EARLY, &victim ) == SIR_SCRIPT_OK );
	backend.taskToCancel = victim;
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 20, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 1 && nStale == 0 );
	SIR_TEST_CHECK( backend.eCancelStatus == SIR_SCRIPT_OK && backend.eEnqueueStatus == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.pendingCount( SIR_SCRIPT_PHASE_EARLY ) == 1 );
	SIR_TEST_CHECK( backend.nLastEvent == 777 );
	SIR_TEST_CHECK( runtime.cancelScheduled( victim ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 20, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 1 && backend.nLastEvent == 9001 );

	sirScriptTaskHandle_t canceled = {};
	SIR_TEST_CHECK( runtime.enqueueEvent( 25, { &nMethodArg, 4, 4 }, SIR_SCRIPT_PHASE_LATE, &canceled ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.cancelScheduled( canceled ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.cancelScheduled( canceled ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.pendingCount( SIR_SCRIPT_PHASE_LATE ) == 0 );

	sirScriptTaskHandle_t invalid = {};
	alignas( 8 ) unsigned char szUnsafe[8] = {};
	SIR_TEST_CHECK( runtime.enqueueEvent( 28, { szUnsafe, 8, 8 }, SIR_SCRIPT_PHASE_EARLY, &invalid ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 88, 13, { szUnsafe, 8, 8 }, SIR_SCRIPT_PHASE_EARLY, &invalid ) ==
	                SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 88, 0, { szUnsafe, 8, 8 }, SIR_SCRIPT_PHASE_EARLY, &invalid ) ==
	                SIR_SCRIPT_TYPE_MISMATCH );
	alignas( 16 ) unsigned char szLarge[512] = {};
	alignas( 128 ) unsigned char szWide[128] = {};
	SIR_TEST_CHECK( runtime.enqueueEvent( 26, { szLarge, 512, 16 }, SIR_SCRIPT_PHASE_EARLY, &invalid ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( runtime.enqueueEvent( 27, { szWide, 128, 128 }, SIR_SCRIPT_PHASE_EARLY, &invalid ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( runtime.enqueueEvent( 25, { nullptr, 4, 4 }, SIR_SCRIPT_PHASE_EARLY, &invalid ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( runtime.enqueueEvent( 25, { &nMethodArg, 4, 4 }, ( sirScriptPhase_t )9, &invalid ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( runtime.pumpPhase( ( sirScriptPhase_t )9, 1, &nProcessed, &nStale ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 1, &nProcessed, &nProcessed ) == SIR_SCRIPT_INVALID_ARGUMENT );

	sirScriptTaskHandle_t tasks[64] = {};
	for ( uint32_t iTask = 0; iTask < 64; ++iTask ) {
		SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 88, 10, { &nMethodArg, 4, 4 }, SIR_SCRIPT_PHASE_LATE, &tasks[iTask] ) ==
		                SIR_SCRIPT_OK );
	}
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 88, 10, { &nMethodArg, 4, 4 }, SIR_SCRIPT_PHASE_LATE, &invalid ) ==
	                SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( runtime.cancelScheduled( tasks[10] ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 88, 10, { &nMethodArg, 4, 4 }, SIR_SCRIPT_PHASE_LATE, &invalid ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( invalid.nSlot == tasks[10].nSlot && invalid.nGeneration != tasks[10].nGeneration );
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_LATE, 64, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 64 && nStale == 0 );
	SIR_TEST_CHECK( runtime.pendingCount( SIR_SCRIPT_PHASE_LATE ) == 0 );

	sirScriptTaskHandle_t failing = {};
	sirScriptTaskHandle_t succeeding = {};
	uint32_t nSuccessValue = 600;
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 90, 0, { nullptr, 0, 1 }, SIR_SCRIPT_PHASE_EARLY, &failing ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 88, 10, { &nSuccessValue, 4, 4 }, SIR_SCRIPT_PHASE_EARLY, &succeeding ) ==
	                SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 20, &nProcessed, &nStale ) == SIR_SCRIPT_BACKEND_FAILURE );
	SIR_TEST_CHECK( nProcessed == 2 && nStale == 0 && backend.nLastEvent == 600 );

	sirScriptTaskHandle_t stale = {};
	SIR_TEST_CHECK( runtime.enqueueInvoke( instance, 88, 10, { &nMethodArg, 4, 4 }, SIR_SCRIPT_PHASE_EARLY, &stale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.destroyInstance( instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 2, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 1 && nStale == 1 );
	SIR_TEST_CHECK( runtime.cancelScheduled( stale ) == SIR_SCRIPT_INVALID_HANDLE );

	sirScriptResult_t eFromOtherThread = SIR_SCRIPT_OK;
	std::thread foreign( [&]() { eFromOtherThread = runtime.enqueueEvent( 25, { &nMethodArg, 4, 4 }, SIR_SCRIPT_PHASE_LATE, &invalid ); } );
	foreign.join();
	SIR_TEST_CHECK( eFromOtherThread == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( runtime.pendingCount( SIR_SCRIPT_PHASE_LATE ) == 0 );
	SIR_TEST_CHECK( runtime.unloadProgram( program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.stopBackend( 1 ) == SIR_SCRIPT_OK );
	return true;
}

static bool testShutdownAll( void ) noexcept
{
	SIRScriptServer server;
	sirScriptTestProgramSource_t source = {};
	source.nPayload = 0x123456789ABCDEF0ULL;
	SIR_TEST_CHECK( server.runtime().setProgramSource( testProgramSourceAPI( &source ) ) == SIR_SCRIPT_OK );
	SIRScriptRuntime &runtime = server.runtime();
	SIRScriptBindingRegistry &registry = server.bindings();
	sirScriptTestBackend_t backend = {};
	backend.pRuntime = &runtime;
	SIR_TEST_CHECK( registry.registerType( { 10, 4, 4, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 25, 10 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	backend.bEnqueueDuringShutdown = true;
	SIR_TEST_CHECK( runtime.registerBackend( 123, testBackendAPI( &backend ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.startBackend( 123 ) == SIR_SCRIPT_OK );
	sirScriptProgramHandle_t program = {};
	sirScriptInstanceHandle_t instance = {};
	sirScriptSubscriptionHandle_t subscription = {};
	sirScriptTaskHandle_t task = {};
	SIR_TEST_CHECK( runtime.loadProgram( 123, 456, 1, &program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.createInstance( program, 789, &instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.subscribe( instance, 25, 88, &subscription ) == SIR_SCRIPT_OK );
	uint32_t nPayload = 99;
	SIR_TEST_CHECK( runtime.enqueueEvent( 25, { &nPayload, 4, 4 }, SIR_SCRIPT_PHASE_LATE, &task ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.shutdownAll() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.nShutdowns == 1 && backend.nDestroys == 1 );
	SIR_TEST_CHECK( backend.nUnloads == 1 && backend.nStops == 1 && backend.nInvokes == 0 );
	SIR_TEST_CHECK( backend.eShutdownEnqueueStatus == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( runtime.pendingCount( SIR_SCRIPT_PHASE_EARLY ) == 0 );
	SIR_TEST_CHECK( runtime.pendingCount( SIR_SCRIPT_PHASE_LATE ) == 0 );
	SIR_TEST_CHECK( runtime.cancelScheduled( task ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.destroyInstance( instance ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.unloadProgram( program ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.unsubscribe( subscription ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.shutdownAll() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.nStops == 1 && backend.nShutdowns == 1 );
	SIR_TEST_CHECK( runtime.unregisterBackend( 123 ) == SIR_SCRIPT_OK );
	return true;
}

static bool testCrossBackendEvents( void ) noexcept
{
	SIRScriptServer server;
	sirScriptTestProgramSource_t source = {};
	source.nPayload = 0x123456789ABCDEF0ULL;
	SIR_TEST_CHECK( server.runtime().setProgramSource( testProgramSourceAPI( &source ) ) == SIR_SCRIPT_OK );
	SIRScriptBindingRegistry &registry = server.bindings();
	SIRScriptRuntime &runtime = server.runtime();
	sirScriptTestBackend_t first = {};
	sirScriptTestBackend_t second = {};
	SIR_TEST_CHECK( registry.registerType( { 10, 4, 4, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 25, 10 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.registerBackend( 1, testBackendAPI( &first ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.registerBackend( 2, testBackendAPI( &second ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.startBackend( 1 ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.startBackend( 2 ) == SIR_SCRIPT_OK );
	sirScriptProgramHandle_t firstProgram = {};
	sirScriptProgramHandle_t secondProgram = {};
	sirScriptInstanceHandle_t firstInstance = {};
	sirScriptInstanceHandle_t secondInstance = {};
	sirScriptSubscriptionHandle_t firstSubscription = {};
	sirScriptSubscriptionHandle_t secondSubscription = {};
	SIR_TEST_CHECK( runtime.loadProgram( 1, 123, 1, &firstProgram ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.loadProgram( 2, 456, 1, &secondProgram ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.nActive == 2 );
	SIR_TEST_CHECK( runtime.createInstance( firstProgram, 100, &firstInstance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.createInstance( secondProgram, 200, &secondInstance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.subscribe( firstInstance, 25, 88, &firstSubscription ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.subscribe( secondInstance, 25, 88, &secondSubscription ) == SIR_SCRIPT_OK );

	uint32_t nPayload = 55;
	sirScriptTaskHandle_t task = {};
	SIR_TEST_CHECK( runtime.enqueueEvent( 25, { &nPayload, 4, 4 }, SIR_SCRIPT_PHASE_EARLY, &task ) == SIR_SCRIPT_OK );
	uint32_t nProcessed = 0;
	uint32_t nStale = 0;
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 1, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 1 && nStale == 0 );
	SIR_TEST_CHECK( first.nInvokes == 1 && second.nInvokes == 1 );
	SIR_TEST_CHECK( first.nLastEvent == 55 && second.nLastEvent == 55 );

	SIR_TEST_CHECK( runtime.destroyInstance( firstInstance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.unloadProgram( firstProgram ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.nActive == 1 );
	SIR_TEST_CHECK( runtime.stopBackend( 1 ) == SIR_SCRIPT_OK );
	nPayload = 99;
	SIR_TEST_CHECK( runtime.publishEvent( 25, { &nPayload, 4, 4 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( first.nInvokes == 1 && second.nInvokes == 2 && second.nLastEvent == 99 );
	SIR_TEST_CHECK( runtime.shutdownAll() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( first.nStops == 1 && second.nStops == 1 );
	SIR_TEST_CHECK( first.nShutdowns == 1 && second.nShutdowns == 1 );
	SIR_TEST_CHECK( source.nActive == 0 && source.nReleases == 2 );
	return true;
}

static bool testProgramSourceLeases( void ) noexcept
{
	SIRScriptServer server;
	SIRScriptBindingRegistry &registry = server.bindings();
	SIRScriptRuntime &runtime = server.runtime();
	sirScriptTestProgramSource_t source = {};
	source.nPayload = 0xCAFE1234U;
	sirScriptTestBackend_t backend = {};
	backend.pSource = &source;
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.registerBackend( 1, testBackendAPI( &backend ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.startBackend( 1 ) == SIR_SCRIPT_OK );

	sirScriptProgramHandle_t program = {};
	SIR_TEST_CHECK( runtime.loadProgram( 1, 500, 1, &program ) == SIR_SCRIPT_WRONG_STATE );
	sirScriptProgramSourceAPI_t wrong = testProgramSourceAPI( &source );
	wrong.nABIRevision = 2;
	SIR_TEST_CHECK( runtime.setProgramSource( wrong ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( runtime.setProgramSource( testProgramSourceAPI( &source ) ) == SIR_SCRIPT_OK );

	source.bFailNextAcquire = true;
	SIR_TEST_CHECK( runtime.loadProgram( 1, 500, 1, &program ) == SIR_SCRIPT_NOT_FOUND );
	SIR_TEST_CHECK( source.nActive == 0 && source.nReleases == 0 );
	source.bBadRevision = true;
	SIR_TEST_CHECK( runtime.loadProgram( 1, 500, 1, &program ) == SIR_SCRIPT_TYPE_MISMATCH );
	source.bBadRevision = false;
	SIR_TEST_CHECK( source.nActive == 0 && source.nReleases == 1 );
	source.bBadAlignment = true;
	SIR_TEST_CHECK( runtime.loadProgram( 1, 500, 1, &program ) == SIR_SCRIPT_TYPE_MISMATCH );
	source.bBadAlignment = false;
	SIR_TEST_CHECK( source.nActive == 0 && source.nReleases == 2 );
	source.bZeroToken = true;
	SIR_TEST_CHECK( runtime.loadProgram( 1, 500, 1, &program ) == SIR_SCRIPT_BACKEND_FAILURE );
	source.bZeroToken = false;
	SIR_TEST_CHECK( source.nActive == 0 && source.nReleases == 2 );

	backend.bFailNextLoad = true;
	SIR_TEST_CHECK( runtime.loadProgram( 1, 500, 1, &program ) == SIR_SCRIPT_BACKEND_FAILURE );
	SIR_TEST_CHECK( source.nActive == 0 && source.nReleases == 3 );
	SIR_TEST_CHECK( runtime.loadProgram( 1, 500, 1, &program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( !backend.bBadProgramView && source.nActive == 1 );
	SIR_TEST_CHECK( runtime.clearProgramSource() == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( runtime.setProgramSource( testProgramSourceAPI( &source ) ) == SIR_SCRIPT_BUSY );
	sirScriptInstanceHandle_t instance = {};
	SIR_TEST_CHECK( runtime.createInstance( program, 100, &instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.unloadProgram( program ) == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( source.nActive == 1 );
	SIR_TEST_CHECK( runtime.destroyInstance( instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.unloadProgram( program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.nActiveLeasesAtUnload == 1 && source.nActive == 0 && source.nReleases == 4 );
	SIR_TEST_CHECK( runtime.clearProgramSource() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.loadProgram( 1, 500, 2, &program ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( runtime.stopBackend( 1 ) == SIR_SCRIPT_OK );
	return true;
}

static bool testCrossThreadIngress( void ) noexcept
{
	SIRScriptServer server;
	SIRScriptBindingRegistry &registry = server.bindings();
	SIRScriptRuntime &runtime = server.runtime();
	sirScriptTestProgramSource_t source = {};
	source.nPayload = 0x789ABCDEULL;
	sirScriptTestBackend_t backend = {};
	backend.ownerThread = std::this_thread::get_id();
	backend.pRuntime = &runtime;
	SIR_TEST_CHECK( registry.registerType( { 10, 4, 4, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerType( { 11, 8, 8, SIR_SCRIPT_TYPE_PLAIN_BYTES } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 25, 10 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 26, 11 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.setProgramSource( testProgramSourceAPI( &source ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.registerBackend( 1, testBackendAPI( &backend ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.startBackend( 1 ) == SIR_SCRIPT_OK );
	sirScriptProgramHandle_t program = {};
	sirScriptInstanceHandle_t instance = {};
	sirScriptSubscriptionHandle_t subscription = {};
	SIR_TEST_CHECK( runtime.loadProgram( 1, 100, 1, &program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.createInstance( program, 200, &instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.subscribe( instance, 25, 88, &subscription ) == SIR_SCRIPT_OK );
	const sirScriptRuntimeFootprint_t initial = runtime.queryMemoryFootprint();
	SIR_TEST_CHECK( initial.nRuntimeBytes >= initial.nIngressBytes + initial.nSchedulerBytes );
	SIR_TEST_CHECK( initial.nIngressBytes >= 64 * 256 && initial.nIngressCalls == 0 );

	uint64_t nPointerFrame = 0;
	SIR_TEST_CHECK( runtime.postEvent( 26, { &nPointerFrame, 8, 8 }, SIR_SCRIPT_PHASE_EARLY ) == SIR_SCRIPT_TYPE_MISMATCH );
	uint32_t nBad = 5;
	SIR_TEST_CHECK( runtime.postEvent( 25, { &nBad, 4, 4 }, SIR_SCRIPT_PHASE_COUNT ) == SIR_SCRIPT_INVALID_ARGUMENT );

	std::atomic<uint32_t> nProducerFailures( 0 );
	std::thread workers[4];
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		workers[iWorker] = std::thread( [&runtime, &nProducerFailures, iWorker]() {
			for ( uint32_t iEvent = 0; iEvent < 16; ++iEvent ) {
				uint32_t nValue = 1;
				const sirScriptPhase_t ePhase = iWorker % 2 == 0 ? SIR_SCRIPT_PHASE_EARLY : SIR_SCRIPT_PHASE_LATE;
				if ( runtime.postEvent( 25, { &nValue, 4, 4 }, ePhase ) != SIR_SCRIPT_OK ) {
					nProducerFailures.fetch_add( 1, std::memory_order_relaxed );
				}
			}
		} );
	}
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		workers[iWorker].join();
	}
	SIR_TEST_CHECK( nProducerFailures.load() == 0 );
	SIR_TEST_CHECK( runtime.queryMemoryFootprint().nIngressCalls == 64 );
	SIR_TEST_CHECK( runtime.pendingCount( SIR_SCRIPT_PHASE_EARLY ) == 0 );
	uint32_t nProcessed = 0;
	SIR_TEST_CHECK( runtime.pumpIngress( SIR_SCRIPT_PHASE_EARLY, 12, &nProcessed ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 12 && backend.nInvokes == 12 );
	SIR_TEST_CHECK( runtime.queryMemoryFootprint().nIngressCalls == 52 );
	backend.bPostFromInvoke = true;
	SIR_TEST_CHECK( runtime.pumpIngress( SIR_SCRIPT_PHASE_EARLY, 100, &nProcessed ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 20 && backend.eEnqueueStatus == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.queryMemoryFootprint().nIngressCalls == 33 );
	SIR_TEST_CHECK( runtime.pumpIngress( SIR_SCRIPT_PHASE_LATE, 100, &nProcessed ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 32 );
	SIR_TEST_CHECK( runtime.pumpIngress( SIR_SCRIPT_PHASE_EARLY, 100, &nProcessed ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 1 && backend.nLastEvent == 777 );
	SIR_TEST_CHECK( backend.nInvokes == 65 && backend.nEventSum == 841 && !backend.bWrongInvokeThread );
	SIR_TEST_CHECK( runtime.queryMemoryFootprint().nIngressCalls == 0 );

	SIR_TEST_CHECK( runtime.shutdownAll() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.postEvent( 25, { &nBad, 4, 4 }, SIR_SCRIPT_PHASE_EARLY ) == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( runtime.queryMemoryFootprint().nIngressCalls == 0 );
	return true;
}

static bool testConcurrentIngress( void ) noexcept
{
	SIRScriptIngress ingress;
	std::atomic<uint32_t> nFinished( 0 );
	std::atomic<uint32_t> nFailures( 0 );
	std::thread workers[4];
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		workers[iWorker] = std::thread( [&ingress, &nFinished, &nFailures, iWorker]() {
			for ( uint32_t iEvent = 0; iEvent < 320; ++iEvent ) {
				sirScriptIngressPacket_t packet = {};
				packet.nEventTypeId = iWorker + 1;
				packet.nBytes = sizeof( uint32_t );
				packet.nAlignment = alignof( uint32_t );
				const uint32_t nValue = iWorker * 320 + iEvent;
				memcpy( packet.szPayload, &nValue, sizeof( nValue ) );
				for ( ;; ) {
					const sirScriptResult_t eResult = ingress.tryPost( SIR_SCRIPT_PHASE_EARLY, packet );
					if ( eResult == SIR_SCRIPT_OK ) {
						break;
					}
					if ( eResult != SIR_SCRIPT_CAPACITY_EXCEEDED ) {
						nFailures.fetch_add( 1, std::memory_order_relaxed );
						break;
					}
					std::this_thread::yield();
				}
			}
			nFinished.fetch_add( 1, std::memory_order_release );
		} );
	}
	uint32_t nReceived = 0;
	uint64_t nSum = 0;
	while ( nFinished.load( std::memory_order_acquire ) != 4 || ingress.pendingCount( SIR_SCRIPT_PHASE_EARLY ) != 0 ) {
		const uint64_t nEnd = ingress.snapshotTail( SIR_SCRIPT_PHASE_EARLY );
		for ( uint32_t iTask = 0; iTask < 32; ++iTask ) {
			sirScriptIngressPacket_t packet = {};
			if ( !ingress.tryPop( SIR_SCRIPT_PHASE_EARLY, nEnd, &packet ) ) {
				break;
			}
			uint32_t nValue = 0;
			memcpy( &nValue, packet.szPayload, sizeof( nValue ) );
			nSum += nValue;
			++nReceived;
		}
		std::this_thread::yield();
	}
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		workers[iWorker].join();
	}
	SIR_TEST_CHECK( nReceived == 1280 && nFailures.load() == 0 );
	SIR_TEST_CHECK( nSum == ( uint64_t )1279 * 1280 / 2 );
	ingress.close();
	return true;
}

static bool testIngressCapacity( void ) noexcept
{
	SIRScriptIngress ingress;
	sirScriptIngressPacket_t packet = {};
	packet.nEventTypeId = 123;
	packet.nBytes = 4;
	packet.nAlignment = 4;
	for ( uint32_t iEvent = 0; iEvent < 64; ++iEvent ) {
		SIR_TEST_CHECK( ingress.tryPost( SIR_SCRIPT_PHASE_EARLY, packet ) == SIR_SCRIPT_OK );
	}
	SIR_TEST_CHECK( ingress.pendingCount( SIR_SCRIPT_PHASE_EARLY ) == 64 );
	SIR_TEST_CHECK( ingress.tryPost( SIR_SCRIPT_PHASE_EARLY, packet ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	const uint64_t nSnapshot = ingress.snapshotTail( SIR_SCRIPT_PHASE_EARLY );
	for ( uint32_t iEvent = 0; iEvent < 64; ++iEvent ) {
		sirScriptIngressPacket_t read = {};
		SIR_TEST_CHECK( ingress.tryPop( SIR_SCRIPT_PHASE_EARLY, nSnapshot, &read ) );
		SIR_TEST_CHECK( read.nEventTypeId == 123 );
	}
	SIR_TEST_CHECK( ingress.pendingCount( SIR_SCRIPT_PHASE_EARLY ) == 0 );
	SIR_TEST_CHECK( ingress.tryPost( SIR_SCRIPT_PHASE_EARLY, packet ) == SIR_SCRIPT_OK );
	ingress.close();
	SIR_TEST_CHECK( ingress.tryPost( SIR_SCRIPT_PHASE_EARLY, packet ) == SIR_SCRIPT_BUSY );
	ingress.clear();
	SIR_TEST_CHECK( ingress.pendingCount( SIR_SCRIPT_PHASE_EARLY ) == 0 );
	return true;
}

typedef struct sirScriptTestPayloadHeap_s
{
	uint32_t nAllocations;
	uint32_t nReleases;
	uint32_t nLive;
	bool bRejectNext;
} sirScriptTestPayloadHeap_t;

static sirScriptResult_t testAllocatePayload( void *pContext, uint32_t nBytes, uint32_t nAlignment,
                                              sirScriptPayloadAllocation_t *pAllocation ) noexcept
{
	sirScriptTestPayloadHeap_t *pHeap = ( sirScriptTestPayloadHeap_t * )pContext;
	*pAllocation = {};
	if ( pHeap->bRejectNext ) {
		pHeap->bRejectNext = false;
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	void *pMemory = ::operator new( nBytes, std::align_val_t( nAlignment ), std::nothrow );
	if ( pMemory == nullptr ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	++pHeap->nAllocations;
	++pHeap->nLive;
	pAllocation->pData = pMemory;
	pAllocation->nToken = ( ( uint64_t )nAlignment << 32 ) | pHeap->nAllocations;
	return SIR_SCRIPT_OK;
}

static void testReleasePayload( void *pContext, sirScriptPayloadAllocation_t allocation ) noexcept
{
	sirScriptTestPayloadHeap_t *pHeap = ( sirScriptTestPayloadHeap_t * )pContext;
	const uint32_t nAlignment = ( uint32_t )( allocation.nToken >> 32 );
	::operator delete( allocation.pData, std::align_val_t( nAlignment ) );
	++pHeap->nReleases;
	--pHeap->nLive;
}

static sirScriptPayloadAllocatorAPI_t testPayloadAPI( sirScriptTestPayloadHeap_t *pHeap ) noexcept
{
	return { SIR_SCRIPT_PAYLOAD_ALLOCATOR_ABI_REVISION, pHeap, 1024, 256, testAllocatePayload, testReleasePayload };
}

static bool testAllocatedScheduler( void ) noexcept
{
	sirScriptTestPayloadHeap_t heap = {};
	SIRScriptScheduler scheduler;
	sirScriptScheduledCall_t call = {};
	call.nDispatchId = 55;
	call.nAlignment = 64;
	call.nBytes = 512;
	call.eKind = SIR_SCRIPT_SCHEDULED_EVENT;
	alignas( 64 ) unsigned char szPayload[512] = {};
	for ( uint32_t iByte = 0; iByte < sizeof( szPayload ); ++iByte ) {
		szPayload[iByte] = ( unsigned char )( iByte & 255 );
	}
	sirScriptTaskHandle_t first = {};
	SIR_TEST_CHECK( scheduler.enqueue( SIR_SCRIPT_PHASE_EARLY, call, szPayload, &first ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	sirScriptPayloadAllocatorAPI_t invalid = testPayloadAPI( &heap );
	invalid.nABIRevision = 90;
	SIR_TEST_CHECK( scheduler.setPayloadAllocator( invalid ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( scheduler.setPayloadAllocator( testPayloadAPI( &heap ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( scheduler.enqueue( SIR_SCRIPT_PHASE_EARLY, call, szPayload, &first ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( first.nSlot != 0 && heap.nLive == 1 && scheduler.allocatedPayloadBytes() == 512 );
	SIR_TEST_CHECK( scheduler.setPayloadAllocator( {} ) == SIR_SCRIPT_BUSY );
	memset( szPayload, 0, sizeof( szPayload ) );
	sirScriptScheduledCall_t transferred = {};
	SIR_TEST_CHECK( scheduler.dequeue( SIR_SCRIPT_PHASE_EARLY, scheduler.lastSequence(), &transferred ) );
	SIR_TEST_CHECK( transferred.allocation.pData != nullptr && transferred.nBytes == 512 );
	SIR_TEST_CHECK( ( ( unsigned char * )transferred.allocation.pData )[129] == 129 );
	SIR_TEST_CHECK( scheduler.cancel( first ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( scheduler.livePayloadCount() == 1 );
	SIR_TEST_CHECK( scheduler.setPayloadAllocator( {} ) == SIR_SCRIPT_BUSY );
	scheduler.releasePayload( &transferred );
	SIR_TEST_CHECK( heap.nLive == 0 && scheduler.allocatedPayloadBytes() == 0 );
	SIR_TEST_CHECK( scheduler.peakPayloadBytes() == 512 );
	SIR_TEST_CHECK( scheduler.setPayloadAllocator( {} ) == SIR_SCRIPT_OK );

	SIR_TEST_CHECK( scheduler.setPayloadAllocator( testPayloadAPI( &heap ) ) == SIR_SCRIPT_OK );
	call.nBytes = 512;
	heap.bRejectNext = true;
	SIR_TEST_CHECK( scheduler.enqueue( SIR_SCRIPT_PHASE_EARLY, call, szPayload, &first ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( scheduler.pendingCount( SIR_SCRIPT_PHASE_EARLY ) == 0 && heap.nLive == 0 );
	SIR_TEST_CHECK( scheduler.enqueue( SIR_SCRIPT_PHASE_EARLY, call, szPayload, &first ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( scheduler.cancel( first ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( heap.nLive == 0 && heap.nReleases == 2 );
	SIR_TEST_CHECK( scheduler.enqueue( SIR_SCRIPT_PHASE_LATE, call, szPayload, &first ) == SIR_SCRIPT_OK );
	scheduler.clear();
	SIR_TEST_CHECK( heap.nLive == 0 && heap.nReleases == 3 );
	return true;
}

static bool testRuntimePayloadHeap( void ) noexcept
{
	sirScriptTestPayloadHeap_t heap = {};
	SIRScriptServer server;
	SIRScriptBindingRegistry &registry = server.bindings();
	SIRScriptRuntime &runtime = server.runtime();
	SIR_TEST_CHECK( registry.registerType( { 18, 512, 16, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerType( { 19, 128, 128, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerType( { 20, 2048, 16, SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 80, 18 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 81, 19 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.registerEvent( { 82, 20 } ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	sirScriptTestProgramSource_t source = {};
	sirScriptTestBackend_t backend = {};
	backend.pRuntime = &runtime;
	SIR_TEST_CHECK( runtime.setProgramSource( testProgramSourceAPI( &source ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.registerBackend( 1, testBackendAPI( &backend ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.startBackend( 1 ) == SIR_SCRIPT_OK );
	sirScriptProgramHandle_t program = {};
	sirScriptInstanceHandle_t instance = {};
	sirScriptSubscriptionHandle_t subscription = {};
	SIR_TEST_CHECK( runtime.loadProgram( 1, 321, 1, &program ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.createInstance( program, 123, &instance ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.subscribe( instance, 80, 91, &subscription ) == SIR_SCRIPT_OK );
	alignas( 16 ) unsigned char szLarge[512] = {};
	alignas( 128 ) unsigned char szWide[128] = {};
	alignas( 16 ) unsigned char szTooLarge[2048] = {};
	szLarge[400] = 42;
	sirScriptTaskHandle_t first = {};
	sirScriptTaskHandle_t second = {};
	SIR_TEST_CHECK( runtime.enqueueEvent( 80, { szLarge, 512, 16 }, SIR_SCRIPT_PHASE_EARLY, &first ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( runtime.setPayloadAllocator( testPayloadAPI( &heap ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.enqueueEvent( 80, { szLarge, 512, 16 }, SIR_SCRIPT_PHASE_EARLY, &first ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.enqueueEvent( 81, { szWide, 128, 128 }, SIR_SCRIPT_PHASE_LATE, &second ) == SIR_SCRIPT_OK );
	sirScriptTaskHandle_t invalid = {};
	SIR_TEST_CHECK( runtime.enqueueEvent( 82, { szTooLarge, 2048, 16 }, SIR_SCRIPT_PHASE_EARLY, &invalid ) ==
	                SIR_SCRIPT_CAPACITY_EXCEEDED );
	const sirScriptRuntimeFootprint_t footprint = runtime.queryMemoryFootprint();
	SIR_TEST_CHECK( footprint.nExternalPayloadBytes == 640 && footprint.nLiveExternalPayloads == 2 );
	SIR_TEST_CHECK( footprint.nPeakExternalPayloadBytes == 640 && footprint.nScheduledCalls == 2 );
	SIR_TEST_CHECK( runtime.setPayloadAllocator( {} ) == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( runtime.cancelScheduled( first ) == SIR_SCRIPT_OK );
	uint32_t nProcessed = 0;
	uint32_t nStale = 0;
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_LATE, 1, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nProcessed == 1 && nStale == 0 && heap.nLive == 0 );
	SIR_TEST_CHECK( runtime.cancelScheduled( second ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( runtime.queryMemoryFootprint().nExternalPayloadBytes == 0 );
	SIR_TEST_CHECK( runtime.setPayloadAllocator( {} ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.setPayloadAllocator( testPayloadAPI( &heap ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.enqueueEvent( 80, { szLarge, 512, 16 }, SIR_SCRIPT_PHASE_EARLY, &first ) == SIR_SCRIPT_OK );
	szLarge[400] = 0;
	SIR_TEST_CHECK( runtime.pumpPhase( SIR_SCRIPT_PHASE_EARLY, 1, &nProcessed, &nStale ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( backend.nLargeMarker == 42 && backend.nPayloadsAtInvoke == 1 );
	SIR_TEST_CHECK( heap.nLive == 0 );
	SIR_TEST_CHECK( runtime.enqueueEvent( 80, { szLarge, 512, 16 }, SIR_SCRIPT_PHASE_EARLY, &first ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( runtime.shutdownAll() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( heap.nLive == 0 && heap.nAllocations == heap.nReleases );
	SIR_TEST_CHECK( runtime.queryMemoryFootprint().nExternalPayloadBytes == 0 );
	return true;
}

int main( void )
{
	if ( !testBindings() || !testSchemaFingerprint() || !testRuntime() || !testScheduler() || !testShutdownAll() ||
	     !testCrossBackendEvents() || !testProgramSourceLeases() || !testCrossThreadIngress() || !testIngressCapacity() ||
	     !testConcurrentIngress() || !testAllocatedScheduler() || !testRuntimePayloadHeap() ) {
		return 1;
	}
	puts( "SIREngine scripting foundation PASS" );
	return 0;
}
