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

#include <Audio/FMOD/SIRFMODSnapshotBackend.h>

#include <stdlib.h>
#include <string.h>

typedef struct sirFMODSnapshotFixtureInstance_s
{
	uint8_t bAllocated;
	uint8_t bPlaying;
} sirFMODSnapshotFixtureInstance_t;

typedef struct sirFMODSnapshotFixture_s
{
	sirFMODSnapshotFixtureInstance_t instances[4] = {};
	uint32_t nDescription = 7;
	uint32_t nResolves = 0;
	uint32_t nCreates = 0;
	uint32_t nStarts = 0;
	uint32_t nStops = 0;
	uint32_t nReleases = 0;
	uint32_t nFailResolve = 0;
	uint32_t nFailCreate = 0;
	uint32_t nFailStart = 0;
	uint32_t nFailStop = 0;
	uint32_t nFailRelease = 0;
	sirAudioStopMode_t eLastStop = SIR_AUDIO_STOP_IMMEDIATE;
	bool bStudioAlive = false;
} sirFMODSnapshotFixture_t;

static uint32_t s_nSnapshotAllocations = 0;
static bool s_bSnapshotRejectAllocation = false;

static void checkSnapshot( bool bValue ) noexcept
{
	if ( !bValue ) {
		abort();
	}
}

static void *snapshotAllocate( void *, size_t nBytes, size_t ) noexcept
{
	if ( s_bSnapshotRejectAllocation ) {
		return nullptr;
	}
	void *pMemory = malloc( nBytes );
	if ( pMemory != nullptr ) {
		++s_nSnapshotAllocations;
	}
	return pMemory;
}

static void snapshotDeallocate( void *, void *pMemory, size_t, size_t ) noexcept
{
	if ( pMemory != nullptr ) {
		--s_nSnapshotAllocations;
		free( pMemory );
	}
}

static int32_t snapshotStudioCreate( void *pData, void **ppStudio ) noexcept
{
	sirFMODSnapshotFixture_t *pFixture = ( sirFMODSnapshotFixture_t * )pData;
	pFixture->bStudioAlive = true;
	*ppStudio = pFixture;
	return 0;
}

static int32_t snapshotGetCore( void *pData, void *pStudio, void **ppCore ) noexcept
{
	checkSnapshot( pData == pStudio );
	*ppCore = pData;
	return 0;
}

static int32_t snapshotGetVersion( void *, void *, uint32_t *pVersion ) noexcept
{
	*pVersion = 0x00020400;
	return 0;
}

static int32_t snapshotGetDriverCount( void *, void *, int32_t *pCount ) noexcept
{
	*pCount = 1;
	return 0;
}

static int32_t snapshotSetDriver( void *, void *, int32_t ) noexcept
{
	return 0;
}

static int32_t snapshotSetNoSound( void *, void * ) noexcept
{
	return 0;
}

static int32_t snapshotStudioInitialize( void *, void *, const sirFMODContextConfig_t * ) noexcept
{
	return 0;
}

static int32_t snapshotStudioUpdate( void *, void * ) noexcept
{
	return 0;
}

static int32_t snapshotStudioRelease( void *pData, void *pStudio ) noexcept
{
	checkSnapshot( pData == pStudio );
	( ( sirFMODSnapshotFixture_t * )pData )->bStudioAlive = false;
	return 0;
}

static int32_t snapshotRegisterDeviceLoss( void *, void *, void *, void ( * )( void * ) noexcept ) noexcept
{
	return 0;
}

static int32_t fakeQueryOutputDevice( void *pUserData, void *, int32_t *pDriver, uint64_t *pPhysicalId ) noexcept
{
	( void )pUserData;
	*pDriver = 0;
	*pPhysicalId = UINT64_C( 0xAA55 );
	return 0;
}

static sirFMODNativeOps_t makeSnapshotContextOps( sirFMODSnapshotFixture_t *pFixture ) noexcept
{
	return { 3,
	         0x00020400,
	         pFixture,
	         snapshotStudioCreate,
	         snapshotGetCore,
	         snapshotGetVersion,
	         snapshotGetDriverCount,
	         snapshotSetDriver,
	         fakeQueryOutputDevice,
	         snapshotSetNoSound,
	         snapshotStudioInitialize,
	         snapshotStudioUpdate,
	         snapshotStudioRelease,
	         snapshotRegisterDeviceLoss };
}

