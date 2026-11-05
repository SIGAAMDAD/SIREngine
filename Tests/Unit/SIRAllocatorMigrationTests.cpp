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

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/Allocators/SIRLinearAllocator.h>
#include <Core/Memory/Allocators/SIRPoolAllocator.h>
#include <Core/Memory/Allocators/SIRBitmapAllocator.h>
#include <Core/Memory/Allocators/SIRSlabAllocator.h>
#include <Core/Memory/Allocators/SIRTLSFAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/Allocators/SIRPageRunAllocator.h>
#include <Core/Memory/Allocators/SIRFrameAllocator.h>
#include <Core/Memory/Allocators/SIRRingAllocator.h>
#include <Core/Memory/Allocators/SIRHandleHeap.h>
#include <Core/Memory/Allocators/SIRHighLowAllocator.h>
#include <Core/Memory/Allocators/SIRVirtualStackAllocator.h>
#include <Core/Memory/Allocators/SIRJobScratchAllocator.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRMediumPageAllocator.h>
#include <Core/Memory/Allocators/SIRCacheAllocator.h>
#include <Core/Memory/Allocators/SIRSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRReclaimableSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SIR_CHECK( expression )                                                                                                            \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d failed: %s\n", __FILE__, __LINE__, #expression );                                                      \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool testPrimitiveAllocators( SIRMemoryManager& memory, uint64_t nPage )
{
	SIRMemorySegment *pLinear = memory.createSegment( "test.linear", nPage * 32 );
	SIRMemorySegment *pPool = memory.createSegment( "test.pool", nPage * 32 );
	SIRMemorySegment *pBitmap = memory.createSegment( "test.bitmap", nPage * 32 );
	SIRMemorySegment *pSlab = memory.createSegment( "test.slab", nPage * 32 );
	SIR_CHECK( pLinear != nullptr && pPool != nullptr && pBitmap != nullptr && pSlab != nullptr );

	SIRLinearAllocator linear;
	SIR_CHECK( linear.init( pLinear ) );
	void *pFirst = linear.alloc( 31, 32 );
	SIR_CHECK( pFirst != nullptr && ( ( uintptr_t )pFirst % 32 ) == 0 );
	uint64_t nMark = linear.getMark();
	SIR_CHECK( linear.alloc( 100, 64 ) != nullptr );
	SIR_CHECK( linear.rewind( nMark ) );
	SIR_CHECK( linear.alloc( 100, 64 ) != nullptr );
	SIR_CHECK( linear.reset() );
	SIR_CHECK( linear.shutdown() );

	SIRPoolAllocator pool;
	SIR_CHECK( pool.init( pPool, 64 ) );
	void *pPooled = pool.alloc();
	SIR_CHECK( pPooled != nullptr );
	SIR_CHECK( pool.getLiveSlots() == 1 );
	SIR_CHECK( pool.free( pPooled ) );
	SIR_CHECK( !pool.free( pPooled ) );
	SIR_CHECK( pool.shutdown() );

	SIRBitmapAllocator bitmap;
	SIR_CHECK( bitmap.init( pBitmap, 32 ) );
	void *pMapped = bitmap.alloc();
	SIR_CHECK( pMapped != nullptr && bitmap.getLiveSlots() == 1 );
	SIR_CHECK( bitmap.free( pMapped ) );
	SIR_CHECK( bitmap.shutdown() );

	SIRSlabAllocator slab;
	SIR_CHECK( slab.init( pSlab, 64 ) );
	void *pSlabSlot = slab.alloc();
	SIR_CHECK( pSlabSlot != nullptr && slab.owns( pSlabSlot ) );
	SIR_CHECK( slab.free( pSlabSlot ) );
	SIR_CHECK( slab.shutdown() );
	return true;
}

static bool testVariableAllocators( SIRMemoryManager& memory, uint64_t nPage )
{
	SIRMemorySegment *pTLSF = memory.createSegment( "test.tlsf", nPage * 64 );
	SIRMemorySegment *pBuddy = memory.createSegment( "test.buddy", nPage * 64 );
	SIRMemorySegment *pPageRun = memory.createSegment( "test.pagerun", nPage * 64 );
	SIR_CHECK( pTLSF != nullptr && pBuddy != nullptr && pPageRun != nullptr );

	SIRTLSFAllocator tlsf;
	SIR_CHECK( tlsf.init( pTLSF ) );
	void *pMedium = tlsf.alloc( 1234, 32 );
	SIR_CHECK( pMedium != nullptr && ( ( uintptr_t )pMedium % 32 ) == 0 );
	SIR_CHECK( tlsf.getUsableSize( pMedium ) >= 1234 );
	memset( pMedium, 0xa5, 1234 );
	SIR_CHECK( tlsf.free( pMedium ) );
	SIR_CHECK( tlsf.shutdown() );

	SIRBuddyAllocator buddy;
	SIR_CHECK( buddy.init( pBuddy ) );
	void *pLarge = buddy.alloc( 9000, 64 );
	SIR_CHECK( pLarge != nullptr && buddy.getAllocationCount() == 1 );
	SIR_CHECK( buddy.getUsableSize( pLarge ) >= 9000 );
	SIR_CHECK( buddy.free( pLarge ) );

	SIRPageRunAllocator pages;
	SIR_CHECK( pages.init( pPageRun ) );
	void *pRun = pages.alloc( nPage * 3, nPage );
	SIR_CHECK( pRun != nullptr && pages.getAllocationCount() == 1 );
	SIR_CHECK( pages.free( pRun ) );
	return true;
}

