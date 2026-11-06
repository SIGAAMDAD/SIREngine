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

#include <Core/Jobs/SIRTaskFenceTracker.h>
#include <Core/Jobs/SIRTaskGraph.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

#define SIR_FENCE_CHECK( condition )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Fence test failed %s:%d: %s\n", __FILE__, __LINE__, #condition );                                            \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirFenceFinalizerContext_s
{
	uint32_t nCalls;
	uint32_t nReleased;
	bool bAllow;
} sirFenceFinalizerContext_t;

static bool releaseFenceResource( void *pContext ) noexcept
{
	sirFenceFinalizerContext_t *pInfo = ( sirFenceFinalizerContext_t * )pContext;
	++pInfo->nCalls;
	if ( !pInfo->bAllow ) {
		return false;
	}
	++pInfo->nReleased;
	return true;
}

typedef struct sirFenceReentryContext_s
{
	SIRTaskFenceTracker *pTracker;
	uint32_t nDenied;
} sirFenceReentryContext_t;

static bool rejectNestedAcknowledgement( void *pContext ) noexcept
{
	sirFenceReentryContext_t *pReentry = ( sirFenceReentryContext_t * )pContext;
	uint32_t nAcknowledged = UINT32_MAX;
	if ( !pReentry->pTracker->advanceCompleted( 99, SIR_TASK_RESULT_SUCCESS, &nAcknowledged ) && nAcknowledged == 0 &&
	     !pReentry->pTracker->hasCapacity() && !pReentry->pTracker->untrackLast( {} ) && !pReentry->pTracker->shutdown() &&
	     !pReentry->pTracker->track( {}, 999 ) ) {
		++pReentry->nDenied;
		return true;
	}
	return false;
}

static sirTaskResult_t deferredProvider( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_PENDING;
}

static sirTaskResult_t incrementCounter( void *pContext, sirTaskHandle_t ) noexcept
{
	( ( std::atomic<uint32_t> * )pContext )->fetch_add( 1 );
	return SIR_TASK_RESULT_SUCCESS;
}

typedef struct sirFenceTrackedContext_s
{
	SIRTaskFenceTracker *pTracker;
	sirFenceFinalizerContext_t *pFinalizer;
	uint64_t nFence;
	bool bUndo;
} sirFenceTrackedContext_t;

static sirTaskResult_t registerFenceInsideCallback( void *pContext, sirTaskHandle_t task ) noexcept
{
	sirFenceTrackedContext_t *pTask = ( sirFenceTrackedContext_t * )pContext;
	if ( !pTask->pTracker->track( task, pTask->nFence, releaseFenceResource, pTask->pFinalizer ) ) {
		return SIR_TASK_RESULT_FAILURE;
	}
	if ( pTask->bUndo ) {
		SIR_FENCE_CHECK( pTask->pTracker->untrackLast( task ) );
		return SIR_TASK_RESULT_FAILURE;
	}
	return SIR_TASK_RESULT_PENDING;
}

static void initFenceMemory( SIRMemoryManager& memory )
{
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 };
	SIR_FENCE_CHECK( memory.init( config ) );
}

