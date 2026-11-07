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
#include <Core/Platform/Window/SIRInputWindow.h>
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>
#include <Core/Platform/Input/Linux/SIRLinuxEvdevBackend.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef enum sirX11Fault_e : uint8_t
{
	SIR_X11_FAULT_NONE = 0,
	SIR_X11_FAULT_CREATE_WINDOW,
	SIR_X11_FAULT_CREATE_BITMAP,
	SIR_X11_FAULT_CREATE_CURSOR,
	SIR_X11_FAULT_GRAB_POINTER,
	SIR_X11_FAULT_FILTER_EVENT,
	SIR_X11_FAULT_GENERIC_EVENT
} sirX11Fault_t;

static sirX11Fault_t s_eX11Fault = SIR_X11_FAULT_NONE;

extern "C" Window __real_XCreateSimpleWindow( Display *pDisplay, Window hParent, int nX, int nY, unsigned int nWidth, unsigned int nHeight,
                                              unsigned int nBorderWidth, unsigned long nBorder, unsigned long nBackground );
extern "C" Window __wrap_XCreateSimpleWindow( Display *pDisplay, Window hParent, int nX, int nY, unsigned int nWidth, unsigned int nHeight,
                                              unsigned int nBorderWidth, unsigned long nBorder, unsigned long nBackground )
{
	if ( s_eX11Fault == SIR_X11_FAULT_CREATE_WINDOW ) {
		s_eX11Fault = SIR_X11_FAULT_NONE;
		return 0;
	}
	return __real_XCreateSimpleWindow( pDisplay, hParent, nX, nY, nWidth, nHeight, nBorderWidth, nBorder, nBackground );
}

extern "C" Pixmap __real_XCreateBitmapFromData( Display *pDisplay, Drawable hDrawable, const char *pszData, unsigned int nWidth,
                                                unsigned int nHeight );
extern "C" Pixmap __wrap_XCreateBitmapFromData( Display *pDisplay, Drawable hDrawable, const char *pszData, unsigned int nWidth,
                                                unsigned int nHeight )
{
	if ( s_eX11Fault == SIR_X11_FAULT_CREATE_BITMAP ) {
		s_eX11Fault = SIR_X11_FAULT_NONE;
		return 0;
	}
	return __real_XCreateBitmapFromData( pDisplay, hDrawable, pszData, nWidth, nHeight );
}

extern "C" Cursor __real_XCreatePixmapCursor( Display *pDisplay, Pixmap hSource, Pixmap hMask, XColor *pForeground, XColor *pBackground,
                                              unsigned int nHotX, unsigned int nHotY );
extern "C" Cursor __wrap_XCreatePixmapCursor( Display *pDisplay, Pixmap hSource, Pixmap hMask, XColor *pForeground, XColor *pBackground,
                                              unsigned int nHotX, unsigned int nHotY )
{
	if ( s_eX11Fault == SIR_X11_FAULT_CREATE_CURSOR ) {
		s_eX11Fault = SIR_X11_FAULT_NONE;
		return 0;
	}
	return __real_XCreatePixmapCursor( pDisplay, hSource, hMask, pForeground, pBackground, nHotX, nHotY );
}

extern "C" int __real_XGrabPointer( Display *pDisplay, Window hGrabWindow, Bool bOwnerEvents, unsigned int nEventMask, int ePointerMode,
                                    int eKeyboardMode, Window hConfineWindow, Cursor hCursor, Time nTime );
extern "C" int __wrap_XGrabPointer( Display *pDisplay, Window hGrabWindow, Bool bOwnerEvents, unsigned int nEventMask, int ePointerMode,
                                    int eKeyboardMode, Window hConfineWindow, Cursor hCursor, Time nTime )
{
	if ( s_eX11Fault == SIR_X11_FAULT_GRAB_POINTER ) {
		s_eX11Fault = SIR_X11_FAULT_NONE;
		return AlreadyGrabbed;
	}
	return __real_XGrabPointer( pDisplay, hGrabWindow, bOwnerEvents, nEventMask, ePointerMode, eKeyboardMode, hConfineWindow, hCursor,
	                            nTime );
}

