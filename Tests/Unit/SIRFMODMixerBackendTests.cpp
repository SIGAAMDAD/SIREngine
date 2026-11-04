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

#include <Audio/FMOD/SIRFMODMixerBackend.h>

#include <limits>
#include <stdlib.h>
#include <string.h>

typedef struct sirFMODMixerFixture_s
{
	uint32_t nBus = 11;
	uint32_t nVCA = 22;
	uint32_t nResolveBusCalls = 0;
	uint32_t nResolveVCACalls = 0;
	uint32_t nBusVolumeCalls = 0;
	uint32_t nVCAVolumeCalls = 0;
	uint32_t nBusMuteCalls = 0;
	uint32_t nFailResolve = 0;
	uint32_t nFailUpdate = 0;
	uint8_t bBusMute = 0;
	float fBusVolume = 1.0f;
	float fVCAVolume = 1.0f;
	bool bStudioAlive = false;
} sirFMODMixerFixture_t;

static uint32_t s_nMixerAllocations = 0;
static bool s_bMixerFailAllocation = false;

static void checkMixer( bool bCondition ) noexcept
{
	if ( !bCondition ) {
		abort();
	}
}

static void *mixerAllocate( void *, size_t nBytes, size_t ) noexcept
{
	if ( s_bMixerFailAllocation ) {
		return nullptr;
	}
	void *pMemory = malloc( nBytes );
	if ( pMemory != nullptr ) {
		++s_nMixerAllocations;
	}
	return pMemory;
}

static void mixerDeallocate( void *, void *pMemory, size_t, size_t ) noexcept
{
	if ( pMemory != nullptr ) {
		--s_nMixerAllocations;
		free( pMemory );
	}
}

static int32_t mixerStudioCreate( void *pData, void **ppStudio ) noexcept
{
	sirFMODMixerFixture_t *pFixture = ( sirFMODMixerFixture_t * )pData;
	pFixture->bStudioAlive = true;
	*ppStudio = pFixture;
	return 0;
}

static int32_t mixerGetCore( void *pData, void *pStudio, void **ppCore ) noexcept
{
	checkMixer( pStudio == pData );
	*ppCore = pData;
	return 0;
}

static int32_t mixerGetVersion( void *, void *, uint32_t *pVersion ) noexcept
{
	*pVersion = 0x00020400;
	return 0;
}

static int32_t mixerGetDriverCount( void *, void *, int32_t *pCount ) noexcept
{
	*pCount = 1;
	return 0;
}

static int32_t mixerSetDriver( void *, void *, int32_t ) noexcept
{
	return 0;
}

static int32_t mixerSetNoSound( void *, void * ) noexcept
{
	return 0;
}

static int32_t mixerStudioInitialize( void *, void *, const sirFMODContextConfig_t * ) noexcept
{
	return 0;
}

static int32_t mixerStudioUpdate( void *, void * ) noexcept
{
	return 0;
}

static int32_t mixerStudioRelease( void *pData, void *pStudio ) noexcept
{
	checkMixer( pData == pStudio );
	( ( sirFMODMixerFixture_t * )pData )->bStudioAlive = false;
	return 0;
}

static int32_t mixerRegisterDeviceLoss( void *, void *, void *, void ( * )( void * ) noexcept ) noexcept
{
	return 0;
}

static sirFMODNativeOps_t makeMixerStudioOps( sirFMODMixerFixture_t *pFixture ) noexcept
{
	return { 2,
	         0x00020400,
	         pFixture,
	         mixerStudioCreate,
	         mixerGetCore,
	         mixerGetVersion,
	         mixerGetDriverCount,
	         mixerSetDriver,
	         mixerSetNoSound,
	         mixerStudioInitialize,
	         mixerStudioUpdate,
	         mixerStudioRelease,
	         mixerRegisterDeviceLoss };
}

static int32_t mixerResolveBus( void *pData, void *pStudio, const char *szPath, void **ppBus ) noexcept
{
	sirFMODMixerFixture_t *pFixture = ( sirFMODMixerFixture_t * )pData;
	checkMixer( pStudio == pFixture );
	++pFixture->nResolveBusCalls;
	if ( pFixture->nFailResolve != 0 ) {
		--pFixture->nFailResolve;
		return -41;
	}
	if ( strcmp( szPath, "bus:/Master" ) != 0 ) {
		return -42;
	}
	*ppBus = &pFixture->nBus;
	return 0;
}

