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
#include <vector>

#define SIR_EXPERIMENT_CHECK( expression )                                                                                                 \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Scheduler experiment assertion failed: %s:%d: %s\n", __FILE__, __LINE__, #expression );                      \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static bool sirExperimentMemory( SIRMemoryManager& memory )
{
	const sirMemoryConfig_t config = { 128ULL * 1024 * 1024, 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 32 };
	return memory.init( config );
}

typedef struct sirExperimentRange_s
{
	uint64_t nFirst;
	uint64_t nEnd;
	uint64_t nResult;
} sirExperimentRange_t;

static uint64_t sirExperimentHash( uint64_t nValue ) noexcept
{
	nValue ^= nValue >> 30;
	nValue *= UINT64_C( 0xbf58476d1ce4e5b9 );
	nValue ^= nValue >> 27;
	nValue *= UINT64_C( 0x94d049bb133111eb );
	return nValue ^ ( nValue >> 31 );
}

static sirTaskResult_t sirExperimentRangeTask( void *pContext, sirTaskHandle_t ) noexcept
{
	sirExperimentRange_t *pRange = ( sirExperimentRange_t * )pContext;
	uint64_t nSum = 0;
	for ( uint64_t i = pRange->nFirst; i < pRange->nEnd; ++i ) {
		nSum += sirExperimentHash( i + 1 );
	}
	pRange->nResult = nSum;
	return SIR_TASK_RESULT_SUCCESS;
}

static void sirExperimentCheckGranularity( uint32_t nWorkers, uint32_t nChunk )
{
	constexpr uint32_t N_ELEMENTS = 4096;
	const uint32_t nTasks = ( N_ELEMENTS + nChunk - 1 ) / nChunk;
	SIRMemoryManager memory;
	SIR_EXPERIMENT_CHECK( sirExperimentMemory( memory ) );
	SIRTaskScheduler scheduler;
	SIR_EXPERIMENT_CHECK( scheduler.init( &memory, { nTasks + 8, nWorkers, ( nTasks + 8 ) * 2 } ) );
	const uint64_t nMetadata = memory.getStats().nMetadataUsedBytes;

	std::vector<sirExperimentRange_t> ranges( nTasks );
	std::vector<sirTaskHandle_t> handles( nTasks );
	uint64_t nExpected = 0;
	for ( uint32_t i = 0; i < N_ELEMENTS; ++i ) {
		nExpected += sirExperimentHash( i + 1 );
	}
	for ( uint32_t iRound = 0; iRound < 8; ++iRound ) {
		for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
			const uint64_t nFirst = ( uint64_t )iTask * nChunk;
			const uint64_t nEnd = nFirst + nChunk < N_ELEMENTS ? nFirst + nChunk : N_ELEMENTS;
			ranges[iTask] = { nFirst, nEnd, 0 };
			SIR_EXPERIMENT_CHECK( scheduler.submit( { sirExperimentRangeTask, &ranges[iTask], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU },
			                                        &handles[iTask] ) );
		}
		if ( nWorkers == 0 ) {
			SIR_EXPERIMENT_CHECK( scheduler.pumpCPU( nTasks ) == nTasks );
		}
		uint64_t nActual = 0;
		for ( uint32_t iTask = 0; iTask < nTasks; ++iTask ) {
			if ( nWorkers != 0 ) {
				SIR_EXPERIMENT_CHECK( scheduler.waitTask( handles[iTask] ) );
			}
			SIR_EXPERIMENT_CHECK( scheduler.getState( handles[iTask] ) == SIR_TASK_COMPLETED );
			nActual += ranges[iTask].nResult;
			SIR_EXPERIMENT_CHECK( scheduler.releaseTask( handles[iTask] ) );
		}
		SIR_EXPERIMENT_CHECK( nActual == nExpected );
		SIR_EXPERIMENT_CHECK( scheduler.getActiveCount() == 0 );
		SIR_EXPERIMENT_CHECK( memory.getStats().nMetadataUsedBytes == nMetadata );
	}
	SIR_EXPERIMENT_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

typedef struct sirExperimentSpawn_s
{
	SIRTaskScheduler *pScheduler;
	sirTaskHandle_t *pChildren;
	std::atomic<uint32_t> *pCompleted;
	uint32_t nChildren;
	std::atomic<bool> bSubmissionOk;
} sirExperimentSpawn_t;

static sirTaskResult_t sirExperimentChild( void *pContext, sirTaskHandle_t ) noexcept
{
	( ( std::atomic<uint32_t> * )pContext )->fetch_add( 1, std::memory_order_relaxed );
	return SIR_TASK_RESULT_SUCCESS;
}

static sirTaskResult_t sirExperimentSpawn( void *pContext, sirTaskHandle_t ) noexcept
{
	sirExperimentSpawn_t *pSpawn = ( sirExperimentSpawn_t * )pContext;
	for ( uint32_t i = 0; i < pSpawn->nChildren; ++i ) {
		if ( !pSpawn->pScheduler->submit( { sirExperimentChild, pSpawn->pCompleted, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU },
﻿2026 - 11 - 06T00 : 14 : 49.2215582Z & pSpawn->pChildren[i] ) ) {
			pSpawn->bSubmissionOk.store( false, std::memory_order_release );
			return SIR_TASK_RESULT_FAILURE;
		}
	}
	return SIR_TASK_RESULT_SUCCESS;
}

