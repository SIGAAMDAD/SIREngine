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

#include <Core/Platform/Input/SIRInputFrameState.h>
#include <Core/Platform/Input/SIRInputDeviceRegistry.h>
#include <Core/Platform/SIRPlatformTime.h>
#include <limits.h>

static int32_t saturateInputMovement( int32_t nOld, int32_t nDelta ) noexcept
{
	const int64_t nSum = ( int64_t )nOld + nDelta;
	return nSum > INT32_MAX ? INT32_MAX : ( nSum < INT32_MIN ? INT32_MIN : ( int32_t )nSum );
}

SIRInputFrameState::SIRInputFrameState( void ) noexcept
    : m_Live{}, m_Frames{}, m_nTick( 0 ), m_nSequence( 0 ), m_nLastTick( 0 ), m_nCount( 0 ), m_nPublishedIndex( 0 ), m_nResync( 0 ),
      m_bCollecting( false ), m_bOverflowed( false ), m_bPublished( false )
{
}

bool SIRInputFrameState::begin( uint64_t nTick ) noexcept
{
	if ( m_bCollecting || m_bOverflowed || ( m_bPublished && nTick <= m_nLastTick ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		memset( m_Live[i].nPressed, 0, sizeof( m_Live[i].nPressed ) );
		memset( m_Live[i].nReleased, 0, sizeof( m_Live[i].nReleased ) );
		m_Live[i].nMotionX = m_Live[i].nMotionY = 0;
	}
	m_nCount = 0;
	m_nResync = 0;
	m_bOverflowed = false;
	m_nTick = nTick;
	m_bCollecting = true;
	return true;
}

bool SIRInputFrameState::append( const sirInputEvent_t &event ) noexcept
{
	if ( m_bOverflowed || m_nCount >= SIR_INPUT_EVENT_CAPACITY ) {
		m_bOverflowed = true;
		return false;
	}
	m_Frames[m_nPublishedIndex ^ 1].events[m_nCount++] = event;
	return true;
}

bool SIRInputFrameState::apply( sirInputDeviceState_t &state, const sirInputControlChange_t &change ) const noexcept
{
	if ( change.eSignal == SIR_INPUT_BUTTON ) {
		if ( change.nControl >= SIR_INPUT_BUTTON_CAPACITY || ( change.nValue != 0 && change.nValue != 1 ) ) {
			return false;
		}
		const uint32_t nWord = change.nControl / 32;
		const uint32_t nMask = UINT32_C( 1 ) << ( change.nControl % 32 );
		const bool bDown = change.nValue != 0;
		if ( bDown ) {
			state.nHeld[nWord] |= nMask;
		} else {
			state.nHeld[nWord] &= ~nMask;
		}
		return true;
	}
	if ( change.eSignal == SIR_INPUT_AXIS && change.nControl < SIR_INPUT_AXIS_CAPACITY ) {
		if ( change.nControl < SIR_INPUT_AXIS_COUNT ) {
			if ( change.nValue > 32767 || change.nValue < ( change.nControl >= SIR_INPUT_AXIS_LT ? 0 : -32767 ) ) {
				return false;
			}
		}
		state.nAxes[change.nControl] = change.nValue;
		return true;
	}
	return false;
}

bool SIRInputFrameState::postChanges( sirInputDeviceId_t id, sirInputDeviceClass_t eClass, const sirInputControlChange_t *pChanges,
                                      uint32_t nCount, uint64_t nTime ) noexcept
{
	if ( !m_bCollecting || m_bOverflowed || id.nSlot >= SIR_INPUT_DEVICE_CAPACITY || ( nCount != 0 && pChanges == nullptr ) ||
	     nCount > SIR_INPUT_EVENT_CAPACITY ) {
		return false;
	}
	// The common native path emits a single physical transition. Avoid a
	// full device-state scratch copy and a second scan for that case.
	if ( nCount == 1 ) {
		const sirInputControlChange_t &change = pChanges[0];
		sirInputDeviceState_t &state = m_Live[id.nSlot];
		bool bChanged = false;
		if ( change.eSignal == SIR_INPUT_BUTTON ) {
			if ( change.nControl >= SIR_INPUT_BUTTON_CAPACITY || ( change.nValue != 0 && change.nValue != 1 ) ) {
				return false;
			}
			const uint32_t nWord = change.nControl / 32;
			const uint32_t nMask = UINT32_C( 1 ) << ( change.nControl % 32 );
			const bool bDown = change.nValue != 0;
			bChanged = ( ( state.nHeld[nWord] & nMask ) != 0 ) != bDown;
			if ( !bChanged ) {
				return true;
			}
			if ( m_nCount == SIR_INPUT_EVENT_CAPACITY ) {
				m_bOverflowed = true;
				++m_nResync;
				return false;
			}
			if ( bDown ) {
				state.nHeld[nWord] |= nMask;
				state.nPressed[nWord] |= nMask;
			} else {
				state.nHeld[nWord] &= ~nMask;
				state.nReleased[nWord] |= nMask;
			}
		} else if ( change.eSignal == SIR_INPUT_AXIS ) {
			if ( change.nControl >= SIR_INPUT_AXIS_CAPACITY ) {
				return false;
			}
			if ( change.nControl < SIR_INPUT_AXIS_COUNT ) {
				const bool bTrigger = change.nControl >= SIR_INPUT_AXIS_LT;
				if ( change.nValue > 32767 || change.nValue < ( bTrigger ? 0 : -32767 ) ) {
					return false;
				}
			}
			bChanged = state.nAxes[change.nControl] != change.nValue;
			if ( !bChanged ) {
				return true;
			}
			if ( m_nCount == SIR_INPUT_EVENT_CAPACITY ) {
				m_bOverflowed = true;
				++m_nResync;
				return false;
			}
			state.nAxes[change.nControl] = change.nValue;
		} else {
			return false;
		}
		const sirInputEvent_t event = { nTime, ++m_nSequence, id, change.nValue, 0, change.nControl, change.eSignal, eClass };
		return append( event );
	}

	// Validate as one transaction, including repeated presses and releases.
	sirInputDeviceState_t scratch = m_Live[id.nSlot];
	uint32_t nChanges = 0;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const sirInputControlChange_t &change = pChanges[i];
		if ( change.eSignal == SIR_INPUT_BUTTON && change.nControl >= SIR_INPUT_BUTTON_CAPACITY ) {
			return false;
		}
		bool bChanged = false;
		if ( change.eSignal == SIR_INPUT_BUTTON ) {
			const uint32_t nMask = UINT32_C( 1 ) << ( change.nControl % 32 );
			bChanged = ( ( scratch.nHeld[change.nControl / 32] & nMask ) != 0 ) != ( change.nValue != 0 );
		} else if ( change.eSignal == SIR_INPUT_AXIS && change.nControl < SIR_INPUT_AXIS_CAPACITY ) {
			bChanged = scratch.nAxes[change.nControl] != change.nValue;
		}
		if ( !apply( scratch, change ) ) {
			return false;
		}
		if ( bChanged ) {
			++nChanges;
		}
	}
	if ( nChanges > SIR_INPUT_EVENT_CAPACITY - m_nCount ) {
		m_bOverflowed = true;
		++m_nResync;
		return false;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const sirInputControlChange_t &change = pChanges[i];
		sirInputDeviceState_t &state = m_Live[id.nSlot];
		bool bChanged = false;
		if ( change.eSignal == SIR_INPUT_BUTTON ) {
			const uint32_t nWord = change.nControl / 32;
			const uint32_t nMask = UINT32_C( 1 ) << ( change.nControl % 32 );
			bChanged = ( ( state.nHeld[nWord] & nMask ) != 0 ) != ( change.nValue != 0 );
			if ( bChanged ) {
				if ( change.nValue ) {
					state.nPressed[nWord] |= nMask;
				} else {
					state.nReleased[nWord] |= nMask;
				}
			}
		} else {
			bChanged = state.nAxes[change.nControl] != change.nValue;
		}
		if ( bChanged ) {
			apply( state, change );
			sirInputEvent_t event = { nTime, ++m_nSequence, id, change.nValue, 0, change.nControl, change.eSignal, eClass };
			append( event );
		}
	}
	return true;
}

