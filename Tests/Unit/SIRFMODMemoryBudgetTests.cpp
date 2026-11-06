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
#include <Core/Containers/SIRRawStorage.h>
#include <atomic>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <thread>

static std::atomic<uint32_t> s_nUpstreamLive( 0 );
static void testCheck( bool bValid ) noexcept
{
	if ( !bValid ) {
		abort();
	}
}

static void *testAllocate( void *, size_t nBytes, size_t ) noexcept
{
	void *pData = malloc( nBytes );
	if ( pData != nullptr ) {
		s_nUpstreamLive.fetch_add( 1 );
	}
	return pData;
}

static void testFree( void *, void *pData, size_t, size_t ) noexcept
{
	if ( pData != nullptr ) {
		s_nUpstreamLive.fetch_sub( 1 );
		free( pData );
	}
}

static void testBudget( const sirContainerAllocator_t *pUpstream )
{
	SIRFMODMemoryBudget budget;
	testCheck( budget.configure( pUpstream, 4096 ) == SIR_AUDIO_OK );
	testCheck( budget.configure( pUpstream, 4096 ) == SIR_AUDIO_INVALID_ARGUMENT );
	const sirContainerAllocator_t *pAllocator = budget.getAllocator();
	void *pFirst = nullptr;
	void *pSecond = nullptr;
	testCheck( SIRRawStorage::tryAllocate( pAllocator, 64, 128, &pFirst ) == SIR_CONTAINER_OK );
	testCheck( ( ( uintptr_t )pFirst & 127U ) == 0 );
	testCheck( SIRRawStorage::tryAllocate( pAllocator, 512, 16, &pSecond ) == SIR_CONTAINER_OK );
	const sirFMODMemoryBudgetStats_t stats = budget.getStats();
	testCheck( stats.nCurrentBytes >= 576 && stats.nCurrentBytes <= 4096 );
	testCheck( stats.nPeakBytes == stats.nCurrentBytes && stats.nAllocations == 2 );
	testCheck( budget.shutdown() == SIR_AUDIO_BUSY );
	void *pRejected = nullptr;
	testCheck( SIRRawStorage::tryAllocate( pAllocator, 4096, 16, &pRejected ) == SIR_CONTAINER_OUT_OF_MEMORY );
	testCheck( pRejected == nullptr && budget.getStats().nFailedAllocations == 1 );
	SIRRawStorage::deallocate( pAllocator, pSecond, 512, 16 );
	SIRRawStorage::deallocate( pAllocator, pFirst, 64, 128 );
	testCheck( budget.getStats().nCurrentBytes == 0 );
	testCheck( budget.shutdown() == SIR_AUDIO_OK && budget.getAllocator() == nullptr );
	testCheck( budget.configure( pUpstream, 0 ) == SIR_AUDIO_OK );
	testCheck( SIRRawStorage::tryAllocate( budget.getAllocator(), 8192, 64, &pFirst ) == SIR_CONTAINER_OK );
	testCheck( budget.getStats().nCurrentBytes >= 8192 );
	SIRRawStorage::deallocate( budget.getAllocator(), pFirst, 8192, 64 );
	testCheck( budget.shutdown() == SIR_AUDIO_OK );
}

static void testConcurrentAccounting( const sirContainerAllocator_t *pUpstream )
{
	SIRFMODMemoryBudget budget;
	testCheck( budget.configure( pUpstream, 256 * 1024 ) == SIR_AUDIO_OK );
	const sirContainerAllocator_t *pAllocator = budget.getAllocator();
	std::thread workers[4];
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		workers[iWorker] = std::thread( [pAllocator, iWorker]() {
			for ( uint32_t iAllocation = 0; iAllocation < 2000; ++iAllocation ) {
				void *pData = nullptr;
				const size_t nBytes = 32 + ( iAllocation + iWorker * 7 ) % 256;
				const size_t nAlignment = ( iAllocation & 1 ) == 0 ? 16 : 64;
				testCheck( SIRRawStorage::tryAllocate( pAllocator, nBytes, nAlignment, &pData ) == SIR_CONTAINER_OK );
				testCheck( ( ( uintptr_t )pData & ( nAlignment - 1 ) ) == 0 );
				SIRRawStorage::deallocate( pAllocator, pData, nBytes, nAlignment );
			}
		} );
	}
	for ( std::thread& worker : workers ) {
		worker.join();
	}
	const sirFMODMemoryBudgetStats_t stats = budget.getStats();
	testCheck( stats.nAllocations == 8000 && stats.nFailedAllocations == 0 );
	testCheck( stats.nCurrentBytes == 0 && stats.nPeakBytes != 0 && stats.nPeakBytes <= stats.nBudgetBytes );
	testCheck( budget.shutdown() == SIR_AUDIO_OK );
}

int main( void )
{
	const sirContainerAllocator_t upstream = { nullptr, testAllocate, testFree, 27, 99 };
	testBudget( &upstream );
	testConcurrentAccounting( &upstream );
	testCheck( s_nUpstreamLive.load() == 0 );
	return 0;
}
