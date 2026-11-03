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

#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include <vector>

static bool sirInitExperimentMemory( SIRMemoryManager &memory )
{
	const sirMemoryConfig_t config = { 256ULL * 1024 * 1024, 96ULL * 1024 * 1024, 64ULL * 1024 * 1024, 32 };
	return memory.init( config );
}

static uint64_t sirExperimentMix( uint64_t nValue ) noexcept
{
	nValue ^= nValue >> 30;
	nValue *= UINT64_C( 0xbf58476d1ce4e5b9 );
	nValue ^= nValue >> 27;
	nValue *= UINT64_C( 0x94d049bb133111eb );
	return nValue ^ ( nValue >> 31 );
}

typedef struct sirExperimentPayload_s
{
	std::atomic<uint32_t> *pShared;
	uint64_t *pData;
	uint64_t nFirst;
	uint64_t nCount;
	alignas( 64 ) uint64_t nResult;
	uint32_t nIterations;
	uint32_t nMode;
} sirExperimentPayload_t;

static sirTaskResult_t sirExperimentCallback( void *pContext, sirTaskHandle_t ) noexcept
{
	sirExperimentPayload_t *pPayload = ( sirExperimentPayload_t * )pContext;
	uint64_t nResult = 0;
	for ( uint64_t i = 0; i < pPayload->nCount; ++i ) {
		const uint64_t nInput = pPayload->pData == nullptr ? pPayload->nFirst + i : pPayload->pData[pPayload->nFirst + i];
		uint64_t nValue = nInput + 1;
		for ( uint32_t k = 0; k < pPayload->nIterations; ++k ) {
			nValue = sirExperimentMix( nValue );
		}
		nResult += nValue;
	}
	if ( pPayload->nMode == 1 ) {
		pPayload->pShared->fetch_add( 1, std::memory_order_relaxed );
	}
	pPayload->nResult = nResult;
	return SIR_TASK_RESULT_SUCCESS;
}

// E02: isolate scheduler overhead from benchmark-induced shared cache contention.
// mode 0 = per-task cache-line isolated result; mode 1 = shared atomic update.
static void SIRBenchmarkExperimentContention( benchmark::State &state )
{
	const uint32_t nWorkers = ( uint32_t )state.range( 0 );
	const uint32_t nMode = ( uint32_t )state.range( 1 );
	constexpr uint32_t N_TASKS = 256;
	SIRMemoryManager memory;
	if ( !sirInitExperimentMemory( memory ) ) {
		state.SkipWithError( "experiment memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { N_TASKS + 8, nWorkers, N_TASKS * 2 } ) ) {
		state.SkipWithError( "experiment scheduler initialization failed" );
		memory.shutdown();
		return;
	}
	std::atomic<uint32_t> nShared( 0 );
	std::vector<sirExperimentPayload_t> contexts( N_TASKS );
	std::vector<sirTaskHandle_t> handles( N_TASKS );
	for ( uint32_t i = 0; i < N_TASKS; ++i ) {
		contexts[i] = { &nShared, nullptr, i, 1, 0, 32, nMode };
	}
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		for ( uint32_t i = 0; i < N_TASKS; ++i ) {
			if ( !scheduler.submit( sirExperimentCallback, &contexts[i], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &handles[i] ) ) {
				bFailed = true;
				break;
			}
		}
		if ( !bFailed ) {
			for ( uint32_t i = 0; i < N_TASKS; ++i ) {
				if ( !scheduler.waitTask( handles[i] ) || !scheduler.releaseTask( handles[i] ) ) {
					bFailed = true;
					break;
				}
			}
		}
		if ( bFailed ) {
			state.SkipWithError( "callback contention experiment failed" );
			break;
		}
	}
	state.SetItemsProcessed( state.iterations() * N_TASKS );
	state.counters["shared_updates"] = ( double )nShared.load( std::memory_order_relaxed );
	state.counters["steals"] = ( double )scheduler.getStats().nStealSucceeded;
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "callback contention teardown failed" );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkExperimentContention )
    ->Args( { 1, 0 } )
    ->Args( { 2, 0 } )
    ->Args( { 4, 0 } )
    ->Args( { 8, 0 } )
    ->Args( { 1, 1 } )
    ->Args( { 2, 1 } )
    ->Args( { 4, 1 } )
    ->Args( { 8, 1 } );

