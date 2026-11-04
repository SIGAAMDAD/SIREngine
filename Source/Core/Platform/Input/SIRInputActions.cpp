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

#include <Core/Platform/Input/SIRInputActions.h>
#include <limits.h>
#include <string.h>


static void sirInputWriteU16( uint8_t *pData, uint16_t nValue ) noexcept
{
	pData[0] = ( uint8_t )nValue;
	pData[1] = ( uint8_t )( nValue >> 8 );
}

static void sirInputWriteU32( uint8_t *pData, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pData[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static uint16_t sirInputReadU16( const uint8_t *pData ) noexcept
{
	return ( uint16_t )( pData[0] | ( uint16_t )( pData[1] << 8 ) );
}

static uint32_t sirInputReadU32( const uint8_t *pData ) noexcept
{
	return ( uint32_t )pData[0] | ( ( uint32_t )pData[1] << 8 ) | ( ( uint32_t )pData[2] << 16 ) |
	       ( ( uint32_t )pData[3] << 24 );
}

static uint32_t sirInputProfileHash( const uint8_t *pData, uint32_t nBytes ) noexcept
{
	uint32_t nHash = UINT32_C( 2166136261 );
	for ( uint32_t i = 0; i < nBytes; ++i ) {
		nHash = ( nHash ^ pData[i] ) * UINT32_C( 16777619 );
	}
	return nHash;
}

SIRInputActions::SIRInputActions( void ) noexcept
    : m_Contexts{}, m_Bindings{}, m_Actions{}, m_Held{}, m_Axes{}, m_DeviceIds{}, m_Classes{}, m_Toggled{}, m_nContexts( 0 ),
      m_nBindings( 0 ), m_nActions( 0 ), m_nLastTick( 0 ), m_bContextDirty( false ), m_eCaptureStatus( SIR_INPUT_CAPTURE_IDLE ),
      m_iCaptureBinding( 0 )
{
}

int32_t SIRInputActions::actionIndex( uint32_t nActionId, uint32_t nUser ) const noexcept
{
	for ( uint32_t i = 0; i < m_nActions; ++i ) {
		if ( m_Actions[i].nActionId == nActionId && m_Actions[i].nUser == nUser ) {
			return ( int32_t )i;
		}
	}
	return -1;
}

int32_t SIRInputActions::contextIndex( uint32_t nContextId, uint32_t nUser ) const noexcept
{
	for ( uint32_t i = 0; i < m_nContexts; ++i ) {
		if ( m_Contexts[i].nContextId == nContextId && m_Contexts[i].nUser == nUser ) {
			return ( int32_t )i;
		}
	}
	return -1;
}

bool SIRInputActions::addAction( uint32_t nActionId, uint32_t nUser ) noexcept
{
	if ( nActionId == 0 || nUser >= SIR_INPUT_USER_CAPACITY || m_nActions >= SIR_INPUT_ACTION_CAPACITY ||
	     actionIndex( nActionId, nUser ) >= 0 ) {
		return false;
	}
	m_Actions[m_nActions++] = { nActionId, nUser, 0, 0, false, false, false };
	return true;
}

bool SIRInputActions::addContext( uint32_t nContextId, uint32_t nUser, int32_t nPriority, bool bActive ) noexcept
{
	if ( nContextId == 0 || nUser >= SIR_INPUT_USER_CAPACITY || m_nContexts >= SIR_INPUT_CONTEXT_CAPACITY ||
	     contextIndex( nContextId, nUser ) >= 0 ) {
		return false;
	}
	m_Contexts[m_nContexts++] = { nContextId, nUser, nPriority, bActive };
	m_bContextDirty = true;
	return true;
}

bool SIRInputActions::setContextActive( uint32_t nContextId, uint32_t nUser, bool bActive ) noexcept
{
	const int32_t i = contextIndex( nContextId, nUser );
	if ( i < 0 ) {
		return false;
	}
	if ( m_Contexts[i].bActive != bActive ) {
		m_Contexts[i].bActive = bActive;
		m_bContextDirty = true;
	}
	return true;
}

bool SIRInputActions::bindingValid( const sirInputActionBinding_t &binding, int32_t iExclude ) const noexcept
{
	if ( actionIndex( binding.nActionId, binding.nUser ) < 0 || contextIndex( binding.nContextId, binding.nUser ) < 0 ||
	     ( binding.eSignal != SIR_INPUT_BUTTON && binding.eSignal != SIR_INPUT_AXIS ) ||
	     binding.eClass > SIR_INPUT_CONTROLLER || binding.nControl >= SIR_INPUT_BUTTON_CAPACITY ||
	     ( binding.eSignal == SIR_INPUT_AXIS && binding.nControl >= SIR_INPUT_AXIS_CAPACITY ) ||
	     ( binding.nChord != SIR_INPUT_NO_CHORD && binding.nChord >= SIR_INPUT_BUTTON_CAPACITY ) ||
	     binding.nDeadzone < 0 || binding.nDeadzone >= 32767 || binding.nScale < -32767 || binding.nScale > 32767 ||
	     ( binding.nFlags & ~( SIR_INPUT_BIND_CONSUME | SIR_INPUT_BIND_TOGGLE ) ) != 0 ||
	     ( ( binding.nFlags & SIR_INPUT_BIND_TOGGLE ) != 0 && binding.eSignal != SIR_INPUT_BUTTON ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nBindings; ++i ) {
		if ( ( int32_t )i == iExclude ) {
			continue;
		}
		const sirInputActionBinding_t &other = m_Bindings[i];
		if ( other.nUser == binding.nUser && other.nContextId == binding.nContextId && other.eClass == binding.eClass &&
		     other.eSignal == binding.eSignal && other.nControl == binding.nControl && other.nChord == binding.nChord ) {
			return false;
		}
	}
	return true;
}

bool SIRInputActions::addBinding( const sirInputActionBinding_t &binding ) noexcept
{
	if ( m_nBindings >= SIR_INPUT_BINDING_CAPACITY || !bindingValid( binding, -1 ) ) {
		return false;
	}
	m_Bindings[m_nBindings++] = binding;
	m_bContextDirty = true;
	return true;
}

bool SIRInputActions::rebind( uint32_t iBinding, const sirInputActionBinding_t &binding ) noexcept
{
	if ( iBinding >= m_nBindings ) {
		return false;
	}
	const sirInputActionBinding_t &current = m_Bindings[iBinding];
	if ( current.nActionId != binding.nActionId || current.nContextId != binding.nContextId ||
	     current.nUser != binding.nUser || !bindingValid( binding, ( int32_t )iBinding ) ) {
		return false;
	}
	m_Bindings[iBinding] = binding;
	m_bContextDirty = true;
	return true;
}

bool SIRInputActions::removeBinding( uint32_t iBinding ) noexcept
{
	if ( iBinding >= m_nBindings || m_eCaptureStatus == SIR_INPUT_CAPTURE_PENDING ) {
		return false;
	}
	for ( uint32_t i = iBinding + 1; i < m_nBindings; ++i ) {
		m_Bindings[i - 1] = m_Bindings[i];
	}
	--m_nBindings;
	m_bContextDirty = true;
	return true;
}

bool SIRInputActions::beginCapture( uint32_t iBinding ) noexcept
{
	if ( iBinding >= m_nBindings || m_eCaptureStatus == SIR_INPUT_CAPTURE_PENDING ||
	     !activeBinding( m_Bindings[iBinding], nullptr ) ) {
		return false;
	}
	m_iCaptureBinding = iBinding;
	m_eCaptureStatus = SIR_INPUT_CAPTURE_PENDING;
	return true;
}

void SIRInputActions::cancelCapture( void ) noexcept
{
	m_eCaptureStatus = SIR_INPUT_CAPTURE_IDLE;
	m_iCaptureBinding = 0;
}

sirInputCaptureStatus_t SIRInputActions::captureStatus( void ) const noexcept
{
	return m_eCaptureStatus;
}

bool SIRInputActions::describeBinding( uint32_t iBinding, sirInputActionBinding_t *pOut ) const noexcept
{
	if ( pOut == nullptr || iBinding >= m_nBindings ) {
		return false;
	}
	*pOut = m_Bindings[iBinding];
	return true;
}

bool SIRInputActions::query( uint32_t nActionId, uint32_t nUser, sirInputActionState_t *pOut ) const noexcept
{
	const int32_t i = actionIndex( nActionId, nUser );
	if ( i < 0 || pOut == nullptr ) {
		return false;
	}
	*pOut = m_Actions[i];
	return true;
}

bool SIRInputActions::activeBinding( const sirInputActionBinding_t &binding, int32_t *pPriority ) const noexcept
{
	const int32_t i = contextIndex( binding.nContextId, binding.nUser );
	if ( i < 0 || !m_Contexts[i].bActive ) {
		return false;
	}
	if ( pPriority != nullptr ) {
		*pPriority = m_Contexts[i].nPriority;
	}
	return true;
}

bool SIRInputActions::bindingHeld( const sirInputActionBinding_t &binding, const sirInputFrame_t &frame ) const noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( !frame.nConnected[i] || m_Classes[i] != binding.eClass ||
		     m_DeviceIds[i].nGeneration != frame.devices[i].nGeneration ) {
			continue;
		}
		const uint32_t nUser = frame.nUsers[i] == SIR_INPUT_NO_USER ? 0 : frame.nUsers[i];
		if ( nUser != binding.nUser ) {
			continue;
		}
		if ( binding.nChord != SIR_INPUT_NO_CHORD &&
		     ( m_Held[i][binding.nChord / 32] & ( UINT32_C( 1 ) << ( binding.nChord % 32 ) ) ) == 0 ) {
			continue;
		}
		if ( binding.eSignal == SIR_INPUT_BUTTON &&
		     ( m_Held[i][binding.nControl / 32] & ( UINT32_C( 1 ) << ( binding.nControl % 32 ) ) ) != 0 ) {
			return true;
		}
		if ( binding.eSignal == SIR_INPUT_AXIS && m_Axes[i][binding.nControl] != 0 ) {
			return true;
		}
	}
	return false;
}

int32_t SIRInputActions::bindingValue( const sirInputActionBinding_t &binding, const sirInputFrame_t &frame ) const noexcept
{
	if ( binding.eSignal == SIR_INPUT_BUTTON ) {
		return bindingHeld( binding, frame ) ? 32767 : 0;
	}
	int32_t nSelected = 0;
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( !frame.nConnected[i] || m_Classes[i] != binding.eClass ||
		     m_DeviceIds[i].nGeneration != frame.devices[i].nGeneration ) {
			continue;
		}
		const uint32_t nUser = frame.nUsers[i] == SIR_INPUT_NO_USER ? 0 : frame.nUsers[i];
		if ( nUser != binding.nUser ) {
			continue;
		}
		if ( binding.nChord != SIR_INPUT_NO_CHORD &&
		     ( m_Held[i][binding.nChord / 32] & ( UINT32_C( 1 ) << ( binding.nChord % 32 ) ) ) == 0 ) {
			continue;
		}
		const int32_t nRaw = m_Axes[i][binding.nControl];
		const int64_t nMagnitude = nRaw < 0 ? -( int64_t )nRaw : nRaw;
		if ( nMagnitude <= binding.nDeadzone ) {
			continue;
		}
		const int64_t nNormalized = ( nMagnitude - binding.nDeadzone ) * 32767 / ( 32767 - binding.nDeadzone );
		const int64_t nSigned = ( nRaw < 0 ? -nNormalized : nNormalized ) * binding.nScale / 32767;
		const int32_t nValue = ( int32_t )nSigned;
		if ( ( nValue < 0 ? -( int64_t )nValue : nValue ) > ( nSelected < 0 ? -( int64_t )nSelected : nSelected ) ) {
			nSelected = nValue;
		}
	}
	return nSelected;
}

