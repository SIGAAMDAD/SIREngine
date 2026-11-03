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

#include <Core/Containers/SIRArray.h>
#include <Core/Containers/SIRFlatHashMap.h>
#include <Core/Containers/SIRFlatHashSet.h>
#include <Core/Containers/SIRBitVector.h>
#include <Core/Containers/SIRFixedBitset.h>
#include <Core/Containers/SIRString.h>
#include <Core/Containers/SIRStack.h>
#include <Core/Containers/SIRQueue.h>
#include <Core/Containers/SIRHashMap.h>
#include <Core/Containers/SIRHashSet.h>
#include <Core/Containers/SIRInlineVector.h>
#include <Core/Containers/SIRRingBuffer.h>
#include <Core/Containers/SIRStaticVector.h>
#include <Core/Containers/SIRVector.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_MSC_VER)
#include <malloc.h>
#endif
#include <limits>
#include <stdexcept>

// The test domain is deliberately not a shipping allocator.

static void sirTestCheck( bool bSuccess, const char *pszExpression, int nLine )
{
	if ( !bSuccess ) {
		fprintf( stderr, "SIR container test failed at line %d: %s\n", nLine, pszExpression );
		abort();
	}
}

#define SIR_TEST_CHECK(expression) sirTestCheck( ( expression ), #expression, __LINE__ )


typedef struct sirTestAllocator_s
{
	size_t nLiveBytes;
	size_t nPeakBytes;
	uint32_t nAllocations;
	uint32_t nDeallocations;
	uint32_t nFailAt;
} sirTestAllocator_t;