static int32_t snapshotResolve( void *pData, void *pStudio, const char *szPath, void **ppDescription ) noexcept
{
	sirFMODSnapshotFixture_t *pFixture = ( sirFMODSnapshotFixture_t * )pData;
	checkSnapshot( pStudio == pFixture );
	++pFixture->nResolves;
	if ( pFixture->nFailResolve != 0 ) {
		--pFixture->nFailResolve;
		return -21;
	}
	if ( strcmp( szPath, "snapshot:/Storm" ) != 0 && strcmp( szPath, "snapshot:/Combat" ) != 0 ) {
		return -22;
	}
	*ppDescription = &pFixture->nDescription;
	return 0;
}

static int32_t snapshotCreate( void *pData, void *pDescription, void **ppInstance ) noexcept
{
	sirFMODSnapshotFixture_t *pFixture = ( sirFMODSnapshotFixture_t * )pData;
	checkSnapshot( pDescription == &pFixture->nDescription );
	++pFixture->nCreates;
	for ( uint32_t iInstance = 0; iInstance < 4; ++iInstance ) {
		sirFMODSnapshotFixtureInstance_t& instance = pFixture->instances[iInstance];
		if ( instance.bAllocated != 0 ) {
			continue;
		}
		instance.bAllocated = 1;
		instance.bPlaying = 0;
		*ppInstance = &instance;
		if ( pFixture->nFailCreate != 0 ) {
			--pFixture->nFailCreate;
			return -31;
		}
		return 0;
	}
	return -32;
}

static int32_t snapshotStart( void *pData, void *pInstance ) noexcept
{
	sirFMODSnapshotFixture_t *pFixture = ( sirFMODSnapshotFixture_t * )pData;
	sirFMODSnapshotFixtureInstance_t *pSlot = ( sirFMODSnapshotFixtureInstance_t * )pInstance;
	checkSnapshot( pSlot->bAllocated != 0 );
	++pFixture->nStarts;
	if ( pFixture->nFailStart != 0 ) {
		--pFixture->nFailStart;
		return -41;
	}
	pSlot->bPlaying = 1;
	return 0;
}

static int32_t snapshotStop( void *pData, void *pInstance, sirAudioStopMode_t eMode ) noexcept
{
	sirFMODSnapshotFixture_t *pFixture = ( sirFMODSnapshotFixture_t * )pData;
	sirFMODSnapshotFixtureInstance_t *pSlot = ( sirFMODSnapshotFixtureInstance_t * )pInstance;
	checkSnapshot( pSlot->bAllocated != 0 && pSlot->bPlaying != 0 );
	++pFixture->nStops;
	pFixture->eLastStop = eMode;
	if ( pFixture->nFailStop != 0 ) {
		--pFixture->nFailStop;
		return -51;
	}
	pSlot->bPlaying = 0;
	return 0;
}

static int32_t snapshotRelease( void *pData, void *pInstance ) noexcept
{
	sirFMODSnapshotFixture_t *pFixture = ( sirFMODSnapshotFixture_t * )pData;
	sirFMODSnapshotFixtureInstance_t *pSlot = ( sirFMODSnapshotFixtureInstance_t * )pInstance;
	checkSnapshot( pSlot->bAllocated != 0 );
	++pFixture->nReleases;
	if ( pFixture->nFailRelease != 0 ) {
		--pFixture->nFailRelease;
		return -61;
	}
	pSlot->bAllocated = 0;
	pSlot->bPlaying = 0;
	return 0;
}

static sirFMODSnapshotNativeOps_t makeSnapshotOps( sirFMODSnapshotFixture_t *pFixture ) noexcept
{
	return { 1, pFixture, snapshotResolve, snapshotCreate, snapshotStart, snapshotStop, snapshotRelease };
}

