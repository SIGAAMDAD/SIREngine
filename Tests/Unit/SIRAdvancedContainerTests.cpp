/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
This Source Code Form is subject to the terms of the Mozilla Public License, v2.
===========================================================================
*/
#include <Core/Containers/SIRPackedMemoryMap.h>
#include <Core/Containers/SIRFrozenHashIndex.h>
#include <Core/Containers/SIRFrozenSortedIndex.h>
#include <Core/Containers/SIRColony.h>
#include <Core/Containers/SIRAdaptiveSmallMap.h>
#include <Core/Containers/SIRCSRBuilder.h>
#include <Core/Containers/SIRRankSelectBitVector.h>
#include <Core/Containers/SIREliasFanoSet.h>
#include <Core/Containers/SIRBinaryFuseFilter.h>
#include <Core/Containers/SIRAoSoABlockVector.h>
#include <Core/Containers/SIRReadMostlyMap.h>
#include <Core/Containers/SIRAdaptiveRoaringDirectory.h>
#include <Core/Containers/SIRS3FIFOIndex.h>
#include <Core/Containers/SIRTinyLFUAdmissionIndex.h>
#include <Core/Containers/SIRCellCSRIndex.h>
#include <Core/Containers/SIRPackedCSRGraph.h>
#include <Core/Containers/SIRBatchMpmcQueue.h>
#include <Core/Containers/SIRHierarchicalTimingWheel.h>
#include <Core/Containers/SIRCompactSparseMultimap.h>
#include <Core/Containers/SIRSuccinctWaveletMatrix.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <set>
#include <algorithm>
#include <random>
#include <vector>
#include <Core/Platform/Shared/Threading/SIRThread.h>
#include <atomic>
#include <stdexcept>

