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

#include <Core/Platform/SIRInput.h>
#include <Core/Platform/SIRPlatformTime.h>
#include <limits.h>

SIRInputSystem::SIRInputSystem( void ) noexcept
    : m_Devices{}, m_Events{}, m_nSequence( 0 ), m_nRead( 0 ), m_nWrite( 0 ), m_nCount( 0 ), m_nActiveDevices( 0 ), m_bOverflowed( false ),
      m_Published{}, m_nPublishedTick( 0 ), m_bHasPublished( false )
{
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		m_Devices[i].nGeneration = 1;
	}
}

bool SIRInputSystem::isValid( sirInputDeviceHandle_t hDevice ) const noexcept
{
	return hDevice.nSlot < SIR_INPUT_MAX_DEVICES && m_Devices[hDevice.nSlot].bActive &&
	       m_Devices[hDevice.nSlot].nGeneration == hDevice.nGeneration;
}

uint64_t SIRInputSystem::resolveTime( uint64_t nTimestampNs ) const noexcept
{
	return nTimestampNs != 0 ? nTimestampNs : SIRPlatformTime::getMonotonicNanoseconds();
}

bool SIRInputSystem::enqueue( const sirInputEvent_t &event ) noexcept
{
	if ( m_nCount == SIR_INPUT_MAX_EVENTS ) {
		m_bOverflowed = true;
		return false;
	}
	m_Events[m_nWrite] = event;
	m_nWrite = ( m_nWrite + 1 ) % SIR_INPUT_MAX_EVENTS;
	++m_nCount;
	return true;
}

bool SIRInputSystem::findDevice( sirInputDeviceKind_t eKind, uint64_t nNativeId, sirInputDeviceHandle_t *pHandle ) const noexcept
{
	if ( pHandle == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		if ( m_Devices[i].bActive && m_Devices[i].state.eKind == eKind && m_Devices[i].state.nNativeId == nNativeId ) {
			*pHandle = { i, m_Devices[i].nGeneration };
			return true;
		}
	}
	return false;
}

bool SIRInputSystem::addDevice( sirInputDeviceKind_t eKind, uint64_t nNativeId, sirInputDeviceHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr || eKind > SIR_INPUT_DEVICE_GAMEPAD || findDevice( eKind, nNativeId, pHandle ) || m_bOverflowed ||
	     m_nCount == SIR_INPUT_MAX_EVENTS ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		sirInputDeviceSlot_t &slot = m_Devices[i];
		if ( slot.bActive ) {
			continue;
		}
		memset( &slot.state, 0, sizeof( slot.state ) );
		slot.state.eKind = eKind;
		slot.state.nNativeId = nNativeId;
		slot.bActive = true;
		++m_nActiveDevices;
		*pHandle = { i, slot.nGeneration };
		sirInputEvent_t event = { resolveTime( 0 ), ++m_nSequence, *pHandle, 0, 0, 0, SIR_INPUT_EVENT_CONNECTED, eKind };
		return enqueue( event );
	}
	return false;
}

bool SIRInputSystem::removeDevice( sirInputDeviceHandle_t hDevice ) noexcept
{
	if ( !isValid( hDevice ) ) {
		return false;
	}
	sirInputDeviceSlot_t &slot = m_Devices[hDevice.nSlot];
	sirInputEvent_t event = { resolveTime( 0 ), ++m_nSequence, hDevice, 0, 0, 0, SIR_INPUT_EVENT_DISCONNECTED, slot.state.eKind };
	const bool bQueued = enqueue( event );
	memset( &slot.state, 0, sizeof( slot.state ) );
	slot.bActive = false;
	++slot.nGeneration;
	if ( slot.nGeneration == 0 ) {
		slot.nGeneration = 1;
	}
	--m_nActiveDevices;
	return bQueued;
}

bool SIRInputSystem::pushButton( sirInputDeviceHandle_t hDevice, uint16_t nCode, bool bPressed, uint64_t nTimestampNs ) noexcept
{
	if ( !isValid( hDevice ) || nCode >= SIR_INPUT_MAX_BUTTONS || m_bOverflowed ) {
		return false;
	}
	sirInputDeviceState_t &state = m_Devices[hDevice.nSlot].state;
	const uint32_t nWord = nCode / 32;
	const uint32_t nMask = UINT32_C( 1 ) << ( nCode % 32 );
	const bool bHeld = ( state.nButtons[nWord] & nMask ) != 0;
	if ( bHeld == bPressed ) {
		return true;
	}
	sirInputEvent_t event = { resolveTime( nTimestampNs ), ++m_nSequence, hDevice, bPressed ? 1 : 0, 0, nCode,
	                          SIR_INPUT_EVENT_BUTTON,      state.eKind };
	if ( !enqueue( event ) ) {
		return false;
	}
	if ( bPressed ) {
		state.nButtons[nWord] |= nMask;
		state.nPressed[nWord] |= nMask;
	} else {
		state.nButtons[nWord] &= ~nMask;
		state.nReleased[nWord] |= nMask;
	}
	return true;
}