static bool testEpochAllocators( SIRMemoryManager& memory, uint64_t nPage )
{
	SIRMemorySegment *pFrameA = memory.createSegment( "test.frame.a", nPage * 16 );
	SIRMemorySegment *pFrameB = memory.createSegment( "test.frame.b", nPage * 16 );
	SIRMemorySegment *pRing = memory.createSegment( "test.ring", nPage * 16 );
	SIR_CHECK( pFrameA != nullptr && pFrameB != nullptr && pRing != nullptr );

	SIRFrameAllocator frame;
	SIRMemorySegment *szFrameSegments[2] = { pFrameA, pFrameB };
	SIR_CHECK( frame.init( szFrameSegments, 2 ) );
	SIR_CHECK( frame.beginEpoch( 1, 0 ) );
	SIR_CHECK( frame.alloc( 128 ) != nullptr );
	SIR_CHECK( frame.submitEpoch( 1 ) );
	SIR_CHECK( frame.releaseCompleted( 1 ) );
	SIR_CHECK( frame.shutdown() );

	SIRRingAllocator ring;
	SIR_CHECK( ring.init( pRing, 16 ) );
	uint64_t nSequence = 0;
	SIR_CHECK( ring.alloc( 64, 16, &nSequence ) != nullptr );
	SIR_CHECK( ring.getOutstandingCount() == 1 );
	SIR_CHECK( ring.retireThrough( nSequence ) );
	SIR_CHECK( ring.getOutstandingCount() == 0 );
	SIR_CHECK( ring.shutdown() );
	return true;
}

static bool testHandleAllocator( SIRMemoryManager& memory, uint64_t nPage )
{
	SIRMemorySegment *pSegment = memory.createSegment( "test.handle", nPage * 32 );
	SIR_CHECK( pSegment != nullptr );
	SIRHandleHeap heap;
	SIR_CHECK( heap.init( pSegment, 64 ) );
	sirMemoryHandle_t handle = {};
	void *pData = heap.alloc( 128, 16, &handle );
	SIR_CHECK( pData != nullptr && heap.resolve( handle ) != nullptr );
	SIR_CHECK( heap.pin( handle ) != nullptr );
	SIR_CHECK( !heap.free( handle ) );
	SIR_CHECK( heap.unpin( handle ) );
	SIR_CHECK( heap.free( handle ) );
	SIR_CHECK( heap.resolve( handle ) == nullptr );
	SIR_CHECK( heap.shutdown() );
	return true;
}