#define SIR_ASSERT( b )                                                                                                                    \
	do {                                                                                                                                   \
		if ( !( b ) ) {                                                                                                                    \
			fprintf( stderr, "Advanced container failure line %d: %s\n", __LINE__, #b );                                                   \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

struct sirAdvancedTestAllocator_t
{
	size_t nLiveBytes = 0;
	size_t nPeakBytes = 0;
	uint32_t nAllocations = 0;
	uint32_t nFailAt = 0;
};

static void *allocateTracked( void *pContext, size_t nBytes, size_t nAlignment )
{
	sirAdvancedTestAllocator_t *pState = ( sirAdvancedTestAllocator_t * )pContext;
	++pState->nAllocations;
	if ( pState->nFailAt == pState->nAllocations ) {
		return nullptr;
	}
	void *pMemory = nullptr;
	if ( nAlignment < sizeof( void * ) ) {
		nAlignment = sizeof( void * );
	}
#if defined( _MSC_VER )
	pMemory = _aligned_malloc( nBytes, nAlignment );
#else
	if ( posix_memalign( &pMemory, nAlignment, nBytes ) != 0 ) {
		pMemory = nullptr;
	}
#endif
	if ( pMemory != nullptr ) {
		pState->nLiveBytes += nBytes;
		if ( pState->nLiveBytes > pState->nPeakBytes ) {
			pState->nPeakBytes = pState->nLiveBytes;
		}
	}
	return pMemory;
}

static void deallocateTracked( void *pContext, void *pMemory, size_t nBytes, size_t )
{
	sirAdvancedTestAllocator_t *pState = ( sirAdvancedTestAllocator_t * )pContext;
	SIR_ASSERT( pState->nLiveBytes >= nBytes );
	pState->nLiveBytes -= nBytes;
#if defined( _MSC_VER )
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

static sirContainerAllocator_t allocator( sirAdvancedTestAllocator_t *pState ) noexcept
{
	return { pState, allocateTracked, deallocateTracked, 771, 17 };
}

static void testFrozen( const sirContainerAllocator_t *pAllocator, sirAdvancedTestAllocator_t *pState )
{
	SIRFrozenSortedIndex<int, int> sorted( pAllocator );
	SIRFrozenSortedIndex<int, int>::Entry values[] = { { 1, 11 }, { 5, 15 }, { 6, 16 }, { 20, 30 } };
	SIR_ASSERT( sorted.tryBuildSorted( values, 4 ) == SIR_CONTAINER_OK );
	SIR_ASSERT( sorted.getSize() == 4 && *sorted.find( 5 ) == 15 && sorted.find( 7 ) == nullptr );
	SIR_ASSERT( sorted.tryBuildSorted( values, 0 ) == SIR_CONTAINER_OK && sorted.getSize() == 0 );
	SIR_ASSERT( sorted.tryBuildSorted( values, 4 ) == SIR_CONTAINER_OK );
	values[1].key = 1;
	SIR_ASSERT( sorted.tryBuildSorted( values, 4 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	SIR_ASSERT( sorted.getSize() == 4 && *sorted.find( 5 ) == 15 );
	values[1].key = 5;
	pState->nFailAt = pState->nAllocations + 1;
	SIR_ASSERT( sorted.tryBuildSorted( values, 4 ) == SIR_CONTAINER_OUT_OF_MEMORY );
	pState->nFailAt = 0;
	SIR_ASSERT( *sorted.find( 5 ) == 15 );
	SIRFrozenHashIndex<uint32_t, uint32_t> frozen( pAllocator );
	SIRFrozenHashIndex<uint32_t, uint32_t>::Entry hashValues[400];
	for ( uint32_t iIndex = 0; iIndex < 400; ++iIndex ) {
		hashValues[iIndex] = { iIndex * 2654435761u, iIndex + 3 };
	}
	SIR_ASSERT( frozen.tryBuild( hashValues, 400 ) == SIR_CONTAINER_OK );
	for ( uint32_t iIndex = 0; iIndex < 400; ++iIndex ) {
		SIR_ASSERT( frozen.find( hashValues[iIndex].key ) != nullptr );
		SIR_ASSERT( *frozen.find( hashValues[iIndex].key ) == iIndex + 3 );
	}
	SIR_ASSERT( frozen.find( 99993 ) == nullptr );
	hashValues[4].key = hashValues[3].key;
	SIR_ASSERT( frozen.tryBuild( hashValues, 400 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	SIR_ASSERT( *frozen.find( hashValues[5].key ) == 8 );
}

struct alignas( 128 ) sirStableObject_t
{
	static uint32_t s_nLive;
	uint64_t nData[16]{};
	explicit sirStableObject_t( uint64_t nValue ) noexcept
	{
		nData[0] = nValue;
		++s_nLive;
	}
	~sirStableObject_t( void ) noexcept
	{
		--s_nLive;
	}
};
uint32_t sirStableObject_t::s_nLive = 0;

static void testColonyAndSmall( const sirContainerAllocator_t *pAllocator, sirAdvancedTestAllocator_t *pState )
{
	{
		SIRColony<sirStableObject_t, 16> colony( pAllocator );
		sirStableObject_t *szPointers[300]{};
		for ( uint32_t iIndex = 0; iIndex < 300; ++iIndex ) {
			SIR_ASSERT( colony.tryEmplace( &szPointers[iIndex], iIndex * 7 ) == SIR_CONTAINER_OK );
			SIR_ASSERT( ( ( uintptr_t )szPointers[iIndex] & 127 ) == 0 );
		}
		for ( uint32_t iIndex = 0; iIndex < 300; iIndex += 2 ) {
			SIR_ASSERT( colony.erase( szPointers[iIndex] ) );
		}
		for ( uint32_t iIndex = 1; iIndex < 300; iIndex += 2 ) {
			SIR_ASSERT( szPointers[iIndex]->nData[0] == iIndex * 7 );
		}
		for ( uint32_t iIndex = 0; iIndex < 150; ++iIndex ) {
			sirStableObject_t *pValue = nullptr;
			SIR_ASSERT( colony.tryEmplace( &pValue, uint64_t( 900 + iIndex ) ) == SIR_CONTAINER_OK );
		}
		uint32_t nVisited = 0;
		colony.forEach( [&]( const sirStableObject_t& ) { ++nVisited; } );
		SIR_ASSERT( nVisited == 300 && colony.getSize() == 300 );
		colony.clear();
		SIR_ASSERT( sirStableObject_t::s_nLive == 0 );
	}
	{
		SIRAdaptiveSmallMap<uint32_t, uint32_t, 4> small( pAllocator );
		for ( uint32_t iIndex = 0; iIndex < 4; ++iIndex ) {
			SIR_ASSERT( small.tryInsert( iIndex, iIndex + 7 ) == SIR_CONTAINER_OK );
		}
		SIR_ASSERT( !small.isHashed() && small.getSize() == 4 );
		pState->nFailAt = pState->nAllocations + 1;
		SIR_ASSERT( small.tryInsert( 99, 12 ) == SIR_CONTAINER_OUT_OF_MEMORY );
		pState->nFailAt = 0;
		SIR_ASSERT( !small.isHashed() && small.getSize() == 4 );
		SIR_ASSERT( small.tryInsert( 99, 12 ) == SIR_CONTAINER_OK && small.isHashed() );
		SIR_ASSERT( small.erase( 99 ) && !small.erase( 99 ) );
		for ( uint32_t iIndex = 0; iIndex < 4; ++iIndex ) {
			SIR_ASSERT( *small.find( iIndex ) == iIndex + 7 );
		}
	}
}

static void testBitIndexes( const sirContainerAllocator_t *pAllocator )
{
	std::mt19937 rng( 778 );
	for ( uint32_t nSize : { 0u, 1u, 63u, 64u, 65u, 129u, 3000u } ) {
		const uint32_t nWords = ( nSize + 63 ) / 64;
		std::vector<uint64_t> bits( nWords, 0 );
		for ( uint32_t iIndex = 0; iIndex < nSize; ++iIndex ) {
			if ( ( rng() % 5 ) == 0 ) {
				bits[iIndex / 64] |= uint64_t( 1 ) << ( iIndex % 64 );
			}
		}
		SIRRankSelectBitVector index( pAllocator );
		SIR_ASSERT( index.tryBuild( bits.empty() ? nullptr : bits.data(), nSize ) == SIR_CONTAINER_OK );
		uint32_t nOnes = 0;
		for ( uint32_t iIndex = 0; iIndex < nSize; ++iIndex ) {
			SIR_ASSERT( index.rank1( iIndex ) == nOnes );
			if ( ( bits[iIndex / 64] >> ( iIndex % 64 ) ) & 1 ) {
				SIR_ASSERT( index.select1( nOnes ) == iIndex );
				++nOnes;
			}
		}
		SIR_ASSERT( index.rank1( nSize ) == nOnes && index.getOnes() == nOnes );
		SIR_ASSERT( index.select1( nOnes ) == UINT32_MAX );
	}
	const uint64_t szValues[] = { 0, 1, 3, 7, 8, 1025, 65536, ( uint64_t( 1 ) << 50 ) + 11 };
	SIREliasFanoSet ef( pAllocator );
	SIR_ASSERT( ef.tryBuildSorted( szValues, 8 ) == SIR_CONTAINER_OK );
	for ( uint32_t iIndex = 0; iIndex < 8; ++iIndex ) {
		SIR_ASSERT( ef.select( iIndex ) == szValues[iIndex] && ef.contains( szValues[iIndex] ) );
	}
	SIR_ASSERT( !ef.contains( 9 ) && ef.select( 8 ) == UINT64_MAX );
	const uint64_t szInvalid[] = { 1, 1 };
	SIR_ASSERT( ef.tryBuildSorted( szInvalid, 2 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	SIRBinaryFuseFilter filter( pAllocator );
	std::vector<uint64_t> positives;
	for ( uint32_t iIndex = 0; iIndex < 1024; ++iIndex ) {
		positives.push_back( ( uint64_t )iIndex * 0x9e3779b97f4a7c15ull );
	}
	SIR_ASSERT( filter.tryBuild( positives.data(), ( uint32_t )positives.size() ) == SIR_CONTAINER_OK );
	for ( uint64_t nKey : positives ) {
		SIR_ASSERT( filter.mayContain( nKey ) );
	}
	SIR_ASSERT( filter.tryBuild( szInvalid, 2 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	SIRBinaryFuseFilter empty( pAllocator );
	SIR_ASSERT( empty.tryBuild( nullptr, 0 ) == SIR_CONTAINER_OK );
	SIR_ASSERT( !empty.mayContain( 7 ) );
	for ( uint32_t nItems : { 1u, 2u, 7u, 64u, 512u, 8192u } ) {
		SIRBinaryFuseFilter target( pAllocator );
		std::vector<uint64_t> keys;
		for ( uint32_t iIndex = 0; iIndex < nItems; ++iIndex ) {
			keys.push_back( ( uint64_t )rng() * 0xa0761d6478bd642full + iIndex );
		}
		SIR_ASSERT( target.tryBuild( keys.data(), nItems ) == SIR_CONTAINER_OK );
		for ( uint64_t nKey : keys ) {
			SIR_ASSERT( target.mayContain( nKey ) );
		}
	}
	SIRSuccinctWaveletMatrix wavelet( pAllocator );
	std::vector<uint32_t> symbols( 2000 );
	for ( uint32_t& nValue : symbols ) {
		nValue = rng() % 29;
	}
	SIR_ASSERT( wavelet.tryBuild( symbols.data(), ( uint32_t )symbols.size(), 5 ) == SIR_CONTAINER_OK );
	for ( uint32_t iIndex = 0; iIndex < symbols.size(); ++iIndex ) {
		SIR_ASSERT( wavelet.access( iIndex ) == symbols[iIndex] );
	}
	for ( uint32_t nKey = 0; nKey < 30; ++nKey ) {
		uint32_t nExpected = 0;
		for ( uint32_t iIndex = 0; iIndex < symbols.size(); ++iIndex ) {
			SIR_ASSERT( wavelet.rank( nKey, iIndex ) == nExpected );
			if ( symbols[iIndex] == nKey ) {
				SIR_ASSERT( wavelet.select( nKey, nExpected ) == iIndex );
				++nExpected;
			}
		}
		SIR_ASSERT( wavelet.rank( nKey, ( uint32_t )symbols.size() ) == nExpected );
		SIR_ASSERT( wavelet.select( nKey, nExpected ) == UINT32_MAX );
	}
}

struct sirAoSoAAliasingValue_t
{
	static bool s_bThrowCopy;
	uint32_t nValue = 0;
	sirAoSoAAliasingValue_t( void ) noexcept = default;
	explicit sirAoSoAAliasingValue_t( uint32_t nInput ) noexcept : nValue( nInput )
	{
	}
	sirAoSoAAliasingValue_t( const sirAoSoAAliasingValue_t& other ) : nValue( other.nValue )
	{
		if ( s_bThrowCopy ) {
			throw std::runtime_error( "deliberate AoSoA copy failure" );
		}
	}
	sirAoSoAAliasingValue_t& operator=( const sirAoSoAAliasingValue_t& other ) noexcept
	{
		nValue = other.nValue;
		return *this;
	}
};
bool sirAoSoAAliasingValue_t::s_bThrowCopy = false;

static void testAliasingAndRollback( const sirContainerAllocator_t *pAllocator )
{
	SIRAoSoABlockVector<sirAoSoAAliasingValue_t, sirAoSoAAliasingValue_t, 2> blocked( pAllocator );
	sirAoSoAAliasingValue_t first( 10 );
	sirAoSoAAliasingValue_t second( 20 );
	SIR_ASSERT( blocked.tryEmplaceBack( first, second ) == SIR_CONTAINER_OK );
	SIR_ASSERT( blocked.tryEmplaceBack( first, second ) == SIR_CONTAINER_OK );
	const sirAoSoAAliasingValue_t *pOriginal = blocked.tryGetFirst( 0 );
	const sirAoSoAAliasingValue_t *pOther = blocked.tryGetSecond( 1 );
	SIR_ASSERT( blocked.tryEmplaceBack( *pOriginal, *pOther ) == SIR_CONTAINER_OK );
	SIR_ASSERT( blocked.getSize() == 3 );
	SIR_ASSERT( blocked.tryGetFirst( 2 )->nValue == 10 && blocked.tryGetSecond( 2 )->nValue == 20 );
	const uint32_t nBefore = blocked.getSize();
	sirAoSoAAliasingValue_t::s_bThrowCopy = true;
	bool bThrown = false;
	try {
		blocked.tryEmplaceBack( first, second );
	} catch ( const std::runtime_error& ) {
		bThrown = true;
	}
	sirAoSoAAliasingValue_t::s_bThrowCopy = false;
	SIR_ASSERT( bThrown && blocked.getSize() == nBefore );
	SIR_ASSERT( blocked.tryGetFirst( 2 )->nValue == 10 );

	SIRPackedCSRGraph<uint64_t> graph( pAllocator );
	SIR_ASSERT( graph.tryInitialize( 4, 1 ) == SIR_CONTAINER_OK );
	SIR_ASSERT( graph.tryAddEdge( 0, 1, uint64_t( 1234 ) ) == SIR_CONTAINER_OK );
	const uint64_t *pAliased = nullptr;
	graph.forEachEdge( 0, [&]( uint32_t, const uint64_t& nValue ) { pAliased = &nValue; } );
	SIR_ASSERT( pAliased != nullptr );
	SIR_ASSERT( graph.tryAddEdge( 0, 2, *pAliased ) == SIR_CONTAINER_OK );
	uint32_t nCopied = 0;
	graph.forEachEdge( 0, [&]( uint32_t, const uint64_t& nValue ) {
		SIR_ASSERT( nValue == 1234 );
		++nCopied;
	} );
	SIR_ASSERT( nCopied == 2 );
}

static void testPackedAndGraphs( const sirContainerAllocator_t *pAllocator )
{
	SIRPackedMemoryMap<uint32_t, uint32_t> map( pAllocator );
	std::map<uint32_t, uint32_t> oracle;
	std::mt19937 rng( 149 );
	for ( uint32_t iIndex = 0; iIndex < 3000; ++iIndex ) {
		uint32_t nKey = rng() % 500;
		if ( ( rng() & 3 ) == 0 ) {
			SIR_ASSERT( map.erase( nKey ) == ( oracle.erase( nKey ) != 0 ) );
		} else if ( oracle.count( nKey ) == 0 ) {
			SIR_ASSERT( map.tryInsert( nKey, iIndex ) == SIR_CONTAINER_OK );
			oracle[nKey] = iIndex;
		}
		if ( iIndex % 100 == 0 ) {
			std::vector<std::pair<uint32_t, uint32_t>> seen;
			map.forEach( [&]( uint32_t nFound, uint32_t nValue ) { seen.emplace_back( nFound, nValue ); } );
			SIR_ASSERT( seen.size() == oracle.size() );
			uint32_t iSeen = 0;
			for ( const auto& pair : oracle ) {
				SIR_ASSERT( seen[iSeen].first == pair.first && seen[iSeen].second == pair.second );
				++iSeen;
			}
		}
	}
	SIRAoSoABlockVector<int, float, 8> aosoa( pAllocator );
	for ( uint32_t iIndex = 0; iIndex < 1000; ++iIndex ) {
		SIR_ASSERT( aosoa.tryEmplaceBack( iIndex * 3, iIndex * 0.25f ) == SIR_CONTAINER_OK );
	}
	for ( uint32_t iIndex = 0; iIndex < 1000; ++iIndex ) {
		SIR_ASSERT( *aosoa.tryGetFirst( iIndex ) == ( int )( iIndex * 3 ) );
		SIR_ASSERT( *aosoa.tryGetSecond( iIndex ) == iIndex * 0.25f );
	}
	SIRCSRBuilder<uint32_t> builder( pAllocator, 64 );
	SIRPackedCSRGraph<uint32_t> dynamic( pAllocator );
	SIR_ASSERT( dynamic.tryInitialize( 64, 2 ) == SIR_CONTAINER_OK );
	for ( uint32_t iIndex = 0; iIndex < 1000; ++iIndex ) {
		const uint32_t iSource = iIndex % 64;
		const uint32_t iDestination = ( iIndex * 17 ) % 64;
		SIR_ASSERT( builder.tryAddEdge( iSource, iDestination, iIndex ) == SIR_CONTAINER_OK );
		SIR_ASSERT( dynamic.tryAddEdge( iSource, iDestination, iIndex ) == SIR_CONTAINER_OK );
	}
	SIRCSRGraph<uint32_t> frozen( pAllocator );
	SIR_ASSERT( builder.tryFreeze( frozen ) == SIR_CONTAINER_OK && frozen.getEdgeCount() == 1000 );
	uint32_t nDynamic = 0;
	for ( uint32_t iSource = 0; iSource < 64; ++iSource ) {
		dynamic.forEachEdge( iSource, [&]( uint32_t, uint32_t ) { ++nDynamic; } );
	}
	SIR_ASSERT( nDynamic == 1000 && dynamic.getEdgeCount() == 1000 );
	SIR_ASSERT( dynamic.eraseEdge( 0, 0 ) && dynamic.getEdgeCount() == 999 );
	SIRCellCSRIndex<uint32_t, uint32_t> cell( pAllocator );
	SIRCellCSRIndex<uint32_t, uint32_t>::Entry entries[100];
	for ( uint32_t iIndex = 0; iIndex < 100; ++iIndex ) {
		entries[iIndex] = { iIndex % 7, iIndex };
	}
	SIR_ASSERT( cell.tryBuild( entries, 100 ) == SIR_CONTAINER_OK );
	const uint32_t *pRange = nullptr;
	uint32_t nRange = 0;
	SIR_ASSERT( cell.find( 3, &pRange, &nRange ) && nRange == 14 );
	SIRCompactSparseMultimap<uint32_t, uint32_t> multi( pAllocator );
	SIRCompactSparseMultimap<uint32_t, uint32_t>::Entry mapEntries[100];
	for ( uint32_t iIndex = 0; iIndex < 100; ++iIndex ) {
		mapEntries[iIndex] = { iIndex % 7, iIndex };
	}
	SIR_ASSERT( multi.tryBuild( mapEntries, 100 ) == SIR_CONTAINER_OK );
	SIR_ASSERT( multi.find( 3, &pRange, &nRange ) && nRange == 14 );
}

static void testRoaringAndCache( const sirContainerAllocator_t *pAllocator )
{
	SIRAdaptiveRoaringDirectory bitmap( pAllocator );
	std::set<uint32_t> oracle;
	std::mt19937 rng( 993 );
	for ( uint32_t iIndex = 0; iIndex < 14000; ++iIndex ) {
		const uint32_t nValue = ( rng() & 1 ) ? rng() % 65536 : rng();
		if ( ( rng() & 3 ) == 0 ) {
			SIR_ASSERT( bitmap.erase( nValue ) == ( oracle.erase( nValue ) != 0 ) );
		} else {
			SIR_ASSERT( bitmap.tryAdd( nValue ) == SIR_CONTAINER_OK );
			oracle.insert( nValue );
		}
		if ( iIndex % 1000 == 0 ) {
			std::vector<uint32_t> actual;
			bitmap.forEach( [&]( uint32_t nFound ) { actual.push_back( nFound ); } );
			SIR_ASSERT( actual.size() == oracle.size() );
			auto iterator = oracle.begin();
			for ( uint32_t iIndex = 0; iIndex < actual.size(); ++iIndex ) {
				SIR_ASSERT( actual[iIndex] == *iterator );
				++iterator;
			}
		}
	}
	SIRAdaptiveRoaringDirectory other( pAllocator );
	for ( uint32_t iIndex = 0; iIndex < 65536; ++iIndex ) {
		SIR_ASSERT( other.tryAdd( iIndex ) == SIR_CONTAINER_OK );
	}
	SIR_ASSERT( bitmap.tryUnionWith( other ) == SIR_CONTAINER_OK );
	for ( uint32_t iIndex = 0; iIndex < 65536; ++iIndex ) {
		SIR_ASSERT( bitmap.contains( iIndex ) );
	}
	SIRAdaptiveRoaringDirectory intersection( pAllocator );
	for ( uint32_t iIndex = 0; iIndex < 65536; ++iIndex ) {
		SIR_ASSERT( intersection.tryAdd( iIndex ) == SIR_CONTAINER_OK );
	}
	SIRAdaptiveRoaringDirectory even( pAllocator );
	for ( uint32_t iIndex = 0; iIndex < 65536; iIndex += 2 ) {
		SIR_ASSERT( even.tryAdd( iIndex ) == SIR_CONTAINER_OK );
	}
	SIR_ASSERT( intersection.tryIntersectWith( even ) == SIR_CONTAINER_OK );
	SIR_ASSERT( intersection.getSize() == 32768 );
	for ( uint32_t iIndex = 0; iIndex < 65536; ++iIndex ) {
		SIR_ASSERT( intersection.contains( iIndex ) == ( iIndex % 2 == 0 ) );
	}
	SIR_ASSERT( intersection.tryIntersectWith( intersection ) == SIR_CONTAINER_OK );
	SIR_ASSERT( intersection.getSize() == 32768 );
	SIR_ASSERT( intersection.tryIntersectWith( bitmap ) == SIR_CONTAINER_OK );
	SIR_ASSERT( intersection.getSize() == 32768 );
	SIRS3FIFOIndex fifo( pAllocator );
	SIR_ASSERT( fifo.tryInitialize( 64, 16 ) == SIR_CONTAINER_OK );
	for ( uint32_t iIndex = 0; iIndex < 1000; ++iIndex ) {
		bool bHit = false;
		SIR_ASSERT( fifo.tryAccess( iIndex % 200, &bHit ) == SIR_CONTAINER_OK );
		SIR_ASSERT( fifo.getResidentCount() <= 64 && fifo.getGhostCount() <= 16 );
	}
	SIRTinyLFUAdmissionIndex tlfu( pAllocator );
	SIR_ASSERT( tlfu.tryInitialize( 256, 4096 ) == SIR_CONTAINER_OK );
	for ( uint32_t iIndex = 0; iIndex < 1000; ++iIndex ) {
		tlfu.record( 7 );
	}
	SIR_ASSERT( tlfu.estimate( 7 ) >= tlfu.estimate( 12345678 ) );
}

static void testTiming( const sirContainerAllocator_t *pAllocator )
{
	SIRHierarchicalTimingWheel wheel( pAllocator );
	SIRHierarchicalTimingWheel::Handle handle{};
	SIR_ASSERT( wheel.trySchedule( 256, 18, &handle ) == SIR_CONTAINER_OK );
	SIR_ASSERT( wheel.cancel( handle ) && !wheel.cancel( handle ) );
	for ( uint32_t iIndex = 0; iIndex < 1500; ++iIndex ) {
		SIR_ASSERT( wheel.trySchedule( iIndex % 1024, iIndex, &handle ) == SIR_CONTAINER_OK );
	}
	std::vector<uint64_t> hits;
	wheel.advanceTo( 1030, [&]( uint64_t nPayload ) { hits.push_back( nPayload ); } );
	SIR_ASSERT( hits.size() == 1500 && wheel.getActiveCount() == 0 );
	for ( uint32_t iIndex = 0; iIndex < 1500; ++iIndex ) {
		bool bFound = false;
		for ( uint64_t nPayload : hits ) {
			if ( nPayload == iIndex ) {
				bFound = true;
				break;
			}
		}
		SIR_ASSERT( bFound );
	}
	SIR_ASSERT( wheel.trySchedule( 70000, 42, &handle ) == SIR_CONTAINER_OK );
	wheel.advanceTo( 70000, [&]( uint64_t nPayload ) { SIR_ASSERT( nPayload == 42 ); } );
	SIR_ASSERT( wheel.getActiveCount() == 0 );
	// Callback-created same-tick entries must be processed before advancing.
	SIR_ASSERT( wheel.trySchedule( 70001, 31, &handle ) == SIR_CONTAINER_OK );
	uint32_t nCallbacks = 0;
	wheel.advanceTo( 70001, [&]( uint64_t nPayload ) {
		++nCallbacks;
		if ( nPayload == 31 ) {
			SIRHierarchicalTimingWheel::Handle nested{};
			SIR_ASSERT( wheel.trySchedule( 70001, 32, &nested ) == SIR_CONTAINER_OK );
		}
	} );
	SIR_ASSERT( nCallbacks == 2 && wheel.getActiveCount() == 0 );
	SIRHierarchicalTimingWheel::Handle selfHandle{};
	SIR_ASSERT( wheel.trySchedule( 70002, 76, &selfHandle ) == SIR_CONTAINER_OK );
	wheel.advanceTo( 70002, [&]( uint64_t nPayload ) {
		SIR_ASSERT( nPayload == 76 );
		SIR_ASSERT( !wheel.cancel( selfHandle ) );
	} );
	SIR_ASSERT( wheel.getActiveCount() == 0 );
	SIR_ASSERT( wheel.trySchedule( 70003, 77, &handle ) == SIR_CONTAINER_OK );
	bool bThrew = false;
	try {
		wheel.advanceTo( 70003, [&]( uint64_t ) { throw std::runtime_error( "timer callback failure" ); } );
	} catch ( const std::runtime_error& ) {
		bThrew = true;
	}
	SIR_ASSERT( bThrew && wheel.getActiveCount() == 1 );
	wheel.advanceTo( 70003, [&]( uint64_t nPayload ) { SIR_ASSERT( nPayload == 77 ); } );
	SIR_ASSERT( wheel.getActiveCount() == 0 );
}

struct sirQueueThreadContext_t
{
	SIRBatchMpmcQueue<uint32_t> *pQueue;
	std::atomic<uint32_t> *pCounter;
	std::atomic<uint32_t> *pDuplicates;
	std::atomic<uint32_t> *pszSeen;
	uint32_t iWorker;
	uint32_t nItems;
};

static void queueProducer( void *pContext )
{
	sirQueueThreadContext_t *pWorker = ( sirQueueThreadContext_t * )pContext;
	for ( uint32_t iIndex = pWorker->iWorker; iIndex < pWorker->nItems; iIndex += 4 ) {
		while ( pWorker->pQueue->tryEnqueue( iIndex ) != SIR_CONTAINER_OK ) {
		}
		pWorker->pCounter->fetch_add( 1 );
	}
}

static void queueConsumer( void *pContext )
{
	sirQueueThreadContext_t *pWorker = ( sirQueueThreadContext_t * )pContext;
	while ( pWorker->pCounter->load() < pWorker->nItems ) {
		uint32_t nValue = 0;
		if ( pWorker->pQueue->tryDequeue( &nValue ) != SIR_CONTAINER_OK ) {
			continue;
		}
		if ( nValue >= pWorker->nItems ) {
			pWorker->pDuplicates->fetch_add( 1 );
		} else if ( pWorker->pszSeen[nValue].fetch_add( 1 ) != 0 ) {
			pWorker->pDuplicates->fetch_add( 1 );
		}
		pWorker->pCounter->fetch_add( 1 );
	}
}

struct sirReadThreadContext_t
{
	SIRReadMostlyMap<uint32_t, uint32_t> *pSnapshots;
	std::atomic<bool> *pStop;
	std::atomic<uint32_t> *pErrors;
};

static void snapshotReader( void *pContext )
{
	sirReadThreadContext_t *pWorker = ( sirReadThreadContext_t * )pContext;
	while ( !pWorker->pStop->load() ) {
		auto guard = pWorker->pSnapshots->read();
		for ( uint32_t iIndex = 0; iIndex < 20; ++iIndex ) {
			const uint32_t *pValue = guard.find( iIndex );
			if ( pValue == nullptr ) {
				pWorker->pErrors->fetch_add( 1 );
			} else if ( *pValue != iIndex + 10 && *pValue != iIndex + 20 ) {
				pWorker->pErrors->fetch_add( 1 );
			}
		}
	}
}

struct sirBatchThreadContext_t
{
	SIRBatchMpmcQueue<uint32_t> *pQueue;
	std::atomic<uint32_t> *pCounter;
	std::atomic<uint32_t> *pDuplicates;
	std::atomic<uint32_t> *pszSeen;
	uint32_t nItems;
};

static void batchProducer( void *pContext )
{
	sirBatchThreadContext_t *pWorker = ( sirBatchThreadContext_t * )pContext;
	constexpr uint32_t BATCH_SIZE = 8;
	for ( ;; ) {
		const uint32_t iFirst = pWorker->pCounter->fetch_add( BATCH_SIZE );
		if ( iFirst >= pWorker->nItems ) {
			break;
		}
		uint32_t szValues[BATCH_SIZE]{};
		for ( uint32_t iIndex = 0; iIndex < BATCH_SIZE; ++iIndex ) {
			szValues[iIndex] = iFirst + iIndex;
		}
		while ( pWorker->pQueue->tryEnqueueBatch( szValues, BATCH_SIZE ) != SIR_CONTAINER_OK ) {
		}
	}
}

static void batchConsumer( void *pContext )
{
	sirBatchThreadContext_t *pWorker = ( sirBatchThreadContext_t * )pContext;
	constexpr uint32_t BATCH_SIZE = 8;
	while ( pWorker->pCounter->load() < pWorker->nItems ) {
		uint32_t szValues[BATCH_SIZE]{};
		if ( pWorker->pQueue->tryDequeueBatch( szValues, BATCH_SIZE ) != SIR_CONTAINER_OK ) {
			continue;
		}
		for ( uint32_t iIndex = 0; iIndex < BATCH_SIZE; ++iIndex ) {
			const uint32_t nValue = szValues[iIndex];
			if ( nValue >= pWorker->nItems ) {
				pWorker->pDuplicates->fetch_add( 1 );
			} else if ( pWorker->pszSeen[nValue].fetch_add( 1 ) != 0 ) {
				pWorker->pDuplicates->fetch_add( 1 );
			}
		}
		pWorker->pCounter->fetch_add( BATCH_SIZE );
	}
}

static void testConcurrent( const sirContainerAllocator_t *pAllocator )
{
	constexpr uint32_t PRODUCERS = 4;
	constexpr uint32_t CONSUMERS = 4;
	constexpr uint32_t COUNT = 30000;
	constexpr uint32_t BATCH_COUNT = 32768;
	SIRThread szThreads[PRODUCERS + CONSUMERS];

	SIRBatchMpmcQueue<uint32_t> queue( pAllocator );
	SIR_ASSERT( queue.tryInitialize( 256 ) == SIR_CONTAINER_OK );
	uint32_t szInput[4] = { 11, 13, 17, 19 };
	uint32_t szOutput[4]{};
	SIR_ASSERT( queue.tryEnqueueBatch( szInput, 4 ) == SIR_CONTAINER_OK );
	SIR_ASSERT( queue.tryDequeueBatch( szOutput, 4 ) == SIR_CONTAINER_OK );
	SIR_ASSERT( szOutput[0] == 11 && szOutput[3] == 19 );

	std::atomic<uint32_t> nProduced( 0 );
	std::atomic<uint32_t> nConsumed( 0 );
	std::atomic<uint32_t> nDuplicates( 0 );
	std::atomic<uint32_t> szSeen[COUNT];
	for ( uint32_t iIndex = 0; iIndex < COUNT; ++iIndex ) {
		szSeen[iIndex].store( 0 );
	}
	sirQueueThreadContext_t szProducers[PRODUCERS]{};
	sirQueueThreadContext_t szConsumers[CONSUMERS]{};
	for ( uint32_t iWorker = 0; iWorker < PRODUCERS; ++iWorker ) {
		szProducers[iWorker] = { &queue, &nProduced, &nDuplicates, szSeen, iWorker, COUNT };
		SIR_ASSERT( szThreads[iWorker].start( queueProducer, &szProducers[iWorker] ) );
	}
	for ( uint32_t iWorker = 0; iWorker < CONSUMERS; ++iWorker ) {
		szConsumers[iWorker] = { &queue, &nConsumed, &nDuplicates, szSeen, iWorker, COUNT };
		SIR_ASSERT( szThreads[PRODUCERS + iWorker].start( queueConsumer, &szConsumers[iWorker] ) );
	}
	for ( uint32_t iWorker = 0; iWorker < PRODUCERS + CONSUMERS; ++iWorker ) {
		SIR_ASSERT( szThreads[iWorker].join() );
	}
	SIR_ASSERT( nProduced.load() == COUNT && nConsumed.load() == COUNT && nDuplicates.load() == 0 );
	for ( uint32_t iIndex = 0; iIndex < COUNT; ++iIndex ) {
		SIR_ASSERT( szSeen[iIndex].load() == 1 );
	}

	SIRReadMostlyMap<uint32_t, uint32_t> snapshots( pAllocator );
	SIRReadMostlyMap<uint32_t, uint32_t>::Entry szEntries[20];
	for ( uint32_t iIndex = 0; iIndex < 20; ++iIndex ) {
		szEntries[iIndex] = { iIndex, iIndex + 10 };
	}
	SIR_ASSERT( snapshots.tryPublish( szEntries, 20 ) == SIR_CONTAINER_OK );
	std::atomic<bool> bStop( false );
	std::atomic<uint32_t> nErrors( 0 );
	sirReadThreadContext_t szReaders[4]{};
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		szReaders[iWorker] = { &snapshots, &bStop, &nErrors };
		SIR_ASSERT( szThreads[iWorker].start( snapshotReader, &szReaders[iWorker] ) );
	}
	for ( uint32_t iIndex = 0; iIndex < 20; ++iIndex ) {
		szEntries[iIndex].value = iIndex + 20;
	}
	SIR_ASSERT( snapshots.tryPublish( szEntries, 20 ) == SIR_CONTAINER_OK );
	bStop.store( true );
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		SIR_ASSERT( szThreads[iWorker].join() );
	}
	SIR_ASSERT( nErrors.load() == 0 );

	SIRBatchMpmcQueue<uint32_t> batched( pAllocator );
	SIR_ASSERT( batched.tryInitialize( 64 ) == SIR_CONTAINER_OK );
	std::atomic<uint32_t> nNextProduce( 0 );
	std::atomic<uint32_t> nNextConsumed( 0 );
	std::atomic<uint32_t> nDuplicateBatch( 0 );
	std::atomic<uint32_t> szBatchSeen[BATCH_COUNT];
	for ( uint32_t iIndex = 0; iIndex < BATCH_COUNT; ++iIndex ) {
		szBatchSeen[iIndex].store( 0 );
	}
	sirBatchThreadContext_t producer = { &batched, &nNextProduce, &nDuplicateBatch, szBatchSeen, BATCH_COUNT };
	sirBatchThreadContext_t consumer = { &batched, &nNextConsumed, &nDuplicateBatch, szBatchSeen, BATCH_COUNT };
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		SIR_ASSERT( szThreads[iWorker].start( batchProducer, &producer ) );
	}
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		SIR_ASSERT( szThreads[4 + iWorker].start( batchConsumer, &consumer ) );
	}
	for ( uint32_t iWorker = 0; iWorker < 8; ++iWorker ) {
		SIR_ASSERT( szThreads[iWorker].join() );
	}
	SIR_ASSERT( nNextConsumed.load() == BATCH_COUNT && nDuplicateBatch.load() == 0 );
	for ( uint32_t iIndex = 0; iIndex < BATCH_COUNT; ++iIndex ) {
		SIR_ASSERT( szBatchSeen[iIndex].load() == 1 );
	}
}

int main( void )
{
	sirAdvancedTestAllocator_t state;
	const sirContainerAllocator_t alloc = allocator( &state );
	testFrozen( &alloc, &state );
	SIR_ASSERT( state.nLiveBytes == 0 );
	testColonyAndSmall( &alloc, &state );
	SIR_ASSERT( state.nLiveBytes == 0 );
	testBitIndexes( &alloc );
	SIR_ASSERT( state.nLiveBytes == 0 );
	testPackedAndGraphs( &alloc );
	testAliasingAndRollback( &alloc );
	SIR_ASSERT( state.nLiveBytes == 0 );
	testRoaringAndCache( &alloc );
	SIR_ASSERT( state.nLiveBytes == 0 );
	testTiming( &alloc );
	SIR_ASSERT( state.nLiveBytes == 0 );
	testConcurrent( &alloc );
	SIR_ASSERT( state.nLiveBytes == 0 );
	puts( "SIREngine advanced containers: 20 type smoke/differential suites passed" );
	return 0;
}
