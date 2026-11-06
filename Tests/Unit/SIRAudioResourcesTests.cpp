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

static void testValidManifestAndHandles( const sirContainerAllocator_t *pAllocator, sirAudioTestAllocationContext_t& state )
{
	sirAudioTestManifest_t manifest = makeManifest();
	SIRAudioResourceStore store;
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
	testCheck( store.isLoaded() && store.getProvider() == SIR_AUDIO_PROVIDER_FMOD );
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

	// Runtime lookup owns a copy, independent of caller-side temporary bytes.
	manifest.events[0].nEventId = 999;
	testCheck( store.findEvent( 100, event ) && event.nEventId == 100 );
	manifest = makeManifest();
	const uint64_t nFirstEpoch = store.getEpoch();
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
	testCheck( store.getEpoch() != nFirstEpoch );
	testCheck( !store.resolveEvent( hEvent, event ) && !store.resolvePackage( hPackage, package ) );
	testCheck( store.findEvent( 200, event ) && event.nEventId == 200 );
	store.shutdown();
	testCheck( !store.isLoaded() && !store.resolveEvent( hEvent, event ) );
	testCheck( state.nAllocations == state.nFrees );
}

static void testRejectedManifests( const sirContainerAllocator_t *pAllocator, sirAudioTestAllocationContext_t& state )
{
	SIRAudioResourceStore store;
	sirAudioTestManifest_t manifest = makeManifest();
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_WWISE, 3, pAllocator ) ==
	           SIR_AUDIO_MANIFEST_PROVIDER_MISMATCH );
	manifest.header.nVersion = 99;
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	manifest.events[0].nProviderEventKey ^= 1;
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_HASH_MISMATCH );
	manifest = makeManifest();
	manifest.header.nTotalBytes--;
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	manifest.header.nDependencyCount = UINT32_MAX;
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	testCheck( store.load( &manifest, sizeof( manifest ) - 1, SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 1, pAllocator ) ==
	           SIR_AUDIO_MANIFEST_MISSING_CAPABILITY );
	manifest = makeManifest();
	manifest.events[1].nEventId = manifest.events[0].nEventId;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_DUPLICATE_ID );
	manifest = makeManifest();
	manifest.packages[1].nPackageId = manifest.packages[0].nPackageId;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_DUPLICATE_ID );
	manifest = makeManifest();
	manifest.controls[1].nControlId = manifest.controls[0].nControlId;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_DUPLICATE_ID );
	manifest = makeManifest();
	manifest.events[1].nPackageId = 9999;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) ==
	           SIR_AUDIO_MANIFEST_MISSING_DEPENDENCY );
	manifest = makeManifest();
	manifest.dependencies[0] = 9999;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) ==
	           SIR_AUDIO_MANIFEST_MISSING_DEPENDENCY );
	manifest = makeManifest();
	manifest.header.nEndianness = 2;
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_BAD_SCHEMA );
	manifest = makeManifest();
	manifest.packages[1].iDependencyStart = 1;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_INVALID_RECORD );
	manifest = makeManifest();
	manifest.dependencies[0] = manifest.packages[1].nPackageId;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_INVALID_RECORD );
	manifest = makeManifest();
	manifest.controls[0].fDefault = NAN;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_INVALID_RECORD );
	manifest = makeManifest();
	manifest.controls[1].fDefault = 1.5f;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_INVALID_RECORD );
	manifest = makeManifest();
	manifest.controls[0].eBindingMode = SIR_AUDIO_BINDING_REJECT;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
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
	testCheck( store.load( &cycle, sizeof( cycle ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_CYCLIC_DEPENDENCY );
	testCheck( !store.isLoaded() && state.nAllocations == state.nFrees );
}

static void testRollbackAndAllocation( const sirContainerAllocator_t *pAllocator, sirAudioTestAllocationContext_t& state )
{
	SIRAudioResourceStore store;
	sirAudioTestManifest_t manifest = makeManifest();
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
	const uint64_t nEpoch = store.getEpoch();
	const uint32_t nAllocationsBefore = state.nAllocations;
	state.bFailNext = true;
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_OUT_OF_MEMORY );
	testCheck( state.nAllocations == nAllocationsBefore && store.getEpoch() == nEpoch );
	sirAudioEventRecord_t event = {};
	testCheck( store.findEvent( 100, event ) );
	manifest.events[0].nEventId = 0;
	refreshHash( manifest );
	testCheck( store.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_INVALID_RECORD );
	testCheck( store.getEpoch() == nEpoch && store.findEvent( 100, event ) );
	store.shutdown();
	testCheck( state.nAllocations == state.nFrees );
}

static void testSystemResourceBinding( const sirContainerAllocator_t *pAllocator, sirAudioTestAllocationContext_t& state )
{
	sirAudioTestManifest_t manifest = makeManifest();
	SIRAudioResourceStore resources;
	testCheck( resources.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
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
	testCheck( resources.load( &manifest, sizeof( manifest ), SIR_AUDIO_PROVIDER_FMOD, 3, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
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
