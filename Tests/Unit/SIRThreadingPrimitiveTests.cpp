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

#include <atomic>
#include <Core/Platform/Shared/Threading/SIRScopedLock.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>

#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <type_traits>

typedef enum sirTestPhase_e : uint8_t
{
	SIR_TEST_PHASE_IDLE = 0,
	SIR_TEST_PHASE_RUNNING = 1
} sirTestPhase_t;

typedef struct sirThreadIdentityProbe_s
{
	sirThreadId_t hOwner;
	std::atomic<bool> bCrossThreadValidated;
} sirThreadIdentityProbe_t;

static void sirCheckThreadIdentity( void *pContext )
{
	sirThreadIdentityProbe_t *pProbe = ( sirThreadIdentityProbe_t * )pContext;
	pProbe->bCrossThreadValidated.store( !SIRThread::isCurrentThread( pProbe->hOwner ), std::memory_order_release );
}

static bool testEngineThreadIdentity( void )
{
	const sirThreadId_t hOwner = SIRThread::currentThreadId();
	if ( !SIRThread::isCurrentThread( hOwner ) ) {
		return false;
	}
	sirThreadIdentityProbe_t probe{};
	probe.hOwner = hOwner;
	probe.bCrossThreadValidated.store( false, std::memory_order_relaxed );
	SIRThread worker;
	if ( !worker.start( &sirCheckThreadIdentity, &probe ) || !worker.join() ) {
		return false;
	}
	return probe.bCrossThreadValidated.load( std::memory_order_acquire ) && SIRThread::isCurrentThread( hOwner );
}

static bool testAtomicOrders( void )
{
	static_assert( std::atomic<uint64_t>::is_always_lock_free, "64-bit lock free atomic is required" );
	static_assert( sizeof( std::atomic<uint64_t> ) == sizeof( uint64_t ), "no wrapper overhead" );
	static_assert( sizeof( std::atomic<uint32_t> ) == sizeof( uint32_t ), "no wrapper overhead" );
	static_assert( !std::is_copy_constructible<std::atomic<uint64_t>>::value, "no atomic copies" );
	static_assert( !std::is_copy_constructible<SIRScopedLock<SIRMutex>>::value, "no guard copies" );

	std::atomic<uint64_t> nState( 5 );
	if ( nState.fetch_add( 3, std::memory_order_acq_rel ) != 5 || nState.load() != 8 ) {
		return false;
	}
	if ( nState.fetch_sub( 2, std::memory_order_acq_rel ) != 8 || nState.load() != 6 ) {
		return false;
	}
	uint64_t nExpected = 3;
	if ( nState.compare_exchange_strong( nExpected, 15, std::memory_order_acq_rel ) || nExpected != 6 ) {
		return false;
	}
	if ( !nState.compare_exchange_strong( nExpected, 15, std::memory_order_acq_rel, std::memory_order_acquire ) ) {
		return false;
	}
	if ( nState.exchange( 11, std::memory_order_release ) != 15 || nState.load( std::memory_order_acquire ) != 11 ) {
		return false;
	}
	if ( ( nState &= 7 ) != 3 || ( nState |= 16 ) != 19 || ( nState ^= 2 ) != 17 ) {
		return false;
	}
	if ( ++nState != 18 || nState++ != 18 || --nState != 18 || nState-- != 18 || nState.load() != 17 ) {
		return false;
	}

	std::atomic<bool> bFlag;
	if ( bFlag.load() || bFlag.exchange( true ) || !bFlag.load() ) {
		return false;
	}
	bFlag.store( false, std::memory_order_release );
	std::atomic<bool> flag;
	if ( flag.load() || flag.exchange( true, std::memory_order_acquire ) || !flag.load() || !flag.exchange( true ) ) {
		return false;
	}
	flag.store( false, std::memory_order_release );
	if ( flag.load() ) {
		return false;
	}

	std::atomic<float> fCount( 1.5f );
	if ( fCount.fetch_add( 2.0f ) != 1.5f || fCount.fetch_sub( 0.5f ) != 3.5f || fCount.load() != 3.0f ) {
		return false;
	}

	int szValues[8]{};
	std::atomic<int *> pValue( szValues );
	if ( pValue.fetch_add( 2 ) != szValues || pValue.load() != szValues + 2 ) {
		return false;
	}
	if ( pValue.fetch_sub( 1 ) != szValues + 2 || pValue.load() != szValues + 1 ) {
		return false;
	}
	if ( ( pValue += 2 ) != szValues + 3 || pValue-- != szValues + 3 || pValue.load() != szValues + 2 ) {
		return false;
	}

	std::atomic<sirTestPhase_t> ePhase( SIR_TEST_PHASE_IDLE );
	ePhase.store( SIR_TEST_PHASE_RUNNING, std::memory_order_release );
	if ( ePhase.load( std::memory_order_acquire ) != SIR_TEST_PHASE_RUNNING ) {
		return false;
	}
	return nState.is_lock_free() && flag.is_lock_free();
}

