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

#include <Core/Memory/Allocators/SIRFrameAllocator.h>
#include <Core/Memory/Allocators/SIRJobScratchAllocator.h>
#include <Core/Memory/Allocators/SIRVirtualStackAllocator.h>
#include <Core/Memory/Allocators/SIRRingAllocator.h>
#include <Core/Memory/Allocators/SIRHandleHeap.h>
#include <Core/Memory/Allocators/SIRCacheAllocator.h>
#include <Core/Memory/SIRSharedMemoryPool.h>
#include <Core/Memory/SIRMemoryColdPageStager.h>
#include <Core/Memory/SIRMemoryLayoutPlanner.h>

#include <gtest/gtest.h>

TEST( SIRAllocatorLifetime, FrameEpochRequiresCompletionBeforeReuse )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *segments[2] = { memory.segment( "frame.0", 16 ), memory.segment( "frame.1", 16 ) };
	ASSERT_NE( segments[0], nullptr );
	ASSERT_NE( segments[1], nullptr );
	SIRFrameAllocator allocator;
	ASSERT_TRUE( allocator.init( segments, 2 ) );
	EXPECT_EQ( allocator.alloc( 32 ), nullptr );
	ASSERT_TRUE( allocator.beginEpoch( 10, 0 ) );
	void *pEpoch10 = allocator.alloc( 128, 64 );
	ASSERT_NE( pEpoch10, nullptr );
	memset( pEpoch10, 0x4a, 128 );
	EXPECT_TRUE( allocator.submitEpoch( 10 ) );
	ASSERT_TRUE( allocator.beginEpoch( 11, 0 ) );
	void *pEpoch11 = allocator.alloc( 128, 64 );
	ASSERT_NE( pEpoch11, nullptr );
	EXPECT_NE( pEpoch10, pEpoch11 );
	EXPECT_EQ( ( ( uint8_t * )pEpoch10 )[0], 0x4a );
	ASSERT_TRUE( allocator.submitEpoch( 11 ) );
	EXPECT_FALSE( allocator.beginEpoch( 12, 0 ) );
	ASSERT_TRUE( allocator.releaseCompleted( 10 ) );
	ASSERT_TRUE( allocator.beginEpoch( 12, 10 ) );
	EXPECT_NE( allocator.alloc( 128 ), nullptr );
	ASSERT_TRUE( allocator.submitEpoch( 12 ) );
	ASSERT_TRUE( allocator.releaseCompleted( 12 ) );
	EXPECT_TRUE( allocator.trimIdle() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorLifetime, JobScratchRejectsStaleTokensAndReusesSlot )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *segments[2] = { memory.segment( "job.0", 16 ), memory.segment( "job.1", 16 ) };
	ASSERT_NE( segments[0], nullptr );
	ASSERT_NE( segments[1], nullptr );
	SIRJobScratchAllocator allocator;
	ASSERT_TRUE( allocator.init( segments, 2 ) );
	const sirJobScratchToken_t first = allocator.beginJob( 0 );
	ASSERT_NE( allocator.alloc( first, 700, 64 ), nullptr );
	const uint64_t nMark = allocator.mark( first );
	ASSERT_NE( allocator.alloc( first, 200, 16 ), nullptr );
	ASSERT_TRUE( allocator.rewind( first, nMark ) );
	ASSERT_TRUE( allocator.completeJob( first, true ) );
	EXPECT_EQ( allocator.alloc( first, 64, 16 ), nullptr );
	EXPECT_FALSE( allocator.completeJob( first ) );
	const sirJobScratchToken_t second = allocator.beginJob( 0 );
	EXPECT_NE( allocator.alloc( second, 64, 16 ), nullptr );
	ASSERT_TRUE( allocator.completeJobRetaining( second, memory.page() ) );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorLifetime, VirtualStackNestedBookmarksAndDecommit )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "stack", 16 );
	ASSERT_NE( pSegment, nullptr );
	SIRVirtualStackAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, SIR_STACK_DECOMMIT_ON_EMPTY ) );
	{
		auto outer = allocator.createScopedBookmark();
		void *pA = allocator.alloc( 65, 64 );
		ASSERT_NE( pA, nullptr );
		const uint64_t nOuter = allocator.getUsedBytes();
		{
			auto inner = allocator.createScopedBookmark();
			ASSERT_NE( allocator.alloc( 8192, 128 ), nullptr );
			EXPECT_GT( allocator.getUsedBytes(), nOuter );
		}
		EXPECT_EQ( allocator.getUsedBytes(), nOuter );
	}
	EXPECT_EQ( allocator.getUsedBytes(), 0u );
	EXPECT_EQ( allocator.getCommittedBytes(), 0u );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorLifetime, RingSequenceBackpressureRetirementAndWraparound )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "ring", 8 );
	ASSERT_NE( pSegment, nullptr );
	SIRRingAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 8 ) );
	uint64_t sequence[8] = {};
	void *pFirst = allocator.alloc( 200, 16, &sequence[0] );
	ASSERT_NE( pFirst, nullptr );
	for ( uint32_t i = 1; i < 8; ++i ) {
		ASSERT_NE( allocator.alloc( 200, 16, &sequence[i] ), nullptr );
		EXPECT_GT( sequence[i], sequence[i - 1] );
	}
	EXPECT_EQ( allocator.getOutstandingCount(), 8u );
	EXPECT_EQ( allocator.alloc( 64, 16, &sequence[0] ), nullptr );
	EXPECT_FALSE( allocator.trimIfEmpty() );
	EXPECT_FALSE( allocator.retireThrough( UINT64_MAX ) );
	ASSERT_TRUE( allocator.retireThrough( sequence[3] ) );
	EXPECT_EQ( allocator.getOutstandingCount(), 4u );
	uint64_t nNewest = 0;
	ASSERT_NE( allocator.alloc( 128, 16, &nNewest ), nullptr );
	ASSERT_TRUE( allocator.retireThrough( nNewest ) );
	EXPECT_EQ( allocator.getOutstandingCount(), 0u );
	EXPECT_TRUE( allocator.trimIfEmpty() );
	EXPECT_EQ( pSegment->getCommittedBytes(), 0u );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorLifetime, HandlePinGenerationAndCompactionIntegrity )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "handle", 64 );
	ASSERT_NE( pSegment, nullptr );
	SIRHandleHeap allocator;
	ASSERT_TRUE( allocator.init( pSegment, 64 ) );
	sirMemoryHandle_t handles[12] = {};
	for ( uint32_t i = 0; i < 12; ++i ) {
		void *pMemory = allocator.alloc( 512, 64, &handles[i] );
		ASSERT_NE( pMemory, nullptr );
		memset( pMemory, ( int )( i + 1 ), 512 );
	}
	ASSERT_NE( allocator.pin( handles[5] ), nullptr );
	EXPECT_FALSE( allocator.free( handles[5] ) );
	ASSERT_TRUE( allocator.compact() );
	EXPECT_NE( allocator.resolve( handles[5] ), nullptr );
	ASSERT_TRUE( allocator.unpin( handles[5] ) );
	for ( uint32_t i = 0; i < 12; i += 2 ) {
		ASSERT_TRUE( allocator.free( handles[i] ) );
		EXPECT_EQ( allocator.resolve( handles[i] ), nullptr );
	}
	ASSERT_TRUE( allocator.compact() );
	for ( uint32_t i = 1; i < 12; i += 2 ) {
		const uint8_t *pBytes = ( const uint8_t * )allocator.resolve( handles[i] );
		ASSERT_NE( pBytes, nullptr );
		for ( uint32_t j = 0; j < 512; ++j ) {
			ASSERT_EQ( pBytes[j], i + 1 );
		}
		ASSERT_TRUE( allocator.free( handles[i] ) );
	}
	EXPECT_EQ( allocator.getLiveCount(), 0u );
	EXPECT_TRUE( allocator.trimUnusedPages() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorLifetime, CachePinnedEntriesCannotBeEvicted )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "cache", 128 );
	ASSERT_NE( pSegment, nullptr );
	SIRCacheAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 32 ) );
	sirCacheHandle_t handles[12] = {};
	for ( uint32_t i = 0; i < 12; ++i ) {
		void *pMemory = allocator.alloc( 1024, 16, &handles[i] );
		ASSERT_NE( pMemory, nullptr );
		memset( pMemory, ( int )( i + 10 ), 1024 );
	}
	ASSERT_NE( allocator.pin( handles[0] ), nullptr );
	EXPECT_FALSE( allocator.release( handles[0] ) );
	for ( uint32_t i = 1; i < 12; ++i ) {
		EXPECT_TRUE( allocator.touch( handles[i] ) );
	}
	EXPECT_TRUE( allocator.evictOne() );
	EXPECT_NE( allocator.resolve( handles[0] ), nullptr );
	EXPECT_GT( allocator.getEvictedCount(), 0u );
	ASSERT_TRUE( allocator.unpin( handles[0] ) );
	ASSERT_TRUE( allocator.release( handles[0] ) );
	EXPECT_EQ( allocator.resolve( handles[0] ), nullptr );
	for ( uint32_t i = 1; i < 12; ++i ) {
		if ( allocator.resolve( handles[i] ) != nullptr ) {
			ASSERT_TRUE( allocator.release( handles[i] ) );
		}
	}
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorLifetime, SharedPoolRejectsOutOfOrderReleaseAndBounds )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "shared", 16 );
	ASSERT_NE( pSegment, nullptr );
	SIRSharedMemoryPool allocator;
	ASSERT_TRUE( allocator.init( pSegment ) );
	sirSharedMemoryRegion_t first = {};
	sirSharedMemoryRegion_t second = {};
	ASSERT_TRUE( allocator.allocateRegion( memory.page() * 2, first ) );
	ASSERT_TRUE( allocator.allocateRegion( memory.page() * 3, second ) );
	EXPECT_FALSE( allocator.releaseLastRegion( first ) );
	EXPECT_FALSE( SIRSharedMemoryPool::commit( first, memory.page() * 2, memory.page() ) );
	ASSERT_TRUE( SIRSharedMemoryPool::commit( first, 0, memory.page() ) );
	ASSERT_TRUE( SIRSharedMemoryPool::decommit( first, 0, memory.page() ) );
	ASSERT_TRUE( allocator.releaseLastRegion( second ) );
	ASSERT_TRUE( allocator.releaseLastRegion( first ) );
	EXPECT_EQ( allocator.getAvailableBytes(), pSegment->getReservedBytes() );
}

