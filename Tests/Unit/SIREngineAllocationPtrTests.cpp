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

#include <Core/Memory/Allocators/SIRAllocationHandle.h>
#include <Core/Memory/Pointers/SIRArenaPtr.h>
#include <Core/Memory/Pointers/SIRFramePtr.h>
#include <Core/Memory/Pointers/SIRPoolPtr.h>
#include <Core/Memory/Pointers/SIRSelfRelativePtr.h>
#include <Core/Memory/Allocators/SIRPoolAllocator.h>
#include <Core/Memory/Allocators/SIRFrameAllocator.h>
#include <Core/Memory/Allocators/SIRLinearAllocator.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>
#include <utility>

class SIRPoolTestObject
{
public:
	explicit SIRPoolTestObject( uint32_t *pDestroyed, uint32_t nValue ) noexcept : m_pDestroyed( pDestroyed ), m_nValue( nValue )
	{
	}
	~SIRPoolTestObject( void ) noexcept
	{
		++*m_pDestroyed;
	}
	uint32_t getValue( void ) const noexcept
	{
		return m_nValue;
	}

private:
	uint32_t *m_pDestroyed;
	uint32_t m_nValue;
};

typedef struct alignas( 32 ) sirTransientTestValue_s
{
	uint32_t nValue;
	explicit sirTransientTestValue_s( uint32_t nInput ) noexcept : nValue( nInput )
	{
	}
} sirTransientTestValue_t;

typedef struct sirRelocationTestBlob_s
{
	SIRSelfRelativePtr<uint32_t> pTarget;
	uint32_t nValue;
} sirRelocationTestBlob_t;

#define SIR_TEST_CHECK( condition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Allocation pointer test failed at %d: %s\n", __LINE__, #condition );                                         \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

#define ASSERT_TRUE( condition ) SIR_TEST_CHECK( ( condition ) )
#define ASSERT_FALSE( condition ) SIR_TEST_CHECK( !( condition ) )
#define ASSERT_NE( a, b ) SIR_TEST_CHECK( ( a ) != ( b ) )
#define EXPECT_TRUE( condition ) SIR_TEST_CHECK( ( condition ) )
#define EXPECT_FALSE( condition ) SIR_TEST_CHECK( !( condition ) )
#define EXPECT_EQ( a, b ) SIR_TEST_CHECK( ( a ) == ( b ) )

static_assert( !std::is_copy_constructible<SIRPoolPtr<SIRPoolTestObject>>::value, "Pool ownership cannot be copied" );
static_assert( std::is_move_constructible<SIRPoolPtr<SIRPoolTestObject>>::value, "Pool ownership can move" );
#define SIR_TEST_CHECK( condition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Allocation pointer test failed at %d: %s\n", __LINE__, #condition );                                         \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

#define ASSERT_TRUE( condition ) SIR_TEST_CHECK( ( condition ) )
#define ASSERT_FALSE( condition ) SIR_TEST_CHECK( !( condition ) )
#define ASSERT_NE( a, b ) SIR_TEST_CHECK( ( a ) != ( b ) )
#define EXPECT_TRUE( condition ) SIR_TEST_CHECK( ( condition ) )
#define EXPECT_FALSE( condition ) SIR_TEST_CHECK( !( condition ) )
#define EXPECT_EQ( a, b ) SIR_TEST_CHECK( ( a ) == ( b ) )

static_assert( !std::is_copy_constructible<SIRAllocationPin<uint32_t>>::value, "Heap pins cannot be copied" );
static_assert( std::is_trivially_copyable<sirRelocationTestBlob_t>::value, "Cooked blobs must remain trivially copyable" );

