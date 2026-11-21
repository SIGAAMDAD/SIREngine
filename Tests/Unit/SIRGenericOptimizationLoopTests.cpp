/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRInlineVector.h>
#include <Core/Containers/SIRString.h>
#include <Core/Containers/SIRRingBuffer.h>
#include <Core/Containers/SIRQueue.h>
#include <Core/Containers/SIRBitVector.h>
#include <Core/Containers/SIRFixedBitset.h>
#include <Core/Containers/SIRPagedArray.h>
#include <Core/Containers/SIRPriorityQueue.h>
#include <deque>
#include <vector>
#include <string>
#include <random>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void require( bool bValue ) noexcept
{
	if ( !bValue ) {
		abort();
	}
}

struct sirTestDomain_t
{
	uint64_t nLiveBytes = 0;
	uint64_t nAllocations = 0;
	uint64_t nFailAt = 0;
};

static void *testAllocate( void *pContext, size_t nSize, size_t nAlignment )
{
	sirTestDomain_t *pState = ( sirTestDomain_t * )pContext;
	++pState->nAllocations;
	if ( pState->nFailAt == pState->nAllocations ) {
		return nullptr;
	}
	if ( nAlignment < sizeof( void * ) ) {
		nAlignment = sizeof( void * );
	}
	void *pMemory = nullptr;
	if ( posix_memalign( &pMemory, nAlignment, nSize ) != 0 ) {
		return nullptr;
	}
	pState->nLiveBytes += nSize;
	return pMemory;
}

static void testDeallocate( void *pContext, void *pMemory, size_t nSize, size_t ) noexcept
{
	sirTestDomain_t *pState = ( sirTestDomain_t * )pContext;
	require( pState->nLiveBytes >= nSize );
	pState->nLiveBytes -= nSize;
	free( pMemory );
}

struct sirWideValue_t
{
	uint64_t szData[8];
	sirWideValue_t( uint64_t nValue = 0 ) noexcept
	    : szData{ nValue, nValue + 1, nValue + 2, nValue + 3, nValue + 4, nValue + 5, nValue + 6, nValue + 7 }
	{
	}
	bool operator==( const sirWideValue_t& other ) const noexcept
	{
		for ( uint32_t iPart = 0; iPart < 8; ++iPart ) {
			if ( szData[iPart] != other.szData[iPart] ) {
				return false;
			}
		}
		return true;
	}
};

struct sirNontrivial_t
{
	static int s_nLive;
	uint64_t nValue;
	sirNontrivial_t( uint64_t nInput = 0 ) noexcept : nValue( nInput )
	{
		++s_nLive;
	}
	sirNontrivial_t( const sirNontrivial_t& other ) noexcept : nValue( other.nValue )
	{
		++s_nLive;
	}
	sirNontrivial_t( sirNontrivial_t&& other ) noexcept : nValue( other.nValue )
	{
		++s_nLive;
	}
	sirNontrivial_t& operator=( sirNontrivial_t&& other ) noexcept
	{
		nValue = other.nValue;
		return *this;
	}
	sirNontrivial_t& operator=( const sirNontrivial_t& other ) noexcept
	{
		nValue = other.nValue;
		return *this;
	}
	~sirNontrivial_t( void ) noexcept
	{
		--s_nLive;
	}
};
int sirNontrivial_t::s_nLive = 0;

struct alignas( 128 ) sirAligned_t
{
	uint64_t nValue;
	sirAligned_t( uint64_t nInput = 0 ) noexcept : nValue( nInput )
	{
	}
};

