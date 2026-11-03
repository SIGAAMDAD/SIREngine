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

#include "SIRAllocatorTestCommon.h"

#include <Core/Memory/Allocators/SIRSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRReclaimableSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/Allocators/SIRThreadLocalAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRTLSFAllocator.h>
#include <Core/Memory/Allocators/SIRPoolAllocator.h>
#include <Core/Memory/SIRAllocationRouter.h>
#include <Core/Memory/SIRMemoryPressureCoordinator.h>
#include <Core/Memory/SIRGeneralHeap.h>

#include <gtest/gtest.h>
#include <atomic>
#include <thread>

TEST( SIRAllocatorIntegration, SizeClassBoundariesAndInvalidSizes )
{
	for ( uint32_t nBytes = 1; nBytes <= 2048; ++nBytes ) {
		const uint32_t nClass = SIRSizeClassAllocator::getClassIndex( nBytes );
		if ( nClass < SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT ) {
			EXPECT_GE( SIRSizeClassAllocator::getClassBytes( nClass ), nBytes );
		}
		const uint32_t nFineClass = SIRSizeClassAllocator::getFineClassIndex( nBytes );
		if ( nFineClass < SIRSizeClassAllocator::SIR_FINE_SIZE_CLASS_COUNT ) {
			EXPECT_GE( SIRSizeClassAllocator::getFineClassBytes( nFineClass ), nBytes );
		}
	}
	EXPECT_GE( SIRSizeClassAllocator::getClassIndex( UINT32_MAX ), SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT );
}