bool SIRInputFrameState::postDevice( sirInputDeviceId_t id, sirInputDeviceClass_t eClass, sirInputSignal_t eSignal,
                                     uint64_t nTime ) noexcept
{
	if ( !m_bCollecting || id.nSlot >= SIR_INPUT_DEVICE_CAPACITY ||
	     ( eSignal != SIR_INPUT_CONNECTED && eSignal != SIR_INPUT_DISCONNECTED && eSignal != SIR_INPUT_RESYNC ) ) {
		return false;
	}
	sirInputEvent_t event = { nTime, ++m_nSequence, id, 0, 0, 0, eSignal, eClass };
	if ( !append( event ) ) {
		return false;
	}
	if ( eSignal != SIR_INPUT_CONNECTED ) {
		clearDevice( id );
	}
	if ( eSignal == SIR_INPUT_RESYNC ) {
		++m_nResync;
	}
	return true;
}

bool SIRInputFrameState::postPointer( sirInputDeviceId_t id, sirInputDeviceClass_t eClass, sirInputSignal_t eSignal, uint16_t nControl,
                                      int32_t nX, int32_t nY, uint64_t nTime ) noexcept
{
	if ( !m_bCollecting || m_bOverflowed || id.nSlot >= SIR_INPUT_DEVICE_CAPACITY || eClass != SIR_INPUT_POINTER ||
	     ( eSignal != SIR_INPUT_MOTION && eSignal != SIR_INPUT_WHEEL ) ) {
		return false;
	}
	sirInputEvent_t event = { nTime, ++m_nSequence, id, nX, nY, nControl, eSignal, eClass };
	if ( !append( event ) ) {
		return false;
	}
	if ( eSignal == SIR_INPUT_MOTION ) {
		sirInputDeviceState_t &state = m_Live[id.nSlot];
		state.nMotionX = saturateInputMovement( state.nMotionX, nX );
		state.nMotionY = saturateInputMovement( state.nMotionY, nY );
	}
	return true;
}