extern "C" Bool __real_XFilterEvent( XEvent *pEvent, Window hWindow );
extern "C" Bool __wrap_XFilterEvent( XEvent *pEvent, Window hWindow )
{
	if ( s_eX11Fault == SIR_X11_FAULT_FILTER_EVENT ) {
		s_eX11Fault = SIR_X11_FAULT_NONE;
		return True;
	}
	return __real_XFilterEvent( pEvent, hWindow );
}

extern "C" int __real_XNextEvent( Display *pDisplay, XEvent *pEvent );
extern "C" int __wrap_XNextEvent( Display *pDisplay, XEvent *pEvent )
{
	const int nResult = __real_XNextEvent( pDisplay, pEvent );
	if ( s_eX11Fault == SIR_X11_FAULT_GENERIC_EVENT ) {
		s_eX11Fault = SIR_X11_FAULT_NONE;
		pEvent->type = GenericEvent;
		pEvent->xcookie.extension = -1;
	}
	return nResult;
}

static bool sirOpenWindowWithRetry( SIRInputWindow& window, SIRInputService& input, SIRInputBackend& backend ) noexcept
{
	for ( uint32_t iAttempt = 0; iAttempt < 16; ++iAttempt ) {
		if ( window.open( input, backend ) ) {
			return true;
		}
		( void )usleep( 1000 );
	}
	return false;
}

static Display *sirOpenDisplayWithRetry( void ) noexcept
{
	for ( uint32_t iAttempt = 0; iAttempt < 16; ++iAttempt ) {
		Display *pDisplay = XOpenDisplay( nullptr );
		if ( pDisplay != nullptr ) {
			return pDisplay;
		}
		( void )usleep( 1000 );
	}
	return nullptr;
}

static bool sirTestNativeWindowLifecycle( void ) noexcept
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	SIRInputWindow window;
	if ( window.isOpen() || window.focused() || window.cursorMode() != SIR_INPUT_CURSOR_FREE || window.pump( input ) ||
	     window.setCursorMode( SIR_INPUT_CURSOR_CAPTURED ) ) {
		return false;
	}
	window.setTextEnabled( true );
	s_eX11Fault = SIR_X11_FAULT_CREATE_WINDOW;
	for ( uint32_t iAttempt = 0; iAttempt < 16 && s_eX11Fault != SIR_X11_FAULT_NONE; ++iAttempt ) {
		if ( window.open( input, backend ) ) {
			return false;
		}
		( void )usleep( 1000 );
	}
	if ( s_eX11Fault != SIR_X11_FAULT_NONE ) {
		return false;
	}
	if ( !sirOpenWindowWithRetry( window, input, backend ) || !window.isOpen() || window.open( input, backend ) ) {
		return false;
	}
	SIRInputService other;
	if ( window.pump( other ) || !input.begin( 1 ) || !window.pump( input ) || input.activeDeviceCount() != 2 ) {
		return false;
	}
	s_eX11Fault = SIR_X11_FAULT_CREATE_BITMAP;
	if ( window.setCursorMode( SIR_INPUT_CURSOR_HIDDEN ) || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		return false;
	}
	s_eX11Fault = SIR_X11_FAULT_CREATE_CURSOR;
	if ( window.setCursorMode( SIR_INPUT_CURSOR_HIDDEN ) || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		return false;
	}
	if ( !window.setCursorMode( SIR_INPUT_CURSOR_HIDDEN ) || window.cursorMode() != SIR_INPUT_CURSOR_HIDDEN ||
	     !window.setCursorMode( SIR_INPUT_CURSOR_FREE ) || !window.setCursorMode( SIR_INPUT_CURSOR_CAPTURED ) ||
	     window.setCursorMode( ( sirInputCursorMode_t )3 ) ) {
		return false;
	}
	window.setTextEnabled( true );
	window.setTextEnabled( false );
	input.sample();
	if ( !input.publish() ) {
		return false;
	}
	window.close();
	window.close();
	if ( window.isOpen() || window.setCursorMode( SIR_INPUT_CURSOR_FREE ) || window.pump( input ) ) {
		return false;
	}
	input.shutdown();
	return true;
}