static void testMonotonicFenceAndSafePoint( void )
{
	SIRMemoryManager memory;
	initFenceMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_FENCE_CHECK( scheduler.init( &memory, { 16, 0, 32 } ) );
	SIRTaskFenceTracker tracker;
	SIR_FENCE_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 3 ) );
	sirTaskHandle_t tasks[3] = {};
	for ( uint32_t i = 0; i < 3; ++i ) {
		SIR_FENCE_CHECK( scheduler.submit( { deferredProvider, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &tasks[i] ) );
	}
	SIR_FENCE_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 3 ) == 3 );
	sirTaskState_t eState = SIR_TASK_INVALID;
	sirTaskExecutor_t eExecutor = SIR_TASK_EXEC_CPU;
	SIR_FENCE_CHECK( scheduler.queryTask( tasks[0], &eState, &eExecutor ) );
	SIR_FENCE_CHECK( eState == SIR_TASK_AWAITING_PROVIDER && eExecutor == SIR_TASK_EXEC_RENDER );
	sirFenceFinalizerContext_t finalizer[3] = { { 0, 0, true }, { 0, 0, true }, { 0, 0, true } };
	SIR_FENCE_CHECK( tracker.track( tasks[0], 4, releaseFenceResource, &finalizer[0] ) );
	SIR_FENCE_CHECK( !tracker.track( tasks[0], 4 ) );
	SIR_FENCE_CHECK( tracker.track( tasks[1], 4, releaseFenceResource, &finalizer[1] ) );
	SIR_FENCE_CHECK( tracker.track( tasks[2], 7, releaseFenceResource, &finalizer[2] ) );
	SIR_FENCE_CHECK( !tracker.hasCapacity() );
	SIR_FENCE_CHECK( !tracker.track( tasks[2], 8 ) );
	SIR_FENCE_CHECK( !tracker.shutdown() );
	SIR_FENCE_CHECK( !scheduler.shutdown() );
	std::thread wrongThread( [&]() {
		uint32_t nDone = UINT32_MAX;
		SIR_FENCE_CHECK( !tracker.advanceCompleted( 7, SIR_TASK_RESULT_SUCCESS, &nDone ) );
		SIR_FENCE_CHECK( nDone == 0 );
		SIR_FENCE_CHECK( !tracker.track( tasks[0], 9 ) );
	} );
	wrongThread.join();
	uint32_t nDone = UINT32_MAX;
	SIR_FENCE_CHECK( tracker.advanceCompleted( 3, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 0 );
	SIR_FENCE_CHECK( tracker.advanceCompleted( 4, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 2 );
	SIR_FENCE_CHECK( finalizer[0].nReleased == 1 && finalizer[1].nReleased == 1 && finalizer[2].nReleased == 0 );
	SIR_FENCE_CHECK( scheduler.getState( tasks[0] ) == SIR_TASK_COMPLETED );
	SIR_FENCE_CHECK( scheduler.getState( tasks[2] ) == SIR_TASK_AWAITING_PROVIDER );
	SIR_FENCE_CHECK( !tracker.track( tasks[0], 4 ) );
	SIR_FENCE_CHECK( tracker.advanceCompleted( 7, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 1 );
	SIR_FENCE_CHECK( tracker.getPendingCount() == 0 );
	for ( uint32_t i = 0; i < 3; ++i ) {
		SIR_FENCE_CHECK( finalizer[i].nReleased == 1 );
		SIR_FENCE_CHECK( scheduler.releaseTask( tasks[i] ) );
	}
	SIR_FENCE_CHECK( tracker.shutdown() );
	SIR_FENCE_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testFinalizerRetryAndCancellation( void )
{
	SIRMemoryManager memory;
	initFenceMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_FENCE_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
	SIRTaskFenceTracker tracker;
	SIR_FENCE_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_PHYSICS, 2 ) );
	sirTaskHandle_t parent = {};
	SIR_FENCE_CHECK( scheduler.submit( { deferredProvider, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_PHYSICS }, &parent ) );
	SIR_FENCE_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_PHYSICS, 1 ) == 1 );
	std::atomic<uint32_t> nCalls( 0 );
	sirTaskHandle_t child = {};
	SIR_FENCE_CHECK(
	    scheduler.submitAfter( { incrementCounter, &nCalls, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &parent, 1, &child ) );
	sirFenceFinalizerContext_t finalizer = { 0, 0, false };
	SIR_FENCE_CHECK( tracker.track( parent, 11, releaseFenceResource, &finalizer ) );
	SIR_FENCE_CHECK( scheduler.cancel( parent ) );
	uint32_t nDone = UINT32_MAX;
	SIR_FENCE_CHECK( !tracker.advanceCompleted( 11, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 0 );
	SIR_FENCE_CHECK( tracker.getPendingCount() == 1 && finalizer.nCalls == 1 );
	SIR_FENCE_CHECK( scheduler.getState( parent ) == SIR_TASK_AWAITING_PROVIDER );
	SIR_FENCE_CHECK( scheduler.getState( child ) == SIR_TASK_BLOCKED );
	finalizer.bAllow = true;
	SIR_FENCE_CHECK( tracker.advanceCompleted( 11, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 1 );
	SIR_FENCE_CHECK( finalizer.nCalls == 2 && finalizer.nReleased == 1 );
	SIR_FENCE_CHECK( scheduler.getState( parent ) == SIR_TASK_CANCELLED );
	SIR_FENCE_CHECK( scheduler.getState( child ) == SIR_TASK_CANCELLED && nCalls.load() == 0 );
	SIR_FENCE_CHECK( scheduler.releaseTask( child ) && scheduler.releaseTask( parent ) );

	sirTaskHandle_t failed = {};
	SIR_FENCE_CHECK( scheduler.submit( { deferredProvider, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_PHYSICS }, &failed ) );
	SIR_FENCE_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_PHYSICS, 1 ) == 1 );
	sirFenceReentryContext_t reentry = { &tracker, 0 };
	SIR_FENCE_CHECK( tracker.track( failed, 15, rejectNestedAcknowledgement, &reentry ) );
	SIR_FENCE_CHECK( tracker.failQuiesced( &nDone ) && nDone == 1 );
	SIR_FENCE_CHECK( reentry.nDenied == 1 );
	SIR_FENCE_CHECK( tracker.failQuiesced( &nDone ) && nDone == 0 );
	SIR_FENCE_CHECK( scheduler.getState( failed ) == SIR_TASK_FAILED );
	SIR_FENCE_CHECK( scheduler.releaseTask( failed ) );
	SIR_FENCE_CHECK( tracker.shutdown() && scheduler.shutdown() );
	memory.shutdown();
}