static int32_t mixerResolveVCA( void *pData, void *pStudio, const char *szPath, void **ppVCA ) noexcept
{
	sirFMODMixerFixture_t *pFixture = ( sirFMODMixerFixture_t * )pData;
	checkMixer( pStudio == pFixture );
	++pFixture->nResolveVCACalls;
	if ( strcmp( szPath, "vca:/Music" ) != 0 ) {
		return -42;
	}
	*ppVCA = &pFixture->nVCA;
	return 0;
}

static int32_t mixerSetBusVolume( void *pData, void *pBus, float fVolume ) noexcept
{
	sirFMODMixerFixture_t *pFixture = ( sirFMODMixerFixture_t * )pData;
	checkMixer( pBus == &pFixture->nBus );
	++pFixture->nBusVolumeCalls;
	if ( pFixture->nFailUpdate != 0 ) {
		--pFixture->nFailUpdate;
		return -52;
	}
	pFixture->fBusVolume = fVolume;
	return 0;
}

static int32_t mixerSetBusMute( void *pData, void *pBus, uint8_t bMute ) noexcept
{
	sirFMODMixerFixture_t *pFixture = ( sirFMODMixerFixture_t * )pData;
	checkMixer( pBus == &pFixture->nBus );
	++pFixture->nBusMuteCalls;
	pFixture->bBusMute = bMute;
	return 0;
}

static int32_t mixerSetVCAVolume( void *pData, void *pVCA, float fVolume ) noexcept
{
	sirFMODMixerFixture_t *pFixture = ( sirFMODMixerFixture_t * )pData;
	checkMixer( pVCA == &pFixture->nVCA );
	++pFixture->nVCAVolumeCalls;
	pFixture->fVCAVolume = fVolume;
	return 0;
}

static sirFMODMixerNativeOps_t makeMixerOps( sirFMODMixerFixture_t *pFixture ) noexcept
{
	return { 1, pFixture, mixerResolveBus, mixerResolveVCA, mixerSetBusVolume, mixerSetBusMute, mixerSetVCAVolume };
}

