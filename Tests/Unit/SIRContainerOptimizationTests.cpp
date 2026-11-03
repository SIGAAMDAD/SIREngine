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

#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRInlineVector.h>
#include <Core/Containers/SIRFlatHashMap.h>
#include <Core/Containers/SIRFlatHashSet.h>
#include <Core/Containers/SIRString.h>
#include <Core/Containers/SIRBitVector.h>
#include <Core/Containers/SIRFixedBitset.h>
#include <Core/Containers/SIRRingBuffer.h>
#include <Core/Containers/SIRPagedArray.h>
#include <stdio.h>
#include <initializer_list>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <utility>
#if defined( _MSC_VER )
#include <malloc.h>
#endif

#define SIR_CHECK( condition )                                                                                                             \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Container regression failed at line %d: %s\n", __LINE__, #condition );                                       \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirOptimizationTestMemory_s
{
	size_t nLive;
	uint32_t nAllocations;
	uint32_t nFailAt;
} sirOptimizationTestMemory_t;

static void *sirOptimizationAllocate( void *pContext, size_t nBytes, size_t nAlignment )
{
	sirOptimizationTestMemory_t *pMemory = ( sirOptimizationTestMemory_t * )pContext;
	++pMemory->nAllocations;
	if ( pMemory->nFailAt != 0 && pMemory->nAllocations == pMemory->nFailAt ) {
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
		pMemory->nLive += nBytes;
	}
	return pResult;
}

