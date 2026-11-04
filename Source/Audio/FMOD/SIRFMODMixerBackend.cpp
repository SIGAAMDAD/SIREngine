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
#include <Audio/FMOD/SIRFMODBankStore.h>

#include <Core/Containers/SIRRawStorage.h>
#include <cmath>
#include <string.h>

SIRFMODMixerBackend::~SIRFMODMixerBackend( void ) noexcept
{
	shutdown();
}

bool SIRFMODMixerBackend::isReady( void ) const noexcept
{
	return m_bInitialized && m_pContext != nullptr && m_pContext->isReady() && m_pContext->getEpoch() == m_nContextEpoch;
}

sirAudioResult_t SIRFMODMixerBackend::configure( SIRFMODContext *pContext, const sirContainerAllocator_t *pAllocator,
                                                 const sirFMODMixerNativeOps_t &ops, uint32_t nMaxControls ) noexcept
{
	if ( m_pContext != nullptr || m_bInitialized || !SIRRawStorage::isValidAllocator( pAllocator ) || pContext == nullptr ||
	     !pContext->isReady() || nMaxControls == 0 || ops.nAbiVersion != 1 || ops.resolveBus == nullptr || ops.resolveVCA == nullptr ||
	     ops.setBusVolume == nullptr || ops.setBusMute == nullptr || ops.setVCAVolume == nullptr ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	m_pContext = pContext;
	m_pAllocator = pAllocator;
	m_Ops = ops;
	m_nCapacity = nMaxControls;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODMixerBackend::initialize( void ) noexcept
{
	if ( m_pContext == nullptr || m_pAllocator == nullptr || m_nCapacity == 0 || m_bInitialized || !m_pContext->isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}

	size_t nBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( m_nCapacity, sizeof( sirFMODMixerControl_t ), &nBytes ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( sirFMODMixerControl_t ), &pMemory ) != SIR_CONTAINER_OK ) {
		return SIR_AUDIO_CAPACITY;
	}

	m_pControls = ( sirFMODMixerControl_t * )pMemory;
	memset( m_pControls, 0, nBytes );
	m_nCount = 0;
	m_nContextEpoch = m_pContext->getEpoch();
	m_nLastNativeResult = 0;
	m_bInitialized = true;
	return SIR_AUDIO_OK;
}

void SIRFMODMixerBackend::shutdown( void ) noexcept
{
	if ( m_nCount != 0 && !isReady() ) {
		abort();
	}
	for ( uint32_t iControl = 0; iControl < m_nCount; ++iControl ) {
		const sirFMODMixerControl_t &control = m_pControls[iControl];
		if ( control.pBankStore != nullptr ) {
			control.pBankStore->unpinBankObject( control.nPackageId );
		}
		m_pContext->unpinBorrowedBankObject( control.pBankStore != nullptr );
	}
	if ( m_pControls != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pControls, ( size_t )m_nCapacity * sizeof( sirFMODMixerControl_t ),
		                           alignof( sirFMODMixerControl_t ) );
	}

	m_pControls = nullptr;
	m_pContext = nullptr;
	m_pAllocator = nullptr;
	m_Ops = {};
	m_nContextEpoch = 0;
	m_nCapacity = 0;
	m_nCount = 0;
	m_nLastNativeResult = 0;
	m_bInitialized = false;
}

bool SIRFMODMixerBackend::findControl( uint64_t nControlId, uint32_t &iIndex ) const noexcept
{
	uint32_t iLow = 0;
	uint32_t iHigh = m_nCount;
	while ( iLow < iHigh ) {
		const uint32_t iMiddle = iLow + ( iHigh - iLow ) / 2;
		if ( m_pControls[iMiddle].nControlId < nControlId ) {
			iLow = iMiddle + 1;
		} else {
			iHigh = iMiddle;
		}
	}
	iIndex = iLow;
	return iLow < m_nCount && m_pControls[iLow].nControlId == nControlId;
}

