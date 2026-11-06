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

#include <Resources/SIRResourceGraph.h>

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

#ifdef _WIN32
#include <malloc.h>
#endif

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

typedef struct sirDerivedTestAllocator_s
{
	std::atomic<uint32_t> nLive;
	std::atomic<bool> bFailNext;
	std::atomic<uint32_t> nAllocationCalls;
	std::atomic<uint32_t> nFailOrdinal;
} sirDerivedTestAllocator_t;

static void *testAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nDomain;
	sirDerivedTestAllocator_t *pTest = ( sirDerivedTestAllocator_t * )pContext;
	const uint32_t nOrdinal = pTest->nAllocationCalls.fetch_add( 1 ) + 1;
	if ( pTest->bFailNext.exchange( false ) || ( pTest->nFailOrdinal.load() != 0 && pTest->nFailOrdinal.load() == nOrdinal ) ||
	     nBytes == 0 || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pMemory = nullptr;
#ifdef _WIN32
	pMemory = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	size_t nActualAlignment = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pMemory, nActualAlignment, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pMemory != nullptr ) {
		pTest->nLive.fetch_add( 1 );
	}
	return pMemory;
}

static void testFree( void *pContext, void *pMemory, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nBytes;
	( void )nAlignment;
	( void )nDomain;
	sirDerivedTestAllocator_t *pTest = ( sirDerivedTestAllocator_t * )pContext;
	pTest->nLive.fetch_sub( 1 );
#ifdef _WIN32
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

typedef struct sirDerivedTestProvider_s
{
	uint32_t nAttempts;
	uint32_t nReleased;
	uint64_t nLastToken;
	bool bRejectRelease;
} sirDerivedTestProvider_t;

static bool releaseNative( void *pContext, uint64_t nToken ) noexcept
{
	sirDerivedTestProvider_t *pProvider = ( sirDerivedTestProvider_t * )pContext;
	++pProvider->nAttempts;
	if ( pProvider->bRejectRelease ) {
		return false;
	}
	++pProvider->nReleased;
	pProvider->nLastToken = nToken;
	return true;
}

static SIRResourceGraphConfig graphConfig( sirDerivedTestAllocator_t& rAllocator ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &rAllocator, testAllocate, testFree };
	config.m_nMetadataDomain = 2;
	config.m_nMaxResources = 8;
	config.m_nMaxStaging = 8;
	config.m_nMaxProviderPins = 8;
	config.m_nMaxCommittedBytes = 1024 * 1024;
	return config;
}

static SIRResourceDerivedConfig derivedConfig( sirDerivedTestAllocator_t& rAllocator ) noexcept
{
	SIRResourceDerivedConfig config = {};
	config.allocator = { &rAllocator, testAllocate, testFree };
	config.nMetadataDomain = 2;
	config.nMaxRepresentations = 4;
	config.nMaxProviders = 2;
	config.nMaxTrackedBytes = 300;
	return config;
}

static SIRResourceHandle publishSource( SIRResourceGraphWriter& rWriter, uint64_t nAssetId ) noexcept
{
	SIRResourcePublication publication = {};
	publication.m_Key = { nAssetId, 17, 0 };
	publication.m_eKind = SIR_RESOURCE_KIND_BLOB;
	publication.m_nSchemaVersion = 1;
	publication.m_nFormatId = 1;
	SIRResourceHandle handle = {};
	REQUIRE( rWriter.publish( publication, &handle ) == SIR_RESOURCE_RESULT_SUCCESS );
	return handle;
}

static sirProviderCompletionSet_t derivedCompletion( uint32_t nProvider, uint64_t nFence ) noexcept
{
	return { nProvider, 1, 1, 1, { nFence, 0, 0, 0 } };
}

