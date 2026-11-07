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
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdio.h>

typedef struct sirX11SinkCount_s
{
	uint32_t nEvents;
	uint32_t nKeyPress;
} sirX11SinkCount_t;

static void sirCountX11Event( const XEvent& event, void *pContext ) noexcept
{
	sirX11SinkCount_t *pCount = ( sirX11SinkCount_t * )pContext;
	++pCount->nEvents;
	if ( event.type == KeyPress ) {
		++pCount->nKeyPress;
	}
}

int main( void )
{
	SIRWindowDriverX11 driver;
	SIRWindowSystem windows( driver );
	sirX11SinkCount_t counts = {};
	if ( !driver.setEventSink( sirCountX11Event, &counts ) ) {
		return 7;
	}
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		fputs( "X11 driver initialization FAIL\n", stderr );
		return 1;
	}
	const sirWindowCreateDesc_t desc = { "SIR multiwindow test", 640, 480 };
	sirWindowHandle_t first = {}, second = {};
	if ( windows.createWindow( desc, &first ) != SIR_WINDOW_OK || windows.createWindow( desc, &second ) != SIR_WINDOW_OK ) {
		return 2;
	}
	sirWindowNativeLease_t lease = {};
	sirWindowNativeView_t view = {};
	if ( windows.acquireNativeLease( second, &lease, &view ) != SIR_WINDOW_OK || view.ePlatform != SIR_WINDOW_PLATFORM_X11 ||
	     view.nDisplayOrInstance == 0 || view.nNativeWindow == 0 ) {
		return 3;
	}
	Display *pDisplay = ( Display * )view.nDisplayOrInstance;
	XResizeWindow( pDisplay, ( Window )view.nNativeWindow, 320, 240 );
	XSelectInput( pDisplay, ( Window )view.nNativeWindow, KeyPressMask | StructureNotifyMask | FocusChangeMask );
	XEvent key = {};
	key.xkey.type = KeyPress;
	key.xkey.display = pDisplay;
	key.xkey.window = ( Window )view.nNativeWindow;
	key.xkey.root = DefaultRootWindow( pDisplay );
	key.xkey.keycode = 38;
	key.xkey.same_screen = True;
	XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, KeyPressMask, &key );
	XEvent event = {};
	event.xclient.type = ClientMessage;
	event.xclient.window = ( Window )view.nNativeWindow;
	event.xclient.message_type = XInternAtom( pDisplay, "WM_PROTOCOLS", True );
	event.xclient.format = 32;
	event.xclient.data.l[0] = ( long )XInternAtom( pDisplay, "WM_DELETE_WINDOW", True );
	XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, NoEventMask, &event );
	XSync( pDisplay, False );
	for ( uint32_t i = 0; i < 16; ++i ) {
		if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK ) {
			return 4;
		}
	}
	sirWindowSnapshot_t snapshot = {};
	if ( counts.nEvents == 0 || counts.nKeyPress != 1 ) {
		return 8;
	}
	if ( windows.querySnapshot( second, &snapshot ) != SIR_WINDOW_OK || snapshot.nWidth != 320 || snapshot.nHeight != 240 ||
	     !snapshot.bCloseRequested || windows.destroyWindow( second ) != SIR_WINDOW_BUSY ) {
		return 5;
	}
	if ( windows.releaseNativeLease( lease ) != SIR_WINDOW_OK || windows.destroyWindow( second ) != SIR_WINDOW_OK ||
	     windows.destroyWindow( first ) != SIR_WINDOW_OK || windows.shutdown() != SIR_SYSTEM_OK ) {
		return 6;
	}
	puts( "SIRWindowX11DriverTests PASS" );
	return 0;
}
