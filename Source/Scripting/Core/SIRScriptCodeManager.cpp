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

#include <Scripting/Core/SIRScriptCodeManager.h>

SIRScriptCodeManager::SIRScriptCodeManager( const SIRScriptBindingRegistry *pRegistry ) noexcept
    : m_pRegistry( pRegistry ), m_OwnerThread( std::this_thread::get_id() ), m_Provider{}, m_nTargetId( 0 ), m_szArtifacts{}, m_szLeases{},
      m_bInProvider( false )
{
}

SIRScriptCodeManager::~SIRScriptCodeManager( void ) noexcept
{
	// Runtime must release every source lease before manager destruction.
	( void )shutdown();
}

bool SIRScriptCodeManager::isOwnerThread( void ) const noexcept
{
	return std::this_thread::get_id() == m_OwnerThread;
}

uint32_t SIRScriptCodeManager::nextGeneration( uint32_t nGeneration ) noexcept
{
	++nGeneration;
	return nGeneration == 0 ? 1 : nGeneration;
}

bool SIRScriptCodeManager::validView( const sirScriptProgramView_t &view, const sirScriptCodeArtifact_t &artifact ) noexcept
{
	return view.nProgramId == artifact.nProgramId && view.nRevision == artifact.nRevision && view.nFormatId == artifact.nFormatId &&
	       view.pData != nullptr && view.nBytes != 0 && view.nAlignment != 0 && view.nAlignment <= 4096 &&
	       ( view.nAlignment & ( view.nAlignment - 1 ) ) == 0 && ( ( uintptr_t )view.pData & ( view.nAlignment - 1 ) ) == 0;
}

