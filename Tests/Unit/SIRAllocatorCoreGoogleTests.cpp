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
#include <Core/Memory/Allocators/SIRMediumPageAllocator.h>
#include <Core/Memory/Allocators/SIRHighLowAllocator.h>

#include <gtest/gtest.h>

TEST( SIRAllocatorCore, LinearAlignmentBookmarksAndExhaustion )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "linear", 16 );
	ASSERT_NE( pSegment, nullptr );
	SIRLinearAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment ) );
	EXPECT_FALSE( allocator.init( pSegment ) );
	EXPECT_EQ( allocator.alloc( 0, 16 ), nullptr );
	EXPECT_EQ( allocator.alloc( 8, 3 ), nullptr );
	EXPECT_EQ( allocator.alloc( UINT64_MAX, 16 ), nullptr );

	for ( uint32_t nAlignment = 1; nAlignment <= 4096; nAlignment <<= 1 ) {
		void *pMemory = allocator.alloc( 63, nAlignment );
		ASSERT_NE( pMemory, nullptr );
		EXPECT_EQ( ( uintptr_t )pMemory % nAlignment, 0u );
		memset( pMemory, 0x35, 63 );
	}
	const uint64_t nMark = allocator.getMark();
	ASSERT_NE( allocator.alloc( 2048, 64 ), nullptr );
	ASSERT_TRUE( allocator.rewind( nMark ) );
	EXPECT_EQ( allocator.getUsedBytes(), nMark );
	EXPECT_FALSE( allocator.rewind( allocator.getCapacity() + 1 ) );
	ASSERT_NE( allocator.alloc( 2048, 64 ), nullptr );
	ASSERT_TRUE( allocator.reset( true ) );
	EXPECT_EQ( allocator.getUsedBytes(), 0u );
	EXPECT_EQ( pSegment->getCommittedBytes(), 0u );
	ASSERT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorCore, PoolRandomizedReuseAndInvalidFree )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "pool", 32 );
	ASSERT_NE( pSegment, nullptr );
	SIRPoolAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 64, 16 ) );
	EXPECT_FALSE( allocator.init( pSegment, 64 ) );
	EXPECT_TRUE( sirTestFixedTrace( allocator, 4096, 64, 0xA14BC14DULL ) );
	EXPECT_EQ( allocator.getLiveSlots(), 0u );
	void *pMemory = allocator.alloc();
	ASSERT_NE( pMemory, nullptr );
	EXPECT_FALSE( allocator.free( ( uint8_t * )pMemory + 1 ) );
	EXPECT_TRUE( allocator.free( pMemory ) );
	EXPECT_FALSE( allocator.free( pMemory ) );
	EXPECT_TRUE( allocator.reset( true ) );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorCore, PoolExhaustionAndRecovery )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "pool.exhaust", 4 );
	ASSERT_NE( pSegment, nullptr );
	SIRPoolAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 128, 16 ) );
	std::vector<void *> slots;
	for ( uint32_t i = 0; i < 65536; ++i ) {
		void *pMemory = allocator.alloc();
		if ( pMemory == nullptr ) {
			break;
		}
		slots.push_back( pMemory );
	}
	ASSERT_GT( slots.size(), 1u );
	EXPECT_LE( slots.size(), ( size_t )( pSegment->getReservedBytes() / allocator.getSlotStride() ) );
	EXPECT_EQ( allocator.alloc(), nullptr );
	for ( void *pMemory : slots ) {
		ASSERT_TRUE( allocator.free( pMemory ) );
	}
	EXPECT_EQ( allocator.getLiveSlots(), 0u );
	void *pAgain = allocator.alloc();
	ASSERT_NE( pAgain, nullptr );
	ASSERT_TRUE( allocator.free( pAgain ) );
	EXPECT_TRUE( allocator.reset() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorCore, BitmapRandomizedReuseAndInvalidFree )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "bitmap", 32 );
	ASSERT_NE( pSegment, nullptr );
	SIRBitmapAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 48, 16 ) );
	EXPECT_TRUE( sirTestFixedTrace( allocator, 4096, 48, 0xA42EDB12ULL ) );
	EXPECT_EQ( allocator.getLiveSlots(), 0u );
	void *pMemory = allocator.alloc();
	ASSERT_NE( pMemory, nullptr );
	EXPECT_FALSE( allocator.free( ( uint8_t * )pMemory + 1 ) );
	EXPECT_TRUE( allocator.free( pMemory ) );
	EXPECT_FALSE( allocator.free( pMemory ) );
	EXPECT_TRUE( allocator.trimUnusedPages() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorCore, SlabRandomizedReuseAndRelease )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "slab", 64 );
	ASSERT_NE( pSegment, nullptr );
	SIRSlabAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, 80, 16, 16384 ) );
	EXPECT_TRUE( sirTestFixedTrace( allocator, 4096, 80, 0x52ABCFF1ULL ) );
	EXPECT_EQ( allocator.getLiveSlots(), 0u );
	void *pMemory = allocator.alloc();
	ASSERT_NE( pMemory, nullptr );
	EXPECT_TRUE( allocator.owns( pMemory ) );
	EXPECT_FALSE( allocator.free( ( uint8_t * )pMemory + 1 ) );
	EXPECT_TRUE( allocator.free( pMemory ) );
	EXPECT_FALSE( allocator.free( pMemory ) );
	EXPECT_TRUE( allocator.trimEmptySlabsTo( 0 ) );
	EXPECT_EQ( allocator.getCommittedSlabs(), 0u );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorCore, TLSFRandomizedFragmentationAndCoalescing )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "tlsf", 256 );
	ASSERT_NE( pSegment, nullptr );
	SIRTLSFAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment ) );
	EXPECT_TRUE( sirTestVariableTrace( allocator, 6000, 4096, 0x777733331234ULL ) );
	EXPECT_EQ( allocator.getAllocationCount(), 0u );
	EXPECT_TRUE( allocator.trimUnusedPages() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorCore, FreeListRandomizedFragmentationAndCoalescing )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "freelist", 256 );
	ASSERT_NE( pSegment, nullptr );
	SIRFreeListAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment ) );
	EXPECT_TRUE( sirTestVariableTrace( allocator, 6000, 4096, 0x914532ABCDEFULL ) );
	EXPECT_EQ( allocator.getAllocationCount(), 0u );
	EXPECT_TRUE( allocator.trimUnusedPages() );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorCore, BuddyRandomizedSplitMergeAndReclamation )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "buddy", 256 );
	ASSERT_NE( pSegment, nullptr );
	SIRBuddyAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment ) );
	EXPECT_TRUE( sirTestVariableTrace( allocator, 5000, 8192, 0x12121212ABULL ) );
	EXPECT_EQ( allocator.getAllocationCount(), 0u );
	EXPECT_TRUE( allocator.trimUnusedPages() );
}

