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

#include <Audio/SIRAudioResources.h>
#include <Resources/SIRResourceGraph.h>
#include <Audio/SIRAudioSystem.h>
#include "SIRAudioMockBackend.h"
#include <cmath>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void testCheckAt( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "Audio resource test failure at line %u\n", nLine );
		abort();
	}
}

#define testCheck( condition ) testCheckAt( ( condition ), __LINE__ )
#define CHECK( condition ) testCheckAt( ( condition ), __LINE__ )

typedef struct sirAudioTestAllocationContext_s
{
	uint32_t nAllocations;
	uint32_t nFrees;
	bool bFailNext;
} sirAudioTestAllocationContext_t;

static void *testAllocate( void *pContext, size_t nBytes, size_t ) noexcept
{
	sirAudioTestAllocationContext_t *pState = ( sirAudioTestAllocationContext_t * )pContext;
	if ( pState->bFailNext ) {
		pState->bFailNext = false;
		return nullptr;
	}
	void *pMemory = malloc( nBytes );
	if ( pMemory != nullptr ) {
		++pState->nAllocations;
	}
	return pMemory;
}

static void testDeallocate( void *pContext, void *pMemory, size_t, size_t ) noexcept
{
	sirAudioTestAllocationContext_t *pState = ( sirAudioTestAllocationContext_t * )pContext;
	if ( pMemory != nullptr ) {
		++pState->nFrees;
		free( pMemory );
	}
}

typedef struct sirAudioTestManifest_s
{
	sirAudioManifestHeader_t header;
	sirAudioPackageRecord_t packages[2];
	sirAudioEventRecord_t events[2];
	sirAudioControlRecord_t controls[2];
	uint64_t dependencies[1];
} sirAudioTestManifest_t;

static_assert( sizeof( sirAudioTestManifest_t ) == 64 + 64 + 64 + 80 + 8, "Test manifest has padding" );

static void refreshHash( sirAudioTestManifest_t& manifest ) noexcept
{
	manifest.header.nContentHash =
	    SIRAudioResourceStore::hashPayload( ( ( const uint8_t * )&manifest ) + sizeof( manifest.header.nContentHash ),
		                                    sizeof( manifest ) - sizeof( manifest.header.nContentHash ) );
}

static sirAudioTestManifest_t makeManifest( void ) noexcept
{
	sirAudioTestManifest_t manifest = {};
	manifest.header.nMagic = SIR_AUDIO_MANIFEST_MAGIC;
	manifest.header.nVersion = SIR_AUDIO_MANIFEST_VERSION;
	manifest.header.nHeaderBytes = sizeof( sirAudioManifestHeader_t );
	manifest.header.nTotalBytes = sizeof( manifest );
	manifest.header.nEndianness = SIR_AUDIO_MANIFEST_LITTLE_ENDIAN;
	manifest.header.eProvider = SIR_AUDIO_PROVIDER_FMOD;
	manifest.header.nPackageCount = 2;
	manifest.header.nEventCount = 2;
	manifest.header.nControlCount = 2;
	manifest.header.nDependencyCount = 1;
	manifest.header.nBuildFingerprint = 1234567;
	manifest.header.nRequiredCapabilities = 0x01;
	manifest.packages[0] = { 10, 1001, 0x01, 0, 0 };
	manifest.packages[1] = { 20, 1002, 0, 0, 1 };
	manifest.events[0] = { 100, 10, 2001, 0x02 };
	manifest.events[1] = { 200, 20, 2002, 0 };
	manifest.controls[0] = { 300, 3001, 0, 0.5f, 0.0f, 1.0f, SIR_AUDIO_CONTROL_GLOBAL, SIR_AUDIO_CONTROL_FLOAT, SIR_AUDIO_BINDING_NATIVE,
	                         0 };
	manifest.controls[1] = { 400, 0, 0, 2.0f, 0.0f, 4.0f, SIR_AUDIO_CONTROL_EMITTER, SIR_AUDIO_CONTROL_DISCRETE, SIR_AUDIO_BINDING_EMULATED,
	                         0 };
	manifest.dependencies[0] = 10;
	refreshHash( manifest );
	return manifest;
}