static void sirExperimentCheckStealing( void )
{
	SIRMemoryManager memory;
	SIR_EXPERIMENT_CHECK( sirExperimentMemory( memory ) );
	SIRTaskScheduler scheduler;
	constexpr uint32_t N_CHILDREN = 128;
	SIR_EXPERIMENT_CHECK( scheduler.init( &memory, { N_CHILDREN + 8, 4, 512 } ) );
	std::atomic<uint32_t> nCompleted( 0 );
	sirTaskHandle_t children[N_CHILDREN] = {};
	sirExperimentSpawn_t spawn = { &scheduler, children, &nCompleted, N_CHILDREN, true };
	for ( uint32_t iRound = 0; iRound < 20; ++iRound ) {
		sirTaskHandle_t root = {};
		SIR_EXPERIMENT_CHECK( scheduler.submit( { sirExperimentSpawn, &spawn, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &root ) );
		SIR_EXPERIMENT_CHECK( scheduler.waitTask( root ) );
		SIR_EXPERIMENT_CHECK( spawn.bSubmissionOk.load() );
		SIR_EXPERIMENT_CHECK( scheduler.releaseTask( root ) );
		for ( uint32_t i = 0; i < N_CHILDREN; ++i ) {
			SIR_EXPERIMENT_CHECK( scheduler.waitTask( children[i] ) );
			SIR_EXPERIMENT_CHECK( scheduler.releaseTask( children[i] ) );
		}
	}
	SIR_EXPERIMENT_CHECK( nCompleted.load() == 20 * N_CHILDREN );
	SIR_EXPERIMENT_CHECK( scheduler.getActiveCount() == 0 );
	SIR_EXPERIMENT_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

typedef struct sirExperimentDependent_s
{
	std::atomic<uint32_t> *pVisits;
	uint32_t nIndex;
	uint32_t nExpectedParent;
} sirExperimentDependent_t;

static sirTaskResult_t sirExperimentDependent( void *pContext, sirTaskHandle_t ) noexcept
{
	sirExperimentDependent_t *pTask = ( sirExperimentDependent_t * )pContext;
	if ( pTask->nIndex != 0 && pTask->pVisits[pTask->nExpectedParent].load( std::memory_order_acquire ) == 0 ) {
		return SIR_TASK_RESULT_FAILURE;
	}
	pTask->pVisits[pTask->nIndex].store( 1, std::memory_order_release );
	return SIR_TASK_RESULT_SUCCESS;
}

static void sirExperimentCheckDag( uint32_t nWorkers, bool bFanOut )
{
	constexpr uint32_t N_TASKS = 64;
	SIRMemoryManager memory;
	SIR_EXPERIMENT_CHECK( sirExperimentMemory( memory ) );
	SIRTaskScheduler scheduler;
	SIR_EXPERIMENT_CHECK( scheduler.init( &memory, { N_TASKS + 8, nWorkers, 256 } ) );
	std::atomic<uint32_t> visits[N_TASKS] = {};
	sirExperimentDependent_t contexts[N_TASKS] = {};
	sirTaskBatchTask_t tasks[N_TASKS] = {};
	sirTaskBatchEdge_t edges[N_TASKS - 1] = {};
	sirTaskHandle_t handles[N_TASKS] = {};
	for ( uint32_t i = 0; i < N_TASKS; ++i ) {
		contexts[i] = { visits, i, bFanOut ? 0 : ( i == 0 ? 0 : i - 1 ) };
		tasks[i] = { sirExperimentDependent, &contexts[i], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
		if ( i != 0 ) {
			edges[i - 1] = { bFanOut ? 0U : i - 1, i };
		}
	}
	SIR_EXPERIMENT_CHECK( scheduler.submitBatch( { tasks, N_TASKS, edges, N_TASKS - 1 }, handles ) );
	if ( nWorkers == 0 ) {
		SIR_EXPERIMENT_CHECK( scheduler.pumpCPU( N_TASKS ) == N_TASKS );
	}
	for ( uint32_t i = 0; i < N_TASKS; ++i ) {
		if ( nWorkers != 0 ) {
			SIR_EXPERIMENT_CHECK( scheduler.waitTask( handles[i] ) );
		}
		SIR_EXPERIMENT_CHECK( scheduler.getState( handles[i] ) == SIR_TASK_COMPLETED );
		SIR_EXPERIMENT_CHECK( visits[i].load() == 1 );
		SIR_EXPERIMENT_CHECK( scheduler.releaseTask( handles[i] ) );
	}
	SIR_EXPERIMENT_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

int main( void )
{
	for ( const uint32_t nWorkers : { 0U, 1U, 4U } ) {
		for ( const uint32_t nChunk : { 16U, 64U, 256U, 1024U } ) {
			sirExperimentCheckGranularity( nWorkers, nChunk );
		}
		sirExperimentCheckDag( nWorkers, false );
		sirExperimentCheckDag( nWorkers, true );
	}
	sirExperimentCheckStealing();
	return 0;
}
