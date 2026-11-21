/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Resources/SIRResourceGraph.h>
#include <Resources/SIRResourceGraphSync.h>
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#endif
#include <thread>
#include <type_traits>

// Canonical SIRResourceReadScope already owns reader-lane lifetime.

static bool testWriterAuthority( void ) noexcept
{
	SIRResourceGraphSync sync;
	std::atomic<uint64_t> szEpochs[4] = {};
	sync.configure( szEpochs, 4 );

	if ( !sync.isWriterThread() ) {
		return false;
	}
	{
		SIRResourceGraphWriteGuard guard( sync );
		SIRResourceGraphWriteGuard nested( sync );
		if ( !guard.isActive() || !sync.isWriteActive() || nested.isActive() ) {
			return false;
		}
	}
	if ( sync.isWriteActive() ) {
		return false;
	}

	std::atomic<bool> bForeignRejected{ false };
	std::thread foreign( [&sync, &bForeignRejected]() {
		SIRResourceGraphWriteGuard guard( sync );
		bForeignRejected.store( !guard.isActive() && !sync.isWriterThread(), std::memory_order_relaxed );
	} );
	foreign.join();
	if ( !bForeignRejected.load( std::memory_order_relaxed ) ) {
		return false;
	}

	SIRResourceGraphWriteGuard newGuard( sync );
	return newGuard.isActive();
}

static bool testReaderLanes( void ) noexcept
{
	SIRResourceGraphSync sync;
	std::atomic<uint64_t> szEpochs[2] = {};
	sync.configure( szEpochs, 2 );

	const uint64_t nFirst = sync.registerLane();
	const uint64_t nSecond = sync.registerLane();
	if ( nFirst == 0 || nSecond == 0 || nFirst == nSecond || sync.registerLane() != 0 || sync.enterRead() != 0 ) {
		return false;
	}

	const uint64_t nActive = sync.enterRegisteredLane( nFirst );
	if ( nActive == 0 || sync.enterRegisteredLane( nFirst ) != 0 || sync.countActiveReaders() != 1 ) {
		return false;
	}
	{
		SIRResourceGraphWriteGuard guard( sync );
		if ( !guard.isActive() ) {
			return false;
		}
		const uint64_t nBefore = sync.currentEpoch();
		sync.advanceEpoch( nBefore );
		if ( sync.currentEpoch() != nBefore + 1 || sync.oldestProtectedEpoch() != nBefore ) {
			return false;
		}
	}

	sync.leaveRead( nActive );
	if ( sync.countActiveReaders() != 0 ) {
		return false;
	}
	sync.releaseLane( nFirst );
	const uint64_t nOrdinary = sync.enterRead();
	if ( nOrdinary == 0 || sync.countActiveReaders() != 1 ) {
		return false;
	}
	sync.leaveRead( nOrdinary );
	sync.releaseLane( nSecond );
	return sync.readersQuiescent() && sync.oldestProtectedEpoch() == UINT64_MAX;
}

static bool testPinMutex( void ) noexcept
{
	SIRResourceGraphPinMutex pinMutex;
	if ( !pinMutex.try_lock() ) {
		return false;
	}

	std::atomic<bool> bBlocked{ false };
	std::thread worker( [&pinMutex, &bBlocked]() { bBlocked.store( !pinMutex.try_lock(), std::memory_order_release ); } );
	worker.join();
	pinMutex.unlock();
	if ( !bBlocked.load( std::memory_order_acquire ) ) {
		return false;
	}
	if ( !pinMutex.try_lock() ) {
		return false;
	}
	pinMutex.unlock();
	return true;
}

struct SIRReentryAllocator
{
	SIRResourceGraphWriter *m_pWriter = nullptr;
	bool m_bTrigger = false;
	bool m_bObserved = false;
	SIRResourceResult m_eNestedResult = SIR_RESOURCE_RESULT_SUCCESS;
};