TEST( SIRAllocatorLifetime, ColdStagerMergesRangesAndReportsActualPageCommit )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "stage", 16 );
	ASSERT_NE( pSegment, nullptr );
	SIRMemoryColdPageStager stager;
	ASSERT_TRUE( stager.init( &memory.manager(), 8 ) );
	EXPECT_FALSE( stager.add( pSegment, 1, memory.page() ) );
	ASSERT_TRUE( stager.add( pSegment, 0, memory.page() * 2 ) );
	ASSERT_TRUE( stager.add( pSegment, memory.page(), memory.page() * 3 ) );
	ASSERT_TRUE( stager.add( pSegment, memory.page() * 4, memory.page() ) );
	const sirColdStageReport_t result = stager.commitStaged();
	EXPECT_EQ( result.nCompletedRanges, 1u );
	EXPECT_EQ( result.nNewCommittedBytes, memory.page() * 5 );
	EXPECT_EQ( pSegment->getCommittedBytes(), memory.page() * 5 );
	EXPECT_TRUE( pSegment->decommit( 0, memory.page() * 5 ) );
}

TEST( SIRAllocatorLifetime, LayoutPlannerRejectsImpossibleReservations )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	ASSERT_GT( nPage, 0u );
	const sirMemoryConfig_t config = { nPage * 128, nPage * 64, nPage * 8, 8 };
	const sirMemoryBudgetProfile_t budget = { nPage * 48, nPage * 64 };
	sirMemoryLayoutResult_t result = {};
	sirMemoryLayoutEntry_t entries[2] = {
	    { nPage * 16, nPage * 16, 0, false, false },
	    { nPage * 32, nPage * 32, 0, false, false },
	};
	EXPECT_TRUE( SIRMemoryLayoutPlanner::preflight( config, budget, entries, 2, &result ) );
	EXPECT_EQ( result.nSegmentCount, 2u );
	entries[1].nReserveBytes = UINT64_MAX;
	EXPECT_FALSE( SIRMemoryLayoutPlanner::preflight( config, budget, entries, 2, &result ) );
	EXPECT_FALSE( SIRMemoryLayoutPlanner::preflight( config, budget, nullptr, 2, &result ) );
}