static void testMixerControls( const sirContainerAllocator_t *pAllocator )
{
	sirFMODMixerFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 16, -1, 1, 1, 0, 0 };
	checkMixer( context.initialize( config, makeMixerStudioOps( &fixture ) ) == SIR_FMOD_OK );

	SIRFMODMixerBackend mixer;
	checkMixer( mixer.configure( &context, pAllocator, makeMixerOps( &fixture ), 0 ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.configure( &context, pAllocator, makeMixerOps( &fixture ), 2 ) == SIR_AUDIO_OK );
	checkMixer( mixer.initialize() == SIR_AUDIO_OK );
	checkMixer( mixer.initialize() == SIR_AUDIO_NOT_INITIALIZED );
	checkMixer( mixer.registerBus( 0, "bus:/Master" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.registerBus( 10, nullptr ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.registerBus( 10, "" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.registerBus( 10, "bus:/Missing" ) == SIR_AUDIO_BACKEND_ERROR );
	checkMixer( mixer.getControlCount() == 0 );

	fixture.nFailResolve = 1;
	checkMixer( mixer.registerBus( 10, "bus:/Master" ) == SIR_AUDIO_BACKEND_ERROR );
	checkMixer( mixer.getLastNativeResult() == -41 );
	checkMixer( mixer.registerBus( 10, "bus:/Master" ) == SIR_AUDIO_OK );
	checkMixer( mixer.registerVCA( 5, "vca:/Music" ) == SIR_AUDIO_OK );
	checkMixer( mixer.registerVCA( 5, "vca:/Music" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.registerBus( 11, "bus:/Master" ) == SIR_AUDIO_CAPACITY );
	checkMixer( mixer.getControlCount() == 2 );
	checkMixer( context.getBorrowedBankObjectCount() == 2 );

	checkMixer( mixer.setBusVolume( 10, 0.75f ) == SIR_AUDIO_OK );
	checkMixer( fixture.fBusVolume == 0.75f && fixture.nBusVolumeCalls == 1 );
	checkMixer( mixer.setVCAVolume( 5, 0.25f ) == SIR_AUDIO_OK );
	checkMixer( fixture.fVCAVolume == 0.25f && fixture.nVCAVolumeCalls == 1 );
	checkMixer( mixer.setBusMute( 10, true ) == SIR_AUDIO_OK );
	checkMixer( fixture.bBusMute == 1 && fixture.nBusMuteCalls == 1 );

	checkMixer( mixer.setBusVolume( 5, 0.5f ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.setVCAVolume( 10, 0.5f ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.setBusMute( 5, true ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.setBusVolume( 10, std::numeric_limits<float>::quiet_NaN() ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.setBusVolume( 10, std::numeric_limits<float>::infinity() ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.setBusVolume( 10, -0.1f ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.setBusVolume( 10, 1.1f ) == SIR_AUDIO_INVALID_ARGUMENT );

	fixture.nFailUpdate = 1;
	checkMixer( mixer.setBusVolume( 10, 0.5f ) == SIR_AUDIO_BACKEND_ERROR );
	checkMixer( mixer.getLastNativeResult() == -52 && fixture.fBusVolume == 0.75f );
	checkMixer( mixer.setBusVolume( 10, 0.5f ) == SIR_AUDIO_OK && fixture.fBusVolume == 0.5f );

	checkMixer( mixer.unregisterControl( 10 ) == SIR_AUDIO_OK );
	checkMixer( context.getBorrowedBankObjectCount() == 1 );
	checkMixer( mixer.unregisterControl( 10 ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkMixer( mixer.registerBus( 2, "bus:/Master" ) == SIR_AUDIO_OK );
	checkMixer( mixer.setBusVolume( 2, 0.1f ) == SIR_AUDIO_OK );
	checkMixer( mixer.setVCAVolume( 5, 0.9f ) == SIR_AUDIO_OK );
	checkMixer( mixer.getControlCount() == 2 );
	mixer.shutdown();
	checkMixer( mixer.getControlCount() == 0 );
	checkMixer( context.getBorrowedBankObjectCount() == 0 );
	checkMixer( mixer.setBusMute( 2, true ) == SIR_AUDIO_NOT_INITIALIZED );
	checkMixer( context.shutdown() == SIR_FMOD_OK );
}

static void testMixerAllocationAndEpoch( const sirContainerAllocator_t *pAllocator )
{
	sirFMODMixerFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 16, -1, 1, 1, 0, 0 };
	checkMixer( context.initialize( config, makeMixerStudioOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODMixerBackend mixer;
	checkMixer( mixer.configure( &context, pAllocator, makeMixerOps( &fixture ), 1 ) == SIR_AUDIO_OK );
	s_bMixerFailAllocation = true;
	checkMixer( mixer.initialize() == SIR_AUDIO_CAPACITY );
	s_bMixerFailAllocation = false;
	checkMixer( mixer.initialize() == SIR_AUDIO_OK );
	checkMixer( mixer.registerBus( 7, "bus:/Master" ) == SIR_AUDIO_OK );
	checkMixer( context.getBorrowedBankObjectCount() == 1 );
	checkMixer( context.shutdown() == SIR_FMOD_BAD_STATE );
	checkMixer( context.isReady() );
	checkMixer( mixer.setBusMute( 7, true ) == SIR_AUDIO_OK );
	mixer.shutdown();
	checkMixer( context.getBorrowedBankObjectCount() == 0 );
	checkMixer( context.shutdown() == SIR_FMOD_OK );
	checkMixer( context.initialize( config, makeMixerStudioOps( &fixture ) ) == SIR_FMOD_OK );
	checkMixer( mixer.setBusVolume( 7, 0.4f ) == SIR_AUDIO_NOT_INITIALIZED );
	checkMixer( context.shutdown() == SIR_FMOD_OK );
}

int main( void )
{
	const sirContainerAllocator_t allocator = { nullptr, mixerAllocate, mixerDeallocate, 0, 0 };
	testMixerControls( &allocator );
	testMixerAllocationAndEpoch( &allocator );
	checkMixer( s_nMixerAllocations == 0 );
	return 0;
}