// G01/G02: keep identical useful work while varying the number of task envelopes.
static void SIRBenchmarkExperimentGranularity( benchmark::State &state )
{
	const uint32_t nWorkers = ( uint32_t )state.range( 0 );
	const uint32_t nChunk = ( uint32_t )state.range( 1 );
	const uint32_t nIterations = ( uint32_t )state.range( 2 );
	constexpr uint32_t N_ELEMENTS = 4096;
	const uint32_t nTasks = ( N_ELEMENTS + nChunk - 1 ) / nChunk;
	SIRMemoryManager memory;
	if ( !sirInitExperimentMemory( memory ) ) {
		state.SkipWithError( "granularity memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { nTasks + 8, nWorkers, nTasks * 2 + 16 } ) ) {
		state.SkipWithError( "granularity scheduler initialization failed" );
		memory.shutdown();
		return;
	}
	std::vector<sirExperimentPayload_t> contexts( nTasks );
	std::vector<sirTaskHandle_t> handles( nTasks );
	for ( uint32_t i = 0; i < nTasks; ++i ) {
		const uint64_t nFirst = ( uint64_t )i * nChunk;
		const uint64_t nCount = nFirst + nChunk > N_ELEMENTS ? N_ELEMENTS - nFirst : nChunk;
		contexts[i] = { nullptr, nullptr, nFirst, nCount, 0, nIterations, 0 };
	}
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		for ( uint32_t i = 0; i < nTasks; ++i ) {
			if ( !scheduler.submit( sirExperimentCallback, &contexts[i], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &handles[i] ) ) {
				bFailed = true;
				break;
			}
		}
		if ( !bFailed && nWorkers == 0 && scheduler.pumpCPU( nTasks ) != nTasks ) {
			bFailed = true;
		}
		if ( !bFailed ) {
			for ( uint32_t i = 0; i < nTasks; ++i ) {
				if ( ( nWorkers != 0 && !scheduler.waitTask( handles[i] ) ) || !scheduler.releaseTask( handles[i] ) ) {
					bFailed = true;
					break;
				}
				benchmark::DoNotOptimize( contexts[i].nResult );
			}
		}
		if ( bFailed ) {
			state.SkipWithError( "granularity experiment failed" );
			break;
		}
	}
	state.SetItemsProcessed( state.iterations() * N_ELEMENTS );
	state.counters["tasks_per_batch"] = ( double )nTasks;
	state.counters["metadata_bytes"] = ( double )memory.getStats().nMetadataUsedBytes;
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "granularity teardown failed" );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkExperimentGranularity )
    ->Args( { 0, 16, 0 } )
    ->Args( { 1, 16, 0 } )
    ->Args( { 4, 16, 0 } )
    ->Args( { 4, 64, 0 } )
    ->Args( { 4, 256, 0 } )
    ->Args( { 4, 1024, 0 } )
    ->Args( { 8, 64, 0 } )
    ->Args( { 4, 16, 8 } )
    ->Args( { 4, 64, 8 } )
    ->Args( { 4, 256, 8 } );

typedef struct sirExperimentSpawner_s
{
	SIRTaskScheduler *pScheduler;
	sirTaskHandle_t *pHandles;
	sirExperimentPayload_t *pContexts;
	uint32_t nChildren;
	bool bValid;
} sirExperimentSpawner_t;

static sirTaskResult_t sirExperimentSpawnCallback( void *pContext, sirTaskHandle_t ) noexcept
{
	sirExperimentSpawner_t *pSpawn = ( sirExperimentSpawner_t * )pContext;
	for ( uint32_t i = 0; i < pSpawn->nChildren; ++i ) {
		if ( !pSpawn->pScheduler->submit( sirExperimentCallback, &pSpawn->pContexts[i], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU,
		                                  &pSpawn->pHandles[i] ) ) {
			pSpawn->bValid = false;
			return SIR_TASK_RESULT_FAILURE;
		}
	}
	return SIR_TASK_RESULT_SUCCESS;
}

// Q06/Q07: deliberately produces *worker-local* jobs so real theft can occur.
static void SIRBenchmarkExperimentLocalSteal( benchmark::State &state )
{
	const uint32_t nWorkers = ( uint32_t )state.range( 0 );
	const uint32_t nChildren = ( uint32_t )state.range( 1 );
	SIRMemoryManager memory;
	if ( !sirInitExperimentMemory( memory ) ) {
		state.SkipWithError( "local-steal memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { nChildren + 8, nWorkers, ( nChildren + 8 ) * 2 } ) ) {
		state.SkipWithError( "local-steal scheduler initialization failed" );
		memory.shutdown();
		return;
	}
	std::vector<sirExperimentPayload_t> contexts( nChildren );
	std::vector<sirTaskHandle_t> handles( nChildren );
	for ( uint32_t i = 0; i < nChildren; ++i ) {
		contexts[i] = { nullptr, nullptr, i, 16, 0, 16, 0 };
	}
	sirExperimentSpawner_t spawn = { &scheduler, handles.data(), contexts.data(), nChildren, true };
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		sirTaskHandle_t root = {};
		if ( !scheduler.submit( sirExperimentSpawnCallback, &spawn, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &root ) ||
		     !scheduler.waitTask( root ) || !spawn.bValid || !scheduler.releaseTask( root ) ) {
			bFailed = true;
		}
		if ( !bFailed ) {
			for ( uint32_t i = 0; i < nChildren; ++i ) {
				if ( !scheduler.waitTask( handles[i] ) || !scheduler.releaseTask( handles[i] ) ) {
					bFailed = true;
					break;
				}
			}
		}
		if ( bFailed ) {
			state.SkipWithError( "worker-local stealing experiment failed" );
			break;
		}
	}
	state.SetItemsProcessed( state.iterations() * nChildren );
	const sirTaskSchedulerStats_t stats = scheduler.getStats();
	state.counters["steal_attempts"] = ( double )stats.nStealAttempts;
	state.counters["steal_successes"] = ( double )stats.nStealSucceeded;
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "local-steal teardown failed" );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkExperimentLocalSteal )->Args( { 2, 64 } )->Args( { 4, 128 } )->Args( { 8, 256 } );

