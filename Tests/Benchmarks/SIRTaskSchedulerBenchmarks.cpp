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
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

static sirTaskResult_t sirBenchmarkNoop( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_SUCCESS;
}

static sirTaskResult_t sirBenchmarkDeferred( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_PENDING;
}

static bool sirInitBenchmarkMemory( SIRMemoryManager& memory ) noexcept
{
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 };
	return memory.init( config );
}

static void SIRBenchmarkTaskSubmitPumpRelease( benchmark::State& state )
{
	SIRMemoryManager memory;
	if ( !sirInitBenchmarkMemory( memory ) ) {
		state.SkipWithError( "SIR task memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { 128, 0, 512 } ) ) {
		state.SkipWithError( "SIR task scheduler initialization failed" );
		return;
	}
	for ( auto _ : state ) {
		( void )_;
		sirTaskHandle_t task = {};
		if ( !scheduler.submit( sirBenchmarkNoop, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &task ) ||
		     scheduler.pumpCPU( 1 ) != 1 || !scheduler.releaseTask( task ) ) {
			state.SkipWithError( "SIR CPU task completion failed" );
			break;
		}
		benchmark::DoNotOptimize( task );
	}
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "SIR CPU scheduler shutdown failed" );
	}
	memory.shutdown();
}

static void SIRBenchmarkTaskBatch( benchmark::State& state )
{
	SIRMemoryManager memory;
	if ( !sirInitBenchmarkMemory( memory ) ) {
		state.SkipWithError( "SIR batch memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { 128, 0, 512 } ) ) {
		state.SkipWithError( "SIR batch scheduler initialization failed" );
		return;
	}
	const uint32_t nTasks = ( uint32_t )state.range( 0 );
	sirTaskBatchTask_t tasks[64] = {};
	sirTaskBatchEdge_t edges[63] = {};
	sirTaskHandle_t handles[64] = {};
	for ( uint32_t i = 0; i < nTasks; ++i ) {
		tasks[i] = { sirBenchmarkNoop, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
		if ( i != 0 ) {
			edges[i - 1] = { i - 1, i };
		}
	}
	for ( auto _ : state ) {
		( void )_;
		if ( !scheduler.submitBatch( tasks, nTasks, edges, nTasks - 1, handles ) || scheduler.pumpCPU( nTasks ) != nTasks ) {
			state.SkipWithError( "SIR batch dispatch failed" );
			break;
		}
		for ( uint32_t i = 0; i < nTasks; ++i ) {
			if ( !scheduler.releaseTask( handles[i] ) ) {
				state.SkipWithError( "SIR batch release failed" );
				break;
			}
		}
	}
	state.SetItemsProcessed( state.iterations() * nTasks );
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "SIR batch scheduler shutdown failed" );
	}
	memory.shutdown();
}

static void SIRBenchmarkFenceAcknowledgement( benchmark::State& state )
{
	SIRMemoryManager memory;
	if ( !sirInitBenchmarkMemory( memory ) ) {
		state.SkipWithError( "SIR fence memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { 128, 0, 512 } ) ) {
		state.SkipWithError( "SIR fence scheduler initialization failed" );
		return;
	}
	SIRTaskFenceTracker tracker;
	if ( !tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 128 ) ) {
		state.SkipWithError( "SIR fence tracker initialization failed" );
		return;
	}
	uint64_t nFence = 0;
	for ( auto _ : state ) {
		( void )_;
		sirTaskHandle_t task = {};
		uint32_t nAcknowledged = 0;
		++nFence;
		if ( !scheduler.submit( sirBenchmarkDeferred, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER, &task ) ||
		     scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) != 1 || !tracker.track( task, nFence ) ||
		     !tracker.advanceCompleted( nFence, SIR_TASK_RESULT_SUCCESS, &nAcknowledged ) || nAcknowledged != 1 ||
		     !scheduler.releaseTask( task ) ) {
			state.SkipWithError( "SIR fence acknowledgement failed" );
			break;
		}
		benchmark::DoNotOptimize( nAcknowledged );
	}
	if ( !tracker.shutdown() || !scheduler.shutdown() ) {
		state.SkipWithError( "SIR fence shutdown failed" );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkTaskSubmitPumpRelease );
BENCHMARK( SIRBenchmarkTaskBatch )->Arg( 1 )->Arg( 8 )->Arg( 32 )->Arg( 64 );
BENCHMARK( SIRBenchmarkFenceAcknowledgement );

// Contention profiles deliberately include submission, wakeup, execution and
// release. Each benchmark owns all task metadata outside the measured loop.
// These are CPU scheduler measurements, never GPU/physics/audio device times.

typedef struct sirBenchmarkWorkerContext_s
{
	std::atomic<uint32_t> *pCompleted;
	uint32_t nWork;
} sirBenchmarkWorkerContext_t;

static sirTaskResult_t sirBenchmarkWorkerJob( void *pContext, sirTaskHandle_t task ) noexcept
{
	sirBenchmarkWorkerContext_t *pWork = ( sirBenchmarkWorkerContext_t * )pContext;
	uint64_t nValue = task.nSlot + 1;
	for ( uint32_t iWork = 0; iWork < pWork->nWork; ++iWork ) {
		nValue = nValue * UINT64_C( 6364136223846793005 ) + UINT64_C( 1442695040888963407 );
	}
	benchmark::DoNotOptimize( nValue );
	pWork->pCompleted->fetch_add( 1, std::memory_order_relaxed );
	return SIR_TASK_RESULT_SUCCESS;
}

static void SIRBenchmarkWorkerBatch( benchmark::State& state )
{
	const uint32_t nWorkers = ( uint32_t )state.range( 0 );
	const uint32_t nTasks = ( uint32_t )state.range( 1 );
	const uint32_t nWork = ( uint32_t )state.range( 2 );
	SIRMemoryManager memory;
	if ( !sirInitBenchmarkMemory( memory ) ) {
		state.SkipWithError( "SIR worker benchmark memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { nTasks + 16, nWorkers, ( nTasks + 16 ) * 2 } ) ) {
		state.SkipWithError( "SIR worker benchmark scheduler initialization failed" );
		return;
	}
	std::atomic<uint32_t> nCompleted( 0 );
	sirBenchmarkWorkerContext_t context = { &nCompleted, nWork };
	std::vector<sirTaskHandle_t> handles( nTasks );
	bool bFailed = false;
	const uint64_t nInitialSteals = scheduler.getStats().nStealSucceeded;
	for ( auto _ : state ) {
		( void )_;
		for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
			if ( !scheduler.submit( sirBenchmarkWorkerJob, &context, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &handles[iTask] ) ) {
				bFailed = true;
				break;
			}
		}
		if ( bFailed ) {
			state.SkipWithError( "SIR worker benchmark submission failed" );
			break;
		}
		for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
			if ( !scheduler.waitTask( handles[iTask] ) || !scheduler.releaseTask( handles[iTask] ) ) {
				bFailed = true;
				break;
			}
		}
		if ( bFailed ) {
			state.SkipWithError( "SIR worker benchmark completion failed" );
			break;
		}
	}
	state.SetItemsProcessed( nCompleted.load( std::memory_order_relaxed ) );
	state.counters["steals"] = ( double )( scheduler.getStats().nStealSucceeded - nInitialSteals );
	state.counters["metadata_bytes"] = ( double )memory.getStats().nMetadataUsedBytes;
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "SIR worker benchmark shutdown failed" );
	}
	memory.shutdown();
}

