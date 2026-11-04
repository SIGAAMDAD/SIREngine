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

#include <Core/Strings/SIRStringPool.h>
#include <gtest/gtest.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

/* An injectable test allocator. Shipping Core requires a SIRMemory domain. */
typedef struct sirPoolTestAllocator_s
{
	uint64_t nLive = 0;
	uint64_t nCalls = 0;
	uint64_t nDeallocations = 0;
	uint64_t nFailCall = 0;
} sirPoolTestAllocator_t;

static void *sirPoolTestAllocate( void *pContext, size_t nBytes, size_t )
{
	sirPoolTestAllocator_t *pState = ( sirPoolTestAllocator_t * )pContext;
	++pState->nCalls;
	if ( pState->nCalls == pState->nFailCall ) {
		return nullptr;
	}
	void *pMemory = malloc( nBytes );
	if ( pMemory != nullptr ) {
		pState->nLive += nBytes;
	}
	return pMemory;
}

static void sirPoolTestDeallocate( void *pContext, void *pMemory, size_t nBytes, size_t )
{
	sirPoolTestAllocator_t *pState = ( sirPoolTestAllocator_t * )pContext;
	++pState->nDeallocations;
	pState->nLive -= nBytes;
	free( pMemory );
}

static sirContainerAllocator_t sirPoolTestDomain( sirPoolTestAllocator_t *pState )
{
	return { pState, sirPoolTestAllocate, sirPoolTestDeallocate, 1u, 17u };
}

static sirStringPoolConfig_t sirPoolTestConfig( void )
{
	sirStringPoolConfig_t config = SIRStringPool::defaultConfig();
	config.nHashBuckets = 32;
	config.nMaxNames = 16;
	config.nDynamicTextBytes = 256;
	config.bTieredCache = false;
	config.nShortSlots = 2;
	config.nLongSlots = 1;
	config.nShortBytes = 16;
	config.nLongBytes = 1024;
	return config;
}

static void sirTestAppend16( std::vector<uint8_t>& bytes, uint16_t nValue )
{
	bytes.push_back( ( uint8_t )nValue );
	bytes.push_back( ( uint8_t )( nValue >> 8 ) );
}

static void sirTestAppend32( std::vector<uint8_t>& bytes, uint32_t nValue )
{
	sirTestAppend16( bytes, ( uint16_t )nValue );
	sirTestAppend16( bytes, ( uint16_t )( nValue >> 16 ) );
}

static void sirTestStore16( std::vector<uint8_t>& bytes, size_t iOffset, uint16_t nValue )
{
	bytes[iOffset] = ( uint8_t )nValue;
	bytes[iOffset + 1] = ( uint8_t )( nValue >> 8 );
}

static void sirTestStore32( std::vector<uint8_t>& bytes, size_t iOffset, uint32_t nValue )
{
	sirTestStore16( bytes, iOffset, ( uint16_t )nValue );
	sirTestStore16( bytes, iOffset + 2, ( uint16_t )( nValue >> 16 ) );
}

static void sirTestStore64( std::vector<uint8_t>& bytes, size_t iOffset, uint64_t nValue )
{
	sirTestStore32( bytes, iOffset, ( uint32_t )nValue );
	sirTestStore32( bytes, iOffset + 4, ( uint32_t )( nValue >> 32 ) );
}

static uint64_t sirTestHash( const uint8_t *pBytes, size_t nBytes )
{
	uint64_t nHash = 14695981039346656037ULL;
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash = ( nHash ^ pBytes[iByte] ) * 1099511628211ULL;
	}
	return nHash;
}

typedef struct sirTestCookedName_s
{
	std::string sText;
	bool bCompressed;
} sirTestCookedName_t;

static std::vector<uint8_t> sirTestCatalog( const std::vector<sirTestCookedName_t>& names )
{
	std::vector<uint8_t> bytes( 48, 0 );
	memcpy( bytes.data(), "SIRS", 4 );
	sirTestStore16( bytes, 4, SIR_STRING_CATALOG_VERSION );
	sirTestStore32( bytes, 8, ( uint32_t )names.size() );
	sirTestStore32( bytes, 12, ( uint32_t )names.size() * 8u );
	sirTestStore32( bytes, 44, 48 );
	std::vector<uint8_t> blob;
	uint64_t nCorpus = 14695981039346656037ULL;
	for ( const sirTestCookedName_t& name : names ) {
		sirTestAppend32( bytes, ( uint32_t )blob.size() );
		sirTestAppend16( bytes, ( uint16_t )name.sText.size() );
		if ( name.bCompressed ) {
			const size_t nEncoded = name.sText.size() + ( name.sText.size() + 127 ) / 128;
			sirTestAppend16( bytes, ( uint16_t )nEncoded );
			for ( size_t iByte = 0; iByte < name.sText.size(); ) {
				const size_t nRemaining = name.sText.size() - iByte;
				const size_t nChunk = nRemaining < 128 ? nRemaining : 128;
				blob.push_back( ( uint8_t )( nChunk - 1 ) );
				blob.insert( blob.end(), name.sText.begin() + iByte, name.sText.begin() + iByte + nChunk );
				iByte += nChunk;
			}
		} else {
			sirTestAppend16( bytes, ( uint16_t )( 0x8000u | name.sText.size() ) );
			blob.insert( blob.end(), name.sText.begin(), name.sText.end() );
			blob.push_back( 0 );
		}
		for ( unsigned char nCharacter : name.sText ) {
			nCorpus = ( nCorpus ^ nCharacter ) * 1099511628211ULL;
		}
		nCorpus = ( nCorpus ^ 0xffu ) * 1099511628211ULL;
	}
	sirTestStore32( bytes, 24, ( uint32_t )blob.size() );
	bytes.insert( bytes.end(), blob.begin(), blob.end() );
	sirTestStore64( bytes, 28, sirTestHash( bytes.data() + 48, bytes.size() - 48 ) );
	sirTestStore64( bytes, 36, nCorpus );
	return bytes;
}

static void sirTestRehashCatalog( std::vector<uint8_t>& bytes )
{
	if ( bytes.size() >= 48 ) {
		sirTestStore64( bytes, 28, sirTestHash( bytes.data() + 48, bytes.size() - 48 ) );
	}
}

static bool sirTestInitCatalog( SIRStringPool *pPool, const std::vector<uint8_t>& bytes, const sirStringPoolConfig_t& config,
                                std::vector<uint8_t> *pSpan )
{
	const size_t nRequired = SIRStringPool::requiredBytes( config, bytes.data(), bytes.size() );
	if ( nRequired == 0 ) {
		return false;
	}
	pSpan->resize( nRequired );
	return pPool->initialize( { pSpan->data(), pSpan->size(), bytes.data(), bytes.size() }, config );
}

TEST( SIRStringPool, AtomIdentityAndLifetime )
{
	static_assert( sizeof( SIRStringAtom ) == 4, "atom must remain 32-bit" );
	SIRStringAtom empty;
	EXPECT_FALSE( empty.isValid() );
	EXPECT_FALSE( ( bool )empty );
	EXPECT_EQ( empty.getId(), 0u );
	SIRStringAtom a( 17 );
	SIRStringAtom b( 17 );
	SIRStringAtom c( 19 );
	EXPECT_TRUE( a.isValid() );
	EXPECT_TRUE( ( bool )a );
	EXPECT_EQ( a.hash(), 17u );
	EXPECT_EQ( a( a ), 17u );
	EXPECT_TRUE( a == b );
	EXPECT_TRUE( a != c );
	EXPECT_FALSE( a != b );
	EXPECT_FALSE( a == c );
	a.reset();
	EXPECT_FALSE( a.isValid() );
}

TEST( SIRStringPool, DomainFailuresAndSpanValidation )
{
	sirPoolTestAllocator_t state = {};
	sirContainerAllocator_t domain = sirPoolTestDomain( &state );
	sirStringPoolConfig_t config = sirPoolTestConfig();
	SIRStringPool pool;
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_FALSE( pool.isInitialized() );
	EXPECT_EQ( pool.getStringCount(), 0u );
	EXPECT_EQ( pool.getSlotCount(), 0u );
	EXPECT_EQ( pool.getBlockCount(), 0u );
	EXPECT_EQ( pool.getReservedBytes(), 0u );
	EXPECT_FALSE( pool.initialize( ( const sirContainerAllocator_t * )nullptr, config ) );
	sirContainerAllocator_t missing = domain;
	missing.pfnAllocate = nullptr;
	EXPECT_FALSE( pool.initialize( &missing, config ) );
	missing = domain;
	missing.pfnDeallocate = nullptr;
	EXPECT_FALSE( pool.initialize( &missing, config ) );
	config.nMaxNames = 0;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	EXPECT_FALSE( pool.initialize( &domain, config ) );
	config = sirPoolTestConfig();
	state.nFailCall = state.nCalls + 1;
	EXPECT_FALSE( pool.initialize( &domain, config ) );
	EXPECT_EQ( state.nLive, 0u );
	state.nFailCall = 0;
	const size_t nRequired = SIRStringPool::requiredBytes( config, 0 );
	std::vector<uint8_t> small( nRequired - 1, 0 );
	EXPECT_FALSE( pool.initialize( { small.data(), small.size(), nullptr, 0 }, config ) );
	EXPECT_FALSE( pool.initialize( { nullptr, nRequired, nullptr, 0 }, config ) );
	EXPECT_FALSE( pool.initialize( { small.data(), nRequired, nullptr, 2 }, config ) );
	EXPECT_TRUE( pool.initialize( &domain, config ) );
	EXPECT_TRUE( pool.isInitialized() );
	EXPECT_FALSE( pool.initialize( &domain, config ) );
	EXPECT_FALSE( pool.initialize( { small.data(), nRequired, nullptr, 0 }, config ) );
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_EQ( state.nLive, 0u );
	EXPECT_EQ( state.nCalls, state.nDeallocations + 1 );
}

TEST( SIRStringPool, ConfigOverflowAndInvalidShape )
{
	sirStringPoolConfig_t config = sirPoolTestConfig();
	EXPECT_TRUE( SIRStringPool::requiredBytes( config, 0 ) != 0 );
	config.nHashBuckets = 0;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config = sirPoolTestConfig();
	config.nHashBuckets = 3;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config = sirPoolTestConfig();
	config.nMaxNames = 0x1000000u;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config = sirPoolTestConfig();
	config.nLongBytes = 8;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config = sirPoolTestConfig();
	config.nLongBytes = 1000;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config = sirPoolTestConfig();
	config.nShortSlots = UINT32_MAX;
	config.nLongSlots = 1;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config = sirPoolTestConfig();
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 32 ), 0u );
	config = sirPoolTestConfig();
	EXPECT_EQ( SIRStringPool::requiredBytes( config, ( const void * )nullptr, 50 ), 0u );
	const uint8_t junk[] = { 'N', 'M', 'D', 'S', 2, 0, 0, 0 };
	EXPECT_EQ( SIRStringPool::requiredBytes( config, junk, sizeof( junk ) ), 0u );
}

