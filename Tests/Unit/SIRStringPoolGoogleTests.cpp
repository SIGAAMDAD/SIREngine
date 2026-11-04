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
			sirTestAppend16( bytes, ( uint16_t )( 1 + name.sText.size() ) );
			blob.push_back( ( uint8_t )( name.sText.size() - 1 ) );
			blob.insert( blob.end(), name.sText.begin(), name.sText.end() );
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
