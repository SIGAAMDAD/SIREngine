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

#include <Core/Jobs/SIRTaskGraph.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

#define SIR_PROVIDER_CHECK( expression )                                                                                                   \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Provider test failed %s:%d: %s\n", __FILE__, __LINE__, #expression );                                        \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static sirTaskResult_t providerDeferred( void *pContext, sirTaskHandle_t ) noexcept
{
	( ( std::atomic<uint32_t> * )pContext )->fetch_add( 1 );
	return SIR_TASK_RESULT_PENDING;
}

static sirTaskResult_t providerCount( void *pContext, sirTaskHandle_t ) noexcept
{
	( ( std::atomic<uint32_t> * )pContext )->fetch_add( 1 );
	return SIR_TASK_RESULT_SUCCESS;
}

static void initProviderMemory( SIRMemoryManager& memory )
{
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 };
	SIR_PROVIDER_CHECK( memory.init( config ) );
}

static void testDeferredCompletionOnRegisteredThread( void )
{
	SIRMemoryManager memory;
	initProviderMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_PROVIDER_CHECK( scheduler.init( &memory, { 16, 0, 32 } ) );
	std::atomic<uint32_t> nLaunch( 0 );
	std::atomic<uint32_t> nCompleted( 0 );
	std::atomic<bool> bBound( false );
	std::atomic<bool> bPump( false );
	std::atomic<bool> bPumped( false );
	std::atomic<bool> bAcknowledge( false );
	std::atomic<bool> bDone( false );
	sirTaskHandle_t render = {};
	SIR_PROVIDER_CHECK( scheduler.submit({ providerDeferred, &nLaunch, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &render) );

	std::thread provider( [&]() {
		SIR_PROVIDER_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
		bBound.store( true );
		while ( !bPump.load() ) {
			std::this_thread::yield();
		}
		SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 10 ) == 1 );
		bPumped.store( true );
		while ( !bAcknowledge.load() ) {
			std::this_thread::yield();
		}
		SIR_PROVIDER_CHECK( scheduler.completeProvider( render, SIR_TASK_RESULT_SUCCESS ) );
		SIR_PROVIDER_CHECK( !scheduler.completeProvider( render, SIR_TASK_RESULT_SUCCESS ) );
		bDone.store( true );
	} );
	while ( !bBound.load() ) {
		std::this_thread::yield();
	}
	SIR_PROVIDER_CHECK( !scheduler.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
	SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 0 );
	SIR_PROVIDER_CHECK( scheduler.getState( render ) == SIR_TASK_QUEUED );
	bPump.store( true );
	while ( !bPumped.load() ) {
		std::this_thread::yield();
	}
	SIR_PROVIDER_CHECK( nLaunch.load() == 1 );
	SIR_PROVIDER_CHECK( scheduler.getState( render ) == SIR_TASK_AWAITING_PROVIDER );
	SIR_PROVIDER_CHECK( scheduler.getStats().nAwaitingProvider == 1 );
	sirTaskHandle_t dependent = {};
	SIR_PROVIDER_CHECK(
	    scheduler.submitAfter({ providerCount, &nCompleted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &render, 1, &dependent) );
	SIR_PROVIDER_CHECK( scheduler.getState( dependent ) == SIR_TASK_BLOCKED );
	SIR_PROVIDER_CHECK( scheduler.pumpCPU( 10 ) == 0 );
	SIR_PROVIDER_CHECK( !scheduler.completeProvider( render, SIR_TASK_RESULT_SUCCESS ) );
	SIR_PROVIDER_CHECK( !scheduler.releaseTask( render ) );
	SIR_PROVIDER_CHECK( !scheduler.shutdown() );
	bAcknowledge.store( true );
	while ( !bDone.load() ) {
		std::this_thread::yield();
	}
	provider.join();
	SIR_PROVIDER_CHECK( scheduler.getState( render ) == SIR_TASK_COMPLETED );
	SIR_PROVIDER_CHECK( scheduler.getState( dependent ) == SIR_TASK_QUEUED );
	SIR_PROVIDER_CHECK( scheduler.getStats().nAwaitingProvider == 0 );
	SIR_PROVIDER_CHECK( scheduler.pumpCPU( 10 ) == 1 );
	SIR_PROVIDER_CHECK( nCompleted.load() == 1 );
	SIR_PROVIDER_CHECK( scheduler.releaseTask( render ) );
	SIR_PROVIDER_CHECK( scheduler.releaseTask( dependent ) );
	SIR_PROVIDER_CHECK( scheduler.getState( render ) == SIR_TASK_INVALID );
	SIR_PROVIDER_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testCancellationRequiresProviderAcknowledgement( void )
{
	SIRMemoryManager memory;
	initProviderMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_PROVIDER_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
	SIR_PROVIDER_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_PHYSICS ) );
	std::atomic<uint32_t> nCalls( 0 );
	sirTaskHandle_t physics = {};
	SIR_PROVIDER_CHECK( scheduler.submit({ providerDeferred, &nCalls, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_PHYSICS }, &physics) );
	SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_PHYSICS, 1 ) == 1 );
	sirTaskHandle_t child = {};
	SIR_PROVIDER_CHECK( scheduler.submitAfter({ providerCount, &nCalls, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &physics, 1, &child) );
	SIR_PROVIDER_CHECK( scheduler.cancel( physics ) );
	SIR_PROVIDER_CHECK( scheduler.isCancellationRequested( physics ) );
	SIR_PROVIDER_CHECK( scheduler.getState( physics ) == SIR_TASK_AWAITING_PROVIDER );
	SIR_PROVIDER_CHECK( scheduler.getState( child ) == SIR_TASK_BLOCKED );
	SIR_PROVIDER_CHECK( !scheduler.shutdown() );
	SIR_PROVIDER_CHECK( scheduler.completeProvider( physics, SIR_TASK_RESULT_SUCCESS ) );
	SIR_PROVIDER_CHECK( scheduler.getState( physics ) == SIR_TASK_CANCELLED );
	SIR_PROVIDER_CHECK( scheduler.getState( child ) == SIR_TASK_CANCELLED );
	SIR_PROVIDER_CHECK( !scheduler.completeProvider( physics, SIR_TASK_RESULT_SUCCESS ) );
	SIR_PROVIDER_CHECK( scheduler.releaseTask( physics ) );
	SIR_PROVIDER_CHECK( scheduler.releaseTask( child ) );
	SIR_PROVIDER_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testProviderGraphAcrossSafePoints( void )
{
	SIRMemoryManager memory;
	initProviderMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_PROVIDER_CHECK( scheduler.init( &memory, { 12, 0, 24 } ) );
	SIR_PROVIDER_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
	SIR_PROVIDER_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_PHYSICS ) );
	SIR_PROVIDER_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_AUDIO ) );
	SIRTaskGraph graph;
	SIR_PROVIDER_CHECK( graph.init( &memory, { 8, 8, 16 } ) );
	std::atomic<uint32_t> nCount( 0 );
	const sirTaskGraphResourceKey_t keyResource = { 0x63, 0x91 };
	const sirTaskGraphAccess_t access = { keyResource, 0, 128, SIR_TASK_GRAPH_WRITE };
	const sirTaskGraphPass_t passes[] = { { providerCount, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU },
	                                      { providerDeferred, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER },
	                                      { providerDeferred, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_PHYSICS },
	                                      { providerDeferred, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_AUDIO },
	                                      { providerCount, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU } };
	for ( uint32_t iPass = 0; iPass < 5; ++iPass ) {
		uint32_t nIndex = UINT32_MAX;
		SIR_PROVIDER_CHECK( graph.addPass( passes[iPass], &access, 1, &nIndex ) );
		SIR_PROVIDER_CHECK( nIndex == iPass );
	}
	SIR_PROVIDER_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_PROVIDER_CHECK( graph.submit( &scheduler ) );
	sirTaskHandle_t handles[5] = {};
	for ( uint32_t i = 0; i < 5; ++i ) {
		SIR_PROVIDER_CHECK( graph.getSubmittedHandle( i, &handles[i] ) );
	}
	SIR_PROVIDER_CHECK( scheduler.pumpCPU( 10 ) == 1 );
	SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 10 ) == 1 );
	SIR_PROVIDER_CHECK( scheduler.getState( handles[1] ) == SIR_TASK_AWAITING_PROVIDER );
	SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_PHYSICS, 10 ) == 0 );
	SIR_PROVIDER_CHECK( scheduler.completeProvider( handles[1], SIR_TASK_RESULT_SUCCESS ) );
	SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_PHYSICS, 10 ) == 1 );
	SIR_PROVIDER_CHECK( scheduler.completeProvider( handles[2], SIR_TASK_RESULT_FAILURE ) );
	SIR_PROVIDER_CHECK( scheduler.getState( handles[2] ) == SIR_TASK_FAILED );
	SIR_PROVIDER_CHECK( scheduler.getState( handles[3] ) == SIR_TASK_CANCELLED );
	SIR_PROVIDER_CHECK( scheduler.getState( handles[4] ) == SIR_TASK_CANCELLED );
	SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_AUDIO, 10 ) == 0 );
	SIR_PROVIDER_CHECK( nCount.load() == 3 );
	SIR_PROVIDER_CHECK( graph.releaseSubmitted() );
	SIR_PROVIDER_CHECK( scheduler.getActiveCount() == 0 );
	SIR_PROVIDER_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

