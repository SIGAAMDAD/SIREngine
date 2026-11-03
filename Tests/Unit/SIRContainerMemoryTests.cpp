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

#include <Core/Containers/SIRFlatHashMap.h>
#include <Core/Containers/SIRFlatHashSet.h>
#include <Core/Containers/SIRString.h>
#include <Core/Containers/SIRBitVector.h>
#include <Core/Containers/SIRRingBuffer.h>
#include <Core/Containers/SIRPriorityQueue.h>
#include <Core/Containers/SIRStack.h>
#include <Core/Containers/SIRQueue.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>
#if defined( _MSC_VER )
#include <malloc.h>
#endif

#define SIR_CHECK( bCondition )                                                                                                            \
	do {                                                                                                                                  \
		if ( !( bCondition ) ) {                                                                                                           \
			fprintf( stderr, "Container memory test failure at line %d: %s\n", __LINE__, #bCondition );                                \
			abort();                                                                                                                     \
		}                                                                                                                                 \
	} while ( 0 )

typedef struct sirMemoryTestState_s
{
	size_t nLiveBytes;
	size_t nPeakBytes;
	uint32_t nAllocations;
	uint32_t nFailAt;
} sirMemoryTestState_t;

static void *sirMemoryTestAllocate( void *pContext, size_t nBytes, size_t nAlignment )
{
	sirMemoryTestState_t *pState = ( sirMemoryTestState_t * )pContext;
	++pState->nAllocations;
	if ( pState->nFailAt == pState->nAllocations ) {
		return nullptr;
	}
	if ( nAlignment < sizeof( void * ) ) {
		nAlignment = sizeof( void * );
	}
	void *pResult = nullptr;
#if defined( _MSC_VER )
	pResult = _aligned_malloc( nBytes, nAlignment );
#else
	if ( posix_memalign( &pResult, nAlignment, nBytes ) != 0 ) {
		pResult = nullptr;
	}
#endif
	if ( pResult != nullptr ) {
		pState->nLiveBytes += nBytes;
		if ( pState->nLiveBytes > pState->nPeakBytes ) {
			pState->nPeakBytes = pState->nLiveBytes;
		}
	}
	return pResult;
}

static void sirMemoryTestDeallocate( void *pContext, void *pMemory, size_t nBytes, size_t )
{
	sirMemoryTestState_t *pState = ( sirMemoryTestState_t * )pContext;
	SIR_CHECK( pState->nLiveBytes >= nBytes );
	pState->nLiveBytes -= nBytes;
#if defined( _MSC_VER )
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

static void sirTestCompactMap( const sirContainerAllocator_t *pAllocator, sirMemoryTestState_t *pState )
{
	using FastMap = SIRFlatHashMap<uint32_t, uint32_t>;
	using CompactMap = SIRFlatHashMap<uint32_t, uint32_t, std::hash<uint32_t>, std::equal_to<uint32_t>, true>;
	SIR_CHECK( sizeof( FastMap ) == sizeof( CompactMap ) );
	{
		FastMap fast( pAllocator );
		CompactMap compact( pAllocator );
		SIR_CHECK( fast.tryReserve( 32768 ) == SIR_CONTAINER_OK );
		size_t nFastBytes = pState->nLiveBytes;
		SIR_CHECK( compact.tryReserve( 32768 ) == SIR_CONTAINER_OK );
		size_t nCompactBytes = pState->nLiveBytes - nFastBytes;
		SIR_CHECK( nFastBytes == 589824 && nCompactBytes == 540672 );
		for ( uint32_t iIndex = 0; iIndex < 32768; ++iIndex ) {
			uint32_t nKey = iIndex * 65536u + 31u;
			SIR_CHECK( fast.tryInsert( nKey, iIndex + 10 ) == SIR_CONTAINER_OK );
			SIR_CHECK( compact.tryInsert( nKey, iIndex + 10 ) == SIR_CONTAINER_OK );
		}
		for ( uint32_t iIndex = 0; iIndex < 32768; ++iIndex ) {
			uint32_t nKey = iIndex * 65536u + 31u;
			SIR_CHECK( fast.find( nKey ) != nullptr && *fast.find( nKey ) == iIndex + 10 );
			SIR_CHECK( compact.find( nKey ) != nullptr && *compact.find( nKey ) == iIndex + 10 );
		}
		for ( uint32_t iIndex = 0; iIndex < 28672; ++iIndex ) {
			SIR_CHECK( fast.erase( iIndex * 65536u + 31u ) );
			SIR_CHECK( compact.erase( iIndex * 65536u + 31u ) );
		}
		SIR_CHECK( fast.getSize() == 4096 && compact.getSize() == 4096 );
		pState->nFailAt = pState->nAllocations + 1;
		SIR_CHECK( compact.tryShrinkToFit() == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		SIR_CHECK( compact.getCapacity() == 65536 );
		SIR_CHECK( *compact.find( 28672u * 65536u + 31u ) == 28682u );
		SIR_CHECK( compact.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( fast.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( compact.getCapacity() == 8192 && fast.getCapacity() == 8192 );
		SIR_CHECK( compact.getFootprint().nLiveDomainBytes == 67584 );
		SIR_CHECK( fast.getFootprint().nLiveDomainBytes == 73728 );
		for ( uint32_t iIndex = 28672; iIndex < 32768; ++iIndex ) {
			uint32_t nKey = iIndex * 65536u + 31u;
			SIR_CHECK( *compact.find( nKey ) == iIndex + 10 );
			SIR_CHECK( *fast.find( nKey ) == iIndex + 10 );
		}
		compact.clear();
		SIR_CHECK( compact.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( compact.getCapacity() == 0 && compact.getFootprint().nLiveDomainBytes == 0 );
		SIR_CHECK( compact.tryInsert( 5u, 7u ) == SIR_CONTAINER_OK );
		SIR_CHECK( *compact.find( 5u ) == 7u );
	}
	SIR_CHECK( pState->nLiveBytes == 0 );
}

static void sirTestCompactSet( const sirContainerAllocator_t *pAllocator, sirMemoryTestState_t *pState )
{
	using FastSet = SIRFlatHashSet<uint32_t>;
	using CompactSet = SIRFlatHashSet<uint32_t, std::hash<uint32_t>, std::equal_to<uint32_t>, true>;
	{
		FastSet fast( pAllocator );
		CompactSet compact( pAllocator );
		SIR_CHECK( fast.tryReserve( 32768 ) == SIR_CONTAINER_OK );
		size_t nFastBytes = pState->nLiveBytes;
		SIR_CHECK( compact.tryReserve( 32768 ) == SIR_CONTAINER_OK );
		size_t nCompactBytes = pState->nLiveBytes - nFastBytes;
		SIR_CHECK( nFastBytes == 327680 && nCompactBytes == 278528 );
		for ( uint32_t iIndex = 0; iIndex < 32768; ++iIndex ) {
			uint32_t nKey = iIndex * 65536u + 31u;
			SIR_CHECK( fast.tryInsert( nKey ) == SIR_CONTAINER_OK );
			SIR_CHECK( compact.tryInsert( nKey ) == SIR_CONTAINER_OK );
		}
		for ( uint32_t iIndex = 0; iIndex < 28672; ++iIndex ) {
			SIR_CHECK( fast.erase( iIndex * 65536u + 31u ) );
			SIR_CHECK( compact.erase( iIndex * 65536u + 31u ) );
		}
		SIR_CHECK( compact.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( fast.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( compact.getFootprint().nLiveDomainBytes == 34816 );
		SIR_CHECK( fast.getFootprint().nLiveDomainBytes == 40960 );
		for ( uint32_t iIndex = 28672; iIndex < 32768; ++iIndex ) {
			uint32_t nKey = iIndex * 65536u + 31u;
			SIR_CHECK( compact.contains( nKey ) && fast.contains( nKey ) );
		}
		SIR_CHECK( compact.tryInsert( 999u ) == SIR_CONTAINER_OK );
		SIR_CHECK( compact.contains( 999u ) );
		compact.clear();
		SIR_CHECK( compact.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( compact.getFootprint().nLiveDomainBytes == 0 );
	}
	SIR_CHECK( pState->nLiveBytes == 0 );
}

static void sirTestSmallString( const sirContainerAllocator_t *pAllocator, sirMemoryTestState_t *pState )
{
	SIR_CHECK( sizeof( SIRString ) <= 40 || sizeof( void * ) != 8 );
	{
		SIRString value( pAllocator );
		SIR_CHECK( value.getCapacity() == 24 );
		SIR_CHECK( value.tryAppendBytes( "abcdefghijklmnopqrstuvw", 23 ) == SIR_CONTAINER_OK );
		SIR_CHECK( value.getByteLength() == 23 && pState->nLiveBytes == 0 );
		SIR_CHECK( value.tryAppendBytes( value.cStr(), 23 ) == SIR_CONTAINER_OK );
		SIR_CHECK( value.getByteLength() == 46 && pState->nLiveBytes != 0 );
		SIRString moved( std::move( value ) );
		SIR_CHECK( value.getByteLength() == 0 && moved.getByteLength() == 46 );
		SIR_CHECK( moved.tryAssignBytes( moved.cStr() + 2, 11 ) == SIR_CONTAINER_OK );
		SIR_CHECK( moved.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( moved.getByteLength() == 11 && moved.getCapacity() == 24 && pState->nLiveBytes == 0 );
		SIR_CHECK( moved.tryAppendBytes( "12345678901234567890", 20 ) == SIR_CONTAINER_OK );
		size_t nBefore = pState->nLiveBytes;
		pState->nFailAt = pState->nAllocations + 1;
		SIR_CHECK( moved.tryShrinkToFit() == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		SIR_CHECK( pState->nLiveBytes == nBefore && moved.getByteLength() == 31 );
		SIR_CHECK( moved.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( moved.getCapacity() == 32 && moved.getByteLength() == 31 );
		SIRString copy( pAllocator );
		copy = std::move( moved );
		SIR_CHECK( copy.getByteLength() == 31 && moved.getByteLength() == 0 );
	}
	SIR_CHECK( pState->nLiveBytes == 0 );
}

static void sirTestAdapterReclamation( const sirContainerAllocator_t *pAllocator, sirMemoryTestState_t *pState )
{
	{
		SIRBitVector bits( pAllocator );
		SIR_CHECK( bits.tryResize( 32768 ) == SIR_CONTAINER_OK );
		SIR_CHECK( pState->nLiveBytes >= 4096 );
		SIR_CHECK( bits.tryResize( 64 ) == SIR_CONTAINER_OK );
		SIR_CHECK( bits.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( pState->nLiveBytes == 8 );
		bits.clearAndRelease();
		SIR_CHECK( pState->nLiveBytes == 0 && bits.getSize() == 0 );
	}
	{
		SIRRingBuffer<uint32_t> ring( pAllocator );
		SIR_CHECK( ring.tryInitialize( 16384 ) == SIR_CONTAINER_OK );
		SIR_CHECK( ring.tryPushBack( 11 ) == SIR_CONTAINER_OK );
		ring.clearAndRelease();
		SIR_CHECK( ring.getCapacity() == 0 && pState->nLiveBytes == 0 );
		SIR_CHECK( ring.tryInitialize( 32 ) == SIR_CONTAINER_OK );
		SIR_CHECK( ring.tryPushBack( 9 ) == SIR_CONTAINER_OK );
	}
	SIR_CHECK( pState->nLiveBytes == 0 );
	{
		SIRPriorityQueue<uint32_t> heap( pAllocator );
		for ( uint32_t iIndex = 0; iIndex < 1024; ++iIndex ) {
			SIR_CHECK( heap.tryPush( iIndex ) == SIR_CONTAINER_OK );
		}
		heap.clear();
		SIR_CHECK( heap.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( pState->nLiveBytes == 0 );
	}
	{
		SIRStack<uint32_t> stack( pAllocator );
		for ( uint32_t iIndex = 0; iIndex < 256; ++iIndex ) {
			SIR_CHECK( stack.tryPush( iIndex ) == SIR_CONTAINER_OK );
		}
		stack.clear();
		SIR_CHECK( stack.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( pState->nLiveBytes == 0 );
	}
	{
		SIRQueue<uint32_t> queue( pAllocator );
		SIR_CHECK( queue.tryInitialize( 4096 ) == SIR_CONTAINER_OK );
		SIR_CHECK( queue.tryPush( 4 ) == SIR_CONTAINER_OK );
		queue.clearAndRelease();
		SIR_CHECK( pState->nLiveBytes == 0 && queue.getCapacity() == 0 );
	}
}

int main( void )
{
	sirMemoryTestState_t state = {};
	sirContainerAllocator_t allocator = { &state, sirMemoryTestAllocate, sirMemoryTestDeallocate, 1u, 2u };
	sirTestCompactMap( &allocator, &state );
	sirTestCompactSet( &allocator, &state );
	sirTestSmallString( &allocator, &state );
	sirTestAdapterReclamation( &allocator, &state );
	SIR_CHECK( state.nLiveBytes == 0 );
	return 0;
}
