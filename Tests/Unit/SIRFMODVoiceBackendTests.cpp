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

#include <Audio/FMOD/SIRFMODVoiceBackend.h>
#include <Audio/SIRAudioSystem.h>
#include <stdlib.h>
#include <string.h>

static void checkVoice( bool bValid ) noexcept
{
	if ( !bValid ) {
		abort();
	}
}

static uint32_t s_nAllocations = 0;
static void *testVoiceAllocate( void *, size_t nBytes, size_t ) noexcept
{
	void *pMemory = malloc( nBytes );
	if ( pMemory != nullptr ) {
		++s_nAllocations;
	}
	return pMemory;
}

static void testVoiceDeallocate( void *, void *pMemory, size_t, size_t ) noexcept
{
	if ( pMemory != nullptr ) {
		--s_nAllocations;
		free( pMemory );
	}
}

typedef struct sirFMODTestInstance_s
{
	uint64_t nEventId;
	bool bUsed;
	bool bStopped;
} sirFMODTestInstance_t;

typedef struct sirFMODVoiceFixture_s
{
	sirFMODTestInstance_t instances[8] = {};
	uint32_t nCreateCalls = 0;
	uint32_t nStartCalls = 0;
	uint32_t nStopCalls = 0;
	uint32_t nReleaseCalls = 0;
	uint32_t nUpdateCalls = 0;
	uint32_t nResolveCalls = 0;
	uint32_t nInstanceParameterCalls = 0;
	uint32_t nGlobalParameterCalls = 0;
	uint64_t nLastParameterId = 0;
	float fLastParameterValue = 0.0f;
	uint32_t nFailParameter = 0;
	uint32_t nEmitterSpatialCalls = 0;
	uint32_t nListenerSpatialCalls = 0;
	uint32_t nListenerCountCalls = 0;
	uint32_t nListenerWeightCalls = 0;
	uint32_t nFailSpatial = 0;
	uint32_t iLastListener = UINT32_MAX;
	float fListenerWeight = -1.0f;
	sirAudioAttributes_t lastSpatial = {};
	uint32_t nFailResolve = 0;
	uint32_t nFailCreate = 0;
	uint32_t nFailStart = 0;
	uint32_t nFailStop = 0;
	uint32_t nFailRelease = 0;
	uint32_t nFailPoll = 0;
	bool bStudioOwned = false;
} sirFMODVoiceFixture_t;

static int32_t testContextCreate( void *pData, void **ppStudio ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	pFixture->bStudioOwned = true;
	*ppStudio = pFixture;
	return 0;
}
static int32_t testContextGetCore( void *, void *pStudio, void **ppCore ) noexcept
{
	*ppCore = pStudio;
	return 0;
}
static int32_t testContextVersion( void *, void *, uint32_t *pVersion ) noexcept
{
	*pVersion = 0x00020400;
	return 0;
}
static int32_t testContextGetDrivers( void *, void *, int32_t *pCount ) noexcept
{
	*pCount = 1;
	return 0;
}
static int32_t testContextSetDriver( void *, void *, int32_t ) noexcept
{
	return 0;
}
static int32_t testContextNoSound( void *, void * ) noexcept
{
	return 0;
}
static int32_t testContextInitialize( void *, void *, const sirFMODContextConfig_t * ) noexcept
{
	return 0;
}
static int32_t testContextUpdate( void *pData, void * ) noexcept
{
	++( ( sirFMODVoiceFixture_t * )pData )->nUpdateCalls;
	return 0;
}
static int32_t testContextRelease( void *pData, void * ) noexcept
{
	( ( sirFMODVoiceFixture_t * )pData )->bStudioOwned = false;
	return 0;
}

static sirFMODNativeOps_t makeContextOps( sirFMODVoiceFixture_t *pFixture ) noexcept
{
	return { 1,
	         0x00020400,
	         pFixture,
	         testContextCreate,
	         testContextGetCore,
	         testContextVersion,
	         testContextGetDrivers,
	         testContextSetDriver,
	         testContextNoSound,
	         testContextInitialize,
	         testContextUpdate,
	         testContextRelease };
}

