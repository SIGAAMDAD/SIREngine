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
#include <Audio/FMOD/SIRFMODBankStore.h>

#include <Core/Containers/SIRRawStorage.h>
#include <stdlib.h>
#include <string.h>

SIRFMODSnapshotBackend::~SIRFMODSnapshotBackend( void ) noexcept
{
	if ( shutdown() != SIR_AUDIO_OK ) {
		abort();
	}
}

bool SIRFMODSnapshotBackend::isReady( void ) const noexcept
{
	return m_bInitialized && m_pContext != nullptr && m_pContext->isReady() && m_pContext->getEpoch() == m_nContextEpoch;
}

sirAudioResult_t SIRFMODSnapshotBackend::configure( SIRFMODContext *pContext, const sirContainerAllocator_t *pAllocator,
                                                    const sirFMODSnapshotNativeOps_t &ops, uint32_t nMaxSnapshots ) noexcept
{
	if ( m_pContext != nullptr || m_bInitialized || pContext == nullptr || !pContext->isReady() ||
	     !SIRRawStorage::isValidAllocator( pAllocator ) || nMaxSnapshots == 0 || ops.nAbiVersion != 1 || ops.resolveSnapshot == nullptr ||
	     ops.createInstance == nullptr || ops.startInstance == nullptr || ops.stopInstance == nullptr || ops.releaseInstance == nullptr ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	m_pContext = pContext;
	m_pAllocator = pAllocator;
	m_Ops = ops;
	m_nCapacity = nMaxSnapshots;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODSnapshotBackend::initialize( void ) noexcept
{
	if ( m_pContext == nullptr || m_pAllocator == nullptr || m_bInitialized || m_nCapacity == 0 || !m_pContext->isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}

	size_t nBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( m_nCapacity, sizeof( sirFMODSnapshotRecord_t ), &nBytes ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( sirFMODSnapshotRecord_t ), &pMemory ) != SIR_CONTAINER_OK ) {
		return SIR_AUDIO_CAPACITY;
	}

	m_pSnapshots = ( sirFMODSnapshotRecord_t * )pMemory;
	memset( m_pSnapshots, 0, nBytes );
	m_nCount = 0;
	m_nContextEpoch = m_pContext->getEpoch();
	m_nLastNativeResult = 0;
	m_bInitialized = true;
	return SIR_AUDIO_OK;
}

void SIRFMODSnapshotBackend::releaseStorage( void ) noexcept
{
	if ( m_pSnapshots != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pSnapshots, ( size_t )m_nCapacity * sizeof( sirFMODSnapshotRecord_t ),
		                           alignof( sirFMODSnapshotRecord_t ) );
	}
	m_pSnapshots = nullptr;
	m_pContext = nullptr;
	m_pAllocator = nullptr;
	m_Ops = {};
	m_nContextEpoch = 0;
	m_nCapacity = 0;
	m_nCount = 0;
	m_nLastNativeResult = 0;
	m_bInitialized = false;
}

