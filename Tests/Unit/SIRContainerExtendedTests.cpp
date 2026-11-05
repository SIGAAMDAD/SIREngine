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

#include <Core/Containers/SIRPriorityQueue.h>
#include <Core/Containers/SIRIntrusiveList.h>
#include <Core/Containers/SIRPagedArray.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <queue>
#include <random>
#if defined( _MSC_VER )
#include <malloc.h>
#endif

static void sirExtendedCheck( bool bSuccess, int nLine )
{
	if ( !bSuccess ) {
		fprintf( stderr, "SIRContainerExtendedTests failure line %d\n", nLine );
		abort();
	}
}
#define CHECK( x ) sirExtendedCheck( ( x ), __LINE__ )

typedef struct sirExtendedAllocator_s
{
	uint64_t nLiveBytes;
	uint32_t nFailures;
	uint32_t nAllocationCount;
} sirExtendedAllocator_t;

static void *extendedAllocate( void *pContext, size_t nBytes, size_t nAlignment )
{
	sirExtendedAllocator_t *pState = ( sirExtendedAllocator_t * )pContext;
	++pState->nAllocationCount;
	if ( pState->nFailures > 0 ) {
		--pState->nFailures;
		return nullptr;
	}
	if ( nAlignment < sizeof( void * ) ) {
		nAlignment = sizeof( void * );
	}
	void *pMemory = nullptr;
#if defined( _MSC_VER )
	pMemory = _aligned_malloc( nBytes, nAlignment );
#else
	if ( posix_memalign( &pMemory, nAlignment, nBytes ) != 0 ) {
		pMemory = nullptr;
	}
#endif
	if ( pMemory != nullptr ) {
		pState->nLiveBytes += nBytes;
	}
	return pMemory;
}

