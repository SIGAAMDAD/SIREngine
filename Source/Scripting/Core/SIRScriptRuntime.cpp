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

#include <Scripting/Core/SIRScriptRuntime.h>

#include <string.h>

SIRScriptRuntime::SIRScriptRuntime( const SIRScriptBindingRegistry *pRegistry ) noexcept
    : m_pRegistry( pRegistry ), m_ProgramSource{}, m_OwnerThread( std::this_thread::get_id() ), m_bDispatching( false ),
      m_bCallingBackend( false ), m_bPumping( false ), m_bShuttingDown( false )
{
	memset( m_szBackends, 0, sizeof( m_szBackends ) );
	memset( m_szPrograms, 0, sizeof( m_szPrograms ) );
	memset( m_szInstances, 0, sizeof( m_szInstances ) );
	memset( m_szSubscriptions, 0, sizeof( m_szSubscriptions ) );
}

bool SIRScriptRuntime::isOwnerThread( void ) const noexcept
{
	return std::this_thread::get_id() == m_OwnerThread;
}

bool SIRScriptRuntime::validFrame( const void *pData, uint32_t nBytes, uint32_t nAlignment ) noexcept
{
	if ( nAlignment == 0 || ( nAlignment & ( nAlignment - 1 ) ) != 0 ) {
		return false;
	}
	if ( nBytes == 0 ) {
		return pData == nullptr;
	}
	return pData != nullptr && ( ( uintptr_t )pData % nAlignment ) == 0;
}

uint32_t SIRScriptRuntime::nextGeneration( uint32_t nGeneration ) noexcept
{
	++nGeneration;
	return nGeneration == 0 ? 1 : nGeneration;
}

bool SIRScriptRuntime::validAPI( const sirScriptBackendAPI_t &api ) noexcept
{
	return api.nABIRevision == SIR_SCRIPT_BACKEND_ABI_REVISION && api.pStart != nullptr && api.pStop != nullptr &&
	       api.pLoadProgram != nullptr && api.pUnloadProgram != nullptr && api.pCreateInstance != nullptr &&
	       api.pInitializeInstance != nullptr && api.pShutdownInstance != nullptr && api.pDestroyInstance != nullptr &&
	       api.pInvoke != nullptr;
}

bool SIRScriptRuntime::validProgramView( sirScriptProgramView_t view, uint64_t nProgramId, uint64_t nRevision ) noexcept
{
	if ( view.nProgramId != nProgramId || view.nRevision != nRevision || view.nFormatId == 0 ) {
		return false;
	}
	if ( view.nAlignment == 0 || ( view.nAlignment & ( view.nAlignment - 1 ) ) != 0 ) {
		return false;
	}
	if ( view.nBytes == 0 ) {
		return view.pData == nullptr;
	}
	return view.pData != nullptr && ( ( uintptr_t )view.pData % view.nAlignment ) == 0;
}

sirScriptResult_t SIRScriptRuntime::setProgramSource( const sirScriptProgramSourceAPI_t &api ) noexcept
{
	if ( !isOwnerThread() || m_bCallingBackend || m_bDispatching || m_bPumping || m_bShuttingDown ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( api.nABIRevision != SIR_SCRIPT_PROGRAM_SOURCE_ABI_REVISION || api.pAcquire == nullptr || api.pRelease == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	for ( uint32_t iProgram = 0; iProgram < MAX_PROGRAMS; ++iProgram ) {
		if ( m_szPrograms[iProgram].bActive ) {
			return SIR_SCRIPT_BUSY;
		}
	}
	m_ProgramSource = api;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptRuntime::clearProgramSource( void ) noexcept
{
	if ( !isOwnerThread() || m_bCallingBackend || m_bDispatching || m_bPumping || m_bShuttingDown ) {
		return SIR_SCRIPT_BUSY;
	}
	for ( uint32_t iProgram = 0; iProgram < MAX_PROGRAMS; ++iProgram ) {
		if ( m_szPrograms[iProgram].bActive ) {
			return SIR_SCRIPT_BUSY;
		}
	}
	m_ProgramSource = {};
	return SIR_SCRIPT_OK;
}

int32_t SIRScriptRuntime::findBackend( uint64_t nBackendId ) const noexcept
{
	for ( uint32_t iBackend = 0; iBackend < MAX_BACKENDS; ++iBackend ) {
		if ( m_szBackends[iBackend].bRegistered && m_szBackends[iBackend].nBackendId == nBackendId ) {
			return ( int32_t )iBackend;
		}
	}
	return -1;
}

int32_t SIRScriptRuntime::resolveProgram( sirScriptProgramHandle_t handle ) const noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > MAX_PROGRAMS ) {
		return -1;
	}
	const uint32_t iProgram = handle.nSlot - 1;
	const sirScriptProgramSlot_t &slot = m_szPrograms[iProgram];
	return slot.bActive && slot.nGeneration == handle.nGeneration ? ( int32_t )iProgram : -1;
}

int32_t SIRScriptRuntime::resolveInstance( sirScriptInstanceHandle_t handle ) const noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > MAX_INSTANCES ) {
		return -1;
	}
	const uint32_t iInstance = handle.nSlot - 1;
	const sirScriptInstanceSlot_t &slot = m_szInstances[iInstance];
	return slot.bActive && slot.nGeneration == handle.nGeneration ? ( int32_t )iInstance : -1;
}

