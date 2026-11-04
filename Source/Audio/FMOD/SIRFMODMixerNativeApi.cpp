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

#include <fmod_studio.hpp>

static int32_t fmodMixerResolveBus( void *, void *pStudio, const char *szPath, void **ppBus ) noexcept
{
	FMOD::Studio::Bus *pBus = nullptr;
	const FMOD_RESULT eResult = ( ( FMOD::Studio::System * )pStudio )->getBus( szPath, &pBus );
	*ppBus = pBus;
	return ( int32_t )eResult;
}

static int32_t fmodMixerResolveVCA( void *, void *pStudio, const char *szPath, void **ppVCA ) noexcept
{
	FMOD::Studio::VCA *pVCA = nullptr;
	const FMOD_RESULT eResult = ( ( FMOD::Studio::System * )pStudio )->getVCA( szPath, &pVCA );
	*ppVCA = pVCA;
	return ( int32_t )eResult;
}

static int32_t fmodMixerSetBusVolume( void *, void *pBus, float fVolume ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::Bus * )pBus )->setVolume( fVolume );
}

static int32_t fmodMixerSetBusMute( void *, void *pBus, uint8_t bMute ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::Bus * )pBus )->setMute( bMute != 0 );
}

static int32_t fmodMixerSetVCAVolume( void *, void *pVCA, float fVolume ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::VCA * )pVCA )->setVolume( fVolume );
}

sirFMODMixerNativeOps_t sirGetFMODMixerNativeOps( void ) noexcept
{
	return { 1, nullptr, fmodMixerResolveBus, fmodMixerResolveVCA, fmodMixerSetBusVolume, fmodMixerSetBusMute, fmodMixerSetVCAVolume };
}
