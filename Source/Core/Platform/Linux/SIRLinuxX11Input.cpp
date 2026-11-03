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

#include <Core/Platform/Linux/SIRLinuxX11Input.h>

#include <limits.h>
#include <X11/XKBlib.h>

SIRLinuxX11Input::SIRLinuxX11Input( SIRInputSystem &input ) noexcept
    : m_Input( input ), m_hWindow( 0 ), m_hKeyboard{}, m_hMouse{}, m_nLastRootX( 0 ), m_nLastRootY( 0 ), m_bAttached( false ),
      m_bFocused( false ), m_bHavePointerPosition( false )
{
}

SIRLinuxX11Input::~SIRLinuxX11Input()
{
	shutdown();
}

long SIRLinuxX11Input::requiredEventMask( void ) noexcept
{
	return FocusChangeMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
}

bool SIRLinuxX11Input::enableDetectableAutoRepeat( Display *pDisplay ) noexcept
{
	if ( pDisplay == nullptr ) {
		return false;
	}
	Bool bSupported = False;
	return XkbSetDetectableAutoRepeat( pDisplay, True, &bSupported ) != 0 && bSupported != False;
}

bool SIRLinuxX11Input::bind( Window hWindow, uint64_t nKeyboardNativeId, uint64_t nMouseNativeId ) noexcept
{
	if ( m_bAttached || hWindow == 0 || nKeyboardNativeId == 0 || nMouseNativeId == 0 ) {
		return false;
	}
	if ( !m_Input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, nKeyboardNativeId, &m_hKeyboard ) ) {
		return false;
	}
	if ( !m_Input.addDevice( SIR_INPUT_DEVICE_MOUSE, nMouseNativeId, &m_hMouse ) ) {
		m_Input.removeDevice( m_hKeyboard );
		m_hKeyboard = {};
		return false;
	}
	m_hWindow = hWindow;
	m_bAttached = true;
	m_bFocused = false;
	m_bHavePointerPosition = false;
	return true;
}

bool SIRLinuxX11Input::loseFocus( void ) noexcept
{
	m_bFocused = false;
	m_bHavePointerPosition = false;
	if ( m_Input.isOverflowed() ) {
		return m_Input.resynchronizeDevice( m_hKeyboard );
	}
	if ( !m_Input.resynchronizeDevice( m_hKeyboard ) ) {
		return m_Input.isOverflowed() && m_Input.resynchronizeDevice( m_hKeyboard );
	}
	if ( !m_Input.resynchronizeDevice( m_hMouse ) ) {
		return m_Input.isOverflowed() && m_Input.resynchronizeDevice( m_hKeyboard );
	}
	return true;
}

bool SIRLinuxX11Input::handleButton( const XButtonEvent &event, bool bPressed ) noexcept
{
	const uint32_t nButton = event.button;
	if ( nButton >= 4 && nButton <= 7 ) {
		if ( !bPressed ) {
			return true;
		}
		const uint16_t nAxis = nButton <= 5 ? 0 : 1;
		const int32_t nDelta = ( nButton == 4 || nButton == 6 ) ? 120 : -120;
		return m_Input.pushWheel( m_hMouse, nAxis, nDelta, 0 );
	}
	uint16_t nCode = 0;
	switch ( nButton ) {
	case Button1:
		nCode = 0;
		break;
	case Button2:
		nCode = 2;
		break;
	case Button3:
		nCode = 1;
		break;
	default:
		if ( nButton < 8 || nButton >= SIR_INPUT_MAX_BUTTONS ) {
			return false;
		}
		nCode = ( uint16_t )nButton;
		break;
	}
	return m_Input.pushButton( m_hMouse, nCode, bPressed, 0 );
}

bool SIRLinuxX11Input::handleMotion( const XMotionEvent &event ) noexcept
{
	const int32_t nRootX = event.x_root;
	const int32_t nRootY = event.y_root;
	if ( !m_bHavePointerPosition ) {
		m_bHavePointerPosition = true;
		m_nLastRootX = nRootX;
		m_nLastRootY = nRootY;
		return true;
	}
	const int64_t nDeltaX = ( int64_t )nRootX - m_nLastRootX;
	const int64_t nDeltaY = ( int64_t )nRootY - m_nLastRootY;
	const int32_t nClampedX = ( int32_t )( nDeltaX > INT_MAX ? INT_MAX : ( nDeltaX < INT_MIN ? INT_MIN : nDeltaX ) );
	const int32_t nClampedY = ( int32_t )( nDeltaY > INT_MAX ? INT_MAX : ( nDeltaY < INT_MIN ? INT_MIN : nDeltaY ) );
	if ( ( nClampedX != 0 || nClampedY != 0 ) && !m_Input.pushPointer( m_hMouse, nClampedX, nClampedY, 0 ) ) {
		return false;
	}
	m_nLastRootX = nRootX;
	m_nLastRootY = nRootY;
	return true;
}

bool SIRLinuxX11Input::handleEvent( const XEvent &event ) noexcept
{
	if ( !m_bAttached || event.xany.window != m_hWindow ) {
		return false;
	}
	switch ( event.type ) {
	case FocusIn:
		m_bFocused = true;
		m_bHavePointerPosition = false;
		return true;
	case FocusOut:
		return loseFocus();
	default:
		break;
	}
	if ( !m_bFocused ) {
		return false;
	}
	switch ( event.type ) {
	case KeyPress:
	case KeyRelease:
		// X11's physical server keycode is distinct from a keysym or UTF-8
		// text/IME stream. Translation belongs to the separate text service.
		return event.xkey.keycode < SIR_INPUT_MAX_BUTTONS &&
		       m_Input.pushButton( m_hKeyboard, ( uint16_t )event.xkey.keycode, event.type == KeyPress, 0 );
	case ButtonPress:
	case ButtonRelease:
		return handleButton( event.xbutton, event.type == ButtonPress );
	case MotionNotify:
		return handleMotion( event.xmotion );
	default:
		return false;
	}
}

void SIRLinuxX11Input::shutdown( void ) noexcept
{
	if ( !m_bAttached ) {
		return;
	}
	m_Input.removeDevice( m_hKeyboard );
	m_Input.removeDevice( m_hMouse );
	m_hKeyboard = {};
	m_hMouse = {};
	m_hWindow = 0;
	m_bAttached = false;
	m_bFocused = false;
	m_bHavePointerPosition = false;
}
