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
#include <Core/Platform/Window/Linux/SIRWindowDriverX11.h>
#include <Core/Platform/Window/SIRWindowSystem.h>
#include <Core/Platform/Linux/SIRX11InputBridge.h>
#include <X11/Xlib.h>
#include <stdio.h>
static uint32_t s_nCursorUnhideCount = 0;
static uint32_t s_nCursorHideCount = 0;
static Window s_hLastCursorHideWindow = 0;
extern "C" int __real_XDefineCursor( Display *pDisplay, Window hWindow, Cursor hCursor );
extern "C" int __wrap_XDefineCursor( Display *pDisplay, Window hWindow, Cursor hCursor )
{
	++s_nCursorHideCount;
	s_hLastCursorHideWindow = hWindow;
	return __real_XDefineCursor( pDisplay, hWindow, hCursor );
}
extern "C" int __real_XUndefineCursor( Display *pDisplay, Window hWindow );
extern "C" int __wrap_XUndefineCursor( Display *pDisplay, Window hWindow )
{
	++s_nCursorUnhideCount;
	return __real_XUndefineCursor( pDisplay, hWindow );
}

static void sirBridgeDispatchEvent( const XEvent& event, void *pContext ) noexcept
{
	( ( SIRX11InputBridge * )pContext )->processEvent( event );
}

