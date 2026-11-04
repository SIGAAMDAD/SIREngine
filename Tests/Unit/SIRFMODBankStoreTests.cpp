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

#include <Audio/FMOD/SIRFMODBankStore.h>
#include <Audio/FMOD/SIRFMODPackageLoader.h>
#include <Audio/SIRAudioResources.h>
#include <Audio/SIRAudioSystem.h>
#include <stdlib.h>
#include <string.h>

static void checkBank( bool bValid ) noexcept
{
	if ( !bValid ) {
		abort();
	}
}

static uint32_t s_nAllocatedBlocks = 0;
static uint32_t s_nFailAllocate = 0;
static void *testBankAllocate( void *, size_t nBytes, size_t ) noexcept
{
	if ( s_nFailAllocate != 0 ) {
		--s_nFailAllocate;
		return nullptr;
	}
	void *pMemory = malloc( nBytes );
	if ( pMemory != nullptr ) {
		++s_nAllocatedBlocks;
	}
	return pMemory;
}

static void testBankFree( void *, void *pMemory, size_t, size_t ) noexcept
{
	if ( pMemory != nullptr ) {
		--s_nAllocatedBlocks;
		free( pMemory );
	}
}

typedef struct sirFMODBankFixtureVoice_s
{
	bool bUsed;
	bool bStopped;
} sirFMODBankFixtureVoice_t;

typedef struct sirFMODBankFixture_s
{
	sirFMODBankFixtureVoice_t voice;
	uint32_t nLoads;
	uint32_t nUnloads;
	uint32_t nPreloads;
	uint32_t nChecks;
	uint32_t nStarts;
	uint32_t nReleases;
	uint32_t nFailLoad;
	uint32_t nFailLoadWithHandle;
	uint32_t nFailUnload;
	uint32_t nFailPreload;
	uint32_t nFailMembership;
	uint32_t nFailSample;
	uint32_t nFailBankB;
	uint32_t nFailBankBWithHandle;
	bool bBankA;
	bool bBankB;
	bool bStudio;
} sirFMODBankFixture_t;

static int32_t testStudioCreate( void *pData, void **ppStudio ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pData;
	pFixture->bStudio = true;
	*ppStudio = pFixture;
	return 0;
}
static int32_t testStudioCore( void *, void *pStudio, void **ppCore ) noexcept
{
	*ppCore = pStudio;
	return 0;
}
static int32_t testStudioVersion( void *, void *, uint32_t *pVersion ) noexcept
{
	*pVersion = 0x00020400;
	return 0;
}
static int32_t testStudioDrivers( void *, void *, int32_t *pCount ) noexcept
{
	*pCount = 1;
	return 0;
}
static int32_t testStudioDriver( void *, void *, int32_t ) noexcept
{
	return 0;
}
static int32_t testStudioNoSound( void *, void * ) noexcept
{
	return 0;
}
static int32_t testStudioInitialize( void *, void *, const sirFMODContextConfig_t * ) noexcept
{
	return 0;
}
static int32_t testStudioUpdate( void *, void * ) noexcept
{
	return 0;
}
static int32_t testStudioRelease( void *pData, void * ) noexcept
{
	( ( sirFMODBankFixture_t * )pData )->bStudio = false;
	return 0;
}
static sirFMODNativeOps_t makeStudioOps( sirFMODBankFixture_t *pFixture ) noexcept
{
	return { 1,
	         0x00020400,
	         pFixture,
	         testStudioCreate,
	         testStudioCore,
	         testStudioVersion,
	         testStudioDrivers,
	         testStudioDriver,
	         testStudioNoSound,
	         testStudioInitialize,
	         testStudioUpdate,
	         testStudioRelease };
}

