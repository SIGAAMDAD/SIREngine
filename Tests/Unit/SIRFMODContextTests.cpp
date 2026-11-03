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

#include <Audio/FMOD/SIRFMODContext.h>
#include <stdlib.h>

typedef enum sirFMODFixtureFailure_e : uint8_t
{
	SIR_FMOD_FIXTURE_NONE = 0,
	SIR_FMOD_FIXTURE_CREATE = 1,
	SIR_FMOD_FIXTURE_CORE = 2,
	SIR_FMOD_FIXTURE_VERSION = 3,
	SIR_FMOD_FIXTURE_DRIVER_COUNT = 4,
	SIR_FMOD_FIXTURE_DRIVER = 5,
	SIR_FMOD_FIXTURE_NO_SOUND = 6,
	SIR_FMOD_FIXTURE_INITIALIZE = 7,
	SIR_FMOD_FIXTURE_UPDATE = 8,
	SIR_FMOD_FIXTURE_RELEASE = 9
} sirFMODFixtureFailure_t;

typedef struct sirFMODFixture_s
{
	uint32_t nVersion = 0x00020400;
	int32_t nDriverCount = 4;
	int32_t iSelectedDriver = -1;
	int32_t nCreateCalls = 0;
	int32_t nCoreCalls = 0;
	int32_t nReleaseCalls = 0;
	int32_t nUpdateCalls = 0;
	int32_t nInitializeCalls = 0;
	int32_t nNoSoundCalls = 0;
	bool bStudioAlive = false;
	bool bCoreAlive = false;
	sirFMODFixtureFailure_t eFailure = SIR_FMOD_FIXTURE_NONE;
} sirFMODFixture_t;

static void checkFMOD( bool bCondition ) noexcept
{
	if ( !bCondition ) {
		abort();
	}
}

static int32_t failOnce( sirFMODFixture_t *pFixture, sirFMODFixtureFailure_t eFailure ) noexcept
{
	if ( pFixture->eFailure == eFailure ) {
		pFixture->eFailure = SIR_FMOD_FIXTURE_NONE;
		return -42;
	}
	return 0;
}

static int32_t fakeCreate( void *pUserData, void **ppStudio ) noexcept
{
	sirFMODFixture_t *pFixture = ( sirFMODFixture_t * )pUserData;
	++pFixture->nCreateCalls;
	pFixture->bStudioAlive = true;
	*ppStudio = pFixture;
	return failOnce( pFixture, SIR_FMOD_FIXTURE_CREATE );
}

static int32_t fakeGetCore( void *pUserData, void *pStudio, void **ppCore ) noexcept
{
	sirFMODFixture_t *pFixture = ( sirFMODFixture_t * )pUserData;
	checkFMOD( pStudio == pFixture );
	++pFixture->nCoreCalls;
	const int32_t nResult = failOnce( pFixture, SIR_FMOD_FIXTURE_CORE );
	if ( nResult == 0 ) {
		pFixture->bCoreAlive = true;
		*ppCore = &pFixture->nVersion;
	}
	return nResult;
}

static int32_t fakeGetVersion( void *pUserData, void *pCore, uint32_t *pVersion ) noexcept
{
	sirFMODFixture_t *pFixture = ( sirFMODFixture_t * )pUserData;
	checkFMOD( pCore == &pFixture->nVersion );
	*pVersion = pFixture->nVersion;
	return failOnce( pFixture, SIR_FMOD_FIXTURE_VERSION );
}

static int32_t fakeGetDriverCount( void *pUserData, void *pCore, int32_t *pCount ) noexcept
{
	sirFMODFixture_t *pFixture = ( sirFMODFixture_t * )pUserData;
	checkFMOD( pCore == &pFixture->nVersion );
	*pCount = pFixture->nDriverCount;
	return failOnce( pFixture, SIR_FMOD_FIXTURE_DRIVER_COUNT );
}

static int32_t fakeSetDriver( void *pUserData, void *pCore, int32_t iDriver ) noexcept
{
	sirFMODFixture_t *pFixture = ( sirFMODFixture_t * )pUserData;
	checkFMOD( pCore == &pFixture->nVersion );
	const int32_t nResult = failOnce( pFixture, SIR_FMOD_FIXTURE_DRIVER );
	if ( nResult == 0 ) {
		pFixture->iSelectedDriver = iDriver;
	}
	return nResult;
}