TEST( SIRStringPool, InternFindAndDirectLeases )
{
	sirPoolTestAllocator_t state = {};
	sirContainerAllocator_t domain = sirPoolTestDomain( &state );
	sirStringPoolConfig_t config = sirPoolTestConfig();
	SIRStringPool pool;
	EXPECT_FALSE( pool.intern( "a" ).isValid() );
	EXPECT_FALSE( pool.find( "a" ).isValid() );
	EXPECT_FALSE( pool.intern( nullptr ).isValid() );
	EXPECT_FALSE( pool.find( nullptr ).isValid() );
	EXPECT_TRUE( pool.initialize( &domain, config ) );
	const SIRStringAtom first = pool.intern( "first" );
	EXPECT_TRUE( first.isValid() );
	EXPECT_EQ( first, pool.intern( "first" ) );
	EXPECT_EQ( first, pool.find( "first" ) );
	EXPECT_EQ( first, pool.findLiteral( "first" ) );
	EXPECT_EQ( first, pool.internLiteral( "first" ) );
	EXPECT_FALSE( pool.find( "absent" ).isValid() );
	const SIRStringAtom empty = pool.intern( "" );
	EXPECT_TRUE( empty.isValid() );
	EXPECT_EQ( empty, pool.find( "" ) );
	EXPECT_EQ( pool.intern( "k", ( size_t )UINT32_MAX + 1u ).getId(), 0u );
	EXPECT_EQ( pool.find( "k", ( size_t )UINT32_MAX + 1u ).getId(), 0u );
	EXPECT_FALSE( pool.intern( nullptr, 0 ).isValid() );
	EXPECT_FALSE( pool.find( nullptr, 0 ).isValid() );
	SIRStringLease lease;
	EXPECT_EQ( pool.acquire( first, lease ), SIR_STRING_OK );
	EXPECT_TRUE( lease.isValid() );
	EXPECT_EQ( lease.bytes(), 5u );
	EXPECT_STREQ( lease.data(), "first" );
	EXPECT_EQ( pool.acquire( first, lease ), SIR_STRING_OK );
	EXPECT_EQ( pool.getStats().nPinnedLeases, 1u );
	EXPECT_FALSE( pool.shutdown() );
	EXPECT_FALSE( pool.retireScope() );
	uint32_t nBytes = 0;
	EXPECT_STREQ( pool.peekRaw( first, nBytes ), "first" );
	EXPECT_EQ( nBytes, 5u );
	char text[20] = {};
	uint32_t nWritten = 77;
	EXPECT_EQ( pool.decodeInto( first, text, sizeof( text ), nWritten ), SIR_STRING_OK );
	EXPECT_EQ( nWritten, 5u );
	EXPECT_STREQ( text, "first" );
	lease.release();
	EXPECT_FALSE( lease.isValid() );
	EXPECT_EQ( lease.data(), nullptr );
	EXPECT_EQ( lease.bytes(), 0u );
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_EQ( state.nLive, 0u );
}

TEST( SIRStringPool, InvalidUtf8AndCapacity )
{
	sirPoolTestAllocator_t state = {};
	sirContainerAllocator_t domain = sirPoolTestDomain( &state );
	sirStringPoolConfig_t config = sirPoolTestConfig();
	config.nMaxNames = 3;
	config.nDynamicTextBytes = 12;
	SIRStringPool pool;
	EXPECT_TRUE( pool.initialize( &domain, config ) );
	const char bad[] = { ( char )0xC0, ( char )0x80 };
	EXPECT_FALSE( pool.intern( bad, sizeof( bad ) ).isValid() );
	EXPECT_FALSE( pool.intern( "\xf0\x80\x80\x80", 4 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xf4\x90\x80\x80", 4 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xed\xa0\x80", 3 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xe2\x82", 2 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xe2\x28\xa1", 3 ).isValid() );
	EXPECT_TRUE( pool.intern( "A" ).isValid() );
	EXPECT_TRUE( pool.intern( "B" ).isValid() );
	EXPECT_TRUE( pool.intern( "C" ).isValid() );
	EXPECT_FALSE( pool.intern( "D" ).isValid() );
	EXPECT_EQ( pool.getStringCount(), 3u );
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_EQ( state.nLive, 0u );
}

TEST( SIRStringPool, CookedRawAndCompressed )
{
	std::vector<sirTestCookedName_t> names = { { "alpha", false }, { "beta", true }, { "delta", false }, { "gamma", true } };
	std::vector<uint8_t> catalog = sirTestCatalog( names );
	sirStringPoolConfig_t config = sirPoolTestConfig();
	SIRStringPool pool;
	std::vector<uint8_t> span;
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	EXPECT_EQ( pool.getStringCount(), 4u );
	EXPECT_EQ( pool.getStats().nDecodedSlotCount, 3u );
	SIRStringAtom alpha = pool.find( "alpha" );
	SIRStringAtom beta = pool.find( "beta" );
	SIRStringAtom gamma = pool.find( "gamma" );
	EXPECT_TRUE( alpha.isValid() );
	EXPECT_TRUE( beta.isValid() );
	EXPECT_TRUE( gamma.isValid() );
	EXPECT_EQ( alpha, pool.intern( "alpha" ) );
	EXPECT_EQ( beta, pool.intern( "beta" ) );
	EXPECT_EQ( pool.getStringCount(), 4u );
	EXPECT_EQ( pool.getDecodedByteLength( beta ), 4u );
	EXPECT_EQ( pool.getContentHash( beta ), SIRStringPool::hashString( "beta", 4 ) );
	uint32_t nBytes = 0;
	EXPECT_STREQ( pool.peekRaw( alpha, nBytes ), "alpha" );
	EXPECT_EQ( nBytes, 5u );
	EXPECT_EQ( pool.peekRaw( beta, nBytes ), nullptr );
	SIRStringLease lease;
	EXPECT_EQ( pool.acquire( beta, lease ), SIR_STRING_OK );
	EXPECT_EQ( lease.bytes(), 4u );
	EXPECT_STREQ( lease.data(), "beta" );
	EXPECT_EQ( pool.acquire( beta, lease ), SIR_STRING_OK );
	EXPECT_TRUE( pool.getStats().nCachedStrings >= 1u );
	char output[40] = {};
	uint32_t nWritten = 0;
	EXPECT_EQ( pool.decodeInto( gamma, output, sizeof( output ), nWritten ), SIR_STRING_OK );
	EXPECT_EQ( nWritten, 5u );
	EXPECT_STREQ( output, "gamma" );
	EXPECT_EQ( pool.getStats().nPinnedLeases, 1u );
	EXPECT_FALSE( pool.retireScope() );
	lease.release();
	EXPECT_TRUE( pool.retireScope() );
	EXPECT_FALSE( pool.find( "runtime" ).isValid() );
	EXPECT_TRUE( pool.find( "beta" ).isValid() );
	EXPECT_FALSE( pool.acquire( beta, lease ) == SIR_STRING_OK );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, CorruptCatalogIsRejected )
{
	sirStringPoolConfig_t config = sirPoolTestConfig();
	std::vector<uint8_t> good = sirTestCatalog( { { "alpha", false }, { "beta", true } } );
	auto reject = [&]( const std::vector<uint8_t>& bytes ) {
		SIRStringPool pool;
		std::vector<uint8_t> span;
		EXPECT_FALSE( sirTestInitCatalog( &pool, bytes, config, &span ) );
	};
	auto bad = good;
	bad[0] = 'N';
	reject( bad );
	bad = good;
	bad[4] = 2;
	reject( bad );
	bad = good;
	bad[6] = 1;
	reject( bad );
	bad = good;
	sirTestStore32( bad, 44, 47 );
	reject( bad );
	bad = good;
	sirTestStore32( bad, 8, 3 );
	reject( bad );
	bad = good;
	sirTestStore32( bad, 12, 8 );
	reject( bad );
	bad = good;
	sirTestStore32( bad, 16, 8 );
	reject( bad );
	bad = good;
	sirTestStore32( bad, 20, 1 );
	reject( bad );
	bad = good;
	sirTestStore32( bad, 24, 20 );
	reject( bad );
	bad = good;
	bad[48] ^= 0xff;
	reject( bad );
	bad = good;
	bad[36] ^= 0xff;
	reject( bad );
	bad = good;
	sirTestStore32( bad, 48, UINT32_MAX );
	sirTestRehashCatalog( bad );
	reject( bad );
	bad = good;
	sirTestStore16( bad, 52, 1025 );
	sirTestRehashCatalog( bad );
	reject( bad );
	bad = good;
	sirTestStore16( bad, 54, 0x4005 );
	sirTestRehashCatalog( bad );
	reject( bad );
	bad = good;
	sirTestStore16( bad, 54, 0xc005 );
	sirTestRehashCatalog( bad );
	reject( bad );
	bad = good;
	sirTestStore16( bad, 54, 0x8006 );
	sirTestRehashCatalog( bad );
	reject( bad );
	bad = good;
	bad[good.size() - 1] = 9;
	sirTestRehashCatalog( bad );
	reject( bad );
	bad = good;
	sirTestStore16( bad, 54, 0x8005 );
	bad[48 + 16 + 5] = 1;
	sirTestRehashCatalog( bad );
	reject( bad );
	bad = sirTestCatalog( { { "z", false }, { "a", false } } );
	reject( bad );
	bad = sirTestCatalog( { { "a", false }, { "a", false } } );
	reject( bad );
	bad = sirTestCatalog( { { "a", false }, { "\xed\xa0\x80", false } } );
	reject( bad );
	bad.resize( 48 );
	reject( bad );
}

TEST( SIRStringPool, CachePinAndEviction )
{
	std::vector<uint8_t> catalog = sirTestCatalog( { { "aaa", true }, { "bbb", true }, { "ccc", true }, { "ddd", true } } );
	sirStringPoolConfig_t config = sirPoolTestConfig();
	config.nShortSlots = 1;
	config.nLongSlots = 0;
	SIRStringPool pool;
	std::vector<uint8_t> span;
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	SIRStringLease first;
	SIRStringLease second;
	SIRStringAtom a = pool.find( "aaa" );
	SIRStringAtom b = pool.find( "bbb" );
	EXPECT_EQ( pool.acquire( a, first ), SIR_STRING_OK );
	EXPECT_EQ( pool.acquire( a, second ), SIR_STRING_OK );
	EXPECT_EQ( pool.acquire( b, second ), SIR_STRING_ALL_SLOTS_PINNED );
	EXPECT_EQ( second.isValid(), false );
	first.release();
	EXPECT_EQ( pool.acquire( b, second ), SIR_STRING_OK );
	EXPECT_STREQ( second.data(), "bbb" );
	EXPECT_EQ( pool.getStats().nCachedStrings, 1u );
	second.release();
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, TextRefDeferredResolution )
{
	const auto catalog = sirTestCatalog( { { "alpha", true } } );
	sirStringPoolConfig_t config = sirPoolTestConfig();
	SIRStringPool pool;
	std::vector<uint8_t> span;
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	SIRStringTextRef ref;
	const char *pText = nullptr;
	uint32_t nBytes = 0;
	EXPECT_EQ( ref.get( pText, nBytes ), SIR_STRING_NOT_INITIALIZED );
	ref.set( &pool, pool.find( "alpha" ) );
	EXPECT_FALSE( ref.isPinned() );
	EXPECT_STREQ( ref.data(), "alpha" );
	EXPECT_TRUE( ref.isPinned() );
	EXPECT_EQ( ref.bytes(), 5u );
	EXPECT_EQ( ref.get( pText, nBytes ), SIR_STRING_OK );
	EXPECT_STREQ( pText, "alpha" );
	EXPECT_FALSE( pool.shutdown() );
	ref.release();
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, MultiplePoolsAndScopeRetire )
{
	sirPoolTestAllocator_t state = {};
	sirContainerAllocator_t domain = sirPoolTestDomain( &state );
	auto config = sirPoolTestConfig();
	SIRStringPool one;
	SIRStringPool two;
	EXPECT_TRUE( one.initialize( &domain, config ) );
	EXPECT_TRUE( two.initialize( &domain, config ) );
	auto before = one.intern( "shared" );
	auto other = two.intern( "shared" );
	EXPECT_TRUE( before != other );
	EXPECT_TRUE( one.retireScope() );
	EXPECT_FALSE( one.find( "shared" ).isValid() );
	EXPECT_TRUE( one.intern( "shared" ) != before );
	SIRStringLease lease;
	EXPECT_EQ( one.acquire( before, lease ), SIR_STRING_INVALID_ATOM );
	EXPECT_TRUE( one.shutdown() );
	EXPECT_TRUE( two.shutdown() );
	EXPECT_EQ( state.nLive, 0u );
}

