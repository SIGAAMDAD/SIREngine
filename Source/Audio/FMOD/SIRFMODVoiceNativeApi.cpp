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

sirFMODVoiceNativeOps_t sirGetFMODVoiceNativeOps( void ) noexcept
{
	return { 1, nullptr, fmodResolveEvent, fmodCreateInstance, fmodStartInstance, fmodStopInstance, fmodIsStopped, fmodReleaseInstance };
}