static int32_t fakeSetNoSound( void *pUserData, void *pCore ) noexcept
{
	sirFMODFixture_t *pFixture = ( sirFMODFixture_t * )pUserData;
	checkFMOD( pCore == &pFixture->nVersion );
	++pFixture->nNoSoundCalls;
	return failOnce( pFixture, SIR_FMOD_FIXTURE_NO_SOUND );
}

static int32_t fakeInitialize( void *pUserData, void *pStudio, const sirFMODContextConfig_t *pConfig ) noexcept
{
	sirFMODFixture_t *pFixture = ( sirFMODFixture_t * )pUserData;
	checkFMOD( pStudio == pFixture );
	checkFMOD( pConfig->nMaxChannels > 0 );
	++pFixture->nInitializeCalls;
	return failOnce( pFixture, SIR_FMOD_FIXTURE_INITIALIZE );
}

static int32_t fakeUpdate( void *pUserData, void *pStudio ) noexcept
{
	sirFMODFixture_t *pFixture = ( sirFMODFixture_t * )pUserData;
	checkFMOD( pStudio == pFixture );
	++pFixture->nUpdateCalls;
	return failOnce( pFixture, SIR_FMOD_FIXTURE_UPDATE );
}

static int32_t fakeRelease( void *pUserData, void *pStudio ) noexcept
{
	sirFMODFixture_t *pFixture = ( sirFMODFixture_t * )pUserData;
	checkFMOD( pStudio == pFixture );
	++pFixture->nReleaseCalls;
	const int32_t nResult = failOnce( pFixture, SIR_FMOD_FIXTURE_RELEASE );
	if ( nResult == 0 ) {
		pFixture->bStudioAlive = false;
		pFixture->bCoreAlive = false;
	}
	return nResult;
}

static sirFMODNativeOps_t fakeOps( sirFMODFixture_t &fixture ) noexcept
{
	return { 1,
	         0x00020400,
	         &fixture,
	         fakeCreate,
	         fakeGetCore,
	         fakeGetVersion,
	         fakeGetDriverCount,
	         fakeSetDriver,
	         fakeSetNoSound,
	         fakeInitialize,
	         fakeUpdate,
	         fakeRelease };
}

static void testContextLifecycle( void )
{
	sirFMODFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 256, -1, 0, 0, 1, 0 };
	checkFMOD( context.getState() == SIR_FMOD_CONTEXT_EMPTY );
	checkFMOD( context.update() == SIR_FMOD_BAD_STATE );
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_OK );
	const uint64_t nFirstEpoch = context.getEpoch();
	checkFMOD( nFirstEpoch != 0 && context.isReady() );
	checkFMOD( fixture.nCreateCalls == 1 && fixture.nCoreCalls == 1 );
	checkFMOD( fixture.nInitializeCalls == 1 && fixture.nReleaseCalls == 0 );
	checkFMOD( context.getRuntimeVersion() == 0x00020400 );
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_BAD_STATE );
	checkFMOD( context.update() == SIR_FMOD_OK && fixture.nUpdateCalls == 1 );
	int32_t nDrivers = -1;
	checkFMOD( context.getDriverCount( nDrivers ) == SIR_FMOD_OK && nDrivers == 4 );
	checkFMOD( context.changeDriver( 4 ) == SIR_FMOD_INVALID_DRIVER );
	checkFMOD( context.changeDriver( -1 ) == SIR_FMOD_INVALID_DRIVER );
	checkFMOD( context.changeDriver( 2 ) == SIR_FMOD_OK && fixture.iSelectedDriver == 2 );
	checkFMOD( context.shutdown() == SIR_FMOD_OK );
	checkFMOD( !fixture.bStudioAlive && !fixture.bCoreAlive );
	checkFMOD( context.shutdown() == SIR_FMOD_OK && fixture.nReleaseCalls == 1 );
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_OK );
	checkFMOD( context.getEpoch() != nFirstEpoch );
	checkFMOD( context.shutdown() == SIR_FMOD_OK && fixture.nReleaseCalls == 2 );
}

