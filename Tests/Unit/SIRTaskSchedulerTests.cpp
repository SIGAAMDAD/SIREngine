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

#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

// CTest must execute assertions in both Debug and Release configurations.
#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Task scheduler test failed: %s:%d: %s\n", __FILE__, __LINE__, #expression );                                 \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static sirTaskResult_t countTask( void *pContext, sirTaskHandle_t ) noexcept
{
	( ( std::atomic<uint32_t> * )pContext )->fetch_add( 1 );
	return SIR_TASK_RESULT_SUCCESS;
}

static sirTaskResult_t failTask( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_FAILURE;
}

typedef struct sirTestBlock_s
{
	std::atomic<bool> bStarted;
	std::atomic<bool> bRelease;
} sirTestBlock_t;

static sirTaskResult_t blockTask( void *pContext, sirTaskHandle_t ) noexcept
{
	sirTestBlock_t *pBlock = ( sirTestBlock_t * )pContext;
	pBlock->bStarted.store( true );
	while ( !pBlock->bRelease.load() ) {
		std::this_thread::yield();
	}
	return SIR_TASK_RESULT_SUCCESS;
}

static void testScheduler( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	SIR_TEST_CHECK( scheduler.init( &memory, { 64, 1, 0 } ) );

	// Owner-affine tasks run only when the initialization thread pumps them.
	std::atomic<uint32_t> nCounter( 0 );
	sirTaskHandle_t owner = {};
	SIR_TEST_CHECK( scheduler.submit( countTask, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN, &owner ) );
	std::thread wrongThread( [&]() { SIR_TEST_CHECK( scheduler.pumpMain( 1 ) == 0 ); } );
	wrongThread.join();
	SIR_TEST_CHECK( nCounter.load() == 0 );
	SIR_TEST_CHECK( scheduler.pumpMain( 1 ) == 1 );
	SIR_TEST_CHECK( scheduler.getState( owner ) == SIR_TASK_COMPLETED );
	SIR_TEST_CHECK( scheduler.releaseTask( owner ) );
	SIR_TEST_CHECK( scheduler.getState( owner ) == SIR_TASK_INVALID );

	// Occupy one worker, then cancel a queued task in constant-time slot lookup.
	sirTestBlock_t block = {};
	sirTaskHandle_t blocker = {};
	SIR_TEST_CHECK( scheduler.submit( blockTask, &block, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &blocker ) );
	while ( !block.bStarted.load() ) {
		std::this_thread::yield();
	}
	sirTaskHandle_t canceled = {};
	SIR_TEST_CHECK( scheduler.submit( countTask, &nCounter, SIR_TASK_PRIORITY_BACKGROUND, SIR_TASK_EXEC_CPU, &canceled ) );
	SIR_TEST_CHECK( scheduler.cancel( canceled ) );
	SIR_TEST_CHECK( scheduler.getState( canceled ) == SIR_TASK_CANCELLED );
	SIR_TEST_CHECK( scheduler.releaseTask( canceled ) );
	block.bRelease.store( true );
	SIR_TEST_CHECK( scheduler.waitTask( blocker ) );
	SIR_TEST_CHECK( scheduler.releaseTask( blocker ) );
	SIR_TEST_CHECK( nCounter.load() == 1 );

	// Recycled slots reject stale generations, and callback failures are terminal.
	sirTaskHandle_t failed = {};
	SIR_TEST_CHECK( scheduler.submit( failTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &failed ) );
	SIR_TEST_CHECK( scheduler.getState( canceled ) == SIR_TASK_INVALID );
	SIR_TEST_CHECK( scheduler.waitTask( failed ) );
	SIR_TEST_CHECK( scheduler.getState( failed ) == SIR_TASK_FAILED );
	SIR_TEST_CHECK( scheduler.releaseTask( failed ) );

	// Concurrent submissions use the same bounded metadata, without allocations.
	constexpr uint32_t N_PRODUCERS = 4;
	constexpr uint32_t TASKS_PER_PRODUCER = 12;
	sirTaskHandle_t handles[N_PRODUCERS][TASKS_PER_PRODUCER] = {};
	std::thread producers[N_PRODUCERS];
	for ( uint32_t iProducer = 0; iProducer < N_PRODUCERS; ++iProducer ) {
		producers[iProducer] = std::thread( [&, iProducer]() {
			for ( uint32_t iTask = 0; iTask < TASKS_PER_PRODUCER; ++iTask ) {
				SIR_TEST_CHECK(
				    scheduler.submit( countTask, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &handles[iProducer][iTask] ) );
			}
		} );
	}
	for ( uint32_t iProducer = 0; iProducer < N_PRODUCERS; ++iProducer ) {
		producers[iProducer].join();
		for ( uint32_t iTask = 0; iTask < TASKS_PER_PRODUCER; ++iTask ) {
			SIR_TEST_CHECK( scheduler.waitTask( handles[iProducer][iTask] ) );
			SIR_TEST_CHECK( scheduler.releaseTask( handles[iProducer][iTask] ) );
		}
	}
	SIR_TEST_CHECK( nCounter.load() == 1 + N_PRODUCERS * TASKS_PER_PRODUCER );
	SIR_TEST_CHECK( scheduler.getActiveCount() == 0 );
	SIR_TEST_CHECK( scheduler.shutdown() );
	SIR_TEST_CHECK( !scheduler.submit( countTask, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &failed ) );
	memory.shutdown();
}