static bool sirTestX11NormalizedEvents( void ) noexcept
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	if ( backend.hapticCapabilities( { 0, 0 } ) != SIR_INPUT_HAPTIC_CAP_NONE || backend.vibrateTriggers( { 0, 0 }, 120, 30 ) ) {
		return false;
	}
	XEvent event = {};
	if ( !input.attach( backend ) || !input.begin( 1 ) || !backend.attachWindow( input, ( Window )123 ) ||
	     backend.attachWindow( input, ( Window )123 ) ) {
		return false;
	}
	event.xany.window = ( Window )123;
	event.type = KeyPress;
	if ( backend.processEvent( input, event ) ) {
		return false;
	}
	event.type = FocusIn;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.type = KeyPress;
	event.xkey.keycode = 40;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.type = KeyRelease;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	for ( uint32_t nButton = 0; nButton < 9; ++nButton ) {
		event.type = ButtonPress;
		event.xbutton.button = nButton;
		if ( !backend.processEvent( input, event ) ) {
			return false;
		}
		event.type = ButtonRelease;
		if ( !backend.processEvent( input, event ) ) {
			return false;
		}
	}
	event.type = MotionNotify;
	event.xmotion.x_root = 10;
	event.xmotion.y_root = 20;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.xmotion.x_root = 15;
	event.xmotion.y_root = -30;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.type = FocusOut;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.type = KeyPress;
	if ( backend.processEvent( input, event ) ) {
		return false;
	}
	event.xany.window = ( Window )124;
	if ( backend.processEvent( input, event ) ) {
		return false;
	}
	backend.resynchronize( input );
	if ( !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *pFrame = input.frame();
	if ( pFrame == nullptr || pFrame->nCount < 10 ) {
		return false;
	}
	input.shutdown();
	return true;
}

static bool sirTestX11BackendFailureAndExtremeMotion( void ) noexcept
{
	SIRInputService missing;
	SIRLinuxX11Backend unowned;
	if ( !missing.begin( 1 ) || unowned.attachWindow( missing, ( Window )777 ) || unowned.attachWindow( missing, ( Window )0 ) ||
	     SIRLinuxX11Backend::enableDetectableRepeat( nullptr ) || unowned.enableRawPointer( nullptr ) ) {
		return false;
	}
	unowned.stop( missing );

	SIRInputService input;
	SIRLinuxX11Backend backend;
	if ( !input.attach( backend ) || !input.begin( 1 ) || backend.attachWindow( input, ( Window )0 ) ||
	     !backend.attachWindow( input, ( Window )456 ) ) {
		return false;
	}
	XEvent event = {};
	event.xany.window = ( Window )456;
	event.type = FocusIn;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.type = MotionNotify;
	event.xmotion.x_root = INT32_MIN;
	event.xmotion.y_root = INT32_MAX;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.xmotion.x_root = INT32_MAX;
	event.xmotion.y_root = INT32_MIN;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.xmotion.x_root = INT32_MIN;
	event.xmotion.y_root = INT32_MAX;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	sirInputDeviceId_t hKeyboard = {};
	if ( !input.findDevice( SIR_INPUT_LINUX_X11, UINT64_C( 456 ) << 1, &hKeyboard ) ||
	     !input.disconnect( SIR_INPUT_LINUX_X11, hKeyboard ) ) {
		return false;
	}
	event.type = FocusOut;
	if ( backend.processEvent( input, event ) ) {
		return false;
	}
	backend.stop( input );
	if ( input.activeDeviceCount() != 0 || !input.publish() || input.frame() == nullptr || input.frame()->nCount < 5 ) {
		return false;
	}
	input.shutdown();
	return true;
}

