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
#include <Core/Containers/SIRRawStorage.h>
#include <Audio/FMOD/SIRFMODAudioThread.h>
#include <chrono>
#include <atomic>
#include <thread>
#include <Audio/FMOD/SIRFMODRuntime.h>
#include <Audio/FMOD/SIRFMODMixerBackend.h>
#include <Audio/FMOD/SIRFMODSnapshotBackend.h>
#include <Audio/FMOD/SIRFMODPackageLoader.h>
#include <Audio/SIRAudioResources.h>
#include <Audio/SIRAudioSystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void checkBankAt( bool bValid, uint32_t nLine ) noexcept
{
	if ( !bValid ) {
		fprintf( stderr, "FMOD bank regression failed at line %u\n", nLine );
		abort();
	}
}

#define checkBank( expression ) checkBankAt( ( expression ), __LINE__ )

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
	uint32_t nAsyncPolls;
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
	bool bTriggerDeviceLost;
	void *pDeviceOwner;
	void ( *pfnDeviceSignal )( void * ) noexcept;
	uint32_t nDeviceDriverSelections;
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
static std::atomic<uint32_t> s_nDriverGate( 0 );
static std::atomic<uint32_t> s_nDriverEntered( 0 );

static int32_t testStudioDriver( void *pUserData, void *, int32_t ) noexcept
{
	++( ( sirFMODBankFixture_t * )pUserData )->nDeviceDriverSelections;
	if ( s_nDriverGate.load( std::memory_order_acquire ) != 0 ) {
		s_nDriverEntered.store( 1, std::memory_order_release );
		while ( s_nDriverGate.load( std::memory_order_acquire ) != 0 ) {
			std::this_thread::yield();
		}
	}
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
static int32_t testStudioUpdate( void *pUserData, void * ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pUserData;
	if ( pFixture->bTriggerDeviceLost && pFixture->pfnDeviceSignal != nullptr ) {
		pFixture->bTriggerDeviceLost = false;
		pFixture->pfnDeviceSignal( pFixture->pDeviceOwner );
	}
	return 0;
}
static int32_t testStudioRelease( void *pData, void * ) noexcept
{
	( ( sirFMODBankFixture_t * )pData )->bStudio = false;
	return 0;
}
static int32_t testStudioDeviceLossCallback( void *pUserData, void *, void *pOwner, void ( *pfnSignal )( void * ) noexcept ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pUserData;
	pFixture->pDeviceOwner = pOwner;
	pFixture->pfnDeviceSignal = pfnSignal;
	return 0;
}

static sirFMODNativeOps_t makeStudioOps( sirFMODBankFixture_t *pFixture ) noexcept
{
	return { 2,
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
	         testStudioRelease,
	         testStudioDeviceLossCallback };
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
	return { 4,       pFixture, testVoiceResolve, testVoiceCreate, testVoiceStart, testVoiceStop, testVoiceStopped, testVoiceRelease,
	         nullptr, nullptr,  nullptr,          nullptr,         nullptr,        nullptr,       nullptr,          nullptr,
	         nullptr, nullptr,  nullptr };
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
static int32_t testContainsBankObject( void *pData, void *pStudio, void *pBank, sirFMODBankObjectKind_t eKind, const char *szPath,
                                       const sirContainerAllocator_t *pAllocator, uint8_t *pbFound ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pData;
	checkBank( pStudio == pFixture && pAllocator != nullptr );
	if ( pFixture->nFailMembership != 0 ) {
		--pFixture->nFailMembership;
		return -45;
	}
	const bool bBankA = pBank == &pFixture->bBankA;
	const bool bBankB = pBank == &pFixture->bBankB;
	*pbFound = 0;
	if ( eKind == SIR_FMOD_BANK_OBJECT_BUS && bBankA && strcmp( szPath, "bus:/Master" ) == 0 ) {
		*pbFound = 1;
	} else if ( eKind == SIR_FMOD_BANK_OBJECT_VCA && ( bBankA || bBankB ) && strcmp( szPath, "vca:/Music" ) == 0 ) {
		*pbFound = 1;
	} else if ( eKind == SIR_FMOD_BANK_OBJECT_SNAPSHOT && bBankA && strcmp( szPath, "snapshot:/Storm" ) == 0 ) {
		*pbFound = 1;
	}
	return 0;
}

static int32_t testLoadBankAsync( void *pData, void *pStudio, const char *szPath, void **ppBank ) noexcept
{
	return testLoadBank( pData, pStudio, szPath, ppBank );
}

static int32_t testGetBankLoadingState( void *pData, void *, sirFMODSampleState_t *pState ) noexcept
{
	sirFMODBankFixture_t *pFixture = ( sirFMODBankFixture_t * )pData;
	if ( pFixture->nAsyncPolls != 0 ) {
		--pFixture->nAsyncPolls;
		*pState = SIR_FMOD_SAMPLE_LOADING;
	} else {
		*pState = SIR_FMOD_SAMPLE_READY;
	}
	return 0;
}

static sirFMODBankNativeOps_t makeBankOps( sirFMODBankFixture_t *pFixture ) noexcept
{
	return { 3,
	         pFixture,
	         testLoadBank,
	         testUnloadBank,
	         testPreloadBank,
	         testSampleState,
	         testContainsEvent,
	         testContainsBankObject,
	         testLoadBankAsync,
	         testGetBankLoadingState };
}

static int32_t testPinnedBusResolve( void *pData, void *pStudio, const char *szPath, void **ppBus ) noexcept
{
	checkBank( pStudio == pData );
	if ( strcmp( szPath, "bus:/Master" ) != 0 ) {
		return -60;
	}
	*ppBus = pData;
	return 0;
}

static int32_t testPinnedVCAResolve( void *pData, void *pStudio, const char *szPath, void **ppVCA ) noexcept
{
	checkBank( pStudio == pData );
	if ( strcmp( szPath, "vca:/Music" ) != 0 ) {
		return -61;
	}
	*ppVCA = pData;
	return 0;
}

static int32_t testPinnedSetVolume( void *, void *, float ) noexcept
{
	return 0;
}

static int32_t testPinnedSetMute( void *, void *, uint8_t ) noexcept
{
	return 0;
}

static int32_t testPinnedSnapshotResolve( void *pData, void *pStudio, const char *szPath, void **ppDescription ) noexcept
{
	checkBank( pStudio == pData );
	if ( strcmp( szPath, "snapshot:/Storm" ) != 0 ) {
		return -62;
	}
	*ppDescription = pData;
	return 0;
}

static void testBorrowedBankObjectPins( const sirContainerAllocator_t *pAllocator )
{
	sirFMODBankFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 16, -1, 1, 1, 0, 0 };
	checkBank( context.initialize( config, makeStudioOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend voices;
	checkBank( voices.configure( &context, pAllocator, makeVoiceOps( &fixture ), 1 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t audioConfig = { 1, 0, 4, 2, 2, 0 };
	checkBank( audio.initialize( audioConfig, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &voices ) == SIR_AUDIO_OK );
	SIRFMODBankStore banks;
	checkBank( banks.initialize( &context, &voices, pAllocator, makeBankOps( &fixture ), 2, 1 ) == SIR_AUDIO_OK );
	checkBank( banks.loadBank( 1, "A.bank" ) == SIR_AUDIO_OK );

	const sirFMODMixerNativeOps_t mixerOps = {
	    1, &fixture, testPinnedBusResolve, testPinnedVCAResolve, testPinnedSetVolume, testPinnedSetMute, testPinnedSetVolume };
	SIRFMODMixerBackend mixer;
	checkBank( mixer.configure( &context, pAllocator, mixerOps, 2 ) == SIR_AUDIO_OK );
	checkBank( mixer.initialize() == SIR_AUDIO_OK );
	checkBank( mixer.registerBus( 10, "bus:/Master" ) == SIR_AUDIO_OK );
	checkBank( mixer.registerVCA( 20, "vca:/Music" ) == SIR_AUDIO_OK );
	checkBank( context.getBorrowedBankObjectCount() == 2 );

	const sirFMODSnapshotNativeOps_t snapshotOps = {
	    1, &fixture, testPinnedSnapshotResolve, testVoiceCreate, testVoiceStart, testVoiceStop, testVoiceRelease };
	SIRFMODSnapshotBackend snapshots;
	checkBank( snapshots.configure( &context, pAllocator, snapshotOps, 1 ) == SIR_AUDIO_OK );
	checkBank( snapshots.initialize() == SIR_AUDIO_OK );
	checkBank( snapshots.registerSnapshot( 30, "snapshot:/Storm" ) == SIR_AUDIO_OK );
	checkBank( context.getBorrowedBankObjectCount() == 3 );
	checkBank( snapshots.startSnapshot( 30 ) == SIR_AUDIO_OK );
	checkBank( snapshots.unregisterSnapshot( 30 ) == SIR_AUDIO_BUSY );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_BUSY );
	checkBank( fixture.nUnloads == 0 && banks.getLoadedBankCount() == 1 );
	checkBank( banks.shutdown() == SIR_AUDIO_BUSY );
	checkBank( context.shutdown() == SIR_FMOD_BAD_STATE );
	checkBank( context.isReady() && fixture.bStudio );

	checkBank( snapshots.stopSnapshot( 30, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_OK );
	checkBank( snapshots.unregisterSnapshot( 30 ) == SIR_AUDIO_OK );
	checkBank( context.getBorrowedBankObjectCount() == 2 );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_BUSY );
	checkBank( mixer.unregisterControl( 10 ) == SIR_AUDIO_OK );
	checkBank( context.getBorrowedBankObjectCount() == 1 );
	mixer.shutdown();
	checkBank( context.getBorrowedBankObjectCount() == 0 );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_OK );
	checkBank( fixture.nUnloads == 1 && banks.getLoadedBankCount() == 0 );
	checkBank( snapshots.shutdown() == SIR_AUDIO_OK );
	checkBank( banks.shutdown() == SIR_AUDIO_OK );
	audio.shutdown();
	checkBank( context.shutdown() == SIR_FMOD_OK );
}

static void testScopedBankOwnership( const sirContainerAllocator_t *pAllocator )
{
	sirFMODBankFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 16, -1, 1, 1, 0, 0 };
	checkBank( context.initialize( config, makeStudioOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend voices;
	checkBank( voices.configure( &context, pAllocator, makeVoiceOps( &fixture ), 1 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t audioConfig = { 1, 0, 4, 2, 2, 0 };
	checkBank( audio.initialize( audioConfig, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &voices ) == SIR_AUDIO_OK );
	SIRFMODBankStore banks;
	checkBank( banks.initialize( &context, &voices, pAllocator, makeBankOps( &fixture ), 2, 1 ) == SIR_AUDIO_OK );
	checkBank( banks.loadBank( 1, "A.bank" ) == SIR_AUDIO_OK );
	checkBank( banks.loadBank( 2, "B.bank" ) == SIR_AUDIO_OK );

	const sirFMODMixerNativeOps_t mixerOps = {
	    1, &fixture, testPinnedBusResolve, testPinnedVCAResolve, testPinnedSetVolume, testPinnedSetMute, testPinnedSetVolume };
	SIRFMODMixerBackend mixer;
	checkBank( mixer.configure( &context, pAllocator, mixerOps, 2 ) == SIR_AUDIO_OK );
	checkBank( mixer.initialize() == SIR_AUDIO_OK );
	checkBank( mixer.registerBusFromBank( 10, 999, &banks, "bus:/Master" ) == SIR_AUDIO_INVALID_HANDLE );
	checkBank( mixer.registerBusFromBank( 10, 2, &banks, "bus:/Master" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkBank( mixer.registerBusFromBank( 10, 1, &banks, "bus:/Master" ) == SIR_AUDIO_OK );
	checkBank( mixer.registerVCAFromBank( 20, 2, &banks, "vca:/Music" ) == SIR_AUDIO_OK );

	const sirFMODSnapshotNativeOps_t snapshotOps = {
	    1, &fixture, testPinnedSnapshotResolve, testVoiceCreate, testVoiceStart, testVoiceStop, testVoiceRelease };
	SIRFMODSnapshotBackend snapshots;
	checkBank( snapshots.configure( &context, pAllocator, snapshotOps, 1 ) == SIR_AUDIO_OK );
	checkBank( snapshots.initialize() == SIR_AUDIO_OK );
	checkBank( snapshots.registerSnapshotFromBank( 30, 2, &banks, "snapshot:/Storm" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkBank( snapshots.registerSnapshotFromBank( 30, 1, &banks, "snapshot:/Storm" ) == SIR_AUDIO_OK );
	checkBank( banks.getBankObjectPinCount( 1 ) == 2 && banks.getBankObjectPinCount( 2 ) == 1 );
	checkBank( context.getBorrowedBankObjectCount() == 3 && context.getUnscopedBankObjectCount() == 0 );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_BUSY );
	checkBank( banks.unloadBank( 2 ) == SIR_AUDIO_BUSY );
	checkBank( mixer.unregisterControl( 20 ) == SIR_AUDIO_OK );
	checkBank( banks.getBankObjectPinCount( 2 ) == 0 );
	checkBank( banks.unloadBank( 2 ) == SIR_AUDIO_OK );
	checkBank( banks.getLoadedBankCount() == 1 && fixture.bBankA && !fixture.bBankB );
	checkBank( snapshots.unregisterSnapshot( 30 ) == SIR_AUDIO_OK );
	checkBank( banks.getBankObjectPinCount( 1 ) == 1 );
	mixer.shutdown();
	checkBank( banks.getBankObjectPinCount( 1 ) == 0 );
	checkBank( context.getBorrowedBankObjectCount() == 0 );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_OK );
	checkBank( snapshots.shutdown() == SIR_AUDIO_OK );
	checkBank( banks.shutdown() == SIR_AUDIO_OK );
	audio.shutdown();
	checkBank( context.shutdown() == SIR_FMOD_OK );
}

static void testNonblockingBanks( const sirContainerAllocator_t *pAllocator )
{
	sirFMODBankFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkBank( context.initialize( contextConfig, makeStudioOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend voices;
	checkBank( voices.configure( &context, pAllocator, makeVoiceOps( &fixture ), 1 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t audioConfig = { 1, 0, 4, 2, 2, 0 };
	checkBank( audio.initialize( audioConfig, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &voices ) == SIR_AUDIO_OK );
	SIRFMODBankStore banks;
	checkBank( banks.initialize( &context, &voices, pAllocator, makeBankOps( &fixture ), 1, 1 ) == SIR_AUDIO_OK );
	fixture.nAsyncPolls = 2;
	checkBank( banks.beginLoadBank( 1, "A.bank" ) == SIR_AUDIO_PENDING );
	checkBank( !banks.hasLoadedBank( 1 ) );
	checkBank( banks.pollBankLoad( 1 ) == SIR_AUDIO_PENDING );
	checkBank( banks.pollBankLoad( 1 ) == SIR_AUDIO_PENDING );
	checkBank( banks.pollBankLoad( 1 ) == SIR_AUDIO_OK );
	checkBank( banks.hasLoadedBank( 1 ) && banks.preloadSamples( 1 ) == SIR_AUDIO_OK );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_OK );
	checkBank( banks.beginLoadBank( 1, "A.bank" ) == SIR_AUDIO_PENDING );
	checkBank( banks.unloadBank( 1 ) == SIR_AUDIO_OK );
	checkBank( banks.shutdown() == SIR_AUDIO_OK );
	audio.shutdown();
	checkBank( context.shutdown() == SIR_FMOD_OK );
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

typedef struct sirFMODRuntimeTestManifest_s
{
	sirAudioManifestHeader_t header;
	sirAudioPackageRecord_t packages[2];
	sirAudioEventRecord_t events[1];
	sirAudioControlRecord_t controls[1];
	uint64_t dependencies[1];
} sirFMODRuntimeTestManifest_t;

static sirFMODRuntimeTestManifest_t makeRuntimeManifest( void ) noexcept
{
	sirFMODRuntimeTestManifest_t image = {};
	image.header.nMagic = SIR_AUDIO_MANIFEST_MAGIC;
	image.header.nTotalBytes = sizeof( image );
	image.header.nPackageCount = 2;
	image.header.nEventCount = 1;
	image.header.nControlCount = 1;
	image.header.nDependencyCount = 1;
	image.header.nVersion = SIR_AUDIO_MANIFEST_VERSION;
	image.header.nHeaderBytes = sizeof( image.header );
	image.header.eProvider = SIR_AUDIO_PROVIDER_FMOD;
	image.header.nEndianness = SIR_AUDIO_MANIFEST_LITTLE_ENDIAN;
	image.packages[0] = { 1, 101, 0, 0, 0 };
	image.packages[1] = { 2, 102, 0, 0, 1 };
	image.events[0] = { 101, 1, 1001, 0 };
	image.controls[0] = { 201, 2001, 0, 0.5f, 0.0f, 1.0f, SIR_AUDIO_CONTROL_PLAYING, SIR_AUDIO_CONTROL_FLOAT, SIR_AUDIO_BINDING_NATIVE, 0 };
	image.dependencies[0] = 1;
	image.header.nContentHash = SIRAudioResourceStore::hashPayload( ( const uint8_t * )&image + sizeof( image.header.nContentHash ),
	                                                                sizeof( image ) - sizeof( image.header.nContentHash ) );
	return image;
}

static int32_t testRuntimeInstanceParameter( void *, void *, uint64_t nNativeId, float fValue ) noexcept
{
	checkBank( nNativeId == 2001 && fValue >= 0.0f && fValue <= 1.0f );
	return 0;
}

static int32_t testRuntimeGlobalParameter( void *, void *, uint64_t, float ) noexcept
{
	return 0;
}

static sirAudioResult_t testResolveEventPath( void *, uint64_t nEventId, uint64_t nProviderKey, const char **pszPath ) noexcept
{
	if ( nEventId != 101 || nProviderKey != 1001 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	*pszPath = "event:/A";
	return SIR_AUDIO_OK;
}

static sirAudioResult_t testResolveRuntimeObject( void *, sirFMODRuntimeObjectKind_t eKind, uint64_t nObjectId, uint64_t nProviderKey,
                                                  const char **pszPath ) noexcept
{
	if ( eKind == SIR_FMOD_RUNTIME_BUS && nObjectId == 10 && nProviderKey == 501 ) {
		*pszPath = "bus:/Master";
		return SIR_AUDIO_OK;
	}
	if ( eKind == SIR_FMOD_RUNTIME_VCA && nObjectId == 20 && nProviderKey == 502 ) {
		*pszPath = "vca:/Music";
		return SIR_AUDIO_OK;
	}
	if ( eKind == SIR_FMOD_RUNTIME_SNAPSHOT && nObjectId == 30 && nProviderKey == 503 ) {
		*pszPath = "snapshot:/Storm";
		return SIR_AUDIO_OK;
	}
	return SIR_AUDIO_INVALID_ARGUMENT;
}

static uint32_t s_nNativeMemoryInstallCalls = 0;
static sirAudioResult_t testNativeMemoryInstall( const sirContainerAllocator_t *pAllocator, uint64_t nBudgetBytes ) noexcept
{
	checkBank( SIRRawStorage::isValidAllocator( pAllocator ) );
	checkBank( nBudgetBytes == 16 * 1024 * 1024 );
	++s_nNativeMemoryInstallCalls;
	return SIR_AUDIO_OK;
}
static sirFMODMemoryBudgetStats_t testNativeMemoryStats( void ) noexcept
{
	return { 16 * 1024 * 1024, 4096, 8192, 4, 0 };
}

static void testRuntimeComposition( const sirContainerAllocator_t *pAllocator )
{
	sirFMODBankFixture_t fixture = {};
	SIRAudioResourceStore manifest;
	sirFMODRuntimeTestManifest_t image = makeRuntimeManifest();
	checkBank( manifest.load( &image, sizeof( image ), SIR_AUDIO_PROVIDER_FMOD, 0, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
	checkBank( manifest.getEventCount() == 1 && manifest.getControlCount() == 1 );
	sirAudioEventRecord_t event = {};
	sirAudioControlRecord_t control = {};
	checkBank( manifest.getEventAt( 0, event ) && event.nEventId == 101 );
	checkBank( manifest.getControlAt( 0, control ) && control.nControlId == 201 );
	checkBank( !manifest.getEventAt( 1, event ) && !manifest.getControlAt( 1, control ) );

	const sirFMODRuntimeObject_t objects[3] = {
	    { 10, 1, 501, SIR_FMOD_RUNTIME_BUS, {} }, { 20, 1, 502, SIR_FMOD_RUNTIME_VCA, {} }, { 30, 1, 503, SIR_FMOD_RUNTIME_SNAPSHOT, {} } };
	sirFMODRuntimeConfig_t config = {};
	config.context = { 16, -1, 1, 1, 0, 0 };
	config.audio = { 1, 0, 8, 4, 4, 0 };
	config.pAllocator = pAllocator;
	config.pManifest = &manifest;
	config.packageResolver = { nullptr, testResolvePackagePath };
	config.eventResolver = { nullptr, testResolveEventPath };
	config.objectResolver = { nullptr, testResolveRuntimeObject };
	config.pObjects = objects;
	config.nObjectCount = 3;
	config.nMaxRegisteredEvents = 1;
	config.nMaxRegisteredParameters = 1;
	config.nMaxEmitterParameters = 0;
	config.nMaxBanks = 2;
	config.nMaxBankEventBindings = 1;
	config.nMaxMixerControls = 2;
	config.nMaxSnapshots = 1;
	config.nNativeMemoryBudgetBytes = 16 * 1024 * 1024;
	sirFMODRuntimeNativeOps_t ops = {};
	ops.context = makeStudioOps( &fixture );
	ops.voice = makeVoiceOps( &fixture );
	ops.voice.setInstanceParameter = testRuntimeInstanceParameter;
	ops.voice.setGlobalParameter = testRuntimeGlobalParameter;
	ops.bank = makeBankOps( &fixture );
	ops.mixer = { 1, &fixture, testPinnedBusResolve, testPinnedVCAResolve, testPinnedSetVolume, testPinnedSetMute, testPinnedSetVolume };
	ops.snapshot = { 1, &fixture, testPinnedSnapshotResolve, testVoiceCreate, testVoiceStart, testVoiceStop, testVoiceRelease };
	ops.memory = { 1, testNativeMemoryInstall, testNativeMemoryStats };

	SIRFMODRuntime runtime;
	checkBank( runtime.initialize( config, ops ) == SIR_AUDIO_OK );
	checkBank( s_nNativeMemoryInstallCalls == 1 );
	checkBank( runtime.getNativeMemoryStats().nCurrentBytes == 4096 );
	checkBank( runtime.isReady() );
	checkBank( runtime.getMemoryStats().nCurrentBytes > 0 );
	checkBank( runtime.getMemoryStats().nPeakBytes >= runtime.getMemoryStats().nCurrentBytes );
	checkBank( runtime.beginLoadPackage( 2, true ) == SIR_AUDIO_PENDING );
	checkBank( runtime.getPendingPackageId() == 2 );
	checkBank( runtime.pollLoadPackage() == SIR_AUDIO_PENDING );
	checkBank( runtime.getBanks().getLoadedBankCount() == 1 && !runtime.getBanks().hasLoadedBank( 1 ) );
	checkBank( runtime.cancelLoadPackage() == SIR_AUDIO_CANCELLED );
	checkBank( runtime.getPendingPackageId() == 0 && runtime.getBanks().getLoadedBankCount() == 0 );
	checkBank( runtime.getBanks().getBoundEventCount() == 0 );
	fixture.nAsyncPolls = 1;
	checkBank( runtime.beginLoadPackage( 2, true ) == SIR_AUDIO_PENDING );
	checkBank( runtime.loadPackage( 2, false ) == SIR_AUDIO_BUSY );
	sirAudioResult_t eAsyncResult = SIR_AUDIO_PENDING;
	for ( uint32_t iAttempt = 0; iAttempt < 16 && eAsyncResult == SIR_AUDIO_PENDING; ++iAttempt ) {
		runtime.update();
		eAsyncResult = runtime.pollLoadPackage();
	}
	checkBank( eAsyncResult == SIR_AUDIO_OK );
	checkBank( runtime.getPendingPackageId() == 0 && runtime.getBanks().getBoundEventCount() == 1 );
	checkBank( runtime.getBanks().getLoadedBankCount() == 2 && fixture.nPreloads >= 2 );
	checkBank( runtime.releasePackage( 2 ) == SIR_AUDIO_OK );
	checkBank( runtime.getBanks().getLoadedBankCount() == 0 );

	checkBank( runtime.loadPackage( 2, false ) == SIR_AUDIO_OK );
	checkBank( runtime.getBanks().getLoadedBankCount() == 2 && runtime.getBanks().getBoundEventCount() == 1 );
	checkBank( runtime.getContext().getBorrowedBankObjectCount() == 3 );
	checkBank( runtime.loadPackage( 2, false ) == SIR_AUDIO_OK );
	checkBank( runtime.getBanks().getBoundEventCount() == 1 );
	checkBank( runtime.releasePackage( 2 ) == SIR_AUDIO_OK );
	checkBank( runtime.getBanks().getLoadedBankCount() == 2 );
	checkBank( runtime.getAudio().setBusVolume( 10, 0.25f ) == SIR_AUDIO_PENDING );
	checkBank( runtime.getAudio().setVCAVolume( 20, 0.75f ) == SIR_AUDIO_PENDING );
	checkBank( runtime.update() == 2 );
	sirAudioPlayingHandle_t playing = {};
	checkBank( runtime.getAudio().playEvent( 101, {}, playing ) == SIR_AUDIO_PENDING );
	checkBank( runtime.update() == 1 );
	checkBank( runtime.getAudio().setParameter( playing, 201, 0.75f ) == SIR_AUDIO_PENDING );
	checkBank( runtime.update() == 1 );
	checkBank( runtime.releasePackage( 2 ) == SIR_AUDIO_BUSY );
	checkBank( runtime.getBanks().getLoadedBankCount() >= 1 );
	checkBank( runtime.getAudio().stopEvent( playing, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	checkBank( runtime.update() == 1 );
	checkBank( runtime.collectUnused() == SIR_AUDIO_OK );
	checkBank( runtime.getBanks().getLoadedBankCount() == 0 && runtime.getContext().getBorrowedBankObjectCount() == 0 );
	checkBank( runtime.shutdown() == SIR_AUDIO_OK );
	checkBank( runtime.getMemoryStats().nCurrentBytes == 0 );
	checkBank( !runtime.isReady() && !fixture.bStudio );
	manifest.shutdown();
}

static sirFMODThreadResult_t testWaitForTicket( SIRFMODAudioThread &thread, uint64_t nTicket )
{
	sirFMODThreadResult_t result = {};
	for ( uint32_t iAttempt = 0; iAttempt < 10000; ++iAttempt ) {
		const sirAudioResult_t eStatus = thread.poll( nTicket, result );
		if ( eStatus == SIR_AUDIO_OK ) {
			checkBank( result.nTicket == nTicket );
			return result;
		}
		checkBank( eStatus == SIR_AUDIO_PENDING );
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	abort();
}

static void testAudioOwnerThread( const sirContainerAllocator_t *pAllocator )
{
	sirFMODBankFixture_t fixture = {};
	SIRAudioResourceStore manifest;
	sirFMODRuntimeTestManifest_t image = makeRuntimeManifest();
	checkBank( manifest.load( &image, sizeof( image ), SIR_AUDIO_PROVIDER_FMOD, 0, pAllocator ) == SIR_AUDIO_MANIFEST_OK );
	sirFMODAudioThreadConfig_t config = {};
	config.runtime.context = { 16, -1, 0, 1, 0, 0 };
	config.runtime.audio = { 4, 0, 64, 16, 16, 0 };
	config.runtime.pAllocator = pAllocator;
	config.runtime.pManifest = &manifest;
	config.runtime.packageResolver = { nullptr, testResolvePackagePath };
	config.runtime.eventResolver = { nullptr, testResolveEventPath };
	config.runtime.nMaxRegisteredEvents = 1;
	config.runtime.nMaxRegisteredParameters = 1;
	config.runtime.nMaxBanks = 2;
	config.runtime.nMaxBankEventBindings = 1;
	config.runtime.nMaxMixerControls = 1;
	config.runtime.nMaxSnapshots = 1;
	config.runtime.nEngineMemoryBudgetBytes = 1024 * 1024;
	config.runtime.nNativeMemoryBudgetBytes = 16 * 1024 * 1024;
	config.nativeOps.context = makeStudioOps( &fixture );
	config.nativeOps.voice = makeVoiceOps( &fixture );
	config.nativeOps.voice.setInstanceParameter = testRuntimeInstanceParameter;
	config.nativeOps.voice.setGlobalParameter = testRuntimeGlobalParameter;
	config.nativeOps.bank = makeBankOps( &fixture );
	config.nativeOps.memory = { 1, testNativeMemoryInstall, testNativeMemoryStats };
	config.nativeOps.mixer = {
	    1, &fixture, testPinnedBusResolve, testPinnedVCAResolve, testPinnedSetVolume, testPinnedSetMute, testPinnedSetVolume };
	config.nativeOps.snapshot = {
	    1, &fixture, testPinnedSnapshotResolve, testVoiceCreate, testVoiceStart, testVoiceStop, testVoiceRelease };
	config.nRequestCapacity = 64;
	config.nIdleSleepMilliseconds = 1;
	SIRFMODAudioThread worker;
	fixture.bTriggerDeviceLost = true;
	checkBank( worker.start( config ) == SIR_AUDIO_OK && worker.isReady() );
	for ( uint32_t iAttempt = 0; iAttempt < 1000 && worker.getAutoRecoveryCount() == 0; ++iAttempt ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	checkBank( worker.getAutoRecoveryCount() == 1 );
	checkBank( worker.getAutoRecoveryFailureCount() == 0 );
	const sirFMODMemoryBudgetStats_t engineStats = worker.getEngineMemoryStats();
	checkBank( engineStats.nBudgetBytes == 1024 * 1024 && engineStats.nCurrentBytes > 0 );
	checkBank( engineStats.nCurrentBytes <= engineStats.nPeakBytes );
	const sirFMODMemoryBudgetStats_t nativeStats = worker.getNativeMemoryStats();
	checkBank( nativeStats.nBudgetBytes == 16 * 1024 * 1024 && nativeStats.nCurrentBytes == 4096 );

	// Hold an already-dispatched Core output call. It cannot be
	// interrupted and therefore must reject cancellation with BUSY.
	s_nDriverEntered.store( 0, std::memory_order_relaxed );
	s_nDriverGate.store( 1, std::memory_order_release );
	sirFMODThreadRequest_t recovery = {};
	recovery.eOperation = SIR_FMOD_THREAD_RECOVER_OUTPUT;
	recovery.iDriver = 0;
	recovery.bFlag = 1;
	uint64_t nRecoveryTicket = 0;
	checkBank( worker.submit( recovery, nRecoveryTicket ) == SIR_AUDIO_PENDING );
	for ( uint32_t iAttempt = 0; iAttempt < 1000 && s_nDriverEntered.load( std::memory_order_acquire ) == 0;
	      ++iAttempt ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
	const bool bNativeEntered = s_nDriverEntered.load( std::memory_order_acquire ) != 0;
	const sirAudioResult_t eCancelRecovery = bNativeEntered ? worker.cancel( nRecoveryTicket ) : SIR_AUDIO_INVALID_HANDLE;
	s_nDriverGate.store( 0, std::memory_order_release );
	checkBank( bNativeEntered && eCancelRecovery == SIR_AUDIO_BUSY );
	const sirFMODThreadResult_t recoveryResult = testWaitForTicket( worker, nRecoveryTicket );
	checkBank( recoveryResult.eResult == SIR_AUDIO_OK );

	sirFMODThreadRequest_t request = {};
	request.eOperation = SIR_FMOD_THREAD_LOAD_PACKAGE;
	request.nId = 2;
	request.bFlag = 1;
	fixture.nAsyncPolls = 4;
	uint64_t nLoadTicket = 0;
	checkBank( worker.submit( request, nLoadTicket ) == SIR_AUDIO_PENDING && nLoadTicket != 0 );
	sirFMODThreadResult_t result = testWaitForTicket( worker, nLoadTicket );
	checkBank( result.eResult == SIR_AUDIO_OK );
	checkBank( worker.poll( nLoadTicket, result ) == SIR_AUDIO_INVALID_HANDLE );
	std::atomic<uint32_t> nCompletedStress( 0 );
	std::thread producers[4];
	for ( uint32_t iProducer = 0; iProducer < 4; ++iProducer ) {
		producers[iProducer] = std::thread( [&worker, &nCompletedStress]() {
			uint64_t nPreviousTicket = 0;
			for ( uint32_t iRequest = 0; iRequest < 128; ++iRequest ) {
				sirFMODThreadRequest_t query = {};
				query.eOperation = SIR_FMOD_THREAD_QUERY_PLAYBACK;
				uint64_t nTicket = 0;
				sirAudioResult_t eSubmit = SIR_AUDIO_CAPACITY;
				for ( uint32_t iTry = 0; iTry < 1000; ++iTry ) {
					eSubmit = worker.submit( query, nTicket );
					if ( eSubmit != SIR_AUDIO_CAPACITY ) {
						break;
					}
					std::this_thread::yield();
				}
				checkBank( eSubmit == SIR_AUDIO_PENDING );
				if ( nPreviousTicket != 0 ) {
					checkBank( worker.cancel( nPreviousTicket ) == SIR_AUDIO_INVALID_HANDLE );
				}
				const sirFMODThreadResult_t answer = testWaitForTicket( worker, nTicket );
				checkBank( answer.eResult == SIR_AUDIO_INVALID_HANDLE );
				nPreviousTicket = nTicket;
				++nCompletedStress;
			}
		} );
	}
	for ( std::thread &producer : producers ) {
		producer.join();
	}
	checkBank( nCompletedStress.load() == 512 );
	request = {};
	request.eOperation = SIR_FMOD_THREAD_PLAY_EVENT;
	request.nId = 101;
	uint64_t nPlayTicket = 0;
	checkBank( worker.submit( request, nPlayTicket ) == SIR_AUDIO_PENDING );
	result = testWaitForTicket( worker, nPlayTicket );
	checkBank( result.eResult == SIR_AUDIO_PENDING && result.hPlaying.nEpoch != 0 );
	const sirAudioPlayingHandle_t hPlaying = result.hPlaying;
	request = {};
	request.eOperation = SIR_FMOD_THREAD_STOP_EVENT;
	request.hPlaying = hPlaying;
	request.eStopMode = SIR_AUDIO_STOP_IMMEDIATE;
	uint64_t nStopTicket = 0;
	checkBank( worker.submit( request, nStopTicket ) == SIR_AUDIO_PENDING );
	result = testWaitForTicket( worker, nStopTicket );
	checkBank( result.eResult == SIR_AUDIO_PENDING );
	request = {};
	request.eOperation = SIR_FMOD_THREAD_RELEASE_PACKAGE;
	request.nId = 2;
	result.eResult = SIR_AUDIO_BUSY;
	for ( uint32_t iAttempt = 0; iAttempt < 100 && result.eResult == SIR_AUDIO_BUSY; ++iAttempt ) {
		uint64_t nReleaseTicket = 0;
		checkBank( worker.submit( request, nReleaseTicket ) == SIR_AUDIO_PENDING );
		result = testWaitForTicket( worker, nReleaseTicket );
		if ( result.eResult == SIR_AUDIO_BUSY ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		}
	}
	checkBank( result.eResult == SIR_AUDIO_OK );
	request = {};
	request.eOperation = SIR_FMOD_THREAD_LOAD_PACKAGE;
	request.nId = 2;
	fixture.nAsyncPolls = 1000000;
	uint64_t nCancelledTicket = 0;
	checkBank( worker.submit( request, nCancelledTicket ) == SIR_AUDIO_PENDING );
	std::this_thread::sleep_for( std::chrono::milliseconds( 8 ) );
	// A pending bank load must not block control/query traffic.
	sirFMODThreadRequest_t query = {};
	query.eOperation = SIR_FMOD_THREAD_QUERY_PLAYBACK;
	uint64_t nQueryTicket = 0;
	checkBank( worker.submit( query, nQueryTicket ) == SIR_AUDIO_PENDING );
	result = testWaitForTicket( worker, nQueryTicket );
	checkBank( result.eResult == SIR_AUDIO_INVALID_HANDLE );
	checkBank( worker.cancel( nQueryTicket ) == SIR_AUDIO_INVALID_HANDLE );
	// Competing root-load requests must fail without consuming the active
	// transaction's completion ticket or blocking ordinary dispatch.
	uint64_t nSecondLoadTicket = 0;
	checkBank( worker.submit( request, nSecondLoadTicket ) == SIR_AUDIO_PENDING );
	result = testWaitForTicket( worker, nSecondLoadTicket );
	checkBank( result.eResult == SIR_AUDIO_BUSY );
	checkBank( worker.cancel( nCancelledTicket ) == SIR_AUDIO_OK );
	result = testWaitForTicket( worker, nCancelledTicket );
	checkBank( result.eResult == SIR_AUDIO_CANCELLED );
	checkBank( worker.getDroppedLifecycleCount() <= 2 );
	checkBank( worker.shutdown() == SIR_AUDIO_OK );
	checkBank( !worker.isReady() );
	checkBank( worker.getEngineMemoryStats().nCurrentBytes == 0 );
	checkBank( worker.getNativeMemoryStats().nCurrentBytes == 0 );
	manifest.shutdown();
}

int main( void )
{
	const sirContainerAllocator_t allocator = { nullptr, testBankAllocate, testBankFree, 0, 0 };
	testBankLifecycle( &allocator );
	testNonblockingBanks( &allocator );
	testBankFailures( &allocator );
	testBorrowedBankObjectPins( &allocator );
	testScopedBankOwnership( &allocator );
	testManifestPackageLoading( &allocator );
	testRuntimeComposition( &allocator );
	testAudioOwnerThread( &allocator );
	checkBank( s_nAllocatedBlocks == 0 );
	return 0;
}