int main( void )
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	SIRX11InputBridge bridge;
	SIRWindowDriverX11 driver;
	SIRWindowSystem windows( driver );
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		return 1;
	}
	const sirWindowCreateDesc_t desc = { "Input bridge", 600, 400 };
	sirWindowHandle_t first = {}, second = {};
	if ( windows.createWindow( desc, &first ) != SIR_WINDOW_OK || windows.createWindow( desc, &second ) != SIR_WINDOW_OK ) {
		return 2;
	}
	sirWindowNativeLease_t lease = {}, secondLease = {};
	sirWindowNativeView_t view = {}, secondView = {};
	if ( windows.acquireNativeLease( first, &lease, &view ) != SIR_WINDOW_OK ||
	     windows.acquireNativeLease( second, &secondLease, &secondView ) != SIR_WINDOW_OK || !bridge.bind( input, backend, view ) ||
	     bridge.bind( input, backend, view ) || !bridge.registerWindow( secondView ) || bridge.registerWindow( secondView ) ||
	     !driver.setEventSink( sirBridgeDispatchEvent, &bridge ) ) {
		return 3;
	}
	sirWindowNativeView_t foreign = secondView;
	++foreign.nDisplayOrInstance;
	if ( bridge.registerWindow( foreign ) ) {
		return 12;
	}
	foreign = secondView;
	foreign.ePlatform = SIR_WINDOW_PLATFORM_WIN32;
	if ( bridge.registerWindow( foreign ) ) {
		return 13;
	}
	bridge.setTextEnabled( true );
	if ( !input.text().snapshot().bEnabled || !input.begin( 1 ) || !bridge.beginFrame() ) {
		return 4;
	}
	Display *pDisplay = ( Display * )view.nDisplayOrInstance;
	XEvent focus = {};
	focus.xfocus.type = FocusIn;
	focus.xfocus.window = ( Window )view.nNativeWindow;
	focus.xfocus.mode = NotifyNormal;
	focus.xfocus.detail = NotifyNonlinear;
	XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, FocusChangeMask, &focus );
	XSync( pDisplay, False );
	for ( uint32_t i = 0; i < 8; ++i ) {
		if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK ) {
			return 5;
		}
	}
	if ( input.activeDeviceCount() != 2 ) {
		return 6;
	}
	// Route the one physical keyboard and pointer seat between two windows.
	XEvent key = {};
	key.xkey.type = KeyPress;
	key.xkey.window = ( Window )view.nNativeWindow;
	key.xkey.display = pDisplay;
	key.xkey.keycode = 40;
	bridge.processEvent( key );
	XEvent secondFocus = focus;
	secondFocus.xfocus.window = ( Window )secondView.nNativeWindow;
	// Cursor capture follows the physical seat, never sticks to a former
	// focused native window when its FocusOut event is missing.
	if ( !bridge.setCursorMode( SIR_INPUT_CURSOR_HIDDEN ) ) {
		return 14;
	}
	const uint32_t nUnhiddenBeforeFocus = s_nCursorUnhideCount;
	const uint32_t nHiddenBeforeFocus = s_nCursorHideCount;
	bridge.processEvent( secondFocus );
	if ( s_nCursorUnhideCount <= nUnhiddenBeforeFocus || s_nCursorHideCount <= nHiddenBeforeFocus ||
	     s_hLastCursorHideWindow != ( Window )secondView.nNativeWindow || bridge.cursorMode() != SIR_INPUT_CURSOR_HIDDEN ||
	     input.activeDeviceCount() != 2 ) {
		return 9;
	}
	key.xkey.window = ( Window )secondView.nNativeWindow;
	key.xkey.keycode = 41;
	bridge.processEvent( key );
	// Keyboard, pointer buttons, and wheel events must use the same
	// physical devices as the focus changes between native windows.
	XEvent pointer = {};
	pointer.xbutton.type = ButtonPress;
	pointer.xbutton.window = ( Window )secondView.nNativeWindow;
	pointer.xbutton.button = Button1;
	bridge.processEvent( pointer );
	pointer.xbutton.type = ButtonRelease;
	bridge.processEvent( pointer );
	pointer.xbutton.type = ButtonPress;
	pointer.xbutton.button = Button4;
	bridge.processEvent( pointer );
	// Remove the *focused* second window, then verify the backend retargets
	// the surviving first window without reconnecting physical devices.
	if ( !bridge.unregisterWindow( secondView.nNativeWindow ) || bridge.unregisterWindow( secondView.nNativeWindow ) ||
	     input.activeDeviceCount() != 2 ) {
		return 10;
	}
	bridge.processEvent( focus );
	if ( !bridge.setCursorMode( SIR_INPUT_CURSOR_FREE ) ) {
		return 15;
	}
	key.xkey.window = ( Window )view.nNativeWindow;
	key.xkey.keycode = 43;
	bridge.processEvent( key );
	pointer.xbutton.window = ( Window )view.nNativeWindow;
	pointer.xbutton.button = Button3;
	bridge.processEvent( pointer );
	pointer.xbutton.window = ( Window )( secondView.nNativeWindow );
	pointer.xbutton.button = Button1;
	bridge.processEvent( pointer ); // Retired window must not receive input.
	input.sample();
	if ( !input.publish() ) {
		return 7;
	}
	const sirInputFrame_t *pFrame = input.frame();
	bool bSawSecondKey = false;
	bool bSawSurvivingKey = false;
	bool bSawSecondButton = false;
	bool bSawSurvivingButton = false;
	bool bSawWheel = false;
	uint32_t nResyncEvents = 0;
	for ( uint32_t i = 0; i < pFrame->nCount; ++i ) {
		if ( pFrame->events[i].eSignal == SIR_INPUT_RESYNC ) {
			++nResyncEvents;
		}
		if ( pFrame->events[i].eSignal == SIR_INPUT_BUTTON && pFrame->events[i].nControl == 0 && pFrame->events[i].nValue == 1 ) {
			bSawSecondButton = true;
		}
		if ( pFrame->events[i].eSignal == SIR_INPUT_BUTTON && pFrame->events[i].nControl == 1 && pFrame->events[i].nValue == 1 ) {
			bSawSurvivingButton = true;
		}
		if ( pFrame->events[i].eSignal == SIR_INPUT_WHEEL && pFrame->events[i].nValue == 120 ) {
			bSawWheel = true;
		}
		if ( pFrame->events[i].eSignal == SIR_INPUT_BUTTON && pFrame->events[i].nControl == 41 && pFrame->events[i].nValue == 1 ) {
			bSawSecondKey = true;
		}
		if ( pFrame->events[i].eSignal == SIR_INPUT_BUTTON && pFrame->events[i].nControl == 43 && pFrame->events[i].nValue == 1 ) {
			bSawSurvivingKey = true;
		}
	}
	if ( !bSawSecondKey || !bSawSurvivingKey || !bSawSecondButton || !bSawSurvivingButton || !bSawWheel || nResyncEvents < 4 ||
	     input.activeDeviceCount() != 2 ) {
		return 11;
	}
	( void )driver.setEventSink( nullptr, nullptr );
	bridge.unbind();
	if ( bridge.isBound() || windows.releaseNativeLease( lease ) != SIR_WINDOW_OK ||
	     windows.releaseNativeLease( secondLease ) != SIR_WINDOW_OK || windows.destroyWindow( second ) != SIR_WINDOW_OK ||
	     windows.destroyWindow( first ) != SIR_WINDOW_OK || windows.shutdown() != SIR_SYSTEM_OK ) {
		return 8;
	}
	input.shutdown();
	puts( "SIRX11InputBridgeTests PASS" );
	return 0;
}