static bool testOtherAllocators( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 8192, nPage * 4096, nPage * 512, 32 };
	SIR_CHECK( memory.init( config ) );

	SIRMemorySegment *pHighLow = memory.createSegment( "test.highlow", nPage * 24 );
	SIRMemorySegment *pStack = memory.createSegment( "test.stack", nPage * 24 );
	SIRMemorySegment *pJob = memory.createSegment( "test.jobscratch", nPage * 24 );
	SIRMemorySegment *pFreeList = memory.createSegment( "test.freelist", nPage * 64 );
	SIRMemorySegment *pMedium = memory.createSegment( "test.medium", nPage * 64 );
	SIRMemorySegment *pCache = memory.createSegment( "test.cache", nPage * 64 );
	SIR_CHECK( pHighLow && pStack && pJob && pFreeList && pMedium && pCache );

	SIRHighLowAllocator highlow;
	SIR_CHECK( highlow.init( pHighLow ) );
	void *pLow = highlow.allocLow( 128, 64 );
	void *pHigh = highlow.allocHigh( 128, 64 );
	SIR_CHECK( pLow && pHigh && pLow != pHigh );
	SIR_CHECK( highlow.reset() && highlow.shutdown() );

	SIRVirtualStackAllocator stack;
	SIR_CHECK( stack.init( pStack, SIR_STACK_DECOMMIT_ON_EMPTY ) );
	{
		auto bookmark = stack.createScopedBookmark();
		SIR_CHECK( stack.alloc( 128, 16 ) != nullptr );
	}
	SIR_CHECK( stack.getUsedBytes() == 0 );
	SIR_CHECK( stack.shutdown() );

	SIRJobScratchAllocator scratch;
	SIRMemorySegment *szJobSegments[1] = { pJob };
	SIR_CHECK( scratch.init( szJobSegments, 1 ) );
	sirJobScratchToken_t token = scratch.beginJob( 0 );
	SIR_CHECK( scratch.alloc( token, 128, 16 ) != nullptr );
	SIR_CHECK( scratch.completeJob( token ) );
	SIR_CHECK( scratch.alloc( token, 128, 16 ) == nullptr );
	SIR_CHECK( scratch.shutdown() );

	SIRFreeListAllocator freeList;
	SIR_CHECK( freeList.init( pFreeList ) );
	void *pFromFreeList = freeList.alloc( 777, 32 );
	SIR_CHECK( pFromFreeList && freeList.getUsableSize( pFromFreeList ) >= 777 );
	SIR_CHECK( freeList.free( pFromFreeList ) && freeList.shutdown() );

	SIRMediumPageAllocator medium;
	SIR_CHECK( medium.init( pMedium, ( uint32_t )( nPage * 4 ) ) );
	void *pFromMedium = medium.alloc( 1056, 16 );
	SIR_CHECK( pFromMedium && medium.getUsableSize( pFromMedium ) >= 1056 );
	SIR_CHECK( medium.free( pFromMedium ) && medium.shutdown() );

	SIRCacheAllocator cache;
	SIR_CHECK( cache.init( pCache, 16 ) );
	sirCacheHandle_t handle = {};
	void *pCached = cache.alloc( 300, 16, &handle );
	SIR_CHECK( pCached && cache.resolve( handle ) );
	SIR_CHECK( cache.pin( handle ) );
	SIR_CHECK( !cache.release( handle ) );
	SIR_CHECK( cache.unpin( handle ) && cache.release( handle ) );
	SIR_CHECK( cache.shutdown() );

	return true;
}

static bool testSizeClasses( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 4096, nPage * 2048, nPage * 512, 32 };
	SIR_CHECK( memory.init( config ) );
	SIRMemorySegment *szSegments[SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT] = {};
	for ( uint32_t iClass = 0; iClass < SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT; ++iClass ) {
		szSegments[iClass] = memory.createSegment( "test.sizeclass", nPage * 32 );
		SIR_CHECK( szSegments[iClass] != nullptr );
	}
	SIRSizeClassAllocator classes;
	SIR_CHECK( classes.init( szSegments ) );
	void *pSmall = classes.alloc( 48 );
	SIR_CHECK( pSmall && classes.free( pSmall, 48 ) );
	SIR_CHECK( classes.shutdown() );

	SIRReclaimableSizeClassAllocator reclaimable;
	SIR_CHECK( reclaimable.init( szSegments ) );
	pSmall = reclaimable.alloc( 48, 16 );
	SIR_CHECK( pSmall && reclaimable.free( pSmall, 48 ) );
	SIR_CHECK( reclaimable.trimIdle() && reclaimable.shutdown() );
	return true;
}

static bool testCompactBucket( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 2048, nPage * 1024, nPage * 256, 8 };
	SIR_CHECK( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "test.bucket", nPage * 256 );
	SIR_CHECK( pSegment != nullptr );
	sirCompactBucketConfig_t configBucket = { 256, 16, 16384, 2, 1, 0 };
	SIRCompactBucketAllocator bucket;
	SIR_CHECK( bucket.init( pSegment, configBucket ) );
	SIR_CHECK( bucket.attach() );
	void *pAllocation = bucket.alloc( 48, 16 );
	SIR_CHECK( pAllocation != nullptr && bucket.owns( pAllocation ) );
	SIR_CHECK( bucket.free( pAllocation ) );
	SIR_CHECK( bucket.detach() );
	SIR_CHECK( bucket.shutdown() );
	return true;
}

int main( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	if ( nPage == 0 ) {
		return 1;
	}

	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 4096, nPage * 2048, nPage * 256, 32 };
	if ( !memory.init( config ) ) {
		fputs( "SIR allocator test: manager initialization failed\n", stderr );
		return 1;
	}

	const bool bPassed = testPrimitiveAllocators( memory, nPage ) && testVariableAllocators( memory, nPage ) &&
	                     testEpochAllocators( memory, nPage ) && testHandleAllocator( memory, nPage ) && testOtherAllocators() &&
	                     testSizeClasses() && testCompactBucket();
	if ( !bPassed ) {
		fputs( "SIR allocator migration tests failed\n", stderr );
		return 1;
	}
	return 0;
}