// Pre-created producers avoid measuring OS thread creation. Their wakeup
// cost is intentionally included in end-to-end multi-producer throughput.
static void SIRBenchmarkConcurrentProducers( benchmark::State& state )
{
	const uint32_t nProducers = ( uint32_t )state.range( 0 );
	const uint32_t nWorkers = ( uint32_t )state.range( 1 );
	constexpr uint32_t TASKS_PER_PRODUCER = 32;
	const uint32_t nTasks = nProducers * TASKS_PER_PRODUCER;
	SIRMemoryManager memory;
	if ( !sirInitBenchmarkMemory( memory ) ) {
		state.SkipWithError( "SIR producer benchmark memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { nTasks + 16, nWorkers, nTasks * 2 } ) ) {
		state.SkipWithError( "SIR producer benchmark scheduler initialization failed" );
		return;
	}
	std::atomic<uint32_t> nCompleted( 0 );
	sirBenchmarkWorkerContext_t context = { &nCompleted, 64 };
	std::vector<sirTaskHandle_t> handles( nTasks );
	std::atomic<uint32_t> nEpoch( 0 );
	std::atomic<uint32_t> nFinished( 0 );
	std::atomic<bool> bStop( false );
	std::atomic<bool> bRejected( false );
	std::mutex triggerMutex;
	std::condition_variable trigger;
	std::vector<std::thread> producers;
	producers.reserve( nProducers );
	for ( uint32_t iProducer = 0; iProducer < nProducers; ++iProducer ) {
		producers.emplace_back( [&, iProducer]() {
			uint32_t nObserved = 0;
			while ( !bStop.load( std::memory_order_acquire ) ) {
				std::unique_lock<std::mutex> lock( triggerMutex );
				trigger.wait( lock, [&]() {
					return bStop.load( std::memory_order_acquire ) || nEpoch.load( std::memory_order_acquire ) != nObserved;
				} );
				if ( bStop.load( std::memory_order_acquire ) ) {
					break;
				}
				nObserved = nEpoch.load( std::memory_order_acquire );
				lock.unlock();
				for ( uint32_t iTask = 0; iTask < TASKS_PER_PRODUCER; ++iTask ) {
					if ( !scheduler.submit( sirBenchmarkWorkerJob, &context, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU,
					                        &handles[iProducer * TASKS_PER_PRODUCER + iTask] ) ) {
						bRejected.store( true, std::memory_order_release );
						break;
					}
				}
				nFinished.fetch_add( 1, std::memory_order_release );
			}
		} );
	}
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		nFinished.store( 0, std::memory_order_relaxed );
		nEpoch.fetch_add( 1, std::memory_order_release );
		trigger.notify_all();
		while ( nFinished.load( std::memory_order_acquire ) != nProducers ) {
			std::this_thread::yield();
		}
		if ( bRejected.load( std::memory_order_acquire ) ) {
			state.SkipWithError( "SIR producer benchmark task admission failed" );
			bFailed = true;
			break;
		}
		for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
			if ( !scheduler.waitTask( handles[iTask] ) || !scheduler.releaseTask( handles[iTask] ) ) {
				state.SkipWithError( "SIR producer benchmark completion failed" );
				bFailed = true;
				break;
			}
		}
		if ( bFailed ) {
			break;
		}
	}
	bStop.store( true, std::memory_order_release );
	nEpoch.fetch_add( 1, std::memory_order_release );
	trigger.notify_all();
	for ( uint32_t iProducer = 0; iProducer < nProducers; ++iProducer ) {
		producers[iProducer].join();
	}
	state.SetItemsProcessed( nCompleted.load( std::memory_order_relaxed ) );
	state.counters["steal_attempts"] = ( double )scheduler.getStats().nStealAttempts;
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "SIR producer benchmark shutdown failed" );
	}
	memory.shutdown();
}

