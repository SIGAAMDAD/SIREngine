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
#include <cassert>
#include <thread>

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
	assert( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	assert( scheduler.init( &memory, { 64, 1 } ) );

	// Owner-affine tasks run only when the initialization thread pumps them.
	std::atomic<uint32_t> nCounter( 0 );
	sirTaskHandle_t owner = {};
	assert( scheduler.submit( countTask, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN, &owner ) );
	std::thread wrongThread( [&]() { assert( scheduler.pumpMain( 1 ) == 0 ); } );
	wrongThread.join();
	assert( nCounter.load() == 0 );
	assert( scheduler.pumpMain( 1 ) == 1 );
	assert( scheduler.getState( owner ) == SIR_TASK_COMPLETED );
	assert( scheduler.releaseTask( owner ) );
	assert( scheduler.getState( owner ) == SIR_TASK_INVALID );

	// Occupy one worker, then cancel a queued task in constant-time slot lookup.
	sirTestBlock_t block = {};
	sirTaskHandle_t blocker = {};
	assert( scheduler.submit( blockTask, &block, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &blocker ) );
	while ( !block.bStarted.load() ) {
		std::this_thread::yield();
	}
	sirTaskHandle_t canceled = {};
	assert( scheduler.submit( countTask, &nCounter, SIR_TASK_PRIORITY_BACKGROUND, SIR_TASK_EXEC_CPU, &canceled ) );
	assert( scheduler.cancel( canceled ) );
	assert( scheduler.getState( canceled ) == SIR_TASK_CANCELLED );
	assert( scheduler.releaseTask( canceled ) );
	block.bRelease.store( true );
	assert( scheduler.waitTask( blocker ) );
	assert( scheduler.releaseTask( blocker ) );
	assert( nCounter.load() == 1 );

	// Recycled slots reject stale generations, and callback failures are terminal.
	sirTaskHandle_t failed = {};
	assert( scheduler.submit( failTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &failed ) );
	assert( scheduler.getState( canceled ) == SIR_TASK_INVALID );
	assert( scheduler.waitTask( failed ) );
	assert( scheduler.getState( failed ) == SIR_TASK_FAILED );
	assert( scheduler.releaseTask( failed ) );

	// Concurrent submissions use the same bounded metadata, without allocations.
	constexpr uint32_t N_PRODUCERS = 4;
	constexpr uint32_t TASKS_PER_PRODUCER = 12;
	sirTaskHandle_t handles[N_PRODUCERS][TASKS_PER_PRODUCER] = {};
	std::thread producers[N_PRODUCERS];
	for ( uint32_t iProducer = 0; iProducer < N_PRODUCERS; ++iProducer ) {
		producers[iProducer] = std::thread( [&, iProducer]() {
			for ( uint32_t iTask = 0; iTask < TASKS_PER_PRODUCER; ++iTask ) {
				assert( scheduler.submit( countTask, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU,
				                          &handles[iProducer][iTask] ) );
			}
		} );
	}
	for ( uint32_t iProducer = 0; iProducer < N_PRODUCERS; ++iProducer ) {
		producers[iProducer].join();
		for ( uint32_t iTask = 0; iTask < TASKS_PER_PRODUCER; ++iTask ) {
			assert( scheduler.waitTask( handles[iProducer][iTask] ) );
			assert( scheduler.releaseTask( handles[iProducer][iTask] ) );
		}
	}
	assert( nCounter.load() == 1 + N_PRODUCERS * TASKS_PER_PRODUCER );
	assert( scheduler.getActiveCount() == 0 );
	assert( scheduler.shutdown() );
	assert( !scheduler.submit( countTask, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &failed ) );
	memory.shutdown();
}

static void testCapacityAndShutdown( void )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 16ULL * 1024 * 1024, 4ULL * 1024 * 1024, 16 };
	SIRMemoryManager memory;
	assert( memory.init( memoryConfig ) );
	SIRTaskScheduler scheduler;
	assert( scheduler.init( &memory, { 2, 1 } ) );
	sirTestBlock_t block = {};
	sirTaskHandle_t first = {};
	sirTaskHandle_t second = {};
	sirTaskHandle_t denied = {};
	assert( scheduler.submit( blockTask, &block, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &first ) );
	while ( !block.bStarted.load() ) {
		std::this_thread::yield();
	}
	assert( scheduler.submit( failTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &second ) );
	assert( !scheduler.submit( failTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &denied ) );
	assert( !scheduler.releaseTask( first ) );
	assert( scheduler.cancel( second ) );
	assert( scheduler.releaseTask( second ) );
	block.bRelease.store( true );
	assert( scheduler.waitTask( first ) );
	assert( scheduler.releaseTask( first ) );
	assert( scheduler.shutdown() );
	memory.shutdown();
}

int main( void )
{
	testScheduler();
	testCapacityAndShutdown();
	return 0;
}