static void testCapacityAndShutdown( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	SIR_TEST_CHECK( scheduler.init( &memory, { 2, 1, 0 } ) );
	sirTestBlock_t block = {};
	sirTaskHandle_t first = {};
	sirTaskHandle_t second = {};
	sirTaskHandle_t denied = {};
	SIR_TEST_CHECK( scheduler.submit( blockTask, &block, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &first ) );
	while ( !block.bStarted.load() ) {
		std::this_thread::yield();
	}
	SIR_TEST_CHECK( scheduler.submit( failTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &second ) );
	SIR_TEST_CHECK( !scheduler.submit( failTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &denied ) );
	SIR_TEST_CHECK( !scheduler.releaseTask( first ) );
	SIR_TEST_CHECK( scheduler.cancel( second ) );
	SIR_TEST_CHECK( scheduler.releaseTask( second ) );
	block.bRelease.store( true );
	SIR_TEST_CHECK( scheduler.waitTask( first ) );
	SIR_TEST_CHECK( scheduler.releaseTask( first ) );
	SIR_TEST_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

// Verifies immutable prerequisite registration, dependent activation, and
// that main-affine execution is explicit rather than run by pool workers.
static void testDependencyExecution( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	SIR_TEST_CHECK( scheduler.init( &memory, { 64, 2, 8 } ) );
	std::atomic<uint32_t> nCount( 0 );
	sirTestBlock_t block = {};
	sirTaskHandle_t root = {}, parallel = {}, join = {}, tail = {}, main = {};
	SIR_TEST_CHECK( scheduler.submit( blockTask, &block, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root ) );
	while ( !block.bStarted.load() ) {
		std::this_thread::yield();
	}
	SIR_TEST_CHECK( scheduler.submit( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &parallel ) );
	const sirTaskHandle_t prerequisites[] = { root, parallel };
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, prerequisites, 2, &join ) );
	SIR_TEST_CHECK( scheduler.getState( join ) == SIR_TASK_BLOCKED );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN, &join, 1, &main ) );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &main, 1, &tail ) );
	SIR_TEST_CHECK( scheduler.getState( tail ) == SIR_TASK_BLOCKED );
	SIR_TEST_CHECK( scheduler.waitTask( parallel ) );
	SIR_TEST_CHECK( nCount.load() == 1 );
	SIR_TEST_CHECK( scheduler.getState( join ) == SIR_TASK_BLOCKED );
	block.bRelease.store( true );
	SIR_TEST_CHECK( scheduler.waitTask( join ) );
	SIR_TEST_CHECK( nCount.load() == 2 );
	SIR_TEST_CHECK( scheduler.getState( main ) == SIR_TASK_QUEUED );
	SIR_TEST_CHECK( scheduler.getState( tail ) == SIR_TASK_BLOCKED );
	SIR_TEST_CHECK( scheduler.pumpMain( 1 ) == 1 );
	SIR_TEST_CHECK( scheduler.waitTask( tail ) );
	SIR_TEST_CHECK( nCount.load() == 4 );
	SIR_TEST_CHECK( scheduler.releaseTask( root ) );
	SIR_TEST_CHECK( scheduler.releaseTask( parallel ) );
	SIR_TEST_CHECK( scheduler.releaseTask( join ) );
	SIR_TEST_CHECK( scheduler.releaseTask( main ) );
	SIR_TEST_CHECK( scheduler.releaseTask( tail ) );
	SIR_TEST_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testCancellationCascade( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	SIR_TEST_CHECK( scheduler.init( &memory, { 64, 1, 16 } ) );
	std::atomic<uint32_t> nCount( 0 );
	sirTestBlock_t block = {};
	sirTaskHandle_t root = {}, childA = {}, childB = {}, diamond = {}, leaf = {};
	SIR_TEST_CHECK( scheduler.submit( blockTask, &block, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root ) );
	while ( !block.bStarted.load() ) {
		std::this_thread::yield();
	}
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root, 1, &childA ) );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root, 1, &childB ) );
	const sirTaskHandle_t parents[] = { childA, childB };
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, parents, 2, &diamond ) );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &diamond, 1, &leaf ) );
	SIR_TEST_CHECK( scheduler.cancel( childA ) );
	SIR_TEST_CHECK( scheduler.getState( childA ) == SIR_TASK_CANCELLED );
	SIR_TEST_CHECK( scheduler.getState( diamond ) == SIR_TASK_CANCELLED );
	SIR_TEST_CHECK( scheduler.getState( leaf ) == SIR_TASK_CANCELLED );
	SIR_TEST_CHECK( scheduler.getState( childB ) == SIR_TASK_BLOCKED );
	SIR_TEST_CHECK( scheduler.releaseTask( childA ) );
	SIR_TEST_CHECK( scheduler.releaseTask( diamond ) );
	SIR_TEST_CHECK( scheduler.releaseTask( leaf ) );
	block.bRelease.store( true );
	SIR_TEST_CHECK( scheduler.waitTask( childB ) );
	SIR_TEST_CHECK( scheduler.releaseTask( childB ) );
	SIR_TEST_CHECK( scheduler.releaseTask( root ) );
	SIR_TEST_CHECK( nCount.load() == 1 );
	SIR_TEST_CHECK( scheduler.getActiveCount() == 0 );
	SIR_TEST_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testFailedParentAndEdgeCapacity( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	SIR_TEST_CHECK( scheduler.init( &memory, { 32, 1, 2 } ) );
	std::atomic<uint32_t> nCount( 0 );
	sirTestBlock_t block = {};
	sirTaskHandle_t root = {}, one = {}, two = {}, rejected = {}, failed = {}, downstream = {};
	SIR_TEST_CHECK( scheduler.submit( blockTask, &block, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root ) );
	while ( !block.bStarted.load() ) {
		std::this_thread::yield();
	}
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root, 1, &one ) );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root, 1, &two ) );
	SIR_TEST_CHECK( !scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root, 1, &rejected ) );
	SIR_TEST_CHECK( scheduler.getActiveCount() == 3 );
	const sirTaskHandle_t duplicates[] = { one, one };
	SIR_TEST_CHECK( !scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, duplicates, 2, &rejected ) );
	SIR_TEST_CHECK( scheduler.cancel( one ) );
	SIR_TEST_CHECK( scheduler.releaseTask( one ) );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root, 1, &one ) );
	block.bRelease.store( true );
	SIR_TEST_CHECK( scheduler.waitTask( two ) );
	SIR_TEST_CHECK( scheduler.waitTask( one ) );
	SIR_TEST_CHECK( scheduler.releaseTask( root ) );
	SIR_TEST_CHECK( scheduler.releaseTask( one ) );
	SIR_TEST_CHECK( scheduler.releaseTask( two ) );
	SIR_TEST_CHECK( nCount.load() == 2 );
	SIR_TEST_CHECK( scheduler.submit( failTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &failed ) );
	SIR_TEST_CHECK( scheduler.waitTask( failed ) );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &failed, 1, &downstream ) );
	SIR_TEST_CHECK( scheduler.getState( downstream ) == SIR_TASK_CANCELLED );
	SIR_TEST_CHECK( !scheduler.cancel( downstream ) );
	SIR_TEST_CHECK( scheduler.releaseTask( downstream ) );
	SIR_TEST_CHECK( scheduler.releaseTask( failed ) );
	SIR_TEST_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testWorkerlessFallback( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	SIR_TEST_CHECK( scheduler.init( &memory, { 16, 0, 8 } ) );
	std::atomic<uint32_t> nCount( 0 );
	sirTaskHandle_t first = {}, second = {}, third = {};
	SIR_TEST_CHECK( scheduler.submit( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &first ) );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &first, 1, &second ) );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN, &second, 1, &third ) );
	SIR_TEST_CHECK( !scheduler.waitTask( first ) );
	SIR_TEST_CHECK( scheduler.pumpCPU( 8 ) == 2 );
	SIR_TEST_CHECK( nCount.load() == 2 );
	SIR_TEST_CHECK( scheduler.pumpMain( 8 ) == 1 );
	SIR_TEST_CHECK( nCount.load() == 3 );
	SIR_TEST_CHECK( scheduler.releaseTask( first ) );
	SIR_TEST_CHECK( scheduler.releaseTask( second ) );
	SIR_TEST_CHECK( scheduler.releaseTask( third ) );
	SIR_TEST_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testFailedParentPropagation( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	SIR_TEST_CHECK( scheduler.init( &memory, { 32, 1, 8 } ) );
	std::atomic<uint32_t> nExecuted( 0 );
	sirTestBlock_t block = {};
	sirTaskHandle_t blocker = {}, failure = {}, dependent = {}, grandchild = {};
	SIR_TEST_CHECK( scheduler.submit( blockTask, &block, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &blocker ) );
	while ( !block.bStarted.load() ) {
		std::this_thread::yield();
	}
	SIR_TEST_CHECK( scheduler.submit( failTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &failure ) );
	SIR_TEST_CHECK( scheduler.submitAfter( countTask, &nExecuted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &failure, 1, &dependent ) );
	SIR_TEST_CHECK(
	    scheduler.submitAfter( countTask, &nExecuted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN, &dependent, 1, &grandchild ) );
	SIR_TEST_CHECK( scheduler.getState( dependent ) == SIR_TASK_BLOCKED );
	block.bRelease.store( true );
	SIR_TEST_CHECK( scheduler.waitTask( failure ) );
	SIR_TEST_CHECK( scheduler.getState( failure ) == SIR_TASK_FAILED );
	SIR_TEST_CHECK( scheduler.getState( dependent ) == SIR_TASK_CANCELLED );
	SIR_TEST_CHECK( scheduler.getState( grandchild ) == SIR_TASK_CANCELLED );
	SIR_TEST_CHECK( scheduler.pumpMain( 2 ) == 0 );
	SIR_TEST_CHECK( nExecuted.load() == 0 );
	SIR_TEST_CHECK( scheduler.releaseTask( blocker ) );
	SIR_TEST_CHECK( scheduler.releaseTask( failure ) );
	SIR_TEST_CHECK( scheduler.releaseTask( dependent ) );
	SIR_TEST_CHECK( scheduler.releaseTask( grandchild ) );
	SIR_TEST_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

// A callback performs nested submissions on one worker. The remaining
// workers can only execute the gated children by stealing from that worker.
typedef struct sirTestFanout_s
{
	SIRTaskScheduler *pScheduler;
	sirTaskHandle_t *pHandles;
	uint32_t nCount;
	std::atomic<uint32_t> nStarted;
	std::atomic<bool> bReady;
	std::atomic<bool> bRelease;
} sirTestFanout_t;

static sirTaskResult_t gatedChildTask( void *pContext, sirTaskHandle_t ) noexcept
{
	sirTestFanout_t *pFanout = ( sirTestFanout_t * )pContext;
	pFanout->nStarted.fetch_add( 1, std::memory_order_release );
	while ( !pFanout->bRelease.load( std::memory_order_acquire ) ) {
		std::this_thread::yield();
	}
	return SIR_TASK_RESULT_SUCCESS;
}

static sirTaskResult_t fanoutTask( void *pContext, sirTaskHandle_t ) noexcept
{
	sirTestFanout_t *pFanout = ( sirTestFanout_t * )pContext;
	for ( uint32_t iTask = 0; iTask < pFanout->nCount; ++iTask ) {
		SIR_TEST_CHECK( pFanout->pScheduler->submit( gatedChildTask, pFanout, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU,
		                                             &pFanout->pHandles[iTask] ) );
	}
	pFanout->bReady.store( true, std::memory_order_release );
	return SIR_TASK_RESULT_SUCCESS;
}

static bool waitForAtLeast( std::atomic<uint32_t> &count, uint32_t nExpected )
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
	while ( count.load( std::memory_order_acquire ) < nExpected ) {
		if ( std::chrono::steady_clock::now() >= deadline ) {
			return false;
		}
		std::this_thread::yield();
	}
	return true;
}