void SIRInputFrameState::clearDevice( sirInputDeviceId_t id ) noexcept
{
	if ( id.nSlot < SIR_INPUT_DEVICE_CAPACITY ) {
		memset( &m_Live[id.nSlot], 0, sizeof( m_Live[id.nSlot] ) );
	}
}

bool SIRInputFrameState::publish( const SIRInputDeviceRegistry &registry ) noexcept
{
	if ( !m_bCollecting || m_bOverflowed ) {
		return false;
	}
	const uint32_t nNext = m_nPublishedIndex ^ 1;
	sirInputFrame_t &frame = m_Frames[nNext];
	frame.nTick = m_nTick;
	frame.nCount = m_nCount;
	frame.nResyncCount = m_nResync;
	memcpy( frame.state, m_Live, sizeof( m_Live ) );
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		sirInputDeviceDescriptor_t descriptor = {};
		registry.inspectSlot( i, &descriptor );
		frame.devices[i] = descriptor.id;
		frame.nConnected[i] = descriptor.bConnected ? 1 : 0;
		frame.nUsers[i] = descriptor.bConnected ? descriptor.nUserId : SIR_INPUT_NO_USER;
	}
	m_nPublishedIndex = nNext;
	m_nLastTick = m_nTick;
	m_bPublished = true;
	m_bCollecting = false;
	return true;
}

bool SIRInputFrameState::canPost( uint32_t nCount ) const noexcept
{
	return m_bCollecting && !m_bOverflowed && nCount <= SIR_INPUT_EVENT_CAPACITY - m_nCount;
}

bool SIRInputFrameState::recover( const SIRInputDeviceRegistry &registry ) noexcept
{
	if ( !m_bCollecting || !m_bOverflowed ) {
		return false;
	}
	m_nCount = 0;
	m_bOverflowed = false;
	m_nResync = 0;
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		sirInputDeviceDescriptor_t descriptor = {};
		registry.inspectSlot( i, &descriptor );
		if ( descriptor.bConnected ) {
			clearDevice( descriptor.id );
			sirInputEvent_t event = { SIRPlatformTime::getMonotonicNanoseconds(),
			                          ++m_nSequence,
			                          descriptor.id,
			                          0,
			                          0,
			                          0,
			                          SIR_INPUT_RESYNC,
			                          descriptor.key.eClass };
			append( event );
			++m_nResync;
		}
	}
	return true;
}

bool SIRInputFrameState::overflowed( void ) const noexcept
{
	return m_bOverflowed;
}
bool SIRInputFrameState::collecting( void ) const noexcept
{
	return m_bCollecting;
}
uint64_t SIRInputFrameState::lastPublishedTick( void ) const noexcept
{
	return m_nLastTick;
}
const sirInputFrame_t *SIRInputFrameState::frame( void ) const noexcept
{
	return m_bPublished ? &m_Frames[m_nPublishedIndex] : nullptr;
}
void SIRInputFrameState::stop( void ) noexcept
{
	m_bCollecting = false;
	m_bPublished = false;
	m_nCount = 0;
	m_bOverflowed = false;
}