static void *reentryAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nDomain;
	SIRReentryAllocator *pAllocator = ( SIRReentryAllocator * )pContext;
	if ( pAllocator->m_bTrigger ) {
		pAllocator->m_bTrigger = false;
		SIRResourceHandle hNested = {};
		const SIRResourceKey nestedKey = { 5555, 2, 0 };
		pAllocator->m_eNestedResult = pAllocator->m_pWriter->request( nestedKey, &hNested );
		pAllocator->m_bObserved = true;
	}

	void *pMemory = nullptr;
#ifdef _WIN32
	pMemory = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nActualAlignment = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pMemory, nActualAlignment, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	return pMemory;
}

static void reentryRelease( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nBytes;
	( void )nAlignment;
	( void )nDomain;
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static bool testGraphWriterReentry( void ) noexcept
{
	SIRReentryAllocator allocator;
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &allocator, reentryAllocate, reentryRelease };
	config.m_nMaxResources = 4;
	config.m_nMaxStaging = 4;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 65536;

	SIRResourceGraph graph( config );
	if ( !graph.isReady() ) {
		return false;
	}

	allocator.m_pWriter = graph.getWriter();
	allocator.m_bTrigger = true;
	SIRResourceStagingHandle hStaging = {};
	SIRResourceMutableBytes bytes = {};
	const SIRResourceResult eStaged = allocator.m_pWriter->allocateStaging( 8, 8, 3, &hStaging, &bytes );
	if ( eStaged != SIR_RESOURCE_RESULT_SUCCESS || !allocator.m_bObserved ) {
		return false;
	}
	if ( allocator.m_eNestedResult != SIR_RESOURCE_RESULT_WRITER_VIOLATION ) {
		return false;
	}
	return allocator.m_pWriter->cancelStaging( hStaging ) == SIR_RESOURCE_RESULT_SUCCESS;
}

static bool testConcurrentPublication( void ) noexcept
{
	SIRResourceGraphSync sync;
	std::atomic<uint64_t> szEpochs[4] = {};
	sync.configure( szEpochs, 4 );

	std::atomic<uint32_t> nEntered{ 0 };
	std::atomic<bool> bRelease{ false };
	std::atomic<bool> bSuccess{ true };
	std::thread szWorkers[3];
	for ( uint32_t iWorker = 0; iWorker < 3; ++iWorker ) {
		szWorkers[iWorker] = std::thread( [&sync, &nEntered, &bRelease, &bSuccess]() {
			const uint64_t nToken = sync.enterRead();
			if ( nToken == 0 ) {
				bSuccess.store( false, std::memory_order_relaxed );
				nEntered.fetch_add( 1, std::memory_order_release );
				return;
			}
			nEntered.fetch_add( 1, std::memory_order_release );
			while ( !bRelease.load( std::memory_order_acquire ) ) {
				std::this_thread::yield();
			}
			sync.leaveRead( nToken );
		} );
	}
	while ( nEntered.load( std::memory_order_acquire ) != 3 ) {
		std::this_thread::yield();
	}

	bool bValid = sync.countActiveReaders() == 3 && sync.oldestProtectedEpoch() == 1;
	{
		SIRResourceGraphWriteGuard guard( sync );
		if ( !guard.isActive() ) {
			bValid = false;
		} else {
			sync.advanceEpoch( 1 );
			bValid = bValid && sync.oldestProtectedEpoch() == 1 && sync.currentEpoch() == 2;
		}
	}
	bRelease.store( true, std::memory_order_release );
	for ( uint32_t iWorker = 0; iWorker < 3; ++iWorker ) {
		szWorkers[iWorker].join();
	}
	return bValid && bSuccess.load( std::memory_order_relaxed ) && sync.readersQuiescent();
}

int main( void )
{
	if ( !testWriterAuthority() || !testReaderLanes() || !testPinMutex() || !testGraphWriterReentry() || !testConcurrentPublication() ) {
		fprintf( stderr, "FAIL: GRDG specialized synchronization contracts\n" );
		return 1;
	}
	printf( "PASS: GRDG specialized read guards, single writer and epoch publication\n" );
	return 0;
}