TEST( SIRAllocatorIntegration, SizeClassesExhaustEveryClassAndRecover )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *segments[SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT] = {};
	for ( uint32_t i = 0; i < SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT; ++i ) {
		segments[i] = memory.segment( "sizeclass", 16 );
		ASSERT_NE( segments[i], nullptr );
	}
	SIRSizeClassAllocator allocator;
	ASSERT_TRUE( allocator.init( segments ) );
	for ( uint32_t i = 0; i < SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT; ++i ) {
		const uint32_t nBytes = SIRSizeClassAllocator::getClassBytes( i );
		std::vector<void *> live;
		for ( uint32_t j = 0; j < 8192; ++j ) {
			void *pMemory = allocator.alloc( nBytes );
			if ( pMemory == nullptr ) {
				break;
			}
			memset( pMemory, ( int )( i + 1 ), nBytes );
			live.push_back( pMemory );
		}
		ASSERT_GT( live.size(), 0u );
		EXPECT_EQ( allocator.alloc( nBytes ), nullptr );
		for ( void *pMemory : live ) {
			ASSERT_EQ( *( ( uint8_t * )pMemory ), i + 1 );
			ASSERT_TRUE( allocator.free( pMemory, nBytes ) );
		}
		void *pAgain = allocator.alloc( nBytes );
		ASSERT_NE( pAgain, nullptr );
		ASSERT_TRUE( allocator.free( pAgain, nBytes ) );
	}
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorIntegration, ReclaimableSizeClassesTrackBytesAndReleaseSlabs )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *segments[SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT] = {};
	for ( uint32_t i = 0; i < SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT; ++i ) {
		segments[i] = memory.segment( "reclaim", 32 );
		ASSERT_NE( segments[i], nullptr );
	}
	SIRReclaimableSizeClassAllocator allocator;
	ASSERT_TRUE( allocator.init( segments ) );
	void *pA = allocator.alloc( 33, 16 );
	void *pB = allocator.alloc( 99, 16 );
	ASSERT_NE( pA, nullptr );
	ASSERT_NE( pB, nullptr );
	EXPECT_EQ( allocator.getLiveRequestedBytes(), 132u );
	EXPECT_TRUE( allocator.free( pA, 33 ) );
	EXPECT_TRUE( allocator.free( pB, 99 ) );
	EXPECT_EQ( allocator.getLiveRequestedBytes(), 0u );
	EXPECT_TRUE( allocator.setWarmSlabs( 0, 0 ) );
	EXPECT_TRUE( allocator.trimTo( 0 ) );
	for ( SIRMemorySegment *pSegment : segments ) {
		EXPECT_EQ( pSegment->getCommittedBytes(), 0u );
	}
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorIntegration, CompactBucketOwnerAndRemoteFree )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "bucket", 256 );
	ASSERT_NE( pSegment, nullptr );
	const sirCompactBucketConfig_t config = { 256, 16, 16384, 2, 1, 0 };
	SIRCompactBucketAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, config ) );
	ASSERT_TRUE( allocator.attach() );
	std::vector<void *> live;
	for ( uint32_t i = 0; i < 400; ++i ) {
		void *pMemory = allocator.alloc( 40, 16 );
		ASSERT_NE( pMemory, nullptr );
		memset( pMemory, 0x4f, 40 );
		live.push_back( pMemory );
	}
	std::atomic<uint32_t> nFreed{ 0 };
	std::thread worker( [&allocator, &live, &nFreed]() {
		for ( void *pMemory : live ) {
			if ( allocator.free( pMemory ) ) {
				nFreed.fetch_add( 1, std::memory_order_relaxed );
			}
		}
	} );
	worker.join();
	EXPECT_EQ( nFreed.load(), ( uint32_t )live.size() );
	for ( uint32_t i = 0; i < 20; ++i ) {
		allocator.drainRemote();
		allocator.servicePending( 1024 );
		if ( allocator.getLiveCount() == 0 ) {
			break;
		}
	}
	EXPECT_EQ( allocator.getLiveCount(), 0u );
	EXPECT_TRUE( allocator.detach() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorIntegration, ThreadLocalSeatRejectsStaleRemoteToken )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *segments[SIRThreadLocalAllocator::SIR_CLASSES] = {};
	for ( uint32_t i = 0; i < SIRThreadLocalAllocator::SIR_CLASSES; ++i ) {
		segments[i] = memory.segment( "seat", 32 );
		ASSERT_NE( segments[i], nullptr );
	}
	SIRThreadLocalAllocator allocator;
	ASSERT_TRUE( allocator.init( &memory.manager(), segments, 1, 128 ) );
	EXPECT_EQ( allocator.alloc( 48 ), nullptr );
	ASSERT_TRUE( allocator.attachThread( 0 ) );
	EXPECT_FALSE( allocator.attachThread( 0 ) );
	const sirThreadSeatToken_t oldToken = allocator.getSeatToken();
	void *pMemory = allocator.alloc( 48, 16 );
	ASSERT_NE( pMemory, nullptr );
	EXPECT_FALSE( allocator.detachThread() );
	ASSERT_TRUE( allocator.free( pMemory, 48, 16 ) );
	ASSERT_TRUE( allocator.detachThread() );
	ASSERT_TRUE( allocator.attachThread( 0 ) );
	const sirThreadSeatToken_t newToken = allocator.getSeatToken();
	EXPECT_NE( oldToken.nGeneration, newToken.nGeneration );
	pMemory = allocator.alloc( 48, 16 );
	ASSERT_NE( pMemory, nullptr );
	EXPECT_FALSE( allocator.queueRemoteFree( oldToken, pMemory, 48, 16 ) );
	ASSERT_TRUE( allocator.free( pMemory, 48, 16 ) );
	ASSERT_TRUE( allocator.detachThread() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorIntegration, ThreadLocalRemoteProducerAndOwnerDrain )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *segments[SIRThreadLocalAllocator::SIR_CLASSES] = {};
	for ( uint32_t i = 0; i < SIRThreadLocalAllocator::SIR_CLASSES; ++i ) {
		segments[i] = memory.segment( "remote", 32 );
		ASSERT_NE( segments[i], nullptr );
	}
	SIRThreadLocalAllocator allocator;
	ASSERT_TRUE( allocator.init( &memory.manager(), segments, 1, 128 ) );
	ASSERT_TRUE( allocator.attachThread( 0 ) );
	const sirThreadSeatToken_t token = allocator.getSeatToken();
	std::vector<void *> live;
	for ( uint32_t i = 0; i < 64; ++i ) {
		void *pMemory = allocator.alloc( 64, 16 );
		ASSERT_NE( pMemory, nullptr );
		live.push_back( pMemory );
	}
	std::atomic<uint32_t> nAccepted{ 0 };
	std::thread producer( [&allocator, &live, &token, &nAccepted]() {
		for ( void *pMemory : live ) {
			if ( allocator.queueRemoteFree( token, pMemory, 64, 16 ) ) {
				nAccepted.fetch_add( 1, std::memory_order_relaxed );
			}
		}
	} );
	producer.join();
	EXPECT_EQ( nAccepted.load(), ( uint32_t )live.size() );
	EXPECT_EQ( allocator.getStats().nRemoteAccepted, ( uint64_t )live.size() );
	EXPECT_GE( allocator.drainRemote(), ( uint32_t )live.size() );
	EXPECT_EQ( allocator.getStats().nLivePayloadBytes, 0u );
	EXPECT_TRUE( allocator.detachThread() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorIntegration, PressureReclaimsCommittedPoolPages )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "pressure", 64 );
	ASSERT_NE( pSegment, nullptr );
	SIRPoolAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 64 ) );
	void *pMemory = allocator.alloc();
	ASSERT_NE( pMemory, nullptr );
	ASSERT_TRUE( allocator.free( pMemory ) );
	ASSERT_GT( pSegment->getCommittedBytes(), 0u );
	SIRMemoryPressureCoordinator coordinator;
	ASSERT_TRUE( coordinator.init( &memory.manager(), 2 ) );
	SIRMemorySegment *segments[1] = { pSegment };
	const sirReclaimOwnerConfig_t config = { &allocator, segments, 1, SIR_RECLAIM_POOL, 0, 0, 0, 0 };
	uint32_t nOwner = UINT32_MAX;
	ASSERT_TRUE( coordinator.registerOwner( config, &nOwner ) );
	EXPECT_FALSE( coordinator.registerOwner( config, &nOwner ) );
	ASSERT_TRUE( coordinator.requestReclaim( memory.page() ) );
	EXPECT_EQ( coordinator.findNextPendingOwner(), nOwner );
	EXPECT_GT( coordinator.serviceOwner( nOwner ), 0u );
	EXPECT_EQ( pSegment->getCommittedBytes(), 0u );
	EXPECT_EQ( coordinator.getStats().nPendingOwners, 0u );
	EXPECT_TRUE( coordinator.shutdown() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorIntegration, RouterChoosesSizedBackendsAndRejectsDoubleRelease )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *classSegments[SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT] = {};
	for ( uint32_t i = 0; i < SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT; ++i ) {
		classSegments[i] = memory.segment( "router.class", 16 );
		ASSERT_NE( classSegments[i], nullptr );
	}
	SIRMemorySegment *pMedium = memory.segment( "router.medium", 64 );
	SIRMemorySegment *pLarge = memory.segment( "router.large", 128 );
	ASSERT_NE( pMedium, nullptr );
	ASSERT_NE( pLarge, nullptr );
	SIRSizeClassAllocator classes;
	SIRFreeListAllocator medium;
	SIRBuddyAllocator large;
	ASSERT_TRUE( classes.init( classSegments ) );
	ASSERT_TRUE( medium.init( pMedium ) );
	ASSERT_TRUE( large.init( pLarge ) );
	const sirAllocationRouterConfig_t config = { &classes, nullptr, &medium, nullptr, &large, nullptr, nullptr, 128, 2048, false, false };
	SIRAllocationRouter router;
	ASSERT_TRUE( router.init( config ) );
	sirRoutedAllocation_t small = {};
	sirRoutedAllocation_t mid = {};
	sirRoutedAllocation_t big = {};
	ASSERT_TRUE( router.alloc( 80, 16, &small ) );
	ASSERT_TRUE( router.alloc( 400, 32, &mid ) );
	ASSERT_TRUE( router.alloc( 5000, 64, &big ) );
	EXPECT_EQ( small.nBacking, SIR_ROUTE_POOL );
	EXPECT_EQ( mid.nBacking, SIR_ROUTE_FREE_LIST );
	EXPECT_EQ( big.nBacking, SIR_ROUTE_BUDDY );
	EXPECT_EQ( router.getRoutedCount( SIR_ROUTE_POOL ), 1u );
	EXPECT_EQ( router.getRoutedCount( SIR_ROUTE_FREE_LIST ), 1u );
	EXPECT_EQ( router.getRoutedCount( SIR_ROUTE_BUDDY ), 1u );
	EXPECT_TRUE( router.free( &small ) );
	EXPECT_FALSE( router.free( &small ) );
	EXPECT_TRUE( router.free( &mid ) );
	EXPECT_TRUE( router.free( &big ) );
	EXPECT_TRUE( classes.shutdown() );
	EXPECT_TRUE( medium.shutdown() );
}