static void SIRBenchmarkWorkerFanInOut( benchmark::State& state )
{
	const uint32_t nWorkers = ( uint32_t )state.range( 0 );
	const uint32_t nChildren = ( uint32_t )state.range( 1 );
	const uint32_t nTasks = nChildren + 2;
	SIRMemoryManager memory;
	if ( !sirInitBenchmarkMemory( memory ) ) {
		state.SkipWithError( "SIR fan-in benchmark memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { nTasks + 16, nWorkers, nChildren * 2 + 32 } ) ) {
		state.SkipWithError( "SIR fan-in benchmark scheduler initialization failed" );
		return;
	}
	std::atomic<uint32_t> nCompleted( 0 );
	sirBenchmarkWorkerContext_t context = { &nCompleted, 256 };
	std::vector<sirTaskBatchTask_t> tasks( nTasks );
	std::vector<sirTaskBatchEdge_t> edges( nChildren * 2 );
	std::vector<sirTaskHandle_t> handles( nTasks );
	for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
		tasks[iTask] = { sirBenchmarkWorkerJob, &context, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	}
	for ( uint32_t iChild = 0; iChild < nChildren; ++iChild ) {
		edges[2 * iChild] = { 0, iChild + 1 };
		edges[2 * iChild + 1] = { iChild + 1, nTasks - 1 };
	}
	for ( auto _ : state ) {
		( void )_;
		if ( !scheduler.submitBatch( tasks.data(), nTasks, edges.data(), ( uint32_t )edges.size(), handles.data() ) ||
		     !scheduler.waitTask( handles[nTasks - 1] ) ) {
			state.SkipWithError( "SIR fan-in benchmark dispatch failed" );
			break;
		}
		bool bRelease = true;
		for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
			if ( !scheduler.releaseTask( handles[iTask] ) ) {
				bRelease = false;
				break;
			}
		}
		if ( !bRelease ) {
			state.SkipWithError( "SIR fan-in benchmark release failed" );
			break;
		}
	}
	state.SetItemsProcessed( nCompleted.load( std::memory_order_relaxed ) );
	state.counters["steals"] = ( double )scheduler.getStats().nStealSucceeded;
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "SIR fan-in benchmark shutdown failed" );
	}
	memory.shutdown();
}

static void SIRBenchmarkFenceBurst( benchmark::State& state )
{
	const uint32_t nTasks = ( uint32_t )state.range( 0 );
	SIRMemoryManager memory;
	if ( !sirInitBenchmarkMemory( memory ) ) {
		state.SkipWithError( "SIR fence burst memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { nTasks + 16, 0, nTasks * 2 } ) ) {
		state.SkipWithError( "SIR fence burst scheduler initialization failed" );
		return;
	}
	SIRTaskFenceTracker tracker;
	if ( !tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, nTasks ) ) {
		state.SkipWithError( "SIR fence burst tracker initialization failed" );
		return;
	}
	std::vector<sirTaskHandle_t> handles( nTasks );
	uint64_t nFence = 0;
	uint64_t nAcknowledgements = 0;
	for ( auto _ : state ) {
		( void )_;
		++nFence;
		bool bAccepted = true;
		for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
			if ( !scheduler.submit( sirBenchmarkDeferred, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER, &handles[iTask] ) ) {
				bAccepted = false;
				break;
			}
		}
		if ( !bAccepted || scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, nTasks ) != nTasks ) {
			state.SkipWithError( "SIR fence burst dispatch failed" );
			break;
		}
		for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
			if ( !tracker.track( handles[iTask], nFence ) ) {
				bAccepted = false;
				break;
			}
		}
		uint32_t nAcknowledged = 0;
		if ( !bAccepted || !tracker.advanceCompleted( nFence, SIR_TASK_RESULT_SUCCESS, &nAcknowledged ) || nAcknowledged != nTasks ) {
			state.SkipWithError( "SIR fence burst acknowledgement failed" );
			break;
		}
		nAcknowledgements += nAcknowledged;
		for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
			if ( !scheduler.releaseTask( handles[iTask] ) ) {
				state.SkipWithError( "SIR fence burst task release failed" );
				break;
			}
		}
	}
	state.SetItemsProcessed( ( int64_t )nAcknowledgements );
	state.counters["metadata_bytes"] = ( double )memory.getStats().nMetadataUsedBytes;
	if ( !tracker.shutdown() || !scheduler.shutdown() ) {
		state.SkipWithError( "SIR fence burst shutdown failed" );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkWorkerBatch )
    ->Args( { 1, 64, 0 } )
    ->Args( { 2, 256, 64 } )
    ->Args( { 4, 256, 64 } )
    ->Args( { 8, 256, 64 } )
    ->Args( { 4, 256, 2048 } );
BENCHMARK( SIRBenchmarkConcurrentProducers )->Args( { 2, 2 } )->Args( { 4, 4 } )->Args( { 8, 4 } )->Args( { 8, 8 } );
BENCHMARK( SIRBenchmarkWorkerFanInOut )->Args( { 1, 32 } )->Args( { 4, 128 } )->Args( { 8, 256 } );
BENCHMARK( SIRBenchmarkFenceBurst )->Arg( 32 )->Arg( 128 );