template <typename T, typename Container>
static void testEraseSequence( Container& values )
{
	std::vector<T> oracle;
	for ( uint32_t iIndex = 0; iIndex < 513; ++iIndex ) {
		require( values.tryEmplaceBack( ( uint64_t )iIndex ) == SIR_CONTAINER_OK );
		oracle.emplace_back( ( uint64_t )iIndex );
	}
	std::mt19937 generator( 283 );
	while ( !oracle.empty() ) {
		const uint32_t iRemove = generator() % oracle.size();
		require( values.tryEraseOrdered( iRemove ) == SIR_CONTAINER_OK );
		oracle.erase( oracle.begin() + iRemove );
		require( values.getSize() == oracle.size() );
		for ( uint32_t iIndex = 0; iIndex < oracle.size(); ++iIndex ) {
			if constexpr ( std::is_same<T, sirNontrivial_t>::value ) {
				require( values[iIndex].nValue == oracle[iIndex].nValue );
			} else {
				require( values[iIndex] == oracle[iIndex] );
			}
		}
	}
	require( values.tryEraseOrdered( 0 ) == SIR_CONTAINER_INVALID_ARGUMENT );
}

static void testVectorAndInline( const sirContainerAllocator_t *pAllocator )
{
	{
		SIRVector<sirWideValue_t> values( pAllocator );
		testEraseSequence<sirWideValue_t>( values );
	}
	{
		SIRInlineVector<sirWideValue_t, 16> values( pAllocator );
		testEraseSequence<sirWideValue_t>( values );
	}
	{
		SIRVector<sirNontrivial_t> values( pAllocator );
		testEraseSequence<sirNontrivial_t>( values );
	}
	{
		SIRInlineVector<sirNontrivial_t, 16> values( pAllocator );
		testEraseSequence<sirNontrivial_t>( values );
	}
	require( sirNontrivial_t::s_nLive == 0 );
}