TEST( SIRStringPool, ConcurrentInternAndAcquire )
{
	sirPoolTestAllocator_t state = {};
	sirContainerAllocator_t domain = sirPoolTestDomain( &state );
	auto config = sirPoolTestConfig();
	config.nMaxNames = 512;
	config.nDynamicTextBytes = 16384;
	SIRStringPool pool;
	EXPECT_TRUE( pool.initialize( &domain, config ) );
	std::atomic<uint32_t> nFailures( 0 );
	std::vector<std::thread> threads;
	for ( uint32_t iThread = 0; iThread < 4; ++iThread ) {
		threads.emplace_back( [&pool, &nFailures]() {
			for ( uint32_t iName = 0; iName < 100; ++iName ) {
				const std::string sText = std::string( "concurrent." ) + std::to_string( iName );
				SIRStringAtom atom = pool.intern( sText.c_str() );
				if ( !atom.isValid() || atom != pool.find( sText.c_str() ) ) {
					++nFailures;
				}
				SIRStringLease lease;
				if ( pool.acquire( atom, lease ) != SIR_STRING_OK || strcmp( lease.data(), sText.c_str() ) != 0 ) {
					++nFailures;
				}
			}
		} );
	}
	for ( std::thread& thread : threads ) {
		thread.join();
	}
	EXPECT_EQ( nFailures.load(), 0u );
	EXPECT_EQ( pool.getStringCount(), 100u );
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_EQ( state.nLive, 0u );
}

static std::vector<uint8_t> sirTestCustomEncoded( const std::string& sDecoded, const std::vector<uint8_t>& encoded )
{
	std::vector<uint8_t> bytes( 48, 0 );
	memcpy( bytes.data(), "SIRS", 4 );
	sirTestStore16( bytes, 4, SIR_STRING_CATALOG_VERSION );
	sirTestStore32( bytes, 8, 1 );
	sirTestStore32( bytes, 12, 8 );
	sirTestStore32( bytes, 24, ( uint32_t )encoded.size() );
	sirTestStore32( bytes, 44, 48 );
	sirTestAppend32( bytes, 0 );
	sirTestAppend16( bytes, ( uint16_t )sDecoded.size() );
	sirTestAppend16( bytes, ( uint16_t )encoded.size() );
	bytes.insert( bytes.end(), encoded.begin(), encoded.end() );
	uint64_t nCorpus = 14695981039346656037ULL;
	for ( uint8_t nChar : sDecoded ) {
		nCorpus = ( nCorpus ^ nChar ) * 1099511628211ULL;
	}
	nCorpus = ( nCorpus ^ 0xffu ) * 1099511628211ULL;
	sirTestStore64( bytes, 36, nCorpus );
	sirTestRehashCatalog( bytes );
	return bytes;
}

static uint64_t sirTestCatalogCorpusHash( const std::vector<uint8_t>& catalog )
{
	uint64_t nHash = 0;
	for ( unsigned int i = 0; i < 8; ++i ) {
		nHash |= ( uint64_t )catalog[36 + i] << ( i * 8 );
	}
	return nHash;
}

TEST( SIRStringPool, CookedOrdinalRequiresCorrectCatalogIdentity )
{
	auto catalog = sirTestCatalog( { { "alpha", false }, { "beta", true }, { "gamma", true } } );
	auto config = sirPoolTestConfig();
	std::vector<uint8_t> span;
	SIRStringPool pool;
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	const uint64_t nHash = sirTestCatalogCorpusHash( catalog );
	EXPECT_FALSE( pool.atomFromCookedOrdinal( 0, nHash + 1 ).isValid() );
	EXPECT_FALSE( pool.atomFromCookedOrdinal( 3, nHash ).isValid() );
	EXPECT_EQ( pool.atomFromCookedOrdinal( 1, nHash ), pool.find( "beta" ) );
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_FALSE( pool.atomFromCookedOrdinal( 0, nHash ).isValid() );
}

TEST( SIRStringPool, MultibyteUnicodeAcceptanceAndRejection )
{
	sirPoolTestAllocator_t state = {};
	sirContainerAllocator_t domain = sirPoolTestDomain( &state );
	auto config = sirPoolTestConfig();
	config.nMaxNames = 32;
	SIRStringPool pool;
	EXPECT_TRUE( pool.initialize( &domain, config ) );
	EXPECT_TRUE( pool.intern( "\xc3\xa9", 2 ).isValid() );
	EXPECT_TRUE( pool.intern( "\xe2\x82\xac", 3 ).isValid() );
	EXPECT_TRUE( pool.intern( "\xf0\x9f\x92\xa9", 4 ).isValid() );
	EXPECT_FALSE( pool.intern( "\x80", 1 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xff", 1 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xc2", 1 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xe2\x82", 2 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xe0\x80\x80", 3 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xf0\x8f\x80\x80", 4 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xed\xbf\xbf", 3 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xf4\x90\x80\x80", 4 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xf5\x80\x80\x80", 4 ).isValid() );
	EXPECT_FALSE( pool.intern( "\xc3"
	                           "A",
	                           2 )
	                  .isValid() );
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_EQ( state.nLive, 0u );
}

TEST( SIRStringPool, LongCacheSlotsAndNoCacheFallback )
{
	const auto catalog = sirTestCatalog( { { "abcdefghijklmnopq", true }, { "longer-but-not-too-long", true } } );
	auto config = sirPoolTestConfig();
	config.nShortSlots = 1;
	config.nLongSlots = 1;
	SIRStringPool pool;
	std::vector<uint8_t> span;
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	SIRStringLease lease;
	EXPECT_EQ( pool.acquire( pool.find( "abcdefghijklmnopq" ), lease ), SIR_STRING_OK );
	EXPECT_STREQ( lease.data(), "abcdefghijklmnopq" );
	EXPECT_EQ( pool.getStats().nPinnedLeases, 1u );
	EXPECT_FALSE( pool.shutdown() );
	lease.release();
	EXPECT_TRUE( pool.shutdown() );
	config.nShortSlots = 0;
	config.nLongSlots = 0;
	span.clear();
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	SIRStringAtom atom = pool.atomFromCookedOrdinal( 0, sirTestCatalogCorpusHash( catalog ) );
	EXPECT_EQ( pool.acquire( atom, lease ), SIR_STRING_OUTPUT_TOO_SMALL );
	char text[64] = {};
	uint32_t nWritten = 0;
	EXPECT_EQ( pool.decodeInto( atom, text, sizeof( text ), nWritten ), SIR_STRING_OK );
	EXPECT_STREQ( text, "abcdefghijklmnopq" );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, BackReferenceCatalogCodec )
{
	auto good = sirTestCustomEncoded( "abcabcabc", { 2, 'a', 'b', 'c', 0x83, 3 } );
	auto config = sirPoolTestConfig();
	std::vector<uint8_t> span;
	SIRStringPool pool;
	EXPECT_TRUE( sirTestInitCatalog( &pool, good, config, &span ) );
	SIRStringLease lease;
	EXPECT_EQ( pool.acquire( pool.find( "abcabcabc" ), lease ), SIR_STRING_OK );
	EXPECT_STREQ( lease.data(), "abcabcabc" );
	lease.release();
	EXPECT_TRUE( pool.shutdown() );
	auto reject = [&]( const std::vector<uint8_t>& bad ) {
		SIRStringPool other;
		std::vector<uint8_t> bytes;
		EXPECT_FALSE( sirTestInitCatalog( &other, bad, config, &bytes ) );
	};
	reject( sirTestCustomEncoded( "abcabcabc", { 2, 'a', 'b', 'c', 0x83, 0 } ) );
	reject( sirTestCustomEncoded( "abcabcabc", { 2, 'a', 'b', 'c', 0x83, 9 } ) );
	reject( sirTestCustomEncoded( "abcabcabc", { 2, 'a', 'b', 'c', 0x83 } ) );
	reject( sirTestCustomEncoded( "abcd", { 2, 'a', 'b', 'c', 0x83, 3 } ) );
	reject( sirTestCustomEncoded( "abcdefghij", { 2, 'a', 'b', 'c', 0x83, 3 } ) );
	reject( sirTestCustomEncoded( "xyz", { 8, 'x' } ) );
	reject( sirTestCustomEncoded( "abcabcabc", { 0x81, 1 } ) );
}

TEST( SIRStringPool, DecodeIntoInvalidAndStale )
{
	auto catalog = sirTestCatalog( { { "alpha", false }, { "beta", true } } );
	auto config = sirPoolTestConfig();
	SIRStringPool pool;
	SIRStringLease lease;
	char szText[40] = {};
	uint32_t nWritten = UINT32_MAX;
	EXPECT_EQ( pool.decodeInto( SIRStringAtom( 1 ), szText, sizeof( szText ), nWritten ), SIR_STRING_NOT_INITIALIZED );
	EXPECT_EQ( nWritten, 0u );
	EXPECT_EQ( pool.acquire( SIRStringAtom( 1 ), lease ), SIR_STRING_NOT_INITIALIZED );
	std::vector<uint8_t> span;
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	SIRStringAtom atom = pool.find( "beta" );
	EXPECT_EQ( pool.decodeInto( atom, nullptr, sizeof( szText ), nWritten ), SIR_STRING_OUTPUT_TOO_SMALL );
	EXPECT_EQ( pool.decodeInto( atom, szText, 4, nWritten ), SIR_STRING_OUTPUT_TOO_SMALL );
	EXPECT_EQ( pool.decodeInto( SIRStringAtom( UINT32_MAX ), szText, sizeof( szText ), nWritten ), SIR_STRING_INVALID_ATOM );
	EXPECT_EQ( pool.getDecodedByteLength( SIRStringAtom( UINT32_MAX ) ), 0u );
	EXPECT_EQ( pool.getContentHash( SIRStringAtom( UINT32_MAX ) ), 0u );
	EXPECT_EQ( pool.peekRaw( SIRStringAtom( UINT32_MAX ), nWritten ), nullptr );
	EXPECT_EQ( pool.acquire( SIRStringAtom( UINT32_MAX ), lease ), SIR_STRING_INVALID_ATOM );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, ColdRegistryHashCollisionResolution )
{
	sirPoolTestAllocator_t state = {};
	auto domain = sirPoolTestDomain( &state );
	auto config = sirPoolTestConfig();
	config.nMaxNames = 256;
	config.nDynamicTextBytes = 8192;
	SIRStringPool pool;
	EXPECT_TRUE( pool.initialize( &domain, config ) );
	for ( uint32_t iString = 0; iString < 256; ++iString ) {
		const std::string sName = "entity." + std::to_string( iString );
		EXPECT_TRUE( pool.intern( sName.c_str() ).isValid() );
	}
	for ( uint32_t iString = 0; iString < 256; ++iString ) {
		const std::string sName = "entity." + std::to_string( iString );
		EXPECT_TRUE( pool.find( sName.c_str() ).isValid() );
	}
	EXPECT_EQ( pool.getStringCount(), 256u );
	EXPECT_FALSE( pool.intern( "entity.257" ).isValid() );
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_EQ( state.nLive, 0u );
}

TEST( SIRStringPool, TextRefIdempotenceAndRelease )
{
	sirPoolTestAllocator_t state = {};
	auto domain = sirPoolTestDomain( &state );
	auto config = sirPoolTestConfig();
	SIRStringPool pool;
	EXPECT_TRUE( pool.initialize( &domain, config ) );
	auto atom = pool.intern( "hello" );
	SIRStringTextRef ref;
	ref.set( &pool, atom );
	EXPECT_EQ( ref.bytes(), 5u );
	EXPECT_TRUE( ref.isPinned() );
	ref.set( &pool, atom );
	EXPECT_STREQ( ref.data(), "hello" );
	ref.set( nullptr, SIRStringAtom() );
	EXPECT_FALSE( ref.isPinned() );
	EXPECT_EQ( ref.bytes(), 0u );
	EXPECT_EQ( ref.data(), nullptr );
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_EQ( state.nLive, 0u );
}

