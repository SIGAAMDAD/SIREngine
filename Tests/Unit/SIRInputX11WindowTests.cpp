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
#include <limits.h>
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
	puts( "SIRInputX11WindowTests PASS" );
	return 0;
}
