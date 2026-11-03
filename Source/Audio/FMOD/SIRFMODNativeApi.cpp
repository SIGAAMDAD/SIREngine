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

// The licensed Studio SDK is required only for this provider-private TU.
#include <fmod.hpp>
#include <fmod_studio.hpp>

static int32_t fmodCreateStudio( void *, void **ppStudio ) noexcept
{
	FMOD::Studio::System *pStudio = nullptr;
	const FMOD_RESULT eResult = FMOD::Studio::System::create( &pStudio, FMOD_VERSION );
	*ppStudio = pStudio;
	return ( int32_t )eResult;
}

static int32_t fmodGetCore( void *, void *pStudio, void **ppCore ) noexcept
{
	FMOD::System *pCore = nullptr;
	const FMOD_RESULT eResult = ( ( FMOD::Studio::System * )pStudio )->getCoreSystem( &pCore );
	*ppCore = pCore;
	return ( int32_t )eResult;
}

static int32_t fmodGetVersion( void *, void *pCore, uint32_t *pVersion ) noexcept
{
	unsigned int nVersion = 0;
	const FMOD_RESULT eResult = ( ( FMOD::System * )pCore )->getVersion( &nVersion );
	*pVersion = ( uint32_t )nVersion;
	return ( int32_t )eResult;
}

static int32_t fmodGetDriverCount( void *, void *pCore, int32_t *pCount ) noexcept
{
	int nCount = 0;
	const FMOD_RESULT eResult = ( ( FMOD::System * )pCore )->getNumDrivers( &nCount );
	*pCount = ( int32_t )nCount;
	return ( int32_t )eResult;
}

static int32_t fmodSetDriver( void *, void *pCore, int32_t iDriver ) noexcept
{
	return ( int32_t )( ( FMOD::System * )pCore )->setDriver( ( int )iDriver );
}

static int32_t fmodSetNoSound( void *, void *pCore ) noexcept
{
	return ( int32_t )( ( FMOD::System * )pCore )->setOutput( FMOD_OUTPUTTYPE_NOSOUND );
}

static int32_t fmodInitializeStudio( void *, void *pStudio, const sirFMODContextConfig_t *pConfig ) noexcept
{
	FMOD_STUDIO_INITFLAGS eStudioFlags = FMOD_STUDIO_INIT_DEFERRED_CALLBACKS;
	if ( pConfig->bSynchronousUpdate != 0 ) {
		eStudioFlags = ( FMOD_STUDIO_INITFLAGS )( eStudioFlags | FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE );
	}
	FMOD_INITFLAGS eCoreFlags = FMOD_INIT_NORMAL;
	if ( pConfig->bRightHanded != 0 ) {
		eCoreFlags = ( FMOD_INITFLAGS )( eCoreFlags | FMOD_INIT_3D_RIGHTHANDED );
	}
	return ( int32_t )( ( FMOD::Studio::System * )pStudio )->initialize( ( int )pConfig->nMaxChannels, eStudioFlags, eCoreFlags, nullptr );
}

static int32_t fmodUpdateStudio( void *, void *pStudio ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::System * )pStudio )->update();
}

static int32_t fmodReleaseStudio( void *, void *pStudio ) noexcept
{
	// Studio owns Core; never call Core::release() on the borrowed pointer.
	return ( int32_t )( ( FMOD::Studio::System * )pStudio )->release();
}

sirFMODNativeOps_t sirGetFMODNativeOps( void ) noexcept
{
	return { 1,
	         FMOD_VERSION,
	         nullptr,
	         fmodCreateStudio,
	         fmodGetCore,
	         fmodGetVersion,
	         fmodGetDriverCount,
	         fmodSetDriver,
	         fmodSetNoSound,
	         fmodInitializeStudio,
	         fmodUpdateStudio,
	         fmodReleaseStudio };
}