static void testFailedCreationAndRollback( void )
{
	const sirFMODFixtureFailure_t failures[] = { SIR_FMOD_FIXTURE_CREATE,       SIR_FMOD_FIXTURE_CORE,   SIR_FMOD_FIXTURE_VERSION,
	                                             SIR_FMOD_FIXTURE_DRIVER_COUNT, SIR_FMOD_FIXTURE_DRIVER, SIR_FMOD_FIXTURE_INITIALIZE };
	for ( uint32_t iIndex = 0; iIndex < sizeof( failures ) / sizeof( failures[0] ); ++iIndex ) {
		sirFMODFixture_t fixture = {};
		fixture.eFailure = failures[iIndex];
		SIRFMODContext context;
		const sirFMODContextConfig_t config = { 128, 1, 0, 0, 0, 0 };
		checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_NATIVE_ERROR );
		checkFMOD( context.getState() == SIR_FMOD_CONTEXT_EMPTY );
		checkFMOD( context.getLastNativeResult() == -42 );
		checkFMOD( !fixture.bStudioAlive && fixture.nReleaseCalls == 1 );
		checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_OK );
		checkFMOD( context.shutdown() == SIR_FMOD_OK );
	}
}

static void testMismatchAndNoSound( void )
{
	sirFMODFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 64, -1, 1, 1, 0, 0 };
	fixture.nVersion = 0x00020300;
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_VERSION_MISMATCH );
	checkFMOD( fixture.nReleaseCalls == 1 && !fixture.bStudioAlive );
	fixture.nVersion = 0x00020400;
	fixture.eFailure = SIR_FMOD_FIXTURE_NO_SOUND;
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_NATIVE_ERROR );
	checkFMOD( fixture.nReleaseCalls == 2 );
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_OK );
	checkFMOD( fixture.nNoSoundCalls == 2 );
	checkFMOD( context.shutdown() == SIR_FMOD_OK );
}

static void testReleaseFailure( void )
{
	sirFMODFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 128, -1, 0, 0, 0, 0 };
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_OK );
	fixture.eFailure = SIR_FMOD_FIXTURE_RELEASE;
	checkFMOD( context.shutdown() == SIR_FMOD_RELEASE_ERROR );
	checkFMOD( context.getState() == SIR_FMOD_CONTEXT_RELEASE_FAILED );
	checkFMOD( fixture.bStudioAlive );
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_BAD_STATE );
	checkFMOD( context.shutdown() == SIR_FMOD_OK );
	checkFMOD( fixture.nReleaseCalls == 2 );
}

static void testUpdateFailures( void )
{
	sirFMODFixture_t fixture = {};
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 128, -1, 0, 0, 0, 0 };
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_OK );
	fixture.eFailure = SIR_FMOD_FIXTURE_UPDATE;
	checkFMOD( context.update() == SIR_FMOD_NATIVE_ERROR );
	checkFMOD( context.getLastNativeResult() == -42 );
	checkFMOD( context.isReady() );
	checkFMOD( context.update() == SIR_FMOD_OK );
	fixture.eFailure = SIR_FMOD_FIXTURE_DRIVER_COUNT;
	checkFMOD( context.changeDriver( 1 ) == SIR_FMOD_NATIVE_ERROR );
	fixture.eFailure = SIR_FMOD_FIXTURE_DRIVER;
	checkFMOD( context.changeDriver( 1 ) == SIR_FMOD_NATIVE_ERROR );
	checkFMOD( context.shutdown() == SIR_FMOD_OK );
}

static void testInvalidConfiguration( void )
{
	sirFMODFixture_t fixture = {};
	SIRFMODContext context;
	sirFMODContextConfig_t config = { 0, -1, 0, 0, 0, 0 };
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_INVALID_ARGUMENT );
	config.nMaxChannels = 4096;
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_INVALID_ARGUMENT );
	config.nMaxChannels = 256;
	config.iOutputDriver = -2;
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_INVALID_ARGUMENT );
	config.iOutputDriver = 1;
	config.bNoSound = 1;
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_INVALID_ARGUMENT );
	config.iOutputDriver = 5;
	config.bNoSound = 0;
	checkFMOD( context.initialize( config, fakeOps( fixture ) ) == SIR_FMOD_INVALID_DRIVER );
	checkFMOD( fixture.nReleaseCalls == 1 );
	config.iOutputDriver = -1;
	sirFMODNativeOps_t ops = fakeOps( fixture );
	ops.nAbiVersion = 99;
	checkFMOD( context.initialize( config, ops ) == SIR_FMOD_INVALID_ARGUMENT );
	ops = fakeOps( fixture );
	ops.createStudio = nullptr;
	checkFMOD( context.initialize( config, ops ) == SIR_FMOD_INVALID_ARGUMENT );
	checkFMOD( fixture.nCreateCalls == 1 );
}

int main( void )
{
	testContextLifecycle();
	testFailedCreationAndRollback();
	testMismatchAndNoSound();
	testReleaseFailure();
	testUpdateFailures();
	testInvalidConfiguration();
	return 0;
}
