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


#include <Core/Platform/SIRAtomic.h>
#include <Core/Platform/SIRScopedLock.h>
#include <Core/Platform/SIRSync.h>
#include <Core/Platform/SIRThread.h>

#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <type_traits>

typedef enum sirTestPhase_e : uint8_t
{
	SIR_TEST_PHASE_IDLE = 0,
	SIR_TEST_PHASE_RUNNING = 1
} sirTestPhase_t;

static bool testAtomicOrders( void )
{
	static_assert( SIRAtomic<uint64_t>::isAlwaysLockFree(), "64-bit lock free atomic is required" );
	static_assert( sizeof( SIRAtomic<uint64_t> ) == sizeof( uint64_t ), "no wrapper overhead" );
	static_assert( sizeof( SIRAtomic<uint32_t> ) == sizeof( uint32_t ), "no wrapper overhead" );
	static_assert( !std::is_copy_constructible<SIRAtomic<uint64_t>>::value, "no atomic copies" );
	static_assert( !std::is_copy_constructible<SIRScopedLock<SIRMutex>>::value, "no guard copies" );

	SIRAtomic<uint64_t> nState( 5 );
	if ( nState.fetchAdd( 3, SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) != 5 || nState.load() != 8 ) {
		return false;
	}
	if ( nState.fetchSub( 2, SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) != 8 || nState.load() != 6 ) {
		return false;
	}
	uint64_t nExpected = 3;
	if ( nState.compareExchangeStrong( nExpected, 15, SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) || nExpected != 6 ) {
		return false;
	}
	if ( !nState.compareExchangeStrong( nExpected, 15, SIR_MEMORY_ORDER_ACQUIRE_RELEASE, SIR_MEMORY_ORDER_ACQUIRE ) ) {
		return false;
	}
	if ( nState.exchange( 11, SIR_MEMORY_ORDER_RELEASE ) != 15 || nState.load( SIR_MEMORY_ORDER_ACQUIRE ) != 11 ) {
		return false;
	}
	if ( (nState &= 7) != 3 || (nState |= 16) != 19 || (nState ^= 2) != 17 ) {
		return false;
	}
	if ( ++nState != 18 || nState++ != 18 || --nState != 18 || nState-- != 18 || nState.load() != 17 ) {
		return false;
	}

	SIRAtomic<bool> bFlag;
	if ( bFlag.load() || bFlag.exchange( true ) || !bFlag.load() ) {
		return false;
	}
	bFlag.store( false, SIR_MEMORY_ORDER_RELEASE );
	SIRAtomicFlag flag;
	if ( flag.test() || flag.testAndSet( SIR_MEMORY_ORDER_ACQUIRE ) || !flag.test() || !flag.testAndSet() ) {
		return false;
	}
	flag.clear( SIR_MEMORY_ORDER_RELEASE );
	if ( flag.test() ) {
		return false;
	}

	SIRAtomic<float> fCount( 1.5f );
	if ( fCount.fetchAdd( 2.0f ) != 1.5f || fCount.fetchSub( 0.5f ) != 3.5f || fCount.load() != 3.0f ) {
		return false;
	}

	int szValues[8]{};
	SIRAtomic<int *> pValue( szValues );
	if ( pValue.fetchAdd( 2 ) != szValues || pValue.load() != szValues + 2 ) {
		return false;
	}
	if ( pValue.fetchSub( 1 ) != szValues + 2 || pValue.load() != szValues + 1 ) {
		return false;
	}
	if ( (pValue += 2) != szValues + 3 || pValue-- != szValues + 3 || pValue.load() != szValues + 2 ) {
		return false;
	}

	SIRAtomic<sirTestPhase_t> ePhase( SIR_TEST_PHASE_IDLE );
	ePhase.store( SIR_TEST_PHASE_RUNNING, SIR_MEMORY_ORDER_RELEASE );
	if ( ePhase.load( SIR_MEMORY_ORDER_ACQUIRE ) != SIR_TEST_PHASE_RUNNING ) {
		return false;
	}
	return nState.isLockFree() && flag.isLockFree();
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
	SIRAtomic<bool> bPublished( false );
	uint32_t nPayload = 0;
	std::thread producer( [&bPublished, &nPayload]() {
		nPayload = 0xA5A5A5A5u;
		bPublished.store( true, SIR_MEMORY_ORDER_RELEASE );
	} );

	while ( !bPublished.load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
		std::this_thread::yield();
	}
	const bool bValid = nPayload == 0xA5A5A5A5u;
	producer.join();
	return bValid;
}

static bool testConcurrentAtomic( void )
{
	SIRAtomic<uint32_t> counter( 0 );
	constexpr uint32_t N_THREADS = 6;
	constexpr uint32_t N_INCREMENTS = 50000;
	std::thread workers[N_THREADS];
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		workers[iThread] = std::thread( [&counter]() {
			for ( uint32_t iIncrement = 0; iIncrement < N_INCREMENTS; ++iIncrement ) {
				counter.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
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
		workers[iThread] = std::thread( [&spin, &nCounter]() {
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
	if ( !testAtomicOrders() || !testScopedLock() || !testReleaseAcquirePublication() || !testConcurrentAtomic() || !testConcurrentSpinLock() ) {
		fputs( "SIRThreadingPrimitiveTests failed\n", stderr );
		return 1;
	}
	return 0;
}