TEST( SIRAllocatorCore, PageRunRandomizedCapacityAndReclamation )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "pagerun", 256 );
	ASSERT_NE( pSegment, nullptr );
	SIRPageRunAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment ) );
	EXPECT_TRUE( sirTestVariableTrace( allocator, 3000, ( uint32_t )( memory.page() * 3 ), 0x44556677ULL ) );
	EXPECT_EQ( allocator.getAllocationCount(), 0u );
	EXPECT_TRUE( allocator.trimUnusedPages() );
}

TEST( SIRAllocatorCore, MediumPageRandomizedAllocationAndTrim )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "medium", 256 );
	ASSERT_NE( pSegment, nullptr );
	SIRMediumPageAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment, ( uint32_t )( memory.page() * 4 ) ) );
	EXPECT_TRUE( sirTestVariableTrace( allocator, 3500, 2048, 0x123ABC321ULL ) );
	const uint64_t nBefore = pSegment->getCommittedBytes();
	( void )allocator.trimEmptyPages( 0 );
	EXPECT_LE( pSegment->getCommittedBytes(), nBefore );
	EXPECT_TRUE( allocator.shutdown() );
}

TEST( SIRAllocatorCore, HighLowIndependentMarkersAndCapacity )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "highlow", 16 );
	ASSERT_NE( pSegment, nullptr );
	SIRHighLowAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment ) );
	void *pLow = allocator.allocLow( 128, 64 );
	void *pHigh = allocator.allocHigh( 256, 128 );
	ASSERT_NE( pLow, nullptr );
	ASSERT_NE( pHigh, nullptr );
	EXPECT_LE( ( uintptr_t )pLow + 128, ( uintptr_t )pHigh );
	const uint64_t nLowMark = allocator.getLowMark();
	const uint64_t nHighMark = allocator.getHighMark();
	ASSERT_NE( allocator.allocLow( 1024, 32 ), nullptr );
	ASSERT_NE( allocator.allocHigh( 1024, 32 ), nullptr );
	EXPECT_TRUE( allocator.rewindLow( nLowMark ) );
	EXPECT_TRUE( allocator.rewindHigh( nHighMark ) );
	EXPECT_FALSE( allocator.rewindLow( pSegment->getReservedBytes() + 1 ) );
	EXPECT_TRUE( allocator.reset() );
	EXPECT_EQ( allocator.getLowUsedBytes(), 0u );
	EXPECT_EQ( allocator.getHighUsedBytes(), 0u );
	EXPECT_TRUE( allocator.trimUnusedPages() );
	EXPECT_TRUE( allocator.shutdown() );
}