static void extendedDeallocate( void *pContext, void *pMemory, size_t nBytes, size_t )
{
	sirExtendedAllocator_t *pState = ( sirExtendedAllocator_t * )pContext;
	CHECK( pState->nLiveBytes >= nBytes );
	pState->nLiveBytes -= nBytes;
#if defined( _MSC_VER )
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

class SIRTrackedValue
{
public:
	static int s_nLive;
	int m_nValue;
	explicit SIRTrackedValue( int nValue ) noexcept : m_nValue( nValue )
	{
		++s_nLive;
	}
	SIRTrackedValue( SIRTrackedValue&& other ) noexcept : m_nValue( other.m_nValue )
	{
		++s_nLive;
	}
	SIRTrackedValue( const SIRTrackedValue& other ) noexcept : m_nValue( other.m_nValue )
	{
		++s_nLive;
	}
	~SIRTrackedValue( void ) noexcept
	{
		--s_nLive;
	}
};
int SIRTrackedValue::s_nLive = 0;

int main( void )
{
	sirExtendedAllocator_t state = {};
	sirContainerAllocator_t allocator = { &state, extendedAllocate, extendedDeallocate, 2, 91 };
	{
		SIRPriorityQueue<int> heap( &allocator );
		std::priority_queue<int> reference;
		std::mt19937 generator( 977 );
		for ( int i = 0; i < 8192; ++i ) {
			int nValue = ( int )generator();
			CHECK( heap.tryEmplace( nValue ) == SIR_CONTAINER_OK );
			reference.push( nValue );
		}
		while ( !reference.empty() ) {
			CHECK( heap.top() != nullptr && *heap.top() == reference.top() );
			CHECK( heap.tryPop() == SIR_CONTAINER_OK );
			reference.pop();
		}
		CHECK( heap.tryPop() == SIR_CONTAINER_EMPTY );
		heap.clear();
	}
	for ( int nMode = 0; nMode < 3; ++nMode ) {
		SIRPriorityQueue<int> heap( &allocator );
		std::priority_queue<int> reference;
		CHECK( heap.tryReserve( 4096 ) == SIR_CONTAINER_OK );
		for ( int iIndex = 0; iIndex < 4096; ++iIndex ) {
			int nValue = nMode == 0 ? iIndex : ( nMode == 1 ? 4095 - iIndex : iIndex % 31 );
			CHECK( heap.tryPush( nValue ) == SIR_CONTAINER_OK );
			reference.push( nValue );
		}
		while ( !reference.empty() ) {
			CHECK( heap.top() != nullptr && *heap.top() == reference.top() );
			CHECK( heap.tryPop() == SIR_CONTAINER_OK );
			reference.pop();
		}
		CHECK( heap.isEmpty() );
	}
	CHECK( state.nLiveBytes == 0 );
	{
		SIRPagedArray<SIRTrackedValue, 7> pages( &allocator );
		CHECK( pages.tryGet( 0 ) == nullptr );
		CHECK( pages.tryEmplaceBack( 1 ) == SIR_CONTAINER_OK );
		SIRTrackedValue *pFirst = pages.tryGet( 0 );
		for ( int i = 1; i < 2000; ++i ) {
			CHECK( pages.tryEmplaceBack( i + 1 ) == SIR_CONTAINER_OK );
		}
		CHECK( pages.getSize() == 2000 );
		CHECK( pages.getPageCount() == 286 );
		CHECK( pages.tryGet( 0 ) == pFirst && pFirst->m_nValue == 1 );
		CHECK( pages.tryGet( 1999 )->m_nValue == 2000 );
		for ( int i = 0; i < 1994; ++i ) {
			CHECK( pages.tryPopBack() == SIR_CONTAINER_OK );
		}
		CHECK( pages.tryGet( 6 ) == nullptr );
		CHECK( pages.tryEmplaceBack( 99 ) == SIR_CONTAINER_OK );
		CHECK( pages.tryGet( 6 )->m_nValue == 99 );
		pages.clearAndRelease();
		CHECK( pages.getPageCount() == 0 );
		CHECK( state.nLiveBytes == 0 );
		state.nFailures = 1;
		CHECK( pages.tryEmplaceBack( 42 ) == SIR_CONTAINER_OUT_OF_MEMORY );
		CHECK( pages.getSize() == 0 && pages.getPageCount() == 0 );
		CHECK( pages.tryEmplaceBack( 43 ) == SIR_CONTAINER_OK );
		CHECK( pages.tryGet( 0 )->m_nValue == 43 );
	}
	CHECK( SIRTrackedValue::s_nLive == 0 );
	CHECK( state.nLiveBytes == 0 );
	{
		sirIntrusiveLink_t a = {};
		sirIntrusiveLink_t b = {};
		sirIntrusiveLink_t c = {};
		SIRIntrusiveList list;
		SIRIntrusiveList other;
		CHECK( list.tryPushBack( &a ) == SIR_CONTAINER_OK );
		CHECK( list.tryPushBack( &b ) == SIR_CONTAINER_OK );
		CHECK( list.tryPushFront( &c ) == SIR_CONTAINER_OK );
		CHECK( list.getSize() == 3 && list.front() == &c && list.back() == &b );
		CHECK( c.pNext == &a && a.pPrevious == &c && a.pNext == &b );
		CHECK( other.tryRemove( &a ) == SIR_CONTAINER_INVALID_HANDLE );
		CHECK( list.tryPushBack( &a ) == SIR_CONTAINER_INVALID_ARGUMENT );
		CHECK( list.tryRemove( &a ) == SIR_CONTAINER_OK );
		CHECK( c.pNext == &b && b.pPrevious == &c );
		list.clear();
		CHECK( a.pOwner == nullptr && b.pOwner == nullptr && c.pOwner == nullptr );
		CHECK( list.isEmpty() );
		CHECK( other.tryPushBack( &a ) == SIR_CONTAINER_OK );
	}
	CHECK( state.nLiveBytes == 0 );
	return 0;
}