static int32_t testVoiceResolve( void *pData, void *, const char *szPath, void **ppDescription ) noexcept
{
	if ( strcmp( szPath, "event:/A" ) != 0 ) {
		return -30;
	}
	*ppDescription = pData;
	return 0;
}
static int32_t testVoiceCreate( void *pData, void *, void **ppInstance ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pData;
	if ( pFixture->voice.bUsed ) {
		return -31;
	}
	pFixture->voice.bUsed = true;
	pFixture->voice.bStopped = false;
	*ppInstance = &pFixture->voice;
	return 0;
}
static int32_t testVoiceStart( void *pData, void * ) noexcept
{
	++( ( sirFMODBankFixture_t * )pData )->nStarts;
	return 0;
}
static int32_t testVoiceStop( void *, void *pInstance, sirAudioStopMode_t ) noexcept
{
	( ( sirFMODBankFixtureVoice_t * )pInstance )->bStopped = true;
	return 0;
}
static int32_t testVoiceStopped( void *, void *pInstance, uint8_t *pbStopped ) noexcept
{
	*pbStopped = ( ( sirFMODBankFixtureVoice_t * )pInstance )->bStopped ? 1 : 0;
	return 0;
}
static int32_t testVoiceRelease( void *pData, void *pInstance ) noexcept
{
	++( ( sirFMODBankFixture_t * )pData )->nReleases;
	( ( sirFMODBankFixtureVoice_t * )pInstance )->bUsed = false;
	return 0;
}
static sirFMODVoiceNativeOps_t makeVoiceOps( sirFMODBankFixture_t *pFixture ) noexcept
{
	return { 3, pFixture, testVoiceResolve, testVoiceCreate, testVoiceStart, testVoiceStop, testVoiceStopped, testVoiceRelease,
	         nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
}

static int32_t testLoadBank( void *pData, void *pStudio, const char *szPath, void **ppBank ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pData;
	checkBank( pStudio == pFixture );
	++pFixture->nLoads;
	if ( pFixture->nFailLoadWithHandle != 0 ) {
		--pFixture->nFailLoadWithHandle;
		pFixture->bBankA = true;
		*ppBank = &pFixture->bBankA;
		return -46;
	}
	if ( pFixture->nFailLoad != 0 ) {
		--pFixture->nFailLoad;
		return -40;
	}
	if ( strcmp( szPath, "A.bank" ) == 0 ) {
		pFixture->bBankA = true;
		*ppBank = &pFixture->bBankA;
		return 0;
	}
	if ( strcmp( szPath, "B.bank" ) == 0 ) {
		if ( pFixture->nFailBankB != 0 ) {
			--pFixture->nFailBankB;
			return -51;
		}
		if ( pFixture->nFailBankBWithHandle != 0 ) {
			--pFixture->nFailBankBWithHandle;
			pFixture->bBankB = true;
			*ppBank = &pFixture->bBankB;
			return -52;
		}
		pFixture->bBankB = true;
		*ppBank = &pFixture->bBankB;
		return 0;
	}
	return -41;
}
static int32_t testUnloadBank( void *pData, void *pBank ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pData;
	++pFixture->nUnloads;
	if ( pFixture->nFailUnload != 0 ) {
		--pFixture->nFailUnload;
		return -42;
	}
	*( ( bool * )pBank ) = false;
	return 0;
}
static int32_t testPreloadBank( void *pData, void * ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pData;
	++pFixture->nPreloads;
	if ( pFixture->nFailPreload != 0 ) {
		--pFixture->nFailPreload;
		return -43;
	}
	return 0;
}
static int32_t testSampleState( void *pData, void *, sirFMODSampleState_t *pState ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pData;
	if ( pFixture->nFailSample != 0 ) {
		--pFixture->nFailSample;
		return -44;
	}
	*pState = SIR_FMOD_SAMPLE_LOADING;
	return 0;
}
static int32_t testContainsEvent( void *pData, void *pStudio, void *pBank, const char *szPath, const sirContainerAllocator_t *pAllocator,
                                  uint8_t *pbFound ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pData;
	checkBank( pStudio == pFixture && pAllocator != nullptr );
	++pFixture->nChecks;
	if ( pFixture->nFailMembership != 0 ) {
		--pFixture->nFailMembership;
		return -45;
	}
	*pbFound = pBank == &pFixture->bBankA && strcmp( szPath, "event:/A" ) == 0 ? 1 : 0;
	return 0;
}
static sirFMODBankNativeOps_t makeBankOps( sirFMODBankFixture_t *pFixture ) noexcept
{
	return { 1, pFixture, testLoadBank, testUnloadBank, testPreloadBank, testSampleState, testContainsEvent };
}

static void testBankLifecycle( const sirContainerAllocator_t *pAllocator )
{
	sirFMODBankFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkBank( context.initialize( contextConfig, makeStudioOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend voices;
	checkBank( voices.configure( &context, pAllocator, makeVoiceOps( &fixture ), 3 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t audioConfig = { 2, 0, 4, 2, 4, 0 };
	checkBank( audio.initialize( audioConfig, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &voices ) == SIR_AUDIO_OK );
	SIRFMODBankStore banks;
	checkBank( banks.initialize( &context, &voices, pAllocator, makeBankOps( &fixture ), 2, 2 ) == SIR_AUDIO_OK );
	checkBank( banks.loadBank( 0, "A.bank" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkBank( banks.loadBank( 1, "A.bank" ) == SIR_AUDIO_OK );
	checkBank( banks.loadBank( 1, "A.bank" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkBank( banks.loadBank( 2, "B.bank" ) == SIR_AUDIO_OK );
	checkBank( banks.loadBank( 3, "B.bank" ) == SIR_AUDIO_CAPACITY );
	checkBank( banks.getLoadedBankCount() == 2 );
	checkBank( banks.bindEvent( 2, 101, "event:/A" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkBank( banks.bindEvent( 1, 101, "event:/A" ) == SIR_AUDIO_OK );
	checkBank( banks.bindEvent( 1, 101, "event:/A" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkBank( banks.getBoundEventCount() == 1 );
	checkBank( banks.bindEvent( 999, 102, "event:/A" ) == SIR_AUDIO_INVALID_ARGUMENT );
	sirFMODSampleState_t eSampleState = SIR_FMOD_SAMPLE_ERROR;
	checkBank( banks.getSampleState( 1, eSampleState ) == SIR_AUDIO_OK && eSampleState == SIR_FMOD_SAMPLE_UNLOADED );
	checkBank( banks.preloadSamples( 1 ) == SIR_AUDIO_OK );
	checkBank( banks.preloadSamples( 1 ) == SIR_AUDIO_OK && fixture.nPreloads == 1 );
	checkBank( banks.getSampleState( 1, eSampleState ) == SIR_AUDIO_OK && eSampleState == SIR_FMOD_SAMPLE_LOADING );

	sirAudioPlayingHandle_t hPlaying = {};
	checkBank( audio.playEvent( 101, {}, hPlaying ) == SIR_AUDIO_PENDING );
	checkBank( audio.update() == 1 );
	checkBank( banks.unbindEvent( 1, 101 ) == SIR_AUDIO_BUSY );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_BUSY );
	checkBank( banks.shutdown() == SIR_AUDIO_BUSY );
	checkBank( banks.getLoadedBankCount() == 2 );
	checkBank( fixture.bBankA && fixture.nUnloads == 0 );
	checkBank( audio.stopEvent( hPlaying, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	checkBank( audio.update() == 1 );
	checkBank( voices.getActiveVoiceCount() == 0 );
	fixture.nFailUnload = 1;
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_BACKEND_ERROR );
	checkBank( fixture.bBankA && banks.getBoundEventCount() == 1 );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_OK );
	checkBank( !fixture.bBankA && banks.getBoundEventCount() == 0 );
	checkBank( banks.loadBank( 1, "A.bank" ) == SIR_AUDIO_OK );
	checkBank( banks.bindEvent( 1, 101, "event:/A" ) == SIR_AUDIO_OK );
	checkBank( banks.unbindEvent( 1, 101 ) == SIR_AUDIO_OK );
	checkBank( banks.shutdown() == SIR_AUDIO_OK );
	checkBank( banks.getLoadedBankCount() == 0 );
	audio.shutdown();
	checkBank( context.shutdown() == SIR_FMOD_OK );
	checkBank( !fixture.bStudio && !fixture.bBankA && !fixture.bBankB );
}

static void testBankFailures( const sirContainerAllocator_t *pAllocator )
{
	sirFMODBankFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkBank( context.initialize( contextConfig, makeStudioOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend voices;
	checkBank( voices.configure( &context, pAllocator, makeVoiceOps( &fixture ), 2 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t audioConfig = { 1, 0, 4, 2, 2, 0 };
	checkBank( audio.initialize( audioConfig, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &voices ) == SIR_AUDIO_OK );
	SIRFMODBankStore banks;
	const uint32_t nBefore = s_nAllocatedBlocks;
	s_nFailAllocate = 1;
	checkBank( banks.initialize( &context, &voices, pAllocator, makeBankOps( &fixture ), 2, 2 ) == SIR_AUDIO_CAPACITY );
	checkBank( s_nAllocatedBlocks == nBefore );
	checkBank( banks.initialize( &context, &voices, pAllocator, makeBankOps( &fixture ), 2, 2 ) == SIR_AUDIO_OK );
	fixture.nFailLoad = 1;
	checkBank( banks.loadBank( 1, "A.bank" ) == SIR_AUDIO_BACKEND_ERROR );
	checkBank( banks.getLoadedBankCount() == 0 );
	fixture.nFailLoadWithHandle = 1;
	checkBank( banks.loadBank( 1, "A.bank" ) == SIR_AUDIO_BACKEND_ERROR );
	checkBank( banks.getLoadedBankCount() == 1 );
	checkBank( banks.bindEvent( 1, 2, "event:/A" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_OK );
	checkBank( banks.loadBank( 1, "A.bank" ) == SIR_AUDIO_OK );
	fixture.nFailMembership = 1;
	checkBank( banks.bindEvent( 1, 2, "event:/A" ) == SIR_AUDIO_BACKEND_ERROR );
	checkBank( banks.getBoundEventCount() == 0 );
	checkBank( banks.bindEvent( 1, 2, "event:/A" ) == SIR_AUDIO_OK );
	fixture.nFailPreload = 1;
	checkBank( banks.preloadSamples( 1 ) == SIR_AUDIO_BACKEND_ERROR );
	checkBank( banks.preloadSamples( 1 ) == SIR_AUDIO_OK );
	fixture.nFailSample = 1;
	sirFMODSampleState_t eState = SIR_FMOD_SAMPLE_UNLOADED;
	checkBank( banks.getSampleState( 1, eState ) == SIR_AUDIO_BACKEND_ERROR );
	checkBank( banks.getSampleState( 1, eState ) == SIR_AUDIO_OK );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_OK );
	checkBank( banks.loadBank( 7, "A.bank" ) == SIR_AUDIO_OK );
	checkBank( banks.bindEvent( 7, 3, "event:/A" ) == SIR_AUDIO_OK );
	audio.shutdown();
	checkBank( banks.shutdown() == SIR_AUDIO_OK );
	checkBank( context.shutdown() == SIR_FMOD_OK );
}

typedef struct sirFMODPackageTestManifest_s
{
	sirAudioManifestHeader_t header;
	sirAudioPackageRecord_t packages[2];
	uint64_t dependencies[1];
} sirFMODPackageTestManifest_t;

static sirFMODPackageTestManifest_t makePackageManifest( void ) noexcept
{
	sirFMODPackageTestManifest_t image = {};
	image.header.nMagic = SIR_AUDIO_MANIFEST_MAGIC;
	image.header.nTotalBytes = sizeof( image );
	image.header.nPackageCount = 2;
	image.header.nDependencyCount = 1;
	image.header.nVersion = SIR_AUDIO_MANIFEST_VERSION;
	image.header.nHeaderBytes = sizeof( image.header );
	image.header.eProvider = SIR_AUDIO_PROVIDER_FMOD;
	image.header.nEndianness = SIR_AUDIO_MANIFEST_LITTLE_ENDIAN;
	image.packages[0] = { 1, 101, 0, 0, 0 };
	image.packages[1] = { 2, 102, 0, 0, 1 };
	image.dependencies[0] = 1;
	image.header.nContentHash = SIRAudioResourceStore::hashPayload( ( const uint8_t * )&image + sizeof( image.header.nContentHash ),
	                                                                sizeof( image ) - sizeof( image.header.nContentHash ) );
	return image;
}

static sirAudioResult_t testResolvePackagePath( void *, uint64_t nPackageId, uint64_t nProviderKey, const char **pszBankPath ) noexcept
{
	if ( nPackageId == 1 && nProviderKey == 101 ) {
		*pszBankPath = "A.bank";
		return SIR_AUDIO_OK;
	}
	if ( nPackageId == 2 && nProviderKey == 102 ) {
		*pszBankPath = "B.bank";
		return SIR_AUDIO_OK;
	}
	return SIR_AUDIO_INVALID_ARGUMENT;
}

static void testManifestPackageLoading( const sirContainerAllocator_t *pAllocator )
{
	sirFMODBankFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkBank( context.initialize( contextConfig, makeStudioOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend voices;
	checkBank( voices.configure( &context, pAllocator, makeVoiceOps( &fixture ), 2 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t audioConfig = { 1, 0, 4, 2, 2, 0 };
	checkBank( audio.initialize( audioConfig, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &voices ) == SIR_AUDIO_OK );
	SIRFMODBankStore banks;
	checkBank( banks.initialize( &context, &voices, pAllocator, makeBankOps( &fixture ), 2, 1 ) == SIR_AUDIO_OK );
	SIRAudioResourceStore manifest;
	sirFMODPackageTestManifest_t image = makePackageManifest();
	checkBank( manifest.load( &image, sizeof( image ), SIR_AUDIO_PROVIDER_FMOD, 0, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
	checkBank( manifest.getPackageCount() == 2 );
	sirAudioPackageRecord_t package = {};
	uint64_t nDependency = 0;
	checkBank( manifest.getPackageAt( 1, package ) && package.nPackageId == 2 );
	checkBank( manifest.getPackageDependency( 2, 0, nDependency ) && nDependency == 1 );
	checkBank( !manifest.getPackageDependency( 2, 1, nDependency ) );
	checkBank( !manifest.getPackageAt( 2, package ) );
	SIRFMODPackageLoader loader;
	checkBank( loader.initialize( &manifest, &banks, pAllocator, { nullptr, testResolvePackagePath } ) == SIR_AUDIO_OK );
	checkBank( loader.loadPackage( 2, false ) == SIR_AUDIO_OK );
	checkBank( loader.getLoadedCount() == 2 && fixture.bBankA && fixture.bBankB );
	checkBank( loader.loadPackage( 2, true ) == SIR_AUDIO_OK && fixture.nPreloads == 2 );
	checkBank( loader.loadPackage( 2, true ) == SIR_AUDIO_OK && fixture.nPreloads == 2 );
	checkBank( loader.loadPackage( 999, false ) == SIR_AUDIO_INVALID_HANDLE );
	uint32_t nReferences = 0;
	uint32_t nRootReferences = 0;
	checkBank( loader.getPackageReferences( 1, nReferences, nRootReferences ) && nReferences == 3 && nRootReferences == 0 );
	checkBank( loader.getPackageReferences( 2, nReferences, nRootReferences ) && nReferences == 3 && nRootReferences == 3 );
	checkBank( loader.loadPackage( 1, false ) == SIR_AUDIO_OK );
	checkBank( loader.getPackageReferences( 1, nReferences, nRootReferences ) && nReferences == 4 && nRootReferences == 1 );
	checkBank( loader.releasePackage( 2 ) == SIR_AUDIO_OK );
	checkBank( loader.releasePackage( 2 ) == SIR_AUDIO_OK );
	checkBank( loader.getLoadedCount() == 2 && fixture.bBankB );
	checkBank( loader.releasePackage( 2 ) == SIR_AUDIO_OK );
	checkBank( loader.getLoadedCount() == 1 && fixture.bBankA && !fixture.bBankB );
	checkBank( loader.releasePackage( 2 ) == SIR_AUDIO_INVALID_HANDLE );
	checkBank( loader.getPackageReferences( 1, nReferences, nRootReferences ) && nReferences == 1 && nRootReferences == 1 );
	checkBank( loader.releasePackage( 1 ) == SIR_AUDIO_OK );
	checkBank( loader.getLoadedCount() == 0 && !fixture.bBankA );
	checkBank( loader.unloadAll() == SIR_AUDIO_OK );
	checkBank( loader.getLoadedCount() == 0 && !fixture.bBankA && !fixture.bBankB );

	fixture.nFailBankB = 1;
	checkBank( loader.loadPackage( 2, false ) == SIR_AUDIO_BACKEND_ERROR );
	checkBank( loader.getLastFailedPackageId() == 2 );
	checkBank( loader.getLoadedCount() == 0 && banks.getLoadedBankCount() == 0 );
	checkBank( !fixture.bBankA && !fixture.bBankB );
	fixture.nFailBankBWithHandle = 1;
	fixture.nFailUnload = 1;
	checkBank( loader.loadPackage( 2, false ) == SIR_AUDIO_BACKEND_ERROR );
	checkBank( loader.getLoadedCount() == 2 && banks.getLoadedBankCount() == 2 );
	checkBank( loader.loadPackage( 2, false ) == SIR_AUDIO_BUSY );
	checkBank( loader.unloadAll() == SIR_AUDIO_OK );
	checkBank( loader.getLoadedCount() == 0 && !fixture.bBankA && !fixture.bBankB );
	checkBank( loader.loadPackage( 2, false ) == SIR_AUDIO_OK );
	fixture.nFailUnload = 1;
	checkBank( loader.releasePackage( 2 ) == SIR_AUDIO_BACKEND_ERROR );
	checkBank( loader.getLastFailedPackageId() == 2 && loader.getLoadedCount() == 2 );
	checkBank( loader.getPackageReferences( 2, nReferences, nRootReferences ) && nReferences == 0 && nRootReferences == 0 );
	checkBank( loader.releasePackage( 2 ) == SIR_AUDIO_INVALID_HANDLE );
	checkBank( loader.collectUnused() == SIR_AUDIO_OK );
	checkBank( loader.getLoadedCount() == 0 );
	checkBank( loader.loadPackage( 2, false ) == SIR_AUDIO_OK );
	image = makePackageManifest();
	checkBank( manifest.load( &image, sizeof( image ), SIR_AUDIO_PROVIDER_FMOD, 0, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
	checkBank( loader.loadPackage( 2, false ) == SIR_AUDIO_NOT_INITIALIZED );
	checkBank( loader.shutdown() == SIR_AUDIO_OK );
	checkBank( banks.shutdown() == SIR_AUDIO_OK );
	manifest.shutdown();
	audio.shutdown();
	checkBank( context.shutdown() == SIR_FMOD_OK );
	checkBank( !fixture.bStudio );
}

int main( void )
{
	const sirContainerAllocator_t allocator = { nullptr, testBankAllocate, testBankFree, 0, 0 };
	testBankLifecycle( &allocator );
	testBankFailures( &allocator );
	testManifestPackageLoading( &allocator );
	checkBank( s_nAllocatedBlocks == 0 );
	return 0;
}