static bool testScopedLock( void )
{
	SIRMutex mutex;
	{
		SIRScopedLock<SIRMutex> guard( mutex );
		if ( mutex.tryLock() ) {
			mutex.unlock();
			return false;
		}
	}
	if ( !mutex.tryLock() ) {
		return false;
	}
	mutex.unlock();

	{
		SIRConditionalScopedLock<SIRMutex> inactive( mutex, false );
		if ( !mutex.tryLock() ) {
			return false;
		}
		mutex.unlock();
	}
	{
		SIRConditionalScopedLock<SIRMutex> active( mutex, true );
		if ( mutex.tryLock() ) {
			mutex.unlock();
			return false;
		}
	}
	if ( !mutex.tryLock() ) {
		return false;
	}
	mutex.unlock();
	return true;
}

static bool testReleaseAcquirePublication( void )
{
	std::atomic<bool> bPublished( false );
	uint32_t nPayload = 0;
	std::thread producer( [&bPublished, &nPayload]() {
		nPayload = 0xA5A5A5A5u;
		bPublished.store( true, std::memory_order_release );
	} );

	while ( !bPublished.load( std::memory_order_acquire ) ) {
		std::this_thread::yield();
	}
	const bool bValid = nPayload == 0xA5A5A5A5u;
	producer.join();
	return bValid;
}

static bool testConcurrentAtomic( void )
{
	std::atomic<uint32_t> counter( 0 );
	constexpr uint32_t N_THREADS = 6;
	constexpr uint32_t N_INCREMENTS = 50000;
	std::thread workers[N_THREADS];
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		workers[iThread] = std::thread( [&counter, N_INCREMENTS]() {
			for ( uint32_t iIncrement = 0; iIncrement < N_INCREMENTS; ++iIncrement ) {
				counter.fetch_add( 1, std::memory_order_relaxed );
			}
		} );
	}
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		workers[iThread].join();
	}
	return counter.load() == N_THREADS * N_INCREMENTS;
}

static bool testConcurrentSpinLock( void )
{
	SIRSpinLock spin;
	uint32_t nCounter = 0;
	constexpr uint32_t N_THREADS = 5;
	constexpr uint32_t N_INCREMENTS = 30000;
	std::thread workers[N_THREADS];
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		workers[iThread] = std::thread( [&spin, &nCounter, N_INCREMENTS]() {
			for ( uint32_t iIncrement = 0; iIncrement < N_INCREMENTS; ++iIncrement ) {
				SIRScopedLock<SIRSpinLock> guard( spin );
				++nCounter;
			}
		} );
	}
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		workers[iThread].join();
	}
	return nCounter == N_THREADS * N_INCREMENTS;
}

int main( void )
{
	if ( !testEngineThreadIdentity() || !testAtomicOrders() || !testScopedLock() || !testReleaseAcquirePublication() ||
	     !testConcurrentAtomic() || !testConcurrentSpinLock() ) {
		fputs( "SIRThreadingPrimitiveTests failed\n", stderr );
		return 1;
	}
	return 0;
}
