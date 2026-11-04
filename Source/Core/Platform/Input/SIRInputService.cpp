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

static bool sirInputRumbleOutput( void *pContext, sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept
{
	return ( ( SIRInputService * )pContext )->vibrate( hDevice, nLeft, nRight );
}

SIRInputService::SIRInputService( void ) noexcept : m_Actions(), m_Text(), m_Haptics(), m_Registry(), m_State(), m_pBackends{}, m_nBackendCount( 0 ), m_bStopping( false ),
      m_bHapticsPaused( false )
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

sirInputTimestamp_t SIRInputService::timestamp( uint64_t nNative, sirInputClockSource_t eClock ) const noexcept
{
	// A caller-supplied engine monotonic value is the receipt timestamp.
	// Only genuinely independent source clocks require a second clock read.
	if ( nNative != 0 && eClock == SIR_INPUT_CLOCK_ENGINE_MONOTONIC ) {
		return { nNative, nNative, SIR_INPUT_CLOCK_ENGINE_MONOTONIC, 0 };
	}
	const uint64_t nReceivedNs = SIRPlatformTime::getMonotonicNanoseconds();
	const bool bNative = nNative != 0;
	return { bNative ? nNative : nReceivedNs, nReceivedNs, bNative ? eClock : SIR_INPUT_CLOCK_ENGINE_MONOTONIC,
	         ( uint8_t )( bNative ? 1 : 0 ) };
}

bool SIRInputService::begin( uint64_t nTick ) noexcept
{
	if ( m_bStopping || !m_State.begin( nTick ) ) {
		return false;
	}
	m_Text.begin( nTick );
	return true;
}

void SIRInputService::sample( void ) noexcept
{
	if ( !m_bStopping && m_State.collecting() ) {
		for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
			m_pBackends[i]->sample( *this );
		}
		( void )m_Haptics.advance( SIRPlatformTime::getMonotonicNanoseconds(), m_bHapticsPaused,
		                          sirInputRumbleOutput, this );
	}
}

bool SIRInputService::publish( void ) noexcept
{
	if ( !m_State.publish( m_Registry ) ) {
		return false;
	}
	m_Text.publish();
	const sirInputFrame_t *pFrame = m_State.frame();
	if ( pFrame != nullptr ) {
		m_Actions.evaluate( *pFrame );
	}
	return true;
}
void SIRInputService::setTimingMetricsEnabled( bool bEnabled ) noexcept
{
	m_State.setTimingMetricsEnabled( bEnabled );
}

const sirInputFrame_t *SIRInputService::frame( void ) const noexcept
{
	return m_State.frame();
}

SIRInputActions &SIRInputService::actions( void ) noexcept
{
	return m_Actions;
}

const SIRInputActions &SIRInputService::actions( void ) const noexcept
{
	return m_Actions;
}

SIRInputText &SIRInputService::text( void ) noexcept
{
	return m_Text;
}

const SIRInputText &SIRInputService::text( void ) const noexcept
{
	return m_Text;
}

bool SIRInputService::scheduleRumble( const sirInputHapticRequest_t &request, sirInputHapticHandle_t *pHandle ) noexcept
{
	if ( m_bStopping || ( hapticCapabilities( request.hDevice ) & SIR_INPUT_HAPTIC_CAP_RUMBLE ) == 0 ) {
		return false;
	}
	return m_Haptics.schedule( request, SIRPlatformTime::getMonotonicNanoseconds(), pHandle );
}

bool SIRInputService::cancelRumble( sirInputHapticHandle_t hEffect ) noexcept
{
	return m_Haptics.cancel( hEffect );
}

void SIRInputService::setHapticsPaused( bool bPaused ) noexcept
{
	m_bHapticsPaused = bPaused;
	if ( bPaused ) {
		( void )m_Haptics.advance( SIRPlatformTime::getMonotonicNanoseconds(), true, sirInputRumbleOutput, this );
		for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
			sirInputDeviceDescriptor_t desc = {};
			if ( m_Registry.inspectSlot( i, &desc ) && desc.bConnected && desc.key.eClass == SIR_INPUT_CONTROLLER ) {
				( void )vibrateTriggers( desc.id, 0, 0 );
			}
		}
	}
}