static int32_t testResolve( void *pData, void *pStudio, const char *szPath, void **ppDescription ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	checkVoice( pStudio == pFixture );
	++pFixture->nResolveCalls;
	if ( pFixture->nFailResolve != 0 ) {
		--pFixture->nFailResolve;
		return -41;
	}
	if ( strcmp( szPath, "event:/A" ) == 0 ) {
		*ppDescription = &pFixture->instances[6];
	} else if ( strcmp( szPath, "event:/B" ) == 0 ) {
		*ppDescription = &pFixture->instances[7];
	} else {
		return -42;
	}
	return 0;
}
static int32_t testCreateInstance( void *pData, void *pDescription, void **ppInstance ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	checkVoice( pDescription == &pFixture->instances[6] || pDescription == &pFixture->instances[7] );
	++pFixture->nCreateCalls;
	if ( pFixture->nFailCreate != 0 ) {
		--pFixture->nFailCreate;
		return -43;
	}
	for ( uint32_t i = 0; i < 6; ++i ) {
		if ( !pFixture->instances[i].bUsed ) {
			pFixture->instances[i].bUsed = true;
			pFixture->instances[i].bStopped = false;
			*ppInstance = &pFixture->instances[i];
			return 0;
		}
	}
	return -44;
}
static int32_t testStart( void *pData, void * ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	++pFixture->nStartCalls;
	if ( pFixture->nFailStart != 0 ) {
		--pFixture->nFailStart;
		return -45;
	}
	return 0;
}
static int32_t testStop( void *pData, void *pInstance, sirAudioStopMode_t eMode ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	++pFixture->nStopCalls;
	if ( pFixture->nFailStop != 0 ) {
		--pFixture->nFailStop;
		return -46;
	}
	if ( eMode == SIR_AUDIO_STOP_IMMEDIATE ) {
		( ( sirFMODTestInstance_t * )pInstance )->bStopped = true;
	}
	return 0;
}
static int32_t testIsStopped( void *pData, void *pInstance, uint8_t *pbStopped ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	if ( pFixture->nFailPoll != 0 ) {
		--pFixture->nFailPoll;
		return -47;
	}
	*pbStopped = ( ( sirFMODTestInstance_t * )pInstance )->bStopped ? 1 : 0;
	return 0;
}
static int32_t testRelease( void *pData, void *pInstance ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	++pFixture->nReleaseCalls;
	if ( pFixture->nFailRelease != 0 ) {
		--pFixture->nFailRelease;
		return -48;
	}
	( ( sirFMODTestInstance_t * )pInstance )->bUsed = false;
	return 0;
}
static sirFMODVoiceNativeOps_t makeVoiceOps( sirFMODVoiceFixture_t *pFixture ) noexcept
{
	return { 3,       pFixture, testResolve, testCreateInstance, testStart, testStop, testIsStopped, testRelease, nullptr, nullptr, nullptr,
	         nullptr, nullptr,  nullptr };
}

static int32_t testInstanceParameter( void *pData, void *pInstance, uint64_t nNativeId, float fValue ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	checkVoice( pInstance != nullptr );
	++pFixture->nInstanceParameterCalls;
	pFixture->nLastParameterId = nNativeId;
	pFixture->fLastParameterValue = fValue;
	if ( pFixture->nFailParameter != 0 ) {
		--pFixture->nFailParameter;
		return -55;
	}
	return 0;
}

static int32_t testGlobalParameter( void *pData, void *pStudio, uint64_t nNativeId, float fValue ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	checkVoice( pStudio == pFixture );
	++pFixture->nGlobalParameterCalls;
	pFixture->nLastParameterId = nNativeId;
	pFixture->fLastParameterValue = fValue;
	if ( pFixture->nFailParameter != 0 ) {
		--pFixture->nFailParameter;
		return -56;
	}
	return 0;
}