TEST( SIRStringPool, CookedRawLeaseUsesDirectStorageAndBlocksRetirement )
{
	const auto catalog = sirTestCatalog( { { "alpha", false }, { "beta", true } } );
	const auto config = sirPoolTestConfig();
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	const auto atom = pool.find( "alpha" );
	ASSERT_TRUE( atom.isValid() );
	SIRStringLease lease;
	ASSERT_EQ( pool.acquire( atom, lease ), SIR_STRING_OK );
	EXPECT_STREQ( lease.data(), "alpha" );
	EXPECT_EQ( lease.bytes(), 5u );
	EXPECT_EQ( pool.getStats().nPinnedLeases, 1u );
	EXPECT_FALSE( pool.retireScope() );
	EXPECT_FALSE( pool.shutdown() );
	lease.release();
	EXPECT_TRUE( pool.retireScope() );
	EXPECT_FALSE( pool.find( "missing" ).isValid() );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, CorruptionAfterValidationFailsDecodeAndDoesNotPublishCache )
{
	auto catalog = sirTestCatalog( { { "delta", true } } );
	const auto config = sirPoolTestConfig();
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	uint64_t nCatalogHash = 0;
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nCatalogHash |= ( uint64_t )catalog[36 + iByte] << ( iByte * 8 );
	}
	const auto atom = pool.atomFromCookedOrdinal( 0, nCatalogHash );
	ASSERT_TRUE( atom.isValid() );
	const size_t nHeader = 48;
	const size_t nRecordBytes = 8;
	size_t iCatalog = span.size();
	for ( size_t i = 0; i + catalog.size() <= span.size(); ++i ) {
		if ( memcmp( span.data() + i, catalog.data(), catalog.size() ) == 0 ) {
			iCatalog = i;
			break;
		}
	}
	ASSERT_TRUE( iCatalog < span.size() );
	const size_t iToken = iCatalog + nHeader + nRecordBytes;
	const uint8_t nPrevious = span[iToken];
	span[iToken] = 127;
	SIRStringLease lease;
	EXPECT_EQ( pool.acquire( atom, lease ), SIR_STRING_CORRUPT_CATALOG );
	EXPECT_FALSE( lease.isValid() );
	EXPECT_EQ( pool.getStats().nCachedStrings, 0u );
	char szOutput[32] = {};
	uint32_t nWritten = 123;
	EXPECT_EQ( pool.decodeInto( atom, szOutput, sizeof( szOutput ), nWritten ), SIR_STRING_CORRUPT_CATALOG );
	EXPECT_EQ( nWritten, 0u );
	span[iToken] = nPrevious;
	EXPECT_EQ( pool.acquire( atom, lease ), SIR_STRING_OK );
	EXPECT_STREQ( lease.data(), "delta" );
	lease.release();
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, CatalogSizeOverflowRejectsBeforeReadingPayload )
{
	const auto config = sirPoolTestConfig();
	const auto catalog = sirTestCatalog( { { "one", false } } );
	EXPECT_EQ( SIRStringPool::requiredBytes( config, catalog.data(), SIZE_MAX ), 0u );
	auto bounded = config;
	bounded.nMaxNames = 0xffffff;
	bounded.nDynamicTextBytes = UINT32_MAX;
	bounded.nShortSlots = 0;
	bounded.nLongSlots = 0;
	bounded.nShortBytes = 1024;
	bounded.nLongBytes = 1024;
	EXPECT_EQ( SIRStringPool::requiredBytes( bounded, SIZE_MAX ), 0u );
}

TEST( SIRStringPool, DeterministicMalformedCatalogMutationMatrix )
{
	const auto source = sirTestCatalog( { { "alpha", false }, { "beta", true }, { "gamma", true } } );
	const auto config = sirPoolTestConfig();
	uint32_t nSeed = 0x51ab82c9u;
	uint32_t nRejected = 0;
	for ( uint32_t iMutation = 0; iMutation < 1400; ++iMutation ) {
		auto mutated = source;
		nSeed ^= nSeed << 13;
		nSeed ^= nSeed >> 17;
		nSeed ^= nSeed << 5;
		const size_t iByte = ( size_t )nSeed % mutated.size();
		mutated[iByte] ^= ( uint8_t )( 1u << ( ( nSeed >> 8 ) & 7u ) );
		if ( ( iMutation & 3u ) == 0u ) {
			sirTestRehashCatalog( mutated );
		}
		SIRStringPool pool;
		std::vector<uint8_t> span;
		if ( sirTestInitCatalog( &pool, mutated, config, &span ) ) {
			EXPECT_EQ( pool.getStringCount(), 3u );
			EXPECT_TRUE( pool.shutdown() );
		} else {
			++nRejected;
		}
	}
	EXPECT_TRUE( nRejected > 900u );
}

TEST( SIRStringPool, AdaptiveDefaultShapeAndEightThousandNames )
{
	const sirStringPoolConfig_t config = SIRStringPool::defaultConfig();
	EXPECT_EQ( config.nMaxNames, 8192u );
	EXPECT_EQ( config.nHashBuckets, 16384u );
	EXPECT_TRUE( config.bTieredCache );
	EXPECT_EQ( config.nTierSlots[0], 512u );
	EXPECT_EQ( config.nTierSlots[1], 256u );
	EXPECT_EQ( config.nTierSlots[2], 128u );
	EXPECT_EQ( config.nTierSlots[3], 64u );
	EXPECT_EQ( config.nTierSlots[4], 24u );
	EXPECT_EQ( config.nTierSlots[5], 12u );
	EXPECT_EQ( config.nTierBytes[0], 16u );
	EXPECT_EQ( config.nTierBytes[5], 1024u );
	EXPECT_TRUE( config.bAdaptiveBudgets );

	SIRStringPool pool;
	std::vector<uint8_t> span( SIRStringPool::requiredBytes( config, 0 ) );
	EXPECT_TRUE( pool.initialize( { span.data(), span.size(), nullptr, 0 }, config ) );
	EXPECT_EQ( pool.getStats().nActiveNameBudget, 512u );
	EXPECT_EQ( pool.getStats().nActiveDynamicTextBudget, 65536u );
	EXPECT_EQ( pool.getStats().nActiveTierSlots[0], 16u );
	EXPECT_EQ( pool.getStats().nActiveTierSlots[1], 8u );
	EXPECT_EQ( pool.getStats().nActiveTierSlots[2], 4u );
	EXPECT_EQ( pool.getStats().nActiveTierSlots[3], 2u );
	EXPECT_EQ( pool.getStats().nActiveTierSlots[4], 1u );
	EXPECT_EQ( pool.getStats().nActiveTierSlots[5], 1u );

	char szName[32] = {};
	for ( uint32_t iName = 0; iName < 8192; ++iName ) {
		snprintf( szName, sizeof( szName ), "name-%05u", iName );
		EXPECT_TRUE( pool.intern( szName ).isValid() );
	}
	EXPECT_EQ( pool.getStringCount(), 8192u );
	EXPECT_EQ( pool.getStats().nActiveNameBudget, 8192u );
	EXPECT_FALSE( pool.intern( "name-extra" ).isValid() );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, AdaptiveAdmissionBudgetsGrowAndReset )
{
	sirStringPoolConfig_t config = sirPoolTestConfig();
	config.nMaxNames = 8;
	config.nDynamicTextBytes = 64;
	config.nInitialNameBudget = 2;
	config.nInitialDynamicTextBytes = 8;
	config.nInitialShortSlots = 1;
	config.nInitialLongSlots = 1;
	SIRStringPool pool;
	std::vector<uint8_t> span( SIRStringPool::requiredBytes( config, 0 ) );
	EXPECT_TRUE( pool.initialize( { span.data(), span.size(), nullptr, 0 }, config ) );
	EXPECT_EQ( pool.getStats().nActiveNameBudget, 2u );
	EXPECT_EQ( pool.getStats().nActiveDynamicTextBudget, 8u );
	EXPECT_TRUE( pool.intern( "alpha" ).isValid() );
	EXPECT_EQ( pool.getStats().nActiveDynamicTextBudget, 8u );
	EXPECT_TRUE( pool.intern( "beta" ).isValid() );
	EXPECT_EQ( pool.getStats().nActiveDynamicTextBudget, 16u );
	EXPECT_TRUE( pool.intern( "gamma" ).isValid() );
	EXPECT_EQ( pool.getStats().nActiveDynamicTextBudget, 32u );
	EXPECT_EQ( pool.getStats().nActiveNameBudget, 4u );
	EXPECT_TRUE( pool.retireScope() );
	EXPECT_EQ( pool.getStats().nActiveNameBudget, 2u );
	EXPECT_EQ( pool.getStats().nActiveDynamicTextBudget, 8u );
	EXPECT_EQ( pool.getStringCount(), 0u );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, AdaptiveCacheGrowsWithoutMovingPinnedText )
{
	const auto catalog = sirTestCatalog( { { "aa", true }, { "bb", true }, { "cc", true }, { "dd", true }, { "ee", true } } );
	sirStringPoolConfig_t config = sirPoolTestConfig();
	config.nShortSlots = 4;
	config.nLongSlots = 0;
	config.nInitialShortSlots = 1;
	SIRStringPool pool;
	std::vector<uint8_t> span;
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	EXPECT_EQ( pool.getStats().nActiveShortSlots, 1u );
	SIRStringLease leases[5];
	const char *pszExpected[] = { "aa", "bb", "cc", "dd" };
	for ( uint32_t iName = 0; iName < 4; ++iName ) {
		EXPECT_EQ( pool.acquire( pool.find( pszExpected[iName] ), leases[iName] ), SIR_STRING_OK );
		EXPECT_STREQ( leases[iName].data(), pszExpected[iName] );
	}
	EXPECT_EQ( pool.getStats().nActiveShortSlots, 4u );
	EXPECT_EQ( pool.getStats().nPinnedLeases, 4u );
	EXPECT_EQ( pool.acquire( pool.find( "ee" ), leases[4] ), SIR_STRING_ALL_SLOTS_PINNED );
	for ( uint32_t iName = 0; iName < 4; ++iName ) {
		EXPECT_STREQ( leases[iName].data(), pszExpected[iName] );
		leases[iName].release();
	}
	EXPECT_TRUE( pool.retireScope() );
	EXPECT_EQ( pool.getStats().nActiveShortSlots, 1u );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, AdaptiveLongCacheAndCookedIdentityFloor )
{
	const auto catalog = sirTestCatalog( { { "long-alpha", true }, { "long-bravo", true }, { "long-charlie", true } } );
	sirStringPoolConfig_t config = sirPoolTestConfig();
	config.nShortSlots = 0;
	config.nLongSlots = 3;
	config.nShortBytes = 4;
	config.nInitialNameBudget = 1;
	config.nInitialLongSlots = 1;
	SIRStringPool pool;
	std::vector<uint8_t> span;
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	EXPECT_EQ( pool.getStats().nActiveNameBudget, 3u );
	EXPECT_EQ( pool.getStats().nActiveLongSlots, 1u );
	const char *pszExpected[] = { "long-alpha", "long-bravo", "long-charlie" };
	SIRStringLease leases[3];
	for ( uint32_t iName = 0; iName < 3; ++iName ) {
		EXPECT_EQ( pool.acquire( pool.find( pszExpected[iName] ), leases[iName] ), SIR_STRING_OK );
	}
	EXPECT_EQ( pool.getStats().nActiveLongSlots, 3u );
	for ( uint32_t iName = 0; iName < 3; ++iName ) {
		EXPECT_STREQ( leases[iName].data(), pszExpected[iName] );
		leases[iName].release();
	}
	EXPECT_TRUE( pool.retireScope() );
	EXPECT_EQ( pool.getStats().nActiveLongSlots, 1u );
	EXPECT_EQ( pool.getStats().nActiveNameBudget, 3u );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, FixedBudgetsAndInvalidAdaptiveLimits )
{
	sirStringPoolConfig_t config = sirPoolTestConfig();
	config.bAdaptiveBudgets = false;
	SIRStringPool pool;
	std::vector<uint8_t> span( SIRStringPool::requiredBytes( config, 0 ) );
	EXPECT_TRUE( pool.initialize( { span.data(), span.size(), nullptr, 0 }, config ) );
	EXPECT_EQ( pool.getStats().nActiveNameBudget, config.nMaxNames );
	EXPECT_EQ( pool.getStats().nActiveDynamicTextBudget, config.nDynamicTextBytes );
	EXPECT_EQ( pool.getStats().nActiveShortSlots, config.nShortSlots );
	EXPECT_EQ( pool.getStats().nActiveLongSlots, config.nLongSlots );
	EXPECT_TRUE( pool.shutdown() );
	config.bAdaptiveBudgets = true;
	config.nInitialNameBudget = config.nMaxNames + 1;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config.nInitialNameBudget = 0;
	config.nInitialDynamicTextBytes = config.nDynamicTextBytes + 1;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config.nInitialDynamicTextBytes = 0;
	config.nInitialShortSlots = config.nShortSlots + 1;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config.nInitialShortSlots = 0;
	config.nInitialLongSlots = config.nLongSlots + 1;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
}

