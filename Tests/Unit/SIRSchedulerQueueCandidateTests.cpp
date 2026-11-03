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

#include "../Benchmarks/SIRSchedulerQueueCandidates.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#define SIR_QUEUE_TEST( condition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Queue candidate test failure: %s:%d (%s)\n", __FILE__, __LINE__, #condition );                               \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static void sirTestQueueBasic( sirSchedulerQueueCandidate_t eKind )
{
	SIRSchedulerQueueCandidate queue;
	SIR_QUEUE_TEST( !queue.init( eKind, 0, 8 ) );
	SIR_QUEUE_TEST( !queue.init( eKind, 2, 7 ) );
	SIR_QUEUE_TEST( queue.init( eKind, 4, 8 ) );
	SIR_QUEUE_TEST( !queue.init( eKind, 4, 8 ) );
	for ( uint32_t iRound = 0; iRound < 256; ++iRound ) {
		for ( uint32_t i = 0; i < 8; ++i ) {
			SIR_QUEUE_TEST( queue.push( 0, ( ( uint64_t )iRound << 32 ) | ( i + 1 ) ) );
		}
		SIR_QUEUE_TEST( !queue.push( 0, 99 ) );
		SIR_QUEUE_TEST( !queue.push( 4, 99 ) );
		SIR_QUEUE_TEST( !queue.push( 0, 0 ) );
		for ( uint32_t i = 0; i < 8; ++i ) {
			uint64_t nToken = 0;
			if ( iRound & 1 ) {
				SIR_QUEUE_TEST( queue.pop( 0, &nToken ) );
				SIR_QUEUE_TEST( nToken == ( ( ( uint64_t )iRound << 32 ) | ( 8 - i ) ) );
			} else {
				SIR_QUEUE_TEST( queue.steal( 1, 0, &nToken ) );
				SIR_QUEUE_TEST( nToken == ( ( ( uint64_t )iRound << 32 ) | ( i + 1 ) ) );
			}
		}
		uint64_t nToken = 0;
		SIR_QUEUE_TEST( !queue.pop( 0, &nToken ) );
		SIR_QUEUE_TEST( !queue.steal( 1, 0, &nToken ) );
		SIR_QUEUE_TEST( !queue.steal( 0, 0, &nToken ) );
		SIR_QUEUE_TEST( !queue.pop( 1, nullptr ) );
	}
}

static void sirTestQueueLastItemRace( sirSchedulerQueueCandidate_t eKind )
{
	SIRSchedulerQueueCandidate queue;
	SIR_QUEUE_TEST( queue.init( eKind, 2, 8 ) );
	std::atomic<uint32_t> nEpoch( 0 );
	std::atomic<uint32_t> nReady( 0 );
	std::atomic<uint32_t> nDone( 0 );
	std::atomic<uint64_t> nStolen( 0 );
	std::thread thief( [&]() {
		for ( uint32_t i = 1; i <= 1024; ++i ) {
			while ( nEpoch.load( std::memory_order_acquire ) < i ) {
				std::this_thread::yield();
			}
			nReady.store( i, std::memory_order_release );
			uint64_t nToken = 0;
			if ( queue.steal( 1, 0, &nToken ) ) {
				nStolen.store( nToken, std::memory_order_release );
			} else {
				nStolen.store( 0, std::memory_order_release );
			}
			nDone.store( i, std::memory_order_release );
		}
	} );
	for ( uint32_t i = 1; i <= 1024; ++i ) {
		SIR_QUEUE_TEST( queue.push( 0, i ) );
		nEpoch.store( i, std::memory_order_release );
		while ( nReady.load( std::memory_order_acquire ) < i ) {
			std::this_thread::yield();
		}
		uint64_t nLocal = 0;
		const bool bPopped = queue.pop( 0, &nLocal );
		while ( nDone.load( std::memory_order_acquire ) < i ) {
			std::this_thread::yield();
		}
		const uint64_t nRemote = nStolen.load( std::memory_order_acquire );
		SIR_QUEUE_TEST( ( bPopped ? 1 : 0 ) + ( nRemote != 0 ? 1 : 0 ) == 1 );
		SIR_QUEUE_TEST( bPopped ? nLocal == i : nRemote == i );
	}
	thief.join();
}