sirAudioResult_t SIRFMODSnapshotBackend::releaseInstance( sirFMODSnapshotRecord_t &record, bool bStop, sirAudioStopMode_t eMode ) noexcept
{
	if ( record.pInstance == nullptr ) {
		return SIR_AUDIO_OK;
	}

	if ( bStop && record.bStarted != 0 ) {
		m_nLastNativeResult = m_Ops.stopInstance( m_Ops.pUserData, record.pInstance, eMode );
		if ( m_nLastNativeResult != 0 ) {
			return SIR_AUDIO_BACKEND_ERROR;
		}
		record.bStarted = 0;
	}

	m_nLastNativeResult = m_Ops.releaseInstance( m_Ops.pUserData, record.pInstance );
	if ( m_nLastNativeResult != 0 ) {
		return SIR_AUDIO_BACKEND_ERROR;
	}
	record.pInstance = nullptr;
	record.bStarted = 0;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODSnapshotBackend::shutdown( void ) noexcept
{
	if ( m_pSnapshots == nullptr ) {
		releaseStorage();
		return SIR_AUDIO_OK;
	}
	if ( !isReady() ) {
		for ( uint32_t iSnapshot = 0; iSnapshot < m_nCount; ++iSnapshot ) {
			if ( m_pSnapshots[iSnapshot].pInstance != nullptr ) {
				return SIR_AUDIO_NOT_INITIALIZED;
			}
		}
	} else {
		for ( uint32_t iSnapshot = 0; iSnapshot < m_nCount; ++iSnapshot ) {
			const sirAudioResult_t eResult = releaseInstance( m_pSnapshots[iSnapshot], true, SIR_AUDIO_STOP_IMMEDIATE );
			if ( eResult != SIR_AUDIO_OK ) {
				return eResult;
			}
		}
	}
	for ( uint32_t iSnapshot = 0; iSnapshot < m_nCount; ++iSnapshot ) {
		const sirFMODSnapshotRecord_t &record = m_pSnapshots[iSnapshot];
		if ( record.pBankStore != nullptr ) {
			record.pBankStore->unpinBankObject( record.nPackageId );
		}
		m_pContext->unpinBorrowedBankObject( record.pBankStore != nullptr );
	}
	releaseStorage();
	return SIR_AUDIO_OK;
}

bool SIRFMODSnapshotBackend::findSnapshot( uint64_t nSnapshotId, uint32_t &iIndex ) const noexcept
{
	uint32_t iLow = 0;
	uint32_t iHigh = m_nCount;
	while ( iLow < iHigh ) {
		const uint32_t iMiddle = iLow + ( iHigh - iLow ) / 2;
		if ( m_pSnapshots[iMiddle].nSnapshotId < nSnapshotId ) {
			iLow = iMiddle + 1;
		} else {
			iHigh = iMiddle;
		}
	}
	iIndex = iLow;
	return iLow < m_nCount && m_pSnapshots[iLow].nSnapshotId == nSnapshotId;
}

sirAudioResult_t SIRFMODSnapshotBackend::registerSnapshot( uint64_t nSnapshotId, const char *szPath ) noexcept
{
	return registerSnapshotInternal( nSnapshotId, szPath, nullptr, 0 );
}

sirAudioResult_t SIRFMODSnapshotBackend::registerSnapshotFromBank( uint64_t nSnapshotId, uint64_t nPackageId, SIRFMODBankStore *pBankStore,
                                                                   const char *szPath ) noexcept
{
	return registerSnapshotInternal( nSnapshotId, szPath, pBankStore, nPackageId );
}

sirAudioResult_t SIRFMODSnapshotBackend::registerSnapshotInternal( uint64_t nSnapshotId, const char *szPath, SIRFMODBankStore *pBankStore,
                                                                   uint64_t nPackageId ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( nSnapshotId == 0 || szPath == nullptr || strncmp( szPath, "snapshot:/", 10 ) != 0 || szPath[10] == '\0' ||
	     ( pBankStore == nullptr && nPackageId != 0 ) || ( pBankStore != nullptr && nPackageId == 0 ) ||
	     ( pBankStore != nullptr && !pBankStore->isBoundTo( m_pContext ) ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	uint32_t iInsert = 0;
	if ( findSnapshot( nSnapshotId, iInsert ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_nCount >= m_nCapacity ) {
		return SIR_AUDIO_CAPACITY;
	}

	if ( pBankStore != nullptr ) {
		const sirAudioResult_t eMembership =
		    pBankStore->containsBankObject( nPackageId, SIR_FMOD_BANK_OBJECT_SNAPSHOT, szPath );
		if ( eMembership != SIR_AUDIO_OK ) {
			return eMembership;
		}
	}
	void *pDescription = nullptr;
	m_nLastNativeResult = m_Ops.resolveSnapshot( m_Ops.pUserData, m_pContext->m_pStudio, szPath, &pDescription );
	if ( m_nLastNativeResult != 0 || pDescription == nullptr ) {
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
		memmove( m_pSnapshots + iInsert + 1, m_pSnapshots + iInsert, ( size_t )( m_nCount - iInsert ) * sizeof( sirFMODSnapshotRecord_t ) );
	}
	m_pSnapshots[iInsert] = { nSnapshotId, nPackageId, pDescription, nullptr, pBankStore, 0, {} };
	++m_nCount;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODSnapshotBackend::unregisterSnapshot( uint64_t nSnapshotId ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iIndex = 0;
	if ( !findSnapshot( nSnapshotId, iIndex ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( m_pSnapshots[iIndex].pInstance != nullptr ) {
		return SIR_AUDIO_BUSY;
	}

	const sirFMODSnapshotRecord_t record = m_pSnapshots[iIndex];
	if ( iIndex + 1 < m_nCount ) {
		memmove( m_pSnapshots + iIndex, m_pSnapshots + iIndex + 1,
		         ( size_t )( m_nCount - iIndex - 1 ) * sizeof( sirFMODSnapshotRecord_t ) );
	}
	--m_nCount;
	if ( record.pBankStore != nullptr ) {
		record.pBankStore->unpinBankObject( record.nPackageId );
	}
	m_pContext->unpinBorrowedBankObject( record.pBankStore != nullptr );
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODSnapshotBackend::startSnapshot( uint64_t nSnapshotId ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iIndex = 0;
	if ( !findSnapshot( nSnapshotId, iIndex ) ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	sirFMODSnapshotRecord_t &record = m_pSnapshots[iIndex];
	if ( record.pInstance != nullptr ) {
		return SIR_AUDIO_BUSY;
	}

	void *pInstance = nullptr;
	m_nLastNativeResult = m_Ops.createInstance( m_Ops.pUserData, record.pDescription, &pInstance );
	const int32_t nCreateResult = m_nLastNativeResult;
	if ( nCreateResult != 0 || pInstance == nullptr ) {
		if ( pInstance != nullptr ) {
			record.pInstance = pInstance;
			const sirAudioResult_t eRelease = releaseInstance( record, false, SIR_AUDIO_STOP_IMMEDIATE );
			if ( eRelease != SIR_AUDIO_OK ) {
				return eRelease;
			}
		}
		m_nLastNativeResult = nCreateResult;
		return SIR_AUDIO_BACKEND_ERROR;
	}

	record.pInstance = pInstance;
	m_nLastNativeResult = m_Ops.startInstance( m_Ops.pUserData, pInstance );
	if ( m_nLastNativeResult != 0 ) {
		const int32_t nStartResult = m_nLastNativeResult;
		const sirAudioResult_t eRelease = releaseInstance( record, false, SIR_AUDIO_STOP_IMMEDIATE );
		if ( eRelease != SIR_AUDIO_OK ) {
			return eRelease;
		}
		m_nLastNativeResult = nStartResult;
		return SIR_AUDIO_BACKEND_ERROR;
	}
	record.bStarted = 1;
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODSnapshotBackend::stopSnapshot( uint64_t nSnapshotId, sirAudioStopMode_t eMode ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( eMode != SIR_AUDIO_STOP_ALLOW_FADE && eMode != SIR_AUDIO_STOP_IMMEDIATE ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	uint32_t iIndex = 0;
	if ( !findSnapshot( nSnapshotId, iIndex ) || m_pSnapshots[iIndex].pInstance == nullptr ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	return releaseInstance( m_pSnapshots[iIndex], true, eMode );
}

bool SIRFMODSnapshotBackend::isBoundTo( const SIRFMODContext *pContext ) const noexcept
{
	return m_pContext == pContext && isReady();
}

uint32_t SIRFMODSnapshotBackend::getSnapshotCount( void ) const noexcept
{
	return m_nCount;
}

uint32_t SIRFMODSnapshotBackend::getActiveInstanceCount( void ) const noexcept
{
	uint32_t nCount = 0;
	for ( uint32_t iSnapshot = 0; iSnapshot < m_nCount; ++iSnapshot ) {
		if ( m_pSnapshots[iSnapshot].pInstance != nullptr ) {
			++nCount;
		}
	}
	return nCount;
}

int32_t SIRFMODSnapshotBackend::getLastNativeResult( void ) const noexcept
{
	return m_nLastNativeResult;
}