TEST( SIRAllocatorIntegration, FineSizeClassesPreserveAllClassBoundaries )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *segments[SIRSizeClassAllocator::SIR_FINE_SIZE_CLASS_COUNT] = {};
	for ( uint32_t i = 0; i < SIRSizeClassAllocator::SIR_FINE_SIZE_CLASS_COUNT; ++i ) {
		segments[i] = memory.segment( "fine", 16 );
		ASSERT_NE( segments[i], nullptr );
	}
	SIRSizeClassAllocator allocator;
	ASSERT_TRUE( allocator.initFine( segments ) );
	EXPECT_EQ( allocator.getActiveClassCount(), SIRSizeClassAllocator::SIR_FINE_SIZE_CLASS_COUNT );
	for ( uint32_t i = 0; i < SIRSizeClassAllocator::SIR_FINE_SIZE_CLASS_COUNT; ++i ) {
		const uint32_t nSize = SIRSizeClassAllocator::getFineClassBytes( i );
		void *pMemory = allocator.alloc( nSize );
		ASSERT_NE( pMemory, nullptr );
		memset( pMemory, 0x39, nSize );
		EXPECT_TRUE( allocator.free( pMemory, nSize ) );
	}
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorIntegration, ThreadLocalIndependentWorkerSeats )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *segments[2 * SIRThreadLocalAllocator::SIR_CLASSES] = {};
	for ( uint32_t i = 0; i < 2 * SIRThreadLocalAllocator::SIR_CLASSES; ++i ) {
		segments[i] = memory.segment( "worker.seat", 32 );
		ASSERT_NE( segments[i], nullptr );
	}
	SIRThreadLocalAllocator allocator;
	ASSERT_TRUE( allocator.init( &memory.manager(), segments, 2, 128 ) );
	std::atomic<uint32_t> nSucceeded{ 0 };
	std::thread workers[2];
	for ( uint32_t i = 0; i < 2; ++i ) {
		workers[i] = std::thread( [&allocator, &nSucceeded, i]() {
			if ( !allocator.attachThread( i ) ) {
				return;
			}
			std::vector<void *> live;
			for ( uint32_t j = 0; j < 128; ++j ) {
				void *pMemory = allocator.alloc( 32 + ( i * 16 ), 16 );
				if ( pMemory == nullptr ) {
					break;
				}
				memset( pMemory, ( int )( i + 1 ), 32 + i * 16 );
				live.push_back( pMemory );
			}
			if ( live.size() == 128 ) {
				bool bValid = true;
				for ( void *pMemory : live ) {
					bValid &= ( *( ( uint8_t * )pMemory ) == i + 1 );
					bValid &= allocator.free( pMemory, 32 + i * 16 );
				}
				if ( bValid && allocator.detachThread() ) {
					nSucceeded.fetch_add( 1, std::memory_order_relaxed );
				}
			}
		} );
	}
	for ( std::thread &worker : workers ) {
		worker.join();
	}
	EXPECT_EQ( nSucceeded.load(), 2u );
	EXPECT_EQ( allocator.getStats().nActiveSeats, 0u );
	EXPECT_TRUE( allocator.shutdown() );
}