/* Six-tier admission, residency, and pinning regression coverage. */
static sirStringPoolConfig_t sirTestSixTierSmall( void )
{
	sirStringPoolConfig_t config = SIRStringPool::defaultConfig();
	config.nMaxNames = 32;
	config.nDynamicTextBytes = 128;
	config.nHashBuckets = 64;
	for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
		config.nTierSlots[iTier] = 2;
		config.nInitialTierSlots[iTier] = 1;
	}
	return config;
}

static std::string sirTestName( char nLetter, uint32_t nLength )
{
	return std::string( 1, nLetter ) + std::string( nLength - 1, 'q' );
}

TEST( SIRStringPool, SixTierDefaultCountsCapacitiesAndMemoryFootprint )
{
	const sirStringPoolConfig_t config = SIRStringPool::defaultConfig();
	const uint32_t nCounts[] = { 512, 256, 128, 64, 24, 12 };
	const uint32_t nBytes[] = { 16, 64, 128, 256, 512, 1024 };
	for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
		EXPECT_EQ( config.nTierSlots[iTier], nCounts[iTier] );
		EXPECT_EQ( config.nTierBytes[iTier], nBytes[iTier] );
	}
	SIRStringPool pool;
	std::vector<uint8_t> span( SIRStringPool::requiredBytes( config, 0 ) );
	ASSERT_TRUE( pool.initialize( { span.data(), span.size(), nullptr, 0 }, config ) );
	EXPECT_EQ( pool.getStats().nDecodedSlotCount, 996u );
	EXPECT_EQ( pool.getStats().nActiveShortSlots, 28u );
	EXPECT_EQ( pool.getStats().nActiveLongSlots, 4u );
	EXPECT_EQ( pool.getReservedBytes(), span.size() );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, SixTierAllBoundariesAndAdaptiveGrowth )
{
	const uint32_t nLengths[] = { 16, 64, 128, 256, 512, 1024 };
	std::vector<sirTestCookedName_t> names;
	for ( uint32_t iTier = 0; iTier < 6; ++iTier ) {
		names.push_back( { sirTestName( ( char )( 'a' + iTier * 2 ), nLengths[iTier] ), true } );
		names.push_back( { sirTestName( ( char )( 'b' + iTier * 2 ), nLengths[iTier] ), true } );
	}
	const auto catalog = sirTestCatalog( names );
	const auto config = sirTestSixTierSmall();
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	SIRStringLease leases[12];
	for ( uint32_t i = 0; i < names.size(); ++i ) {
		EXPECT_EQ( pool.acquire( pool.find( names[i].sText.data(), names[i].sText.size() ), leases[i] ), SIR_STRING_OK );
		EXPECT_EQ( leases[i].bytes(), names[i].sText.size() );
		EXPECT_EQ( memcmp( leases[i].data(), names[i].sText.data(), names[i].sText.size() ), 0 );
	}
	EXPECT_EQ( pool.getStats().nPinnedLeases, 12u );
	for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
		EXPECT_EQ( pool.getStats().nActiveTierSlots[iTier], 2u );
	}
	EXPECT_FALSE( pool.retireScope() );
	for ( SIRStringLease& lease : leases ) {
		lease.release();
	}
	EXPECT_TRUE( pool.retireScope() );
	for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
		EXPECT_EQ( pool.getStats().nActiveTierSlots[iTier], 1u );
	}
	EXPECT_EQ( pool.getStats().nCachedStrings, 0u );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, SixTierExactSizeClassBoundaries )
{
	sirStringPoolConfig_t config = sirTestSixTierSmall();
	const uint32_t nLengths[] = { 16, 17, 64, 65, 128, 129, 256, 257, 512, 513, 1024 };
	std::vector<sirTestCookedName_t> names;
	for ( uint32_t i = 0; i < 11; ++i ) {
		names.push_back( { sirTestName( ( char )( 'a' + i ), nLengths[i] ), true } );
	}
	const auto catalog = sirTestCatalog( names );
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	for ( const auto& name : names ) {
		SIRStringLease lease;
		EXPECT_EQ( pool.acquire( pool.find( name.sText.data(), name.sText.size() ), lease ), SIR_STRING_OK );
		EXPECT_EQ( lease.bytes(), name.sText.size() );
	}
	EXPECT_EQ( pool.getStats().nCachedStrings, 11u );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, SixTierPinnedFallbackAndEviction )
{
	sirStringPoolConfig_t config = sirTestSixTierSmall();
	for ( uint32_t iTier = 0; iTier < 6; ++iTier ) {
		config.nTierSlots[iTier] = iTier < 2 ? 1u : 0u;
		config.nInitialTierSlots[iTier] = 0;
	}
	const auto catalog = sirTestCatalog( { { "aa", true }, { "bb", true }, { "cc", true }, { "dd", true } } );
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	SIRStringLease first, second, third;
	EXPECT_EQ( pool.acquire( pool.find( "aa" ), first ), SIR_STRING_OK );
	EXPECT_EQ( pool.acquire( pool.find( "bb" ), second ), SIR_STRING_OK );
	EXPECT_EQ( pool.getStats().nActiveTierSlots[0], 1u );
	EXPECT_EQ( pool.getStats().nActiveTierSlots[1], 1u );
	EXPECT_EQ( pool.acquire( pool.find( "cc" ), third ), SIR_STRING_ALL_SLOTS_PINNED );
	EXPECT_STREQ( first.data(), "aa" );
	EXPECT_STREQ( second.data(), "bb" );
	first.release();
	EXPECT_EQ( pool.acquire( pool.find( "cc" ), third ), SIR_STRING_OK );
	EXPECT_STREQ( third.data(), "cc" );
	EXPECT_STREQ( second.data(), "bb" );
	third.release();
	second.release();
	EXPECT_EQ( pool.acquire( pool.find( "dd" ), third ), SIR_STRING_OK );
	third.release();
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, SixTierZeroSlotAndInvalidConfigurationMatrix )
{
	sirStringPoolConfig_t config = sirTestSixTierSmall();
	const size_t nOriginal = SIRStringPool::requiredBytes( config, 0 );
	ASSERT_TRUE( nOriginal > 0 );
	config.nTierBytes[3] = config.nTierBytes[2];
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config.nTierBytes[3] = 256;
	config.nTierBytes[5] = 512;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config.nTierBytes[5] = 1024;
	config.nInitialTierSlots[1] = 3;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config.nInitialTierSlots[1] = 1;
	config.nTierSlots[0] = UINT32_MAX;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config.nTierSlots[0] = 2;
	for ( uint32_t iTier = 0; iTier < 6; ++iTier ) {
		config.nTierSlots[iTier] = 0;
		config.nInitialTierSlots[iTier] = 0;
	}
	EXPECT_TRUE( SIRStringPool::requiredBytes( config, 0 ) > 0 );
	const auto catalog = sirTestCatalog( { { "alpha", true } } );
	SIRStringPool pool;
	std::vector<uint8_t> span;
	EXPECT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	SIRStringLease lease;
	EXPECT_EQ( pool.acquire( pool.find( "alpha" ), lease ), SIR_STRING_ALL_SLOTS_PINNED );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, SixTierFixedBudgetsNoAdaptiveGrowth )
{
	const auto catalog = sirTestCatalog( { { "aa", true }, { "bb", true }, { "cc", true } } );
	sirStringPoolConfig_t config = sirTestSixTierSmall();
	config.bAdaptiveBudgets = false;
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	for ( uint32_t iTier = 0; iTier < 6; ++iTier ) {
		EXPECT_EQ( pool.getStats().nActiveTierSlots[iTier], 2u );
	}
	SIRStringLease lease;
	EXPECT_EQ( pool.acquire( pool.find( "aa" ), lease ), SIR_STRING_OK );
	lease.release();
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, SixTierConcurrentMixedLengthAccess )
{
	const uint32_t nLengths[] = { 12, 40, 100, 190, 400, 900 };
	std::vector<sirTestCookedName_t> names;
	for ( uint32_t i = 0; i < 12; ++i ) {
		names.push_back( { sirTestName( ( char )( 'a' + i ), nLengths[i % 6] ), true } );
	}
	sirStringPoolConfig_t config = sirTestSixTierSmall();
	config.nTierSlots[0] = 4;
	config.nTierSlots[1] = 4;
	config.nTierSlots[2] = 4;
	config.nTierSlots[3] = 4;
	config.nTierSlots[4] = 4;
	config.nTierSlots[5] = 4;
	const auto catalog = sirTestCatalog( names );
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	std::atomic<uint32_t> nErrors( 0 );
	std::vector<std::thread> workers;
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		workers.emplace_back( [&, iWorker]() {
			for ( uint32_t i = 0; i < 1200; ++i ) {
				const uint32_t iName = ( i * 7 + iWorker * 3 ) % ( uint32_t )names.size();
				SIRStringLease lease;
				const auto& name = names[iName].sText;
				if ( pool.acquire( pool.find( name.data(), name.size() ), lease ) != SIR_STRING_OK || lease.bytes() != name.size() ||
				     memcmp( lease.data(), name.data(), name.size() ) != 0 ) {
					nErrors.fetch_add( 1, std::memory_order_relaxed );
				}
			}
		} );
	}
	for ( std::thread& worker : workers ) {
		worker.join();
	}
	EXPECT_EQ( nErrors.load(), 0u );
	EXPECT_EQ( pool.getStats().nPinnedLeases, 0u );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, SixTierCorruptCookedMissDoesNotConsumeBudget )
{
	auto catalog = sirTestCatalog( { { "compressed", true } } );
	sirStringPoolConfig_t config = sirTestSixTierSmall();
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	size_t iCatalog = span.size();
	for ( size_t i = 0; i + catalog.size() <= span.size(); ++i ) {
		if ( memcmp( span.data() + i, catalog.data(), catalog.size() ) == 0 ) {
			iCatalog = i;
			break;
		}
	}
	ASSERT_TRUE( iCatalog < span.size() );
	uint64_t nCatalogHash = 0;
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nCatalogHash |= ( uint64_t )catalog[36 + iByte] << ( iByte * 8 );
	}
	const SIRStringAtom atom = pool.atomFromCookedOrdinal( 0, nCatalogHash );
	ASSERT_TRUE( atom.isValid() );
	const size_t iToken = iCatalog + 56;
	const uint8_t nPrevious = span[iToken];
	span[iToken] = 127;
	const uint32_t nBefore = pool.getStats().nActiveTierSlots[0];
	SIRStringLease lease;
	EXPECT_EQ( pool.acquire( atom, lease ), SIR_STRING_CORRUPT_CATALOG );
	EXPECT_EQ( pool.getStats().nActiveTierSlots[0], nBefore );
	EXPECT_EQ( pool.getStats().nCachedStrings, 0u );
	span[iToken] = nPrevious;
	EXPECT_EQ( pool.acquire( atom, lease ), SIR_STRING_OK );
	lease.release();
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, CoveragePreflightCorruptionMatrix )
{
	sirStringPoolConfig_t config = sirPoolTestConfig();
	const std::vector<uint8_t> good = sirTestCatalog( { { "alpha", false }, { "beta", true } } );
	const size_t nReserved = SIRStringPool::requiredBytes( config, good.data(), good.size() );
	ASSERT_TRUE( nReserved > 0 );
	std::vector<uint8_t> backing( nReserved + 1024 );
	SIRStringPool pool;
	for ( uint32_t iCase = 0; iCase < 12; ++iCase ) {
		std::vector<uint8_t> malformed = good;
		switch ( iCase ) {
		case 0:
			malformed[0] = 'X';
			break;
		case 1:
			malformed[4] = 3;
			break;
		case 2:
			sirTestStore16( malformed, 6, 1 );
			break;
		case 3:
			sirTestStore32( malformed, 44, 0 );
			break;
		case 4:
			sirTestStore32( malformed, 12, 8 );
			break;
		case 5:
			sirTestStore32( malformed, 16, 8 );
			break;
		case 6:
			sirTestStore32( malformed, 20, 8 );
			break;
		case 7:
			sirTestStore32( malformed, 24, 0 );
			break;
		case 8:
			sirTestStore64( malformed, 28, 0 );
			break;
		case 9:
			sirTestStore64( malformed, 36, 0 );
			break;
		case 10:
			sirTestStore32( malformed, 8, 1 );
			break;
		case 11:
			sirTestStore32( malformed, 8, UINT32_MAX );
			break;
		}
		EXPECT_FALSE( pool.initialize( { backing.data(), backing.size(), malformed.data(), malformed.size() }, config ) );
		EXPECT_FALSE( pool.isInitialized() );
	}
	EXPECT_TRUE( pool.shutdown() );
	EXPECT_FALSE( pool.retireScope() );
	EXPECT_EQ( pool.getDecodedByteLength( SIRStringAtom( 10 ) ), 0u );
	EXPECT_EQ( pool.getContentHash( SIRStringAtom( 10 ) ), 0u );
}