static void testString( const sirContainerAllocator_t *pAllocator, sirTestDomain_t *pState )
{
	SIRString value( pAllocator );
	std::string oracle;
	for ( uint32_t iIndex = 0; iIndex < 8192; ++iIndex ) {
		const char nChar = ( char )( iIndex % 31 );
		require( value.tryAppendChar( nChar ) == SIR_CONTAINER_OK );
		oracle.push_back( nChar );
		require( value.getByteLength() == oracle.size() );
		require( memcmp( value.cStr(), oracle.data(), oracle.size() ) == 0 );
		require( value.cStr()[oracle.size()] == '\0' );
	}
	const char nAliased = value.cStr()[1];
	require( value.tryAppendChar( nAliased ) == SIR_CONTAINER_OK );
	oracle.push_back( nAliased );
	require( memcmp( value.cStr(), oracle.data(), oracle.size() ) == 0 );
	const auto nSizeBefore = value.getByteLength();
	for ( uint32_t iIndex = 0; iIndex < 200; ++iIndex ) {
		const uint32_t nUntilGrowth = value.getCapacity() - value.getByteLength() - 1;
		for ( uint32_t iItem = 0; iItem < nUntilGrowth; ++iItem ) {
			require( value.tryAppendChar( 'q' ) == SIR_CONTAINER_OK );
		}
		const uint32_t nBefore = value.getByteLength();
		pState->nFailAt = pState->nAllocations + 1;
		require( value.tryAppendChar( 'z' ) == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		require( value.getByteLength() == nBefore );
		require( value.cStr()[nBefore] == '\0' );
		break;
	}
	require( value.getByteLength() >= nSizeBefore );
	value.clear();
	require( value.getByteLength() == 0 );
	require( value.tryAppendChar( 'x' ) == SIR_CONTAINER_OK );
	require( strcmp( value.cStr(), "x" ) == 0 );
}

template <uint32_t N>
static void testRing( const sirContainerAllocator_t *pAllocator )
{
	SIRRingBuffer<uint32_t> ring( pAllocator );
	std::deque<uint32_t> oracle;
	require( ring.tryPushBackMany( nullptr, 0 ) == SIR_CONTAINER_OK );
	require( ring.tryPopFrontMany( nullptr, 0 ) == SIR_CONTAINER_OK );
	const uint32_t first[] = { 1, 2, 3 };
	require( ring.tryPushBackMany( first, 1 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	require( ring.tryInitialize( N ) == SIR_CONTAINER_OK );
	require( ring.tryPushBackMany( nullptr, 1 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	require( ring.tryPopFrontMany( nullptr, 1 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	std::mt19937 generator( 71 );
	uint32_t nNext = 0;
	for ( uint32_t iStep = 0; iStep < 20000; ++iStep ) {
		if ( ( generator() % 3 ) != 0 ) {
			const uint32_t nCount = generator() % ( N + 2 );
			std::vector<uint32_t> input( nCount );
			for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
				input[iIndex] = nNext++;
			}
			const auto eResult = ring.tryPushBackMany( input.data(), nCount );
			if ( nCount <= N - oracle.size() ) {
				require( eResult == SIR_CONTAINER_OK );
				for ( uint32_t nItem : input ) {
					oracle.push_back( nItem );
				}
			} else {
				require( eResult == SIR_CONTAINER_FULL );
			}
		} else {
			const uint32_t nCount = generator() % ( N + 2 );
			std::vector<uint32_t> output( nCount, UINT32_MAX );
			const auto eResult = ring.tryPopFrontMany( output.data(), nCount );
			if ( nCount <= oracle.size() ) {
				require( eResult == SIR_CONTAINER_OK );
				for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
					require( output[iIndex] == oracle.front() );
					oracle.pop_front();
				}
			} else {
				require( eResult == SIR_CONTAINER_EMPTY );
				for ( uint32_t nItem : output ) {
					require( nItem == UINT32_MAX );
				}
			}
		}
		require( ring.getSize() == oracle.size() );
		require( ( ring.front() == nullptr ) == oracle.empty() );
		if ( !oracle.empty() ) {
			require( *ring.front() == oracle.front() );
			require( *ring.back() == oracle.back() );
		}
	}
	std::vector<uint32_t> output( oracle.size() );
	require( ring.tryPopFrontMany( output.data(), ( uint32_t )oracle.size() ) == SIR_CONTAINER_OK );
	for ( uint32_t nItem : output ) {
		require( nItem == oracle.front() );
		oracle.pop_front();
	}
	require( ring.getSize() == 0 );
}

static void testBitvectors( const sirContainerAllocator_t *pAllocator )
{
	for ( uint32_t nBits : { 0u, 1u, 63u, 64u, 65u, 1024u, 8192u } ) {
		SIRBitVector bits( pAllocator );
		require( bits.tryResize( nBits ) == SIR_CONTAINER_OK );
		std::vector<uint32_t> oracle;
		std::mt19937 generator( 18 );
		for ( uint32_t iBit = 0; iBit < nBits; ++iBit ) {
			if ( generator() % 8 == 0 ) {
				require( bits.trySet( iBit, true ) == SIR_CONTAINER_OK );
				oracle.push_back( iBit );
			}
		}
		std::vector<uint32_t> found;
		bits.forEachSetBit( [&]( uint32_t iBit ) { found.push_back( iBit ); } );
		require( found == oracle );
		const SIRBitVector& view = bits;
		found.clear();
		view.forEachSetBit( [&]( uint32_t iBit ) { found.push_back( iBit ); } );
		require( found == oracle );
		if ( !oracle.empty() ) {
			uint32_t nVisited = 0;
			try {
				bits.forEachSetBit( [&]( uint32_t ) {
					++nVisited;
					throw std::runtime_error( "stop" );
				} );
			} catch ( const std::runtime_error& ) {
			}
			require( nVisited == 1 && bits.count() == oracle.size() );
		}
	}
	SIRFixedBitset<1> one;
	require( one.trySet( 0, true ) == SIR_CONTAINER_OK );
	uint32_t nHits = 0;
	one.forEachSetBit( [&]( uint32_t iBit ) {
		require( iBit == 0 );
		++nHits;
	} );
	require( nHits == 1 );
	SIRFixedBitset<65> edge;
	for ( uint32_t iBit : { 0u, 63u, 64u } ) {
		require( edge.trySet( iBit, true ) == SIR_CONTAINER_OK );
	}
	uint32_t iExpected = 0;
	const uint32_t szKeys[3] = { 0, 63, 64 };
	edge.forEachSetBit( [&]( uint32_t iBit ) { require( iBit == szKeys[iExpected++] ); } );
	require( iExpected == 3 );
	edge.clear();
	edge.forEachSetBit( [&]( uint32_t ) { require( false ); } );
}

template <uint32_t PAGE>
static void testPaged( const sirContainerAllocator_t *pAllocator, sirTestDomain_t *pState )
{
	SIRPagedArray<sirAligned_t, PAGE> values( pAllocator );
	uint64_t nSum = 0;
	for ( uint32_t iIndex = 0; iIndex < 2 * PAGE + 17; ++iIndex ) {
		require( values.tryEmplaceBack( iIndex ) == SIR_CONTAINER_OK );
	}
	require( values.getSize() == 2 * PAGE + 17 );
	const auto& readOnly = values;
	readOnly.forEach( [&]( const sirAligned_t& value ) { nSum += value.nValue; } );
	require( nSum == ( uint64_t )( values.getSize() - 1 ) * values.getSize() / 2 );
	values.forEach( [&]( sirAligned_t& value ) { value.nValue += 7; } );
	for ( uint32_t iIndex = 0; iIndex < values.getSize(); ++iIndex ) {
		require( values.tryGet( iIndex )->nValue == iIndex + 7 );
	}
	const uint32_t nBefore = values.getSize();
	pState->nFailAt = pState->nAllocations + 1;
	while ( values.getSize() % PAGE != 0 ) {
		require( values.tryEmplaceBack( 0 ) == SIR_CONTAINER_OK );
	}
	require( values.tryEmplaceBack( 0 ) == SIR_CONTAINER_OUT_OF_MEMORY );
	pState->nFailAt = 0;
	require( values.getSize() >= nBefore );
	values.clearAndRelease();
	require( values.getSize() == 0 );
}

struct sirReversePriority_t
{
	bool operator()( const uint32_t& nLeft, const uint32_t& nRight ) const noexcept
	{
		return nLeft > nRight;
	}
};

struct sirThrowPriority_t
{
	static bool s_bThrowCopy;
	uint32_t nValue;
	sirThrowPriority_t( uint32_t nInput = 0 ) noexcept : nValue( nInput )
	{
	}
	sirThrowPriority_t( const sirThrowPriority_t& other ) : nValue( other.nValue )
	{
		if ( s_bThrowCopy ) {
			throw std::runtime_error( "injected copy failure" );
		}
	}
	sirThrowPriority_t( sirThrowPriority_t&& other ) noexcept : nValue( other.nValue )
	{
	}
	sirThrowPriority_t& operator=( sirThrowPriority_t&& other ) noexcept
	{
		nValue = other.nValue;
		return *this;
	}
	bool operator<( const sirThrowPriority_t& other ) const noexcept
	{
		return nValue < other.nValue;
	}
};
bool sirThrowPriority_t::s_bThrowCopy = false;

static void testHeapBuild( const sirContainerAllocator_t *pAllocator, sirTestDomain_t *pState )
{
	std::mt19937 generator( 99 );
	for ( uint32_t nCount : { 0u, 1u, 2u, 3u, 16u, 256u, 4096u } ) {
		std::vector<uint32_t> data( nCount );
		for ( uint32_t iItem = 0; iItem < nCount; ++iItem ) {
			data[iItem] = generator() % 128;
		}
		SIRPriorityQueue<uint32_t> heap( pAllocator );
		require( heap.tryBuild( data.data(), nCount ) == SIR_CONTAINER_OK );
		require( heap.getSize() == nCount );
		uint32_t nLast = UINT32_MAX;
		while ( !heap.isEmpty() ) {
			const uint32_t nTop = *heap.top();
			require( nTop <= nLast );
			nLast = nTop;
			require( heap.tryPop() == SIR_CONTAINER_OK );
		}
	}
	{
		SIRPriorityQueue<uint32_t, sirReversePriority_t> heap( pAllocator );
		const uint32_t szValues[7] = { 7, 3, 2, 9, 1, 7, 2 };
		require( heap.tryBuild( szValues, 7 ) == SIR_CONTAINER_OK );
		for ( uint32_t nExpected : { 1u, 2u, 2u, 3u, 7u, 7u, 9u } ) {
			require( heap.top() != nullptr && *heap.top() == nExpected );
			require( heap.tryPop() == SIR_CONTAINER_OK );
		}
	}
	{
		SIRPriorityQueue<sirThrowPriority_t> heap( pAllocator );
		const sirThrowPriority_t szValues[3] = { sirThrowPriority_t( 17 ), sirThrowPriority_t( 32 ), sirThrowPriority_t( 4 ) };
		require( heap.tryBuild( szValues, 3 ) == SIR_CONTAINER_OK );
		require( heap.getSize() == 3 && heap.top()->nValue == 32 );
		require( heap.tryBuild( nullptr, 1 ) == SIR_CONTAINER_INVALID_ARGUMENT );
		require( heap.getSize() == 3 );
		pState->nFailAt = pState->nAllocations + 1;
		require( heap.tryBuild( szValues, 3 ) == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		require( heap.getSize() == 3 && heap.top()->nValue == 32 );
		sirThrowPriority_t::s_bThrowCopy = true;
		bool bThrown = false;
		try {
			heap.tryBuild( szValues, 3 );
		} catch ( const std::runtime_error& ) {
			bThrown = true;
		}
		sirThrowPriority_t::s_bThrowCopy = false;
		require( bThrown );
		require( heap.getSize() == 3 && heap.top()->nValue == 32 );
		const sirThrowPriority_t *pAlias = heap.top();
		require( heap.tryBuild( pAlias, 1 ) == SIR_CONTAINER_OK );
		require( heap.getSize() == 1 && heap.top()->nValue == 32 );
		require( heap.tryBuild( nullptr, 0 ) == SIR_CONTAINER_OK );
		require( heap.isEmpty() );
	}
}

int main( void )
{
	sirTestDomain_t state{};
	sirContainerAllocator_t allocator{ &state, testAllocate, testDeallocate, 5, 17 };
	testHeapBuild( &allocator, &state );
	require( state.nLiveBytes == 0 );
	testVectorAndInline( &allocator );
	require( state.nLiveBytes == 0 );
	testString( &allocator, &state );
	require( state.nLiveBytes == 0 );
	testRing<1>( &allocator );
	testRing<3>( &allocator );
	testRing<257>( &allocator );
	testRing<1024>( &allocator );
	require( state.nLiveBytes == 0 );
	{
		SIRQueue<uint32_t> queue( &allocator );
		require( queue.tryInitialize( 3 ) == SIR_CONTAINER_OK );
		uint32_t szInput[3] = { 4, 5, 6 };
		uint32_t szOutput[3] = {};
		require( queue.tryPushMany( szInput, 3 ) == SIR_CONTAINER_OK );
		require( queue.tryPopMany( szOutput, 3 ) == SIR_CONTAINER_OK );
		require( memcmp( szInput, szOutput, sizeof( szInput ) ) == 0 );
	}
	testBitvectors( &allocator );
	require( state.nLiveBytes == 0 );
	testPaged<1>( &allocator, &state );
	testPaged<16>( &allocator, &state );
	testPaged<256>( &allocator, &state );
	testPaged<1024>( &allocator, &state );
	require( state.nLiveBytes == 0 );
	return 0;
}