static Window sirFindNativeInputWindow( Display *pDisplay ) noexcept
{
	Window hRoot = 0;
	Window hParent = 0;
	Window *pChildren = nullptr;
	unsigned int nChildren = 0;
	if ( !XQueryTree( pDisplay, DefaultRootWindow( pDisplay ), &hRoot, &hParent, &pChildren, &nChildren ) ) {
		return 0;
	}
	Window hFound = 0;
	for ( unsigned int i = 0; i < nChildren; ++i ) {
		char *pszName = nullptr;
		if ( XFetchName( pDisplay, pChildren[i], &pszName ) && pszName != nullptr ) {
			if ( strcmp( pszName, "SIREngine native input" ) == 0 ) {
				hFound = pChildren[i];
			}
		}
		if ( pszName != nullptr ) {
			XFree( pszName );
		}
		if ( hFound != 0 ) {
			break;
		}
	}
	if ( pChildren != nullptr ) {
		XFree( pChildren );
	}
	return hFound;
}

static bool sirTestNativeWindowFailurePaths( void ) noexcept
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	SIRLinuxEvdevBackend invalidDesktop;
	SIRInputWindow window;
	if ( window.open( input, invalidDesktop ) || window.isOpen() ) {
		return false;
	}
	const char *pszOriginalDisplay = getenv( "DISPLAY" );
	if ( pszOriginalDisplay == nullptr || strlen( pszOriginalDisplay ) >= 256 ) {
		return false;
	}
	char szDisplay[256] = {};
	( void )snprintf( szDisplay, sizeof( szDisplay ), "%s", pszOriginalDisplay );
	if ( setenv( "DISPLAY", ":65000", 1 ) != 0 ) {
		return false;
	}
	const bool bRejectedMissingDisplay = !window.open( input, backend );
	const bool bRestoredDisplay = setenv( "DISPLAY", szDisplay, 1 ) == 0;
	if ( !bRejectedMissingDisplay || !bRestoredDisplay || window.isOpen() ) {
		return false;
	}
	if ( !input.attach( backend ) || window.open( input, backend ) || window.isOpen() ) {
		return false;
	}
	input.shutdown();

	SIRInputService pumpInput;
	SIRLinuxX11Backend pumpBackend;
	SIRInputWindow pumpWindow;
	if ( !sirOpenWindowWithRetry( pumpWindow, pumpInput, pumpBackend ) || !pumpInput.begin( 1 ) ||
	     !pumpBackend.attachWindow( pumpInput, ( Window )123456 ) || pumpWindow.pump( pumpInput ) ) {
		return false;
	}
	pumpWindow.close();
	pumpInput.shutdown();
	return true;
}

static bool sirPumpUntilClosed( SIRInputWindow& window, SIRInputService& input ) noexcept
{
	for ( uint32_t iAttempt = 0; iAttempt < 32 && window.isOpen(); ++iAttempt ) {
		( void )window.pump( input );
		if ( window.isOpen() ) {
			( void )usleep( 1000 );
		}
	}
	return !window.isOpen();
}