static void testAdmissionAndRollback( void )
{
	SIRMemoryManager memory;
	initFenceMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_FENCE_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
	SIRTaskFenceTracker tracker;
	SIR_FENCE_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_AUDIO, 1 ) );
	sirFenceFinalizerContext_t f = { 0, 0, true };
	sirFenceTrackedContext_t firstContext = { &tracker, &f, 3, false };
	sirFenceTrackedContext_t secondContext = { &tracker, &f, 4, false };
	sirTaskHandle_t first = {};
	sirTaskHandle_t second = {};
	SIR_FENCE_CHECK(
	    scheduler.submit( { registerFenceInsideCallback, &firstContext, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_AUDIO }, &first ) );
	SIR_FENCE_CHECK(
	    scheduler.submit( { registerFenceInsideCallback, &secondContext, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_AUDIO }, &second ) );
	SIR_FENCE_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_AUDIO, 2 ) == 2 );
	SIR_FENCE_CHECK( scheduler.getState( first ) == SIR_TASK_AWAITING_PROVIDER );
	SIR_FENCE_CHECK( scheduler.getState( second ) == SIR_TASK_FAILED );
	SIR_FENCE_CHECK( !tracker.track( second, 4 ) );
	SIR_FENCE_CHECK( tracker.getPendingCount() == 1 );
	uint32_t nDone = UINT32_MAX;
	SIR_FENCE_CHECK( tracker.advanceCompleted( 3, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 1 );
	SIR_FENCE_CHECK( scheduler.releaseTask( first ) && scheduler.releaseTask( second ) );

	sirFenceTrackedContext_t rollback = { &tracker, &f, 8, true };
	sirTaskHandle_t undone = {};
	SIR_FENCE_CHECK(
	    scheduler.submit( { registerFenceInsideCallback, &rollback, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_AUDIO }, &undone ) );
	SIR_FENCE_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_AUDIO, 1 ) == 1 );
	SIR_FENCE_CHECK( scheduler.getState( undone ) == SIR_TASK_FAILED );
	SIR_FENCE_CHECK( tracker.getPendingCount() == 0 );
	SIR_FENCE_CHECK( scheduler.releaseTask( undone ) );
	SIR_FENCE_CHECK( tracker.shutdown() && scheduler.shutdown() );
	memory.shutdown();
}