static void *audioSourceAllocate( void *, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	void *pData = nullptr;
#ifdef _WIN32
	pData = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nAlign = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pData, nAlign, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	return pData;
}

static void audioSourceFree( void *, void *pData, uint64_t, uint32_t, uint32_t ) noexcept
{
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static SIRResourceGraphConfig audioSourceConfig( void ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { nullptr, audioSourceAllocate, audioSourceFree };
	config.m_nMetadataDomain = 1;
	config.m_nMaxResources = 8;
	config.m_nMaxStaging = 16;
	config.m_nMaxProviderPins = 8;
	config.m_nMaxCommittedBytes = 1024 * 1024;
	config.nMaxReaders = 16;
	return config;
}

struct sirAudioTestResourceGraph_t
{
	SIRResourceGraph graph;
	sirAudioTestResourceGraph_t( void ) noexcept : graph( audioSourceConfig() )
	{
		testCheck( graph.isReady() );
	}
};

struct sirAudioTestLoad_t
{
	const void *pBytes;
	size_t nBytes;
	sirAudioProvider_t eProvider;
	uint64_t nCapabilities;
	const sirContainerAllocator_t *pAllocator;
};

static sirAudioManifestResult_t loadTest( SIRAudioResourceStore& rStore, SIRResourceGraph& rGraph,
                                          const sirAudioTestLoad_t& rLoad ) noexcept
{
	SIRResourceGraphWriter *pWriter = rGraph.getWriter();
	testCheck( pWriter != nullptr );
	SIRResourceStagingHandle hStaging = {};
	SIRResourceMutableBytes bytes = {};
	testCheck( pWriter->allocateStaging( rLoad.nBytes, 16, 1, &hStaging, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( bytes.m_pData, rLoad.pBytes, rLoad.nBytes );
	const uint8_t *pData = ( const uint8_t * )rLoad.pBytes;
	uint64_t nDigest = UINT64_C( 0xcbf29ce484222325 );
	for ( size_t i = 0; i < rLoad.nBytes; ++i ) {
		nDigest = ( nDigest ^ pData[i] ) * UINT64_C( 0x100000001b3 );
	}
	const SIRResourceChunkPublication chunk = { { 1 }, { nDigest, nDigest ^ UINT64_C( 0xa43c5d9f ), 1, 0 }, hStaging, false };
	const SIRResourcePublication publication = { { 750, 11, 0 }, SIR_RESOURCE_KIND_BLOB, 1, 1, &chunk, 1, nullptr, 0 };
	SIRResourceHandle hManifest = {};
	testCheck( pWriter->publish( publication, &hManifest ) == SIR_RESOURCE_RESULT_SUCCESS );
	const sirAudioManifestLoadOptions_t options = { rLoad.pAllocator, rLoad.nCapabilities, rLoad.eProvider, 0x41554449 };
	const sirAudioManifestResult_t eResult = rStore.load( rGraph, hManifest, { 1 }, options );
	( void )pWriter->collectRetired( 16 );
	return eResult;
}

static void testValidManifestAndHandles( const sirContainerAllocator_t *pAllocator, sirAudioTestAllocationContext_t& state )
{
	sirAudioTestResourceGraph_t sources;
	SIRResourceGraph& graph = sources.graph;
	sirAudioTestManifest_t manifest = makeManifest();
	SIRAudioResourceStore store;
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_OK );
	testCheck( store.isLoaded() && store.getProvider() == SIR_AUDIO_PROVIDER_FMOD );
	testCheck( store.getSourceRevision() == 1 && graph.getStats().m_nActiveReaders == 0 );
	{
		SIRResourceReadScope scope( graph );
		SIRResourceHandle h = {};
		SIRResourceView view;
		SIRResourceConstBytes canonical = {};
		testCheck( graph.find( scope, { 750, 11, 0 }, &h ) == SIR_RESOURCE_RESULT_SUCCESS );
		testCheck( graph.resolve( scope, h, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
		testCheck( view.getChunk( { 1 }, &canonical ) == SIR_RESOURCE_RESULT_SUCCESS );
		testCheck( canonical.m_pData == store.getBorrowedManifestBytes() );
	}
	testCheck( store.getBuildFingerprint() == 1234567 && store.getEpoch() != 0 );
	sirAudioEventRecord_t event = {};
	sirAudioEventHandle_t hEvent = {};
	testCheck( store.findEvent( 100, event, &hEvent ) && event.nProviderEventKey == 2001 );
	testCheck( hEvent.nGeneration == 1 && hEvent.nEpoch == store.getEpoch() );
	testCheck( store.resolveEvent( hEvent, event ) && event.nPackageId == 10 );
	sirAudioPackageRecord_t package = {};
	sirAudioPackageHandle_t hPackage = {};
	testCheck( store.findPackage( 20, package, &hPackage ) && package.nDependencyCount == 1 );
	testCheck( store.resolvePackage( hPackage, package ) && package.nProviderPackageKey == 1002 );
	sirAudioControlRecord_t control = {};
	testCheck( store.findControl( 300, control ) && control.eBindingMode == SIR_AUDIO_BINDING_NATIVE );
	testCheck( store.validateControl( 300, SIR_AUDIO_CONTROL_GLOBAL, 0.75f ) == SIR_AUDIO_OK );
	testCheck( store.validateControl( 300, SIR_AUDIO_CONTROL_EMITTER, 0.75f ) == SIR_AUDIO_INVALID_ARGUMENT );
	testCheck( store.validateControl( 300, SIR_AUDIO_CONTROL_GLOBAL, 1.75f ) == SIR_AUDIO_INVALID_ARGUMENT );
	testCheck( store.validateControl( 400, SIR_AUDIO_CONTROL_EMITTER, 2.0f ) == SIR_AUDIO_OK );
	testCheck( store.validateControl( 400, SIR_AUDIO_CONTROL_EMITTER, 2.5f ) == SIR_AUDIO_INVALID_ARGUMENT );
	testCheck( store.validateControl( 999, SIR_AUDIO_CONTROL_GLOBAL, 1.0f ) == SIR_AUDIO_INVALID_HANDLE );
	testCheck( !store.findControl( 999, control ) && !store.findEvent( 999, event ) && !store.findPackage( 999, package ) );
	const uint32_t nBeforeLookups = state.nAllocations;
	for ( uint32_t iLookup = 0; iLookup < 64; ++iLookup ) {
		testCheck( store.findEvent( 100, event ) && store.findControl( 300, control ) && store.findPackage( 20, package ) );
	}
	testCheck( state.nAllocations == nBeforeLookups );

	// A persistent provider pin, rather than an open read scope, holds the
	// exact canonical bytes across recooks without blocking other readers.
	CHECK( graph.getStats().m_nActiveReaders == 0 );
	// The caller's temporary input is not retained; GRDG owns the manifest.

	manifest.events[0].nEventId = 999;
	testCheck( store.findEvent( 100, event ) && event.nEventId == 100 );
	manifest = makeManifest();
	const uint8_t *pFirstCanonical = store.getBorrowedManifestBytes();
	const uint64_t nFirstEpoch = store.getEpoch();
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_OK );
	testCheck( store.getEpoch() != nFirstEpoch );
	CHECK( store.getBorrowedManifestBytes() == pFirstCanonical && graph.getStats().m_nActiveReaders == 0 );
	testCheck( !store.resolveEvent( hEvent, event ) && !store.resolvePackage( hPackage, package ) );
	testCheck( store.findEvent( 200, event ) && event.nEventId == 200 );
	store.shutdown();
	testCheck( !store.isLoaded() && !store.resolveEvent( hEvent, event ) );
	testCheck( graph.getStats().m_nActiveReaders == 0 );
	testCheck( state.nAllocations == state.nFrees );
}

static void testRejectedManifests( const sirContainerAllocator_t *pAllocator, sirAudioTestAllocationContext_t& state )
{
	sirAudioTestResourceGraph_t sources;
	SIRResourceGraph& graph = sources.graph;
	SIRAudioResourceStore store;
	sirAudioTestManifest_t manifest = makeManifest();
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_WWISE, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_PROVIDER_MISMATCH );
	manifest.header.nVersion = 99;
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	manifest.events[0].nProviderEventKey ^= 1;
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_HASH_MISMATCH );
	manifest = makeManifest();
	manifest.header.nTotalBytes--;
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	manifest.header.nDependencyCount = UINT32_MAX;
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ) - 1, SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 1, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_MISSING_CAPABILITY );
	manifest = makeManifest();
	manifest.events[1].nEventId = manifest.events[0].nEventId;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_DUPLICATE_ID );
	manifest = makeManifest();
	manifest.packages[1].nPackageId = manifest.packages[0].nPackageId;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_DUPLICATE_ID );
	manifest = makeManifest();
	manifest.controls[1].nControlId = manifest.controls[0].nControlId;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_DUPLICATE_ID );
	manifest = makeManifest();
	manifest.events[1].nPackageId = 9999;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_MISSING_DEPENDENCY );
	manifest = makeManifest();
	manifest.dependencies[0] = 9999;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_MISSING_DEPENDENCY );
	manifest = makeManifest();
	manifest.header.nEndianness = 2;
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	manifest.packages[1].iDependencyStart = 1;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_INVALID_RECORD );
	manifest = makeManifest();
	manifest.dependencies[0] = manifest.packages[1].nPackageId;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_INVALID_RECORD );
	manifest = makeManifest();
	manifest.controls[0].fDefault = NAN;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_INVALID_RECORD );
	manifest = makeManifest();
	manifest.controls[1].fDefault = 1.5f;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_INVALID_RECORD );
	manifest = makeManifest();
	manifest.controls[0].eBindingMode = SIR_AUDIO_BINDING_REJECT;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_OK );
	testCheck( store.validateControl( 300, SIR_AUDIO_CONTROL_GLOBAL, 0.5f ) == SIR_AUDIO_UNSUPPORTED );
	store.shutdown();
	testCheck( state.nAllocations == state.nFrees );
}

