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
#include <math.h>
#if defined( SIR_INPUT_XINPUT2_AVAILABLE )
#include <X11/extensions/XInput2.h>
#endif

SIRLinuxX11Backend::SIRLinuxX11Backend( void ) noexcept
    : m_hWindow( 0 ), m_hKeyboard{}, m_hPointer{}, m_nRootX( 0 ), m_nRootY( 0 ), m_bAttached( false ), m_bFocused( false ),
      m_bHaveMotion( false ), m_bRawPointer( false ), m_nXiOpcode( 0 ), m_fMotionRemainderX( 0.0 ), m_fMotionRemainderY( 0.0 )
{
}

sirInputBackendId_t SIRLinuxX11Backend::id( void ) const noexcept
{
	return SIR_INPUT_LINUX_X11;
}
void SIRLinuxX11Backend::sample( SIRInputService& service ) noexcept
{
	( void )service;
}
void SIRLinuxX11Backend::resynchronize( SIRInputService& service ) noexcept
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

bool SIRLinuxX11Backend::attachWindow( SIRInputService& service, Window hWindow ) noexcept
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

bool SIRLinuxX11Backend::focusWindow( SIRInputService& service, Window hWindow ) noexcept
{
	if ( !m_bAttached || hWindow == 0 ) {
		return false;
	}
	if ( m_hWindow == hWindow ) {
		return true;
	}
	// One native X11 seat serves all windows. Retarget the existing keyboard
	// and pointer identities instead of registering duplicate devices.
	if ( !service.loseBackendState( id() ) ) {
		return false;
	}
	m_hWindow = hWindow;
	m_bFocused = false;
	m_bHaveMotion = false;
	m_fMotionRemainderX = 0.0;
	m_fMotionRemainderY = 0.0;
	return true;
}

bool SIRLinuxX11Backend::enableRawPointer( Display *pDisplay ) noexcept
{
#if defined( SIR_INPUT_XINPUT2_AVAILABLE )
	if ( pDisplay == nullptr || !m_bAttached || m_bRawPointer ) {
		return false;
	}
	int nOpcode = 0;
	int nEvent = 0;
	int nError = 0;
	if ( !XQueryExtension( pDisplay, "XInputExtension", &nOpcode, &nEvent, &nError ) ) {
		return false;
	}
	int nMajor = 2;
	int nMinor = 0;
	if ( XIQueryVersion( pDisplay, &nMajor, &nMinor ) != Success || nMajor < 2 ) {
		return false;
	}
	unsigned char szMask[XIMaskLen( XI_RawMotion )] = {};
	XISetMask( szMask, XI_RawMotion );
	XIEventMask mask = {};
	mask.deviceid = XIAllMasterDevices;
	mask.mask_len = sizeof( szMask );
	mask.mask = szMask;
	if ( XISelectEvents( pDisplay, DefaultRootWindow( pDisplay ), &mask, 1 ) != Success ) {
		return false;
	}
	XFlush( pDisplay );
	m_nXiOpcode = nOpcode;
	m_bRawPointer = true;
	m_fMotionRemainderX = 0.0;
	m_fMotionRemainderY = 0.0;
	return true;
#else
	( void )pDisplay;
	return false;
#endif
}

bool SIRLinuxX11Backend::processEvent( SIRInputService& service, const XEvent& event ) noexcept
{
#if defined( SIR_INPUT_XINPUT2_AVAILABLE )
	if ( m_bAttached && m_bRawPointer && event.type == GenericEvent && event.xcookie.extension == m_nXiOpcode &&
	     event.xcookie.display != nullptr ) {
		XGenericEventCookie cookie = event.xcookie;
		if ( !XGetEventData( event.xcookie.display, &cookie ) ) {
			return false;
		}
		bool bProcessed = false;
		if ( cookie.evtype == XI_RawMotion && cookie.data != nullptr && m_bFocused ) {
			const XIRawEvent *pRaw = ( const XIRawEvent * )cookie.data;
			const double *pValues = pRaw->raw_values;
			double fDeltaX = 0.0;
			double fDeltaY = 0.0;
			for ( int iAxis = 0; iAxis < pRaw->valuators.mask_len * 8; ++iAxis ) {
				if ( XIMaskIsSet( pRaw->valuators.mask, iAxis ) ) {
					const double fValue = *pValues++;
					if ( iAxis == 0 ) {
						fDeltaX = fValue;
					} else if ( iAxis == 1 ) {
						fDeltaY = fValue;
					}
				}
			}
			if ( isfinite( fDeltaX ) && isfinite( fDeltaY ) ) {
				const double fX = fDeltaX + m_fMotionRemainderX;
				const double fY = fDeltaY + m_fMotionRemainderY;
				const int32_t nX = fX > INT32_MAX ? INT32_MAX : ( fX < INT32_MIN ? INT32_MIN : ( int32_t )fX );
				const int32_t nY = fY > INT32_MAX ? INT32_MAX : ( fY < INT32_MIN ? INT32_MIN : ( int32_t )fY );
				m_fMotionRemainderX = fX > INT32_MAX || fX < INT32_MIN ? 0.0 : fX - nX;
				m_fMotionRemainderY = fY > INT32_MAX || fY < INT32_MIN ? 0.0 : fY - nY;
				bProcessed = ( nX == 0 && nY == 0 ) || service.emitPointer( id(), m_hPointer, SIR_INPUT_MOTION, 0, nX, nY );
			}
		}
		XFreeEventData( event.xcookie.display, &cookie );
		return bProcessed;
	}
#endif
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
		if ( m_bRawPointer ) {
			return true;
		}
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

void SIRLinuxX11Backend::stop( SIRInputService& service ) noexcept
{
	if ( m_bAttached ) {
		service.disconnect( id(), m_hKeyboard );
		service.disconnect( id(), m_hPointer );
	}
	m_hWindow = 0;
	m_hKeyboard = {};
	m_hPointer = {};
	m_bAttached = m_bFocused = m_bHaveMotion = false;
	m_bRawPointer = false;
	m_nXiOpcode = 0;
	m_fMotionRemainderX = 0.0;
	m_fMotionRemainderY = 0.0;
}