static void *testAllocate( void *pContext, size_t nBytes, size_t nAlignment )
{
	sirTestAllocator_t *pState = ( sirTestAllocator_t * )pContext;
	++pState->nAllocations;
	if ( pState->nFailAt != 0 && pState->nAllocations == pState->nFailAt ) {
		return nullptr;
	}
	if ( nAlignment < sizeof( void * ) ) {
		nAlignment = sizeof( void * );
	}
	void *pMemory = nullptr;
#if defined(_MSC_VER)
	pMemory = _aligned_malloc( nBytes, nAlignment );
	if ( pMemory == nullptr ) {
		return nullptr;
	}
#else
	if ( posix_memalign( &pMemory, nAlignment, nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	pState->nLiveBytes += nBytes;
	if ( pState->nLiveBytes > pState->nPeakBytes ) {
		pState->nPeakBytes = pState->nLiveBytes;
	}
	return pMemory;
}

static void testDeallocate( void *pContext, void *pMemory, size_t nBytes, size_t )
{
	sirTestAllocator_t *pState = ( sirTestAllocator_t * )pContext;
	++pState->nDeallocations;
	SIR_TEST_CHECK( nBytes <= pState->nLiveBytes );
	pState->nLiveBytes -= nBytes;
#if defined(_MSC_VER)
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

class SIRTestLiveObject
{
public:
	static int s_nLive;
	static int s_nDestroyed;
	int m_nValue;
	static bool s_bThrowOnCopy;

	explicit SIRTestLiveObject( int nValue ) : m_nValue( nValue ) { ++s_nLive; }
	SIRTestLiveObject( const SIRTestLiveObject &other ) : m_nValue( other.m_nValue )
	{
		if ( s_bThrowOnCopy ) {
			throw std::runtime_error( "copy injection" );
		}
		++s_nLive;
	}
	SIRTestLiveObject( SIRTestLiveObject &&other ) noexcept( false ) : m_nValue( other.m_nValue ) { ++s_nLive; }
	~SIRTestLiveObject( void ) noexcept { --s_nLive; ++s_nDestroyed; }
};
int SIRTestLiveObject::s_nLive = 0;
int SIRTestLiveObject::s_nDestroyed = 0;
bool SIRTestLiveObject::s_bThrowOnCopy = false;

typedef struct alignas( 128 ) sirTestAligned_s
{
	uint64_t nData[2];
} sirTestAligned_t;

static void testVectors( const sirContainerAllocator_t *pAllocator, sirTestAllocator_t *pState )
{
	{
		SIRVector<int> values( pAllocator );
		SIR_TEST_CHECK( sizeof( values ) == 24 );
		SIR_TEST_CHECK( values.tryReserve( 16 ) == SIR_CONTAINER_OK );
		for ( uint32_t iValue = 0; iValue < 1000; ++iValue ) {
			SIR_TEST_CHECK( values.tryEmplaceBack( ( int )iValue ) == SIR_CONTAINER_OK );
		}
		SIR_TEST_CHECK( values.getSize() == 1000 && values[427] == 427 );
		SIR_TEST_CHECK( values.tryGet( 1000 ) == nullptr );
		pState->nFailAt = pState->nAllocations + 1;
		SIR_TEST_CHECK( values.tryReserve( 3000 ) == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		SIR_TEST_CHECK( values.getSize() == 1000 && values[427] == 427 );
		SIR_TEST_CHECK( values.tryPushBack( values[0] ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( values[1000] == 0 );
		SIR_TEST_CHECK( values.tryPopBack() == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( values.tryEraseOrdered( 3 ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( values[3] == 4 && values.getSize() == 999 );
		SIR_TEST_CHECK( values.tryEraseSwap( 0 ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( values.getSize() == 998 );
		SIR_TEST_CHECK( values.tryResize( 1010 ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( values.getSize() == 1010 && values[1009] == 0 );
		SIR_TEST_CHECK( values.tryResize( 5 ) == SIR_CONTAINER_OK );
		values.clear();
		SIR_TEST_CHECK( values.tryPopBack() == SIR_CONTAINER_EMPTY );
	}
	{
		SIRVector<sirTestAligned_t> aligned( pAllocator );
		SIR_TEST_CHECK( aligned.tryEmplaceBack() == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( ( ( uintptr_t )aligned.data() % 128 ) == 0 );
	}
	{
		SIRVector<SIRTestLiveObject> objects( pAllocator );
		for ( int iValue = 0; iValue < 3; ++iValue ) {
			SIR_TEST_CHECK( objects.tryEmplaceBack( iValue ) == SIR_CONTAINER_OK );
		}
		SIRTestLiveObject::s_bThrowOnCopy = true;
		bool bCaught = false;
		try {
			objects.tryReserve( 128 );
		} catch ( const std::runtime_error & ) {
			bCaught = true;
		}
		SIRTestLiveObject::s_bThrowOnCopy = false;
		SIR_TEST_CHECK( bCaught && objects.getSize() == 3 );
		SIR_TEST_CHECK( objects[0].m_nValue == 0 && objects[2].m_nValue == 2 );
	}
	SIR_TEST_CHECK( SIRTestLiveObject::s_nLive == 0 );
	{
		SIRStaticVector<SIRTestLiveObject, 4> fixed;
		SIR_TEST_CHECK( fixed.tryReserve( 5 ) == SIR_CONTAINER_CAPACITY_EXCEEDED );
		for ( int iValue = 0; iValue < 4; ++iValue ) {
			SIR_TEST_CHECK( fixed.tryEmplaceBack( iValue ) == SIR_CONTAINER_OK );
		}
		SIR_TEST_CHECK( fixed.tryEmplaceBack( 10 ) == SIR_CONTAINER_CAPACITY_EXCEEDED );
		fixed.clear();
	}
	SIR_TEST_CHECK( SIRTestLiveObject::s_nLive == 0 );
	{
		SIRInlineVector<int, 8> small( pAllocator );
		uint32_t nBefore = pState->nAllocations;
		for ( int iValue = 0; iValue < 8; ++iValue ) {
			SIR_TEST_CHECK( small.tryEmplaceBack( iValue ) == SIR_CONTAINER_OK );
		}
		SIR_TEST_CHECK( small.usesInlineStorage() && pState->nAllocations == nBefore );
		SIR_TEST_CHECK( small.tryPushBack( small[2] ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( !small.usesInlineStorage() && small[8] == 2 );
		SIR_TEST_CHECK( small.tryResize( 12 ) == SIR_CONTAINER_OK && small[11] == 0 );
		SIR_TEST_CHECK( small.tryEraseOrdered( 2 ) == SIR_CONTAINER_OK && small[2] == 3 );
	}
	{
		SIRVector<int> noDomain( nullptr );
		SIR_TEST_CHECK( noDomain.tryPushBack( 1 ) == SIR_CONTAINER_INVALID_ARGUMENT );
		SIR_TEST_CHECK( noDomain.getSize() == 0 );
	}
	{
		SIRArray<int, 4> array;
		SIR_TEST_CHECK( array.getSize() == 4 && array[0] == 0 );
		SIR_TEST_CHECK( array.tryGet( 4 ) == nullptr );
		SIRArray<int, 0> empty;
		SIR_TEST_CHECK( empty.getSize() == 0 && empty.data() == nullptr && empty.tryGet( 0 ) == nullptr );
	}
}

static void testAssociative( const sirContainerAllocator_t *pAllocator, sirTestAllocator_t *pState )
{
	{
		SIRHashMap<uint32_t, uint32_t> map( pAllocator );
		SIR_TEST_CHECK( map.tryReserve( 4000 ) == SIR_CONTAINER_OK );
		for ( uint32_t iKey = 0; iKey < 4000; ++iKey ) {
			bool bInserted = false;
			SIR_TEST_CHECK( map.tryInsert( iKey, iKey * 3, &bInserted ) == SIR_CONTAINER_OK && bInserted );
		}
		SIR_TEST_CHECK( map.getSize() == 4000 );
		for ( uint32_t iKey = 0; iKey < 4000; ++iKey ) {
			const uint32_t *pFound = map.find( iKey );
			SIR_TEST_CHECK( pFound != nullptr && *pFound == iKey * 3 );
		}
		bool bInserted = true;
		SIR_TEST_CHECK( map.tryInsert( 3u, 99u, &bInserted ) == SIR_CONTAINER_OK && !bInserted );
		SIR_TEST_CHECK( *map.find( 3u ) == 9u );
		for ( uint32_t iKey = 0; iKey < 4000; iKey += 2 ) {
			SIR_TEST_CHECK( map.erase( iKey ) );
		}
		SIR_TEST_CHECK( map.getSize() == 2000 && !map.erase( 2u ) );
		for ( uint32_t iKey = 4000; iKey < 9000; ++iKey ) {
			SIR_TEST_CHECK( map.tryInsert( iKey, iKey + 1 ) == SIR_CONTAINER_OK );
		}
		SIR_TEST_CHECK( map.getSize() == 7000 );
		for ( uint32_t iKey = 4001; iKey < 9000; ++iKey ) {
			SIR_TEST_CHECK( *map.find( iKey ) == iKey + 1 );
		}
		pState->nFailAt = pState->nAllocations + 1;
		SIR_TEST_CHECK( map.tryReserve( 50000 ) == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		SIR_TEST_CHECK( *map.find( 8999u ) == 9000u );
		map.clear();
		SIR_TEST_CHECK( map.getSize() == 0 && map.find( 5u ) == nullptr );
	}
	{
		SIRHashMap<uint32_t, SIRTestLiveObject> objects( pAllocator );
		for ( uint32_t iValue = 0; iValue < 8; ++iValue ) {
			SIR_TEST_CHECK( objects.tryInsert( ( uint32_t )iValue, SIRTestLiveObject( ( int )iValue + 10 ) ) == SIR_CONTAINER_OK );
		}
		SIRTestLiveObject::s_bThrowOnCopy = true;
		bool bCaught = false;
		try {
			objects.tryReserve( 200 );
		} catch ( const std::runtime_error & ) {
			bCaught = true;
		}
		SIRTestLiveObject::s_bThrowOnCopy = false;
		SIR_TEST_CHECK( bCaught && objects.getSize() == 8 && objects.find( 3u )->m_nValue == 13 );
	}
	SIR_TEST_CHECK( SIRTestLiveObject::s_nLive == 0 );
	{
		SIRHashMap<uint32_t, uint32_t> alias( pAllocator );
		SIR_TEST_CHECK( alias.tryReserve( 1 ) == SIR_CONTAINER_OK );
		for ( uint32_t iKey = 0; iKey < 7; ++iKey ) {
			SIR_TEST_CHECK( alias.tryInsert( iKey, iKey + 300 ) == SIR_CONTAINER_OK );
		}
		const uint32_t *pOldValue = alias.find( 3u );
		SIR_TEST_CHECK( pOldValue != nullptr );
		SIR_TEST_CHECK( alias.tryEmplace( 100u, *pOldValue ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( *alias.find( 100u ) == 303u );
	}
	{
		SIRHashSet<uint32_t> set( pAllocator );
		for ( uint32_t iKey = 0; iKey < 4000; ++iKey ) {
			SIR_TEST_CHECK( set.tryInsert( iKey ) == SIR_CONTAINER_OK );
		}
		SIR_TEST_CHECK( set.getSize() == 4000 && set.contains( 213u ) );
		for ( uint32_t iKey = 0; iKey < 4000; iKey += 2 ) {
			SIR_TEST_CHECK( set.erase( iKey ) );
		}
		SIR_TEST_CHECK( !set.contains( 214u ) && set.contains( 213u ) );
		for ( uint32_t iKey = 4000; iKey < 8500; ++iKey ) {
			SIR_TEST_CHECK( set.tryInsert( iKey ) == SIR_CONTAINER_OK );
		}
		SIR_TEST_CHECK( set.getSize() == 6500 );
	}
}

static void testRing( const sirContainerAllocator_t *pAllocator )
{
	SIRRingBuffer<SIRTestLiveObject> ring( pAllocator );
	SIR_TEST_CHECK( ring.tryInitialize( 3 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( ring.tryInitialize( 4 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	for ( int iValue = 0; iValue < 3; ++iValue ) {
		SIR_TEST_CHECK( ring.tryEmplaceBack( iValue ) == SIR_CONTAINER_OK );
	}
	SIR_TEST_CHECK( ring.tryEmplaceBack( 4 ) == SIR_CONTAINER_FULL );
	SIR_TEST_CHECK( ring.front()->m_nValue == 0 );
	SIR_TEST_CHECK( ring.tryPopFront() == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( ring.tryEmplaceBack( 9 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( ring.front()->m_nValue == 1 );
	SIR_TEST_CHECK( ring.tryPopFront() == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( ring.front()->m_nValue == 2 );
	SIR_TEST_CHECK( ring.tryPopFront() == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( ring.front()->m_nValue == 9 );
	SIR_TEST_CHECK( ring.tryPopFront() == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( ring.tryPopFront() == SIR_CONTAINER_EMPTY );
	SIR_TEST_CHECK( ring.tryEmplaceFront( 50 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( ring.front()->m_nValue == 50 && ring.back()->m_nValue == 50 );
	SIR_TEST_CHECK( ring.tryPopBack() == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( ring.tryPopBack() == SIR_CONTAINER_EMPTY );
}

static void testStringsAndBits( const sirContainerAllocator_t *pAllocator )
{
	SIRString str( pAllocator );
	SIR_TEST_CHECK( str.getByteLength() == 0 && str.cStr()[0] == '\0' );
	SIR_TEST_CHECK( str.tryAppendBytes( "hello", 5 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( str.tryAppendBytes( str.cStr(), 5 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( str.getByteLength() == 10 && memcmp( str.cStr(), "hellohello", 10 ) == 0 );
	SIR_TEST_CHECK( str.tryAssignBytes( str.cStr() + 5, 5 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( str.getByteLength() == 5 && strcmp( str.cStr(), "hello" ) == 0 );
	SIR_TEST_CHECK( str.tryAppendBytes( nullptr, 1 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	str.clear();
	SIR_TEST_CHECK( str.getByteLength() == 0 && str.cStr()[0] == '\0' );

	SIRBitVector bits( pAllocator );
	SIR_TEST_CHECK( bits.tryResize( 129 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( bits.trySet( 0, true ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( bits.trySet( 65, true ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( bits.trySet( 128, true ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( bits.test( 65 ) && !bits.test( 64 ) );
	SIR_TEST_CHECK( bits.count() == 3 && bits.findFirstSet() == 0 );
	SIR_TEST_CHECK( bits.tryResize( 64 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( bits.count() == 1 && !bits.test( 65 ) );
	SIR_TEST_CHECK( bits.trySet( 64, true ) == SIR_CONTAINER_INVALID_ARGUMENT );
	bits.clear();
	SIR_TEST_CHECK( bits.count() == 0 && bits.findFirstSet() == UINT32_MAX );

	SIRStack<int> stack( pAllocator );
	SIR_TEST_CHECK( stack.tryPush( 3 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( stack.tryPush( 9 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( *stack.peek() == 9 );
	SIR_TEST_CHECK( stack.tryPop() == SIR_CONTAINER_OK && *stack.peek() == 3 );
	SIR_TEST_CHECK( stack.tryPop() == SIR_CONTAINER_OK && stack.tryPop() == SIR_CONTAINER_EMPTY );

	SIRQueue<int> queue( pAllocator );
	SIR_TEST_CHECK( queue.tryInitialize( 2 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( queue.tryPush( 3 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( queue.tryPush( 9 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( queue.tryPush( 18 ) == SIR_CONTAINER_FULL );
	SIR_TEST_CHECK( *queue.front() == 3 );
	SIR_TEST_CHECK( queue.tryPop() == SIR_CONTAINER_OK && *queue.front() == 9 );

	SIRFixedBitset<130> fixed;
	SIR_TEST_CHECK( fixed.trySet( 129, true ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( fixed.trySet( 1, true ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( fixed.count() == 2 && fixed.findFirstSet() == 1 );
	SIR_TEST_CHECK( fixed.trySet( 130, true ) == SIR_CONTAINER_INVALID_ARGUMENT );
	fixed.clear();
	SIR_TEST_CHECK( fixed.count() == 0 );
}

int main( void )
{
	sirTestAllocator_t state = {};
	sirContainerAllocator_t allocator = { &state, testAllocate, testDeallocate, 1u, 2u };
	testVectors( &allocator, &state );
	testAssociative( &allocator, &state );
	testRing( &allocator );
	testStringsAndBits( &allocator );
	SIR_TEST_CHECK( state.nLiveBytes == 0 );
	SIR_TEST_CHECK( state.nAllocations == state.nDeallocations + 2 ); // two injected allocation failures
	SIR_TEST_CHECK( SIRTestLiveObject::s_nLive == 0 );
	return 0;
}