static int32_t testEmitterSpatial( void *pData, void *pInstance, const sirAudioAttributes_t *pAttributes ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	checkVoice( pInstance != nullptr );
	++pFixture->nEmitterSpatialCalls;
	if ( pFixture->nFailSpatial != 0 ) {
		--pFixture->nFailSpatial;
		return -71;
	}
	pFixture->lastSpatial = *pAttributes;
	return 0;
}

static int32_t testListenerSpatial( void *pData, void *pStudio, uint32_t iListener, const sirAudioAttributes_t *pAttributes ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	checkVoice( pStudio == pFixture );
	++pFixture->nListenerSpatialCalls;
	pFixture->iLastListener = iListener;
	pFixture->lastSpatial = *pAttributes;
	return 0;
}

static int32_t testListenerCount( void *pData, void *pStudio, uint32_t nListeners ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	checkVoice( pStudio == pFixture && nListeners == 1 );
	++pFixture->nListenerCountCalls;
	return 0;
}

static int32_t testListenerWeight( void *pData, void *pStudio, uint32_t iListener, float fWeight ) noexcept
{
	sirFMODVoiceFixture_t *pFixture = ( sirFMODVoiceFixture_t * )pData;
	checkVoice( pStudio == pFixture && iListener == 0 );
	++pFixture->nListenerWeightCalls;
	pFixture->fListenerWeight = fWeight;
	return 0;
}