static void testGraphIntegration( void )
{
	SIRMemoryManager memory;
	initFenceMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_FENCE_CHECK( scheduler.init( &memory, { 12, 0, 24 } ) );
	SIRTaskFenceTracker tracker;
	SIR_FENCE_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 3 ) );
	SIRTaskGraph graph;
	SIR_FENCE_CHECK( graph.init( &memory, { 4, 4, 8 } ) );
	std::atomic<uint32_t> nCalls( 0 );
	sirFenceFinalizerContext_t finalizer = { 0, 0, true };
	sirFenceTrackedContext_t context = { &tracker, &finalizer, 24, false };
	const sirTaskGraphResourceKey_t keyResource = { 123, 456 };
	const sirTaskGraphAccess_t write = { keyResource, 0, 64, SIR_TASK_GRAPH_WRITE };
	const sirTaskGraphAccess_t read = { keyResource, 0, 64, SIR_TASK_GRAPH_READ };
	uint32_t iPass = UINT32_MAX;
	SIR_FENCE_CHECK(
	    graph.addPass( { registerFenceInsideCallback, &context, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &write, 1, &iPass ) );
	SIR_FENCE_CHECK( graph.addPass( { incrementCounter, &nCalls, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &read, 1, &iPass ) );
	SIR_FENCE_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_FENCE_CHECK( graph.submit( &scheduler ) );
	SIR_FENCE_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
	SIR_FENCE_CHECK( scheduler.pumpCPU( 1 ) == 0 );
	sirTaskGraphExecutionSummary_t summary = {};
	SIR_FENCE_CHECK( graph.queryExecution( &summary ) && summary.eState == SIR_TASK_GRAPH_EXEC_PENDING );
	uint32_t nDone = 0;
	SIR_FENCE_CHECK( tracker.advanceCompleted( 24, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 1 );
	SIR_FENCE_CHECK( scheduler.pumpCPU( 1 ) == 1 );
	SIR_FENCE_CHECK( finalizer.nReleased == 1 && nCalls.load() == 1 );
	SIR_FENCE_CHECK( graph.queryExecution( &summary ) && summary.eState == SIR_TASK_GRAPH_EXEC_SUCCEEDED );
	SIR_FENCE_CHECK( graph.releaseSubmitted() );
	SIR_FENCE_CHECK( tracker.shutdown() && scheduler.shutdown() );
	memory.shutdown();
}

static sirTaskResult_t completedProvider( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_SUCCESS;
}

static void testRejectedInitializationAndThreadAffinity( void )
{
	SIRMemoryManager memory;
	SIRTaskScheduler scheduler;
	SIRTaskFenceTracker tracker;
	uint32_t nDone = UINT32_MAX;
	SIR_FENCE_CHECK( !tracker.hasCapacity() && tracker.getPendingCount() == 0 );
	SIR_FENCE_CHECK( !tracker.shutdown() );
	SIR_FENCE_CHECK( !tracker.untrackLast( {} ) );
	SIR_FENCE_CHECK( !tracker.advanceCompleted( 0, SIR_TASK_RESULT_SUCCESS, nullptr ) );
	SIR_FENCE_CHECK( !tracker.advanceCompleted( 0, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 0 );
	SIR_FENCE_CHECK( !tracker.init( nullptr, &scheduler, SIR_TASK_EXEC_RENDER, 1 ) );
	SIR_FENCE_CHECK( !tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 1 ) );
	initFenceMemory( memory );
	SIR_FENCE_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
	SIR_FENCE_CHECK( !tracker.init( &memory, nullptr, SIR_TASK_EXEC_RENDER, 1 ) );
	SIR_FENCE_CHECK( !tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 0 ) );
	SIR_FENCE_CHECK( !tracker.init( &memory, &scheduler, SIR_TASK_EXEC_CPU, 1 ) );
	SIR_FENCE_CHECK( !tracker.init( &memory, &scheduler, SIR_TASK_EXEC_COUNT, 1 ) );
	SIR_FENCE_CHECK( !tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, UINT32_MAX ) );
	SIR_FENCE_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 2 ) );
	SIR_FENCE_CHECK( !tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 2 ) );
	std::thread wrongThread( [&]() {
		uint32_t nWrongDone = UINT32_MAX;
		SIR_FENCE_CHECK( !tracker.shutdown() );
		SIR_FENCE_CHECK( !tracker.hasCapacity() );
		SIR_FENCE_CHECK( tracker.getPendingCount() == 0 );
		SIR_FENCE_CHECK( !tracker.untrackLast( {} ) );
		SIR_FENCE_CHECK( !tracker.track( {}, 1 ) );
		SIR_FENCE_CHECK( !tracker.advanceCompleted( 1, SIR_TASK_RESULT_FAILURE, &nWrongDone ) && nWrongDone == 0 );
		SIRTaskFenceTracker wrongOwner;
		SIR_FENCE_CHECK( !wrongOwner.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 1 ) );
	} );
	wrongThread.join();
	SIR_FENCE_CHECK( tracker.shutdown() );
	SIR_FENCE_CHECK( !tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 2 ) );
	SIR_FENCE_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testRejectedTasksAndFenceOrder( void )
{
	SIRMemoryManager memory;
	initFenceMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_FENCE_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
	SIRTaskFenceTracker tracker;
	SIR_FENCE_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 3 ) );
	sirTaskHandle_t ready = {};
	sirTaskHandle_t completed = {};
	sirTaskHandle_t wrongExecutor = {};
	sirTaskHandle_t deferred = {};
	SIR_FENCE_CHECK( scheduler.submit( { deferredProvider, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &ready ) );
	SIR_FENCE_CHECK( !tracker.track( ready, 5 ) );
	SIR_FENCE_CHECK( scheduler.submit( { completedProvider, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &completed ) );
	SIR_FENCE_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 2 ) == 2 );
	SIR_FENCE_CHECK( !tracker.track( completed, 5 ) );
	SIR_FENCE_CHECK( scheduler.submit( { deferredProvider, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &wrongExecutor ) );
	SIR_FENCE_CHECK( !tracker.track( wrongExecutor, 5 ) );
	SIR_FENCE_CHECK( !tracker.track( { UINT32_MAX, 0 }, 5 ) );
	SIR_FENCE_CHECK( !tracker.untrackLast( ready ) );
	SIR_FENCE_CHECK( tracker.track( ready, 9 ) );
	const sirTaskHandle_t wrongGeneration = { ready.nSlot, ready.nGeneration + 1 };
	SIR_FENCE_CHECK( !tracker.untrackLast( wrongGeneration ) );
	SIR_FENCE_CHECK( !tracker.track( ready, 8 ) );
	SIR_FENCE_CHECK( !tracker.track( ready, 9 ) );
	SIR_FENCE_CHECK( !tracker.untrackLast( ready ) );
	SIR_FENCE_CHECK( !tracker.untrackLast( completed ) );
	SIR_FENCE_CHECK( scheduler.submit( { deferredProvider, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &deferred ) );
	SIR_FENCE_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
	SIR_FENCE_CHECK( tracker.track( deferred, 9 ) );
	uint32_t nDone = UINT32_MAX;
	SIR_FENCE_CHECK( !tracker.advanceCompleted( 9, SIR_TASK_RESULT_PENDING, &nDone ) && nDone == 0 );
	SIR_FENCE_CHECK( tracker.advanceCompleted( 4, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 0 );
	SIR_FENCE_CHECK( !tracker.advanceCompleted( 3, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 0 );
	SIR_FENCE_CHECK( tracker.advanceCompleted( 9, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 2 );
	SIR_FENCE_CHECK( !tracker.track( ready, 9 ) );
	SIR_FENCE_CHECK( scheduler.pumpCPU( 1 ) == 1 );
	SIR_FENCE_CHECK( scheduler.releaseTask( ready ) );
	SIR_FENCE_CHECK( scheduler.releaseTask( completed ) );
	SIR_FENCE_CHECK( scheduler.releaseTask( wrongExecutor ) );
	SIR_FENCE_CHECK( scheduler.releaseTask( deferred ) );
	SIR_FENCE_CHECK( tracker.shutdown() && scheduler.shutdown() );
	memory.shutdown();
}

static void testRollbackWithEarlierPendingRecord( void )
{
	SIRMemoryManager memory;
	initFenceMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_FENCE_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
	SIRTaskFenceTracker tracker;
	SIR_FENCE_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 2 ) );
	sirFenceFinalizerContext_t finalizer = { 0, 0, true };
	sirFenceTrackedContext_t firstContext = { &tracker, &finalizer, 10, false };
	sirFenceTrackedContext_t rollbackContext = { &tracker, &finalizer, 11, true };
	sirTaskHandle_t first = {};
	sirTaskHandle_t undone = {};
	SIR_FENCE_CHECK(
	    scheduler.submit( { registerFenceInsideCallback, &firstContext, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &first ) );
	SIR_FENCE_CHECK(
	    scheduler.submit( { registerFenceInsideCallback, &rollbackContext, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &undone ) );
	SIR_FENCE_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 2 ) == 2 );
	SIR_FENCE_CHECK( scheduler.getState( first ) == SIR_TASK_AWAITING_PROVIDER );
	SIR_FENCE_CHECK( scheduler.getState( undone ) == SIR_TASK_FAILED );
	SIR_FENCE_CHECK( tracker.getPendingCount() == 1 );
	uint32_t nDone = 0;
	SIR_FENCE_CHECK( tracker.advanceCompleted( 10, SIR_TASK_RESULT_SUCCESS, &nDone ) && nDone == 1 );
	SIR_FENCE_CHECK( scheduler.releaseTask( first ) && scheduler.releaseTask( undone ) );
	SIR_FENCE_CHECK( tracker.shutdown() && scheduler.shutdown() );
	memory.shutdown();
}

static void testDestructorShutsDownQuiescentTracker( void )
{
	SIRMemoryManager memory;
	initFenceMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_FENCE_CHECK( scheduler.init( &memory, { 4, 0, 4 } ) );
	{
		SIRTaskFenceTracker tracker;
		SIR_FENCE_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 1 ) );
		SIR_FENCE_CHECK( tracker.hasCapacity() );
		// Destruction without an explicit shutdown is valid for a quiescent tracker.
	}
	SIR_FENCE_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

int main( void )
{
	testMonotonicFenceAndSafePoint();
	testFinalizerRetryAndCancellation();
	testAdmissionAndRollback();
	testGraphIntegration();
	testRejectedInitializationAndThreadAffinity();
	testRejectedTasksAndFenceOrder();
	testRollbackWithEarlierPendingRecord();
	testDestructorShutsDownQuiescentTracker();
	return 0;
}
