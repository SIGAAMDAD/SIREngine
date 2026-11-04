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

#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/SIRPlatformTime.h>

SIRInputService::SIRInputService( void ) noexcept : m_Registry(), m_State(), m_pBackends{}, m_nBackendCount( 0 ), m_bStopping( false )
{
}

bool SIRInputService::hasBackend( sirInputBackendId_t eSource ) const noexcept
{
	for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
		if ( m_pBackends[i]->id() == eSource ) {
			return true;
		}
	}
	return false;
}

bool SIRInputService::attach( SIRInputBackend &backend ) noexcept
{
	if ( m_bStopping || m_State.collecting() || m_nBackendCount == SIR_INPUT_BACKEND_CAPACITY || backend.id() == 0 ||
	     hasBackend( backend.id() ) ) {
		return false;
	}
	m_pBackends[m_nBackendCount++] = &backend;
	return true;
}

uint64_t SIRInputService::timestamp( uint64_t nNative ) const noexcept
{
	return nNative != 0 ? nNative : SIRPlatformTime::getMonotonicNanoseconds();
}

bool SIRInputService::begin( uint64_t nTick ) noexcept
{
	return !m_bStopping && m_State.begin( nTick );
}

void SIRInputService::sample( void ) noexcept
{
	if ( !m_bStopping && m_State.collecting() ) {
		for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
			m_pBackends[i]->sample( *this );
		}
	}
}

bool SIRInputService::publish( void ) noexcept
{
	return m_State.publish( m_Registry );
}
const sirInputFrame_t *SIRInputService::frame( void ) const noexcept
{
	return m_State.frame();
}

bool SIRInputService::recover( void ) noexcept
{
	if ( !m_State.recover( m_Registry ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
		m_pBackends[i]->resynchronize( *this );
	}
	return true;
}

bool SIRInputService::verifyOwner( sirInputBackendId_t eSource, sirInputDeviceId_t id, sirInputDeviceDescriptor_t *pOut ) const noexcept
{
	return hasBackend( eSource ) && m_Registry.describe( id, pOut ) && pOut->key.eBackend == eSource;
}

bool SIRInputService::connect( const sirInputHardwareKey_t &key, sirInputDeviceId_t *pOut ) noexcept
{
	if ( m_bStopping || !m_State.collecting() || !hasBackend( key.eBackend ) || pOut == nullptr || !m_State.canPost( 1 ) ||
	     !m_Registry.connect( key, pOut ) ) {
		return false;
	}
	if ( !m_State.postDevice( *pOut, key.eClass, SIR_INPUT_CONNECTED, timestamp( 0 ) ) ) {
		m_Registry.disconnect( *pOut );
		return false;
	}
	return true;
}

bool SIRInputService::disconnect( sirInputBackendId_t eSource, sirInputDeviceId_t id ) noexcept
{
	sirInputDeviceDescriptor_t desc = {};
	if ( !m_State.collecting() || !verifyOwner( eSource, id, &desc ) ) {
		return false;
	}
	// Always retire the handle, even when queue capacity is exhausted.
	const bool bQueued = m_State.postDevice( id, desc.key.eClass, SIR_INPUT_DISCONNECTED, timestamp( 0 ) );
	m_State.clearDevice( id );
	m_Registry.disconnect( id );
	return bQueued;
}

bool SIRInputService::pairUser( sirInputDeviceId_t id, uint32_t nUser ) noexcept
{
	return m_Registry.pairUser( id, nUser );
}
bool SIRInputService::describe( sirInputDeviceId_t id, sirInputDeviceDescriptor_t *pOut ) const noexcept
{
	return m_Registry.describe( id, pOut );
}
bool SIRInputService::findDevice( sirInputBackendId_t eSource, uint64_t nNative, sirInputDeviceId_t *pOut ) const noexcept
{
	return m_Registry.resolve( eSource, nNative, pOut );
}

bool SIRInputService::emitControls( sirInputBackendId_t eSource, sirInputDeviceId_t id, const sirInputControlChange_t *pChanges,
                                    uint32_t nCount, uint64_t nTime ) noexcept
{
	sirInputDeviceClass_t eClass = SIR_INPUT_KEYBOARD;
	return m_Registry.authorize( eSource, id, &eClass ) && m_State.postChanges( id, eClass, pChanges, nCount, timestamp( nTime ) );
}

bool SIRInputService::emitPointer( sirInputBackendId_t eSource, sirInputDeviceId_t id, sirInputSignal_t eSignal, uint16_t nControl,
                                   int32_t nX, int32_t nY, uint64_t nTime ) noexcept
{
	sirInputDeviceClass_t eClass = SIR_INPUT_KEYBOARD;
	return m_Registry.authorize( eSource, id, &eClass ) && m_State.postPointer( id, eClass, eSignal, nControl, nX, nY, timestamp( nTime ) );
}

bool SIRInputService::loseState( sirInputBackendId_t eSource, sirInputDeviceId_t id ) noexcept
{
	sirInputDeviceClass_t eClass = SIR_INPUT_KEYBOARD;
	if ( !m_Registry.authorize( eSource, id, &eClass ) ) {
		return false;
	}
	return m_State.postDevice( id, eClass, SIR_INPUT_RESYNC, timestamp( 0 ) );
}

bool SIRInputService::vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept
{
	sirInputDeviceDescriptor_t desc = {};
	if ( !m_Registry.describe( id, &desc ) || desc.key.eClass != SIR_INPUT_CONTROLLER ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
		if ( m_pBackends[i]->id() == desc.key.eBackend ) {
			return m_pBackends[i]->vibrate( id, nLeft, nRight );
		}
	}
	return false;
}

uint32_t SIRInputService::activeDeviceCount( void ) const noexcept
{
	return m_Registry.count();
}

void SIRInputService::shutdown( void ) noexcept
{
	if ( m_bStopping ) {
		return;
	}
	m_bStopping = true;
	for ( uint32_t i = m_nBackendCount; i > 0; --i ) {
		m_pBackends[i - 1]->stop( *this );
		m_pBackends[i - 1] = nullptr;
	}
	m_nBackendCount = 0;
	m_State.stop();
	m_Registry.reset();
}