TEST( SIRStringPool, CoverageTierOverflowAndInvalidInitShapes )
{
	sirStringPoolConfig_t config = SIRStringPool::defaultConfig();
	config.nTierSlots[0] = UINT32_MAX;
	config.nTierSlots[1] = UINT32_MAX;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config = SIRStringPool::defaultConfig();
	config.nTierBytes[5] = 512;
	EXPECT_EQ( SIRStringPool::requiredBytes( config, 0 ), 0u );
	config = SIRStringPool::defaultConfig();
	config.nTierBytes[5] = 2048;
	EXPECT_TRUE( SIRStringPool::requiredBytes( config, 0 ) > 0 );
	config = sirPoolTestConfig();
	config.nHashBuckets = 1;
	EXPECT_TRUE( SIRStringPool::requiredBytes( config, 0 ) > 0 );
	config = sirPoolTestConfig();
	config.nHashBuckets = 0x80000000u;
	EXPECT_TRUE( SIRStringPool::requiredBytes( config, 0 ) > 0 );
}

TEST( SIRStringPool, CoverageHotPinAndDirectReadPaths )
{
	auto catalog = sirTestCatalog( { { "alpha", false }, { "bravo", true }, { "charlie", true } } );
	auto config = sirTestSixTierSmall();
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	const SIRStringAtom raw = pool.find( "alpha" );
	const SIRStringAtom cooked = pool.find( "bravo" );
	const SIRStringAtom third = pool.find( "charlie" );
	const SIRStringAtom dynamic = pool.intern( "dynamically-registered" );
	ASSERT_TRUE( raw.isValid() && cooked.isValid() && third.isValid() && dynamic.isValid() );
	EXPECT_EQ( pool.getDecodedByteLength( raw ), 5u );
	EXPECT_EQ( pool.getDecodedByteLength( cooked ), 5u );
	EXPECT_EQ( pool.getDecodedByteLength( dynamic ), 22u );
	EXPECT_EQ( pool.getContentHash( raw ), SIRStringPool::hashString( "alpha", 5 ) );
	EXPECT_EQ( pool.getContentHash( cooked ), SIRStringPool::hashString( "bravo", 5 ) );
	EXPECT_EQ( pool.getContentHash( dynamic ), SIRStringPool::hashString( "dynamically-registered", 22 ) );
	uint32_t nBytes = 100;
	EXPECT_STREQ( pool.peekRaw( raw, nBytes ), "alpha" );
	EXPECT_STREQ( pool.peekRaw( dynamic, nBytes ), "dynamically-registered" );
	EXPECT_EQ( pool.peekRaw( cooked, nBytes ), nullptr );
	SIRStringLease first;
	EXPECT_EQ( pool.acquire( cooked, first ), SIR_STRING_OK );
	EXPECT_STREQ( first.data(), "bravo" );
	SIRStringLease second;
	EXPECT_EQ( pool.acquire( cooked, second ), SIR_STRING_OK );
	EXPECT_STREQ( second.data(), "bravo" );
	EXPECT_EQ( pool.getStats().nPinnedLeases, 2u );
	first.release();
	second.release();
	SIRStringLease thirdLease;
	EXPECT_EQ( pool.acquire( third, thirdLease ), SIR_STRING_OK );
	thirdLease.release();
	SIRStringLease rehot;
	EXPECT_EQ( pool.acquire( cooked, rehot ), SIR_STRING_OK );
	rehot.release();
	EXPECT_EQ( pool.decodeInto( cooked, nullptr, 10, nBytes ), SIR_STRING_OUTPUT_TOO_SMALL );
	char szBytes[64] = {};
	EXPECT_EQ( pool.decodeInto( dynamic, szBytes, sizeof( szBytes ), nBytes ), SIR_STRING_OK );
	EXPECT_STREQ( szBytes, "dynamically-registered" );
	EXPECT_TRUE( pool.shutdown() );
}

TEST( SIRStringPool, CoverageTierPinnedFallbackAndRetirement )
{
	auto catalog = sirTestCatalog( { { "aaaaaaa", true }, { "bbbbbbb", true }, { "ccccccc", true }, { "ddddddd", true } } );
	auto config = sirTestSixTierSmall();
	for ( uint32_t i = 0; i < SIR_STRING_CACHE_TIER_COUNT; ++i ) {
		config.nTierSlots[i] = 1;
		config.nInitialTierSlots[i] = 1;
	}
	SIRStringPool pool;
	std::vector<uint8_t> span;
	ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
	SIRStringLease leases[4];
	const char *names[] = { "aaaaaaa", "bbbbbbb", "ccccccc", "ddddddd" };
	for ( uint32_t i = 0; i < 4; ++i ) {
		EXPECT_EQ( pool.acquire( pool.find( names[i] ), leases[i] ), SIR_STRING_OK );
		EXPECT_STREQ( leases[i].data(), names[i] );
	}
	EXPECT_FALSE( pool.retireScope() );
	for ( SIRStringLease& lease : leases ) {
		lease.release();
	}
	EXPECT_TRUE( pool.retireScope() );
	const SIRStringAtom atom = pool.find( "aaaaaaa" );
	EXPECT_TRUE( atom.isValid() );
	EXPECT_EQ( pool.acquire( atom, leases[0] ), SIR_STRING_OK );
	leases[0].release();
	EXPECT_TRUE( pool.shutdown() );
}

/* Test-only negative-state instrumentation for private invariant guards.
   This class is the sole friend of the pool and does not alter its ABI. */
class SIRStringPoolTestAccess
{
public:
	static void testCarveGuards( void )
	{
		sirPoolTestAllocator_t allocatorState = {};
		sirContainerAllocator_t domain = sirPoolTestDomain( &allocatorState );
		const sirStringPoolConfig_t config = sirPoolTestConfig();
		SIRStringPool pool;
		ASSERT_TRUE( pool.initialize( &domain, config ) );
		uint8_t *pMemory = pool.m_pMemory;
		const size_t nCapacity = pool.m_nMemoryBytes;
		const size_t nCursor = pool.m_nCursor;
		EXPECT_EQ( pool.carve( 1, 0 ), nullptr );
		EXPECT_EQ( pool.carve( 1, 3 ), nullptr );
		pool.m_pMemory = nullptr;
		EXPECT_EQ( pool.carve( 1, 1 ), nullptr );
		pool.m_pMemory = pMemory;
		pool.m_nCursor = SIZE_MAX;
		EXPECT_EQ( pool.carve( 1, 1 ), nullptr );
		pool.m_nCursor = UINTPTR_MAX - ( uintptr_t )pMemory;
		EXPECT_EQ( pool.carve( 1, 8 ), nullptr );
		pool.m_nCursor = nCursor;
		pool.m_nMemoryBytes = nCursor;
		EXPECT_EQ( pool.carve( 1, 1 ), nullptr );
		pool.m_nCursor = nCursor + 1;
		EXPECT_EQ( pool.carve( 0, 1 ), nullptr );
		pool.m_nCursor = nCursor;
		pool.m_nMemoryBytes = nCapacity;
		EXPECT_TRUE( pool.shutdown() );
		EXPECT_EQ( allocatorState.nLive, 0u );
	}

	static void testCookedDecodingGuards( void )
	{
		const auto catalog = sirTestCatalog( { { "", true }, { "alpha", false }, { "bravo", true }, { "charlie", true } } );
		const auto config = sirTestSixTierSmall();
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
		uint32_t nWritten = 0;
		char szText[128] = {};
		EXPECT_FALSE( pool.decodeCooked( 0, nullptr, sizeof( szText ), nWritten ) );
		EXPECT_FALSE( pool.decodeCooked( UINT32_MAX, szText, sizeof( szText ), nWritten ) );
		EXPECT_FALSE( pool.decodeCooked( 1, szText, 1, nWritten ) );
		EXPECT_TRUE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		EXPECT_EQ( nWritten, 0u );
		EXPECT_TRUE( pool.matchesCooked( 0, "", 0 ) );
		EXPECT_FALSE( pool.matchesCooked( 0, "a", 1 ) );
		EXPECT_TRUE( pool.matchesCooked( 1, "alpha", 5 ) );
		EXPECT_FALSE( pool.matchesCooked( 1, "omega", 5 ) );
		EXPECT_TRUE( pool.matchesCooked( 2, "bravo", 5 ) );
		EXPECT_FALSE( pool.matchesCooked( 2, "wrong", 5 ) );
		EXPECT_FALSE( pool.matchesCooked( 2, "b", 1 ) );
		const SIRStringAtom dynamic = pool.intern( "dynamic" );
		ASSERT_TRUE( dynamic.isValid() );
		EXPECT_TRUE( pool.matchesDynamic( pool.m_nCookedNames, "dynamic", 7 ) );
		EXPECT_FALSE( pool.matchesDynamic( pool.m_nCookedNames, "dynamic", 6 ) );
		EXPECT_FALSE( pool.matchesDynamic( pool.m_nCookedNames, "missing", 7 ) );
		EXPECT_FALSE( pool.matchesDynamic( pool.m_nCookedNames, "x", 1 ) );
		const uint32_t nBlobBytes = pool.m_nBlobBytes;
		pool.m_nBlobBytes = 0;
		EXPECT_FALSE( pool.decodeCooked( 2, szText, sizeof( szText ), nWritten ) );
		pool.m_nBlobBytes = nBlobBytes;
		uint8_t *pBlob = const_cast<uint8_t *>( pool.m_pBlob );
		const uint32_t nBravoOffset = ( uint32_t )catalog[48 + 16] | ( ( uint32_t )catalog[48 + 17] << 8 );
		const uint8_t nOriginalToken = pBlob[nBravoOffset];
		pBlob[nBravoOffset] = 127;
		EXPECT_FALSE( pool.decodeCooked( 2, szText, sizeof( szText ), nWritten ) );
		pBlob[nBravoOffset] = nOriginalToken;
		EXPECT_TRUE( pool.decodeCooked( 2, szText, sizeof( szText ), nWritten ) );
		EXPECT_STREQ( szText, "bravo" );
		EXPECT_EQ( pool.makeAtom( UINT32_MAX ).getId(), 0u );
		EXPECT_TRUE( pool.makeAtom( 0 ).isValid() );
		const uint32_t nBuckets = pool.m_nHashBuckets;
		pool.m_nHashBuckets = 0;
		EXPECT_EQ( pool.findLocalFlat( "alpha", 5, SIRStringPool::hashString( "alpha", 5 ) ), 0u );
		pool.m_nHashBuckets = nBuckets;
		EXPECT_TRUE( pool.shutdown() );
	}

