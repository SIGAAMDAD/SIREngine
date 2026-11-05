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

#include <Core/Platform/SIRSemaphore.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			std::fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                    \
			std::abort();                                                                                                                  \
		}                                                                                                                                  \
	} while ( false )

static void testLifecycle( void )
{
	SIRSemaphore semaphore;
	SIR_TEST( !semaphore.isOpen() );
	SIR_TEST( semaphore.wait() == SIR_SEMAPHORE_CLOSED );
	SIR_TEST( semaphore.tryWait() == SIR_SEMAPHORE_CLOSED );
	SIR_TEST( semaphore.post() == SIR_SEMAPHORE_CLOSED );
	SIR_TEST( semaphore.create( UINT32_MAX ) == SIR_SEMAPHORE_INVALID_ARGUMENT );
	SIR_TEST( semaphore.create() == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.isOpen() );
	SIR_TEST( semaphore.create() == SIR_SEMAPHORE_INVALID_ARGUMENT );
	SIR_TEST( semaphore.tryWait() == SIR_SEMAPHORE_WOULD_BLOCK );
	SIR_TEST( semaphore.waitFor( 0 ) == SIR_SEMAPHORE_WOULD_BLOCK );
	SIR_TEST( semaphore.waitFor( 8 ) == SIR_SEMAPHORE_TIMED_OUT );
	SIR_TEST( semaphore.post() == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.waitFor( 100 ) == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.tryWait() == SIR_SEMAPHORE_WOULD_BLOCK );
	SIR_TEST( semaphore.close() == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.close() == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.post() == SIR_SEMAPHORE_CLOSED );
	SIR_TEST( semaphore.create( 2 ) == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.wait() == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.wait() == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.tryWait() == SIR_SEMAPHORE_WOULD_BLOCK );
	SIR_TEST( semaphore.close() == SIR_SEMAPHORE_OK );
}

static void testMultithread( void )
{
	SIRSemaphore semaphore;
	SIR_TEST( semaphore.create( 0 ) == SIR_SEMAPHORE_OK );
	static constexpr uint32_t N_THREADS = 4;
	static constexpr uint32_t N_PER_THREAD = 1024;
	std::atomic<uint32_t> nCompleted{ 0 };
	std::thread workers[N_THREADS];
	for ( uint32_t i = 0; i < N_THREADS; ++i ) {
		workers[i] = std::thread( [&]() {
			for ( uint32_t j = 0; j < N_PER_THREAD; ++j ) {
				SIR_TEST( semaphore.waitFor( 3000 ) == SIR_SEMAPHORE_OK );
				nCompleted.fetch_add( 1, std::memory_order_relaxed );
			}
		} );
	}
	for ( uint32_t i = 0; i < N_THREADS * N_PER_THREAD; ++i ) {
		SIR_TEST( semaphore.post() == SIR_SEMAPHORE_OK );
	}
	for ( auto& worker : workers ) {
		worker.join();
	}
	SIR_TEST( nCompleted.load( std::memory_order_relaxed ) == N_THREADS * N_PER_THREAD );
	SIR_TEST( semaphore.tryWait() == SIR_SEMAPHORE_WOULD_BLOCK );
	SIR_TEST( semaphore.close() == SIR_SEMAPHORE_OK );
}

static void testLimit( void )
{
	SIRSemaphore semaphore;
	SIR_TEST( semaphore.create( INT32_MAX ) == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.post() == SIR_SEMAPHORE_LIMIT_REACHED );
	SIR_TEST( semaphore.tryWait() == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.post() == SIR_SEMAPHORE_OK );
	SIR_TEST( semaphore.close() == SIR_SEMAPHORE_OK );
}

int main( void )
{
	testLifecycle();
	testMultithread();
	testLimit();
	return 0;
}
