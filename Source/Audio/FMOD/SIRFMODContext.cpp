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

#include <Core/Platform/SIRAtomic.h>
#include <stdlib.h>
#include <limits.h>

static SIRAtomic<uint64_t> s_nFMODEpoch = SIRAtomic<uint64_t>( 1 );

SIRFMODContext::~SIRFMODContext( void ) noexcept
{
	if ( shutdown() != SIR_FMOD_OK ) {
		// The caller must resolve release failure before destroying the owner.
		abort();
	}
}

sirFMODContextResult_t SIRFMODContext::rollback( sirFMODContextResult_t eOriginalError, int32_t nNativeError ) noexcept
{
	const sirFMODContextResult_t eShutdown = shutdown();
	if ( eShutdown != SIR_FMOD_OK ) {
		return eShutdown;
	}
	m_nLastNativeResult = nNativeError;
	return eOriginalError;
}

sirFMODContextResult_t SIRFMODContext::initialize( const sirFMODContextConfig_t &config, const sirFMODNativeOps_t &ops ) noexcept
{
	if ( m_eState != SIR_FMOD_CONTEXT_EMPTY ) {
		return SIR_FMOD_BAD_STATE;
	}
	if ( config.nMaxChannels <= 0 || config.nMaxChannels > 4095 || config.iOutputDriver < -1 ||
	     ( config.bNoSound != 0 && config.iOutputDriver >= 0 ) || config.nReserved != 0 || ops.nAbiVersion != 1 || ops.nSDKVersion == 0 ||
	     ops.createStudio == nullptr || ops.getCore == nullptr || ops.getVersion == nullptr || ops.getDriverCount == nullptr ||
	     ops.setDriver == nullptr || ops.setNoSound == nullptr || ops.initializeStudio == nullptr || ops.updateStudio == nullptr ||
	     ops.releaseStudio == nullptr ) {
		return SIR_FMOD_INVALID_ARGUMENT;
	}

	m_Ops = ops;
	m_nLastNativeResult = 0;
	m_nRuntimeVersion = 0;
	void *pStudio = nullptr;
	const int32_t nCreateResult = m_Ops.createStudio( m_Ops.pUserData, &pStudio );
	if ( pStudio == nullptr ) {
		m_Ops = {};
		m_nLastNativeResult = nCreateResult;
		return SIR_FMOD_NATIVE_ERROR;
	}
	m_pStudio = pStudio;
	m_eState = SIR_FMOD_CONTEXT_CREATED;
	if ( nCreateResult != 0 ) {
		return rollback( SIR_FMOD_NATIVE_ERROR, nCreateResult );
	}

	void *pCore = nullptr;
	const int32_t nCoreResult = m_Ops.getCore( m_Ops.pUserData, m_pStudio, &pCore );
	if ( nCoreResult != 0 || pCore == nullptr ) {
		return rollback( SIR_FMOD_NATIVE_ERROR, nCoreResult );
	}
	m_pCoreBorrowed = pCore;
	uint32_t nRuntimeVersion = 0;
	const int32_t nVersionResult = m_Ops.getVersion( m_Ops.pUserData, m_pCoreBorrowed, &nRuntimeVersion );
	if ( nVersionResult != 0 ) {
		return rollback( SIR_FMOD_NATIVE_ERROR, nVersionResult );
	}
	if ( nRuntimeVersion != m_Ops.nSDKVersion ) {
		return rollback( SIR_FMOD_VERSION_MISMATCH, 0 );
	}

	if ( config.bNoSound != 0 ) {
		const int32_t nOutputResult = m_Ops.setNoSound( m_Ops.pUserData, m_pCoreBorrowed );
		if ( nOutputResult != 0 ) {
			return rollback( SIR_FMOD_NATIVE_ERROR, nOutputResult );
		}
	} else if ( config.iOutputDriver >= 0 ) {
		int32_t nDriverCount = 0;
		const int32_t nCountResult = m_Ops.getDriverCount( m_Ops.pUserData, m_pCoreBorrowed, &nDriverCount );
		if ( nCountResult != 0 ) {
			return rollback( SIR_FMOD_NATIVE_ERROR, nCountResult );
		}
		if ( config.iOutputDriver >= nDriverCount ) {
			return rollback( SIR_FMOD_INVALID_DRIVER, 0 );
		}
		const int32_t nDriverResult = m_Ops.setDriver( m_Ops.pUserData, m_pCoreBorrowed, config.iOutputDriver );
		if ( nDriverResult != 0 ) {
			return rollback( SIR_FMOD_NATIVE_ERROR, nDriverResult );
		}
	}

	const int32_t nInitializeResult = m_Ops.initializeStudio( m_Ops.pUserData, m_pStudio, &config );
	if ( nInitializeResult != 0 ) {
		return rollback( SIR_FMOD_NATIVE_ERROR, nInitializeResult );
	}
	m_eState = SIR_FMOD_CONTEXT_READY;
	m_nRuntimeVersion = nRuntimeVersion;
	m_nEpoch = s_nFMODEpoch.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
	return SIR_FMOD_OK;
}