static void testWorkerStealing( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	SIR_TEST_CHECK( scheduler.init( &memory, { 64, 4, 16 } ) );
	sirTaskHandle_t children[24] = {};
	sirTestFanout_t fanout = {};
	fanout.pScheduler = &scheduler;
	fanout.pHandles = children;
	fanout.nCount = 24;
	sirTaskHandle_t parent = {};
	SIR_TEST_CHECK( scheduler.submit( fanoutTask, &fanout, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &parent ) );
	SIR_TEST_CHECK( waitForAtLeast( fanout.nStarted, 2 ) );
	const sirTaskSchedulerStats_t midway = scheduler.getStats();
	SIR_TEST_CHECK( midway.nStealSucceeded > 0 );
	fanout.bRelease.store( true, std::memory_order_release );
	SIR_TEST_CHECK( scheduler.waitTask( parent ) );
	for ( uint32_t iTask = 0; iTask < fanout.nCount; ++iTask ) {
		SIR_TEST_CHECK( scheduler.waitTask( children[iTask] ) );
		SIR_TEST_CHECK( scheduler.getState( children[iTask] ) == SIR_TASK_COMPLETED );
		SIR_TEST_CHECK( scheduler.releaseTask( children[iTask] ) );
	}
	SIR_TEST_CHECK( scheduler.releaseTask( parent ) );
	const sirTaskSchedulerStats_t stats = scheduler.getStats();
	SIR_TEST_CHECK( stats.nSubmitted == 25 );
	SIR_TEST_CHECK( stats.nExecuted == 25 );
	SIR_TEST_CHECK( stats.nActive == 0 );
	SIR_TEST_CHECK( stats.nReadyCpu == 0 );
	SIR_TEST_CHECK( stats.nStealSucceeded <= stats.nStealAttempts );
	SIR_TEST_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

// Cancel a local-queue task while its parent still occupies the sole worker.
// This specifically checks that cancellation removes from a worker queue,
// rather than incorrectly modifying the global ingress list.
typedef struct sirTestLocalCancel_s
{
	SIRTaskScheduler *pScheduler;
	sirTaskHandle_t child;
	std::atomic<bool> bQueued;
	std::atomic<bool> bRelease;
	std::atomic<uint32_t> nExecuted;
} sirTestLocalCancel_t;

static sirTaskResult_t blockedSpawner( void *pContext, sirTaskHandle_t ) noexcept
{
	sirTestLocalCancel_t *pTest = ( sirTestLocalCancel_t * )pContext;
	SIR_TEST_CHECK( pTest->pScheduler->submit( countTask, &pTest->nExecuted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &pTest->child ) );
	pTest->bQueued.store( true, std::memory_order_release );
	while ( !pTest->bRelease.load( std::memory_order_acquire ) ) {
		std::this_thread::yield();
	}
	return SIR_TASK_RESULT_SUCCESS;
}

static void testLocalCancellation( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	SIR_TEST_CHECK( scheduler.init( &memory, { 4, 1, 4 } ) );
	sirTestLocalCancel_t context = {};
	context.pScheduler = &scheduler;
	sirTaskHandle_t parent = {};
	SIR_TEST_CHECK( scheduler.submit( blockedSpawner, &context, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &parent ) );
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
	while ( !context.bQueued.load( std::memory_order_acquire ) ) {
		SIR_TEST_CHECK( std::chrono::steady_clock::now() < deadline );
		std::this_thread::yield();
	}
	SIR_TEST_CHECK( scheduler.getState( context.child ) == SIR_TASK_QUEUED );
	SIR_TEST_CHECK( scheduler.getStats().nReadyCpu == 1 );
	SIR_TEST_CHECK( scheduler.cancel( context.child ) );
	SIR_TEST_CHECK( scheduler.getStats().nReadyCpu == 0 );
	SIR_TEST_CHECK( scheduler.releaseTask( context.child ) );
	context.bRelease.store( true, std::memory_order_release );
	SIR_TEST_CHECK( scheduler.waitTask( parent ) );
	SIR_TEST_CHECK( scheduler.releaseTask( parent ) );
	SIR_TEST_CHECK( context.nExecuted.load() == 0 );
	SIR_TEST_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static sirTaskResult_t pendingProviderTask( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_PENDING;
}

static void testInvalidInputAndProviderLifecycle( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIRTaskScheduler cold;
	sirTaskHandle_t handle = {};
	sirTaskState_t state = SIR_TASK_INVALID;
	sirTaskExecutor_t executor = SIR_TASK_EXEC_CPU;
	SIR_TEST_CHECK( !cold.shutdown() );
	SIR_TEST_CHECK( !cold.init( nullptr, { 8, 0, 8 } ) );
	SIR_TEST_CHECK( !cold.init( &memory, { 8, 0, 8 } ) );
	SIR_TEST_CHECK( !cold.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
	SIR_TEST_CHECK( !cold.submit( countTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &handle ) );
	SIR_TEST_CHECK( !cold.cancel( handle ) );
	SIR_TEST_CHECK( !cold.releaseTask( handle ) );
	SIR_TEST_CHECK( !cold.queryTask( handle, &state, &executor ) );
	SIR_TEST_CHECK( cold.getState( handle ) == SIR_TASK_INVALID );
	SIR_TEST_CHECK( !cold.completeProvider( handle, SIR_TASK_RESULT_SUCCESS ) );
	SIR_TEST_CHECK( cold.pumpMain( 1 ) == 0 && cold.pumpCPU( 1 ) == 0 );
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIR_TEST_CHECK( !cold.init( &memory, { 0, 0, 0 } ) );
	SIR_TEST_CHECK( !cold.init( &memory, { UINT32_MAX, 0, 0 } ) );
	SIR_TEST_CHECK( !cold.init( &memory, { 8, 0, UINT32_MAX } ) );
	SIR_TEST_CHECK( cold.init( &memory, { 8, 0, 8 } ) );
	SIR_TEST_CHECK( !cold.init( &memory, { 8, 0, 8 } ) );
	SIR_TEST_CHECK( !cold.submit( nullptr, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &handle ) );
	SIR_TEST_CHECK( !cold.submit( countTask, nullptr, ( sirTaskPriority_t )255, SIR_TASK_EXEC_CPU, &handle ) );
	SIR_TEST_CHECK( !cold.submit( countTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_COUNT, &handle ) );
	SIR_TEST_CHECK( !cold.submit( countTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, nullptr ) );
	SIR_TEST_CHECK( !cold.submitAfter( countTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, nullptr, 1, &handle ) );
	SIR_TEST_CHECK( !cold.queryTask( handle, nullptr, &executor ) );
	SIR_TEST_CHECK( !cold.queryTask( handle, &state, nullptr ) );
	SIR_TEST_CHECK( !cold.queryTask( handle, &state, &executor ) );
	SIR_TEST_CHECK( !cold.isCancellationRequested( handle ) );
	SIR_TEST_CHECK( !cold.waitTask( handle ) );
	SIR_TEST_CHECK( !cold.cancel( handle ) );
	SIR_TEST_CHECK( !cold.releaseTask( handle ) );
	std::atomic<uint32_t> nBatchCount( 0 );
	const sirTaskBatchTask_t batch[2] = { { countTask, &nBatchCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN },
	                                      { countTask, &nBatchCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN } };
	sirTaskBatchTask_t invalidBatch[2] = { batch[0], batch[1] };
	const sirTaskBatchEdge_t edge = { 0, 1 };
	sirTaskHandle_t batchHandles[2] = {};
	SIR_TEST_CHECK( !cold.submitBatch( nullptr, 2, &edge, 1, batchHandles ) );
	SIR_TEST_CHECK( !cold.submitBatch( batch, 2, &edge, 1, nullptr ) );
	SIR_TEST_CHECK( !cold.submitBatch( batch, 0, &edge, 1, batchHandles ) );
	SIR_TEST_CHECK( !cold.submitBatch( batch, 2, nullptr, 1, batchHandles ) );
	invalidBatch[0].pFunction = nullptr;
	SIR_TEST_CHECK( !cold.submitBatch( invalidBatch, 2, &edge, 1, batchHandles ) );
	invalidBatch[0] = batch[0];
	invalidBatch[1].ePriority = ( sirTaskPriority_t )255;
	SIR_TEST_CHECK( !cold.submitBatch( invalidBatch, 2, &edge, 1, batchHandles ) );
	invalidBatch[1] = batch[1];
	invalidBatch[1].eExecutor = SIR_TASK_EXEC_COUNT;
	SIR_TEST_CHECK( !cold.submitBatch( invalidBatch, 2, &edge, 1, batchHandles ) );
	const sirTaskBatchEdge_t backward = { 1, 0 };
	const sirTaskBatchEdge_t outOfRange = { 0, 2 };
	SIR_TEST_CHECK( !cold.submitBatch( batch, 2, &backward, 1, batchHandles ) );
	SIR_TEST_CHECK( !cold.submitBatch( batch, 2, &outOfRange, 1, batchHandles ) );
	SIR_TEST_CHECK( !cold.bindProviderOwner( SIR_TASK_EXEC_CPU ) );
	SIR_TEST_CHECK( !cold.bindProviderOwner( SIR_TASK_EXEC_COUNT ) );
	SIR_TEST_CHECK( cold.pumpProvider( SIR_TASK_EXEC_CPU, 1 ) == 0 );
	SIR_TEST_CHECK( cold.pumpProvider( SIR_TASK_EXEC_COUNT, 1 ) == 0 );
	SIR_TEST_CHECK( !cold.completeProvider( handle, SIR_TASK_RESULT_PENDING ) );
	SIR_TEST_CHECK( !cold.completeProvider( handle, SIR_TASK_RESULT_SUCCESS ) );
	SIR_TEST_CHECK( !cold.submitAfter( countTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &handle, UINT32_MAX, &handle ) );
	SIR_TEST_CHECK( cold.submit( pendingProviderTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER, &handle ) );
	SIR_TEST_CHECK( !cold.releaseTask( handle ) );
	SIR_TEST_CHECK( cold.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 0 );
	SIR_TEST_CHECK( cold.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
	SIR_TEST_CHECK( cold.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
	std::atomic<uint32_t> nWrongOwner{ 0 };
	std::thread wrong( [&]() {
		if ( cold.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 0 && !cold.bindProviderOwner( SIR_TASK_EXEC_RENDER ) &&
		     !cold.completeProvider( handle, SIR_TASK_RESULT_SUCCESS ) && !cold.shutdown() ) {
			nWrongOwner.store( 1 );
		}
	} );
	wrong.join();
	SIR_TEST_CHECK( nWrongOwner.load() == 1 );
	SIR_TEST_CHECK( cold.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
	SIR_TEST_CHECK( cold.getState( handle ) == SIR_TASK_AWAITING_PROVIDER );
	SIR_TEST_CHECK( !cold.shutdown() );
	SIR_TEST_CHECK( !cold.completeProvider( handle, SIR_TASK_RESULT_PENDING ) );
	SIR_TEST_CHECK( cold.cancel( handle ) );
	SIR_TEST_CHECK( cold.isCancellationRequested( handle ) );
	SIR_TEST_CHECK( cold.completeProvider( handle, SIR_TASK_RESULT_SUCCESS ) );
	SIR_TEST_CHECK( cold.getState( handle ) == SIR_TASK_CANCELLED );
	SIR_TEST_CHECK( !cold.cancel( handle ) );
	SIR_TEST_CHECK( cold.releaseTask( handle ) );
	SIR_TEST_CHECK( cold.submitBatch( batch, 2, &edge, 1, batchHandles ) );
	SIR_TEST_CHECK( cold.pumpMain( 2 ) == 2 && nBatchCount.load() == 2 );
	SIR_TEST_CHECK( cold.getState( batchHandles[0] ) == SIR_TASK_COMPLETED );
	SIR_TEST_CHECK( cold.releaseTask( batchHandles[0] ) && cold.releaseTask( batchHandles[1] ) );
	SIR_TEST_CHECK( cold.shutdown() );
	SIR_TEST_CHECK( !cold.shutdown() );
	SIR_TEST_CHECK( !cold.init( &memory, { 8, 0, 8 } ) );
	memory.shutdown();
}

typedef struct sirTestShutdownPoll_s
{
	SIRTaskScheduler *pScheduler;
	std::atomic<uint32_t> nStarted;
} sirTestShutdownPoll_t;

static sirTaskResult_t taskUntilShutdown( void *pContext, sirTaskHandle_t task ) noexcept
{
	sirTestShutdownPoll_t *pTest = ( sirTestShutdownPoll_t * )pContext;
	pTest->nStarted.store( 1, std::memory_order_release );
	while ( !pTest->pScheduler->isCancellationRequested( task ) ) {
		std::this_thread::yield();
	}
	return SIR_TASK_RESULT_SUCCESS;
}

static void testShutdownWithLiveTasksAndProviderWait( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	{
		SIRTaskScheduler scheduler;
		SIR_TEST_CHECK( scheduler.init( &memory, { 8, 1, 16 } ) );
		SIR_TEST_CHECK( scheduler.pumpCPU( 1 ) == 0 );
		SIR_TEST_CHECK( !scheduler.waitTask( { UINT32_MAX, 0 } ) );
		SIR_TEST_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
		sirTaskHandle_t provider = {};
		SIR_TEST_CHECK( scheduler.submit( pendingProviderTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER, &provider ) );
		SIR_TEST_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
		SIR_TEST_CHECK( !scheduler.waitTask( provider ) );
		SIR_TEST_CHECK( scheduler.completeProvider( provider, SIR_TASK_RESULT_SUCCESS ) );
		SIR_TEST_CHECK( scheduler.releaseTask( provider ) );
		sirTestShutdownPoll_t context = {};
		context.pScheduler = &scheduler;
		std::atomic<uint32_t> nExecuted( 0 );
		sirTaskHandle_t running = {};
		sirTaskHandle_t queued = {};
		sirTaskHandle_t blocked = {};
		SIR_TEST_CHECK( scheduler.submit( taskUntilShutdown, &context, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &running ) );
		SIR_TEST_CHECK( waitForAtLeast( context.nStarted, 1 ) );
		SIR_TEST_CHECK( scheduler.submit( countTask, &nExecuted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &queued ) );
		SIR_TEST_CHECK(
		    scheduler.submitAfter( countTask, &nExecuted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN, &queued, 1, &blocked ) );
		SIR_TEST_CHECK( scheduler.getState( running ) == SIR_TASK_RUNNING );
		SIR_TEST_CHECK( scheduler.getState( queued ) == SIR_TASK_QUEUED );
		SIR_TEST_CHECK( scheduler.getState( blocked ) == SIR_TASK_BLOCKED );
		std::thread unauthorized( [&]() { SIR_TEST_CHECK( !scheduler.shutdown() ); } );
		unauthorized.join();
		SIR_TEST_CHECK( scheduler.shutdown() );
		SIR_TEST_CHECK( nExecuted.load() == 0 );
		SIR_TEST_CHECK( scheduler.getState( running ) == SIR_TASK_INVALID );
	}
	memory.shutdown();
}

int main( void )
{
	testScheduler();
	testCapacityAndShutdown();
	testDependencyExecution();
	testCancellationCascade();
	testFailedParentAndEdgeCapacity();
	testWorkerlessFallback();
	testFailedParentPropagation();
	testWorkerStealing();
	testLocalCancellation();
	testInvalidInputAndProviderLifecycle();
	testShutdownWithLiveTasksAndProviderWait();
	return 0;
}