sirAudioResult_t SIRFMODMixerBackend::registerControl( uint64_t nControlId, const char *szPath, sirFMODMixerControlKind_t eKind,
                                                       SIRFMODBankStore *pBankStore, uint64_t nPackageId ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nControlId == 0 || szPath == nullptr || szPath[0] == '\0' || ( pBankStore == nullptr && nPackageId != 0 ) ||
	     ( pBankStore != nullptr && nPackageId == 0 ) || ( pBankStore != nullptr && !pBankStore->isBoundTo( m_pContext ) ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	uint32_t iInsert = 0;
	if ( findControl( nControlId, iInsert ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_nCount == m_nCapacity ) {
		return SIR_AUDIO_CAPACITY;
	}

	void *pNative = nullptr;
	if ( eKind == SIR_FMOD_MIXER_BUS ) {
		m_nLastNativeResult = m_Ops.resolveBus( m_Ops.pUserData, m_pContext->m_pStudio, szPath, &pNative );
	} else {
		m_nLastNativeResult = m_Ops.resolveVCA( m_Ops.pUserData, m_pContext->m_pStudio, szPath, &pNative );
	}
	if ( m_nLastNativeResult != 0 || pNative == nullptr ) {
		return SIR_AUDIO_BACKEND_ERROR;
	}

	if ( pBankStore != nullptr ) {
		const sirAudioResult_t ePin = pBankStore->pinBankObject( nPackageId );
		if ( ePin != SIR_AUDIO_OK ) {
			return ePin;
		}
	}
	if ( !m_pContext->pinBorrowedBankObject( pBankStore != nullptr ) ) {
		if ( pBankStore != nullptr ) {
			pBankStore->unpinBankObject( nPackageId );
		}
		return SIR_AUDIO_CAPACITY;
	}

	if ( iInsert < m_nCount ) {
		memmove( m_pControls + iInsert + 1, m_pControls + iInsert, ( size_t )( m_nCount - iInsert ) * sizeof( sirFMODMixerControl_t ) );
	}
	m_pControls[iInsert] = { nControlId, nPackageId, pNative, pBankStore, eKind, {} };
	++m_nCount;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODMixerBackend::registerBus( uint64_t nControlId, const char *szPath ) noexcept
{
	return registerControl( nControlId, szPath, SIR_FMOD_MIXER_BUS );
}

sirAudioResult_t SIRFMODMixerBackend::registerVCA( uint64_t nControlId, const char *szPath ) noexcept
{
	return registerControl( nControlId, szPath, SIR_FMOD_MIXER_VCA );
}

sirAudioResult_t SIRFMODMixerBackend::registerBusFromBank( uint64_t nControlId, uint64_t nPackageId, SIRFMODBankStore *pBankStore,
                                                           const char *szPath ) noexcept
{
	return registerControl( nControlId, szPath, SIR_FMOD_MIXER_BUS, pBankStore, nPackageId );
}

sirAudioResult_t SIRFMODMixerBackend::registerVCAFromBank( uint64_t nControlId, uint64_t nPackageId, SIRFMODBankStore *pBankStore,
                                                           const char *szPath ) noexcept
{
	return registerControl( nControlId, szPath, SIR_FMOD_MIXER_VCA, pBankStore, nPackageId );
}

sirAudioResult_t SIRFMODMixerBackend::unregisterControl( uint64_t nControlId ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iIndex = 0;
	if ( !findControl( nControlId, iIndex ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	const sirFMODMixerControl_t control = m_pControls[iIndex];
	if ( iIndex + 1 < m_nCount ) {
		memmove( m_pControls + iIndex, m_pControls + iIndex + 1, ( size_t )( m_nCount - iIndex - 1 ) * sizeof( sirFMODMixerControl_t ) );
	}
	--m_nCount;
	if ( control.pBankStore != nullptr ) {
		control.pBankStore->unpinBankObject( control.nPackageId );
	}
	m_pContext->unpinBorrowedBankObject( control.pBankStore != nullptr );
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODMixerBackend::setVolume( uint64_t nControlId, float fVolume, sirFMODMixerControlKind_t eKind ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( !std::isfinite( fVolume ) || fVolume < 0.0f || fVolume > 1.0f ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	uint32_t iIndex = 0;
	if ( !findControl( nControlId, iIndex ) || m_pControls[iIndex].eKind != eKind ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	if ( eKind == SIR_FMOD_MIXER_BUS ) {
		m_nLastNativeResult = m_Ops.setBusVolume( m_Ops.pUserData, m_pControls[iIndex].pNative, fVolume );
	} else {
		m_nLastNativeResult = m_Ops.setVCAVolume( m_Ops.pUserData, m_pControls[iIndex].pNative, fVolume );
	}
	return m_nLastNativeResult == 0 ? SIR_AUDIO_OK : SIR_AUDIO_BACKEND_ERROR;
}

sirAudioResult_t SIRFMODMixerBackend::setBusVolume( uint64_t nControlId, float fVolume ) noexcept
{
	return setVolume( nControlId, fVolume, SIR_FMOD_MIXER_BUS );
}

sirAudioResult_t SIRFMODMixerBackend::setVCAVolume( uint64_t nControlId, float fVolume ) noexcept
{
	return setVolume( nControlId, fVolume, SIR_FMOD_MIXER_VCA );
}

sirAudioResult_t SIRFMODMixerBackend::setBusMute( uint64_t nControlId, bool bMute ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}

	uint32_t iIndex = 0;
	if ( !findControl( nControlId, iIndex ) || m_pControls[iIndex].eKind != SIR_FMOD_MIXER_BUS ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	m_nLastNativeResult = m_Ops.setBusMute( m_Ops.pUserData, m_pControls[iIndex].pNative, bMute ? 1 : 0 );
	return m_nLastNativeResult == 0 ? SIR_AUDIO_OK : SIR_AUDIO_BACKEND_ERROR;
}

bool SIRFMODMixerBackend::isBoundTo( const SIRFMODContext *pContext ) const noexcept
{
	return m_pContext == pContext && isReady();
}

uint32_t SIRFMODMixerBackend::getControlCount( void ) const noexcept
{
	return m_nCount;
}

int32_t SIRFMODMixerBackend::getLastNativeResult( void ) const noexcept
{
	return m_nLastNativeResult;
}
