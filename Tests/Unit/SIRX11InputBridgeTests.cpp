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
#include <Core/Platform/Input/Linux/SIRX11InputBridge.h>
#include <X11/Xlib.h>
#include <stdio.h>

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
	sirWindowNativeLease_t lease = {};
	sirWindowNativeView_t view = {};
	if ( windows.acquireNativeLease( first, &lease, &view ) != SIR_WINDOW_OK || !bridge.bind( input, backend, view ) ||
	     bridge.bind( input, backend, view ) || !driver.setEventSink( sirBridgeDispatchEvent, &bridge ) ) {
		return 3;
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
	input.sample();
	if ( !input.publish() ) {
		return 7;
	}
	( void )driver.setEventSink( nullptr, nullptr );
	bridge.unbind();
	if ( bridge.isBound() || windows.releaseNativeLease( lease ) != SIR_WINDOW_OK || windows.destroyWindow( second ) != SIR_WINDOW_OK ||
	     windows.destroyWindow( first ) != SIR_WINDOW_OK || windows.shutdown() != SIR_SYSTEM_OK ) {
		return 8;
	}
	input.shutdown();
	puts( "SIRX11InputBridgeTests PASS" );
	return 0;
}
