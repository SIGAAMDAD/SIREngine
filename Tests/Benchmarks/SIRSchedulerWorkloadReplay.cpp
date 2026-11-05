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

#include <benchmark/benchmark.h>
#include <vector>

// SYNTHETIC kernels. These are controlled workload categories, never a
// substitute for native rendering/Jolt/FMOD/Terrain3D provider measurements.
// All policies perform identical useful work for a given profile.

typedef struct sirReplayRange_s
{
	const uint64_t *pInput;
	uint64_t nBegin;
	uint64_t nEnd;
	uint64_t nOutput;
	uint32_t nProfile;
} sirReplayRange_t;

static uint64_t sirReplayCompute( uint64_t nSeed, uint32_t nProfile, uint32_t nIndex ) noexcept
{
	uint64_t nValue = nSeed + 1;
	uint32_t nWork = 0;
	switch ( nProfile ) {
	case 0: // Renderer CPU preparation: moderately uniform work.
		nWork = 8;
		break;
	case 1: // Terrain/HLOD: relatively expensive chunk transformation.
		nWork = 24;
		break;
	case 2: // GOAP-like uneven planning cost.
		nWork = 2 + ( nIndex % 13 ) * 3;
		break;
	case 3: // Physics-like narrow, regular numerical work.
		nWork = 12;
		break;
	case 4: // Streaming decompression-like byte transforms.
		nWork = 4;
		break;
	default: // Audio preparation / events: very short operations.
		nWork = 1;
		break;
	}
	for ( uint32_t i = 0; i < nWork; ++i ) {
		nValue ^= nValue >> 27;
		nValue *= UINT64_C( 0x3c79ac492ba7b653 );
		nValue += UINT64_C( 0x1c69b3f74ac4ae35 );
	}
	return nValue;
}

static sirTaskResult_t sirReplayTask( void *pContext, sirTaskHandle_t ) noexcept
{
	sirReplayRange_t *pRange = ( sirReplayRange_t * )pContext;
	uint64_t nResult = 0;
	for ( uint64_t i = pRange->nBegin; i < pRange->nEnd; ++i ) {
		nResult += sirReplayCompute( pRange->pInput[i], pRange->nProfile, ( uint32_t )i );
	}
	pRange->nOutput = nResult;
	return SIR_TASK_RESULT_SUCCESS;
}

// Workload IDs: 0 renderer, 1 terrain, 2 AI, 3 physics, 4 streaming, 5 audio/events.
// Run each with equal input count and constant result checksum across all policies.
static void SIRBenchmarkExperimentReplay( benchmark::State& state )
{
	const uint32_t nProfile = ( uint32_t )state.range( 0 );
	const uint32_t nWorkers = ( uint32_t )state.range( 1 );
	const uint32_t nChunk = ( uint32_t )state.range( 2 );
	constexpr uint32_t N_ITEMS = 8192;
	const uint32_t nTasks = ( N_ITEMS + nChunk - 1 ) / nChunk;
	const sirMemoryConfig_t config = { 128ULL * 1024 * 1024, 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 32 };
	SIRMemoryManager memory;
	if ( !memory.init( config ) ) {
		state.SkipWithError( "synthetic replay memory initialization failed" );
		return;
	}
	SIRTaskScheduler scheduler;
	if ( !scheduler.init( &memory, { nTasks + 16, nWorkers, nTasks * 2 + 16 } ) ) {
		state.SkipWithError( "synthetic replay scheduler initialization failed" );
		memory.shutdown();
		return;
	}
	std::vector<uint64_t> szData( N_ITEMS );
	std::vector<sirReplayRange_t> ranges( nTasks );
	std::vector<sirTaskHandle_t> handles( nTasks );
	uint64_t nExpected = 0;
	for ( uint32_t i = 0; i < N_ITEMS; ++i ) {
		szData[i] = ( uint64_t )i * UINT64_C( 0x9e3779b97f4a7c15 );
		nExpected += sirReplayCompute( szData[i], nProfile, i );
	}
	for ( uint32_t i = 0; i < nTasks; ++i ) {
		const uint64_t nBegin = ( uint64_t )i * nChunk;
		const uint64_t nEnd = nBegin + nChunk > N_ITEMS ? N_ITEMS : nBegin + nChunk;
		ranges[i] = { szData.data(), nBegin, nEnd, 0, nProfile };
	}
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		for ( uint32_t i = 0; i < nTasks; ++i ) {
			if ( !scheduler.submit( sirReplayTask, &ranges[i], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU, &handles[i] ) ) {
				bFailed = true;
				break;
			}
		}
		if ( !bFailed && nWorkers == 0 && scheduler.pumpCPU( nTasks ) != nTasks ) {
			bFailed = true;
		}
		uint64_t nActual = 0;
		if ( !bFailed ) {
			for ( uint32_t i = 0; i < nTasks; ++i ) {
				if ( ( nWorkers != 0 && !scheduler.waitTask( handles[i] ) ) || !scheduler.releaseTask( handles[i] ) ) {
					bFailed = true;
					break;
				}
				nActual += ranges[i].nOutput;
			}
		}
		if ( bFailed || nActual != nExpected ) {
			state.SkipWithError( "synthetic replay failed checksum or task completion" );
			break;
		}
		benchmark::DoNotOptimize( nActual );
	}
	state.SetItemsProcessed( state.iterations() * N_ITEMS );
	state.counters["profile_id"] = ( double )nProfile;
	state.counters["tasks_per_batch"] = ( double )nTasks;
	state.counters["metadata_bytes"] = ( double )memory.getStats().nMetadataUsedBytes;
	if ( !scheduler.shutdown() ) {
		state.SkipWithError( "synthetic replay teardown failed" );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkExperimentReplay )
    ->Args( { 0, 1, 32 } )
    ->Args( { 0, 4, 32 } )
    ->Args( { 0, 4, 128 } )
    ->Args( { 0, 8, 128 } )
    ->Args( { 1, 1, 32 } )
    ->Args( { 1, 4, 32 } )
    ->Args( { 1, 4, 128 } )
    ->Args( { 1, 8, 128 } )
    ->Args( { 2, 1, 32 } )
    ->Args( { 2, 4, 32 } )
    ->Args( { 2, 4, 128 } )
    ->Args( { 2, 8, 128 } )
    ->Args( { 3, 1, 32 } )
    ->Args( { 3, 4, 32 } )
    ->Args( { 3, 4, 128 } )
    ->Args( { 3, 8, 128 } )
    ->Args( { 4, 1, 32 } )
    ->Args( { 4, 4, 32 } )
    ->Args( { 4, 4, 128 } )
    ->Args( { 4, 8, 128 } )
    ->Args( { 5, 1, 32 } )
    ->Args( { 5, 4, 32 } )
    ->Args( { 5, 4, 128 } )
    ->Args( { 5, 8, 128 } );