	static void testHotHintGuards( void )
	{
		const auto catalog = sirTestCatalog( { { "bravo", true }, { "charlie", true } } );
		const auto config = sirTestSixTierSmall();
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
		SIRStringLease first;
		const SIRStringAtom atom = pool.atomFromCookedOrdinal( 0, sirTestCatalogCorpusHash( catalog ) );
		EXPECT_FALSE( pool.tryAcquireHot( 0, atom.getId(), first ) );
		EXPECT_EQ( pool.acquire( atom, first ), SIR_STRING_OK );
		SIRStringLease second;
		EXPECT_FALSE( pool.tryAcquireHot( 0, atom.getId() + 1, second ) );
		EXPECT_FALSE( pool.tryAcquireHot( 5, atom.getId(), second ) );
		const uint32_t nHot = pool.m_pCookedHot[0].load( std::memory_order_relaxed );
		ASSERT_TRUE( nHot != 0 );
		pool.m_pCookedHot[0].store( pool.m_nCacheSlots + 1, std::memory_order_relaxed );
		EXPECT_FALSE( pool.tryAcquireHot( 0, atom.getId(), second ) );
		pool.m_pCookedHot[0].store( 0, std::memory_order_relaxed );
		EXPECT_FALSE( pool.tryAcquireHot( 0, atom.getId(), second ) );
		pool.m_pCookedHot[0].store( nHot, std::memory_order_relaxed );
		EXPECT_TRUE( pool.tryAcquireHot( 0, atom.getId(), second ) );
		EXPECT_EQ( pool.getStats().nPinnedLeases, 2u );
		second.release();
		first.release();
		EXPECT_TRUE( pool.shutdown() );
	}

	static void testRawLoaderGuards( void )
	{
		const auto good = sirTestCatalog( { { "alpha", false }, { "beta", true } } );
		const auto config = sirPoolTestConfig();
		for ( uint32_t iCase = 0; iCase < 11; ++iCase ) {
			SIRStringPool pool;
			std::vector<uint8_t> span;
			ASSERT_TRUE( sirTestInitCatalog( &pool, good, config, &span ) );
			auto corrupted = good;
			switch ( iCase ) {
			case 0:
				corrupted[0] = 'X';
				break;
			case 1:
				corrupted[4] = 2;
				break;
			case 2:
				corrupted[6] = 1;
				break;
			case 3:
				sirTestStore32( corrupted, 44, 47 );
				break;
			case 4:
				sirTestStore32( corrupted, 8, 1 );
				break;
			case 5:
				sirTestStore32( corrupted, 12, 0 );
				break;
			case 6:
				sirTestStore32( corrupted, 16, 1 );
				break;
			case 7:
				sirTestStore32( corrupted, 20, 1 );
				break;
			case 8:
				sirTestStore32( corrupted, 24, 0 );
				break;
			case 9:
				sirTestStore64( corrupted, 28, 0 );
				break;
			case 10:
				sirTestStore64( corrupted, 36, 0 );
				break;
			}
			EXPECT_FALSE( pool.loadCatalog( corrupted.data(), corrupted.size() ) );
			EXPECT_TRUE( pool.shutdown() );
		}
	}

	static void testCatalogRecordCorruption( void )
	{
		const auto good = sirTestCatalog( { { "alpha", false }, { "bravo", true } } );
		const auto config = sirPoolTestConfig();
		for ( uint32_t iCase = 0; iCase < 15; ++iCase ) {
			SIRStringPool pool;
			std::vector<uint8_t> span;
			ASSERT_TRUE( sirTestInitCatalog( &pool, good, config, &span ) );
			auto bad = good;
			const uint32_t nBlobStart = 48u + 16u;
			switch ( iCase ) {
			case 0:
				sirTestStore16( bad, 48 + 4, 1025 );
				break;
			case 1:
				sirTestStore16( bad, 48 + 6, 0x4005u );
				break;
			case 2:
				sirTestStore16( bad, 48 + 6, 0xc005u );
				break;
			case 3:
				sirTestStore32( bad, 48, 10000u );
				break;
			case 4:
				sirTestStore16( bad, 48 + 6, 0xbfffu );
				break;
			case 5:
				sirTestStore16( bad, 48 + 6, 0x8004u );
				break;
			case 6:
				sirTestStore32( bad, 48, ( uint32_t )( bad.size() - nBlobStart - 5 ) );
				break;
			case 7:
				bad[nBlobStart + 5] = 'X';
				break;
			case 8:
				sirTestStore16( bad, 48 + 8 + 4, 6 );
				break;
			case 9:
				bad[nBlobStart] = 0xff;
				break;
			case 10:
				memcpy( bad.data() + nBlobStart, "zzzzz", 5 );
				break;
			case 11:
				memcpy( bad.data() + nBlobStart, "bravo", 5 );
				break;
			case 12:
				sirTestStore64( bad, 36, 0u );
				break;
			case 13:
				sirTestStore16( bad, 48 + 8 + 6, 1 );
				break;
			case 14:
				sirTestStore16( bad, 48 + 8 + 6, 0x8005u );
				break;
			}
			sirTestRehashCatalog( bad );
			EXPECT_FALSE( pool.loadCatalog( bad.data(), bad.size() ) );
			EXPECT_TRUE( pool.shutdown() );
		}
	}