static SIRDerivedResourceRegistration derived( SIRResourceHandle hSource, uint64_t nRevision, uint32_t nProvider, uint32_t nRepresentation,
                                               uint64_t nToken, uint64_t nCommitted ) noexcept
{
	SIRDerivedResourceRegistration registration = {};
	registration.m_Key = { hSource, nRevision, nProvider, nRepresentation, 0, 0 };
	registration.m_Accounting = { SIR_MEMORY_SPACE_DEVICE, 5, nCommitted, nCommitted - 8, nRevision };
	registration.m_nNativeToken = nToken;
	return registration;
}

static void testProviderLifetimeAndPhysicalAccounting( void )
{
	sirDerivedTestAllocator_t allocator = {};
	{
		SIRResourceGraph graph( graphConfig( allocator ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		const SIRResourceHandle hSource = publishSource( *pWriter, 44 );
		SIRResourceDerivedRegistry registry( graph, derivedConfig( allocator ) );
		REQUIRE( registry.isReady() );
		sirDerivedTestProvider_t gpu = {};
		sirDerivedTestProvider_t audio = {};
		REQUIRE( registry.registerProvider( 7, { &gpu, releaseNative } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( registry.registerProvider( 9, { &audio, releaseNative } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( registry.registerProvider( 7, { &gpu, releaseNative } ) == SIR_RESOURCE_RESULT_BUSY );
		REQUIRE( registry.registerProvider( 11, { &audio, releaseNative } ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );

		SIRDerivedResourceHandle hA = {};
		SIRDerivedResourceHandle hB = {};
		const SIRDerivedResourceRegistration a = derived( hSource, 1, 7, 1, 100, 128 );
		const SIRDerivedResourceRegistration b = derived( hSource, 1, 9, 1, 100, 128 );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, hSource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( registry.registerRepresentation( scope, view, a, &hA ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( registry.registerRepresentation( scope, view, a, &hB ) == SIR_RESOURCE_RESULT_BUSY );
			SIRDerivedResourceRegistration duplicateToken = derived( hSource, 1, 7, 2, 100, 128 );
			REQUIRE( registry.registerRepresentation( scope, view, duplicateToken, &hB ) == SIR_RESOURCE_RESULT_BUSY );
			SIRDerivedResourceRegistration wrongRevision = derived( hSource, 2, 7, 3, 300, 128 );
			REQUIRE( registry.registerRepresentation( scope, view, wrongRevision, &hB ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			REQUIRE( registry.registerRepresentation( scope, view, b, &hB ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRDerivedResourceRegistration tooLarge = derived( hSource, 1, 9, 2, 301, 64 );
			REQUIRE( registry.registerRepresentation( scope, view, tooLarge, &hB ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
			// Registration is tied to the captured source revision; publishing
			// a replacement leaves this specific provider version pinned.
			REQUIRE( publishSource( *pWriter, 44 ).m_nGeneration == hSource.m_nGeneration );
			REQUIRE( view.getRevision() == 1 );
		}
		SIRResourceDerivedStats stats = {};
		REQUIRE( registry.getStats( &stats ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( stats.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 256 );
		REQUIRE( stats.nResidentBytes[SIR_MEMORY_SPACE_DEVICE] == 240 );
		REQUIRE( stats.nLiveRepresentations == 2 && stats.nRegisteredProviders == 2 );
		REQUIRE( registry.retireRepresentation( hA, derivedCompletion( 7, 10 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( registry.retireRepresentation( hA, derivedCompletion( 7, 10 ) ) == SIR_RESOURCE_RESULT_BUSY );
		REQUIRE( registry.retireRepresentation( hB, derivedCompletion( 9, 5 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->retire( hSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		// Both old and new canonical revisions remain retired but pinned.
		REQUIRE( pWriter->collectRetired( 8 ) == 1 );
		REQUIRE( registry.updateProviderCompletion( derivedCompletion( 7, 9 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( gpu.nAttempts == 0 );
		REQUIRE( registry.updateProviderCompletion( derivedCompletion( 9, 5 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( audio.nReleased == 1 && audio.nLastToken == 100 );
		SIRDerivedResourceAccounting accounting = {};
		REQUIRE( registry.getAccounting( hB, &accounting ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( registry.getStats( &stats ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( stats.nPendingRetirements == 1 && stats.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 128 );
		REQUIRE( registry.updateProviderCompletion( derivedCompletion( 7, 10 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( gpu.nReleased == 1 && gpu.nAttempts == 1 );
		REQUIRE( registry.updateProviderCompletion( derivedCompletion( 7, 9 ) ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.getStats( &stats ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( stats.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 0 );
		REQUIRE( stats.nResidentBytes[SIR_MEMORY_SPACE_DEVICE] == 0 );
		REQUIRE( stats.nPeakCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 256 );
		REQUIRE( stats.nLiveRepresentations == 0 && stats.nPendingRetirements == 0 );
		REQUIRE( pWriter->collectRetired( 8 ) == 1 );
		REQUIRE( pWriter->collectRetired( 8 ) == 0 );
	}
	REQUIRE( allocator.nLive.load() == 0 );
}

static void testRetryStaleGenerationAndWriterBoundary( void )
{
	sirDerivedTestAllocator_t allocator = {};
	{
		SIRResourceGraph graph( graphConfig( allocator ) );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		const SIRResourceHandle hSource = publishSource( *pWriter, 11 );
		SIRResourceDerivedRegistry registry( graph, derivedConfig( allocator ) );
		sirDerivedTestProvider_t provider = {};
		provider.bRejectRelease = true;
		REQUIRE( registry.registerProvider( 7, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_SUCCESS );

		std::atomic<uint32_t> result{ 0 };
		std::thread unauthorized( [&registry, &result]() { result.store( ( uint32_t )registry.updateProviderCompletion( derivedCompletion( 7, 10 ) ) ); } );
		unauthorized.join();
		REQUIRE( result.load() == SIR_RESOURCE_RESULT_WRITER_VIOLATION );

		SIRDerivedResourceHandle oldHandle = {};
		SIRDerivedResourceHandle newHandle = {};
		SIRDerivedResourceRegistration registration = derived( hSource, 1, 7, 1, 1001, 96 );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, hSource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( registry.registerRepresentation( scope, view, registration, &oldHandle ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		REQUIRE( registry.retireRepresentation( oldHandle, derivedCompletion( 7, 20 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( registry.updateProviderCompletion( derivedCompletion( 7, 20 ) ) == SIR_RESOURCE_RESULT_BUSY );
		REQUIRE( provider.nAttempts == 1 && provider.nReleased == 0 );
		SIRDerivedResourceAccounting accounting = {};
		REQUIRE( registry.getAccounting( oldHandle, &accounting ) == SIR_RESOURCE_RESULT_SUCCESS );
		provider.bRejectRelease = false;
		REQUIRE( registry.updateProviderCompletion( derivedCompletion( 7, 20 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( provider.nAttempts == 2 && provider.nReleased == 1 );
		REQUIRE( registry.getAccounting( oldHandle, &accounting ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );

		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, hSource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( registry.registerRepresentation( scope, view, registration, &newHandle ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		REQUIRE( oldHandle.m_nSlot == newHandle.m_nSlot && oldHandle.m_nGeneration != newHandle.m_nGeneration );
		REQUIRE( registry.retireRepresentation( oldHandle, derivedCompletion( 7, 0 ) ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( registry.retireRepresentation( newHandle, derivedCompletion( 7, 20 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( provider.nReleased == 2 );
		REQUIRE( pWriter->retire( hSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 8 ) == 1 );
	}
	REQUIRE( allocator.nLive.load() == 0 );
}

static void testConstructorOutOfMemory( void )
{
	sirDerivedTestAllocator_t allocator = {};
	{
		SIRResourceGraph graph( graphConfig( allocator ) );
		REQUIRE( graph.isReady() );
		const uint32_t nBefore = allocator.nLive.load();
		allocator.bFailNext.store( true );
		{
			SIRResourceDerivedRegistry failed( graph, derivedConfig( allocator ) );
			REQUIRE( !failed.isReady() );
		}
		REQUIRE( allocator.nLive.load() == nBefore );
		allocator.nAllocationCalls.store( 0 );
		allocator.nFailOrdinal.store( 2 );
		{
			SIRResourceDerivedRegistry failed( graph, derivedConfig( allocator ) );
			REQUIRE( !failed.isReady() );
		}
		allocator.nFailOrdinal.store( 0 );
		REQUIRE( allocator.nAllocationCalls.load() == 2 && allocator.nLive.load() == nBefore );
	}
	REQUIRE( allocator.nLive.load() == 0 );
}

static void testInvalidInputsAndCapacity( void )
{
	sirDerivedTestAllocator_t allocator = {};
	{
		SIRResourceGraph graph( graphConfig( allocator ) );
		REQUIRE( graph.isReady() );
		SIRResourceDerivedConfig badConfig = derivedConfig( allocator );
		badConfig.nMaxProviders = 0;
		{
			SIRResourceDerivedRegistry invalid( graph, badConfig );
			REQUIRE( !invalid.isReady() );
			REQUIRE( invalid.registerProvider( 7, { nullptr, releaseNative } ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			REQUIRE( invalid.updateProviderCompletion( derivedCompletion( 7, 0 ) ) == SIR_RESOURCE_RESULT_FAILED );
			SIRResourceDerivedStats invalidStats = {};
			REQUIRE( invalid.getStats( &invalidStats ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		}
		badConfig = derivedConfig( allocator );
		badConfig.nMaxRepresentations = 0;
		{
			const uint32_t nBefore = allocator.nLive.load();
			SIRResourceDerivedRegistry invalid( graph, badConfig );
			REQUIRE( !invalid.isReady() && allocator.nLive.load() == nBefore );
			SIRDerivedResourceAccounting ignored = {};
			SIRDerivedResourceHandle handle = {};
			REQUIRE( invalid.retireRepresentation( handle, derivedCompletion( 7, 0 ) ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			REQUIRE( invalid.getAccounting( handle, &ignored ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			REQUIRE( invalid.registerProvider( 7, { nullptr, releaseNative } ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		}
		badConfig = derivedConfig( allocator );
		badConfig.nMaxTrackedBytes = 0;
		{
			SIRResourceDerivedRegistry invalid( graph, badConfig );
			REQUIRE( !invalid.isReady() );
		}
		badConfig = derivedConfig( allocator );
		badConfig.allocator.m_pfnAllocate = nullptr;
		{
			SIRResourceDerivedRegistry invalid( graph, badConfig );
			REQUIRE( !invalid.isReady() );
		}
		badConfig = derivedConfig( allocator );
		badConfig.allocator.m_pfnFree = nullptr;
		{
			SIRResourceDerivedRegistry invalid( graph, badConfig );
			REQUIRE( !invalid.isReady() );
		}
		const SIRResourceHandle hSource = publishSource( *graph.getWriter(), 400 );
		SIRResourceDerivedRegistry registry( graph, derivedConfig( allocator ) );
		REQUIRE( registry.isReady() );
		sirDerivedTestProvider_t provider = {};
		REQUIRE( registry.registerProvider( 0, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.registerProvider( 7, { &provider, nullptr } ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.registerProvider( 7, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( registry.registerProvider( 7, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_BUSY );
		REQUIRE( registry.registerProvider( 8, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( registry.registerProvider( 9, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		REQUIRE( registry.updateProviderCompletion( derivedCompletion( 9, 0 ) ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.getStats( nullptr ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		SIRDerivedResourceAccounting accounting = {};
		REQUIRE( registry.getAccounting( { UINT32_MAX, 0 }, &accounting ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.getAccounting( { 0, 1 }, nullptr ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.getAccounting( { 0, 1 }, &accounting ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( registry.retireRepresentation( { UINT32_MAX, 0 }, derivedCompletion( 7, 0 ) ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( registry.retireRepresentation( { 0, 1 }, derivedCompletion( 7, 0 ) ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		SIRDerivedResourceHandle live[4] = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, hSource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			const SIRDerivedResourceRegistration base = derived( hSource, 1, 7, 1, 500, 32 );
			SIRDerivedResourceRegistration changed = base;
			REQUIRE( registry.registerRepresentation( scope, view, base, nullptr ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed.m_nNativeToken = 0;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Key.m_nRevision = 0;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Key.m_nRepresentationId = 0;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Key.m_nReserved = 1;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Key.m_Resource.m_nReserved = 1;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Accounting.m_eSpace = ( decltype( changed.m_Accounting.m_eSpace ) )255;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Accounting.m_nCommittedBytes = 0;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Accounting.m_nResidentBytes = 33;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Accounting.m_nSourceRevision = 0;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Key.m_nProviderId = 55;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			changed = base;
			changed.m_Key.m_Resource.m_nGeneration++;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			changed = base;
			changed.m_Key.m_Resource.m_nSlot = UINT32_MAX;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			changed = base;
			changed.m_Key.m_Resource.m_nTypeId++;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			changed = base;
			changed.m_Key.m_nRevision = 2;
			changed.m_Accounting.m_nSourceRevision = 2;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			changed = base;
			changed.m_Accounting.m_nCommittedBytes = 301;
			changed.m_Accounting.m_nResidentBytes = 293;
			REQUIRE( registry.registerRepresentation( scope, view, changed, &live[0] ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
			SIRResourceView invalidView;
			REQUIRE( registry.registerRepresentation( scope, invalidView, base, &live[0] ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			std::thread unauthorized( [&]() {
				SIRDerivedResourceHandle ignored = {};
				SIRDerivedResourceAccounting ignoredAccounting = {};
				SIRResourceDerivedStats ignoredStats = {};
				REQUIRE( registry.registerProvider( 8, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
				REQUIRE( registry.registerRepresentation( scope, view, base, &ignored ) == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
				REQUIRE( registry.retireRepresentation( { 0, 1 }, derivedCompletion( 7, 0 ) ) == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
				REQUIRE( registry.getAccounting( { 0, 1 }, &ignoredAccounting ) == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
				REQUIRE( registry.getStats( &ignoredStats ) == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
			} );
			unauthorized.join();
			SIRDerivedResourceRegistration variantA = base;
			variantA.m_Key.m_nVariantId = 1;
			variantA.m_nNativeToken = 900;
			REQUIRE( registry.registerRepresentation( scope, view, variantA, &live[0] ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRDerivedResourceRegistration variantB = variantA;
			variantB.m_Key.m_nVariantId = 2;
			variantB.m_nNativeToken = 901;
			REQUIRE( registry.registerRepresentation( scope, view, variantB, &live[1] ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( registry.retireRepresentation( live[0], derivedCompletion( 7, 0 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( registry.retireRepresentation( live[1], derivedCompletion( 7, 0 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( registry.updateProviderCompletion( derivedCompletion( 7, 0 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( provider.nReleased == 2 );
			for ( uint32_t i = 0; i < 4; ++i ) {
				const SIRDerivedResourceRegistration registration = derived( hSource, 1, 7, i + 1, 500 + i, 32 );
				REQUIRE( registry.registerRepresentation( scope, view, registration, &live[i] ) == SIR_RESOURCE_RESULT_SUCCESS );
			}
			changed = derived( hSource, 1, 7, 5, 505, 32 );
			SIRDerivedResourceHandle ignored = {};
			REQUIRE( registry.registerRepresentation( scope, view, changed, &ignored ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
			REQUIRE( registry.getAccounting( { live[0].m_nSlot, live[0].m_nGeneration + 1 }, &accounting ) ==
			         SIR_RESOURCE_RESULT_INVALID_HANDLE );
		}
		for ( uint32_t i = 0; i < 4; ++i ) {
			REQUIRE( registry.retireRepresentation( live[i], derivedCompletion( 7, 1 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		REQUIRE( registry.updateProviderCompletion( derivedCompletion( 7, 1 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( provider.nReleased == 6 );
	}
	REQUIRE( allocator.nLive.load() == 0 );
}

static void testProviderPinCapacityAndReclaim( void )
{
	sirDerivedTestAllocator_t allocator = {};
	{
		SIRResourceGraphConfig config = graphConfig( allocator );
		config.m_nMaxProviderPins = 1;
		SIRResourceGraph graph( config );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		const SIRResourceHandle hSource = publishSource( *pWriter, 703 );
		SIRResourceDerivedRegistry registry( graph, derivedConfig( allocator ) );
		REQUIRE( registry.isReady() );
		sirDerivedTestProvider_t provider = {};
		REQUIRE( registry.registerProvider( 7, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRDerivedResourceHandle first = {};
		SIRDerivedResourceHandle second = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, hSource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			const SIRDerivedResourceRegistration a = derived( hSource, 1, 7, 1, 900, 32 );
			const SIRDerivedResourceRegistration b = derived( hSource, 1, 7, 2, 901, 32 );
			REQUIRE( registry.registerRepresentation( scope, view, a, &first ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( registry.registerRepresentation( scope, view, b, &second ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
			REQUIRE( registry.retireRepresentation( first, derivedCompletion( 7, 1 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( registry.updateProviderCompletion( derivedCompletion( 7, 1 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( provider.nReleased == 1 );
			REQUIRE( registry.registerRepresentation( scope, view, b, &second ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		REQUIRE( registry.retireRepresentation( second, derivedCompletion( 7, 2 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( registry.updateProviderCompletion( derivedCompletion( 7, 2 ) ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( provider.nReleased == 2 );
	}
	REQUIRE( allocator.nLive.load() == 0 );
}

static void testProviderMultilaneGenerationRetirement( void )
{
	sirDerivedTestAllocator_t allocator = {};
	{
		SIRResourceGraph graph( graphConfig( allocator ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		const SIRResourceHandle hSource = publishSource( *pWriter, 801 );
		SIRResourceDerivedRegistry registry( graph, derivedConfig( allocator ) );
		sirDerivedTestProvider_t provider = {};
		REQUIRE( registry.registerProvider( 7, 3, 2, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( registry.registerProvider( 8, 0, 2, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.registerProvider( 8, 1, 5, { &provider, releaseNative } ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		SIRDerivedResourceHandle handle = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, hSource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			const SIRDerivedResourceRegistration registration = derived( hSource, 1, 7, 1, 777, 64 );
			REQUIRE( registry.registerRepresentation( scope, view, registration, &handle ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		const sirProviderCompletionSet_t required = { 7, 3, 3, 2, { 7, 11, 0, 0 } };
		REQUIRE( registry.retireRepresentation( handle, { 7, 2, 3, 2, { 7, 11, 0, 0 } } ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.retireRepresentation( handle, required ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( registry.updateProviderCompletion( { 7, 2, 3, 2, { 7, 11, 0, 0 } } ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.updateProviderCompletion( { 7, 3, 1, 2, { 7, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( provider.nAttempts == 0 );
		REQUIRE( registry.updateProviderCompletion( { 7, 3, 2, 2, { 0, 10, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( provider.nAttempts == 0 );
		REQUIRE( registry.updateProviderCompletion( { 7, 3, 1, 2, { 6, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( registry.updateProviderCompletion( { 7, 3, 2, 2, { 0, 11, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( provider.nReleased == 1 && provider.nLastToken == 777 );
		SIRDerivedResourceAccounting accounting = {};
		REQUIRE( registry.getAccounting( handle, &accounting ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( pWriter->retire( hSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 8 ) == 1 );
	}
	REQUIRE( allocator.nLive.load() == 0 );
}

int main( void )
{
	testProviderLifetimeAndPhysicalAccounting();
	testRetryStaleGenerationAndWriterBoundary();
	testConstructorOutOfMemory();
	testInvalidInputsAndCapacity();
	testProviderPinCapacityAndReclaim();
	testProviderMultilaneGenerationRetirement();
	puts( "PASS: derived provider accounting, retirement fences, pins and ABA protection" );
	return 0;
}
