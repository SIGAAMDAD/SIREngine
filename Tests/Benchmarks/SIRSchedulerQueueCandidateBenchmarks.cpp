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

#include "SIRSchedulerQueueCandidates.h"

#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include <vector>

// This is a queue-algorithm comparison, not a replacement engine executor:
// all variants get identical owner-only pushes, owner LIFO pops, FIFO steals,
// fixed-capacity rings, precreated threads, and equal synthetic payloads.
typedef struct alignas( 64 ) sirQueueBenchmarkWorker_s
{
	std::atomic<uint32_t> nCompleted{ 0 };
	uint64_t nChecksum = 0;
	uint32_t nSteals = 0;
} sirQueueBenchmarkWorker_t;

static uint64_t sirQueueBenchmarkMix( uint64_t nValue ) noexcept
{
	nValue ^= nValue >> 30;
	nValue *= UINT64_C( 0xbf58476d1ce4e5b9 );
	nValue ^= nValue >> 27;
	nValue *= UINT64_C( 0x94d049bb133111eb );
	return nValue ^ ( nValue >> 31 );
}

static void SIRBenchmarkQueueCandidate( benchmark::State &state )
{
	const sirSchedulerQueueCandidate_t eKind = ( sirSchedulerQueueCandidate_t )state.range( 0 );
	const uint32_t nWorkers = ( uint32_t )state.range( 1 );
	const uint32_t nItems = ( uint32_t )state.range( 2 );
	const uint32_t nWork = ( uint32_t )state.range( 3 );
	const bool bImbalanced = state.range( 4 ) != 0;
	if ( nWorkers == 0 || nWorkers > 8 || nItems < 16 || nItems > 512 ) {
		state.SkipWithError( "invalid worker experiment" );
		return;
	}
	SIRSchedulerQueueCandidate queue;
	if ( !queue.init( eKind, nWorkers, 1024 ) ) {
		state.SkipWithError( "queue candidate initialization failed" );
		return;
	}
	std::unique_ptr<sirQueueBenchmarkWorker_t[]> workerState( new sirQueueBenchmarkWorker_t[nWorkers] );
	std::atomic<uint64_t> nEpoch( 0 );
	std::atomic<uint32_t> nPrepared( 0 );
	std::atomic<uint32_t> nFinished( 0 );
	std::atomic<bool> bStop( false );
	std::atomic<bool> bRejected( false );
	std::vector<std::thread> workers;
	workers.reserve( nWorkers );
	uint32_t nExpectedTotal = 0;
	uint64_t nExpectedChecksum = 0;
	for ( uint32_t i = 0; i < nWorkers; ++i ) {
		const uint32_t nCount = bImbalanced ? ( i == 0 ? nItems * 2 : nItems / 2 ) : nItems;
		nExpectedTotal += nCount;
		for ( uint32_t k = 0; k < nCount; ++k ) {
			uint64_t nPayload = ( ( uint64_t )( i + 1 ) << 32 ) | ( k + 1 );
			for ( uint32_t n = 0; n < nWork; ++n ) {
				nPayload = sirQueueBenchmarkMix( nPayload );
			}
			nExpectedChecksum ^= nPayload;
		}
	}
	for ( uint32_t i = 0; i < nWorkers; ++i ) {
		workers.emplace_back( [&, i]() {
			uint64_t nPreviousEpoch = 0;
			for ( ;; ) {
				uint64_t nCurrentEpoch;
				while ( ( nCurrentEpoch = nEpoch.load( std::memory_order_acquire ) ) == nPreviousEpoch &&
				        !bStop.load( std::memory_order_acquire ) ) {
					std::this_thread::yield();
				}
				if ( bStop.load( std::memory_order_acquire ) ) {
					return;
				}
				nPreviousEpoch = nCurrentEpoch;
				const uint32_t nCount = bImbalanced ? ( i == 0 ? nItems * 2 : nItems / 2 ) : nItems;
				for ( uint32_t k = 0; k < nCount; ++k ) {
					const uint64_t nToken = ( ( uint64_t )( i + 1 ) << 32 ) | ( k + 1 );
					if ( !queue.push( i, nToken ) ) {
						bRejected.store( true, std::memory_order_release );
						break;
					}
				}
				nPrepared.fetch_add( 1, std::memory_order_acq_rel );
				while ( nPrepared.load( std::memory_order_acquire ) != nWorkers ) {
					std::this_thread::yield();
				}
				uint32_t nSteals = 0;
				uint64_t nChecksum = 0;
				while ( !bRejected.load( std::memory_order_acquire ) ) {
					uint64_t nToken = 0;
					bool bGot = queue.pop( i, &nToken );
					if ( !bGot ) {
						for ( uint32_t j = 1; j < nWorkers; ++j ) {
							const uint32_t nVictim = ( i + j ) % nWorkers;
							if ( queue.steal( i, nVictim, &nToken ) ) {
								bGot = true;
								++nSteals;
								break;
							}
						}
					}
					if ( bGot ) {
						uint64_t nValue = nToken;
						for ( uint32_t n = 0; n < nWork; ++n ) {
							nValue = sirQueueBenchmarkMix( nValue );
						}
						nChecksum ^= nValue;
						workerState[i].nCompleted.fetch_add( 1, std::memory_order_release );
					} else {
						uint32_t nTotal = 0;
						for ( uint32_t j = 0; j < nWorkers; ++j ) {
							nTotal += workerState[j].nCompleted.load( std::memory_order_acquire );
						}
						if ( nTotal >= nExpectedTotal ) {
							break;
						}
						std::this_thread::yield();
					}
				}
				workerState[i].nChecksum = nChecksum;
				workerState[i].nSteals = nSteals;
				nFinished.fetch_add( 1, std::memory_order_release );
			}
		} );
	}
	uint64_t nTotalSteals = 0;
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		nPrepared.store( 0, std::memory_order_relaxed );
		nFinished.store( 0, std::memory_order_relaxed );
		for ( uint32_t i = 0; i < nWorkers; ++i ) {
			workerState[i].nCompleted.store( 0, std::memory_order_relaxed );
		}
		nEpoch.fetch_add( 1, std::memory_order_release );
		while ( nFinished.load( std::memory_order_acquire ) != nWorkers ) {
			std::this_thread::yield();
		}
		if ( bRejected.load( std::memory_order_acquire ) ) {
			bFailed = true;
			state.SkipWithError( "candidate queue rejected reserved workload" );
			break;
		}
		uint32_t nTotal = 0;
		uint64_t nChecksum = 0;
		for ( uint32_t i = 0; i < nWorkers; ++i ) {
			nTotal += workerState[i].nCompleted.load( std::memory_order_relaxed );
			nChecksum ^= workerState[i].nChecksum;
			nTotalSteals += workerState[i].nSteals;
		}
		if ( nTotal != nExpectedTotal || nChecksum != nExpectedChecksum ) {
			bFailed = true;
			state.SkipWithError( "candidate queue lost, duplicated, or corrupted work" );
			break;
		}
	}
	bStop.store( true, std::memory_order_release );
	nEpoch.fetch_add( 1, std::memory_order_release );
	for ( std::thread &worker : workers ) {
		worker.join();
	}
	if ( !bFailed ) {
		state.SetItemsProcessed( state.iterations() * nExpectedTotal );
		state.counters["steals"] = ( double )nTotalSteals;
		state.counters["queue_kind"] = ( double )eKind;
	}
}

BENCHMARK( SIRBenchmarkQueueCandidate )
    ->Args( { 0, 1, 256, 0, 0 } )
    ->Args( { 1, 1, 256, 0, 0 } )
    ->Args( { 2, 1, 256, 0, 0 } )
    ->Args( { 0, 2, 256, 0, 0 } )
    ->Args( { 1, 2, 256, 0, 0 } )
    ->Args( { 2, 2, 256, 0, 0 } )
    ->Args( { 0, 4, 256, 0, 0 } )
    ->Args( { 1, 4, 256, 0, 0 } )
    ->Args( { 2, 4, 256, 0, 0 } )
    ->Args( { 0, 8, 256, 0, 0 } )
    ->Args( { 1, 8, 256, 0, 0 } )
    ->Args( { 2, 8, 256, 0, 0 } )
    ->Args( { 0, 4, 256, 64, 0 } )
    ->Args( { 1, 4, 256, 64, 0 } )
    ->Args( { 2, 4, 256, 64, 0 } )
    ->Args( { 0, 4, 256, 0, 1 } )
    ->Args( { 1, 4, 256, 0, 1 } )
    ->Args( { 2, 4, 256, 0, 1 } );