static bool sirTestNativeClientMessages( void ) noexcept
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	SIRInputWindow window;
	if ( !sirOpenWindowWithRetry( window, input, backend ) || !input.begin( 1 ) || !window.pump( input ) ) {
		return false;
	}
	Display *pSender = sirOpenDisplayWithRetry();
	if ( pSender == nullptr ) {
		return false;
	}
	const Window hWindow = sirFindNativeInputWindow( pSender );
	if ( hWindow == 0 ) {
		XCloseDisplay( pSender );
		return false;
	}
	const Atom hProtocols = XInternAtom( pSender, "WM_PROTOCOLS", False );
	const Atom hDelete = XInternAtom( pSender, "WM_DELETE_WINDOW", False );
	const Atom hOther = XInternAtom( pSender, "WM_TAKE_FOCUS", False );
	XEvent event = {};
	event.xclient.type = ClientMessage;
	event.xclient.display = pSender;
	event.xclient.window = hWindow;
	event.xclient.message_type = hProtocols;
	event.xclient.format = 32;
	event.xclient.data.l[0] = ( long )hOther;
	const bool bSentOther = XSendEvent( pSender, hWindow, False, NoEventMask, &event ) != 0;
	XSync( pSender, False );
	const bool bIgnoredOther = bSentOther && window.pump( input ) && window.isOpen();
	if ( !bIgnoredOther ) {
		XCloseDisplay( pSender );
		return false;
	}
	// The close protocol must also match its message type and wire format.
	event.xclient.message_type = hOther;
	event.xclient.data.l[0] = ( long )hDelete;
	const bool bSentWrongType = XSendEvent( pSender, hWindow, False, NoEventMask, &event ) != 0;
	XSync( pSender, False );
	if ( !bSentWrongType || !window.pump( input ) || !window.isOpen() ) {
		XCloseDisplay( pSender );
		return false;
	}
	event.xclient.message_type = hProtocols;
	event.xclient.format = 8;
	const bool bSentWrongFormat = XSendEvent( pSender, hWindow, False, NoEventMask, &event ) != 0;
	XSync( pSender, False );
	if ( !bSentWrongFormat || !window.pump( input ) || !window.isOpen() ) {
		XCloseDisplay( pSender );
		return false;
	}
	event.xclient.format = 32;
	event.xclient.data.l[0] = ( long )hOther;
	s_eX11Fault = SIR_X11_FAULT_GENERIC_EVENT;
	const bool bSentGeneric = XSendEvent( pSender, hWindow, False, NoEventMask, &event ) != 0;
	XSync( pSender, False );
	if ( !bSentGeneric || !window.pump( input ) || !window.isOpen() || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		XCloseDisplay( pSender );
		return false;
	}

	window.setTextEnabled( true );
	XEvent focus = {};
	focus.xfocus.type = FocusIn;
	focus.xfocus.display = pSender;
	focus.xfocus.window = hWindow;
	focus.xfocus.mode = NotifyNormal;
	focus.xfocus.detail = NotifyNonlinear;
	const bool bSentFocus = XSendEvent( pSender, hWindow, False, FocusChangeMask, &focus ) != 0;
	XSync( pSender, False );
	if ( !bSentFocus || !window.pump( input ) || !window.focused() ) {
		XCloseDisplay( pSender );
		return false;
	}
	window.setTextEnabled( false );
	window.setTextEnabled( true );
	s_eX11Fault = SIR_X11_FAULT_GRAB_POINTER;
	if ( window.setCursorMode( SIR_INPUT_CURSOR_CAPTURED ) || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		XCloseDisplay( pSender );
		return false;
	}
	if ( !input.text().snapshot().bEnabled || !window.setCursorMode( SIR_INPUT_CURSOR_CAPTURED ) ||
	     window.cursorMode() != SIR_INPUT_CURSOR_CAPTURED ) {
		XCloseDisplay( pSender );
		return false;
	}
	XEvent key = {};
	key.xkey.type = KeyPress;
	key.xkey.display = pSender;
	key.xkey.window = hWindow;
	key.xkey.root = DefaultRootWindow( pSender );
	key.xkey.keycode = XKeysymToKeycode( pSender, XK_a );
	const bool bSentKey = XSendEvent( pSender, hWindow, False, KeyPressMask, &key ) != 0;
	XSync( pSender, False );
	if ( !bSentKey || !window.pump( input ) ) {
		XCloseDisplay( pSender );
		return false;
	}
	s_eX11Fault = SIR_X11_FAULT_FILTER_EVENT;
	const bool bSentFiltered = XSendEvent( pSender, hWindow, False, KeyPressMask, &key ) != 0;
	XSync( pSender, False );
	if ( !bSentFiltered || !window.pump( input ) || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		XCloseDisplay( pSender );
		return false;
	}
	window.setTextEnabled( false );
	// XSendEvent delivers to the destination window even if the payload
	// refers to an unrelated window. It must not update this input owner.
	key.xkey.window = hWindow + 1024;
	const bool bSentOtherWindow = XSendEvent( pSender, hWindow, False, KeyPressMask, &key ) != 0;
	XSync( pSender, False );
	if ( !bSentOtherWindow || !window.pump( input ) || !window.isOpen() ) {
		XCloseDisplay( pSender );
		return false;
	}
	focus.xfocus.type = FocusOut;
	const bool bSentBlur = XSendEvent( pSender, hWindow, False, FocusChangeMask, &focus ) != 0;
	XSync( pSender, False );
	if ( !bSentBlur || !window.pump( input ) || window.focused() ) {
		XCloseDisplay( pSender );
		return false;
	}
	focus.xfocus.type = FocusIn;
	const bool bRegainedFocus = XSendEvent( pSender, hWindow, False, FocusChangeMask, &focus ) != 0;
	XSync( pSender, False );
	if ( !bRegainedFocus || !window.pump( input ) || !window.focused() || window.cursorMode() != SIR_INPUT_CURSOR_CAPTURED ) {
		XCloseDisplay( pSender );
		return false;
	}

	event.xclient.data.l[0] = ( long )hDelete;
	const bool bSentDelete = XSendEvent( pSender, hWindow, False, NoEventMask, &event ) != 0;
	XSync( pSender, False );
	const bool bClosed = bSentDelete && sirPumpUntilClosed( window, input );
	XCloseDisplay( pSender );
	window.close();
	input.shutdown();
	return bClosed;
}

