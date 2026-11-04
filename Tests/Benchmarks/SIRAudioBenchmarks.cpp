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

#include <Audio/FMOD/SIRFMODMemoryBudget.h>
#include <Audio/SIRAudioQueue.h>
#include <Core/Containers/SIRRawStorage.h>
#include <benchmark/benchmark.h>
#include <stdlib.h>

static void *benchmarkAudioAllocate( void *, size_t nBytes, size_t ) noexcept
{
	return malloc( nBytes );
}

static void benchmarkAudioDeallocate( void *, void *pData, size_t, size_t ) noexcept
{
	free( pData );
}

static void SIRBenchmarkAudioCommandQueue( benchmark::State &state )
{
	const sirContainerAllocator_t allocator = { nullptr, benchmarkAudioAllocate, benchmarkAudioDeallocate, 0, 0 };
	SIRAudioQueue<sirAudioCommand_t> queue;
	if ( !queue.initialize( &allocator, ( uint32_t )state.range( 0 ) ) ) {
		state.SkipWithError( "Audio command queue allocation failed" );
		return;
	}
	sirAudioCommand_t command = {};
	sirAudioCommand_t returned = {};
	uint64_t nSequence = 1;
	for ( auto _ : state ) {
		( void )_;
		command.nSequence = nSequence++;
		const bool bPush = queue.tryPush( command );
		const bool bPop = queue.tryPop( returned );
		benchmark::DoNotOptimize( bPush );
		benchmark::DoNotOptimize( bPop );
		benchmark::DoNotOptimize( returned.nSequence );
	}
	queue.shutdown();
	state.SetItemsProcessed( state.iterations() );
}

static void SIRBenchmarkFMODMemoryDomain( benchmark::State &state )
{
	const sirContainerAllocator_t allocator = { nullptr, benchmarkAudioAllocate, benchmarkAudioDeallocate, 0, 0 };
	SIRFMODMemoryBudget budget;
	if ( budget.configure( &allocator, 2 * 1024 * 1024 ) != SIR_AUDIO_OK ) {
		state.SkipWithError( "Audio memory domain initialization failed" );
		return;
	}
	const sirContainerAllocator_t *pDomain = budget.getAllocator();
	for ( auto _ : state ) {
		( void )_;
		void *pMemory = nullptr;
		const sirContainerResult_t eResult = SIRRawStorage::tryAllocate( pDomain, ( size_t )state.range( 0 ), 16, &pMemory );
		if ( eResult != SIR_CONTAINER_OK ) {
			state.SkipWithError( "Budgeted allocation failed" );
			break;
		}
		benchmark::DoNotOptimize( pMemory );
		SIRRawStorage::deallocate( pDomain, pMemory, ( size_t )state.range( 0 ), 16 );
	}
	if ( budget.shutdown() != SIR_AUDIO_OK ) {
		state.SkipWithError( "Audio domain leaked allocations" );
	}
	state.SetItemsProcessed( state.iterations() );
}

BENCHMARK( SIRBenchmarkAudioCommandQueue )->Arg( 64 )->Arg( 1024 );
BENCHMARK( SIRBenchmarkFMODMemoryDomain )->Arg( 64 )->Arg( 256 )->Arg( 2048 );
