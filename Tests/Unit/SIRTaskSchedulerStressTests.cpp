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
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#define SIR_STRESS_CHECK( expression )                                                                                                     \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Scheduler stress failure: %s:%d: %s\n", __FILE__, __LINE__, #expression );                                   \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static void initStressMemory( SIRMemoryManager& memory )
{
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 16ULL * 1024 * 1024, 32 };
	SIR_STRESS_CHECK( memory.init( config ) );
}

static sirTaskResult_t incrementStress( void *pContext, sirTaskHandle_t ) noexcept
{
	( ( std::atomic<uint32_t> * )pContext )->fetch_add( 1, std::memory_order_relaxed );
	return SIR_TASK_RESULT_SUCCESS;
}

static sirTaskResult_t deferredStress( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_PENDING;
}

static void testConcurrentProducersAndMetadata( void )
{
	SIRMemoryManager memory;
	initStressMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_STRESS_CHECK( scheduler.init( &memory, { 256, 4, 512 } ) );
	const uint64_t nMetadataAfterInit = memory.getStats().nMetadataUsedBytes;
	constexpr uint32_t PRODUCERS = 4;
	constexpr uint32_t PER_PRODUCER = 48;
	constexpr uint32_t ROUNDS = 16;
	std::atomic<uint32_t> nCompleted( 0 );
	std::atomic<bool> bObserve( true );
	std::thread observer( [&]() {
		while ( bObserve.load( std::memory_order_acquire ) ) {
			const sirTaskSchedulerStats_t stats = scheduler.getStats();
			SIR_STRESS_CHECK( stats.nActive <= 256 );
			std::this_thread::yield();
		}
	} );
	for ( uint32_t iRound = 0; iRound < ROUNDS; ++iRound ) {
		sirTaskHandle_t handles[PRODUCERS][PER_PRODUCER] = {};
		std::thread producers[PRODUCERS];
		for ( uint32_t iProducer = 0; iProducer < PRODUCERS; ++iProducer ) {
			producers[iProducer] = std::thread( [&, iProducer]() {
				for ( uint32_t iTask = 0; iTask < PER_PRODUCER; ++iTask ) {
					SIR_STRESS_CHECK( scheduler.submit({ incrementStress, &nCompleted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &handles[iProducer][iTask]) );
				}
			} );
		}
		for ( uint32_t iProducer = 0; iProducer < PRODUCERS; ++iProducer ) {
			producers[iProducer].join();
		}
		for ( uint32_t iProducer = 0; iProducer < PRODUCERS; ++iProducer ) {
			for ( uint32_t iTask = 0; iTask < PER_PRODUCER; ++iTask ) {
				const sirTaskHandle_t task = handles[iProducer][iTask];
				SIR_STRESS_CHECK( scheduler.waitTask( task ) );
				SIR_STRESS_CHECK( scheduler.getState( task ) == SIR_TASK_COMPLETED );
				SIR_STRESS_CHECK( scheduler.releaseTask( task ) );
				SIR_STRESS_CHECK( scheduler.getState( task ) == SIR_TASK_INVALID );
			}
		}
		SIR_STRESS_CHECK( scheduler.getActiveCount() == 0 );
		SIR_STRESS_CHECK( memory.getStats().nMetadataUsedBytes == nMetadataAfterInit );
	}
	bObserve.store( false, std::memory_order_release );
	observer.join();
	SIR_STRESS_CHECK( nCompleted.load() == PRODUCERS * PER_PRODUCER * ROUNDS );
	const sirTaskSchedulerStats_t stats = scheduler.getStats();
	SIR_STRESS_CHECK( stats.nExecuted == PRODUCERS * PER_PRODUCER * ROUNDS );
	SIR_STRESS_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

typedef struct sirStressPriorityContext_s
{
	uint32_t *pNext;
	uint32_t nSequence;
	bool *pViolation;
} sirStressPriorityContext_t;

static sirTaskResult_t recordPriorityStress( void *pContext, sirTaskHandle_t ) noexcept
{
	sirStressPriorityContext_t *pPriority = ( sirStressPriorityContext_t * )pContext;
	if ( *pPriority->pNext != pPriority->nSequence ) {
		*pPriority->pViolation = true;
	}
	++( *pPriority->pNext );
	return SIR_TASK_RESULT_SUCCESS;
}

static void testPriorityProgressAndFifo( void )
{
	SIRMemoryManager memory;
	initStressMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_STRESS_CHECK( scheduler.init( &memory, { 160, 0, 160 } ) );
	sirTaskHandle_t handles[160] = {};
	sirStressPriorityContext_t contexts[160] = {};
	uint32_t nNext[3] = {};
	bool bViolation = false;
	for ( uint32_t i = 0; i < 160; ++i ) {
		const uint32_t iPriority = i < 96 ? 0 : ( i < 128 ? 1 : 2 );
		contexts[i] = { &nNext[iPriority], iPriority == 0 ? i : ( iPriority == 1 ? i - 96 : i - 128 ), &bViolation };
		SIR_STRESS_CHECK(
		    scheduler.submit({ recordPriorityStress, &contexts[i], ( sirTaskPriority_t )iPriority, SIR_TASK_EXEC_CPU }, &handles[i]) );
	}
	SIR_STRESS_CHECK( scheduler.pumpCPU( 48 ) == 48 );
	SIR_STRESS_CHECK( nNext[0] != 0 && nNext[1] != 0 && nNext[2] != 0 );
	SIR_STRESS_CHECK( scheduler.pumpCPU( 160 ) == 112 );
	SIR_STRESS_CHECK( nNext[0] == 96 && nNext[1] == 32 && nNext[2] == 32 && !bViolation );
	for ( uint32_t i = 0; i < 160; ++i ) {
		SIR_STRESS_CHECK( scheduler.releaseTask( handles[i] ) );
	}
	SIR_STRESS_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

typedef struct sirStressDagContext_s
{
	std::atomic<uint8_t> *pDone;
	std::atomic<bool> *pViolated;
	const uint8_t *pDependencies;
	uint32_t nIndex;
	uint32_t nCount;
} sirStressDagContext_t;

static sirTaskResult_t verifyDagStress( void *pContext, sirTaskHandle_t ) noexcept
{
	sirStressDagContext_t *pDag = ( sirStressDagContext_t * )pContext;
	for ( uint32_t iParent = 0; iParent < pDag->nIndex; ++iParent ) {
		if ( pDag->pDependencies[iParent * pDag->nCount + pDag->nIndex] != 0 &&
		     pDag->pDone[iParent].load( std::memory_order_acquire ) == 0 ) {
			pDag->pViolated->store( true, std::memory_order_release );
		}
	}
	pDag->pDone[pDag->nIndex].store( 1, std::memory_order_release );
	return SIR_TASK_RESULT_SUCCESS;
}

static void testRandomizedDependencyDags( void )
{
	SIRMemoryManager memory;
	initStressMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_STRESS_CHECK( scheduler.init( &memory, { 128, 4, 1024 } ) );
	constexpr uint32_t N = 64;
	std::atomic<uint8_t> szCompleted[N] = {};
	uint8_t szDependencies[N * N] = {};
	sirStressDagContext_t contexts[N] = {};
	sirTaskBatchTask_t tasks[N] = {};
	sirTaskBatchEdge_t edges[1024] = {};
	sirTaskHandle_t handles[N] = {};
	std::atomic<bool> bViolated( false );
	uint32_t nRandom = 0xB5A31D19u;
	for ( uint32_t iRound = 0; iRound < 40; ++iRound ) {
		for ( uint32_t i = 0; i < N; ++i ) {
			szCompleted[i].store( 0, std::memory_order_relaxed );
		}
		memset( szDependencies, 0, sizeof( szDependencies ) );
		uint32_t nEdges = 0;
		for ( uint32_t iChild = 0; iChild < N; ++iChild ) {
			contexts[iChild] = { szCompleted, &bViolated, szDependencies, iChild, N };
			tasks[iChild] = { verifyDagStress, &contexts[iChild], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
			for ( uint32_t iParent = 0; iParent < iChild; ++iParent ) {
				nRandom = nRandom * 1664525u + 1013904223u;
				if ( ( nRandom % 19u ) == 0 ) {
					szDependencies[iParent * N + iChild] = 1;
					edges[nEdges++] = { iParent, iChild };
				}
			}
		}
		SIR_STRESS_CHECK( scheduler.submitBatch({ tasks, N, edges, nEdges }, handles) );
		for ( uint32_t i = 0; i < N; ++i ) {
			SIR_STRESS_CHECK( scheduler.waitTask( handles[i] ) );
			SIR_STRESS_CHECK( scheduler.getState( handles[i] ) == SIR_TASK_COMPLETED );
		}
		SIR_STRESS_CHECK( !bViolated.load( std::memory_order_acquire ) );
		for ( uint32_t i = 0; i < N; ++i ) {
			SIR_STRESS_CHECK( scheduler.releaseTask( handles[i] ) );
		}
	}
	SIR_STRESS_CHECK( scheduler.getActiveCount() == 0 );
	SIR_STRESS_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testCancellationWidthAndGenerations( void )
{
	SIRMemoryManager memory;
	initStressMemory( memory );
	SIRTaskScheduler scheduler;
	constexpr uint32_t N = 256;
	SIR_STRESS_CHECK( scheduler.init( &memory, { N, 0, N * 2 } ) );
	std::atomic<uint32_t> nCount( 0 );
	sirTaskBatchTask_t tasks[N] = {};
	sirTaskBatchEdge_t edges[( N - 2 ) * 2] = {};
	sirTaskHandle_t handles[N] = {};
	for ( uint32_t i = 0; i < N; ++i ) {
		tasks[i] = { incrementStress, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	}
	for ( uint32_t i = 1; i < N - 1; ++i ) {
		edges[2 * ( i - 1 )] = { 0, i };
		edges[2 * ( i - 1 ) + 1] = { i, N - 1 };
	}
	SIR_STRESS_CHECK( scheduler.submitBatch({ tasks, N, edges, ( N - 2 ) * 2 }, handles) );
	SIR_STRESS_CHECK( scheduler.getState( handles[N - 1] ) == SIR_TASK_BLOCKED );
	const sirTaskHandle_t stale = handles[0];
	SIR_STRESS_CHECK( scheduler.cancel( handles[0] ) );
	for ( uint32_t i = 0; i < N; ++i ) {
		SIR_STRESS_CHECK( scheduler.getState( handles[i] ) == SIR_TASK_CANCELLED );
		SIR_STRESS_CHECK( scheduler.releaseTask( handles[i] ) );
	}
	SIR_STRESS_CHECK( nCount.load() == 0 );
	SIR_STRESS_CHECK( scheduler.submitBatch({ tasks, N, nullptr, 0 }, handles) );
	SIR_STRESS_CHECK( scheduler.getState( stale ) == SIR_TASK_INVALID );
	SIR_STRESS_CHECK( scheduler.pumpCPU( N ) == N );
	for ( uint32_t i = 0; i < N; ++i ) {
		SIR_STRESS_CHECK( scheduler.getState( handles[i] ) == SIR_TASK_COMPLETED );
		SIR_STRESS_CHECK( scheduler.releaseTask( handles[i] ) );
	}
	SIR_STRESS_CHECK( nCount.load() == N );
	SIR_STRESS_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

typedef struct sirStressFenceContext_s
{
	uint32_t nFinalizers;
} sirStressFenceContext_t;

static bool finalizeFenceStress( void *pContext ) noexcept
{
	++( ( sirStressFenceContext_t * )pContext )->nFinalizers;
	return true;
}

static void testFenceBurstsAndCancellation( void )
{
	SIRMemoryManager memory;
	initStressMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_STRESS_CHECK( scheduler.init( &memory, { 80, 0, 80 } ) );
	SIRTaskFenceTracker tracker;
	SIR_STRESS_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 64 ) );
	sirTaskHandle_t handles[64] = {};
	sirStressFenceContext_t contexts[64] = {};
	for ( uint32_t i = 0; i < 64; ++i ) {
		SIR_STRESS_CHECK( scheduler.submit({ deferredStress, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &handles[i]) );
	}
	SIR_STRESS_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 64 ) == 64 );
	for ( uint32_t i = 0; i < 64; ++i ) {
		SIR_STRESS_CHECK( tracker.track( handles[i], 1 + i / 8, finalizeFenceStress, &contexts[i] ) );
		SIR_STRESS_CHECK( !scheduler.releaseTask( handles[i] ) );
		if ( i % 3 == 0 ) {
			SIR_STRESS_CHECK( scheduler.cancel( handles[i] ) );
		}
	}
	std::thread nonOwner( [&]() {
		uint32_t nAcknowledged = 0;
		SIR_STRESS_CHECK( !tracker.advanceCompleted( 8, SIR_TASK_RESULT_SUCCESS, &nAcknowledged ) );
	} );
	nonOwner.join();
	uint32_t nCompleted = 0;
	for ( uint32_t nFence = 1; nFence <= 8; ++nFence ) {
		uint32_t nAcknowledged = UINT32_MAX;
		SIR_STRESS_CHECK( tracker.advanceCompleted( nFence, SIR_TASK_RESULT_SUCCESS, &nAcknowledged ) );
		SIR_STRESS_CHECK( nAcknowledged == 8 );
		nCompleted += nAcknowledged;
		SIR_STRESS_CHECK( tracker.getPendingCount() == 64 - nCompleted );
	}
	for ( uint32_t i = 0; i < 64; ++i ) {
		SIR_STRESS_CHECK( contexts[i].nFinalizers == 1 );
		SIR_STRESS_CHECK( scheduler.getState( handles[i] ) == ( i % 3 == 0 ? SIR_TASK_CANCELLED : SIR_TASK_COMPLETED ) );
		SIR_STRESS_CHECK( scheduler.releaseTask( handles[i] ) );
	}
	SIR_STRESS_CHECK( tracker.shutdown() );
	SIR_STRESS_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

int main( void )
{
	testConcurrentProducersAndMetadata();
	testPriorityProgressAndFifo();
	testRandomizedDependencyDags();
	testCancellationWidthAndGenerations();
	testFenceBurstsAndCancellation();
	return 0;
}
