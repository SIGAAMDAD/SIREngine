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

#include <Core/Memory/Allocators/SIRLinearAllocator.h>
#include <Core/Memory/Allocators/SIRPoolAllocator.h>
#include <Core/Memory/Allocators/SIRBitmapAllocator.h>
#include <Core/Memory/Allocators/SIRSlabAllocator.h>
#include <Core/Memory/Allocators/SIRTLSFAllocator.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/Allocators/SIRPageRunAllocator.h>
#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/Allocators/SIRReclaimableSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRSizeClassAllocator.h>
#include <Core/Memory/SIRGeneralHeap.h>
#include <Core/Memory/SIRAllocationRouter.h>
#include <Core/Memory/SIRMemoryPressureCoordinator.h>
#include <Core/Memory/SIRMemoryColdPageStager.h>

#include <gtest/gtest.h>
#include <atomic>
#include <thread>

TEST( SIRAllocatorNegative, NullInitializationAndInvalidAlignments )
{
	SIRLinearAllocator linear;
	SIRPoolAllocator pool;
	SIRBitmapAllocator bitmap;
	SIRSlabAllocator slab;
	SIRTLSFAllocator tlsf;
	SIRFreeListAllocator freeList;
	SIRBuddyAllocator buddy;
	SIRPageRunAllocator pages;
	EXPECT_FALSE( linear.init( nullptr ) );
	EXPECT_FALSE( pool.init( nullptr, 64 ) );
	EXPECT_FALSE( bitmap.init( nullptr, 64 ) );
	EXPECT_FALSE( slab.init( nullptr, 64 ) );
	EXPECT_FALSE( tlsf.init( nullptr ) );
	EXPECT_FALSE( freeList.init( nullptr ) );
	EXPECT_FALSE( buddy.init( nullptr ) );
	EXPECT_FALSE( pages.init( nullptr ) );

	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "bad.align", 16 );
	ASSERT_NE( pSegment, nullptr );
	EXPECT_FALSE( pool.init( pSegment, 0 ) );
	EXPECT_FALSE( pool.init( pSegment, 64, 3 ) );
	EXPECT_FALSE( bitmap.init( pSegment, 64, 3 ) );
	EXPECT_FALSE( slab.init( pSegment, 64, 3 ) );
	ASSERT_TRUE( tlsf.init( pSegment ) );
	EXPECT_EQ( tlsf.alloc( 32, 3 ), nullptr );
	EXPECT_EQ( tlsf.alloc( UINT32_MAX, 16 ), nullptr );
	EXPECT_TRUE( tlsf.shutdown() );
}

TEST( SIRAllocatorNegative, FreedPointersRejectedAcrossIndependentVariableAllocators )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pTLSF = memory.segment( "wrong.tlsf", 64 );
	SIRMemorySegment *pList = memory.segment( "wrong.list", 64 );
	ASSERT_NE( pTLSF, nullptr );
	ASSERT_NE( pList, nullptr );
	SIRTLSFAllocator tlsf;
	SIRFreeListAllocator freeList;
	ASSERT_TRUE( tlsf.init( pTLSF ) );
	ASSERT_TRUE( freeList.init( pList ) );
	void *pA = tlsf.alloc( 320, 16 );
	void *pB = freeList.alloc( 384, 32 );
	ASSERT_NE( pA, nullptr );
	ASSERT_NE( pB, nullptr );
	EXPECT_FALSE( tlsf.free( pB ) );
	EXPECT_FALSE( freeList.free( pA ) );
	EXPECT_TRUE( tlsf.free( pA ) );
	EXPECT_TRUE( freeList.free( pB ) );
	EXPECT_FALSE( tlsf.free( pA ) );
	EXPECT_FALSE( freeList.free( pB ) );
	EXPECT_TRUE( tlsf.shutdown() );
	EXPECT_TRUE( freeList.shutdown() );
}