static void sirOptimizationDeallocate( void *pContext, void *pMemory, size_t nBytes, size_t )
{
	sirOptimizationTestMemory_t *pState = ( sirOptimizationTestMemory_t * )pContext;
	SIR_CHECK( pState->nLive >= nBytes );
	pState->nLive -= nBytes;
#if defined( _MSC_VER )
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

class SIRTestCopyFailure
{
public:
	static uint32_t s_nLive;
	static bool s_bFail;
	int m_nValue;

	explicit SIRTestCopyFailure( int nValue ) : m_nValue( nValue )
	{
		++s_nLive;
	}
	SIRTestCopyFailure( const SIRTestCopyFailure &other ) : m_nValue( other.m_nValue )
	{
		if ( s_bFail ) {
			throw std::runtime_error( "copy-failure injection" );
		}
		++s_nLive;
	}
	SIRTestCopyFailure( SIRTestCopyFailure &&other ) noexcept( false ) : m_nValue( other.m_nValue )
	{
		++s_nLive;
	}
	~SIRTestCopyFailure( void ) noexcept
	{
		--s_nLive;
	}
};
uint32_t SIRTestCopyFailure::s_nLive = 0;
bool SIRTestCopyFailure::s_bFail = false;

static void sirTestExceptionRollback( const sirContainerAllocator_t *pAllocator )
{
	SIRVector<SIRTestCopyFailure> values( pAllocator );
	for ( int iIndex = 0; iIndex < 8; ++iIndex ) {
		SIR_CHECK( values.tryEmplaceBack( iIndex ) == SIR_CONTAINER_OK );
	}
	SIR_CHECK( values.getSize() == values.getCapacity() );
	SIRTestCopyFailure::s_bFail = true;
	bool bCaught = false;
	try {
		values.tryEmplaceBack( 99 );
	} catch ( const std::runtime_error & ) {
		bCaught = true;
	}
	SIRTestCopyFailure::s_bFail = false;
	SIR_CHECK( bCaught && values.getSize() == 8 );
	SIR_CHECK( values[0].m_nValue == 0 && values[7].m_nValue == 7 );
	SIR_CHECK( SIRTestCopyFailure::s_nLive == 8 );
}

static void sirTestAliasing( const sirContainerAllocator_t *pAllocator, sirOptimizationTestMemory_t *pState )
{
	{
		SIRVector<int> values( pAllocator );
		for ( int iIndex = 0; iIndex < 8; ++iIndex ) {
			SIR_CHECK( values.tryEmplaceBack( iIndex + 100 ) == SIR_CONTAINER_OK );
		}
		SIR_CHECK( values.getSize() == values.getCapacity() );
		SIR_CHECK( values.tryEmplaceBack( values[0] ) == SIR_CONTAINER_OK );
		SIR_CHECK( values[8] == 100 && values[0] == 100 );
	}
	{
		SIRVector<std::pair<int, int>> values( pAllocator );
		for ( int iIndex = 0; iIndex < 8; ++iIndex ) {
			SIR_CHECK( values.tryEmplaceBack( iIndex, iIndex + 1 ) == SIR_CONTAINER_OK );
		}
		SIR_CHECK( values.tryEmplaceBack( values[0].first, values[0].second ) == SIR_CONTAINER_OK );
		SIR_CHECK( values[8].first == 0 && values[8].second == 1 );
	}
	{
		SIRInlineVector<int, 1> values( pAllocator );
		for ( int iIndex = 0; iIndex < 8; ++iIndex ) {
			SIR_CHECK( values.tryEmplaceBack( iIndex + 10 ) == SIR_CONTAINER_OK );
		}
		SIR_CHECK( values.getSize() == values.getCapacity() );
		SIR_CHECK( values.tryEmplaceBack( values[0] ) == SIR_CONTAINER_OK );
		SIR_CHECK( values[8] == 10 && values[0] == 10 );
	}
	{
		SIRFlatHashMap<int, int> values( pAllocator );
		for ( int iIndex = 0; iIndex < 7; ++iIndex ) {
			SIR_CHECK( values.tryInsert( iIndex, 100 + iIndex ) == SIR_CONTAINER_OK );
		}
		const int &nNewKey = *values.find( 0 );
		SIR_CHECK( values.tryEmplace( nNewKey, 999 ) == SIR_CONTAINER_OK );
		SIR_CHECK( values.find( 100 ) != nullptr && *values.find( 100 ) == 999 );
		SIR_CHECK( values.find( 0 ) != nullptr && *values.find( 0 ) == 100 );
	}
	{
		SIRFlatHashMap<int, int> values( pAllocator );
		for ( int iIndex = 0; iIndex < 7; ++iIndex ) {
			SIR_CHECK( values.tryInsert( iIndex, 100 + iIndex ) == SIR_CONTAINER_OK );
		}
		const int &nNewValue = *values.find( 0 );
		SIR_CHECK( values.tryEmplace( 999, nNewValue ) == SIR_CONTAINER_OK );
		SIR_CHECK( values.find( 999 ) != nullptr && *values.find( 999 ) == 100 );
	}
	{
		SIRVector<int> values( pAllocator );
		for ( int iIndex = 0; iIndex < 8; ++iIndex ) {
			SIR_CHECK( values.tryEmplaceBack( iIndex + 100 ) == SIR_CONTAINER_OK );
		}
		pState->nFailAt = pState->nAllocations + 1;
		SIR_CHECK( values.tryEmplaceBack( values[0] ) == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		SIR_CHECK( values.getSize() == 8 && values[0] == 100 );
	}
	{
		SIRFlatHashMap<int, int> values( pAllocator );
		for ( int iIndex = 0; iIndex < 7; ++iIndex ) {
			SIR_CHECK( values.tryInsert( iIndex, 100 + iIndex ) == SIR_CONTAINER_OK );
		}
		const int &nKey = *values.find( 0 );
		pState->nFailAt = pState->nAllocations + 1;
		SIR_CHECK( values.tryEmplace( nKey, 999 ) == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		SIR_CHECK( values.getSize() == 7 && values.find( 100 ) == nullptr );
	}
}

static void sirTestGrowth( const sirContainerAllocator_t *pAllocator, sirOptimizationTestMemory_t *pState )
{
	{
		SIRVector<int> values( pAllocator );
		uint32_t nInitial = pState->nAllocations;
		for ( uint32_t iIndex = 0; iIndex < 8192; ++iIndex ) {
			SIR_CHECK( values.tryResize( iIndex + 1 ) == SIR_CONTAINER_OK );
			values[iIndex] = ( int )iIndex;
		}
		SIR_CHECK( pState->nAllocations - nInitial < 30 );
		SIR_CHECK( values[8191] == 8191 );
	}
	{
		SIRInlineVector<int, 4> values( pAllocator );
		uint32_t nInitial = pState->nAllocations;
		for ( uint32_t iIndex = 0; iIndex < 8192; ++iIndex ) {
			SIR_CHECK( values.tryResize( iIndex + 1 ) == SIR_CONTAINER_OK );
			values[iIndex] = ( int )iIndex;
		}
		SIR_CHECK( pState->nAllocations - nInitial < 30 );
	}
	{
		SIRString text( pAllocator );
		uint32_t nInitial = pState->nAllocations;
		for ( uint32_t iIndex = 0; iIndex < 8192; ++iIndex ) {
			SIR_CHECK( text.tryAppendBytes( "abcdefghijklmnopqrstuvwxABCDEFGH", 32 ) == SIR_CONTAINER_OK );
		}
		SIR_CHECK( text.getByteLength() == 8192 * 32 );
		SIR_CHECK( pState->nAllocations - nInitial < 40 );
		SIR_CHECK( text.tryAppendBytes( text.cStr() + 3, 6 ) == SIR_CONTAINER_OK );
		SIR_CHECK( text.getByteLength() == 8192 * 32 + 6 );
		SIR_CHECK( memcmp( text.cStr() + 8192 * 32, "defghi", 6 ) == 0 );
		SIR_CHECK( text.tryAssignBytes( text.cStr() + 10, 8 ) == SIR_CONTAINER_OK );
		SIR_CHECK( memcmp( text.cStr(), "klmnopqr", 8 ) == 0 );
	}
	{
		SIRBitVector bits( pAllocator );
		uint32_t nInitial = pState->nAllocations;
		for ( uint32_t iIndex = 1; iIndex <= 65536; ++iIndex ) {
			SIR_CHECK( bits.tryResize( iIndex ) == SIR_CONTAINER_OK );
		}
		SIR_CHECK( pState->nAllocations - nInitial < 25 );
		SIR_CHECK( bits.trySet( 65535, true ) == SIR_CONTAINER_OK );
		SIR_CHECK( bits.findFirstSet() == 65535 );
		SIR_CHECK( bits.tryResize( 10 ) == SIR_CONTAINER_OK );
		SIR_CHECK( bits.tryResize( 65536 ) == SIR_CONTAINER_OK );
		SIR_CHECK( bits.findFirstSet() == UINT32_MAX );
	}
}

static void sirTestReclamation( const sirContainerAllocator_t *pAllocator, sirOptimizationTestMemory_t *pState )
{
	{
		SIRVector<int> values( pAllocator );
		SIR_CHECK( values.tryReserve( 4096 ) == SIR_CONTAINER_OK );
		SIR_CHECK( values.tryResize( 64 ) == SIR_CONTAINER_OK );
		values[0] = 123;
		size_t nBefore = pState->nLive;
		pState->nFailAt = pState->nAllocations + 1;
		SIR_CHECK( values.tryShrinkToFit() == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		SIR_CHECK( values.getCapacity() == 4096 && values[0] == 123 && pState->nLive == nBefore );
		SIR_CHECK( values.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( values.getCapacity() == 64 && values[0] == 123 && pState->nLive < nBefore );
		values.clear();
		SIR_CHECK( values.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( values.getCapacity() == 0 && values.data() == nullptr );
	}
	SIR_CHECK( pState->nLive == 0 );
	{
		SIRInlineVector<int, 8> values( pAllocator );
		for ( uint32_t iIndex = 0; iIndex < 128; ++iIndex ) {
			SIR_CHECK( values.tryEmplaceBack( ( int )iIndex ) == SIR_CONTAINER_OK );
		}
		size_t nBefore = pState->nLive;
		SIR_CHECK( values.tryResize( 96 ) == SIR_CONTAINER_OK );
		pState->nFailAt = pState->nAllocations + 1;
		SIR_CHECK( values.tryShrinkToFit() == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		SIR_CHECK( values.getCapacity() >= 128 && values[0] == 0 && pState->nLive == nBefore );
		SIR_CHECK( values.tryResize( 5 ) == SIR_CONTAINER_OK );
		SIR_CHECK( values.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( values.usesInlineStorage() && values.getCapacity() == 8 && values[4] == 4 );
		SIR_CHECK( pState->nLive == 0 );
	}
	{
		SIRPagedArray<int, 32> values( pAllocator );
		for ( uint32_t iIndex = 0; iIndex < 513; ++iIndex ) {
			SIR_CHECK( values.tryEmplaceBack( ( int )iIndex ) == SIR_CONTAINER_OK );
		}
		int *pFirst = values.tryGet( 0 );
		const size_t nBefore = pState->nLive;
		while ( values.getSize() > 13 ) {
			SIR_CHECK( values.tryPopBack() == SIR_CONTAINER_OK );
		}
		SIR_CHECK( values.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( values.getPageCount() == 1 && values.tryGet( 0 ) == pFirst );
		SIR_CHECK( *values.tryGet( 12 ) == 12 && pState->nLive < nBefore );
		values.clear();
		SIR_CHECK( values.tryShrinkToFit() == SIR_CONTAINER_OK );
		SIR_CHECK( values.getPageCount() == 0 && pState->nLive == 0 );
	}
}
static void sirTestDistribution( const sirContainerAllocator_t *pAllocator )
{
	for ( uint32_t nStride : { 1u, 256u, 65536u } ) {
		SIRFlatHashMap<uint32_t, uint32_t> values( pAllocator );
		SIRFlatHashSet<uint32_t> keys( pAllocator );
		SIR_CHECK( values.tryReserve( 8192 ) == SIR_CONTAINER_OK );
		SIR_CHECK( keys.tryReserve( 8192 ) == SIR_CONTAINER_OK );
		for ( uint32_t iIndex = 0; iIndex < 8192; ++iIndex ) {
			uint32_t nKey = iIndex * nStride + 37;
			SIR_CHECK( values.tryInsert( nKey, iIndex ) == SIR_CONTAINER_OK );
			SIR_CHECK( keys.tryInsert( nKey ) == SIR_CONTAINER_OK );
		}
		for ( uint32_t iIndex = 0; iIndex < 8192; ++iIndex ) {
			uint32_t nKey = iIndex * nStride + 37;
			SIR_CHECK( values.find( nKey ) != nullptr && *values.find( nKey ) == iIndex );
			SIR_CHECK( keys.contains( nKey ) );
		}
	}
}

static void sirTestRing( const sirContainerAllocator_t *pAllocator )
{
	SIRRingBuffer<int> ring( pAllocator );
	SIR_CHECK( ring.tryInitialize( 5 ) == SIR_CONTAINER_OK );
	for ( int iIndex = 0; iIndex < 5; ++iIndex ) {
		SIR_CHECK( ring.tryPushBack( iIndex ) == SIR_CONTAINER_OK );
	}
	SIR_CHECK( ring.tryPushBack( 5 ) == SIR_CONTAINER_FULL );
	for ( int iIndex = 0; iIndex < 3; ++iIndex ) {
		SIR_CHECK( ring.front() != nullptr && *ring.front() == iIndex );
		SIR_CHECK( ring.tryPopFront() == SIR_CONTAINER_OK );
	}
	for ( int iIndex = 5; iIndex < 8; ++iIndex ) {
		SIR_CHECK( ring.tryPushBack( iIndex ) == SIR_CONTAINER_OK );
	}
	SIR_CHECK( ring.front() != nullptr && *ring.front() == 3 );
	SIR_CHECK( ring.back() != nullptr && *ring.back() == 7 );
}

int main( void )
{
	sirOptimizationTestMemory_t state = {};
	sirContainerAllocator_t allocator = { &state, sirOptimizationAllocate, sirOptimizationDeallocate, 12, 34 };
	sirTestAliasing( &allocator, &state );
	sirTestExceptionRollback( &allocator );
	SIR_CHECK( SIRTestCopyFailure::s_nLive == 0 );
	sirTestGrowth( &allocator, &state );
	sirTestReclamation( &allocator, &state );
	sirTestDistribution( &allocator );
	sirTestRing( &allocator );
	SIR_CHECK( state.nLive == 0 );
	puts( "SIR container optimization regressions passed" );
	return 0;
}