bool SIRInputActions::shadowed( const sirInputActionBinding_t &binding, const sirInputFrame_t &frame ) const noexcept
{
	int32_t nPriority = 0;
	if ( !activeBinding( binding, &nPriority ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nBindings; ++i ) {
		const sirInputActionBinding_t &other = m_Bindings[i];
		if ( other.nUser != binding.nUser || other.eClass != binding.eClass || other.eSignal != binding.eSignal ||
		     other.nControl != binding.nControl || ( other.nFlags & SIR_INPUT_BIND_CONSUME ) == 0 ) {
			continue;
		}
		int32_t nOtherPriority = 0;
		if ( activeBinding( other, &nOtherPriority ) && nOtherPriority > nPriority && bindingValue( other, frame ) != 0 ) {
			return true;
		}
	}
	return false;
}

void SIRInputActions::refreshAction( uint32_t iAction, const sirInputFrame_t &frame, int32_t nMinimumPriority, bool bEdge ) noexcept
{
	sirInputActionState_t &state = m_Actions[iAction];
	int32_t nNewValue = 0;
	for ( uint32_t i = 0; i < m_nBindings; ++i ) {
		const sirInputActionBinding_t &binding = m_Bindings[i];
		if ( binding.nActionId != state.nActionId || binding.nUser != state.nUser ) {
			continue;
		}
		int32_t nPriority = 0;
		if ( !activeBinding( binding, &nPriority ) || nPriority < nMinimumPriority || shadowed( binding, frame ) ) {
			continue;
		}
		const int32_t nValue = ( binding.nFlags & SIR_INPUT_BIND_TOGGLE ) != 0
		                           ? ( m_Toggled[iAction] ? 32767 : 0 )
		                           : bindingValue( binding, frame );
		if ( ( nValue < 0 ? -( int64_t )nValue : nValue ) > ( nNewValue < 0 ? -( int64_t )nNewValue : nNewValue ) ) {
			nNewValue = nValue;
		}
	}
	const bool bNewHeld = nNewValue != 0;
	if ( bEdge && bNewHeld != state.bHeld ) {
		state.bPressed |= bNewHeld;
		state.bReleased |= !bNewHeld;
		++state.nTransitions;
	}
	state.nValue = nNewValue;
	state.bHeld = bNewHeld;
}

void SIRInputActions::processEvent( const sirInputEvent_t &event, const sirInputFrame_t &frame ) noexcept
{
	if ( event.hDevice.nSlot >= SIR_INPUT_DEVICE_CAPACITY ) {
		return;
	}
	const uint32_t iSlot = event.hDevice.nSlot;
	if ( event.eSignal == SIR_INPUT_CONNECTED ) {
		m_DeviceIds[iSlot] = event.hDevice;
		m_Classes[iSlot] = event.eClass;
		memset( m_Held[iSlot], 0, sizeof( m_Held[iSlot] ) );
		memset( m_Axes[iSlot], 0, sizeof( m_Axes[iSlot] ) );
		return;
	}
	if ( m_DeviceIds[iSlot].nGeneration != event.hDevice.nGeneration ) {
		return;
	}
	if ( event.eSignal == SIR_INPUT_DISCONNECTED || event.eSignal == SIR_INPUT_RESYNC ) {
		memset( m_Held[iSlot], 0, sizeof( m_Held[iSlot] ) );
		memset( m_Axes[iSlot], 0, sizeof( m_Axes[iSlot] ) );
		for ( uint32_t i = 0; i < m_nActions; ++i ) {
			refreshAction( i, frame, INT32_MIN, true );
		}
		return;
	}
	if ( event.eSignal != SIR_INPUT_BUTTON && event.eSignal != SIR_INPUT_AXIS ) {
		return;
	}
	if ( event.eSignal == SIR_INPUT_BUTTON ) {
		if ( event.nControl >= SIR_INPUT_BUTTON_CAPACITY ) {
			return;
		}
		const uint32_t nMask = UINT32_C( 1 ) << ( event.nControl % 32 );
		if ( event.nValue ) {
			m_Held[iSlot][event.nControl / 32] |= nMask;
		} else {
			m_Held[iSlot][event.nControl / 32] &= ~nMask;
		}
	} else {
		if ( event.nControl >= SIR_INPUT_AXIS_CAPACITY ) {
			return;
		}
		m_Axes[iSlot][event.nControl] = event.nValue;
	}

	const uint32_t nUser = frame.nUsers[iSlot] == SIR_INPUT_NO_USER ? 0 : frame.nUsers[iSlot];
	if ( m_eCaptureStatus == SIR_INPUT_CAPTURE_PENDING && m_iCaptureBinding < m_nBindings ) {
		const sirInputActionBinding_t &old = m_Bindings[m_iCaptureBinding];
		const bool bButton = event.eSignal == SIR_INPUT_BUTTON && event.nValue != 0;
		const bool bAxis = event.eSignal == SIR_INPUT_AXIS && ( event.nValue > 16384 || event.nValue < -16384 );
		if ( old.nUser == nUser && ( bButton || bAxis ) ) {
			sirInputActionBinding_t changed = old;
			changed.eClass = event.eClass;
			changed.eSignal = event.eSignal;
			changed.nControl = event.nControl;
			changed.nChord = SIR_INPUT_NO_CHORD;
			if ( bAxis ) {
				changed.nFlags &= ( uint8_t )~SIR_INPUT_BIND_TOGGLE;
			}
			m_eCaptureStatus = rebind( m_iCaptureBinding, changed ) ? SIR_INPUT_CAPTURE_ACCEPTED : SIR_INPUT_CAPTURE_CONFLICT;
			return;
		}
	}
	for ( uint32_t i = 0; i < m_nBindings; ++i ) {
		const sirInputActionBinding_t &binding = m_Bindings[i];
		if ( binding.nUser != nUser || binding.eClass != event.eClass ||
		     ( binding.eSignal != event.eSignal && binding.nChord != event.nControl ) ||
		     ( binding.nControl != event.nControl && binding.nChord != event.nControl ) ) {
			continue;
		}
		int32_t nPriority = 0;
		if ( !activeBinding( binding, &nPriority ) ) {
			continue;
		}
		const int32_t iAction = actionIndex( binding.nActionId, binding.nUser );
		if ( iAction < 0 ) {
			continue;
		}
		if ( ( binding.nFlags & SIR_INPUT_BIND_TOGGLE ) != 0 && event.eSignal == SIR_INPUT_BUTTON &&
		     binding.nControl == event.nControl && event.nValue != 0 &&
		     ( binding.nChord == SIR_INPUT_NO_CHORD ||
		       ( m_Held[iSlot][binding.nChord / 32] & ( UINT32_C( 1 ) << ( binding.nChord % 32 ) ) ) != 0 ) ) {
			m_Toggled[iAction] = !m_Toggled[iAction];
		}
		refreshAction( ( uint32_t )iAction, frame, INT32_MIN, true );
	}
}

void SIRInputActions::evaluate( const sirInputFrame_t &frame ) noexcept
{
	if ( frame.nTick == 0 || ( m_nLastTick != 0 && frame.nTick <= m_nLastTick ) ) {
		return;
	}
	for ( uint32_t i = 0; i < m_nActions; ++i ) {
		m_Actions[i].bPressed = false;
		m_Actions[i].bReleased = false;
		m_Actions[i].nTransitions = 0;
	}
	if ( m_bContextDirty ) {
		for ( uint32_t i = 0; i < m_nActions; ++i ) {
			refreshAction( i, frame, INT32_MIN, false );
		}
		m_bContextDirty = false;
	}
	for ( uint32_t i = 0; i < frame.nCount && i < SIR_INPUT_EVENT_CAPACITY; ++i ) {
		processEvent( frame.events[i], frame );
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( !frame.nConnected[i] ) {
			memset( m_Held[i], 0, sizeof( m_Held[i] ) );
			memset( m_Axes[i], 0, sizeof( m_Axes[i] ) );
			continue;
		}
		if ( m_DeviceIds[i].nGeneration != frame.devices[i].nGeneration ) {
			m_DeviceIds[i] = frame.devices[i];
		}
		memcpy( m_Held[i], frame.state[i].nHeld, sizeof( m_Held[i] ) );
		memcpy( m_Axes[i], frame.state[i].nAxes, sizeof( m_Axes[i] ) );
	}
	for ( uint32_t i = 0; i < m_nActions; ++i ) {
		refreshAction( i, frame, INT32_MIN, false );
	}
	m_nLastTick = frame.nTick;
}


bool SIRInputActions::saveProfile( uint8_t *pData, uint32_t nCapacity, uint32_t *pWritten ) const noexcept
{
	const uint32_t nRequired = SIR_INPUT_PROFILE_HEADER_BYTES + m_nContexts * SIR_INPUT_PROFILE_CONTEXT_BYTES +
	                           m_nActions * SIR_INPUT_PROFILE_ACTION_BYTES + m_nBindings * SIR_INPUT_PROFILE_BINDING_BYTES;
	if ( pWritten != nullptr ) {
		*pWritten = nRequired;
	}
	if ( pData == nullptr || nCapacity < nRequired ) {
		return false;
	}

	memset( pData, 0, nRequired );
	sirInputWriteU32( pData, UINT32_C( 0x41524953 ) );
	sirInputWriteU16( pData + 4, 1 );
	sirInputWriteU16( pData + 6, SIR_INPUT_PROFILE_HEADER_BYTES );
	sirInputWriteU32( pData + 8, m_nContexts );
	sirInputWriteU32( pData + 12, m_nActions );
	sirInputWriteU32( pData + 16, m_nBindings );
	uint32_t nOffset = SIR_INPUT_PROFILE_HEADER_BYTES;
	for ( uint32_t i = 0; i < m_nContexts; ++i ) {
		const sirInputActionContext_t &context = m_Contexts[i];
		sirInputWriteU32( pData + nOffset, context.nContextId );
		sirInputWriteU32( pData + nOffset + 4, context.nUser );
		sirInputWriteU32( pData + nOffset + 8, ( uint32_t )context.nPriority );
		pData[nOffset + 12] = ( uint8_t )( context.bActive ? 1 : 0 );
		nOffset += SIR_INPUT_PROFILE_CONTEXT_BYTES;
	}
	for ( uint32_t i = 0; i < m_nActions; ++i ) {
		sirInputWriteU32( pData + nOffset, m_Actions[i].nActionId );
		sirInputWriteU32( pData + nOffset + 4, m_Actions[i].nUser );
		nOffset += SIR_INPUT_PROFILE_ACTION_BYTES;
	}
	for ( uint32_t i = 0; i < m_nBindings; ++i ) {
		const sirInputActionBinding_t &binding = m_Bindings[i];
		sirInputWriteU32( pData + nOffset, binding.nActionId );
		sirInputWriteU32( pData + nOffset + 4, binding.nContextId );
		sirInputWriteU16( pData + nOffset + 8, binding.nControl );
		sirInputWriteU16( pData + nOffset + 10, binding.nChord );
		sirInputWriteU32( pData + nOffset + 12, binding.nUser );
		sirInputWriteU32( pData + nOffset + 16, ( uint32_t )binding.nDeadzone );
		sirInputWriteU32( pData + nOffset + 20, ( uint32_t )binding.nScale );
		pData[nOffset + 24] = ( uint8_t )binding.eClass;
		pData[nOffset + 25] = ( uint8_t )binding.eSignal;
		pData[nOffset + 26] = binding.nFlags;
		nOffset += SIR_INPUT_PROFILE_BINDING_BYTES;
	}
	sirInputWriteU32( pData + 20, sirInputProfileHash( pData + SIR_INPUT_PROFILE_HEADER_BYTES,
	                                                nRequired - SIR_INPUT_PROFILE_HEADER_BYTES ) );
	return true;
}

bool SIRInputActions::loadProfile( const uint8_t *pData, uint32_t nBytes ) noexcept
{
	if ( pData == nullptr || nBytes < SIR_INPUT_PROFILE_HEADER_BYTES ||
	     sirInputReadU32( pData ) != UINT32_C( 0x41524953 ) || sirInputReadU16( pData + 4 ) != 1 ||
	     sirInputReadU16( pData + 6 ) != SIR_INPUT_PROFILE_HEADER_BYTES ) {
		return false;
	}
	const uint32_t nContexts = sirInputReadU32( pData + 8 );
	const uint32_t nActions = sirInputReadU32( pData + 12 );
	const uint32_t nBindings = sirInputReadU32( pData + 16 );
	if ( nContexts > SIR_INPUT_CONTEXT_CAPACITY || nActions > SIR_INPUT_ACTION_CAPACITY ||
	     nBindings > SIR_INPUT_BINDING_CAPACITY ) {
		return false;
	}
	const uint64_t nExpected = SIR_INPUT_PROFILE_HEADER_BYTES + ( uint64_t )nContexts * SIR_INPUT_PROFILE_CONTEXT_BYTES +
	                           ( uint64_t )nActions * SIR_INPUT_PROFILE_ACTION_BYTES +
	                           ( uint64_t )nBindings * SIR_INPUT_PROFILE_BINDING_BYTES;
	if ( nExpected != nBytes ||
	     sirInputReadU32( pData + 20 ) != sirInputProfileHash( pData + SIR_INPUT_PROFILE_HEADER_BYTES,
	                                                          nBytes - SIR_INPUT_PROFILE_HEADER_BYTES ) ) {
		return false;
	}

	SIRInputActions scratch;
	uint32_t nOffset = SIR_INPUT_PROFILE_HEADER_BYTES;
	for ( uint32_t i = 0; i < nContexts; ++i ) {
		const uint32_t nId = sirInputReadU32( pData + nOffset );
		const uint32_t nUser = sirInputReadU32( pData + nOffset + 4 );
		const int32_t nPriority = ( int32_t )sirInputReadU32( pData + nOffset + 8 );
		const uint8_t nActive = pData[nOffset + 12];
		if ( nActive > 1 || pData[nOffset + 13] != 0 || pData[nOffset + 14] != 0 || pData[nOffset + 15] != 0 ||
		     !scratch.addContext( nId, nUser, nPriority, nActive != 0 ) ) {
			return false;
		}
		nOffset += SIR_INPUT_PROFILE_CONTEXT_BYTES;
	}
	for ( uint32_t i = 0; i < nActions; ++i ) {
		if ( !scratch.addAction( sirInputReadU32( pData + nOffset ), sirInputReadU32( pData + nOffset + 4 ) ) ) {
			return false;
		}
		nOffset += SIR_INPUT_PROFILE_ACTION_BYTES;
	}
	for ( uint32_t i = 0; i < nBindings; ++i ) {
		sirInputActionBinding_t binding = {};
		binding.nActionId = sirInputReadU32( pData + nOffset );
		binding.nContextId = sirInputReadU32( pData + nOffset + 4 );
		binding.nControl = sirInputReadU16( pData + nOffset + 8 );
		binding.nChord = sirInputReadU16( pData + nOffset + 10 );
		binding.nUser = sirInputReadU32( pData + nOffset + 12 );
		binding.nDeadzone = ( int32_t )sirInputReadU32( pData + nOffset + 16 );
		binding.nScale = ( int32_t )sirInputReadU32( pData + nOffset + 20 );
		binding.eClass = ( sirInputDeviceClass_t )pData[nOffset + 24];
		binding.eSignal = ( sirInputSignal_t )pData[nOffset + 25];
		binding.nFlags = pData[nOffset + 26];
		if ( pData[nOffset + 27] != 0 || !scratch.addBinding( binding ) ) {
			return false;
		}
		nOffset += SIR_INPUT_PROFILE_BINDING_BYTES;
	}
	*this = scratch;
	return true;
}

void SIRInputActions::clear( void ) noexcept
{
	memset( m_Contexts, 0, sizeof( m_Contexts ) );
	memset( m_Bindings, 0, sizeof( m_Bindings ) );
	memset( m_Actions, 0, sizeof( m_Actions ) );
	memset( m_Held, 0, sizeof( m_Held ) );
	memset( m_Axes, 0, sizeof( m_Axes ) );
	memset( m_DeviceIds, 0, sizeof( m_DeviceIds ) );
	memset( m_Classes, 0, sizeof( m_Classes ) );
	memset( m_Toggled, 0, sizeof( m_Toggled ) );
	m_nContexts = 0;
	m_nBindings = 0;
	m_nActions = 0;
	m_nLastTick = 0;
	m_bContextDirty = false;
	m_eCaptureStatus = SIR_INPUT_CAPTURE_IDLE;
	m_iCaptureBinding = 0;
}

uint32_t SIRInputActions::bindingCount( void ) const noexcept
{
	return m_nBindings;
}

uint32_t SIRInputActions::contextCount( void ) const noexcept
{
	return m_nContexts;
}