// D06: isolate batch submission + execution across chain, star, and diamond DAGs.
static void SIRBenchmarkExperimentDagShape( benchmark::State &state )
{
	const uint32_t nWorkers = ( uint32_t )state.range( 0 );
	const uint32_t nTasks = ( uint32_t )state.range( 1 );
	const uint32_t nShape = ( uint32_t )state.range( 2 );
	SIRMemoryManager memory;
	if ( !sirInitExperimentMemory( memory ) ) {
		state.SkipWithError( "DAG memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { nTasks + 8, nWorkers, nTasks * 3 } ) ) {
		state.SkipWithError( "DAG scheduler initialization failed" );
		memory.shutdown();
		return;
	}
	std::vector<sirExperimentPayload_t> contexts( nTasks );
	std::vector<sirTaskBatchTask_t> tasks( nTasks );
	std::vector<sirTaskBatchEdge_t> edges;
	std::vector<sirTaskHandle_t> handles( nTasks );
	for ( uint32_t i = 0; i < nTasks; ++i ) {
		contexts[i] = { nullptr, nullptr, i, 1, 0, 32, 0 };
		tasks[i] = { sirExperimentCallback, &contexts[i], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
		if ( i == 0 ) {
			continue;
		}
		if ( nShape == 0 ) {
			edges.push_back( { i - 1, i } );
		} else if ( nShape == 1 ) {
			edges.push_back( { 0, i } );
		} else {
			edges.push_back( { ( i - 1 ) / 2, i } );
		}
	}
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		if ( !scheduler.submitBatch( tasks.data(), nTasks, edges.data(), ( uint32_t )edges.size(), handles.data() ) ) {
			bFailed = true;
		}
		if ( !bFailed && nWorkers == 0 && scheduler.pumpCPU( nTasks ) != nTasks ) {
			bFailed = true;
		}
		if ( !bFailed ) {
			for ( uint32_t i = 0; i < nTasks; ++i ) {
				if ( ( nWorkers != 0 && !scheduler.waitTask( handles[i] ) ) || !scheduler.releaseTask( handles[i] ) ) {
					bFailed = true;
					break;
				}
			}
		}
		if ( bFailed ) {
			state.SkipWithError( "DAG shape execution failed" );
			break;
		}
	}
	state.SetItemsProcessed( state.iterations() * nTasks );
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "DAG teardown failed" );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkExperimentDagShape )
    ->Args( { 0, 64, 0 } )
    ->Args( { 4, 64, 0 } )
    ->Args( { 4, 64, 1 } )
    ->Args( { 4, 64, 2 } )
    ->Args( { 8, 128, 0 } )
    ->Args( { 8, 128, 1 } )
    ->Args( { 8, 128, 2 } );

// D08: graph compilation only; reset/addPass are excluded from timing.
// mode 0 = unique resources, 1 = all readers, 2 = overlapping read/write.
static void SIRBenchmarkExperimentGraphCompile( benchmark::State &state )
{
	const uint32_t nPasses = ( uint32_t )state.range( 0 );
	const uint32_t nMode = ( uint32_t )state.range( 1 );
	SIRMemoryManager memory;
	if ( !sirInitExperimentMemory( memory ) ) {
		state.SkipWithError( "graph compile memory initialization failed" );
		return;
	}
	SIRTaskGraph graph;
	if ( !graph.init( &memory, { nPasses + 8, nPasses + 8, nPasses * nPasses + 8 } ) ) {
		state.SkipWithError( "graph compiler initialization failed" );
		memory.shutdown();
		return;
	}
	const sirTaskGraphPass_t pass = { sirExperimentCallback, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	for ( auto _ : state ) {
		( void )_;
		state.PauseTiming();
		bool bPrepared = graph.reset();
		for ( uint32_t i = 0; bPrepared && i < nPasses; ++i ) {
			const sirTaskGraphResourceKey_t key = { nMode == 0 ? i + 1U : 1U, 1U };
			const sirTaskGraphAccess_t access = { key, nMode == 2 ? ( uint64_t )i * 8 : 0, nMode == 2 ? 64U : 16U,
			                                      nMode == 1 ? SIR_TASK_GRAPH_READ : SIR_TASK_GRAPH_WRITE };
			uint32_t nIndex = 0;
			bPrepared = graph.addPass( pass, &access, 1, &nIndex );
		}
		state.ResumeTiming();
		if ( !bPrepared || graph.compile() != SIR_TASK_GRAPH_OK ) {
			state.SkipWithError( "graph compilation failed" );
			break;
		}
		benchmark::DoNotOptimize( graph.getEdgeCount() );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkExperimentGraphCompile )
    ->Args( { 32, 0 } )
    ->Args( { 32, 1 } )
    ->Args( { 32, 2 } )
    ->Args( { 128, 0 } )
    ->Args( { 128, 1 } )
    ->Args( { 128, 2 } );
