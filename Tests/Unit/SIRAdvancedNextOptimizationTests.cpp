/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
This Source Code Form is subject to the terms of the Mozilla Public License, v2.
===========================================================================
*/
#include <Core/Containers/SIRAdaptiveRoaringDirectory.h>
#include <Core/Containers/SIRCSRBuilder.h>
#include <Core/Containers/SIRCellCSRIndex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>
#if defined( _MSC_VER )
#include <malloc.h>
#endif

#define REQUIRE( b )                                                                                                                       \
	do {                                                                                                                                   \
		if ( !( b ) ) {                                                                                                                    \
			fprintf( stderr, "Advanced optimization failure at line %u: %s\n", ( unsigned )__LINE__, #b );                                 \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

struct sirNextAllocationState_t
{
	uint64_t nLive = 0;
	uint64_t nPeak = 0;
	uint32_t nCalls = 0;
	uint32_t nFailAt = 0;
};

static void *allocateTracked( void *pContext, size_t nBytes, size_t nAlignment ) noexcept
{
	sirNextAllocationState_t *pState = ( sirNextAllocationState_t * )pContext;
	++pState->nCalls;
	if ( pState->nCalls == pState->nFailAt ) {
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
		return nullptr;
	}
#endif
	if ( pMemory != nullptr ) {
		pState->nLive += nBytes;
		pState->nPeak = pState->nPeak > pState->nLive ? pState->nPeak : pState->nLive;
	}
	return pMemory;
}

static void deallocateTracked( void *pContext, void *pMemory, size_t nBytes, size_t ) noexcept
{
	if ( pMemory == nullptr ) {
		return;
	}
	sirNextAllocationState_t *pState = ( sirNextAllocationState_t * )pContext;
	REQUIRE( pState->nLive >= nBytes );
	pState->nLive -= nBytes;
#if defined( _MSC_VER )
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

static sirContainerAllocator_t makeAllocator( sirNextAllocationState_t *pState ) noexcept
{
	return { pState, allocateTracked, deallocateTracked, 27, 19 };
}

static void checkBitmap( const SIRAdaptiveRoaringDirectory& bitmap, const std::set<uint32_t>& expected )
{
	REQUIRE( bitmap.getSize() == expected.size() );
	uint32_t nPrevious = 0;
	bool bFirst = true;
	uint64_t nCount = 0;
	auto iExpected = expected.begin();
	bitmap.forEach( [&]( uint32_t nValue ) {
		REQUIRE( iExpected != expected.end() );
		REQUIRE( nValue == *iExpected );
		REQUIRE( bFirst || nValue > nPrevious );
		bFirst = false;
		nPrevious = nValue;
		++nCount;
		++iExpected;
	} );
	REQUIRE( nCount == expected.size() && iExpected == expected.end() );
	for ( uint32_t nValue : expected ) {
		REQUIRE( bitmap.contains( nValue ) );
	}
}

static void checkBitmapCase( const sirContainerAllocator_t *pAllocator, uint32_t nItems, uint32_t nMode )
{
	SIRAdaptiveRoaringDirectory left( pAllocator );
	SIRAdaptiveRoaringDirectory right( pAllocator );
	SIRAdaptiveRoaringDirectory intersection( pAllocator );
	std::set<uint32_t> a, b, u, i;
	for ( uint32_t iIndex = 0; iIndex < nItems; ++iIndex ) {
		uint32_t nLeft = iIndex;
		uint32_t nRight = iIndex + nItems / 2;
		if ( nMode == 1 ) {
			nLeft = ( iIndex * 7919u ) % 65536;
			nRight = ( iIndex * 5521u + 193 ) % 65536;
		} else if ( nMode == 2 ) {
			nLeft = ( iIndex * 2654435761u ) ^ ( ( iIndex % 29 ) << 16 );
			nRight = ( iIndex * 2246822519u ) ^ ( ( ( iIndex + 7 ) % 29 ) << 16 );
		} else if ( nMode == 3 ) {
			nRight = iIndex;
		} else if ( nMode == 4 ) {
			nLeft = iIndex;
			nRight = ( iIndex * 7919u ) % 65536;
		}
		REQUIRE( left.tryAdd( nLeft ) == SIR_CONTAINER_OK );
		REQUIRE( intersection.tryAdd( nLeft ) == SIR_CONTAINER_OK );
		REQUIRE( right.tryAdd( nRight ) == SIR_CONTAINER_OK );
		a.insert( nLeft );
		b.insert( nRight );
	}
	for ( uint32_t nKey : a ) {
		u.insert( nKey );
		if ( b.count( nKey ) ) {
			i.insert( nKey );
		}
	}
	for ( uint32_t nKey : b ) {
		u.insert( nKey );
	}
	REQUIRE( left.tryUnionWith( right ) == SIR_CONTAINER_OK );
	checkBitmap( left, u );
	checkBitmap( right, b );
	REQUIRE( intersection.tryIntersectWith( right ) == SIR_CONTAINER_OK );
	checkBitmap( intersection, i );
	checkBitmap( right, b );
	REQUIRE( left.tryUnionWith( left ) == SIR_CONTAINER_OK );
	checkBitmap( left, u );
	REQUIRE( intersection.tryIntersectWith( intersection ) == SIR_CONTAINER_OK );
	checkBitmap( intersection, i );
}

static void testBitmapAlgebra( const sirContainerAllocator_t *pAllocator )
{
	const uint32_t szSizes[] = { 0, 1, 2, 63, 64, 65, 1024, 2048, 4095, 4096, 4097, 8192 };
	for ( uint32_t nCount : szSizes ) {
		for ( uint32_t iMode = 0; iMode < 5; ++iMode ) {
			checkBitmapCase( pAllocator, nCount, iMode );
		}
	}
	SIRAdaptiveRoaringDirectory a( pAllocator );
	SIRAdaptiveRoaringDirectory b( pAllocator );
	REQUIRE( a.tryAdd( UINT32_MAX ) == SIR_CONTAINER_OK );
	REQUIRE( b.tryAdd( 0 ) == SIR_CONTAINER_OK );
	REQUIRE( b.tryAdd( UINT32_MAX ) == SIR_CONTAINER_OK );
	REQUIRE( a.tryUnionWith( b ) == SIR_CONTAINER_OK );
	checkBitmap( a, std::set<uint32_t>{ 0, UINT32_MAX } );
	REQUIRE( a.tryIntersectWith( b ) == SIR_CONTAINER_OK );
	checkBitmap( a, std::set<uint32_t>{ 0, UINT32_MAX } );
	b.erase( UINT32_MAX );
	REQUIRE( a.tryIntersectWith( b ) == SIR_CONTAINER_OK );
	checkBitmap( a, std::set<uint32_t>{ 0 } );
}

static void testBitmapFailure( sirNextAllocationState_t *pState )
{
	const auto allocator = makeAllocator( pState );
	for ( uint32_t iMode = 0; iMode < 2; ++iMode ) {
		bool bSawFailure = false;
		bool bSawSuccess = false;
		for ( uint32_t nFailure = 1; nFailure <= 60; ++nFailure ) {
			SIRAdaptiveRoaringDirectory a( &allocator ), b( &allocator );
			std::set<uint32_t> initial, other, expected;
			for ( uint32_t i = 0; i < 1200; ++i ) {
				const uint32_t nLeft = ( i * 3331u ) % 65536u | ( ( i % 5u ) << 16 );
				const uint32_t nRight = ( i * 1259u + 7u ) % 65536u | ( ( ( i + 3u ) % 5u ) << 16 );
				REQUIRE( a.tryAdd( nLeft ) == SIR_CONTAINER_OK );
				REQUIRE( b.tryAdd( nRight ) == SIR_CONTAINER_OK );
				initial.insert( nLeft );
				other.insert( nRight );
			}
			if ( iMode == 0 ) {
				expected = initial;
				for ( uint32_t nValue : other ) {
					expected.insert( nValue );
				}
			} else {
				for ( uint32_t nValue : initial ) {
					if ( other.count( nValue ) ) {
						expected.insert( nValue );
					}
				}
			}
			pState->nFailAt = pState->nCalls + nFailure;
			const sirContainerResult_t eResult = iMode == 0 ? a.tryUnionWith( b ) : a.tryIntersectWith( b );
			pState->nFailAt = 0;
			if ( eResult == SIR_CONTAINER_OUT_OF_MEMORY ) {
				bSawFailure = true;
				checkBitmap( a, initial );
			} else {
				REQUIRE( eResult == SIR_CONTAINER_OK );
				bSawSuccess = true;
				checkBitmap( a, expected );
			}
			checkBitmap( b, other );
		}
		REQUIRE( bSawFailure && bSawSuccess );
	}
}

struct sirGraphRecord_t
{
	uint32_t nSource;
	uint32_t nDestination;
	uint32_t nValue;
	bool operator<( const sirGraphRecord_t& other ) const noexcept
	{
		if ( nSource != other.nSource ) {
			return nSource < other.nSource;
		}
		if ( nDestination != other.nDestination ) {
			return nDestination < other.nDestination;
		}
		return nValue < other.nValue;
	}
};

static void checkGraph( const SIRCSRGraph<uint32_t>& graph, uint32_t nVertices, std::vector<sirGraphRecord_t>& expected )
{
	REQUIRE( graph.getVertexCount() == nVertices );
	REQUIRE( graph.getEdgeCount() == expected.size() );
	std::sort( expected.begin(), expected.end() );
	std::vector<sirGraphRecord_t> actual;
	for ( uint32_t iVertex = 0; iVertex < nVertices; ++iVertex ) {
		uint32_t nLast = 0;
		for ( uint32_t iEdge = graph.getEdgeBegin( iVertex ); iEdge < graph.getEdgeEnd( iVertex ); ++iEdge ) {
			const uint32_t nTo = graph.getDestination( iEdge );
			REQUIRE( iEdge == graph.getEdgeBegin( iVertex ) || nLast <= nTo );
			nLast = nTo;
			const uint32_t *pValue = graph.tryGetValue( iEdge );
			REQUIRE( pValue != nullptr );
			actual.push_back( { iVertex, nTo, *pValue } );
		}
	}
	std::sort( actual.begin(), actual.end() );
	REQUIRE( actual.size() == expected.size() );
	for ( size_t i = 0; i < actual.size(); ++i ) {
		REQUIRE( actual[i].nSource == expected[i].nSource );
		REQUIRE( actual[i].nDestination == expected[i].nDestination );
		REQUIRE( actual[i].nValue == expected[i].nValue );
	}
}

static void testCSRBuilder( const sirContainerAllocator_t *pAllocator )
{
	const uint32_t szVertexCounts[] = { 0, 1, 2, 7, 64, 1024 };
	for ( uint32_t nVertices : szVertexCounts ) {
		for ( uint32_t nEdgeTarget : { 0u, 1u, 13u, 256u, 4096u } ) {
			SIRCSRBuilder<uint32_t> builder( pAllocator, nVertices );
			SIRCSRGraph<uint32_t> graph( pAllocator );
			std::vector<sirGraphRecord_t> expected;
			for ( uint32_t iEdge = 0; iEdge < nEdgeTarget && nVertices > 0; ++iEdge ) {
				const uint32_t iSource = iEdge * 2654435761u % nVertices;
				const uint32_t iDestination = ( iEdge * 19471637u + 33u ) % nVertices;
				REQUIRE( builder.tryAddEdge( iSource, iDestination, iEdge + 37u ) == SIR_CONTAINER_OK );
				expected.push_back( { iSource, iDestination, iEdge + 37u } );
			}
			REQUIRE( builder.tryAddEdge( nVertices, 0, 1u ) == SIR_CONTAINER_INVALID_ARGUMENT );
			REQUIRE( builder.tryFreeze( graph ) == SIR_CONTAINER_OK );
			checkGraph( graph, nVertices, expected );
		}
	}
}

static void testCSRFailure( sirNextAllocationState_t *pState )
{
	const auto allocator = makeAllocator( pState );
	SIRCSRBuilder<uint32_t> builder( &allocator, 19 );
	std::vector<sirGraphRecord_t> expected;
	for ( uint32_t iEdge = 0; iEdge < 400; ++iEdge ) {
		const uint32_t iSource = ( iEdge * 17u ) % 19u;
		const uint32_t iDestination = ( iEdge * 3u + 1 ) % 19u;
		REQUIRE( builder.tryAddEdge( iSource, iDestination, iEdge ) == SIR_CONTAINER_OK );
		expected.push_back( { iSource, iDestination, iEdge } );
	}
	SIRCSRGraph<uint32_t> graph( &allocator );
	REQUIRE( builder.tryFreeze( graph ) == SIR_CONTAINER_OK );
	for ( uint32_t nFailure = 1; nFailure <= 24; ++nFailure ) {
		pState->nFailAt = pState->nCalls + nFailure;
		const sirContainerResult_t eResult = builder.tryFreeze( graph );
		pState->nFailAt = 0;
		REQUIRE( eResult == SIR_CONTAINER_OK || eResult == SIR_CONTAINER_OUT_OF_MEMORY );
		checkGraph( graph, 19, expected );
		REQUIRE( builder.getPendingEdges() == 400 );
	}
	SIRCSRBuilder<uint32_t> huge( &allocator, UINT32_MAX );
	REQUIRE( huge.tryFreeze( graph ) == SIR_CONTAINER_SIZE_OVERFLOW );
	checkGraph( graph, 19, expected );
}

struct sirThrowingGraphValue_t
{
	static uint32_t s_nCopies;
	static uint32_t s_nThrowAfter;
	uint32_t nValue = 0;
	sirThrowingGraphValue_t( void ) noexcept = default;
	explicit sirThrowingGraphValue_t( uint32_t nInput ) noexcept : nValue( nInput )
	{
	}
	sirThrowingGraphValue_t( const sirThrowingGraphValue_t& other ) : nValue( other.nValue )
	{
		if ( ++s_nCopies == s_nThrowAfter ) {
			throw std::runtime_error( "deliberate graph value copy failure" );
		}
	}
	sirThrowingGraphValue_t( sirThrowingGraphValue_t&& ) noexcept = default;
	sirThrowingGraphValue_t& operator=( const sirThrowingGraphValue_t& ) = default;
	sirThrowingGraphValue_t& operator=( sirThrowingGraphValue_t&& ) noexcept = default;
};
uint32_t sirThrowingGraphValue_t::s_nCopies = 0;
uint32_t sirThrowingGraphValue_t::s_nThrowAfter = 0;

static void testCSRThrow( const sirContainerAllocator_t *pAllocator )
{
	SIRCSRBuilder<sirThrowingGraphValue_t> builder( pAllocator, 4 );
	for ( uint32_t iEdge = 0; iEdge < 20; ++iEdge ) {
		const sirThrowingGraphValue_t value( iEdge );
		REQUIRE( builder.tryAddEdge( iEdge % 4, ( iEdge * 3 ) % 4, value ) == SIR_CONTAINER_OK );
	}
	SIRCSRGraph<sirThrowingGraphValue_t> graph( pAllocator );
	REQUIRE( builder.tryFreeze( graph ) == SIR_CONTAINER_OK );
	const auto nSize = graph.getEdgeCount();
	sirThrowingGraphValue_t::s_nThrowAfter = sirThrowingGraphValue_t::s_nCopies + 6;
	bool bThrown = false;
	try {
		builder.tryFreeze( graph );
	} catch ( const std::runtime_error& ) {
		bThrown = true;
	}
	REQUIRE( bThrown );
	REQUIRE( graph.getEdgeCount() == nSize );
	for ( uint32_t iIndex = 0; iIndex < graph.getEdgeCount(); ++iIndex ) {
		REQUIRE( graph.tryGetValue( iIndex ) != nullptr );
	}
	sirThrowingGraphValue_t::s_nThrowAfter = 0;
}

static void testCellIndex( sirNextAllocationState_t *pState )
{
	const auto allocator = makeAllocator( pState );
	using Index = SIRCellCSRIndex<uint32_t, uint32_t>;
	const uint32_t szSizes[] = { 0, 1, 2, 3, 16, 64, 512, 4096 };
	for ( uint32_t nCount : szSizes ) {
		for ( uint32_t nMode = 0; nMode < 5; ++nMode ) {
			std::vector<Index::Entry> records;
			std::map<uint32_t, std::multiset<uint32_t>> oracle;
			for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
				const uint32_t nCell = nMode == 0 ? iIndex : nMode == 4 ? 7 : iIndex / 16;
				const uint32_t nHandle = iIndex * 2654435761u + 19;
				records.push_back( { nCell, nHandle } );
				oracle[nCell].insert( nHandle );
			}
			if ( nMode == 2 ) {
				std::reverse( records.begin(), records.end() );
			}
			if ( nMode == 3 ) {
				std::mt19937 generator( 1107 );
				std::shuffle( records.begin(), records.end(), generator );
			}
			Index index( &allocator );
			REQUIRE( index.tryBuild( records.data(), nCount ) == SIR_CONTAINER_OK );
			REQUIRE( index.getCellCount() == oracle.size() );
			for ( const auto& group : oracle ) {
				const uint32_t *pHandles = nullptr;
				uint32_t nHandles = 0;
				REQUIRE( index.find( group.first, &pHandles, &nHandles ) );
				REQUIRE( nHandles == group.second.size() );
				std::multiset<uint32_t> actual;
				for ( uint32_t iEntry = 0; iEntry < nHandles; ++iEntry ) {
					actual.insert( pHandles[iEntry] );
				}
				REQUIRE( actual == group.second );
			}
			const uint32_t *pSentinel = ( const uint32_t * )0x20;
			uint32_t nSentinel = 991;
			REQUIRE( !index.find( UINT32_MAX, &pSentinel, &nSentinel ) );
			REQUIRE( pSentinel == ( const uint32_t * )0x20 && nSentinel == 991 );
			REQUIRE( !index.find( 0, nullptr, &nSentinel ) );
		}
	}
	{
		using Descending = SIRCellCSRIndex<int32_t, uint32_t, std::greater<int32_t>>;
		const Descending::Entry records[] = { { 9, 1 }, { 9, 2 }, { 4, 3 }, { 2, 4 }, { -3, 5 } };
		Descending index( &allocator );
		REQUIRE( index.tryBuild( records, 5 ) == SIR_CONTAINER_OK );
		const uint32_t *pFound = nullptr;
		uint32_t nFound = 0;
		REQUIRE( index.find( 9, &pFound, &nFound ) && nFound == 2 );
		REQUIRE( index.find( -3, &pFound, &nFound ) && nFound == 1 && *pFound == 5 );
	}
	{
		Index index( &allocator );
		Index::Entry original[] = { { 11, 71 }, { 5, 72 }, { 5, 73 } };
		REQUIRE( index.tryBuild( original, 3 ) == SIR_CONTAINER_OK );
		std::vector<Index::Entry> ordered;
		std::vector<Index::Entry> shuffled;
		for ( uint32_t iIndex = 0; iIndex < 512; ++iIndex ) {
			ordered.push_back( { iIndex / 4, iIndex + 100 } );
			shuffled.push_back( { iIndex / 4, iIndex + 100 } );
		}
		std::mt19937 generator( 207 );
		std::shuffle( shuffled.begin(), shuffled.end(), generator );
		for ( uint32_t nFailure = 1; nFailure <= 8; ++nFailure ) {
			for ( uint32_t nOrder = 0; nOrder < 2; ++nOrder ) {
				pState->nFailAt = pState->nCalls + nFailure;
				const auto& input = nOrder ? shuffled : ordered;
				const sirContainerResult_t eResult = index.tryBuild( input.data(), ( uint32_t )input.size() );
				pState->nFailAt = 0;
				REQUIRE( eResult == SIR_CONTAINER_OK || eResult == SIR_CONTAINER_OUT_OF_MEMORY );
				const uint32_t *pFound = nullptr;
				uint32_t nFound = 0;
				if ( eResult == SIR_CONTAINER_OUT_OF_MEMORY ) {
					REQUIRE( index.find( 11, &pFound, &nFound ) || index.find( 5, &pFound, &nFound ) );
				} else {
					REQUIRE( index.getCellCount() == 128 );
				}
			}
		}
	}
	REQUIRE( pState->nLive == 0 );
}

int main( void )
{
	sirNextAllocationState_t state{};
	const auto allocator = makeAllocator( &state );
	{
		testBitmapAlgebra( &allocator );
		testBitmapFailure( &state );
		testCSRBuilder( &allocator );
		testCSRFailure( &state );
		testCSRThrow( &allocator );
		testCellIndex( &state );
	}
	REQUIRE( state.nLive == 0 );
	return 0;
}