uint32_t SIRInputService::hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept
{
	sirInputDeviceDescriptor_t desc = {};
	if ( !m_Registry.describe( hDevice, &desc ) || desc.key.eClass != SIR_INPUT_CONTROLLER ) {
		return SIR_INPUT_HAPTIC_CAP_NONE;
	}
	for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
		if ( m_pBackends[i]->id() == desc.key.eBackend ) {
			return m_pBackends[i]->hapticCapabilities( hDevice );
		}
	}
	return SIR_INPUT_HAPTIC_CAP_NONE;
}

bool SIRInputService::vibrateTriggers( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept
{
	if ( m_bStopping || ( m_bHapticsPaused && ( nLeft != 0 || nRight != 0 ) ) ||
	     ( hapticCapabilities( hDevice ) & SIR_INPUT_HAPTIC_CAP_TRIGGER_RUMBLE ) == 0 ) {
		return false;
	}
	sirInputDeviceDescriptor_t desc = {};
	if ( !m_Registry.describe( hDevice, &desc ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
		if ( m_pBackends[i]->id() == desc.key.eBackend ) {
			return m_pBackends[i]->vibrateTriggers( hDevice, nLeft, nRight );
		}
	}
	return false;
}

uint32_t SIRInputService::activeHaptics( void ) const noexcept
{
	return m_Haptics.activeCount();
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
	if ( !m_State.postDevice( *pOut, key.eClass, SIR_INPUT_CONNECTED, timestamp( 0, SIR_INPUT_CLOCK_ENGINE_MONOTONIC ) ) ) {
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
	const bool bQueued =
	    m_State.postDevice( id, desc.key.eClass, SIR_INPUT_DISCONNECTED, timestamp( 0, SIR_INPUT_CLOCK_ENGINE_MONOTONIC ) );
	m_Haptics.forgetDevice( id, sirInputRumbleOutput, this );
	( void )vibrateTriggers( id, 0, 0 );
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
                                    uint32_t nCount, uint64_t nTime, sirInputClockSource_t eClock ) noexcept
{
	sirInputDeviceClass_t eClass = SIR_INPUT_KEYBOARD;
	return m_Registry.authorize( eSource, id, &eClass ) && m_State.postChanges( id, eClass, pChanges, nCount, timestamp( nTime, eClock ) );
}

bool SIRInputService::emitPointer( sirInputBackendId_t eSource, sirInputDeviceId_t id, sirInputSignal_t eSignal, uint16_t nControl,
                                   int32_t nX, int32_t nY, uint64_t nTime, sirInputClockSource_t eClock ) noexcept
{
	sirInputDeviceClass_t eClass = SIR_INPUT_KEYBOARD;
	return m_Registry.authorize( eSource, id, &eClass ) &&
	       m_State.postPointer( id, eClass, eSignal, nControl, nX, nY, timestamp( nTime, eClock ) );
}

bool SIRInputService::loseState( sirInputBackendId_t eSource, sirInputDeviceId_t id ) noexcept
{
	sirInputDeviceClass_t eClass = SIR_INPUT_KEYBOARD;
	if ( !m_Registry.authorize( eSource, id, &eClass ) ) {
		return false;
	}
	return m_State.postDevice( id, eClass, SIR_INPUT_RESYNC, timestamp( 0, SIR_INPUT_CLOCK_ENGINE_MONOTONIC ) );
}

bool SIRInputService::loseBackendState( sirInputBackendId_t eSource ) noexcept
{
	if ( m_bStopping || !m_State.collecting() || !hasBackend( eSource ) ) {
		return false;
	}

	bool bOkay = true;
	for ( uint32_t iSlot = 0; iSlot < SIR_INPUT_DEVICE_CAPACITY; ++iSlot ) {
		sirInputDeviceDescriptor_t desc = {};
		if ( !m_Registry.inspectSlot( iSlot, &desc ) || !desc.bConnected || desc.key.eBackend != eSource ) {
			continue;
		}
		if ( !loseState( eSource, desc.id ) ) {
			bOkay = false;
		}
	}
	return bOkay;
}

bool SIRInputService::vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept
{
	if ( m_bHapticsPaused && ( nLeft != 0 || nRight != 0 ) ) {
		return false;
	}
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
	m_Haptics.stopAll( sirInputRumbleOutput, this );
	m_bStopping = true;
	for ( uint32_t i = m_nBackendCount; i > 0; --i ) {
		m_pBackends[i - 1]->stop( *this );
		m_pBackends[i - 1] = nullptr;
	}
	m_nBackendCount = 0;
	m_State.stop();
	m_Registry.reset();
	m_Actions.clear();
	m_Text.reset();
}