TEST( SIRAllocatorLifetime, HandleCompactionBudgetPreservesUnmovedPayloads )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "handle.budget", 64 );
	ASSERT_NE( pSegment, nullptr );
	SIRHandleHeap allocator;
	ASSERT_TRUE( allocator.init( pSegment, 32 ) );
	sirMemoryHandle_t handles[3] = {};
	for ( uint32_t i = 0; i < 3; ++i ) {
		void *pMemory = allocator.alloc( 256, 32, &handles[i] );
		ASSERT_NE( pMemory, nullptr );
		memset( pMemory, ( int )( i + 30 ), 256 );
	}
	ASSERT_TRUE( allocator.free( handles[0] ) );
	const uint64_t nUpperBound = allocator.getCompactionPayloadUpperBound();
	ASSERT_GT( nUpperBound, 0u );
	EXPECT_FALSE( allocator.compactWithinBudget( nUpperBound - 1 ) );
	EXPECT_TRUE( allocator.compactWithinBudget( nUpperBound ) );
	for ( uint32_t i = 1; i < 3; ++i ) {
		const uint8_t *pBytes = ( const uint8_t * )allocator.resolve( handles[i] );
		ASSERT_NE( pBytes, nullptr );
		for ( uint32_t j = 0; j < 256; ++j ) {
			ASSERT_EQ( pBytes[j], i + 30 );
		}
		ASSERT_TRUE( allocator.free( handles[i] ) );
	}
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorLifetime, CachePressureRespectsPinsAndReclaimsColdEntries )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "cache.pressure", 128 );
	ASSERT_NE( pSegment, nullptr );
	SIRCacheAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 32 ) );
	sirCacheHandle_t handles[16] = {};
	for ( uint32_t i = 0; i < 16; ++i ) {
		ASSERT_NE( allocator.alloc( 2048, 16, &handles[i] ), nullptr );
	}
	ASSERT_NE( allocator.pin( handles[0] ), nullptr );
	const sirCachePressureResult_t result = allocator.reclaimCold( memory.page(), 16 );
	EXPECT_GT( result.nEvicted, 0u );
	EXPECT_GT( result.nFreedBlockBytes, 0u );
	EXPECT_NE( allocator.resolve( handles[0] ), nullptr );
	ASSERT_TRUE( allocator.unpin( handles[0] ) );
	for ( sirCacheHandle_t handle : handles ) {
		if ( allocator.resolve( handle ) != nullptr ) {
			ASSERT_TRUE( allocator.release( handle ) );
		}
	}
	EXPECT_EQ( allocator.getLiveCount(), 0u );
	EXPECT_TRUE( allocator.shutdown() );
}