sirFMODContextResult_t SIRFMODContext::shutdown( void ) noexcept
{
	// Borrowed Studio object pointers become invalid when a bank or Studio
	// releases their descriptions. Preserve the owner for explicit cleanup.
	if ( m_nBorrowedBankObjects != 0 ) {
		return SIR_FMOD_BAD_STATE;
	}
	if ( m_pStudio == nullptr ) {
		m_Ops = {};
		m_eState = SIR_FMOD_CONTEXT_EMPTY;
		m_pCoreBorrowed = nullptr;
		m_nRuntimeVersion = 0;
		m_nEpoch = 0;
		return SIR_FMOD_OK;
	}
	const int32_t nResult = m_Ops.releaseStudio( m_Ops.pUserData, m_pStudio );
	if ( nResult != 0 ) {
		m_eState = SIR_FMOD_CONTEXT_RELEASE_FAILED;
		m_nLastNativeResult = nResult;
		return SIR_FMOD_RELEASE_ERROR;
	}
	m_pStudio = nullptr;
	m_pCoreBorrowed = nullptr;
	m_Ops = {};
	m_eState = SIR_FMOD_CONTEXT_EMPTY;
	m_nLastNativeResult = 0;
	m_nRuntimeVersion = 0;
	m_nEpoch = 0;
	m_nBorrowedBankObjects = 0;
	m_nUnscopedBankObjects = 0;
	return SIR_FMOD_OK;
}

sirFMODContextResult_t SIRFMODContext::update( void ) noexcept
{
	if ( m_eState != SIR_FMOD_CONTEXT_READY ) {
		return SIR_FMOD_BAD_STATE;
	}
	const int32_t nResult = m_Ops.updateStudio( m_Ops.pUserData, m_pStudio );
	m_nLastNativeResult = nResult;
	return nResult == 0 ? SIR_FMOD_OK : SIR_FMOD_NATIVE_ERROR;
}

sirFMODContextResult_t SIRFMODContext::getDriverCount( int32_t &nCount ) noexcept
{
	nCount = 0;
	if ( m_eState != SIR_FMOD_CONTEXT_READY ) {
		return SIR_FMOD_BAD_STATE;
	}
	const int32_t nResult = m_Ops.getDriverCount( m_Ops.pUserData, m_pCoreBorrowed, &nCount );
	m_nLastNativeResult = nResult;
	return nResult == 0 ? SIR_FMOD_OK : SIR_FMOD_NATIVE_ERROR;
}

sirFMODContextResult_t SIRFMODContext::changeDriver( int32_t iDriver ) noexcept
{
	if ( m_eState != SIR_FMOD_CONTEXT_READY ) {
		return SIR_FMOD_BAD_STATE;
	}
	int32_t nCount = 0;
	const sirFMODContextResult_t eCount = getDriverCount( nCount );
	if ( eCount != SIR_FMOD_OK ) {
		return eCount;
	}
	if ( iDriver < 0 || iDriver >= nCount ) {
		return SIR_FMOD_INVALID_DRIVER;
	}
	const int32_t nResult = m_Ops.setDriver( m_Ops.pUserData, m_pCoreBorrowed, iDriver );
	m_nLastNativeResult = nResult;
	return nResult == 0 ? SIR_FMOD_OK : SIR_FMOD_NATIVE_ERROR;
}

bool SIRFMODContext::isReady( void ) const noexcept
{
	return m_eState == SIR_FMOD_CONTEXT_READY;
}

sirFMODContextState_t SIRFMODContext::getState( void ) const noexcept
{
	return m_eState;
}

int32_t SIRFMODContext::getLastNativeResult( void ) const noexcept
{
	return m_nLastNativeResult;
}

uint32_t SIRFMODContext::getRuntimeVersion( void ) const noexcept
{
	return m_nRuntimeVersion;
}

bool SIRFMODContext::pinBorrowedBankObject( bool bBankScoped ) noexcept
{
	if ( !isReady() || m_nBorrowedBankObjects == UINT32_MAX ||
	     ( !bBankScoped && m_nUnscopedBankObjects == UINT32_MAX ) ) {
		return false;
	}
	++m_nBorrowedBankObjects;
	if ( !bBankScoped ) {
		++m_nUnscopedBankObjects;
	}
	return true;
}

void SIRFMODContext::unpinBorrowedBankObject( bool bBankScoped ) noexcept
{
	if ( m_nBorrowedBankObjects == 0 || ( !bBankScoped && m_nUnscopedBankObjects == 0 ) ) {
		abort();
	}
	--m_nBorrowedBankObjects;
	if ( !bBankScoped ) {
		--m_nUnscopedBankObjects;
	}
}

uint32_t SIRFMODContext::getBorrowedBankObjectCount( void ) const noexcept
{
	return m_nBorrowedBankObjects;
}

uint32_t SIRFMODContext::getUnscopedBankObjectCount( void ) const noexcept
{
	return m_nUnscopedBankObjects;
}

uint64_t SIRFMODContext::getEpoch( void ) const noexcept
{
	return m_nEpoch;
}
