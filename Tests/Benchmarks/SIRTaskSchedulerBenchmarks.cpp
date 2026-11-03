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

static sirTaskResult_t sirBenchmarkNoop( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_SUCCESS;
}

static sirTaskResult_t sirBenchmarkDeferred( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_PENDING;
}

static bool sirInitBenchmarkMemory( SIRMemoryManager &memory ) noexcept
{
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 };
	return memory.init( config );
}

static void SIRBenchmarkTaskSubmitPumpRelease( benchmark::State &state )
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

static void SIRBenchmarkTaskBatch( benchmark::State &state )
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

static void SIRBenchmarkFenceAcknowledgement( benchmark::State &state )
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