static void sirTestQueueContended( sirSchedulerQueueCandidate_t eKind )
{
	constexpr uint32_t N_WORKERS = 4;
	constexpr uint32_t N_ITEMS = 1024;
	SIRSchedulerQueueCandidate queue;
	SIR_QUEUE_TEST( queue.init( eKind, N_WORKERS, 1024 ) );
	std::atomic<uint32_t> nReady( 0 );
	std::atomic<uint32_t> nFinished( 0 );
	std::atomic<uint32_t> nConsumed( 0 );
	std::atomic<uint32_t> nSteals( 0 );
	std::atomic<uint8_t> seen[N_ITEMS] = {};
	std::vector<std::thread> workers;
	for ( uint32_t iWorker = 0; iWorker < N_WORKERS; ++iWorker ) {
		workers.emplace_back( [&, iWorker]() {
			const uint32_t nCount = iWorker == 0 ? 640 : 128;
			for ( uint32_t i = 0; i < nCount; ++i ) {
				const uint64_t nToken = ( ( uint64_t )iWorker << 32 ) | ( i + 1 );
				SIR_QUEUE_TEST( queue.push( iWorker, nToken ) );
			}
			nReady.fetch_add( 1, std::memory_order_acq_rel );
			while ( nReady.load( std::memory_order_acquire ) != N_WORKERS ) {
				std::this_thread::yield();
			}
			// Delay the heavily loaded owner until another worker proves it
			// can claim a task. This makes the steal assertion deterministic.
			if ( iWorker == 0 ) {
				while ( nSteals.load( std::memory_order_acquire ) == 0 ) {
					std::this_thread::yield();
				}
			}
			while ( nConsumed.load( std::memory_order_acquire ) < N_ITEMS ) {
				uint64_t nToken = 0;
				bool bGot = queue.pop( iWorker, &nToken );
				if ( !bGot ) {
					for ( uint32_t j = 1; j < N_WORKERS; ++j ) {
						const uint32_t nVictim = ( iWorker + j ) % N_WORKERS;
						if ( queue.steal( iWorker, nVictim, &nToken ) ) {
							bGot = true;
							nSteals.fetch_add( 1, std::memory_order_relaxed );
							break;
						}
					}
				}
				if ( bGot ) {
					const uint32_t nOwner = ( uint32_t )( nToken >> 32 );
					const uint32_t nIndex = ( uint32_t )( nToken & UINT32_MAX );
					SIR_QUEUE_TEST( nOwner < N_WORKERS && nIndex != 0 && nIndex <= ( nOwner == 0 ? 640 : 128 ) );
					const uint32_t nSeenIndex = ( nOwner == 0 ? 0 : 640 + ( nOwner - 1 ) * 128 ) + nIndex - 1;
					SIR_QUEUE_TEST( seen[nSeenIndex].fetch_add( 1, std::memory_order_acq_rel ) == 0 );
					nConsumed.fetch_add( 1, std::memory_order_release );
				} else {
					std::this_thread::yield();
				}
			}
			nFinished.fetch_add( 1, std::memory_order_acq_rel );
		} );
	}
	for ( std::thread &worker : workers ) {
		worker.join();
	}
	SIR_QUEUE_TEST( nFinished.load( std::memory_order_acquire ) == N_WORKERS );
	SIR_QUEUE_TEST( nConsumed.load( std::memory_order_acquire ) == N_ITEMS );
	SIR_QUEUE_TEST( nSteals.load( std::memory_order_relaxed ) > 0 );
	for ( uint32_t i = 0; i < N_ITEMS; ++i ) {
		SIR_QUEUE_TEST( seen[i].load( std::memory_order_relaxed ) == 1 );
	}
}

int main( void )
{
	for ( uint32_t i = 0; i < 3; ++i ) {
		const sirSchedulerQueueCandidate_t eKind = ( sirSchedulerQueueCandidate_t )i;
		sirTestQueueBasic( eKind );
		sirTestQueueLastItemRace( eKind );
		for ( uint32_t run = 0; run < 8; ++run ) {
			sirTestQueueContended( eKind );
		}
	}
	puts( "Scheduler queue candidates: all correctness checks passed" );
	return 0;
}