	static void testDecodeCorruption( void )
	{
		const auto good = sirTestCatalog( { { "bravo", true }, { "charlie", true } } );
		const auto config = sirTestSixTierSmall();
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, good, config, &span ) );
		uint8_t *pEntry = ( uint8_t * )pool.m_pCooked;
		uint8_t saved[8] = {};
		memcpy( saved, pEntry, sizeof( saved ) );
		uint8_t *pBlob = const_cast<uint8_t *>( pool.m_pBlob );
		const uint8_t savedToken = pBlob[0];
		const uint8_t savedDistance = pBlob[1];
		char szText[128] = {};
		uint32_t nWritten = 0;
		uint32_t nBadOffset = pool.m_nBlobBytes + 1;
		memcpy( pEntry, &nBadOffset, sizeof( nBadOffset ) );
		EXPECT_FALSE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		memcpy( pEntry, saved, sizeof( saved ) );
		nBadOffset = pool.m_nBlobBytes;
		memcpy( pEntry, &nBadOffset, sizeof( nBadOffset ) );
		EXPECT_FALSE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		memcpy( pEntry, saved, sizeof( saved ) );
		uint16_t nBadFlags = 0x4000u;
		memcpy( pEntry + 6, &nBadFlags, sizeof( nBadFlags ) );
		EXPECT_FALSE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		memcpy( pEntry, saved, sizeof( saved ) );
		pBlob[0] = 128;
		pBlob[1] = 0;
		EXPECT_FALSE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		pBlob[1] = 1;
		EXPECT_FALSE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		uint16_t nBadLength = 2;
		memcpy( pEntry + 4, &nBadLength, sizeof( nBadLength ) );
		EXPECT_FALSE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		memcpy( pEntry, saved, sizeof( saved ) );
		nBadFlags = 1;
		memcpy( pEntry + 6, &nBadFlags, sizeof( nBadFlags ) );
		EXPECT_FALSE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		pBlob[0] = 1;
		EXPECT_FALSE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		pBlob[0] = 0;
		EXPECT_FALSE( pool.decodeCooked( 0, szText, sizeof( szText ), nWritten ) );
		pBlob[0] = savedToken;
		pBlob[1] = savedDistance;
		memcpy( pEntry, saved, sizeof( saved ) );
		const uint32_t nBase = pool.m_nBaseId;
		pool.m_nBaseId = 0;
		EXPECT_TRUE( pool.matchesCooked( 0, "bravo", 5 ) );
		EXPECT_FALSE( pool.matchesCooked( 0, "other", 5 ) );
		pBlob[0] = 127;
		EXPECT_FALSE( pool.matchesCooked( 0, "bravo", 5 ) );
		pBlob[0] = savedToken;
		pool.m_nBaseId = nBase;
		EXPECT_TRUE( pool.shutdown() );
	}

	static void testTierGenerationWrapAndStaleSlot( void )
	{
		const auto good = sirTestCatalog( { { "aaaaaaa", true }, { "bbbbbbb", true }, { "ccccccc", true } } );
		auto config = sirTestSixTierSmall();
		for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
			config.nTierSlots[iTier] = iTier == 0 ? 1 : 0;
			config.nInitialTierSlots[iTier] = iTier == 0 ? 1 : 0;
		}
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, good, config, &span ) );
		SIRStringLease lease;
		const auto first = pool.atomFromCookedOrdinal( 0, sirTestCatalogCorpusHash( good ) );
		const auto second = pool.atomFromCookedOrdinal( 1, sirTestCatalogCorpusHash( good ) );
		EXPECT_EQ( pool.acquire( first, lease ), SIR_STRING_OK );
		lease.release();
		static_assert( sizeof( std::atomic<uint32_t> ) == sizeof( uint32_t ), "test fixture requires 32-bit atomics" );
		uint8_t *pSlotBytes = ( uint8_t * )pool.m_pCache;
		const uint32_t nWrap = UINT32_MAX;
		memcpy( pSlotBytes + 8, &nWrap, sizeof( nWrap ) );
		EXPECT_EQ( pool.acquire( second, lease ), SIR_STRING_OK );
		uint32_t nGeneration = 0;
		memcpy( &nGeneration, pSlotBytes + 8, sizeof( nGeneration ) );
		EXPECT_EQ( nGeneration, 1u );
		lease.release();
		EXPECT_EQ( pool.m_pCookedHot[0].load(), 0u );
		EXPECT_TRUE( pool.shutdown() );
	}

	static void testTextRefAndConfigurationEdges( void )
	{
		const auto good = sirTestCatalog( { { "bravo", true } } );
		const auto config = sirPoolTestConfig();
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, good, config, &span ) );
		SIRStringTextRef reference;
		const auto atom = pool.find( "bravo" );
		const auto missing = pool.intern( "additional" );
		const SIRStringAtom invalid( UINT32_MAX );
		reference.set( &pool, invalid );
		const char *pText = nullptr;
		uint32_t nBytes = 0;
		EXPECT_EQ( reference.get( pText, nBytes ), SIR_STRING_INVALID_ATOM );
		EXPECT_EQ( pText, nullptr );
		reference.set( &pool, atom );
		reference.set( &pool, atom );
		EXPECT_EQ( reference.get( pText, nBytes ), SIR_STRING_OK );
		EXPECT_STREQ( pText, "bravo" );
		reference.release();
		EXPECT_TRUE( missing.isValid() );
		EXPECT_TRUE( pool.shutdown() );
		auto invalidConfig = SIRStringPool::defaultConfig();
		invalidConfig.nTierBytes[5] = 1000;
		EXPECT_EQ( SIRStringPool::requiredBytes( invalidConfig, 0 ), 0u );
	}

	static void testRawEmptyAndDirectDecodeBranches( void )
	{
		const auto catalog = sirTestCatalog( { { "", false }, { "alpha", false }, { "bravo", true } } );
		const auto config = sirPoolTestConfig();
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
		const auto empty = pool.atomFromCookedOrdinal( 0, sirTestCatalogCorpusHash( catalog ) );
		const auto raw = pool.atomFromCookedOrdinal( 1, sirTestCatalogCorpusHash( catalog ) );
		const auto cooked = pool.atomFromCookedOrdinal( 2, sirTestCatalogCorpusHash( catalog ) );
		EXPECT_TRUE( pool.matchesCooked( 0, "", 0 ) );
		EXPECT_FALSE( pool.matchesCooked( 0, "X", 1 ) );
		EXPECT_TRUE( pool.matchesCooked( 1, "alpha", 5 ) );
		EXPECT_FALSE( pool.matchesCooked( 1, "other", 5 ) );
		char szText[64] = {};
		uint32_t nWritten = 0;
		EXPECT_EQ( pool.decodeInto( empty, szText, sizeof( szText ), nWritten ), SIR_STRING_OK );
		EXPECT_EQ( nWritten, 0u );
		EXPECT_EQ( pool.decodeInto( raw, szText, sizeof( szText ), nWritten ), SIR_STRING_OK );
		EXPECT_STREQ( szText, "alpha" );
		EXPECT_EQ( pool.decodeInto( cooked, szText, sizeof( szText ), nWritten ), SIR_STRING_OK );
		EXPECT_STREQ( szText, "bravo" );
		EXPECT_TRUE( pool.shutdown() );
		SIRStringPool dynamicPool;
		std::vector<uint8_t> dynamicSpan( SIRStringPool::requiredBytes( config, 0 ) );
		ASSERT_TRUE( dynamicPool.initialize( { dynamicSpan.data(), dynamicSpan.size(), nullptr, 0 }, config ) );
		const auto dynamicEmpty = dynamicPool.intern( "", 0 );
		EXPECT_TRUE( dynamicEmpty.isValid() );
		EXPECT_TRUE( dynamicPool.matchesDynamic( 0, "", 0 ) );
		EXPECT_FALSE( dynamicPool.matchesDynamic( 0, "a", 1 ) );
		EXPECT_EQ( dynamicPool.decodeInto( dynamicEmpty, szText, sizeof( szText ), nWritten ), SIR_STRING_OK );
		EXPECT_EQ( nWritten, 0u );
		EXPECT_TRUE( dynamicPool.shutdown() );
	}

	static void testTierHotAndPinnedErrorBranches( void )
	{
		const auto catalog = sirTestCatalog( { { "aaaaaaa", true }, { "bbbbbbb", true } } );
		auto config = sirTestSixTierSmall();
		for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
			config.nTierSlots[iTier] = iTier == 0 ? 1 : 0;
			config.nInitialTierSlots[iTier] = iTier == 0 ? 1 : 0;
		}
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
		const auto first = pool.atomFromCookedOrdinal( 0, sirTestCatalogCorpusHash( catalog ) );
		const auto second = pool.atomFromCookedOrdinal( 1, sirTestCatalogCorpusHash( catalog ) );
		SIRStringLease lease;
		EXPECT_EQ( pool.acquire( first, lease ), SIR_STRING_OK );
		lease.release();
		EXPECT_EQ( pool.acquireTiered( 0, first, lease ), SIR_STRING_OK );
		lease.release();
		uint8_t *pSlotBytes = ( uint8_t * )pool.m_pCache;
		const uint32_t nForgedDynamicAtom = pool.m_nBaseId + pool.m_nCookedNames;
		std::atomic<uint32_t> *pAtomId = ( std::atomic<uint32_t> * )( pSlotBytes + sizeof( std::atomic<uint32_t> ) );
		pAtomId->store( nForgedDynamicAtom, std::memory_order_relaxed );
		EXPECT_EQ( pool.acquireTiered( 1, second, lease ), SIR_STRING_OK );
		lease.release();
		EXPECT_TRUE( pool.shutdown() );
		SIRStringPool rejected;
		ASSERT_TRUE( sirTestInitCatalog( &rejected, catalog, config, &span ) );
		std::atomic<uint32_t> *pPin = ( std::atomic<uint32_t> * )rejected.m_pCache;
		pPin->store( 1, std::memory_order_relaxed );
		EXPECT_EQ( rejected.acquireTiered( 0, rejected.atomFromCookedOrdinal( 0, sirTestCatalogCorpusHash( catalog ) ), lease ),
		           SIR_STRING_ALL_SLOTS_PINNED );
		pPin->store( 0, std::memory_order_relaxed );
		EXPECT_TRUE( rejected.shutdown() );
	}

	static void testLegacyScanBranches( void )
	{
		const auto catalog = sirTestCatalog( { { "aaaaaaa", true }, { "bbbbbbb", true }, { "ccccccc", true } } );
		auto config = sirPoolTestConfig();
		config.nShortSlots = 3;
		config.nLongSlots = 0;
		config.nInitialShortSlots = 1;
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
		const uint64_t nHash = sirTestCatalogCorpusHash( catalog );
		SIRStringLease lease;
		EXPECT_EQ( pool.acquire( pool.atomFromCookedOrdinal( 0, nHash ), lease ), SIR_STRING_OK );
		lease.release();
		pool.m_nNextUnusedShort = pool.m_nShortSlots;
		pool.m_nActiveShortSlots = 1;
		pool.m_nCacheCursor = 1;
		EXPECT_EQ( pool.acquire( pool.atomFromCookedOrdinal( 1, nHash ), lease ), SIR_STRING_OK );
		lease.release();
		EXPECT_TRUE( pool.shutdown() );
		SIRStringPool generationPool;
		config.nShortSlots = 1;
		config.nInitialShortSlots = 1;
		ASSERT_TRUE( sirTestInitCatalog( &generationPool, catalog, config, &span ) );
		EXPECT_EQ( generationPool.acquire( generationPool.atomFromCookedOrdinal( 0, nHash ), lease ), SIR_STRING_OK );
		lease.release();
		uint8_t *pSlotBytes = ( uint8_t * )generationPool.m_pCache;
		const uint32_t nWrap = UINT32_MAX;
		memcpy( pSlotBytes + 8, &nWrap, sizeof( nWrap ) );
		EXPECT_EQ( generationPool.acquire( generationPool.atomFromCookedOrdinal( 1, nHash ), lease ), SIR_STRING_OK );
		uint32_t nGeneration = 0;
		memcpy( &nGeneration, pSlotBytes + 8, sizeof( nGeneration ) );
		EXPECT_EQ( nGeneration, 1u );
		lease.release();
		EXPECT_TRUE( generationPool.shutdown() );
	}

	static void testMalformedFallbackAndFullTable( void )
	{
		const auto catalog = sirTestCatalog( { { "bravo", true }, { "charlie", true } } );
		auto config = sirPoolTestConfig();
		config.nShortSlots = 1;
		config.nLongSlots = 0;
		config.nShortBytes = 16;
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
		const uint64_t nHash = sirTestCatalogCorpusHash( catalog );
		const auto first = pool.atomFromCookedOrdinal( 0, nHash );
		const auto second = pool.atomFromCookedOrdinal( 1, nHash );
		SIRStringLease lease;
		EXPECT_EQ( pool.acquire( first, lease ), SIR_STRING_OK );
		char szText[32] = {};
		uint32_t nWritten = 0;
		EXPECT_EQ( pool.decodeInto( second, szText, sizeof( szText ), nWritten ), SIR_STRING_OK );
		EXPECT_STREQ( szText, "charlie" );
		uint8_t *pBlob = const_cast<uint8_t *>( pool.m_pBlob );
		const uint8_t nOld = pBlob[6];
		pBlob[6] = 127;
		EXPECT_EQ( pool.decodeInto( second, szText, sizeof( szText ), nWritten ), SIR_STRING_CORRUPT_CATALOG );
		pBlob[6] = nOld;
		lease.release();
		EXPECT_TRUE( pool.shutdown() );
	}

	static void testRemainingDefensiveBoundaries( void )
	{
		SIRStringPool uninitialized;
		EXPECT_EQ( uninitialized.getStats().nCachedStrings, 0u );
		const auto catalog = sirTestCatalog( { { "bravo", true } } );
		auto config = sirTestSixTierSmall();
		for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
			config.nTierSlots[iTier] = iTier == 0 ? 1 : 0;
			config.nInitialTierSlots[iTier] = iTier == 0 ? 1 : 0;
		}
		SIRStringPool pool;
		std::vector<uint8_t> span;
		ASSERT_TRUE( sirTestInitCatalog( &pool, catalog, config, &span ) );
		const auto atom = pool.atomFromCookedOrdinal( 0, sirTestCatalogCorpusHash( catalog ) );
		SIRStringLease lease;
		uint32_t szTierBytes[SIR_STRING_CACHE_TIER_COUNT] = {};
		for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
			szTierBytes[iTier] = pool.m_nTierBytes[iTier];
			pool.m_nTierBytes[iTier] = 1;
		}
		EXPECT_EQ( pool.acquireTiered( 0, atom, lease ), SIR_STRING_OUTPUT_TOO_SMALL );
		for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
			pool.m_nTierBytes[iTier] = szTierBytes[iTier];
		}
		EXPECT_EQ( pool.acquire( atom, lease ), SIR_STRING_OK );
		lease.release();
		const uint32_t nBase = pool.m_nBaseId;
		pool.m_nBaseId = 0;
		EXPECT_EQ( pool.acquireTiered( 0, atom, lease ), SIR_STRING_OK );
		lease.release();
		pool.m_nBaseId = nBase;
		const uint32_t nOriginalNames = pool.m_nNames.load( std::memory_order_relaxed );
		pool.m_nNames.store( pool.m_nMaxNames, std::memory_order_relaxed );
		EXPECT_FALSE( pool.intern( "new-descriptor" ).isValid() );
		pool.m_nNames.store( nOriginalNames, std::memory_order_relaxed );
		const uint32_t nTextCapacity = pool.m_nDynamicTextCapacity;
		pool.m_nDynamicTextCapacity = 0;
		EXPECT_FALSE( pool.intern( "new-descriptor" ).isValid() );
		pool.m_nDynamicTextCapacity = nTextCapacity;
		EXPECT_TRUE( pool.shutdown() );
		auto legacy = sirPoolTestConfig();
		legacy.nShortSlots = 0;
		legacy.nLongSlots = 0;
		SIRStringPool oldPool;
		ASSERT_TRUE( sirTestInitCatalog( &oldPool, catalog, legacy, &span ) );
		const uint32_t nLongBytes = oldPool.m_nLongBytes;
		oldPool.m_nLongBytes = 1;
		EXPECT_EQ( oldPool.acquire( oldPool.atomFromCookedOrdinal( 0, sirTestCatalogCorpusHash( catalog ) ), lease ),
		           SIR_STRING_OUTPUT_TOO_SMALL );
		oldPool.m_nLongBytes = nLongBytes;
		EXPECT_TRUE( oldPool.shutdown() );
	}

protected:
private:
};

TEST( SIRStringPool, CoverageFaultInjectedInternalGuards )
{
	SIRStringPoolTestAccess::testCarveGuards();
	SIRStringPoolTestAccess::testCookedDecodingGuards();
	SIRStringPoolTestAccess::testHotHintGuards();
	SIRStringPoolTestAccess::testRawLoaderGuards();
	SIRStringPoolTestAccess::testCatalogRecordCorruption();
	SIRStringPoolTestAccess::testDecodeCorruption();
	SIRStringPoolTestAccess::testTierGenerationWrapAndStaleSlot();
	SIRStringPoolTestAccess::testTextRefAndConfigurationEdges();
	SIRStringPoolTestAccess::testRawEmptyAndDirectDecodeBranches();
	SIRStringPoolTestAccess::testTierHotAndPinnedErrorBranches();
	SIRStringPoolTestAccess::testLegacyScanBranches();
	SIRStringPoolTestAccess::testMalformedFallbackAndFullTable();
	SIRStringPoolTestAccess::testRemainingDefensiveBoundaries();
}
