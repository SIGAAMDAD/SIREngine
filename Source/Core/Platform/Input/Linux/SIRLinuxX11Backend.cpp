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

#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>
#include <X11/XKBlib.h>
#include <limits.h>

SIRLinuxX11Backend::SIRLinuxX11Backend( void ) noexcept
    : m_hWindow( 0 ), m_hKeyboard{}, m_hPointer{}, m_nRootX( 0 ), m_nRootY( 0 ), m_bAttached( false ), m_bFocused( false ),
      m_bHaveMotion( false )
{
}

sirInputBackendId_t SIRLinuxX11Backend::id( void ) const noexcept
{
	return SIR_INPUT_LINUX_X11;
}
void SIRLinuxX11Backend::sample( SIRInputService &service ) noexcept
{
	( void )service;
}
void SIRLinuxX11Backend::resynchronize( SIRInputService &service ) noexcept
{
	( void )service;
	m_bHaveMotion = false;
}
bool SIRLinuxX11Backend::vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept
{
	( void )hDevice;
	( void )nLeft;
	( void )nRight;
	return false;
}

long SIRLinuxX11Backend::requiredEventMask( void ) noexcept
{
	return FocusChangeMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
}

bool SIRLinuxX11Backend::enableDetectableRepeat( Display *pDisplay ) noexcept
{
	Bool bSupported = False;
	return pDisplay != nullptr && XkbSetDetectableAutoRepeat( pDisplay, True, &bSupported ) && bSupported;
}

bool SIRLinuxX11Backend::attachWindow( SIRInputService &service, Window hWindow ) noexcept
{
	if ( m_bAttached || hWindow == 0 ) {
		return false;
	}
	const uint64_t nKey = ( uint64_t )hWindow << 1;
	if ( !service.connect( { nKey, nKey, id(), SIR_INPUT_KEYBOARD }, &m_hKeyboard ) ) {
		return false;
	}
	const uint64_t nPointer = nKey | 1;
	if ( !service.connect( { nPointer, nPointer, id(), SIR_INPUT_POINTER }, &m_hPointer ) ) {
		service.disconnect( id(), m_hKeyboard );
		return false;
	}
	m_hWindow = hWindow;
	m_bAttached = true;
	return true;
}

bool SIRLinuxX11Backend::processEvent( SIRInputService &service, const XEvent &event ) noexcept
{
	if ( !m_bAttached || event.xany.window != m_hWindow ) {
		return false;
	}
	if ( event.type == FocusIn ) {
		m_bFocused = true;
		m_bHaveMotion = false;
		return true;
	}
	if ( event.type == FocusOut ) {
		m_bFocused = false;
		m_bHaveMotion = false;
		const bool bKeyboard = service.loseState( id(), m_hKeyboard );
		const bool bPointer = service.loseState( id(), m_hPointer );
		return bKeyboard && bPointer;
	}
	if ( !m_bFocused ) {
		return false;
	}
	if ( event.type == KeyPress || event.type == KeyRelease ) {
		const sirInputControlChange_t change = { SIR_INPUT_BUTTON, ( uint16_t )event.xkey.keycode, event.type == KeyPress ? 1 : 0 };
		return service.emitControls( id(), m_hKeyboard, &change, 1 );
	}
	if ( event.type == ButtonPress || event.type == ButtonRelease ) {
		const uint32_t nButton = event.xbutton.button;
		if ( nButton >= 4 && nButton <= 7 ) {
			if ( event.type != ButtonPress ) {
				return true;
			}
			const uint16_t nAxis = nButton <= 5 ? 0 : 1;
			const int32_t nDelta = ( nButton == 4 || nButton == 6 ) ? 120 : -120;
			return service.emitPointer( id(), m_hPointer, SIR_INPUT_WHEEL, nAxis, nDelta, 0 );
		}
		if ( nButton == 0 || nButton > 3 ) {
			return true;
		}
		const uint16_t nCode = nButton == 1 ? 0 : ( nButton == 3 ? 1 : 2 );
		const sirInputControlChange_t change = { SIR_INPUT_BUTTON, nCode, event.type == ButtonPress ? 1 : 0 };
		return service.emitControls( id(), m_hPointer, &change, 1 );
	}
	if ( event.type == MotionNotify ) {
		if ( !m_bHaveMotion ) {
			m_nRootX = event.xmotion.x_root;
			m_nRootY = event.xmotion.y_root;
			m_bHaveMotion = true;
			return true;
		}
		const int64_t nDX = ( int64_t )event.xmotion.x_root - m_nRootX;
		const int64_t nDY = ( int64_t )event.xmotion.y_root - m_nRootY;
		m_nRootX = event.xmotion.x_root;
		m_nRootY = event.xmotion.y_root;
		const int32_t nX = ( int32_t )( nDX > INT32_MAX ? INT32_MAX : ( nDX < INT32_MIN ? INT32_MIN : nDX ) );
		const int32_t nY = ( int32_t )( nDY > INT32_MAX ? INT32_MAX : ( nDY < INT32_MIN ? INT32_MIN : nDY ) );
		return service.emitPointer( id(), m_hPointer, SIR_INPUT_MOTION, 0, nX, nY );
	}
	return false;
}

void SIRLinuxX11Backend::stop( SIRInputService &service ) noexcept
{
	if ( m_bAttached ) {
		service.disconnect( id(), m_hKeyboard );
		service.disconnect( id(), m_hPointer );
	}
	m_hWindow = 0;
	m_hKeyboard = {};
	m_hPointer = {};
	m_bAttached = m_bFocused = m_bHaveMotion = false;
}