typedef struct sirBenchmarkLatencyContext_s
{
	std::chrono::steady_clock::time_point submitted;
	uint64_t nObservedNanoseconds;
} sirBenchmarkLatencyContext_t;

static sirTaskResult_t sirBenchmarkLatencyJob( void *pContext, sirTaskHandle_t ) noexcept
{
	sirBenchmarkLatencyContext_t *pLatency = ( sirBenchmarkLatencyContext_t * )pContext;
	const auto now = std::chrono::steady_clock::now();
	pLatency->nObservedNanoseconds = ( uint64_t )std::chrono::duration_cast<std::chrono::nanoseconds>( now - pLatency->submitted ).count();
	return SIR_TASK_RESULT_SUCCESS;
}

// Measures submit-to-callback-start latency under real worker contention.
// Reported p50/p95/p99 are per-batch order statistics, averaged across runs.
// Timestamp reads are part of the measured task workload; sorting is excluded.
static void SIRBenchmarkWorkerDispatchLatency( benchmark::State& state )
{
	const uint32_t nWorkers = ( uint32_t )state.range( 0 );
	constexpr uint32_t N_TASKS = 128;
	SIRMemoryManager memory;
	if ( !sirInitBenchmarkMemory( memory ) ) {
		state.SkipWithError( "SIR latency benchmark memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { N_TASKS + 16, nWorkers, N_TASKS * 2 } ) ) {
		state.SkipWithError( "SIR latency benchmark scheduler initialization failed" );
		return;
	}
	sirBenchmarkLatencyContext_t contexts[N_TASKS] = {};
	sirTaskHandle_t handles[N_TASKS] = {};
	std::vector<uint64_t> szSamples( N_TASKS );
	uint64_t nP50Total = 0;
	uint64_t nP95Total = 0;
	uint64_t nP99Total = 0;
	uint64_t nBatches = 0;
	for ( auto _ : state ) {
		( void )_;
		bool bSuccess = true;
		for ( uint32_t iTask = 0; iTask < N_TASKS; ++iTask ) {
			contexts[iTask].submitted = std::chrono::steady_clock::now();
			if ( !scheduler.submit( sirBenchmarkLatencyJob, &contexts[iTask], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU,
			                        &handles[iTask] ) ) {
				bSuccess = false;
				break;
			}
		}
		if ( !bSuccess ) {
			state.SkipWithError( "SIR latency benchmark submission failed" );
			break;
		}
		for ( uint32_t iTask = 0; iTask < N_TASKS; ++iTask ) {
			if ( !scheduler.waitTask( handles[iTask] ) || !scheduler.releaseTask( handles[iTask] ) ) {
				bSuccess = false;
				break;
			}
		}
		if ( !bSuccess ) {
			state.SkipWithError( "SIR latency benchmark task completion failed" );
			break;
		}
		state.PauseTiming();
		for ( uint32_t iTask = 0; iTask < N_TASKS; ++iTask ) {
			szSamples[iTask] = contexts[iTask].nObservedNanoseconds;
		}
		std::sort( szSamples.begin(), szSamples.end() );
		nP50Total += szSamples[( N_TASKS * 50 + 99 ) / 100 - 1];
		nP95Total += szSamples[( N_TASKS * 95 + 99 ) / 100 - 1];
		nP99Total += szSamples[( N_TASKS * 99 + 99 ) / 100 - 1];
		++nBatches;
		state.ResumeTiming();
	}
	if ( nBatches != 0 ) {
		state.counters["dispatch_p50_ns"] = ( double )nP50Total / nBatches;
		state.counters["dispatch_p95_ns"] = ( double )nP95Total / nBatches;
		state.counters["dispatch_p99_ns"] = ( double )nP99Total / nBatches;
	}
	state.SetItemsProcessed( ( int64_t )nBatches * N_TASKS );
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "SIR latency benchmark shutdown failed" );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkWorkerDispatchLatency )->Arg( 1 )->Arg( 4 )->Arg( 8 );