TEST( SIRAllocatorNegative, BudgetDenialDoesNotDestroyExistingPoolPayload )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	ASSERT_GT( nPage, 0u );
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 128, nPage * 24, nPage * 8, 8 };
	ASSERT_TRUE( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "budget.pool", nPage * 32, 0, SIR_MEMORY_DOMAIN_GAMEPLAY );
	ASSERT_NE( pSegment, nullptr );
	ASSERT_TRUE( memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_GAMEPLAY, nPage * 2 ) );
	SIRPoolAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 64, 16, nPage ) );
	std::vector<void *> live;
	for ( uint32_t i = 0; i < 512; ++i ) {
		void *pMemory = allocator.alloc();
		if ( pMemory == nullptr ) {
			break;
		}
		memset( pMemory, 0x6a, 64 );
		live.push_back( pMemory );
	}
	ASSERT_GT( live.size(), 0u );
	EXPECT_LE( pSegment->getCommittedBytes(), nPage * 2 );
	EXPECT_EQ( allocator.alloc(), nullptr );
	for ( void *pMemory : live ) {
		EXPECT_EQ( ( ( uint8_t * )pMemory )[0], 0x6a );
		ASSERT_TRUE( allocator.free( pMemory ) );
	}
	EXPECT_TRUE( allocator.reset( true ) );
	EXPECT_EQ( pSegment->getCommittedBytes(), 0u );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorNegative, PressureDefersReclamationWhileAllocationIsLive )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "pressure.live", 16 );
	ASSERT_NE( pSegment, nullptr );
	SIRPoolAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 128 ) );
	void *pLive = allocator.alloc();
	ASSERT_NE( pLive, nullptr );
	memset( pLive, 0x73, 128 );
	SIRMemoryPressureCoordinator pressure;
	ASSERT_TRUE( pressure.init( &memory.manager(), 1 ) );
	SIRMemorySegment *segments[1] = { pSegment };
	const sirReclaimOwnerConfig_t config = { &allocator, segments, 1, SIR_RECLAIM_POOL, 0, 0, 0, 0 };
	uint32_t nOwner = UINT32_MAX;
	ASSERT_TRUE( pressure.registerOwner( config, &nOwner ) );
	ASSERT_TRUE( pressure.requestReclaim( memory.page() ) );
	EXPECT_EQ( pressure.serviceOwner( nOwner ), 0u );
	EXPECT_EQ( *( ( uint8_t * )pLive ), 0x73 );
	ASSERT_TRUE( allocator.free( pLive ) );
	EXPECT_GT( pressure.serviceOwner( nOwner ), 0u );
	EXPECT_EQ( pSegment->getCommittedBytes(), 0u );
	EXPECT_TRUE( pressure.shutdown() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorNegative, ColdStageRejectsOutOfOrderAndCapacityOverflow )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "stage.order", 16 );
	ASSERT_NE( pSegment, nullptr );
	SIRMemoryColdPageStager stager;
	ASSERT_TRUE( stager.init( &memory.manager(), 2 ) );
	ASSERT_TRUE( stager.add( pSegment, memory.page() * 2, memory.page() ) );
	EXPECT_FALSE( stager.add( pSegment, memory.page(), memory.page() ) );
	ASSERT_TRUE( stager.add( pSegment, memory.page() * 3, memory.page() ) );
	EXPECT_FALSE( stager.add( pSegment, memory.page() * 4, memory.page() ) );
	stager.clear();
	ASSERT_TRUE( stager.add( pSegment, 0, memory.page() ) );
	const sirColdStageReport_t result = stager.commitStaged();
	EXPECT_EQ( result.nCompletedRanges, 1u );
	EXPECT_EQ( result.nNewCommittedBytes, memory.page() );
	ASSERT_TRUE( pSegment->decommit( 0, memory.page() ) );
}

