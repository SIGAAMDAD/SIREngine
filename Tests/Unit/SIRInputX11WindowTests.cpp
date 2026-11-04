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
#include <Core/Platform/Input/SIRInputWindow.h>
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>

#include <X11/Xlib.h>
#include <stdio.h>

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
	if ( !window.open( input, backend ) || !window.isOpen() || window.open( input, backend ) ) {
		return false;
	}
	SIRInputService other;
	if ( window.pump( other ) || !input.begin( 1 ) || !window.pump( input ) || input.activeDeviceCount() != 2 ) {
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
	puts( "SIRInputX11WindowTests PASS" );
	return 0;
}