sirScriptResult_t SIRScriptCodeManager::configure( uint64_t nTargetId, const sirScriptProgramSourceAPI_t &provider ) noexcept
{
	if ( !isOwnerThread() || m_bInProvider ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( nTargetId == 0 || provider.nABIRevision != SIR_SCRIPT_PROGRAM_SOURCE_ABI_REVISION || provider.pAcquire == nullptr ||
	     provider.pRelease == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( m_Provider.pAcquire != nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	m_nTargetId = nTargetId;
	m_Provider = provider;
	return SIR_SCRIPT_OK;
}

int32_t SIRScriptCodeManager::resolve( sirScriptCodeHandle_t handle ) const noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > MAX_ARTIFACTS ) {
		return -1;
	}
	const uint32_t iSlot = handle.nSlot - 1;
	const sirScriptCodeSlot_t &slot = m_szArtifacts[iSlot];
	return slot.bOccupied && slot.nGeneration == handle.nGeneration ? ( int32_t )iSlot : -1;
}

sirScriptResult_t SIRScriptCodeManager::stage( const sirScriptCodeArtifact_t &artifact, sirScriptCodeHandle_t *pHandle ) noexcept
{
	if ( !isOwnerThread() || m_bInProvider ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pHandle == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( m_pRegistry == nullptr || !m_pRegistry->isPublished() || m_Provider.pAcquire == nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( artifact.nBackendId == 0 || artifact.nProgramId == 0 || artifact.nRevision == 0 || artifact.nFormatId == 0 ||
	     artifact.nSchemaFingerprint == 0 || artifact.nTargetId == 0 || artifact.nBuildFingerprint == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( artifact.nSchemaFingerprint != m_pRegistry->schemaFingerprint() || artifact.nTargetId != m_nTargetId ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	int32_t iFree = -1;
	for ( uint32_t iSlot = 0; iSlot < MAX_ARTIFACTS; ++iSlot ) {
		const sirScriptCodeSlot_t &slot = m_szArtifacts[iSlot];
		if ( !slot.bOccupied && iFree < 0 ) {
			iFree = ( int32_t )iSlot;
		}
		if ( slot.bOccupied && slot.artifact.nBackendId == artifact.nBackendId && slot.artifact.nProgramId == artifact.nProgramId &&
		     slot.artifact.nRevision == artifact.nRevision ) {
			return SIR_SCRIPT_DUPLICATE;
		}
	}
	if ( iFree < 0 ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}

	sirScriptProgramView_t view = {};
	uint64_t nProviderLease = 0;
	m_bInProvider = true;
	const sirScriptResult_t eAcquire =
	    m_Provider.pAcquire( m_Provider.pUser, artifact.nBackendId, artifact.nProgramId, artifact.nRevision, &view, &nProviderLease );
	m_bInProvider = false;
	if ( eAcquire != SIR_SCRIPT_OK ) {
		// A failed acquisition is forbidden from handing ownership to the caller.
		if ( nProviderLease != 0 ) {
			m_bInProvider = true;
			m_Provider.pRelease( m_Provider.pUser, nProviderLease );
			m_bInProvider = false;
		}
		return eAcquire;
	}
	if ( nProviderLease == 0 ) {
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( !validView( view, artifact ) ) {
		m_bInProvider = true;
		m_Provider.pRelease( m_Provider.pUser, nProviderLease );
		m_bInProvider = false;
		return SIR_SCRIPT_TYPE_MISMATCH;
	}

	sirScriptCodeSlot_t &slot = m_szArtifacts[iFree];
	slot.nGeneration = nextGeneration( slot.nGeneration );
	slot.artifact = artifact;
	slot.view = view;
	slot.nProviderLease = nProviderLease;
	slot.nPins = 0;
	slot.bOccupied = true;
	slot.bActive = false;
	slot.bRetired = false;
	*pHandle = { ( uint32_t )iFree + 1, slot.nGeneration };
	return SIR_SCRIPT_OK;
}

void SIRScriptCodeManager::destroySlot( uint32_t iSlot ) noexcept
{
	sirScriptCodeSlot_t &slot = m_szArtifacts[iSlot];
	if ( !slot.bOccupied || slot.nPins != 0 ) {
		return;
	}
	const uint64_t nToken = slot.nProviderLease;
	slot.bOccupied = false;
	slot.bActive = false;
	slot.bRetired = false;
	slot.view = {};
	slot.nProviderLease = 0;
	slot.nGeneration = nextGeneration( slot.nGeneration );
	m_bInProvider = true;
	m_Provider.pRelease( m_Provider.pUser, nToken );
	m_bInProvider = false;
}

sirScriptResult_t SIRScriptCodeManager::activate( sirScriptCodeHandle_t handle ) noexcept
{
	if ( !isOwnerThread() || m_bInProvider ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptCodeSlot_t &candidate = m_szArtifacts[iSlot];
	if ( candidate.bActive || candidate.bRetired ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	for ( uint32_t iOld = 0; iOld < MAX_ARTIFACTS; ++iOld ) {
		sirScriptCodeSlot_t &old = m_szArtifacts[iOld];
		if ( old.bOccupied && old.bActive && old.artifact.nBackendId == candidate.artifact.nBackendId &&
		     old.artifact.nProgramId == candidate.artifact.nProgramId ) {
			old.bActive = false;
			old.bRetired = true;
			if ( old.nPins == 0 ) {
				destroySlot( iOld );
			}
		}
	}
	candidate.bActive = true;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptCodeManager::discard( sirScriptCodeHandle_t handle ) noexcept
{
	if ( !isOwnerThread() || m_bInProvider ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	if ( m_szArtifacts[iSlot].bActive || m_szArtifacts[iSlot].bRetired || m_szArtifacts[iSlot].nPins != 0 ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	destroySlot( ( uint32_t )iSlot );
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptCodeManager::retire( sirScriptCodeHandle_t handle ) noexcept
{
	if ( !isOwnerThread() || m_bInProvider ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptCodeSlot_t &slot = m_szArtifacts[iSlot];
	if ( slot.bRetired ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	slot.bActive = false;
	slot.bRetired = true;
	if ( slot.nPins == 0 ) {
		destroySlot( ( uint32_t )iSlot );
	}
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptCodeManager::activeRevision( uint64_t nBackendId, uint64_t nProgramId, uint64_t *pRevision ) const noexcept
{
	if ( !isOwnerThread() || m_bInProvider ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( nBackendId == 0 || nProgramId == 0 || pRevision == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	for ( uint32_t iSlot = 0; iSlot < MAX_ARTIFACTS; ++iSlot ) {
		const sirScriptCodeSlot_t &slot = m_szArtifacts[iSlot];
		if ( slot.bOccupied && slot.bActive && slot.artifact.nBackendId == nBackendId && slot.artifact.nProgramId == nProgramId ) {
			*pRevision = slot.artifact.nRevision;
			return SIR_SCRIPT_OK;
		}
	}
	return SIR_SCRIPT_NOT_FOUND;
}

sirScriptResult_t SIRScriptCodeManager::acquire( uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision,
                                                 sirScriptProgramView_t *pView, uint64_t *pToken ) noexcept
{
	if ( !isOwnerThread() || m_bInProvider ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pView == nullptr || pToken == nullptr || nBackendId == 0 || nProgramId == 0 || nRevision == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	int32_t iCode = -1;
	for ( uint32_t iSlot = 0; iSlot < MAX_ARTIFACTS; ++iSlot ) {
		const sirScriptCodeSlot_t &slot = m_szArtifacts[iSlot];
		if ( slot.bOccupied && slot.bActive && slot.artifact.nBackendId == nBackendId && slot.artifact.nProgramId == nProgramId &&
		     slot.artifact.nRevision == nRevision ) {
			iCode = ( int32_t )iSlot;
			break;
		}
	}
	if ( iCode < 0 ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	for ( uint32_t iLease = 0; iLease < MAX_LEASES; ++iLease ) {
		sirScriptCodeLeaseSlot_t &lease = m_szLeases[iLease];
		if ( lease.bOccupied ) {
			continue;
		}
		lease.nGeneration = nextGeneration( lease.nGeneration );
		lease.iArtifact = ( uint32_t )iCode;
		lease.bOccupied = true;
		++m_szArtifacts[iCode].nPins;
		*pView = m_szArtifacts[iCode].view;
		*pToken = ( ( uint64_t )lease.nGeneration << 32 ) | ( iLease + 1 );
		return SIR_SCRIPT_OK;
	}
	return SIR_SCRIPT_CAPACITY_EXCEEDED;
}

void SIRScriptCodeManager::release( uint64_t nToken ) noexcept
{
	if ( !isOwnerThread() || m_bInProvider ) {
		return;
	}
	const uint32_t nSlot = ( uint32_t )nToken;
	const uint32_t nGeneration = ( uint32_t )( nToken >> 32 );
	if ( nSlot == 0 || nSlot > MAX_LEASES || nGeneration == 0 ) {
		return;
	}
	sirScriptCodeLeaseSlot_t &lease = m_szLeases[nSlot - 1];
	if ( !lease.bOccupied || lease.nGeneration != nGeneration ) {
		return;
	}
	const uint32_t iCode = lease.iArtifact;
	lease.bOccupied = false;
	sirScriptCodeSlot_t &slot = m_szArtifacts[iCode];
	if ( slot.nPins == 0 ) {
		return;
	}
	--slot.nPins;
	if ( slot.bRetired && slot.nPins == 0 ) {
		destroySlot( iCode );
	}
}

sirScriptResult_t SIRScriptCodeManager::acquireThunk( void *pUser, uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision,
                                                      sirScriptProgramView_t *pView, uint64_t *pToken ) noexcept
{
	if ( pUser == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	return ( ( SIRScriptCodeManager * )pUser )->acquire( nBackendId, nProgramId, nRevision, pView, pToken );
}

void SIRScriptCodeManager::releaseThunk( void *pUser, uint64_t nToken ) noexcept
{
	if ( pUser != nullptr ) {
		( ( SIRScriptCodeManager * )pUser )->release( nToken );
	}
}

sirScriptProgramSourceAPI_t SIRScriptCodeManager::makeProgramSourceAPI( void ) noexcept
{
	sirScriptProgramSourceAPI_t api = {};
	api.nABIRevision = SIR_SCRIPT_PROGRAM_SOURCE_ABI_REVISION;
	api.pUser = this;
	api.pAcquire = acquireThunk;
	api.pRelease = releaseThunk;
	return api;
}

sirScriptResult_t SIRScriptCodeManager::shutdown( void ) noexcept
{
	if ( !isOwnerThread() || m_bInProvider ) {
		return SIR_SCRIPT_BUSY;
	}
	for ( uint32_t iLease = 0; iLease < MAX_LEASES; ++iLease ) {
		if ( m_szLeases[iLease].bOccupied ) {
			return SIR_SCRIPT_BUSY;
		}
	}
	for ( uint32_t iSlot = 0; iSlot < MAX_ARTIFACTS; ++iSlot ) {
		if ( m_szArtifacts[iSlot].bOccupied ) {
			destroySlot( iSlot );
		}
	}
	m_Provider = {};
	m_nTargetId = 0;
	return SIR_SCRIPT_OK;
}

sirScriptCodeStats_t SIRScriptCodeManager::stats( void ) const noexcept
{
	sirScriptCodeStats_t stats = {};
	if ( !isOwnerThread() || m_bInProvider ) {
		return stats;
	}
	for ( uint32_t iSlot = 0; iSlot < MAX_ARTIFACTS; ++iSlot ) {
		const sirScriptCodeSlot_t &slot = m_szArtifacts[iSlot];
		if ( !slot.bOccupied ) {
			continue;
		}
		++stats.nArtifacts;
		stats.nPinnedArtifactBytes += slot.view.nBytes;
		if ( slot.bRetired ) {
			++stats.nRetiredArtifacts;
		}
		stats.nActiveRuntimeLeases += slot.nPins;
	}
	return stats;
}