static void testParameterDispatch( const sirContainerAllocator_t *pAllocator )
{
	sirFMODVoiceFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkVoice( context.initialize( contextConfig, makeContextOps( &fixture ) ) == SIR_FMOD_OK );
	sirFMODVoiceNativeOps_t nativeOps = makeVoiceOps( &fixture );
	nativeOps.setInstanceParameter = testInstanceParameter;
	nativeOps.setGlobalParameter = testGlobalParameter;
	SIRFMODVoiceBackend backend;
	checkVoice( backend.configure( &context, pAllocator, nativeOps, 1, 2 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 1, 0, 8, 4, 2, 0 };
	checkVoice( audio.initialize( config, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &backend ) == SIR_AUDIO_OK );
	checkVoice( backend.registerEvent( 10, "event:/A" ) == SIR_AUDIO_OK );
	checkVoice( backend.registerParameter( 200, 0x1111222233334444ULL, SIR_AUDIO_CONTROL_GLOBAL ) == SIR_AUDIO_OK );
	checkVoice( backend.registerParameter( 100, 0x5555666677778888ULL, SIR_AUDIO_CONTROL_PLAYING ) == SIR_AUDIO_OK );
	checkVoice( backend.registerParameter( 100, 9, SIR_AUDIO_CONTROL_PLAYING ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkVoice( backend.registerParameter( 300, 9, SIR_AUDIO_CONTROL_PLAYING ) == SIR_AUDIO_CAPACITY );
	checkVoice( backend.registerParameter( 0, 9, SIR_AUDIO_CONTROL_PLAYING ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkVoice( backend.registerParameter( 400, 0, SIR_AUDIO_CONTROL_GLOBAL ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkVoice( backend.registerParameter( 400, 9, SIR_AUDIO_CONTROL_EMITTER ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkVoice( audio.setGlobalParameter( 200, 0.25f ) == SIR_AUDIO_PENDING );
	audio.update();
	checkVoice( fixture.nGlobalParameterCalls == 1 );
	checkVoice( fixture.nLastParameterId == 0x1111222233334444ULL && fixture.fLastParameterValue == 0.25f );
	sirAudioPlayingHandle_t playing = {};
	checkVoice( audio.playEvent( 10, {}, playing ) == SIR_AUDIO_PENDING );
	audio.update();
	checkVoice( audio.setParameter( playing, 100, 0.75f ) == SIR_AUDIO_PENDING );
	audio.update();
	checkVoice( fixture.nInstanceParameterCalls == 1 );
	checkVoice( fixture.nLastParameterId == 0x5555666677778888ULL && fixture.fLastParameterValue == 0.75f );
	checkVoice( backend.getActiveVoiceCount() == 1 );
	const sirAudioBackendOps_t backendOps = SIRFMODVoiceBackend::getBackendOps();
	sirAudioCommand_t invalid = {};
	invalid.eType = SIR_AUDIO_COMMAND_SET_PARAMETER;
	invalid.hPlaying = playing;
	invalid.nControlId = 200;
	checkVoice( backendOps.submit( &backend, &invalid, 1 ) == SIR_AUDIO_UNSUPPORTED );
	invalid.eType = SIR_AUDIO_COMMAND_SET_GLOBAL_PARAMETER;
	invalid.nControlId = 100;
	checkVoice( backendOps.submit( &backend, &invalid, 1 ) == SIR_AUDIO_UNSUPPORTED );
	checkVoice( fixture.nGlobalParameterCalls == 1 && fixture.nInstanceParameterCalls == 1 );
	fixture.nFailParameter = 1;
	sirAudioCommand_t failed = {};
	failed.eType = SIR_AUDIO_COMMAND_SET_PARAMETER;
	failed.hPlaying = playing;
	failed.nControlId = 100;
	failed.fValue = 0.5f;
	checkVoice( backendOps.submit( &backend, &failed, 1 ) == SIR_AUDIO_BACKEND_ERROR );
	checkVoice( backend.getLastNativeResult() == -55 );
	checkVoice( backend.getActiveVoiceCount() == 1 );
	checkVoice( backend.unregisterParameter( 100 ) == SIR_AUDIO_OK );
	checkVoice( backend.unregisterParameter( 100 ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkVoice( backend.unregisterParameter( 200 ) == SIR_AUDIO_OK );
	checkVoice( audio.stopEvent( playing, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	audio.update();
	audio.shutdown();
	checkVoice( context.shutdown() == SIR_FMOD_OK );
}

static void testSpatialDriver( const sirContainerAllocator_t *pAllocator )
{
	sirFMODVoiceFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkVoice( context.initialize( contextConfig, makeContextOps( &fixture ) ) == SIR_FMOD_OK );
	sirFMODVoiceNativeOps_t nativeOps = makeVoiceOps( &fixture );
	nativeOps.setInstanceSpatial = testEmitterSpatial;
	nativeOps.setListenerSpatial = testListenerSpatial;
	nativeOps.setListenerCount = testListenerCount;
	nativeOps.setListenerWeight = testListenerWeight;
	SIRFMODVoiceBackend backend;
	checkVoice( backend.configure( &context, pAllocator, nativeOps, 1, 0, 2, 1 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 2, 2, 8, 4, 4, 1 };
	checkVoice( audio.initialize( config, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &backend ) == SIR_AUDIO_OK );
	checkVoice( fixture.nListenerCountCalls == 1 );
	checkVoice( fixture.nListenerWeightCalls == 1 && fixture.fListenerWeight == 0.0f );
	checkVoice( backend.registerEvent( 10, "event:/A" ) == SIR_AUDIO_OK );
	sirAudioEmitterHandle_t emitter = {};
	sirAudioListenerHandle_t listener = {};
	checkVoice( audio.registerEmitter( emitter ) == SIR_AUDIO_OK );
	checkVoice( audio.registerListener( listener ) == SIR_AUDIO_OK );
	sirAudioAttributes_t attributes = {};
	attributes.fPosition[0] = 19.0f;
	attributes.fForward[2] = -1.0f;
	attributes.fUp[1] = 1.0f;
	checkVoice( audio.setEmitterAttributes( emitter, attributes ) == SIR_AUDIO_PENDING );
	checkVoice( audio.setListenerAttributes( listener, attributes ) == SIR_AUDIO_PENDING );
	audio.update();
	checkVoice( fixture.nListenerSpatialCalls == 1 && fixture.nListenerWeightCalls == 2 );
	checkVoice( fixture.fListenerWeight == 1.0f );
	sirAudioPlayingHandle_t playing = {};
	checkVoice( audio.playEvent( 10, emitter, playing ) == SIR_AUDIO_PENDING );
	audio.update();
	checkVoice( fixture.nEmitterSpatialCalls == 1 && fixture.lastSpatial.fPosition[0] == 19.0f );
	checkVoice( audio.releaseEmitter( emitter ) == SIR_AUDIO_BUSY );
	attributes.fPosition[0] = 25.0f;
	fixture.nFailSpatial = 1;
	checkVoice( audio.setEmitterAttributes( emitter, attributes ) == SIR_AUDIO_PENDING );
	audio.update();
	checkVoice( fixture.nEmitterSpatialCalls >= 2 );
	audio.update();
	checkVoice( fixture.lastSpatial.fPosition[0] == 25.0f );
	checkVoice( audio.stopEvent( playing, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	audio.update();
	checkVoice( audio.releaseEmitter( emitter ) == SIR_AUDIO_PENDING );
	checkVoice( audio.releaseListener( listener ) == SIR_AUDIO_PENDING );
	audio.update();
	checkVoice( fixture.fListenerWeight == 0.0f );
	checkVoice( fixture.nListenerWeightCalls >= 3 );
	audio.shutdown();
	checkVoice( context.shutdown() == SIR_FMOD_OK );
}

static void testVoiceLifecycle( const sirContainerAllocator_t *pAllocator )
{
	sirFMODVoiceFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkVoice( context.initialize( contextConfig, makeContextOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend backend;
	checkVoice( backend.configure( &context, pAllocator, makeVoiceOps( &fixture ), 2 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 2, 0, 8, 4, 4, 0 };
	checkVoice( audio.initialize( config, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &backend ) == SIR_AUDIO_OK );
	checkVoice( backend.registerEvent( 10, "event:/A" ) == SIR_AUDIO_OK );
	checkVoice( backend.registerEvent( 5, "event:/B" ) == SIR_AUDIO_OK );
	checkVoice( backend.registerEvent( 10, "event:/A" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkVoice( backend.registerEvent( 20, "event:/A" ) == SIR_AUDIO_CAPACITY );
	checkVoice( backend.unregisterEvent( 888 ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkVoice( backend.unregisterEvent( 5 ) == SIR_AUDIO_OK );
	checkVoice( backend.registerEvent( 5, "event:/B" ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t first = {};
	checkVoice( audio.playEvent( 10, {}, first ) == SIR_AUDIO_PENDING );
	checkVoice( backend.getActiveVoiceCount() == 0 );
	checkVoice( audio.update() == 1 );
	checkVoice( backend.getActiveVoiceCount() == 1 );
	sirAudioPlaybackState_t eState = SIR_AUDIO_PLAYBACK_FREE;
	checkVoice( audio.getPlaybackState( first, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	checkVoice( backend.unregisterEvent( 10 ) == SIR_AUDIO_BUSY );
	fixture.instances[0].bStopped = true;
	checkVoice( audio.update() == 0 );
	checkVoice( backend.getActiveVoiceCount() == 0 );
	checkVoice( audio.getPlaybackState( first, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FINISHED );
	checkVoice( backend.unregisterEvent( 10 ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t missing = {};
	checkVoice( audio.playEvent( 10, {}, missing ) == SIR_AUDIO_PENDING );
	checkVoice( audio.update() == 1 );
	checkVoice( audio.getPlaybackState( missing, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FAILED );
	checkVoice( backend.registerEvent( 10, "event:/A" ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t next = {};
	checkVoice( audio.playEvent( 5, {}, next ) == SIR_AUDIO_PENDING );
	checkVoice( audio.update() == 1 );
	checkVoice( backend.unregisterEvent( 5 ) == SIR_AUDIO_BUSY );
	fixture.nFailStop = 1;
	checkVoice( audio.stopEvent( next, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	checkVoice( audio.update() == 1 );
	checkVoice( audio.getPlaybackState( next, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	fixture.nFailRelease = 1;
	checkVoice( audio.stopEvent( next, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	checkVoice( audio.update() == 1 );
	checkVoice( backend.getActiveVoiceCount() == 1 );
	checkVoice( audio.getPlaybackState( next, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_STOPPING );
	checkVoice( audio.update() == 0 );
	checkVoice( backend.getActiveVoiceCount() == 0 );
	checkVoice( audio.getPlaybackState( next, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FINISHED );
	checkVoice( backend.unregisterEvent( 5 ) == SIR_AUDIO_OK );
	checkVoice( fixture.nCreateCalls == 2 );
	checkVoice( fixture.nStartCalls == 2 );
	audio.shutdown();
	checkVoice( context.shutdown() == SIR_FMOD_OK );
	checkVoice( !fixture.bStudioOwned );
}

static void testVoiceFailures( const sirContainerAllocator_t *pAllocator )
{
	sirFMODVoiceFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkVoice( context.initialize( contextConfig, makeContextOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend backend;
	checkVoice( backend.configure( &context, pAllocator, makeVoiceOps( &fixture ), 2 ) == SIR_AUDIO_OK );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 2, 0, 8, 4, 4, 0 };
	checkVoice( audio.initialize( config, pAllocator, SIRFMODVoiceBackend::getBackendOps(), &backend ) == SIR_AUDIO_OK );
	fixture.nFailResolve = 1;
	checkVoice( backend.registerEvent( 10, "event:/A" ) == SIR_AUDIO_BACKEND_ERROR );
	checkVoice( backend.registerEvent( 10, "event:/A" ) == SIR_AUDIO_OK );
	fixture.nFailCreate = 1;
	sirAudioPlayingHandle_t playing = {};
	sirAudioPlaybackState_t eState = SIR_AUDIO_PLAYBACK_FREE;
	checkVoice( audio.playEvent( 10, {}, playing ) == SIR_AUDIO_PENDING );
	checkVoice( audio.update() == 1 );
	checkVoice( audio.getPlaybackState( playing, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FAILED );
	fixture.nFailStart = 1;
	checkVoice( audio.playEvent( 10, {}, playing ) == SIR_AUDIO_PENDING );
	checkVoice( audio.update() == 1 );
	checkVoice( audio.getPlaybackState( playing, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FAILED );
	checkVoice( backend.getActiveVoiceCount() == 0 );
	checkVoice( audio.playEvent( 10, {}, playing ) == SIR_AUDIO_PENDING );
	checkVoice( audio.update() == 1 );
	fixture.nFailPoll = 1;
	fixture.instances[0].bStopped = true;
	checkVoice( audio.update() == 0 );
	checkVoice( backend.getActiveVoiceCount() == 1 );
	checkVoice( audio.update() == 0 );
	checkVoice( backend.getActiveVoiceCount() == 0 );
	checkVoice( audio.getPlaybackState( playing, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FINISHED );
	audio.shutdown();
	checkVoice( context.shutdown() == SIR_FMOD_OK );
}

static void testCompletionReservations( const sirContainerAllocator_t *pAllocator )
{
	sirFMODVoiceFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkVoice( context.initialize( contextConfig, makeContextOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend backend;
	checkVoice( backend.configure( &context, pAllocator, makeVoiceOps( &fixture ), 2 ) == SIR_AUDIO_OK );
	const sirAudioBackendOps_t ops = SIRFMODVoiceBackend::getBackendOps();
	const sirAudioBackendConfig_t backendConfig = { 2, 4 };
	checkVoice( ops.initialize( &backend, &backendConfig ) == SIR_AUDIO_OK );
	checkVoice( backend.registerEvent( 10, "event:/A" ) == SIR_AUDIO_OK );
	sirAudioCommand_t first = {};
	first.eType = SIR_AUDIO_COMMAND_PLAY;
	first.hPlaying = { 0, 1, 7 };
	first.nEventId = 10;
	first.nSequence = 1;
	checkVoice( ops.submit( &backend, &first, 1 ) == SIR_AUDIO_OK );
	sirAudioCommand_t second = first;
	second.hPlaying = { 1, 1, 7 };
	second.nSequence = 2;
	checkVoice( ops.submit( &backend, &second, 1 ) == SIR_AUDIO_OK );
	sirAudioCompletion_t pending[4] = {};
	checkVoice( ops.drainCompletions( &backend, pending, 4 ) == 2 );
	checkVoice( pending[0].eType == SIR_AUDIO_COMPLETION_STARTED );
	checkVoice( pending[1].eType == SIR_AUDIO_COMPLETION_STARTED );
	fixture.instances[0].bStopped = true;
	fixture.instances[1].bStopped = true;
	ops.update( &backend );
	checkVoice( ops.drainCompletions( &backend, pending, 4 ) == 2 );
	checkVoice( pending[0].eType == SIR_AUDIO_COMPLETION_FINISHED );
	checkVoice( pending[1].eType == SIR_AUDIO_COMPLETION_FINISHED );
	checkVoice( backend.getActiveVoiceCount() == 0 );
	checkVoice( ops.submit( &backend, &second, 2 ) == SIR_AUDIO_INVALID_ARGUMENT );
	ops.shutdown( &backend );
	checkVoice( context.shutdown() == SIR_FMOD_OK );
}

static void testStartFailureCleanupRetry( const sirContainerAllocator_t *pAllocator )
{
	sirFMODVoiceFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t contextConfig = { 16, -1, 1, 1, 0, 0 };
	checkVoice( context.initialize( contextConfig, makeContextOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODVoiceBackend backend;
	checkVoice( backend.configure( &context, pAllocator, makeVoiceOps( &fixture ), 1 ) == SIR_AUDIO_OK );
	const sirAudioBackendOps_t ops = SIRFMODVoiceBackend::getBackendOps();
	const sirAudioBackendConfig_t backendConfig = { 1, 2 };
	checkVoice( ops.initialize( &backend, &backendConfig ) == SIR_AUDIO_OK );
	checkVoice( backend.registerEvent( 10, "event:/A" ) == SIR_AUDIO_OK );
	sirAudioCommand_t command = {};
	command.eType = SIR_AUDIO_COMMAND_PLAY;
	command.hPlaying = { 0, 1, 7 };
	command.nEventId = 10;
	command.nSequence = 1;
	fixture.nFailStart = 1;
	fixture.nFailRelease = 2;
	checkVoice( ops.submit( &backend, &command, 1 ) == SIR_AUDIO_BACKEND_ERROR );
	checkVoice( backend.getActiveVoiceCount() == 1 );
	checkVoice( ops.submit( &backend, &command, 1 ) == SIR_AUDIO_CAPACITY );
	ops.update( &backend );
	checkVoice( backend.getActiveVoiceCount() == 1 );
	ops.update( &backend );
	checkVoice( backend.getActiveVoiceCount() == 0 );
	checkVoice( ops.submit( &backend, &command, 1 ) == SIR_AUDIO_OK );
	sirAudioCompletion_t completion = {};
	checkVoice( ops.drainCompletions( &backend, &completion, 1 ) == 1 );
	checkVoice( completion.eType == SIR_AUDIO_COMPLETION_STARTED );
	fixture.instances[0].bStopped = true;
	ops.update( &backend );
	checkVoice( ops.drainCompletions( &backend, &completion, 1 ) == 1 );
	checkVoice( completion.eType == SIR_AUDIO_COMPLETION_FINISHED );
	ops.shutdown( &backend );
	checkVoice( context.shutdown() == SIR_FMOD_OK );
}

int main( void )
{
	const sirContainerAllocator_t allocator = { nullptr, testVoiceAllocate, testVoiceDeallocate, 0, 0 };
	testVoiceLifecycle( &allocator );
	testParameterDispatch( &allocator );
	testSpatialDriver( &allocator );
	testVoiceFailures( &allocator );
	testCompletionReservations( &allocator );
	testStartFailureCleanupRetry( &allocator );
	checkVoice( s_nAllocations == 0 );
	return 0;
}
