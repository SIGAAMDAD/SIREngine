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
#include <string.h>

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
	const sirWindowMutation_t title = { SIR_WINDOW_MUTATION_TITLE, "Renamed window", 0, 0 };
	const sirWindowMutation_t resize = { SIR_WINDOW_MUTATION_RESIZE, nullptr, 320, 240 };
	const sirWindowMutation_t minimize = { SIR_WINDOW_MUTATION_MINIMIZE, nullptr, 0, 0 };
	const sirWindowMutation_t maximize = { SIR_WINDOW_MUTATION_MAXIMIZE, nullptr, 0, 0 };
	const sirWindowResult_t eMinimize = windows.mutateWindow( second, minimize );
	if ( windows.mutateWindow( second, title ) != SIR_WINDOW_OK || windows.mutateWindow( second, resize ) != SIR_WINDOW_OK ||
	     windows.mutateWindow( second, maximize ) != SIR_WINDOW_OK ||
	     ( eMinimize != SIR_WINDOW_OK && eMinimize != SIR_WINDOW_UNSUPPORTED ) ) {
		return 9;
	}
	Display *pDisplay = ( Display * )view.nDisplayOrInstance;
	// The UTF-8 EWMH title must follow title mutations, not only creation.
	const Atom hName = XInternAtom( pDisplay, "_NET_WM_NAME", True );
	const Atom hUtf8 = XInternAtom( pDisplay, "UTF8_STRING", True );
	Atom hType = None;
	int nFormat = 0;
	unsigned long nItems = 0, nRemaining = 0;
	unsigned char *pTitle = nullptr;
	if ( hName == None || hUtf8 == None ||
	     XGetWindowProperty( pDisplay, ( Window )view.nNativeWindow, hName, 0, 128, False, hUtf8, &hType, &nFormat, &nItems,
	                         &nRemaining, &pTitle ) != Success ||
	     pTitle == nullptr || hType != hUtf8 || nFormat != 8 || nItems != sizeof( "Renamed window" ) - 1 ||
	     memcmp( pTitle, "Renamed window", nItems ) != 0 ) {
		if ( pTitle != nullptr ) {
			XFree( pTitle );
		}
		return 14;
	}
	XFree( pTitle );
	// The platform owner must publish native mapping state to all consumers.
	XUnmapWindow( pDisplay, ( Window )view.nNativeWindow );
	XSync( pDisplay, False );
	for ( uint32_t i = 0; i < 8; ++i ) {
		if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK ) {
			return 10;
		}
	}
	sirWindowSnapshot_t hidden = {};
	if ( windows.querySnapshot( second, &hidden ) != SIR_WINDOW_OK || hidden.bVisible || hidden.bReady ) {
		return 11;
	}
	XMapWindow( pDisplay, ( Window )view.nNativeWindow );
	XSync( pDisplay, False );
	for ( uint32_t i = 0; i < 8; ++i ) {
		if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK ) {
			return 12;
		}
	}
	if ( windows.querySnapshot( second, &hidden ) != SIR_WINDOW_OK || !hidden.bVisible || !hidden.bReady ) {
		return 13;
	}
	// Root display geometry changes invalidate presentation state for all
	// existing windows without mutating their native client sizes.
	sirWindowSnapshot_t beforeTopology = {}, afterTopology = {};
	if ( windows.querySnapshot( second, &beforeTopology ) != SIR_WINDOW_OK ) {
		return 19;
	}
	XWindowAttributes root = {};
	if ( !XGetWindowAttributes( pDisplay, DefaultRootWindow( pDisplay ), &root ) ) {
		return 20;
	}
	XEvent topology = {};
	topology.xconfigure.type = ConfigureNotify;
	topology.xconfigure.display = pDisplay;
	topology.xconfigure.window = DefaultRootWindow( pDisplay );
	topology.xconfigure.width = root.width + 1;
	topology.xconfigure.height = root.height;
	if ( !XSendEvent( pDisplay, DefaultRootWindow( pDisplay ), False, StructureNotifyMask, &topology ) ) {
		return 21;
	}
	XSync( pDisplay, False );
	for ( uint32_t i = 0; i < 4; ++i ) {
		if ( windows.pumpEvents( 32 ) != SIR_WINDOW_OK ) {
			return 22;
		}
	}
	if ( windows.querySnapshot( second, &afterTopology ) != SIR_WINDOW_OK ||
	     afterTopology.nMetricsRevision <= beforeTopology.nMetricsRevision ) {
		return 23;
	}
	// A focused window must publish per-window focus without creating a new device.
	XEvent focused = {};
	focused.xfocus.type = FocusIn;
	focused.xfocus.display = pDisplay;
	focused.xfocus.window = ( Window )view.nNativeWindow;
	focused.xfocus.mode = NotifyNormal;
	focused.xfocus.detail = NotifyNonlinear;
	XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, FocusChangeMask, &focused );
	XSync( pDisplay, False );
	for ( uint32_t i = 0; i < 4; ++i ) {
		if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK ) {
			return 15;
		}
	}
	sirWindowSnapshot_t focusSnapshot = {};
	if ( windows.querySnapshot( second, &focusSnapshot ) != SIR_WINDOW_OK || !focusSnapshot.bFocused ) {
		return 16;
	}
	focused.xfocus.type = FocusOut;
	XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, FocusChangeMask, &focused );
	XSync( pDisplay, False );
	for ( uint32_t i = 0; i < 4; ++i ) {
		if ( windows.pumpEvents( 128 ) != SIR_WINDOW_OK ) {
			return 17;
		}
	}
	if ( windows.querySnapshot( second, &focusSnapshot ) != SIR_WINDOW_OK || focusSnapshot.bFocused ) {
		return 18;
	}
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