TEST( SIRAllocatorNegative, GeneralHeapCorrectlyRoutesAndReallocates )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSmall = memory.segment( "heap.small", 256 );
	SIRMemorySegment *pMedium = memory.segment( "heap.medium", 64 );
	SIRMemorySegment *pLarge = memory.segment( "heap.large", 128 );
	ASSERT_NE( pSmall, nullptr );
	ASSERT_NE( pMedium, nullptr );
	ASSERT_NE( pLarge, nullptr );
	SIRCompactBucketAllocator small;
	SIRFreeListAllocator medium;
	SIRBuddyAllocator large;
	const sirCompactBucketConfig_t smallConfig = { 256, 16, 16384, 1, 1, 0 };
	ASSERT_TRUE( small.init( pSmall, smallConfig ) );
	ASSERT_TRUE( medium.init( pMedium ) );
	ASSERT_TRUE( large.init( pLarge ) );
	const sirGeneralHeapConfig_t config = { &small, &medium, nullptr, &large, pSmall, pMedium, pLarge, 256, 2048, false };
	SIRGeneralHeap heap;
	ASSERT_TRUE( heap.init( config ) );
	ASSERT_TRUE( heap.attachThread() );
	void *pA = heap.allocZeroed( 16, 4 );
	void *pB = heap.allocAligned( 400, 32 );
	void *pC = heap.allocAligned( 4096, 64 );
	ASSERT_NE( pA, nullptr );
	ASSERT_NE( pB, nullptr );
	ASSERT_NE( pC, nullptr );
	for ( uint32_t i = 0; i < 64; ++i ) {
		ASSERT_EQ( ( ( uint8_t * )pA )[i], 0u );
	}
	memset( pA, 0x33, 64 );
	pA = heap.reallocAligned( pA, 512, 32 );
	ASSERT_NE( pA, nullptr );
	for ( uint32_t i = 0; i < 64; ++i ) {
		ASSERT_EQ( ( ( uint8_t * )pA )[i], 0x33 );
	}
	EXPECT_TRUE( heap.owns( pA ) );
	EXPECT_TRUE( heap.owns( pB ) );
	EXPECT_TRUE( heap.owns( pC ) );
	EXPECT_FALSE( heap.owns( nullptr ) );
	EXPECT_TRUE( heap.tryFree( pA ) );
	EXPECT_TRUE( heap.tryFree( pB ) );
	EXPECT_TRUE( heap.tryFree( pC ) );
	EXPECT_TRUE( heap.detachThread() );
	EXPECT_TRUE( heap.shutdown() );
	EXPECT_TRUE( medium.shutdown() );
	EXPECT_TRUE( small.shutdown() );
}

TEST( SIRAllocatorNegative, RouterRejectsWrongThreadAndInvalidRequestWithoutMutatingOutput )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *classSegments[SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT] = {};
	for ( uint32_t i = 0; i < SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT; ++i ) {
		classSegments[i] = memory.segment( "route.class", 16 );
		ASSERT_NE( classSegments[i], nullptr );
	}
	SIRMemorySegment *pMedium = memory.segment( "route.medium", 64 );
	SIRMemorySegment *pLarge = memory.segment( "route.large", 128 );
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
	sirRoutedAllocation_t result = { ( void * )0x1234, 999, SIR_ROUTE_NONE };
	EXPECT_FALSE( router.alloc( 0, 16, &result ) );
	EXPECT_FALSE( router.alloc( 32, 3, &result ) );
	EXPECT_EQ( result.nBytes, 999u );
	std::atomic<bool> bOtherThreadSucceeded{ false };
	std::thread worker( [&router, &bOtherThreadSucceeded]() {
		sirRoutedAllocation_t allocation = {};
		bOtherThreadSucceeded.store( router.alloc( 64, 16, &allocation ), std::memory_order_relaxed );
	} );
	worker.join();
	EXPECT_FALSE( bOtherThreadSucceeded.load() );
	EXPECT_TRUE( classes.shutdown() );
	EXPECT_TRUE( medium.shutdown() );
}
