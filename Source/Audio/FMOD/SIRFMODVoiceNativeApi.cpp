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
#include <fmod_studio.hpp>

static int32_t fmodResolveEvent( void *, void *pStudio, const char *szPath, void **ppDescription ) noexcept
{
	FMOD::Studio::EventDescription *pDescription = nullptr;
	const FMOD_RESULT eResult = ( ( FMOD::Studio::System * )pStudio )->getEvent( szPath, &pDescription );
	*ppDescription = pDescription;
	return ( int32_t )eResult;
}

static int32_t fmodCreateInstance( void *, void *pDescription, void **ppInstance ) noexcept
{
	FMOD::Studio::EventInstance *pInstance = nullptr;
	const FMOD_RESULT eResult = ( ( FMOD::Studio::EventDescription * )pDescription )->createInstance( &pInstance );
	*ppInstance = pInstance;
	return ( int32_t )eResult;
}

static int32_t fmodStartInstance( void *, void *pInstance ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->start();
}

static int32_t fmodStopInstance( void *, void *pInstance, sirAudioStopMode_t eMode ) noexcept
{
	const FMOD_STUDIO_STOP_MODE eNativeMode =
	    eMode == SIR_AUDIO_STOP_IMMEDIATE ? FMOD_STUDIO_STOP_IMMEDIATE : FMOD_STUDIO_STOP_ALLOWFADEOUT;
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->stop( eNativeMode );
}

static int32_t fmodIsStopped( void *, void *pInstance, uint8_t *pbStopped ) noexcept
{
	FMOD_STUDIO_PLAYBACK_STATE eState = FMOD_STUDIO_PLAYBACK_STOPPED;
	const FMOD_RESULT eResult = ( ( FMOD::Studio::EventInstance * )pInstance )->getPlaybackState( &eState );
	*pbStopped = eState == FMOD_STUDIO_PLAYBACK_STOPPED ? 1 : 0;
	return ( int32_t )eResult;
}

static int32_t fmodReleaseInstance( void *, void *pInstance ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->release();
}

// FMOD_STUDIO_PARAMETER_ID is a pair of uint32_t values.
static FMOD_STUDIO_PARAMETER_ID fmodUnpackParameterId( uint64_t nNativeId ) noexcept
{
	FMOD_STUDIO_PARAMETER_ID id = {};
	id.data1 = ( unsigned int )nNativeId;
	id.data2 = ( unsigned int )( nNativeId >> 32 );
	return id;
}

static int32_t fmodSetInstanceParameter( void *, void *pInstance, uint64_t nNativeId, float fValue ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->setParameterByID( fmodUnpackParameterId( nNativeId ), fValue );
}

static int32_t fmodSetGlobalParameter( void *, void *pStudio, uint64_t nNativeId, float fValue ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::System * )pStudio )->setParameterByID( fmodUnpackParameterId( nNativeId ), fValue );
}

static FMOD_VECTOR fmodSpatialVector( const float *pValues ) noexcept
{
	FMOD_VECTOR vector = {};
	vector.x = pValues[0];
	vector.y = pValues[1];
	vector.z = pValues[2];
	return vector;
}

static FMOD_3D_ATTRIBUTES fmodSpatialAttributes( const sirAudioAttributes_t *pAttributes ) noexcept
{
	FMOD_3D_ATTRIBUTES attributes = {};
	attributes.position = fmodSpatialVector( pAttributes->fPosition );
	attributes.velocity = fmodSpatialVector( pAttributes->fVelocity );
	attributes.forward = fmodSpatialVector( pAttributes->fForward );
	attributes.up = fmodSpatialVector( pAttributes->fUp );
	return attributes;
}

static int32_t fmodSetInstanceSpatial( void *, void *pInstance, const sirAudioAttributes_t *pAttributes ) noexcept
{
	const FMOD_3D_ATTRIBUTES attributes = fmodSpatialAttributes( pAttributes );
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->set3DAttributes( &attributes );
}

static int32_t fmodSetListenerSpatial( void *, void *pStudio, uint32_t iListener, const sirAudioAttributes_t *pAttributes ) noexcept
{
	const FMOD_3D_ATTRIBUTES attributes = fmodSpatialAttributes( pAttributes );
	return ( int32_t )( ( FMOD::Studio::System * )pStudio )->setListenerAttributes( ( int )iListener, &attributes );
}

static int32_t fmodSetListenerCount( void *, void *pStudio, uint32_t nListeners ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::System * )pStudio )->setNumListeners( ( int )nListeners );
}

static int32_t fmodSetListenerWeight( void *, void *pStudio, uint32_t iListener, float fWeight ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::System * )pStudio )->setListenerWeight( ( int )iListener, fWeight );
}

static int32_t fmodSetInstanceVolume( void *, void *pInstance, float fVolume ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->setVolume( fVolume );
}

static int32_t fmodSetInstancePitch( void *, void *pInstance, float fPitch ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->setPitch( fPitch );
}

static int32_t fmodSetInstancePaused( void *, void *pInstance, uint8_t bPaused ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->setPaused( bPaused != 0 );
}

static int32_t fmodSetInstanceTimeline( void *, void *pInstance, int32_t nMilliseconds ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->setTimelinePosition( nMilliseconds );
}

static int32_t fmodGetInstanceTimeline( void *, void *pInstance, int32_t *pnMilliseconds ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::EventInstance * )pInstance )->getTimelinePosition( ( int * )pnMilliseconds );
}

sirFMODVoiceNativeOps_t sirGetFMODVoiceNativeOps( void ) noexcept
{
	return { 4,
	         nullptr,
	         fmodResolveEvent,
	         fmodCreateInstance,
	         fmodStartInstance,
	         fmodStopInstance,
	         fmodIsStopped,
	         fmodReleaseInstance,
	         fmodSetInstanceParameter,
	         fmodSetGlobalParameter,
	         fmodSetInstanceSpatial,
	         fmodSetListenerSpatial,
	         fmodSetListenerCount,
	         fmodSetListenerWeight,
	         fmodSetInstanceVolume,
	         fmodSetInstancePitch,
	         fmodSetInstancePaused,
	         fmodSetInstanceTimeline,
	         fmodGetInstanceTimeline };
}