static void testSnapshotLifecycle( const sirContainerAllocator_t *pAllocator )
{
	sirFMODSnapshotFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 16, -1, 1, 1, 0, 0 };
	checkSnapshot( context.initialize( config, makeSnapshotContextOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODSnapshotBackend snapshots;
	sirFMODSnapshotNativeOps_t ops = makeSnapshotOps( &fixture );
	ops.nAbiVersion = 2;
	checkSnapshot( snapshots.configure( &context, pAllocator, ops, 2 ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkSnapshot( snapshots.configure( &context, pAllocator, makeSnapshotOps( &fixture ), 0 ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkSnapshot( snapshots.configure( &context, pAllocator, makeSnapshotOps( &fixture ), 2 ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.initialize() == SIR_AUDIO_OK );
	checkSnapshot( snapshots.initialize() == SIR_AUDIO_NOT_INITIALIZED );
	checkSnapshot( snapshots.registerSnapshot( 0, "snapshot:/Storm" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkSnapshot( snapshots.registerSnapshot( 30, nullptr ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkSnapshot( snapshots.registerSnapshot( 30, "event:/Noise" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkSnapshot( snapshots.registerSnapshot( 30, "snapshot:/" ) == SIR_AUDIO_INVALID_ARGUMENT );
	fixture.nFailResolve = 1;
	checkSnapshot( snapshots.registerSnapshot( 30, "snapshot:/Storm" ) == SIR_AUDIO_BACKEND_ERROR );
	checkSnapshot( snapshots.getLastNativeResult() == -21 && snapshots.getSnapshotCount() == 0 );
	checkSnapshot( snapshots.registerSnapshot( 30, "snapshot:/Storm" ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.registerSnapshot( 10, "snapshot:/Combat" ) == SIR_AUDIO_OK );
	checkSnapshot( context.getBorrowedBankObjectCount() == 2 );
	checkSnapshot( snapshots.registerSnapshot( 30, "snapshot:/Storm" ) == SIR_AUDIO_INVALID_ARGUMENT );
	checkSnapshot( snapshots.registerSnapshot( 40, "snapshot:/Storm" ) == SIR_AUDIO_CAPACITY );

	checkSnapshot( snapshots.startSnapshot( 30 ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.getActiveInstanceCount() == 1 );
	checkSnapshot( snapshots.startSnapshot( 30 ) == SIR_AUDIO_BUSY );
	checkSnapshot( snapshots.unregisterSnapshot( 30 ) == SIR_AUDIO_BUSY );
	checkSnapshot( snapshots.stopSnapshot( 30, ( sirAudioStopMode_t )7 ) == SIR_AUDIO_INVALID_ARGUMENT );
	fixture.nFailStop = 1;
	checkSnapshot( snapshots.stopSnapshot( 30, SIR_AUDIO_STOP_ALLOW_FADE ) == SIR_AUDIO_BACKEND_ERROR );
	checkSnapshot( snapshots.getLastNativeResult() == -51 && snapshots.getActiveInstanceCount() == 1 );
	fixture.nFailRelease = 1;
	checkSnapshot( snapshots.stopSnapshot( 30, SIR_AUDIO_STOP_ALLOW_FADE ) == SIR_AUDIO_BACKEND_ERROR );
	checkSnapshot( snapshots.getLastNativeResult() == -61 && snapshots.getActiveInstanceCount() == 1 );
	const uint32_t nStopped = fixture.nStops;
	checkSnapshot( snapshots.stopSnapshot( 30, SIR_AUDIO_STOP_ALLOW_FADE ) == SIR_AUDIO_OK );
	checkSnapshot( fixture.nStops == nStopped && fixture.eLastStop == SIR_AUDIO_STOP_ALLOW_FADE );
	checkSnapshot( snapshots.getActiveInstanceCount() == 0 );
	checkSnapshot( snapshots.stopSnapshot( 30, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_INVALID_HANDLE );

	fixture.nFailStart = 1;
	checkSnapshot( snapshots.startSnapshot( 10 ) == SIR_AUDIO_BACKEND_ERROR );
	checkSnapshot( snapshots.getLastNativeResult() == -41 && snapshots.getActiveInstanceCount() == 0 );
	fixture.nFailCreate = 1;
	fixture.nFailRelease = 1;
	checkSnapshot( snapshots.startSnapshot( 10 ) == SIR_AUDIO_BACKEND_ERROR );
	checkSnapshot( snapshots.getLastNativeResult() == -61 && snapshots.getActiveInstanceCount() == 1 );
	checkSnapshot( snapshots.startSnapshot( 10 ) == SIR_AUDIO_BUSY );
	checkSnapshot( snapshots.stopSnapshot( 10, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.getActiveInstanceCount() == 0 );
	checkSnapshot( snapshots.unregisterSnapshot( 10 ) == SIR_AUDIO_OK );
	checkSnapshot( context.getBorrowedBankObjectCount() == 1 );
	checkSnapshot( snapshots.registerSnapshot( 5, "snapshot:/Combat" ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.startSnapshot( 5 ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.stopSnapshot( 5, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_OK );
	checkSnapshot( fixture.eLastStop == SIR_AUDIO_STOP_IMMEDIATE );
	checkSnapshot( snapshots.unregisterSnapshot( 30 ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.unregisterSnapshot( 5 ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.getSnapshotCount() == 0 );
	checkSnapshot( context.getBorrowedBankObjectCount() == 0 );
	checkSnapshot( snapshots.shutdown() == SIR_AUDIO_OK );
	checkSnapshot( context.shutdown() == SIR_FMOD_OK );
}

static void testShutdownFailureRetry( const sirContainerAllocator_t *pAllocator )
{
	sirFMODSnapshotFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 16, -1, 1, 1, 0, 0 };
	checkSnapshot( context.initialize( config, makeSnapshotContextOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODSnapshotBackend snapshots;
	checkSnapshot( snapshots.configure( &context, pAllocator, makeSnapshotOps( &fixture ), 2 ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.initialize() == SIR_AUDIO_OK );
	checkSnapshot( snapshots.registerSnapshot( 4, "snapshot:/Storm" ) == SIR_AUDIO_OK );
	checkSnapshot( snapshots.startSnapshot( 4 ) == SIR_AUDIO_OK );
	fixture.nFailStop = 1;
	checkSnapshot( snapshots.shutdown() == SIR_AUDIO_BACKEND_ERROR );
	checkSnapshot( snapshots.getActiveInstanceCount() == 1 );
	fixture.nFailRelease = 1;
	checkSnapshot( snapshots.shutdown() == SIR_AUDIO_BACKEND_ERROR );
	checkSnapshot( snapshots.getActiveInstanceCount() == 1 );
	checkSnapshot( snapshots.shutdown() == SIR_AUDIO_OK );
	checkSnapshot( snapshots.getActiveInstanceCount() == 0 );
	checkSnapshot( context.shutdown() == SIR_FMOD_OK );
}

static void testAllocationAndEpoch( const sirContainerAllocator_t *pAllocator )
{
	sirFMODSnapshotFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 16, -1, 1, 1, 0, 0 };
	checkSnapshot( context.initialize( config, makeSnapshotContextOps( &fixture ) ) == SIR_FMOD_OK );
	SIRFMODSnapshotBackend snapshots;
	checkSnapshot( snapshots.configure( &context, pAllocator, makeSnapshotOps( &fixture ), 1 ) == SIR_AUDIO_OK );
	s_bSnapshotRejectAllocation = true;
	checkSnapshot( snapshots.initialize() == SIR_AUDIO_CAPACITY );
	s_bSnapshotRejectAllocation = false;
	checkSnapshot( snapshots.initialize() == SIR_AUDIO_OK );
	checkSnapshot( snapshots.registerSnapshot( 1, "snapshot:/Storm" ) == SIR_AUDIO_OK );
	checkSnapshot( context.getBorrowedBankObjectCount() == 1 );
	checkSnapshot( context.shutdown() == SIR_FMOD_BAD_STATE );
	checkSnapshot( snapshots.startSnapshot( 1 ) == SIR_AUDIO_OK );
	checkSnapshot( context.shutdown() == SIR_FMOD_BAD_STATE );
	checkSnapshot( snapshots.shutdown() == SIR_AUDIO_OK );
	checkSnapshot( context.getBorrowedBankObjectCount() == 0 );
	checkSnapshot( context.shutdown() == SIR_FMOD_OK );
	checkSnapshot( context.initialize( config, makeSnapshotContextOps( &fixture ) ) == SIR_FMOD_OK );
	checkSnapshot( snapshots.startSnapshot( 1 ) == SIR_AUDIO_NOT_INITIALIZED );
	checkSnapshot( context.shutdown() == SIR_FMOD_OK );
}

int main( void )
{
	const sirContainerAllocator_t allocator = { nullptr, snapshotAllocate, snapshotDeallocate, 0, 0 };
	testSnapshotLifecycle( &allocator );
	testShutdownFailureRetry( &allocator );
	testAllocationAndEpoch( &allocator );
	checkSnapshot( s_nSnapshotAllocations == 0 );
	return 0;
}