bool SIRInputSystem::submitGamepadSample( sirInputDeviceHandle_t hDevice, const sirInputGamepadSample_t &sample,
                                          uint64_t nTimestampNs ) noexcept
{
	if ( !isValid( hDevice ) || m_Devices[hDevice.nSlot].state.eKind != SIR_INPUT_DEVICE_GAMEPAD || m_bOverflowed ||
	     ( sample.nButtons >> SIR_GAMEPAD_BUTTON_COUNT ) != 0 ) {
		return false;
	}
	for ( uint32_t iAxis = 0; iAxis < SIR_GAMEPAD_AXIS_COUNT; ++iAxis ) {
		const bool bTrigger = iAxis >= SIR_GAMEPAD_AXIS_LEFT_TRIGGER;
		if ( sample.nAxes[iAxis] > 32767 || sample.nAxes[iAxis] < ( bTrigger ? 0 : -32767 ) ) {
			return false;
		}
	}
	const sirInputDeviceState_t &state = m_Devices[hDevice.nSlot].state;
	uint32_t nPending = 0;
	uint32_t nChanges = state.nButtons[0] ^ sample.nButtons;
	while ( nChanges != 0 ) {
		nChanges &= nChanges - 1;
		++nPending;
	}
	for ( uint32_t iAxis = 0; iAxis < SIR_GAMEPAD_AXIS_COUNT; ++iAxis ) {
		if ( state.nAxes[iAxis] != sample.nAxes[iAxis] ) {
			++nPending;
		}
	}
	// Preflight the whole sample: partial gamepad packets are not published.
	if ( nPending > SIR_INPUT_MAX_EVENTS - m_nCount ) {
		m_bOverflowed = true;
		return false;
	}
	for ( uint16_t iButton = 0; iButton < SIR_GAMEPAD_BUTTON_COUNT; ++iButton ) {
		const uint32_t nMask = UINT32_C( 1 ) << iButton;
		if ( ( ( state.nButtons[0] ^ sample.nButtons ) & nMask ) != 0 &&
		     !pushButton( hDevice, iButton, ( sample.nButtons & nMask ) != 0, nTimestampNs ) ) {
			return false;
		}
	}
	for ( uint16_t iAxis = 0; iAxis < SIR_GAMEPAD_AXIS_COUNT; ++iAxis ) {
		if ( !pushAxis( hDevice, iAxis, sample.nAxes[iAxis], nTimestampNs ) ) {
			return false;
		}
	}
	return true;
}

bool SIRInputSystem::submitGamepadFrame( sirInputDeviceHandle_t hDevice, const sirInputGamepadFrameEvent_t *pEvents, uint32_t nCount,
                                         uint64_t nTimestampNs ) noexcept
{
	if ( !isValid( hDevice ) || m_Devices[hDevice.nSlot].state.eKind != SIR_INPUT_DEVICE_GAMEPAD || m_bOverflowed ||
	     nCount > SIR_INPUT_MAX_EVENTS || ( nCount != 0 && pEvents == nullptr ) ) {
		return false;
	}
	if ( nCount == 0 ) {
		return true;
	}

	// Validate and count actual transitions before touching the queue or the
	// published device state. A single packet is committed in original order.
	sirInputDeviceState_t scratch = m_Devices[hDevice.nSlot].state;
	uint32_t nPending = 0;
	for ( uint32_t iEvent = 0; iEvent < nCount; ++iEvent ) {
		const sirInputGamepadFrameEvent_t &event = pEvents[iEvent];
		if ( event.eKind == SIR_INPUT_EVENT_BUTTON ) {
			if ( event.nCode >= SIR_INPUT_MAX_BUTTONS || ( event.nValue != 0 && event.nValue != 1 ) ) {
				return false;
			}
			const uint32_t nWord = event.nCode / 32;
			const uint32_t nMask = UINT32_C( 1 ) << ( event.nCode % 32 );
			const bool bPressed = event.nValue != 0;
			if ( ( ( scratch.nButtons[nWord] & nMask ) != 0 ) != bPressed ) {
				++nPending;
				if ( bPressed ) {
					scratch.nButtons[nWord] |= nMask;
				} else {
					scratch.nButtons[nWord] &= ~nMask;
				}
			}
		} else if ( event.eKind == SIR_INPUT_EVENT_AXIS ) {
			if ( event.nCode >= SIR_INPUT_MAX_AXES ) {
				return false;
			}
			if ( event.nCode < SIR_GAMEPAD_AXIS_COUNT ) {
				const bool bTrigger = event.nCode >= SIR_GAMEPAD_AXIS_LEFT_TRIGGER;
				if ( event.nValue > 32767 || event.nValue < ( bTrigger ? 0 : -32767 ) ) {
					return false;
				}
			}
			if ( scratch.nAxes[event.nCode] != event.nValue ) {
				++nPending;
				scratch.nAxes[event.nCode] = event.nValue;
			}
		} else {
			return false;
		}
	}
	if ( nPending > SIR_INPUT_MAX_EVENTS - m_nCount ) {
		m_bOverflowed = true;
		return false;
	}
	for ( uint32_t iEvent = 0; iEvent < nCount; ++iEvent ) {
		const sirInputGamepadFrameEvent_t &event = pEvents[iEvent];
		if ( event.eKind == SIR_INPUT_EVENT_BUTTON ) {
			if ( !pushButton( hDevice, event.nCode, event.nValue != 0, nTimestampNs ) ) {
				return false;
			}
		} else if ( !pushAxis( hDevice, event.nCode, event.nValue, nTimestampNs ) ) {
			return false;
		}
	}
	return true;
}

