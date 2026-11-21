/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Core/Containers/SIRRankSelectBitVector.h>
#include <Core/Containers/SIRSuccinctWaveletMatrix.h>
#include <Core/Containers/SIRColony.h>
#include <Core/Containers/SIRAdaptiveRoaringDirectory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <new>
#include <vector>
#include <set>
#include <random>
#include <stdexcept>
#if defined( _MSC_VER )
#include <malloc.h>
#endif

#define REQUIRE( b )                                                                                                                       \
	do {                                                                                                                                   \
		if ( !( b ) ) {                                                                                                                    \
			fprintf( stderr, "Optimization test failed at line %u: %s\n", ( unsigned )__LINE__, #b );                                      \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

struct sirOptimizationDomain_t
{
	uint64_t nCurrent = 0;
	uint64_t nPeak = 0;
	uint32_t nAllocations = 0;
	uint32_t nFailureAt = 0;
};

static void *allocate( void *pContext, size_t nBytes, size_t nAlignment ) noexcept
{
	sirOptimizationDomain_t *pDomain = ( sirOptimizationDomain_t * )pContext;
	++pDomain->nAllocations;
	if ( pDomain->nAllocations == pDomain->nFailureAt ) {
		return nullptr;
	}
	if ( nAlignment < sizeof( void * ) ) {
		nAlignment = sizeof( void * );
	}
	void *pMemory = nullptr;
#if defined( _MSC_VER )
	pMemory = _aligned_malloc( nBytes, nAlignment );
	if ( pMemory == nullptr ) {
		return nullptr;
	}
#else
	if ( posix_memalign( &pMemory, nAlignment, nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	pDomain->nCurrent += nBytes;
	if ( pDomain->nCurrent > pDomain->nPeak ) {
		pDomain->nPeak = pDomain->nCurrent;
	}
	return pMemory;
}

static void deallocate( void *pContext, void *pMemory, size_t nBytes, size_t ) noexcept
{
	if ( pMemory == nullptr ) {
		return;
	}
	sirOptimizationDomain_t *pDomain = ( sirOptimizationDomain_t * )pContext;
	REQUIRE( pDomain->nCurrent >= nBytes );
	pDomain->nCurrent -= nBytes;
#if defined( _MSC_VER )
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

static sirContainerAllocator_t makeAllocator( sirOptimizationDomain_t *pDomain ) noexcept
{
	return { pDomain, allocate, deallocate, 91, 23 };
}

static void testRankSelect( const sirContainerAllocator_t *pAllocator, sirOptimizationDomain_t *pDomain )
{
	std::mt19937 rng( 9123 );
	constexpr uint32_t LENGTHS[] = { 0, 1, 2, 7, 8, 31, 32, 63, 64, 65, 127, 128, 129, 255, 256, 257, 4096 };
	for ( uint32_t nBits : LENGTHS ) {
		for ( uint32_t iPattern = 0; iPattern < 5; ++iPattern ) {
			std::vector<uint64_t> words( ( nBits + 63u ) / 64u );
			std::vector<uint32_t> zeros;
			std::vector<uint32_t> ones;
			for ( uint32_t iBit = 0; iBit < nBits; ++iBit ) {
				const bool bOne = iPattern == 0 || ( iPattern == 2 && ( iBit & 1 ) != 0 ) || ( iPattern == 3 && ( rng() % 4 ) != 0 ) ||
				                  ( iPattern == 4 && ( rng() % 4 ) == 0 );
				if ( bOne ) {
					words[iBit / 64] |= uint64_t( 1 ) << ( iBit % 64 );
					ones.push_back( iBit );
				} else {
					zeros.push_back( iBit );
				}
			}
			SIRRankSelectBitVector bits( pAllocator );
			REQUIRE( bits.tryBuild( words.empty() ? nullptr : words.data(), nBits ) == SIR_CONTAINER_OK );
			REQUIRE( bits.getSize() == nBits && bits.getOnes() == ones.size() );
			uint32_t nSeen = 0;
			for ( uint32_t iBit = 0; iBit < nBits; ++iBit ) {
				REQUIRE( bits.rank1( iBit ) == nSeen );
				if ( bits.contains( iBit ) ) {
					++nSeen;
				}
			}
			REQUIRE( bits.rank1( nBits + 300 ) == ones.size() );
			for ( uint32_t iIndex = 0; iIndex < ones.size(); ++iIndex ) {
				REQUIRE( bits.select1( iIndex ) == ones[iIndex] );
				REQUIRE( bits.select1Fast( iIndex ) == ones[iIndex] );
			}
			for ( uint32_t iIndex = 0; iIndex < zeros.size(); ++iIndex ) {
				REQUIRE( bits.select0( iIndex ) == zeros[iIndex] );
			}
			REQUIRE( bits.select0( zeros.size() ) == UINT32_MAX );
			REQUIRE( bits.select1( ones.size() ) == UINT32_MAX );
			REQUIRE( bits.select1Fast( ones.size() ) == UINT32_MAX );
		}
	}
	{
		const uint64_t nInitial = ~uint64_t( 0 );
		SIRRankSelectBitVector bits( pAllocator );
		REQUIRE( bits.tryBuild( &nInitial, 64 ) == SIR_CONTAINER_OK );
		const uint32_t nBefore = pDomain->nAllocations;
		pDomain->nFailureAt = nBefore + 1;
		REQUIRE( bits.tryBuild( &nInitial, 32 ) == SIR_CONTAINER_OUT_OF_MEMORY );
		pDomain->nFailureAt = 0;
		REQUIRE( bits.getSize() == 64 && bits.getOnes() == 64 );
	}
}

static void testWavelet( const sirContainerAllocator_t *pAllocator, sirOptimizationDomain_t *pDomain )
{
	std::mt19937 rng( 2135 );
	constexpr uint32_t LENGTHS[] = { 0, 1, 2, 7, 64, 65, 128, 512, 4096 };
	constexpr uint32_t WIDTHS[] = { 1, 2, 8, 16, 32 };
	for ( uint32_t nBits : WIDTHS ) {
		for ( uint32_t nCount : LENGTHS ) {
			const uint32_t nMask = nBits == 32 ? UINT32_MAX : ( uint32_t( 1 ) << nBits ) - 1;
			std::vector<uint32_t> values( nCount );
			for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
				values[iIndex] = ( uint32_t )rng() & nMask;
			}
			SIRSuccinctWaveletMatrix wavelet( pAllocator );
			REQUIRE( wavelet.tryBuild( values.empty() ? nullptr : values.data(), nCount, nBits ) == SIR_CONTAINER_OK );
			REQUIRE( wavelet.getSize() == nCount && wavelet.access( nCount ) == UINT32_MAX );
			for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
				const uint32_t nValue = values[iIndex];
				uint32_t nOrdinal = 0;
				for ( uint32_t iPrefix = 0; iPrefix < iIndex; ++iPrefix ) {
					nOrdinal += values[iPrefix] == nValue;
				}
				REQUIRE( wavelet.access( iIndex ) == nValue );
				REQUIRE( wavelet.rank( nValue, iIndex ) == nOrdinal );
				REQUIRE( wavelet.select( nValue, nOrdinal ) == iIndex );
			}
			REQUIRE( wavelet.select( 0, nCount + 1 ) == UINT32_MAX );
		}
	}
	SIRSuccinctWaveletMatrix wavelet( pAllocator );
	const uint32_t values[] = { 5, 7, 5 };
	REQUIRE( wavelet.tryBuild( values, 3, 3 ) == SIR_CONTAINER_OK );
	pDomain->nFailureAt = pDomain->nAllocations + 1;
	REQUIRE( wavelet.tryBuild( values, 3, 3 ) == SIR_CONTAINER_OUT_OF_MEMORY );
	pDomain->nFailureAt = 0;
	REQUIRE( wavelet.select( 5, 1 ) == 2 && wavelet.select( 7, 0 ) == 1 );
	REQUIRE( wavelet.tryBuild( values, 3, 0 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	REQUIRE( wavelet.tryBuild( values, 3, 33 ) == SIR_CONTAINER_INVALID_ARGUMENT );
}

struct alignas( 128 ) sirOptimizedObject_t
{
	static uint32_t s_nAlive;
	static bool s_bThrow;
	uint64_t m_nValue;
	explicit sirOptimizedObject_t( uint64_t nValue ) : m_nValue( nValue )
	{
		if ( s_bThrow ) {
			throw std::runtime_error( "construction rejected" );
		}
		++s_nAlive;
	}
	~sirOptimizedObject_t( void ) noexcept
	{
		--s_nAlive;
	}
};
uint32_t sirOptimizedObject_t::s_nAlive = 0;
bool sirOptimizedObject_t::s_bThrow = false;

template <uint32_t N>
static void testColonyPolicy( const sirContainerAllocator_t *pAllocator, sirOptimizationDomain_t *pDomain )
{
	SIRColony<sirOptimizedObject_t, N> colony( pAllocator );
	std::vector<sirOptimizedObject_t *> pointers( 1024 );
	std::vector<bool> live( 1024, true );
	for ( uint32_t iIndex = 0; iIndex < pointers.size(); ++iIndex ) {
		REQUIRE( colony.tryEmplace( &pointers[iIndex], iIndex ) == SIR_CONTAINER_OK );
		REQUIRE( ( ( uintptr_t )pointers[iIndex] % 128 ) == 0 );
	}
	REQUIRE( colony.getSize() == pointers.size() );
	for ( uint32_t iIndex = 0; iIndex < pointers.size(); iIndex += 3 ) {
		REQUIRE( colony.erase( pointers[iIndex] ) );
		live[iIndex] = false;
		REQUIRE( !colony.erase( pointers[iIndex] ) );
	}
	const uint32_t nAllocationsBefore = pDomain->nAllocations;
	for ( uint32_t iIndex = 0; iIndex < pointers.size(); iIndex += 3 ) {
		sirOptimizedObject_t *pNew = nullptr;
		REQUIRE( colony.tryEmplace( &pNew, iIndex + 9999 ) == SIR_CONTAINER_OK );
		REQUIRE( pNew != nullptr && pNew->m_nValue == iIndex + 9999 );
	}
	REQUIRE( colony.getSize() == pointers.size() );
	REQUIRE( pDomain->nAllocations == nAllocationsBefore );
	uint32_t nVisited = 0;
	colony.forEach( [&]( const sirOptimizedObject_t& ) { ++nVisited; } );
	REQUIRE( nVisited == pointers.size() );
	for ( uint32_t iIndex = 0; iIndex < pointers.size(); ++iIndex ) {
		if ( live[iIndex] ) {
			REQUIRE( pointers[iIndex]->m_nValue == iIndex );
		}
	}
	sirOptimizedObject_t *pSentinel = nullptr;
	sirOptimizedObject_t::s_bThrow = true;
	bool bThrew = false;
	try {
		colony.tryEmplace( &pSentinel, 9 );
	} catch ( const std::runtime_error& ) {
		bThrew = true;
	}
	sirOptimizedObject_t::s_bThrow = false;
	REQUIRE( bThrew && pSentinel == nullptr && colony.getSize() == pointers.size() );
	const uint32_t nTotal = colony.getSize();
	colony.clear();
	REQUIRE( colony.getSize() == 0 && sirOptimizedObject_t::s_nAlive == 0 );
	REQUIRE( nTotal == 1024 );
}

static void testColonyFailure( const sirContainerAllocator_t *pAllocator, sirOptimizationDomain_t *pDomain )
{
	SIRColony<sirOptimizedObject_t, 1> colony( pAllocator );
	sirOptimizedObject_t *pItem = nullptr;
	const uint32_t nBefore = pDomain->nAllocations;
	pDomain->nFailureAt = nBefore + 1;
	REQUIRE( colony.tryEmplace( &pItem, 1 ) == SIR_CONTAINER_OUT_OF_MEMORY );
	pDomain->nFailureAt = 0;
	REQUIRE( colony.getSize() == 0 && pItem == nullptr );
	REQUIRE( colony.tryEmplace( &pItem, 8 ) == SIR_CONTAINER_OK );
	REQUIRE( colony.tryEmplace( nullptr, 9 ) == SIR_CONTAINER_INVALID_ARGUMENT );
	REQUIRE( colony.erase( pItem ) );
	REQUIRE( !colony.erase( pItem ) );
	colony.clear();
	REQUIRE( sirOptimizedObject_t::s_nAlive == 0 );
}

static void testRoaring( const sirContainerAllocator_t *pAllocator, sirOptimizationDomain_t *pDomain )
{
	std::mt19937 rng( 78125 );
	SIRAdaptiveRoaringDirectory bitmap( pAllocator );
	std::set<uint32_t> oracle;
	for ( uint32_t iRound = 0; iRound < 28000; ++iRound ) {
		const uint32_t nLow = rng() % 65536;
		const uint32_t nHigh = ( rng() % 4 ) << 16;
		const uint32_t nKey = nHigh | nLow;
		if ( ( rng() % 3 ) != 0 ) {
			REQUIRE( bitmap.tryAdd( nKey ) == SIR_CONTAINER_OK );
			oracle.insert( nKey );
		} else {
			REQUIRE( bitmap.erase( nKey ) == ( oracle.erase( nKey ) != 0 ) );
		}
		if ( ( iRound & 511 ) == 0 ) {
			REQUIRE( bitmap.getSize() == oracle.size() );
			std::set<uint32_t>::const_iterator it = oracle.begin();
			bitmap.forEach( [&]( uint32_t nCurrent ) {
				REQUIRE( it != oracle.end() && *it == nCurrent );
				++it;
			} );
			REQUIRE( it == oracle.end() );
		}
	}
	for ( uint32_t iIndex = 0; iIndex < 8192; ++iIndex ) {
		REQUIRE( bitmap.tryAdd( iIndex ) == SIR_CONTAINER_OK );
		oracle.insert( iIndex );
	}
	REQUIRE( bitmap.getSize() == oracle.size() );
	for ( uint32_t iIndex = 0; iIndex < 8192; ++iIndex ) {
		REQUIRE( bitmap.erase( iIndex ) );
		oracle.erase( iIndex );
	}
	REQUIRE( bitmap.getSize() == oracle.size() );
	const uint32_t nBefore = pDomain->nAllocations;
	pDomain->nFailureAt = nBefore + 1;
	REQUIRE( bitmap.tryAdd( UINT32_MAX ) == SIR_CONTAINER_OUT_OF_MEMORY );
	pDomain->nFailureAt = 0;
	REQUIRE( bitmap.getSize() == oracle.size() );
	REQUIRE( bitmap.tryAdd( UINT32_MAX ) == SIR_CONTAINER_OK );
	REQUIRE( !bitmap.erase( UINT32_MAX - 1 ) && bitmap.erase( UINT32_MAX ) );
}

int main( void )
{
	sirOptimizationDomain_t state{};
	const sirContainerAllocator_t domain = makeAllocator( &state );
	{
		testRankSelect( &domain, &state );
		testWavelet( &domain, &state );
		testColonyPolicy<1>( &domain, &state );
		testColonyPolicy<4>( &domain, &state );
		testColonyPolicy<16>( &domain, &state );
		testColonyPolicy<64>( &domain, &state );
		testColonyFailure( &domain, &state );
		testRoaring( &domain, &state );
	}
	REQUIRE( state.nCurrent == 0 && sirOptimizedObject_t::s_nAlive == 0 );
	return 0;
}