static void sirTestPoolOwnershipAndSlotReuse( void )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "ptr.pool", 8 );
	ASSERT_NE( pSegment, nullptr );
	SIRPoolAllocator pool;
	ASSERT_TRUE( pool.init( pSegment, sizeof( SIRPoolTestObject ), alignof( SIRPoolTestObject ) ) );

	uint32_t nDestroyed = 0;
	{
		auto first = SIRPoolPtr<SIRPoolTestObject>::create( &pool, &nDestroyed, 41 );
		ASSERT_TRUE( first );
		EXPECT_EQ( first->getValue(), 41u );
		EXPECT_EQ( pool.getLiveSlots(), 1u );
		auto second = std::move( first );
		EXPECT_FALSE( first );
		EXPECT_TRUE( second );
		second.reset();
		EXPECT_EQ( nDestroyed, 1u );
		EXPECT_EQ( pool.getLiveSlots(), 0u );
	}
	{
		auto again = SIRPoolPtr<SIRPoolTestObject>::create( &pool, &nDestroyed, 42 );
		ASSERT_TRUE( again );
		EXPECT_EQ( again->getValue(), 42u );
	}
	EXPECT_EQ( nDestroyed, 2u );
	EXPECT_EQ( pool.getLiveSlots(), 0u );
	EXPECT_TRUE( pool.shutdown() );
}

static void sirTestArenaGenerationRejectsRewindResetAndReinitialize( void )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "ptr.arena", 16 );
	ASSERT_NE( pSegment, nullptr );
	SIRLinearAllocator allocator;
	ASSERT_TRUE( allocator.init( pSegment ) );

	const uint64_t nStart = allocator.getMark();
	auto first = SIRArenaPtr<sirTransientTestValue_t>::create( &allocator, 13u );
	ASSERT_TRUE( first );
	EXPECT_EQ( first->nValue, 13u );
	EXPECT_TRUE( allocator.rewind( nStart ) );
	EXPECT_FALSE( first );
	EXPECT_EQ( first.get(), nullptr );

	auto second = SIRArenaPtr<sirTransientTestValue_t>::create( &allocator, 19u );
	ASSERT_TRUE( second );
	EXPECT_FALSE( first );
	EXPECT_TRUE( allocator.reset() );
	EXPECT_FALSE( second );
	EXPECT_TRUE( allocator.shutdown() );
	ASSERT_TRUE( allocator.init( pSegment ) );
	auto third = SIRArenaPtr<sirTransientTestValue_t>::create( &allocator, 29u );
	ASSERT_TRUE( third );
	EXPECT_FALSE( second );
	EXPECT_TRUE( allocator.reset() );
	EXPECT_TRUE( allocator.shutdown() );
}

static void sirTestFrameEpochAndAllocatorLifecycle( void )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *szSegments[2] = { memory.segment( "ptr.frame0", 16 ), memory.segment( "ptr.frame1", 16 ) };
	ASSERT_NE( szSegments[0], nullptr );
	ASSERT_NE( szSegments[1], nullptr );
	SIRFrameAllocator allocator;
	ASSERT_TRUE( allocator.init( szSegments, 2 ) );

	EXPECT_FALSE( SIRFramePtr<sirTransientTestValue_t>::create( &allocator, 2u ) );
	ASSERT_TRUE( allocator.beginEpoch( 11, 0 ) );
	auto first = SIRFramePtr<sirTransientTestValue_t>::create( &allocator, 51u );
	ASSERT_TRUE( first );
	EXPECT_EQ( first->nValue, 51u );
	EXPECT_EQ( first.getEpoch(), 11u );
	ASSERT_TRUE( allocator.submitEpoch( 11 ) );
	EXPECT_FALSE( first );
	ASSERT_TRUE( allocator.releaseCompleted( 11 ) );
	ASSERT_TRUE( allocator.shutdown() );
	ASSERT_TRUE( allocator.init( szSegments, 2 ) );
	ASSERT_TRUE( allocator.beginEpoch( 11, 0 ) );
	EXPECT_FALSE( first );
	ASSERT_TRUE( allocator.submitEpoch( 11 ) );
	ASSERT_TRUE( allocator.releaseCompleted( 11 ) );
	ASSERT_TRUE( allocator.shutdown() );
}

