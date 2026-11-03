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

#include <Core/Containers/SIRContainerHeapAdapter.h>
#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <benchmark/benchmark.h>

class SIRBenchmarkContainerHeap
{
public:
	SIRMemoryManager m_Memory;
	SIRCompactBucketAllocator m_Small;
	SIRFreeListAllocator m_Medium;
	SIRBuddyAllocator m_Large;
	SIRGeneralHeap m_Heap;
	SIRContainerHeapAdapter m_Adapter;

	bool init( void )
	{
		const uint64_t nPage = SIRVirtualMemory::getPageSize();
		if ( nPage == 0 ) {
			return false;
		}
		const sirMemoryConfig_t config = { nPage * 4096, nPage * 3072, nPage * 384, 12 };
		if ( !m_Memory.init( config ) ) {
			return false;
		}
		SIRMemorySegment *pSmall = m_Memory.createSegment( "bench.small", nPage * 256, 0, SIR_MEMORY_DOMAIN_GAMEPLAY );
		SIRMemorySegment *pMedium = m_Memory.createSegment( "bench.medium", nPage * 256, 0, SIR_MEMORY_DOMAIN_GAMEPLAY );
		SIRMemorySegment *pLarge = m_Memory.createSegment( "bench.large", nPage * 1024, 0, SIR_MEMORY_DOMAIN_GAMEPLAY );
		if ( pSmall == nullptr || pMedium == nullptr || pLarge == nullptr ) {
			return false;
		}
		const sirCompactBucketConfig_t bucketConfig = { 256, 16, 16384, 2, 0, 0 };
		if ( !m_Small.init( pSmall, bucketConfig ) || !m_Medium.init( pMedium ) || !m_Large.init( pLarge ) ) {
			return false;
		}
		const sirGeneralHeapConfig_t heapConfig = { &m_Small, &m_Medium, nullptr, &m_Large, pSmall, pMedium, pLarge, 256, 4096, false };
		return m_Heap.init( heapConfig ) && m_Heap.attachThread() && m_Adapter.init( &m_Heap, SIR_MEMORY_DOMAIN_GAMEPLAY, 0x434F4E54u );
	}

	bool shutdown( void )
	{
		return m_Heap.detachThread() && m_Heap.shutdown() && m_Small.shutdown() && m_Medium.shutdown();
	}
};

static void SIRBenchmarkGeneralHeapDirect( benchmark::State &state )
{
	SIRBenchmarkContainerHeap heap;
	if ( !heap.init() ) {
		state.SkipWithError( "Failed to initialize SIR benchmark heap" );
		return;
	}
	const size_t nBytes = ( size_t )state.range( 0 );
	for ( auto _ : state ) {
		( void )_;
		void *pMemory = heap.m_Heap.allocAligned( nBytes, 16 );
		if ( pMemory == nullptr ) {
			state.SkipWithError( "SIRGeneralHeap allocation failed" );
			break;
		}
		benchmark::DoNotOptimize( pMemory );
		if ( !heap.m_Heap.tryFree( pMemory ) ) {
			state.SkipWithError( "SIRGeneralHeap free failed" );
			break;
		}
	}
	if ( !heap.shutdown() ) {
		state.SkipWithError( "SIRGeneralHeap shutdown failed" );
	}
}

static void SIRBenchmarkContainerAdapter( benchmark::State &state )
{
	SIRBenchmarkContainerHeap heap;
	if ( !heap.init() ) {
		state.SkipWithError( "Failed to initialize SIR benchmark heap" );
		return;
	}
	const sirContainerAllocator_t *pAllocator = heap.m_Adapter.getAllocator();
	const size_t nBytes = ( size_t )state.range( 0 );
	for ( auto _ : state ) {
		( void )_;
		void *pMemory = pAllocator->pfnAllocate( pAllocator->pContext, nBytes, 16 );
		if ( pMemory == nullptr ) {
			state.SkipWithError( "Container adapter allocation failed" );
			break;
		}
		benchmark::DoNotOptimize( pMemory );
		pAllocator->pfnDeallocate( pAllocator->pContext, pMemory, nBytes, 16 );
	}
	if ( !heap.shutdown() ) {
		state.SkipWithError( "Container adapter shutdown failed" );
	}
}

BENCHMARK( SIRBenchmarkGeneralHeapDirect )->Arg( 64 )->Arg( 1536 )->Arg( 32768 );
BENCHMARK( SIRBenchmarkContainerAdapter )->Arg( 64 )->Arg( 1536 )->Arg( 32768 );