bool SIRInputSystem::pushAxis( sirInputDeviceHandle_t hDevice, uint16_t nAxis, int32_t nValue, uint64_t nTimestampNs ) noexcept
{
	if ( !isValid( hDevice ) || nAxis >= SIR_INPUT_MAX_AXES || m_bOverflowed ) {
		return false;
	}
	sirInputDeviceState_t &state = m_Devices[hDevice.nSlot].state;
	if ( state.nAxes[nAxis] == nValue ) {
		return true;
	}
	sirInputEvent_t event = { resolveTime( nTimestampNs ), ++m_nSequence, hDevice, nValue, 0, nAxis, SIR_INPUT_EVENT_AXIS, state.eKind };
	if ( !enqueue( event ) ) {
		return false;
	}
	state.nAxes[nAxis] = nValue;
	return true;
}

bool SIRInputSystem::pushPointer( sirInputDeviceHandle_t hDevice, int32_t nDeltaX, int32_t nDeltaY, uint64_t nTimestampNs ) noexcept
{
	if ( !isValid( hDevice ) || m_Devices[hDevice.nSlot].state.eKind != SIR_INPUT_DEVICE_MOUSE || m_bOverflowed ) {
		return false;
	}
	sirInputDeviceState_t &state = m_Devices[hDevice.nSlot].state;
	sirInputEvent_t event = { resolveTime( nTimestampNs ), ++m_nSequence, hDevice, nDeltaX, nDeltaY, 0,
	                          SIR_INPUT_EVENT_POINTER,     state.eKind };
	if ( !enqueue( event ) ) {
		return false;
	}
	const int64_t nSumX = ( int64_t )state.nMouseDeltaX + nDeltaX;
	const int64_t nSumY = ( int64_t )state.nMouseDeltaY + nDeltaY;
	state.nMouseDeltaX = nSumX > INT_MAX ? INT_MAX : ( nSumX < INT_MIN ? INT_MIN : ( int32_t )nSumX );
	state.nMouseDeltaY = nSumY > INT_MAX ? INT_MAX : ( nSumY < INT_MIN ? INT_MIN : ( int32_t )nSumY );
	return true;
}

bool SIRInputSystem::pushWheel( sirInputDeviceHandle_t hDevice, uint16_t nAxis, int32_t nDelta, uint64_t nTimestampNs ) noexcept
{
	if ( !isValid( hDevice ) || m_Devices[hDevice.nSlot].state.eKind != SIR_INPUT_DEVICE_MOUSE || nAxis > 1 || m_bOverflowed ) {
		return false;
	}
	sirInputEvent_t event = { resolveTime( nTimestampNs ), ++m_nSequence,         hDevice, nDelta, 0, nAxis,
	                          SIR_INPUT_EVENT_WHEEL,       SIR_INPUT_DEVICE_MOUSE };
	return enqueue( event );
}

bool SIRInputSystem::pollEvent( sirInputEvent_t *pEvent ) noexcept
{
	if ( pEvent == nullptr || m_nCount == 0 ) {
		return false;
	}
	*pEvent = m_Events[m_nRead];
	m_nRead = ( m_nRead + 1 ) % SIR_INPUT_MAX_EVENTS;
	--m_nCount;
	return true;
}

bool SIRInputSystem::getState( sirInputDeviceHandle_t hDevice, sirInputDeviceState_t *pState ) const noexcept
{
	if ( pState == nullptr || !isValid( hDevice ) ) {
		return false;
	}
	*pState = m_Devices[hDevice.nSlot].state;
	return true;
}

bool SIRInputSystem::isDeviceActive( sirInputDeviceHandle_t hDevice ) const noexcept
{
	return isValid( hDevice );
}

