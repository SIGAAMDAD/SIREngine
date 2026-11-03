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
#include <fmod_studio.hpp>

static int32_t fmodLoadBankFile( void *, void *pStudio, const char *szPath, void **ppBank ) noexcept
{
	FMOD::Studio::Bank *pBank = nullptr;
	const FMOD_RESULT eResult = ( ( FMOD::Studio::System * )pStudio )->loadBankFile( szPath, FMOD_STUDIO_LOAD_BANK_NORMAL, &pBank );
	*ppBank = pBank;
	return ( int32_t )eResult;
}

static int32_t fmodUnloadBank( void *, void *pBank ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::Bank * )pBank )->unload();
}

static int32_t fmodPreloadBankSamples( void *, void *pBank ) noexcept
{
	return ( int32_t )( ( FMOD::Studio::Bank * )pBank )->loadSampleData();
}

static int32_t fmodGetBankSampleState( void *, void *pBank, sirFMODSampleState_t *pState ) noexcept
{
	FMOD_STUDIO_LOADING_STATE eNativeState = FMOD_STUDIO_LOADING_STATE_UNLOADED;
	const FMOD_RESULT eResult = ( ( FMOD::Studio::Bank * )pBank )->getSampleLoadingState( &eNativeState );
	if ( eResult != FMOD_OK ) {
		return ( int32_t )eResult;
	}
	if ( eNativeState == FMOD_STUDIO_LOADING_STATE_LOADED ) {
		*pState = SIR_FMOD_SAMPLE_READY;
	} else if ( eNativeState == FMOD_STUDIO_LOADING_STATE_LOADING ) {
		*pState = SIR_FMOD_SAMPLE_LOADING;
	} else if ( eNativeState == FMOD_STUDIO_LOADING_STATE_ERROR ) {
		*pState = SIR_FMOD_SAMPLE_ERROR;
	} else {
		*pState = SIR_FMOD_SAMPLE_UNLOADED;
	}
	return ( int32_t )FMOD_OK;
}

static int32_t fmodCheckBankEvent( void *, void *pStudio, void *pBank, const char *szPath, const sirContainerAllocator_t *pAllocator,
                                   uint8_t *pbFound ) noexcept
{
	*pbFound = 0;
	FMOD::Studio::EventDescription *pTarget = nullptr;
	FMOD_RESULT eResult = ( ( FMOD::Studio::System * )pStudio )->getEvent( szPath, &pTarget );
	if ( eResult != FMOD_OK ) {
		return ( int32_t )eResult;
	}
	int nEventCount = 0;
	eResult = ( ( FMOD::Studio::Bank * )pBank )->getEventCount( &nEventCount );
	if ( eResult != FMOD_OK ) {
		return ( int32_t )eResult;
	}
	if ( nEventCount <= 0 ) {
		return ( int32_t )FMOD_OK;
	}

	size_t nBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( ( size_t )nEventCount, sizeof( FMOD::Studio::EventDescription * ), &nBytes ) ) {
		return ( int32_t )FMOD_ERR_MEMORY;
	}
	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, nBytes, alignof( FMOD::Studio::EventDescription * ), &pMemory ) != SIR_CONTAINER_OK ) {
		return ( int32_t )FMOD_ERR_MEMORY;
	}

	int nActualCount = 0;
	eResult = ( ( FMOD::Studio::Bank * )pBank )->getEventList( ( FMOD::Studio::EventDescription ** )pMemory, nEventCount, &nActualCount );
	if ( eResult == FMOD_OK ) {
		FMOD::Studio::EventDescription **ppEvents = ( FMOD::Studio::EventDescription ** )pMemory;
		for ( int iEvent = 0; iEvent < nActualCount && iEvent < nEventCount; ++iEvent ) {
			if ( ppEvents[iEvent] == pTarget ) {
				*pbFound = 1;
				break;
			}
		}
	}
	SIRRawStorage::deallocate( pAllocator, pMemory, nBytes, alignof( FMOD::Studio::EventDescription * ) );
	return ( int32_t )eResult;
}

sirFMODBankNativeOps_t sirGetFMODBankNativeOps( void ) noexcept
{
	return { 1, nullptr, fmodLoadBankFile, fmodUnloadBank, fmodPreloadBankSamples, fmodGetBankSampleState, fmodCheckBankEvent };
}
