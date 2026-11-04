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

static int32_t fmodCheckBankObject( void *, void *pStudio, void *pBank, sirFMODBankObjectKind_t eKind, const char *szPath,
                                    const sirContainerAllocator_t *pAllocator, uint8_t *pbFound ) noexcept
{
	*pbFound = 0;
	FMOD::Studio::System *pSystem = ( FMOD::Studio::System * )pStudio;
	FMOD::Studio::Bank *pNativeBank = ( FMOD::Studio::Bank * )pBank;
	void *pTarget = nullptr;
	int nCount = 0;
	FMOD_RESULT eResult = FMOD_OK;
	if ( eKind == SIR_FMOD_BANK_OBJECT_BUS ) {
		FMOD::Studio::Bus *pBus = nullptr;
		eResult = pSystem->getBus( szPath, &pBus );
		if ( eResult != FMOD_OK ) {
			return ( int32_t )eResult;
		}
		pTarget = pBus;
		eResult = pNativeBank->getBusCount( &nCount );
	} else if ( eKind == SIR_FMOD_BANK_OBJECT_VCA ) {
		FMOD::Studio::VCA *pVCA = nullptr;
		eResult = pSystem->getVCA( szPath, &pVCA );
		if ( eResult != FMOD_OK ) {
			return ( int32_t )eResult;
		}
		pTarget = pVCA;
		eResult = pNativeBank->getVCACount( &nCount );
	} else if ( eKind == SIR_FMOD_BANK_OBJECT_SNAPSHOT ) {
		FMOD::Studio::EventDescription *pEvent = nullptr;
		eResult = pSystem->getEvent( szPath, &pEvent );
		if ( eResult != FMOD_OK ) {
			return ( int32_t )eResult;
		}
		pTarget = pEvent;
		eResult = pNativeBank->getEventCount( &nCount );
	} else {
		return ( int32_t )FMOD_ERR_INVALID_PARAM;
	}
	if ( eResult != FMOD_OK || nCount <= 0 ) {
		return ( int32_t )eResult;
	}
	size_t nBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( ( size_t )nCount, sizeof( void * ), &nBytes ) ) {
		return ( int32_t )FMOD_ERR_MEMORY;
	}
	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, nBytes, alignof( void * ), &pMemory ) != SIR_CONTAINER_OK ) {
		return ( int32_t )FMOD_ERR_MEMORY;
	}
	int nActual = 0;
	if ( eKind == SIR_FMOD_BANK_OBJECT_BUS ) {
		FMOD::Studio::Bus **ppObjects = ( FMOD::Studio::Bus ** )pMemory;
		eResult = pNativeBank->getBusList( ppObjects, nCount, &nActual );
		if ( eResult == FMOD_OK ) {
			for ( int iObject = 0; iObject < nActual && iObject < nCount; ++iObject ) {
				if ( ppObjects[iObject] == pTarget ) {
					*pbFound = 1;
					break;
				}
			}
		}
	} else if ( eKind == SIR_FMOD_BANK_OBJECT_VCA ) {
		FMOD::Studio::VCA **ppObjects = ( FMOD::Studio::VCA ** )pMemory;
		eResult = pNativeBank->getVCAList( ppObjects, nCount, &nActual );
		if ( eResult == FMOD_OK ) {
			for ( int iObject = 0; iObject < nActual && iObject < nCount; ++iObject ) {
				if ( ppObjects[iObject] == pTarget ) {
					*pbFound = 1;
					break;
				}
			}
		}
	} else {
		FMOD::Studio::EventDescription **ppObjects = ( FMOD::Studio::EventDescription ** )pMemory;
		eResult = pNativeBank->getEventList( ppObjects, nCount, &nActual );
		if ( eResult == FMOD_OK ) {
			for ( int iObject = 0; iObject < nActual && iObject < nCount; ++iObject ) {
				if ( ppObjects[iObject] == pTarget ) {
					*pbFound = 1;
					break;
				}
			}
		}
	}
	SIRRawStorage::deallocate( pAllocator, pMemory, nBytes, alignof( void * ) );
	return ( int32_t )eResult;
}

static int32_t fmodLoadBankFileAsync( void *, void *pStudio, const char *szPath, void **ppBank ) noexcept
{
	FMOD::Studio::Bank *pBank = nullptr;
	const FMOD_RESULT eResult =
	    ( ( FMOD::Studio::System * )pStudio )->loadBankFile( szPath, FMOD_STUDIO_LOAD_BANK_NONBLOCKING, &pBank );
	*ppBank = pBank;
	return ( int32_t )eResult;
}

static int32_t fmodGetBankLoadingState( void *, void *pBank, sirFMODSampleState_t *pState ) noexcept
{
	FMOD_STUDIO_LOADING_STATE eNativeState = FMOD_STUDIO_LOADING_STATE_UNLOADED;
	const FMOD_RESULT eResult = ( ( FMOD::Studio::Bank * )pBank )->getLoadingState( &eNativeState );
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

sirFMODBankNativeOps_t sirGetFMODBankNativeOps( void ) noexcept
{
	return { 3,
	         nullptr,
	         fmodLoadBankFile,
	         fmodUnloadBank,
	         fmodPreloadBankSamples,
	         fmodGetBankSampleState,
	         fmodCheckBankEvent,
	         fmodCheckBankObject,
	         fmodLoadBankFileAsync,
	         fmodGetBankLoadingState };
}
