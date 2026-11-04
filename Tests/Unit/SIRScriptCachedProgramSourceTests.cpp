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

#include <Scripting/Core/SIRScriptCachedProgramSource.h>
#include <Scripting/Core/SIRScriptBindingRegistry.h>

#include <chrono>
#include <filesystem>
#if defined( _WIN32 )
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirCachedSourceTestAllocator_s
{
	uint32_t nAllocations;
	uint32_t nReleases;
	uint32_t nLive;
	bool bFail;
} sirCachedSourceTestAllocator_t;

static sirScriptResult_t testAlloc( void *pUser, uint32_t nBytes, uint32_t nAlignment, sirScriptPayloadAllocation_t *pResult ) noexcept
{
	sirCachedSourceTestAllocator_t *pAllocator = ( sirCachedSourceTestAllocator_t * )pUser;
	if ( pAllocator->bFail ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	const size_t nRequiredAlignment = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	const size_t nSize = ( ( ( size_t )nBytes + nRequiredAlignment - 1 ) / nRequiredAlignment ) * nRequiredAlignment;
#if defined( _WIN32 )
	void *pData = _aligned_malloc( nSize, nRequiredAlignment );
#else
	void *pData = aligned_alloc( nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment, nSize );
#endif
	if ( pData == nullptr ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	++pAllocator->nAllocations;
	++pAllocator->nLive;
	*pResult = { pData, pAllocator->nAllocations };
	return SIR_SCRIPT_OK;
}

static void testFree( void *pUser, sirScriptPayloadAllocation_t allocation ) noexcept
{
	sirCachedSourceTestAllocator_t *pAllocator = ( sirCachedSourceTestAllocator_t * )pUser;
	if ( allocation.pData != nullptr ) {
#if defined( _WIN32 )
		_aligned_free( allocation.pData );
#else
		free( allocation.pData );
#endif
		++pAllocator->nReleases;
		--pAllocator->nLive;
	}
}

static sirScriptPayloadAllocatorAPI_t testAllocator( sirCachedSourceTestAllocator_t *pAllocator ) noexcept
{
	return { SIR_SCRIPT_PAYLOAD_ALLOCATOR_ABI_REVISION, pAllocator, 2048, 64, testAlloc, testFree };
}

static bool testCacheProvider( void )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-script-provider-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	SIR_TEST_CHECK( std::filesystem::create_directory( root ) );
	SIRScriptArtifactCache cache;
	SIR_TEST_CHECK( cache.configure( root.string().c_str(), 2048 ) == SIR_SCRIPT_OK );
	SIRScriptBindingRegistry registry;
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	alignas( 64 ) uint8_t szFirst[512] = {};
	alignas( 64 ) uint8_t szSecond[512] = {};
	for ( uint32_t i = 0; i < sizeof( szFirst ); ++i ) {
		szFirst[i] = ( uint8_t )( i * 3 + 7 );
		szSecond[i] = ( uint8_t )( i * 5 + 9 );
	}
	const sirScriptCodeArtifact_t firstArtifact = { 1, 50, 1, 77, registry.schemaFingerprint(), 88, 101 };
	const sirScriptCodeArtifact_t secondArtifact = { 1, 50, 2, 77, registry.schemaFingerprint(), 88, 102 };
	sirScriptCacheKey_t firstKey = {};
	sirScriptCacheKey_t secondKey = {};
	SIR_TEST_CHECK( SIRScriptArtifactCache::makeKey( "first", 5, &firstKey ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( SIRScriptArtifactCache::makeKey( "second", 6, &secondKey ) == SIR_SCRIPT_OK );
	sirScriptCachedArtifact_t first = {};
	sirScriptCachedArtifact_t second = {};
	SIR_TEST_CHECK( cache.store( firstArtifact, firstKey, szFirst, sizeof( szFirst ), 64, &first ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.store( secondArtifact, secondKey, szSecond, sizeof( szSecond ), 64, &second ) == SIR_SCRIPT_OK );

	sirCachedSourceTestAllocator_t memory = {};
	SIRScriptCachedProgramSource source;
	SIR_TEST_CHECK( source.configure( nullptr, testAllocator( &memory ), 1024 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( source.configure( &cache, testAllocator( &memory ), 1024 ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.configure( &cache, testAllocator( &memory ), 1024 ) == SIR_SCRIPT_WRONG_STATE );
	sirScriptCachedEntryHandle_t firstHandle = {};
	sirScriptCachedEntryHandle_t secondHandle = {};
	SIR_TEST_CHECK( source.registerArtifact( first, &firstHandle ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.registerArtifact( first, &firstHandle ) == SIR_SCRIPT_DUPLICATE );
	sirScriptCachedArtifact_t conflict = first;
	conflict.artifact.nBuildFingerprint = 999;
	SIR_TEST_CHECK( source.registerArtifact( conflict, &firstHandle ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( source.registerArtifact( second, &secondHandle ) == SIR_SCRIPT_OK );
	const sirScriptProgramSourceAPI_t provider = source.makeProgramSourceAPI();
	SIR_TEST_CHECK( provider.nABIRevision == SIR_SCRIPT_PROGRAM_SOURCE_ABI_REVISION );
	uint64_t nFirstLease = 0;
	uint64_t nSecondLease = 0;
	sirScriptProgramView_t view = {};
	SIR_TEST_CHECK( provider.pAcquire( provider.pUser, 1, 50, 1, &view, &nFirstLease ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nFirstLease != 0 && view.nAlignment == 64 && view.nBytes == 512 );
	SIR_TEST_CHECK( memcmp( view.pData, szFirst, sizeof( szFirst ) ) == 0 );
	SIR_TEST_CHECK( memory.nLive == 1 && source.stats().nResidentBytes == 512 );
	SIR_TEST_CHECK( provider.pAcquire( provider.pUser, 1, 50, 1, &view, &nSecondLease ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memory.nLive == 1 && source.stats().nActiveLeases == 2 );
	SIR_TEST_CHECK( source.unregisterArtifact( firstHandle ) == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( source.shutdown() == SIR_SCRIPT_BUSY );
	provider.pRelease( provider.pUser, nFirstLease );
	provider.pRelease( provider.pUser, nFirstLease );
	SIR_TEST_CHECK( memory.nLive == 1 && source.stats().nActiveLeases == 1 );
	provider.pRelease( provider.pUser, nSecondLease );
	SIR_TEST_CHECK( memory.nLive == 0 && source.stats().nResidentBytes == 0 );
	SIR_TEST_CHECK( source.unregisterArtifact( firstHandle ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.unregisterArtifact( firstHandle ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( source.registerArtifact( first, &firstHandle ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( provider.pAcquire( provider.pUser, 1, 50, 1, &view, &nSecondLease ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nSecondLease != nFirstLease );
	provider.pRelease( provider.pUser, nFirstLease );
	SIR_TEST_CHECK( source.stats().nActiveLeases == 1 );
	provider.pRelease( provider.pUser, nSecondLease );

	// A failed disk read must release its temporary allocation immediately.
	SIR_TEST_CHECK( cache.erase( firstKey ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( provider.pAcquire( provider.pUser, 1, 50, 1, &view, &nSecondLease ) == SIR_SCRIPT_NOT_FOUND );
	SIR_TEST_CHECK( memory.nLive == 0 && memory.nAllocations == memory.nReleases );
	SIR_TEST_CHECK( cache.store( firstArtifact, firstKey, szFirst, sizeof( szFirst ), 64, &first ) == SIR_SCRIPT_OK );
	memory.bFail = true;
	SIR_TEST_CHECK( provider.pAcquire( provider.pUser, 1, 50, 1, &view, &nSecondLease ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	memory.bFail = false;
	SIR_TEST_CHECK( source.stats().nActiveLeases == 0 );

	SIRScriptCodeManager manager( &registry );
	SIR_TEST_CHECK( manager.configure( 88, provider ) == SIR_SCRIPT_OK );
	sirScriptCodeHandle_t codeA = {};
	sirScriptCodeHandle_t codeB = {};
	SIR_TEST_CHECK( manager.stage( firstArtifact, &codeA ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.activate( codeA ) == SIR_SCRIPT_OK );
	const sirScriptProgramSourceAPI_t runtimeSource = manager.makeProgramSourceAPI();
	uint64_t nRuntimeLease = 0;
	SIR_TEST_CHECK( runtimeSource.pAcquire( runtimeSource.pUser, 1, 50, 1, &view, &nRuntimeLease ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memcmp( view.pData, szFirst, sizeof( szFirst ) ) == 0 );
	SIR_TEST_CHECK( manager.stage( secondArtifact, &codeB ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.stats().nResidentBytes == 1024 && source.stats().nResidentArtifacts == 2 );
	SIR_TEST_CHECK( manager.activate( codeB ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.stats().nResidentBytes == 1024 );
	SIR_TEST_CHECK( source.unregisterArtifact( firstHandle ) == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( runtimeSource.pAcquire( runtimeSource.pUser, 1, 50, 2, &view, &nSecondLease ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memcmp( view.pData, szSecond, sizeof( szSecond ) ) == 0 );
	runtimeSource.pRelease( runtimeSource.pUser, nSecondLease );
	runtimeSource.pRelease( runtimeSource.pUser, nRuntimeLease );
	SIR_TEST_CHECK( source.stats().nResidentBytes == 512 );
	SIR_TEST_CHECK( manager.retire( codeB ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.stats().nResidentBytes == 0 && memory.nLive == 0 );

	// Even a valid program cannot exceed the source's independent lease capacity.
	uint64_t szLeases[128] = {};
	for ( uint32_t i = 0; i < 128; ++i ) {
		SIR_TEST_CHECK( provider.pAcquire( provider.pUser, 1, 50, 1, &view, &szLeases[i] ) == SIR_SCRIPT_OK );
	}
	uint64_t nExcess = 0;
	SIR_TEST_CHECK( provider.pAcquire( provider.pUser, 1, 50, 1, &view, &nExcess ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( source.stats().nActiveLeases == 128 && memory.nLive == 1 );
	for ( uint32_t i = 0; i < 128; ++i ) {
		provider.pRelease( provider.pUser, szLeases[i] );
	}
	SIR_TEST_CHECK( memory.nLive == 0 && source.stats().nActiveLeases == 0 );

	sirScriptResult_t eWrongThread = SIR_SCRIPT_OK;
	std::thread worker( [&]() {
		sirScriptProgramView_t workerView = {};
		uint64_t nToken = 0;
		eWrongThread = provider.pAcquire( provider.pUser, 1, 50, 1, &workerView, &nToken );
	} );
	worker.join();
	SIR_TEST_CHECK( eWrongThread == SIR_SCRIPT_BUSY );
	// Per-provider residency pressure must reject the second pin, then recover.
	SIRScriptCachedProgramSource limited;
	SIR_TEST_CHECK( limited.configure( &cache, testAllocator( &memory ), 512 ) == SIR_SCRIPT_OK );
	sirScriptCachedEntryHandle_t limitedFirst = {};
	sirScriptCachedEntryHandle_t limitedSecond = {};
	SIR_TEST_CHECK( limited.registerArtifact( first, &limitedFirst ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( limited.registerArtifact( second, &limitedSecond ) == SIR_SCRIPT_OK );
	const sirScriptProgramSourceAPI_t limitedAPI = limited.makeProgramSourceAPI();
	uint64_t nLimitedFirst = 0;
	uint64_t nLimitedSecond = 0;
	SIR_TEST_CHECK( limitedAPI.pAcquire( limitedAPI.pUser, 1, 50, 1, &view, &nLimitedFirst ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( limitedAPI.pAcquire( limitedAPI.pUser, 1, 50, 2, &view, &nLimitedSecond ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( limited.stats().nResidentBytes == 512 );
	limitedAPI.pRelease( limitedAPI.pUser, nLimitedFirst );
	SIR_TEST_CHECK( limitedAPI.pAcquire( limitedAPI.pUser, 1, 50, 2, &view, &nLimitedSecond ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memcmp( view.pData, szSecond, sizeof( szSecond ) ) == 0 );
	limitedAPI.pRelease( limitedAPI.pUser, nLimitedSecond );
	SIR_TEST_CHECK( limited.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( limited.unregisterArtifact( limitedFirst ) == SIR_SCRIPT_INVALID_HANDLE );

	SIR_TEST_CHECK( source.unregisterArtifact( firstHandle ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.unregisterArtifact( secondHandle ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memory.nAllocations == memory.nReleases );
	SIR_TEST_CHECK( std::filesystem::remove_all( root ) == 3 );
	return true;
}

int main( void )
{
	if ( !testCacheProvider() ) {
		return 1;
	}
	printf( "SIRScriptCachedProgramSource tests passed\n" );
	return 0;
}