bool SIRScriptRuntime::hasLivePrograms( uint32_t iBackend ) const noexcept
{
	for ( uint32_t iProgram = 0; iProgram < MAX_PROGRAMS; ++iProgram ) {
		if ( m_szPrograms[iProgram].bActive && m_szPrograms[iProgram].iBackend == iBackend ) {
			return true;
		}
	}
	return false;
}

sirScriptResult_t SIRScriptRuntime::registerBackend( uint64_t nBackendId, const sirScriptBackendAPI_t &api ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bDispatching || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( nBackendId == 0 || !validAPI( api ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( findBackend( nBackendId ) >= 0 ) {
		return SIR_SCRIPT_DUPLICATE;
	}
	for ( uint32_t iBackend = 0; iBackend < MAX_BACKENDS; ++iBackend ) {
		if ( !m_szBackends[iBackend].bRegistered ) {
			m_szBackends[iBackend] = { nBackendId, api, true, false };
			return SIR_SCRIPT_OK;
		}
	}
	return SIR_SCRIPT_CAPACITY_EXCEEDED;
}

sirScriptResult_t SIRScriptRuntime::startBackend( uint64_t nBackendId ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bDispatching || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_pRegistry == nullptr || !m_pRegistry->isPublished() ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	const int32_t iBackend = findBackend( nBackendId );
	if ( iBackend < 0 ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	sirScriptBackendSlot_t &backend = m_szBackends[iBackend];
	if ( backend.bStarted ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	m_bCallingBackend = true;
	const sirScriptResult_t eResult = backend.api.pStart( backend.api.pUser );
	m_bCallingBackend = false;
	if ( eResult == SIR_SCRIPT_OK ) {
		backend.bStarted = true;
	}
	return eResult;
}

sirScriptResult_t SIRScriptRuntime::stopBackend( uint64_t nBackendId ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bDispatching || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iBackend = findBackend( nBackendId );
	if ( iBackend < 0 ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	sirScriptBackendSlot_t &backend = m_szBackends[iBackend];
	if ( !backend.bStarted ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( hasLivePrograms( ( uint32_t )iBackend ) ) {
		return SIR_SCRIPT_BUSY;
	}
	m_bCallingBackend = true;
	backend.api.pStop( backend.api.pUser );
	m_bCallingBackend = false;
	backend.bStarted = false;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptRuntime::unregisterBackend( uint64_t nBackendId ) noexcept
{
	if ( !isOwnerThread() || m_bDispatching || m_bCallingBackend || m_bPumping ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iBackend = findBackend( nBackendId );
	if ( iBackend < 0 ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	if ( m_szBackends[iBackend].bStarted || hasLivePrograms( ( uint32_t )iBackend ) ) {
		return SIR_SCRIPT_BUSY;
	}
	m_szBackends[iBackend] = {};
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptRuntime::shutdownAll( void ) noexcept
{
	if ( !isOwnerThread() || m_bDispatching || m_bCallingBackend || m_bPumping || m_bShuttingDown ) {
		return SIR_SCRIPT_BUSY;
	}
	m_bShuttingDown = true;
	m_Ingress.close();
	m_Ingress.clear();
	m_Scheduler.clear();
	for ( uint32_t iInstance = 0; iInstance < MAX_INSTANCES; ++iInstance ) {
		if ( !m_szInstances[iInstance].bActive ) {
			continue;
		}
		const sirScriptInstanceHandle_t handle = { iInstance + 1, m_szInstances[iInstance].nGeneration };
		const sirScriptResult_t eResult = destroyInstance( handle );
		if ( eResult != SIR_SCRIPT_OK ) {
			m_bShuttingDown = false;
			return eResult;
		}
	}
	for ( uint32_t iProgram = 0; iProgram < MAX_PROGRAMS; ++iProgram ) {
		if ( !m_szPrograms[iProgram].bActive ) {
			continue;
		}
		const sirScriptProgramHandle_t handle = { iProgram + 1, m_szPrograms[iProgram].nGeneration };
		const sirScriptResult_t eResult = unloadProgram( handle );
		if ( eResult != SIR_SCRIPT_OK ) {
			m_bShuttingDown = false;
			return eResult;
		}
	}
	for ( uint32_t iBackend = 0; iBackend < MAX_BACKENDS; ++iBackend ) {
		if ( !m_szBackends[iBackend].bStarted ) {
			continue;
		}
		const sirScriptResult_t eResult = stopBackend( m_szBackends[iBackend].nBackendId );
		if ( eResult != SIR_SCRIPT_OK ) {
			m_bShuttingDown = false;
			return eResult;
		}
	}
	m_bShuttingDown = false;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptRuntime::loadProgram( uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision,
                                                 sirScriptProgramHandle_t *pHandle ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bDispatching || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pHandle == nullptr || nProgramId == 0 || nRevision == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	const int32_t iBackend = findBackend( nBackendId );
	if ( iBackend < 0 || !m_szBackends[iBackend].bStarted ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	for ( uint32_t iProgram = 0; iProgram < MAX_PROGRAMS; ++iProgram ) {
		const sirScriptProgramSlot_t &slot = m_szPrograms[iProgram];
		if ( slot.bActive && slot.nProgramId == nProgramId && slot.nRevision == nRevision && slot.iBackend == iBackend ) {
			return SIR_SCRIPT_DUPLICATE;
		}
	}
	if ( m_ProgramSource.pAcquire == nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	for ( uint32_t iProgram = 0; iProgram < MAX_PROGRAMS; ++iProgram ) {
		sirScriptProgramSlot_t &slot = m_szPrograms[iProgram];
		if ( slot.bActive ) {
			continue;
		}

		sirScriptProgramView_t view = {};
		uint64_t nLeaseToken = 0;
		m_bCallingBackend = true;
		const sirScriptResult_t eAcquire =
		    m_ProgramSource.pAcquire( m_ProgramSource.pUser, nBackendId, nProgramId, nRevision, &view, &nLeaseToken );
		m_bCallingBackend = false;
		if ( eAcquire != SIR_SCRIPT_OK ) {
			return eAcquire;
		}
		if ( nLeaseToken == 0 ) {
			return SIR_SCRIPT_BACKEND_FAILURE;
		}
		if ( !validProgramView( view, nProgramId, nRevision ) ) {
			m_bCallingBackend = true;
			m_ProgramSource.pRelease( m_ProgramSource.pUser, nLeaseToken );
			m_bCallingBackend = false;
			return SIR_SCRIPT_TYPE_MISMATCH;
		}

		uint64_t nBackendToken = 0;
		sirScriptBackendSlot_t &backend = m_szBackends[iBackend];
		m_bCallingBackend = true;
		const sirScriptResult_t eResult = backend.api.pLoadProgram( backend.api.pUser, view, &nBackendToken );
		m_bCallingBackend = false;
		if ( eResult != SIR_SCRIPT_OK || nBackendToken == 0 ) {
			m_bCallingBackend = true;
			m_ProgramSource.pRelease( m_ProgramSource.pUser, nLeaseToken );
			m_bCallingBackend = false;
			return eResult == SIR_SCRIPT_OK ? SIR_SCRIPT_BACKEND_FAILURE : eResult;
		}

		slot.nGeneration = nextGeneration( slot.nGeneration );
		slot.nProgramId = nProgramId;
		slot.nRevision = nRevision;
		slot.nBackendToken = nBackendToken;
		slot.nLeaseToken = nLeaseToken;
		slot.nReferences = 0;
		slot.iBackend = ( uint8_t )iBackend;
		slot.bActive = true;
		*pHandle = { iProgram + 1, slot.nGeneration };
		return SIR_SCRIPT_OK;
	}
	return SIR_SCRIPT_CAPACITY_EXCEEDED;
}

sirScriptResult_t SIRScriptRuntime::unloadProgram( sirScriptProgramHandle_t handle ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bDispatching || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iProgram = resolveProgram( handle );
	if ( iProgram < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptProgramSlot_t &slot = m_szPrograms[iProgram];
	if ( slot.nReferences != 0 ) {
		return SIR_SCRIPT_BUSY;
	}
	sirScriptBackendSlot_t &backend = m_szBackends[slot.iBackend];
	m_bCallingBackend = true;
	backend.api.pUnloadProgram( backend.api.pUser, slot.nBackendToken );
	m_ProgramSource.pRelease( m_ProgramSource.pUser, slot.nLeaseToken );
	m_bCallingBackend = false;
	slot.nLeaseToken = 0;
	slot.bActive = false;
	slot.nGeneration = nextGeneration( slot.nGeneration );
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptRuntime::createInstance( sirScriptProgramHandle_t program, uint64_t nOwnerId,
                                                    sirScriptInstanceHandle_t *pHandle ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bDispatching || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pHandle == nullptr || nOwnerId == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pHandle = {};
	const int32_t iProgram = resolveProgram( program );
	if ( iProgram < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	for ( uint32_t iInstance = 0; iInstance < MAX_INSTANCES; ++iInstance ) {
		sirScriptInstanceSlot_t &slot = m_szInstances[iInstance];
		if ( slot.bActive ) {
			continue;
		}
		sirScriptProgramSlot_t &loaded = m_szPrograms[iProgram];
		sirScriptBackendSlot_t &backend = m_szBackends[loaded.iBackend];
		uint64_t nBackendToken = 0;
		m_bCallingBackend = true;
		sirScriptResult_t eResult = backend.api.pCreateInstance( backend.api.pUser, loaded.nBackendToken, nOwnerId, &nBackendToken );
		m_bCallingBackend = false;
		if ( eResult != SIR_SCRIPT_OK ) {
			return eResult;
		}
		if ( nBackendToken == 0 ) {
			return SIR_SCRIPT_BACKEND_FAILURE;
		}
		m_bCallingBackend = true;
		eResult = backend.api.pInitializeInstance( backend.api.pUser, nBackendToken );
		m_bCallingBackend = false;
		if ( eResult != SIR_SCRIPT_OK ) {
			m_bCallingBackend = true;
			backend.api.pDestroyInstance( backend.api.pUser, nBackendToken );
			m_bCallingBackend = false;
			slot.nGeneration = nextGeneration( slot.nGeneration );
			return eResult;
		}
		slot.nGeneration = nextGeneration( slot.nGeneration );
		slot.nBackendToken = nBackendToken;
		slot.nOwnerId = nOwnerId;
		slot.iProgram = ( uint32_t )iProgram;
		slot.bActive = true;
		slot.bInvoking = false;
		++loaded.nReferences;
		*pHandle = { iInstance + 1, slot.nGeneration };
		return SIR_SCRIPT_OK;
	}
	return SIR_SCRIPT_CAPACITY_EXCEEDED;
}

void SIRScriptRuntime::invalidateSubscriptions( sirScriptInstanceHandle_t handle ) noexcept
{
	for ( uint32_t iSubscription = 0; iSubscription < MAX_SUBSCRIPTIONS; ++iSubscription ) {
		sirScriptSubscriptionSlot_t &subscription = m_szSubscriptions[iSubscription];
		if ( subscription.bActive && subscription.instance.nSlot == handle.nSlot &&
		     subscription.instance.nGeneration == handle.nGeneration ) {
			subscription.bActive = false;
			subscription.nGeneration = nextGeneration( subscription.nGeneration );
		}
	}
}

sirScriptResult_t SIRScriptRuntime::destroyInstance( sirScriptInstanceHandle_t handle ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bDispatching || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iInstance = resolveInstance( handle );
	if ( iInstance < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptInstanceSlot_t &slot = m_szInstances[iInstance];
	if ( slot.bInvoking ) {
		return SIR_SCRIPT_BUSY;
	}
	invalidateSubscriptions( handle );
	sirScriptProgramSlot_t &program = m_szPrograms[slot.iProgram];
	sirScriptBackendSlot_t &backend = m_szBackends[program.iBackend];
	m_bCallingBackend = true;
	backend.api.pShutdownInstance( backend.api.pUser, slot.nBackendToken );
	backend.api.pDestroyInstance( backend.api.pUser, slot.nBackendToken );
	m_bCallingBackend = false;
	--program.nReferences;
	slot.bActive = false;
	slot.nGeneration = nextGeneration( slot.nGeneration );
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptRuntime::invoke( sirScriptInstanceHandle_t handle, uint64_t nMethodId, sirScriptReadFrame_t arguments,
                                            sirScriptWriteFrame_t result ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iInstance = resolveInstance( handle );
	if ( iInstance < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	if ( nMethodId == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	sirScriptInstanceSlot_t &slot = m_szInstances[iInstance];
	if ( slot.bInvoking || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( !validFrame( arguments.pData, arguments.nBytes, arguments.nAlignment ) ||
	     !validFrame( result.pData, result.nBytes, result.nAlignment ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	const sirScriptProgramSlot_t &program = m_szPrograms[slot.iProgram];
	const sirScriptBackendSlot_t &backend = m_szBackends[program.iBackend];
	slot.bInvoking = true;
	m_bCallingBackend = true;
	const sirScriptResult_t eResult = backend.api.pInvoke( backend.api.pUser, slot.nBackendToken, nMethodId, arguments, result );
	slot.bInvoking = false;
	m_bCallingBackend = false;
	return eResult;
}

sirScriptResult_t SIRScriptRuntime::subscribe( sirScriptInstanceHandle_t instance, uint64_t nEventTypeId, uint64_t nMethodId,
                                               sirScriptSubscriptionHandle_t *pHandle ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bDispatching || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pHandle == nullptr || nMethodId == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( resolveInstance( instance ) < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	if ( m_pRegistry == nullptr || !m_pRegistry->isPublished() ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( !m_pRegistry->hasEvent( nEventTypeId ) ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	// An empty payload is not accepted for nonempty event descriptors here;
	// validateEvent will check actual bytes when the event is published.
	for ( uint32_t iSubscription = 0; iSubscription < MAX_SUBSCRIPTIONS; ++iSubscription ) {
		const sirScriptSubscriptionSlot_t &entry = m_szSubscriptions[iSubscription];
		if ( entry.bActive && entry.instance.nSlot == instance.nSlot && entry.instance.nGeneration == instance.nGeneration &&
		     entry.nEventTypeId == nEventTypeId && entry.nMethodId == nMethodId ) {
			return SIR_SCRIPT_DUPLICATE;
		}
	}
	for ( uint32_t iSubscription = 0; iSubscription < MAX_SUBSCRIPTIONS; ++iSubscription ) {
		sirScriptSubscriptionSlot_t &slot = m_szSubscriptions[iSubscription];
		if ( slot.bActive ) {
			continue;
		}
		slot.nGeneration = nextGeneration( slot.nGeneration );
		slot.instance = instance;
		slot.nEventTypeId = nEventTypeId;
		slot.nMethodId = nMethodId;
		slot.bActive = true;
		*pHandle = { iSubscription + 1, slot.nGeneration };
		return SIR_SCRIPT_OK;
	}
	return SIR_SCRIPT_CAPACITY_EXCEEDED;
}

sirScriptResult_t SIRScriptRuntime::unsubscribe( sirScriptSubscriptionHandle_t handle ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( handle.nSlot == 0 || handle.nSlot > MAX_SUBSCRIPTIONS ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptSubscriptionSlot_t &slot = m_szSubscriptions[handle.nSlot - 1];
	if ( !slot.bActive || slot.nGeneration != handle.nGeneration ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	slot.bActive = false;
	slot.nGeneration = nextGeneration( slot.nGeneration );
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptRuntime::publishEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bDispatching || m_bCallingBackend ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_pRegistry == nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	const sirScriptResult_t eValid = m_pRegistry->validateEvent( nEventTypeId, payload );
	if ( eValid != SIR_SCRIPT_OK ) {
		return eValid;
	}
	m_bDispatching = true;
	sirScriptResult_t eFirstFailure = SIR_SCRIPT_OK;
	const sirScriptWriteFrame_t emptyResult = { nullptr, 0, 1 };
	for ( uint32_t iSubscription = 0; iSubscription < MAX_SUBSCRIPTIONS; ++iSubscription ) {
		const sirScriptSubscriptionSlot_t &slot = m_szSubscriptions[iSubscription];
		if ( !slot.bActive || slot.nEventTypeId != nEventTypeId ) {
			continue;
		}
		const sirScriptResult_t eResult = invoke( slot.instance, slot.nMethodId, payload, emptyResult );
		if ( eFirstFailure == SIR_SCRIPT_OK && eResult != SIR_SCRIPT_OK ) {
			eFirstFailure = eResult;
		}
	}
	m_bDispatching = false;
	return eFirstFailure;
}

sirScriptResult_t SIRScriptRuntime::enqueueEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload, sirScriptPhase_t ePhase,
                                                  sirScriptTaskHandle_t *pHandle ) noexcept
{
	if ( !isOwnerThread() || m_bShuttingDown ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pHandle == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( m_pRegistry == nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	const sirScriptResult_t eValid = m_pRegistry->validateQueuedEvent( nEventTypeId, payload );
	if ( eValid != SIR_SCRIPT_OK ) {
		return eValid;
	}
	if ( payload.nBytes > sizeof( sirScriptScheduledCall_t::szPayload ) || payload.nAlignment > alignof( sirScriptScheduledCall_t ) ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}

	sirScriptScheduledCall_t call = {};
	call.nDispatchId = nEventTypeId;
	call.nBytes = payload.nBytes;
	call.nAlignment = payload.nAlignment;
	call.eKind = SIR_SCRIPT_SCHEDULED_EVENT;
	if ( payload.nBytes != 0 ) {
		memcpy( call.szPayload, payload.pData, payload.nBytes );
	}
	return m_Scheduler.enqueue( ePhase, call, pHandle );
}

sirScriptResult_t SIRScriptRuntime::enqueueInvoke( sirScriptInstanceHandle_t instance, uint64_t nMethodId, uint64_t nArgumentsTypeId,
                                                   sirScriptReadFrame_t arguments, sirScriptPhase_t ePhase,
                                                   sirScriptTaskHandle_t *pHandle ) noexcept
{
	if ( !isOwnerThread() || m_bShuttingDown ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pHandle == nullptr || nMethodId == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( resolveInstance( instance ) < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	if ( m_pRegistry == nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	const sirScriptResult_t eValid = m_pRegistry->validateQueuedFrame( nArgumentsTypeId, arguments );
	if ( eValid != SIR_SCRIPT_OK ) {
		return eValid;
	}
	if ( arguments.nBytes > sizeof( sirScriptScheduledCall_t::szPayload ) || arguments.nAlignment > alignof( sirScriptScheduledCall_t ) ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}

	sirScriptScheduledCall_t call = {};
	call.instance = instance;
	call.nDispatchId = nMethodId;
	call.nBytes = arguments.nBytes;
	call.nAlignment = arguments.nAlignment;
	call.eKind = SIR_SCRIPT_SCHEDULED_INVOKE;
	if ( arguments.nBytes != 0 ) {
		memcpy( call.szPayload, arguments.pData, arguments.nBytes );
	}
	return m_Scheduler.enqueue( ePhase, call, pHandle );
}

sirScriptResult_t SIRScriptRuntime::cancelScheduled( sirScriptTaskHandle_t handle ) noexcept
{
	if ( !isOwnerThread() ) {
		return SIR_SCRIPT_BUSY;
	}
	return m_Scheduler.cancel( handle );
}

uint32_t SIRScriptRuntime::pendingCount( sirScriptPhase_t ePhase ) const noexcept
{
	return isOwnerThread() ? m_Scheduler.pendingCount( ePhase ) : 0;
}

sirScriptResult_t SIRScriptRuntime::pumpPhase( sirScriptPhase_t ePhase, uint32_t nBudget, uint32_t *pProcessed, uint32_t *pStale ) noexcept
{
	if ( !isOwnerThread() || m_bCallingBackend || m_bDispatching || m_bPumping ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pProcessed == nullptr || pStale == nullptr || pProcessed == pStale || ePhase >= SIR_SCRIPT_PHASE_COUNT ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pProcessed = 0;
	*pStale = 0;
	const uint32_t nSnapshot = m_Scheduler.pendingCount( ePhase );
	const uint64_t nLastSequence = m_Scheduler.lastSequence();
	const uint32_t nLimit = nBudget < nSnapshot ? nBudget : nSnapshot;
	m_bPumping = true;
	sirScriptResult_t eFirstFailure = SIR_SCRIPT_OK;
	for ( uint32_t iTask = 0; iTask < nLimit; ++iTask ) {
		sirScriptScheduledCall_t call = {};
		if ( !m_Scheduler.dequeue( ePhase, nLastSequence, &call ) ) {
			break;
		}
		++*pProcessed;
		const sirScriptReadFrame_t payload = { call.nBytes == 0 ? nullptr : call.szPayload, call.nBytes, call.nAlignment };
		const sirScriptWriteFrame_t emptyResult = { nullptr, 0, 1 };
		sirScriptResult_t eResult = SIR_SCRIPT_OK;
		if ( call.eKind == SIR_SCRIPT_SCHEDULED_EVENT ) {
			eResult = publishEvent( call.nDispatchId, payload );
		} else if ( resolveInstance( call.instance ) < 0 ) {
			++*pStale;
		} else {
			eResult = invoke( call.instance, call.nDispatchId, payload, emptyResult );
		}
		if ( eFirstFailure == SIR_SCRIPT_OK && eResult != SIR_SCRIPT_OK ) {
			eFirstFailure = eResult;
		}
	}
	m_bPumping = false;
	return eFirstFailure;
}

// Called by producer threads only after the registry has been published. The
// immutable schema is safe to read concurrently after publication.
sirScriptResult_t SIRScriptRuntime::postEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload, sirScriptPhase_t ePhase ) noexcept
{
	if ( ePhase >= SIR_SCRIPT_PHASE_COUNT ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( m_pRegistry == nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	const sirScriptResult_t eResult = m_pRegistry->validateQueuedEvent( nEventTypeId, payload );
	if ( eResult != SIR_SCRIPT_OK ) {
		return eResult;
	}
	if ( payload.nBytes > sizeof( sirScriptIngressPacket_t::szPayload ) || payload.nAlignment > alignof( sirScriptIngressPacket_t ) ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}

	sirScriptIngressPacket_t packet = {};
	packet.nEventTypeId = nEventTypeId;
	packet.nBytes = payload.nBytes;
	packet.nAlignment = payload.nAlignment;
	if ( payload.nBytes != 0 ) {
		memcpy( packet.szPayload, payload.pData, payload.nBytes );
	}
	return m_Ingress.tryPost( ePhase, packet );
}

sirScriptResult_t SIRScriptRuntime::pumpIngress( sirScriptPhase_t ePhase, uint32_t nBudget, uint32_t *pProcessed ) noexcept
{
	if ( !isOwnerThread() || m_bCallingBackend || m_bDispatching || m_bPumping || m_bShuttingDown ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pProcessed == nullptr || ePhase >= SIR_SCRIPT_PHASE_COUNT ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pProcessed = 0;
	const uint64_t nEndTicket = m_Ingress.snapshotTail( ePhase );
	m_bPumping = true;
	sirScriptResult_t eFirstFailure = SIR_SCRIPT_OK;
	for ( uint32_t iTask = 0; iTask < nBudget; ++iTask ) {
		sirScriptIngressPacket_t packet = {};
		if ( !m_Ingress.tryPop( ePhase, nEndTicket, &packet ) ) {
			break;
		}
		++*pProcessed;
		const sirScriptReadFrame_t payload = { packet.nBytes == 0 ? nullptr : packet.szPayload, packet.nBytes, packet.nAlignment };
		const sirScriptResult_t eResult = publishEvent( packet.nEventTypeId, payload );
		if ( eFirstFailure == SIR_SCRIPT_OK && eResult != SIR_SCRIPT_OK ) {
			eFirstFailure = eResult;
		}
	}
	m_bPumping = false;
	return eFirstFailure;
}

sirScriptRuntimeFootprint_t SIRScriptRuntime::queryMemoryFootprint( void ) const noexcept
{
	if ( !isOwnerThread() ) {
		return {};
	}
	const uint32_t nScheduled = m_Scheduler.pendingCount( SIR_SCRIPT_PHASE_EARLY ) + m_Scheduler.pendingCount( SIR_SCRIPT_PHASE_LATE );
	const uint32_t nIngress = m_Ingress.pendingCount( SIR_SCRIPT_PHASE_EARLY ) + m_Ingress.pendingCount( SIR_SCRIPT_PHASE_LATE );
	return { sizeof( *this ), sizeof( m_Scheduler ), sizeof( m_Ingress ), nScheduled, nIngress };
}