typedef struct sirProviderBlockingContext_s
{
	std::atomic<bool> bRunning;
	std::atomic<bool> bReturn;
} sirProviderBlockingContext_t;

static sirTaskResult_t providerBlockingCallback( void *pContext, sirTaskHandle_t ) noexcept
{
	sirProviderBlockingContext_t *pBlock = ( sirProviderBlockingContext_t * )pContext;
	pBlock->bRunning.store( true );
	while ( !pBlock->bReturn.load() ) {
		std::this_thread::yield();
	}
	return SIR_TASK_RESULT_SUCCESS;
}

static void testShutdownDuringProviderCallback( void )
{
	SIRMemoryManager memory;
	initProviderMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_PROVIDER_CHECK( scheduler.init( &memory, { 4, 0, 8 } ) );
	sirProviderBlockingContext_t block = {};
	sirTaskHandle_t task = {};
	SIR_PROVIDER_CHECK( scheduler.submit({ providerBlockingCallback, &block, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &task) );
	std::thread render( [&]() {
		SIR_PROVIDER_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
		SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
	} );
	while ( !block.bRunning.load() ) {
		std::this_thread::yield();
	}
	SIR_PROVIDER_CHECK( scheduler.getState( task ) == SIR_TASK_RUNNING );
	SIR_PROVIDER_CHECK( !scheduler.shutdown() );
	SIR_PROVIDER_CHECK( !scheduler.releaseTask( task ) );
	block.bReturn.store( true );
	render.join();
	SIR_PROVIDER_CHECK( scheduler.getState( task ) == SIR_TASK_COMPLETED );
	SIR_PROVIDER_CHECK( scheduler.releaseTask( task ) );
	SIR_PROVIDER_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testWorkerProviderHandoff( void )
{
	SIRMemoryManager memory;
	initProviderMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_PROVIDER_CHECK( scheduler.init( &memory, { 24, 2, 24 } ) );
	SIR_PROVIDER_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
	std::atomic<uint32_t> nCount( 0 );
	sirTaskHandle_t root = {};
	sirTaskHandle_t render = {};
	sirTaskHandle_t child = {};
	SIR_PROVIDER_CHECK( scheduler.submit({ providerCount, &nCount, SIR_TASK_PRIORITY_CRITICAL, SIR_TASK_EXEC_CPU }, &root) );
	SIR_PROVIDER_CHECK(
	    scheduler.submitAfter({ providerDeferred, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &root, 1, &render) );
	SIR_PROVIDER_CHECK( scheduler.submitAfter({ providerCount, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &render, 1, &child) );
	SIR_PROVIDER_CHECK( scheduler.waitTask( root ) );
	SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
	SIR_PROVIDER_CHECK( scheduler.getState( child ) == SIR_TASK_BLOCKED );
	SIR_PROVIDER_CHECK( scheduler.completeProvider( render, SIR_TASK_RESULT_SUCCESS ) );
	SIR_PROVIDER_CHECK( scheduler.waitTask( child ) );
	SIR_PROVIDER_CHECK( nCount.load() == 3 );
	SIR_PROVIDER_CHECK( scheduler.releaseTask( root ) );
	SIR_PROVIDER_CHECK( scheduler.releaseTask( render ) );
	SIR_PROVIDER_CHECK( scheduler.releaseTask( child ) );
	SIR_PROVIDER_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testPendingCpuCallbackIsFailure( void )
{
	SIRMemoryManager memory;
	initProviderMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_PROVIDER_CHECK( scheduler.init( &memory, { 4, 0, 4 } ) );
	std::atomic<uint32_t> nCount( 0 );
	sirTaskHandle_t cpu = {};
	SIR_PROVIDER_CHECK( scheduler.submit({ providerDeferred, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &cpu) );
	SIR_PROVIDER_CHECK( scheduler.pumpCPU( 1 ) == 1 );
	SIR_PROVIDER_CHECK( scheduler.getState( cpu ) == SIR_TASK_FAILED );
	SIR_PROVIDER_CHECK( scheduler.releaseTask( cpu ) );
	SIR_PROVIDER_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testBurstAndInvalidCompletions( void )
{
	SIRMemoryManager memory;
	initProviderMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_PROVIDER_CHECK( scheduler.init( &memory, { 64, 0, 64 } ) );
	std::atomic<uint32_t> nCalls( 0 );
	SIR_PROVIDER_CHECK( !scheduler.bindProviderOwner( SIR_TASK_EXEC_CPU ) );
	SIR_PROVIDER_CHECK( !scheduler.bindProviderOwner( SIR_TASK_EXEC_MAIN ) );
	SIR_PROVIDER_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_AUDIO ) );
	sirTaskHandle_t handles[32] = {};
	for ( uint32_t i = 0; i < 32; ++i ) {
		SIR_PROVIDER_CHECK( scheduler.submit({ providerDeferred, &nCalls, SIR_TASK_PRIORITY_BACKGROUND, SIR_TASK_EXEC_AUDIO }, &handles[i]) );
	}
	SIR_PROVIDER_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_AUDIO, 32 ) == 32 );
	SIR_PROVIDER_CHECK( scheduler.getStats().nAwaitingProvider == 32 );
	SIR_PROVIDER_CHECK( !scheduler.completeProvider( handles[0], SIR_TASK_RESULT_PENDING ) );
	for ( uint32_t i = 32; i > 0; --i ) {
		SIR_PROVIDER_CHECK( scheduler.completeProvider( handles[i - 1], SIR_TASK_RESULT_SUCCESS ) );
		SIR_PROVIDER_CHECK( scheduler.releaseTask( handles[i - 1] ) );
	}
	SIR_PROVIDER_CHECK( nCalls.load() == 32 );
	SIR_PROVIDER_CHECK( scheduler.getStats().nAwaitingProvider == 0 );
	SIR_PROVIDER_CHECK( !scheduler.completeProvider( handles[0], SIR_TASK_RESULT_SUCCESS ) );
	SIR_PROVIDER_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

int main( void )
{
	testDeferredCompletionOnRegisteredThread();
	testCancellationRequiresProviderAcknowledgement();
	testProviderGraphAcrossSafePoints();
	testWorkerProviderHandoff();
	testPendingCpuCallbackIsFailure();
	testShutdownDuringProviderCallback();
	testBurstAndInvalidCompletions();
	return 0;
}
