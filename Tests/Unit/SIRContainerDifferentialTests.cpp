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
#include <Core/Containers/SIRInlineVector.h>
#include <Core/Containers/SIRString.h>
#include <Core/Containers/SIRVector.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#if defined(_MSC_VER)
#include <malloc.h>
#endif


static void sirTestCheck( bool bSuccess, const char *pszExpression, int nLine )
{
	if ( !bSuccess ) {
		fprintf( stderr, "SIR container test failed at line %d: %s\n", nLine, pszExpression );
		abort();
	}
}

#define SIR_TEST_CHECK(expression) sirTestCheck( ( expression ), #expression, __LINE__ )

typedef struct sirDifferentialMemory_s
{
	size_t nLiveBytes;
	uint32_t nActive;
} sirDifferentialMemory_t;

static void *diffAllocate( void *pContext, size_t nBytes, size_t nAlignment )
{
	if ( nAlignment < sizeof( void * ) ) {
		nAlignment = sizeof( void * );
	}
	void *pMemory = nullptr;
#if defined(_MSC_VER)
	pMemory = _aligned_malloc( nBytes, nAlignment );
#else
	if ( posix_memalign( &pMemory, nAlignment, nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pMemory != nullptr ) {
		sirDifferentialMemory_t *pMemoryInfo = ( sirDifferentialMemory_t * )pContext;
		pMemoryInfo->nLiveBytes += nBytes;
		++pMemoryInfo->nActive;
	}
	return pMemory;
}

static void diffFree( void *pContext, void *pMemory, size_t nBytes, size_t )
{
	sirDifferentialMemory_t *pMemoryInfo = ( sirDifferentialMemory_t * )pContext;
	SIR_TEST_CHECK( pMemoryInfo->nLiveBytes >= nBytes && pMemoryInfo->nActive > 0 );
	pMemoryInfo->nLiveBytes -= nBytes;
	--pMemoryInfo->nActive;
#if defined(_MSC_VER)
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

typedef struct sirCollisionHash_s
{
	size_t operator()( uint32_t nKey ) const noexcept { return ( nKey >> 3 ) & 15u; }
} sirCollisionHash_t;

static uint32_t nextRandom( uint32_t *pState ) noexcept
{
	uint32_t nValue = *pState;
	nValue ^= nValue << 13;
	nValue ^= nValue >> 17;
	nValue ^= nValue << 5;
	*pState = nValue;
	return nValue;
}

static void testMapAndSet( const sirContainerAllocator_t *pAllocator )
{
	SIRFlatHashMap<uint32_t, uint32_t, sirCollisionHash_t> map( pAllocator );
	SIRFlatHashSet<uint32_t, sirCollisionHash_t> set( pAllocator );
	std::unordered_map<uint32_t, uint32_t, sirCollisionHash_t> expectedMap;
	std::unordered_set<uint32_t, sirCollisionHash_t> expectedSet;
	uint32_t nSeed = 0x12345678u;

	for ( uint32_t iOp = 0; iOp < 12000; ++iOp ) {
		uint32_t nRandom = nextRandom( &nSeed );
		uint32_t nKey = ( nRandom >> 8 ) % 1536;
		uint32_t nValue = nextRandom( &nSeed );
		uint32_t nOperation = nRandom % 4;
		if ( nOperation == 0 || nOperation == 1 ) {
			bool bInserted = false;
			SIR_TEST_CHECK( map.tryInsert( nKey, nValue, &bInserted ) == SIR_CONTAINER_OK );
			bool bExpected = expectedMap.emplace( nKey, nValue ).second;
			SIR_TEST_CHECK( bInserted == bExpected );
			SIR_TEST_CHECK( set.tryInsert( nKey, &bInserted ) == SIR_CONTAINER_OK );
			bExpected = expectedSet.insert( nKey ).second;
			SIR_TEST_CHECK( bInserted == bExpected );
		} else if ( nOperation == 2 ) {
			SIR_TEST_CHECK( map.erase( nKey ) == ( expectedMap.erase( nKey ) != 0 ) );
			SIR_TEST_CHECK( set.erase( nKey ) == ( expectedSet.erase( nKey ) != 0 ) );
		} else {
			const uint32_t *pValue = map.find( nKey );
			std::unordered_map<uint32_t, uint32_t, sirCollisionHash_t>::const_iterator found = expectedMap.find( nKey );
			SIR_TEST_CHECK( ( pValue == nullptr ) == ( found == expectedMap.end() ) );
			if ( pValue != nullptr ) {
				SIR_TEST_CHECK( *pValue == found->second );
			}
			SIR_TEST_CHECK( set.contains( nKey ) == ( expectedSet.find( nKey ) != expectedSet.end() ) );
		}
		SIR_TEST_CHECK( map.getSize() == expectedMap.size() );
		SIR_TEST_CHECK( set.getSize() == expectedSet.size() );
	}
	for ( const auto &entry : expectedMap ) {
		const uint32_t *pValue = map.find( entry.first );
		SIR_TEST_CHECK( pValue != nullptr && *pValue == entry.second );
	}
	for ( uint32_t nKey : expectedSet ) {
		SIR_TEST_CHECK( set.contains( nKey ) );
	}
}

static void testStringGrowth( const sirContainerAllocator_t *pAllocator )
{
	SIRString text( pAllocator );
	std::string expected;
	for ( uint32_t iStep = 0; iStep < 200; ++iStep ) {
		char szValue[7] = { 'a', ( char )( 'a' + iStep % 26 ), '-', '1', '2', '3', '4' };
		SIR_TEST_CHECK( text.tryAppendBytes( szValue, sizeof( szValue ) ) == SIR_CONTAINER_OK );
		expected.append( szValue, sizeof( szValue ) );
		SIR_TEST_CHECK( text.getByteLength() == expected.size() );
		SIR_TEST_CHECK( memcmp( text.cStr(), expected.data(), expected.size() ) == 0 );
	}
	SIR_TEST_CHECK( text.cStr()[text.getByteLength()] == '\0' );
	SIR_TEST_CHECK( text.tryAppendBytes( text.cStr() + 100, 900 ) == SIR_CONTAINER_OK );
	expected.append( expected.data() + 100, 900 );
	SIR_TEST_CHECK( text.getByteLength() == expected.size() );
	SIR_TEST_CHECK( memcmp( text.cStr(), expected.data(), expected.size() ) == 0 );
	SIR_TEST_CHECK( text.tryAssignBytes( text.cStr() + 20, 64 ) == SIR_CONTAINER_OK );
	SIR_TEST_CHECK( memcmp( text.cStr(), expected.data() + 20, 64 ) == 0 );
}

int main( void )
{
	sirDifferentialMemory_t state = {};
	sirContainerAllocator_t allocator = { &state, diffAllocate, diffFree, 1, 2 };
	{
		testMapAndSet( &allocator );
		testStringGrowth( &allocator );
	}
	SIR_TEST_CHECK( state.nLiveBytes == 0 && state.nActive == 0 );
	return 0;
}