typedef struct sirAudioCycleManifest_s
{
	sirAudioTestManifest_t base;
	uint64_t nSecondDependency;
} sirAudioCycleManifest_t;

static void testDependencyCycle( const sirContainerAllocator_t *pAllocator, sirAudioTestAllocationContext_t& state )
{
	sirAudioTestResourceGraph_t sources;
	SIRResourceGraph& graph = sources.graph;
	sirAudioCycleManifest_t cycle = {};
	cycle.base = makeManifest();
	cycle.base.header.nTotalBytes = sizeof( cycle );
	cycle.base.header.nDependencyCount = 2;
	cycle.base.packages[0].nDependencyCount = 1;
	cycle.base.packages[1].iDependencyStart = 1;
	cycle.base.dependencies[0] = 20;
	cycle.nSecondDependency = 10;
	cycle.base.header.nContentHash =
	    SIRAudioResourceStore::hashPayload( ( ( const uint8_t * )&cycle ) + sizeof( cycle.base.header.nContentHash ),
		                                    sizeof( cycle ) - sizeof( cycle.base.header.nContentHash ) );
	SIRAudioResourceStore store;
	testCheck( loadTest( store, graph, { &cycle, sizeof( cycle ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_CYCLIC_DEPENDENCY );
	testCheck( !store.isLoaded() && state.nAllocations == state.nFrees );
}

static void testRollbackAndAllocation( const sirContainerAllocator_t *pAllocator, sirAudioTestAllocationContext_t& state )
{
	sirAudioTestResourceGraph_t sources;
	SIRResourceGraph& graph = sources.graph;
	SIRAudioResourceStore store;
	sirAudioTestManifest_t manifest = makeManifest();
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_OK );
	const uint64_t nEpoch = store.getEpoch();
	const uint32_t nAllocationsBefore = state.nAllocations;
	state.bFailNext = true;
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_OUT_OF_MEMORY );
	testCheck( state.nAllocations == nAllocationsBefore && store.getEpoch() == nEpoch );
	sirAudioEventRecord_t event = {};
	testCheck( store.findEvent( 100, event ) );
	manifest.events[0].nEventId = 0;
	refreshHash( manifest );
	testCheck( loadTest( store, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_INVALID_RECORD );
	testCheck( store.getEpoch() == nEpoch && store.findEvent( 100, event ) );
	store.shutdown();
	testCheck( state.nAllocations == state.nFrees );
}

static void testSystemResourceBinding( const sirContainerAllocator_t *pAllocator, sirAudioTestAllocationContext_t& state )
{
	sirAudioTestResourceGraph_t sources;
	SIRResourceGraph& graph = sources.graph;
	sirAudioTestManifest_t manifest = makeManifest();
	SIRAudioResourceStore resources;
	testCheck( loadTest( resources, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_OK );
	SIRAudioMockBackend backend( pAllocator );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 2, 1, 8, 2, 8, 1 };
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend, &resources ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t playing = {};
	testCheck( audio.playEvent( 999, {}, playing ) == SIR_AUDIO_INVALID_ARGUMENT );
	testCheck( audio.playEvent( 100, {}, playing ) == SIR_AUDIO_PENDING );
	testCheck( audio.setGlobalParameter( 300, 0.75f ) == SIR_AUDIO_PENDING );
	testCheck( audio.setGlobalParameter( 400, 2.0f ) == SIR_AUDIO_INVALID_ARGUMENT );
	testCheck( audio.setGlobalParameter( 300, 2.0f ) == SIR_AUDIO_INVALID_ARGUMENT );
	testCheck( audio.setParameter( playing, 300, 0.5f ) == SIR_AUDIO_INVALID_ARGUMENT );
	sirAudioEmitterHandle_t emitter = {};
	testCheck( audio.registerEmitter( emitter ) == SIR_AUDIO_OK );
	testCheck( audio.setEmitterParameter( emitter, 400, 2.0f ) == SIR_AUDIO_PENDING );
	testCheck( audio.setEmitterParameter( emitter, 400, 2.5f ) == SIR_AUDIO_INVALID_ARGUMENT );
	testCheck( audio.update() == 3 );
	manifest.controls[0].eBindingMode = SIR_AUDIO_BINDING_REJECT;
	refreshHash( manifest );
	testCheck( loadTest( resources, graph, { &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator } ) ==
	           SIR_AUDIO_MANIFEST_OK );
	testCheck( audio.playEvent( 100, {}, playing ) == SIR_AUDIO_INVALID_HANDLE );
	testCheck( audio.setGlobalParameter( 300, 0.5f ) == SIR_AUDIO_INVALID_HANDLE );
	audio.shutdown();
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend, &resources ) == SIR_AUDIO_OK );
	testCheck( audio.setGlobalParameter( 300, 0.5f ) == SIR_AUDIO_UNSUPPORTED );
	audio.shutdown();
	resources.shutdown();
	testCheck( state.nAllocations == state.nFrees );
}

int main( void )
{
	sirAudioTestAllocationContext_t state = {};
	const sirContainerAllocator_t allocator = { &state, testAllocate, testDeallocate, 9, 100 };
	testValidManifestAndHandles( &allocator, state );
	testRejectedManifests( &allocator, state );
	testDependencyCycle( &allocator, state );
	testRollbackAndAllocation( &allocator, state );
	testSystemResourceBinding( &allocator, state );
	testCheck( state.nAllocations == state.nFrees );
	return 0;
}