static bool sirTestNativeDestroyNotification( void ) noexcept
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	SIRInputWindow window;
	if ( !sirOpenWindowWithRetry( window, input, backend ) || !input.begin( 1 ) || !window.pump( input ) ) {
		return false;
	}
	Display *pSender = sirOpenDisplayWithRetry();
	if ( pSender == nullptr ) {
		return false;
	}
	const Window hWindow = sirFindNativeInputWindow( pSender );
	if ( hWindow == 0 ) {
		XCloseDisplay( pSender );
		return false;
	}
	XEvent event = {};
	event.xdestroywindow.type = DestroyNotify;
	event.xdestroywindow.display = pSender;
	event.xdestroywindow.event = hWindow;
	event.xdestroywindow.window = hWindow;
	const bool bSent = XSendEvent( pSender, hWindow, False, StructureNotifyMask, &event ) != 0;
	XSync( pSender, False );
	const bool bClosed = bSent && sirPumpUntilClosed( window, input );
	XCloseDisplay( pSender );
	window.close();
	input.shutdown();
	return bClosed;
}

int main( void )
{
	if ( !sirTestNativeWindowLifecycle() ) {
		fputs( "SIRInputX11WindowTests native lifecycle FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestX11NormalizedEvents() ) {
		fputs( "SIRInputX11WindowTests normalized event path FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestX11BackendFailureAndExtremeMotion() ) {
		fputs( "SIRInputX11WindowTests backend failure and pointer clamp FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestNativeWindowFailurePaths() ) {
		fputs( "SIRInputX11WindowTests native open and attach failures FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestNativeClientMessages() ) {
		fputs( "SIRInputX11WindowTests native client message handling FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestNativeDestroyNotification() ) {
		fputs( "SIRInputX11WindowTests native destroy notification FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputX11WindowTests PASS" );
	return 0;
}
