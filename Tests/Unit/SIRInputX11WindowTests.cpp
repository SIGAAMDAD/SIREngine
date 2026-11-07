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
#include <Core/Platform/Window/SIRWindowSystem.h>
#include <Core/Platform/Window/Linux/SIRWindowDriverX11.h>
#include <Core/Platform/Input/Linux/SIRX11InputBridge.h>
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

static void sirPortedX11InputEvent( const XEvent& event, void *pContext ) noexcept
{
	( ( SIRX11InputBridge * )pContext )->processEvent( event );
}

static bool sirTestPortedNativeWindowAndInput( void ) noexcept
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	SIRX11InputBridge bridge;
	SIRWindowDriverX11 driver;
	SIRWindowSystem windows( driver );
	if ( bridge.isBound() || bridge.setCursorMode( SIR_INPUT_CURSOR_CAPTURED ) || windows.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}

	const sirWindowCreateDesc_t desc = { "SIREngine native input", 800, 450 };
	sirWindowHandle_t hWindow = {};
	s_eX11Fault = SIR_X11_FAULT_CREATE_WINDOW;
	if ( windows.createWindow( desc, &hWindow ) != SIR_WINDOW_NATIVE_FAILURE || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		return false;
	}
	if ( windows.createWindow( desc, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowNativeLease_t lease = {};
	sirWindowNativeView_t view = {};
	if ( windows.acquireNativeLease( hWindow, &lease, &view ) != SIR_WINDOW_OK || !bridge.bind( input, backend, view ) ||
	     bridge.bind( input, backend, view ) || !driver.setEventSink( sirPortedX11InputEvent, &bridge ) ) {
		return false;
	}
	bridge.setTextEnabled( true );
	if ( !input.text().snapshot().bEnabled || !input.begin( 1 ) || !bridge.beginFrame() || !bridge.beginFrame() ) {
		return false;
	}
	Display *pDisplay = ( Display * )view.nDisplayOrInstance;
	XEvent focus = {};
	focus.xfocus.type = FocusIn;
	focus.xfocus.window = ( Window )view.nNativeWindow;
	focus.xfocus.mode = NotifyNormal;
	focus.xfocus.detail = NotifyNonlinear;
	if ( !XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, FocusChangeMask, &focus ) ) {
		return false;
	}
	XSync( pDisplay, False );
	for ( uint32_t i = 0; i < 8; ++i ) {
		if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK ) {
			return false;
		}
	}
	if ( input.activeDeviceCount() != 2 || !input.text().snapshot().bFocused ) {
		return false;
	}
	// The original X11 IME filter and GenericEvent negative paths now reach the borrowed input sink.
	XEvent key = {};
	key.xkey.type = KeyPress;
	key.xkey.display = pDisplay;
	key.xkey.window = ( Window )view.nNativeWindow;
	key.xkey.root = DefaultRootWindow( pDisplay );
	key.xkey.keycode = 38;
	key.xkey.same_screen = True;
	s_eX11Fault = SIR_X11_FAULT_FILTER_EVENT;
	( void )XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, KeyPressMask, &key );
	XSync( pDisplay, False );
	if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		return false;
	}
	s_eX11Fault = SIR_X11_FAULT_GENERIC_EVENT;
	( void )XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, KeyPressMask, &key );
	XSync( pDisplay, False );
	if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		return false;
	}
	s_eX11Fault = SIR_X11_FAULT_CREATE_BITMAP;
	if ( bridge.setCursorMode( SIR_INPUT_CURSOR_HIDDEN ) || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		return false;
	}
	s_eX11Fault = SIR_X11_FAULT_CREATE_CURSOR;
	if ( bridge.setCursorMode( SIR_INPUT_CURSOR_HIDDEN ) || s_eX11Fault != SIR_X11_FAULT_NONE ) {
		return false;
	}
	if ( !bridge.setCursorMode( SIR_INPUT_CURSOR_HIDDEN ) || bridge.cursorMode() != SIR_INPUT_CURSOR_HIDDEN ||
	     !bridge.setCursorMode( SIR_INPUT_CURSOR_FREE ) || !bridge.setCursorMode( SIR_INPUT_CURSOR_CAPTURED ) ||
	     bridge.setCursorMode( ( sirInputCursorMode_t )3 ) ) {
		return false;
	}
	bridge.setTextEnabled( false );
	bridge.setTextEnabled( true );
	input.sample();
	if ( !input.publish() ) {
		return false;
	}
	const Atom hProtocols = XInternAtom( pDisplay, "WM_PROTOCOLS", True );
	const Atom hDelete = XInternAtom( pDisplay, "WM_DELETE_WINDOW", True );
	XEvent ignored = {};
	ignored.xclient.type = ClientMessage;
	ignored.xclient.window = ( Window )view.nNativeWindow;
	ignored.xclient.message_type = hProtocols;
	ignored.xclient.format = 32;
	ignored.xclient.data.l[0] = ( long )hDelete + 1;
	( void )XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, NoEventMask, &ignored );
	XSync( pDisplay, False );
	if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowSnapshot_t snapshot = {};
	if ( windows.querySnapshot( hWindow, &snapshot ) != SIR_WINDOW_OK || snapshot.bCloseRequested ) {
		return false;
	}
	ignored.xclient.data.l[0] = ( long )hDelete;
	( void )XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, NoEventMask, &ignored );
	XSync( pDisplay, False );
	if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK || windows.querySnapshot( hWindow, &snapshot ) != SIR_WINDOW_OK ||
	     !snapshot.bCloseRequested || windows.destroyWindow( hWindow ) != SIR_WINDOW_BUSY ) {
		return false;
	}
	( void )driver.setEventSink( nullptr, nullptr );
	bridge.unbind();
	bridge.unbind();
	if ( bridge.isBound() || bridge.setCursorMode( SIR_INPUT_CURSOR_FREE ) || windows.releaseNativeLease( lease ) != SIR_WINDOW_OK ||
	     windows.destroyWindow( hWindow ) != SIR_WINDOW_OK || windows.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	input.shutdown();
	return true;
}

static bool sirTestWindowDisplayUnavailable( void ) noexcept
{
	const char *pszDisplay = getenv( "DISPLAY" );
	if ( pszDisplay == nullptr || strlen( pszDisplay ) >= 256 ) {
		return false;
	}
	char szDisplay[256] = {};
	( void )snprintf( szDisplay, sizeof( szDisplay ), "%s", pszDisplay );
	if ( setenv( "DISPLAY", ":65000", 1 ) != 0 ) {
		return false;
	}
	SIRWindowDriverX11 driver;
	SIRWindowSystem windows( driver );
	const bool bRejected = windows.initialize() != SIR_SYSTEM_OK;
	const bool bRestored = setenv( "DISPLAY", szDisplay, 1 ) == 0;
	return bRejected && bRestored;
}

int main( void )
{
	if ( !sirTestX11NormalizedEvents() ) {
		fputs( "FAIL normalized\n", stderr );
		return 1;
	}
	if ( !sirTestX11BackendFailureAndExtremeMotion() ) {
		fputs( "FAIL backend\n", stderr );
		return 1;
	}
	if ( !sirTestPortedNativeWindowAndInput() ) {
		fputs( "FAIL native\n", stderr );
		return 1;
	}
	if ( !sirTestWindowDisplayUnavailable() ) {
		fputs( "FAIL display\n", stderr );
		return 1;
	}
	puts( "SIRInputX11WindowTests PASS" );
	return 0;
}