static void sirTestSelfRelativeRebasesWhenBlobIsRelocated( void )
{
	sirRelocationTestBlob_t source = {};
	source.nValue = 57;
	ASSERT_TRUE( source.pTarget.set( &source.nValue ) );
	EXPECT_EQ( source.pTarget.get(), &source.nValue );
	EXPECT_EQ( source.pTarget.getOffset(), ( int32_t )offsetof( sirRelocationTestBlob_t, nValue ) );

	sirRelocationTestBlob_t relocated = {};
	memcpy( &relocated, &source, sizeof( source ) );
	EXPECT_EQ( relocated.pTarget.get(), &relocated.nValue );
	EXPECT_EQ( *relocated.pTarget, 57u );
	const int32_t nPrevious = relocated.pTarget.getOffset();
	EXPECT_FALSE( relocated.pTarget.set( ( uint32_t * )( uintptr_t )0x1000 ) );
	EXPECT_EQ( relocated.pTarget.getOffset(), nPrevious );
	ASSERT_TRUE( relocated.pTarget.set( nullptr ) );
	EXPECT_EQ( relocated.pTarget.get(), nullptr );

	sirRelocationTestBlob_t backwards = {};
	SIRSelfRelativePtr<uint32_t> *pBackward = &backwards.pTarget;
	uint32_t nPrior = 105;
	// The target is allowed on either side of the embedded relative pointer.
	ASSERT_TRUE( pBackward->set( &nPrior ) );
	EXPECT_EQ( pBackward->get(), &nPrior );
}

static void sirTestHandleIsGenerationalAndPinnedAgainstCompaction( void )
{
	SIRTestMemory memory;
	ASSERT_TRUE( memory.ready() );
	SIRMemorySegment *pSegment = memory.segment( "ptr.handle", 32 );
	ASSERT_NE( pSegment, nullptr );
	SIRHandleHeap heap;
	ASSERT_TRUE( heap.init( pSegment, 32 ) );

	auto first = SIRAllocationHandle<sirTransientTestValue_t>::allocate( &heap, 23u );
	ASSERT_TRUE( first.isValid() );
	auto stale = first;
	{
		auto guard = first.pin();
		ASSERT_TRUE( guard );
		EXPECT_EQ( guard->nValue, 23u );
		EXPECT_FALSE( first.release() );
		EXPECT_TRUE( heap.compact() );
		EXPECT_EQ( guard->nValue, 23u );
		guard->nValue = 31u;
	}
	{
		auto guard = stale.pin();
		ASSERT_TRUE( guard );
		EXPECT_EQ( guard->nValue, 31u );
	}
	EXPECT_TRUE( first.release() );
	EXPECT_FALSE( stale.isValid() );
	EXPECT_FALSE( stale.pin() );
	EXPECT_FALSE( stale.release() );

	auto second = SIRAllocationHandle<sirTransientTestValue_t>::allocate( &heap, 62u );
	ASSERT_TRUE( second.isValid() );
	EXPECT_FALSE( stale.isValid() );
	EXPECT_TRUE( second.release() );
	EXPECT_TRUE( heap.shutdown() );
	ASSERT_TRUE( heap.init( pSegment, 32 ) );
	const auto afterReinit = SIRAllocationHandle<sirTransientTestValue_t>::allocate( &heap, 72u );
	ASSERT_TRUE( afterReinit.isValid() );
	EXPECT_FALSE( stale.isValid() );
	EXPECT_FALSE( second.isValid() );
	EXPECT_FALSE( stale.pin() );
	auto newOwner = afterReinit;
	EXPECT_TRUE( newOwner.release() );
	EXPECT_TRUE( heap.shutdown() );
}

int main( void )
{
	sirTestPoolOwnershipAndSlotReuse();
	sirTestArenaGenerationRejectsRewindResetAndReinitialize();
	sirTestFrameEpochAndAllocatorLifecycle();
	sirTestSelfRelativeRebasesWhenBlobIsRelocated();
	sirTestHandleIsGenerationalAndPinnedAgainstCompaction();
	puts( "Engine allocation pointer tests passed" );
	return 0;
}