bool SIRInputSystem::publishTick( uint64_t nTick ) noexcept
{
	if ( nTick == 0 || m_bOverflowed || ( m_bHasPublished && nTick <= m_nPublishedTick ) ) {
		return false;
	}
	// Like Source Engine's queued/current states, keep one stable tick view
	// while the input owner prepares the next live state. Never publish
	// native handles or references to mutable state.
	memcpy( m_Published, m_Devices, sizeof( m_Devices ) );
	m_nPublishedTick = nTick;
	m_bHasPublished = true;
	return true;
}

bool SIRInputSystem::getPublishedState( sirInputDeviceHandle_t hDevice, sirInputDeviceState_t *pState ) const noexcept
{
	if ( pState == nullptr || !m_bHasPublished || hDevice.nSlot >= SIR_INPUT_MAX_DEVICES ) {
		return false;
	}
	const sirInputDeviceSlot_t &slot = m_Published[hDevice.nSlot];
	if ( !slot.bActive || slot.nGeneration != hDevice.nGeneration ) {
		return false;
	}
	*pState = slot.state;
	return true;
}

uint64_t SIRInputSystem::getPublishedTick( void ) const noexcept
{
	return m_bHasPublished ? m_nPublishedTick : 0;
}

bool SIRInputSystem::isOverflowed( void ) const noexcept
{
	return m_bOverflowed;
}

bool SIRInputSystem::resynchronizeDevice( sirInputDeviceHandle_t hDevice ) noexcept
{
	if ( !isValid( hDevice ) ) {
		return false;
	}
	// A backend must reload authoritative hardware state after this reset.
	// The event stream cannot claim that dropped transitions were observed.
	if ( m_bOverflowed ) {
		m_nRead = m_nWrite = m_nCount = 0;
		m_bOverflowed = false;
		for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
			if ( m_Devices[i].bActive ) {
				sirInputDeviceState_t &other = m_Devices[i].state;
				memset( other.nButtons, 0, sizeof( other.nButtons ) );
				memset( other.nPressed, 0, sizeof( other.nPressed ) );
				memset( other.nReleased, 0, sizeof( other.nReleased ) );
				memset( other.nAxes, 0, sizeof( other.nAxes ) );
				other.nMouseDeltaX = other.nMouseDeltaY = 0;
				sirInputDeviceHandle_t hOther = { i, m_Devices[i].nGeneration };
				sirInputEvent_t resync = { resolveTime( 0 ), ++m_nSequence, hOther, 0, 0, 0, SIR_INPUT_EVENT_RESYNC, other.eKind };
				enqueue( resync );
			}
		}
		return true;
	}
	if ( m_nCount == SIR_INPUT_MAX_EVENTS ) {
		m_bOverflowed = true;
		return false;
	}
	sirInputDeviceState_t &state = m_Devices[hDevice.nSlot].state;
	memset( state.nButtons, 0, sizeof( state.nButtons ) );
	memset( state.nPressed, 0, sizeof( state.nPressed ) );
	memset( state.nReleased, 0, sizeof( state.nReleased ) );
	memset( state.nAxes, 0, sizeof( state.nAxes ) );
	state.nMouseDeltaX = state.nMouseDeltaY = 0;
	sirInputEvent_t event = { resolveTime( 0 ), ++m_nSequence, hDevice, 0, 0, 0, SIR_INPUT_EVENT_RESYNC, state.eKind };
	return enqueue( event );
}

uint32_t SIRInputSystem::getActiveDeviceCount( void ) const noexcept
{
	return m_nActiveDevices;
}

uint32_t SIRInputSystem::enumerateDevices( sirInputDeviceHandle_t *pOutput, uint32_t nCapacity ) const noexcept
{
	if ( pOutput == nullptr || nCapacity == 0 ) {
		return m_nActiveDevices;
	}
	uint32_t nWritten = 0;
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES && nWritten < nCapacity; ++i ) {
		if ( m_Devices[i].bActive ) {
			pOutput[nWritten++] = { i, m_Devices[i].nGeneration };
		}
	}
	return nWritten;
}

void SIRInputSystem::beginTick( void ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		if ( !m_Devices[i].bActive ) {
			continue;
		}
		sirInputDeviceState_t &state = m_Devices[i].state;
		memset( state.nPressed, 0, sizeof( state.nPressed ) );
		memset( state.nReleased, 0, sizeof( state.nReleased ) );
		state.nMouseDeltaX = state.nMouseDeltaY = 0;
	}
}

void SIRInputSystem::shutdown( void ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		m_Devices[i].bActive = false;
		++m_Devices[i].nGeneration;
		if ( m_Devices[i].nGeneration == 0 ) {
			m_Devices[i].nGeneration = 1;
		}
	}
	m_nRead = m_nWrite = m_nCount = 0;
	m_nActiveDevices = 0;
	m_bOverflowed = false;
	m_bHasPublished = false;
	m_nPublishedTick = 0;
}
