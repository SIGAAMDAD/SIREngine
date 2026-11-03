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

#include <Core/Platform/SIRInput.h>
#include <Core/Platform/SIRInputControllerProfiles.h>
#if defined( __linux__ )
#include <Core/Platform/Linux/SIRLinuxEvdevInput.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdio.h>

#if defined( SIR_INPUT_X11_AVAILABLE )
#include <Core/Platform/Linux/SIRLinuxX11Input.h>
#endif

static bool testLifecycle( void )
{
	SIRInputSystem input;
	sirInputDeviceHandle_t hFirst = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 123, &hFirst ) || input.getActiveDeviceCount() != 1 ) {
		return false;
	}
	sirInputDeviceHandle_t hEnumerated = {};
	if ( input.enumerateDevices( nullptr, 0 ) != 1 || input.enumerateDevices( &hEnumerated, 1 ) != 1 || hEnumerated.nSlot != hFirst.nSlot ||
	     hEnumerated.nGeneration != hFirst.nGeneration ) {
		return false;
	}
	sirInputDeviceHandle_t hDuplicate = {};
	if ( input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 123, &hDuplicate ) ||
	     !input.findDevice( SIR_INPUT_DEVICE_KEYBOARD, 123, &hDuplicate ) || hFirst.nSlot != hDuplicate.nSlot ) {
		return false;
	}
	if ( !input.removeDevice( hFirst ) || input.getActiveDeviceCount() != 0 || input.removeDevice( hFirst ) ) {
		return false;
	}
	sirInputDeviceState_t state = {};
	if ( input.getState( hFirst, &state ) ) {
		return false;
	}
	sirInputDeviceHandle_t hSecond = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 123, &hSecond ) || hFirst.nSlot != hSecond.nSlot ||
	     hFirst.nGeneration == hSecond.nGeneration ) {
		return false;
	}
	input.shutdown();
	return input.getActiveDeviceCount() == 0 && !input.getState( hSecond, &state );
}

static bool testTransitions( void )
{
	SIRInputSystem input;
	sirInputDeviceHandle_t hKeyboard = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 9, &hKeyboard ) ) {
		return false;
	}
	input.beginTick();
	if ( !input.pushButton( hKeyboard, 42, true, 100 ) || !input.pushButton( hKeyboard, 42, false, 101 ) ) {
		return false;
	}
	sirInputDeviceState_t state = {};
	if ( !input.getState( hKeyboard, &state ) ) {
		return false;
	}
	const uint32_t nMask = UINT32_C( 1 ) << ( 42 % 32 );
	if ( ( state.nPressed[42 / 32] & nMask ) == 0 || ( state.nReleased[42 / 32] & nMask ) == 0 ||
	     ( state.nButtons[42 / 32] & nMask ) != 0 ) {
		return false;
	}
	sirInputEvent_t event = {};
	if ( !input.pollEvent( &event ) || event.eKind != SIR_INPUT_EVENT_CONNECTED || !input.pollEvent( &event ) ||
	     event.eKind != SIR_INPUT_EVENT_BUTTON || event.nValue != 1 || event.nTimestampNs != 100 || !input.pollEvent( &event ) ||
	     event.eKind != SIR_INPUT_EVENT_BUTTON || event.nValue != 0 || event.nTimestampNs != 101 || input.pollEvent( &event ) ) {
		return false;
	}
	input.beginTick();
	return input.getState( hKeyboard, &state ) && state.nPressed[42 / 32] == 0 && state.nReleased[42 / 32] == 0;
}

static bool testPointerAndAnalog( void )
{
	SIRInputSystem input;
	sirInputDeviceHandle_t hMouse = {};
	sirInputDeviceHandle_t hPad = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_MOUSE, 1, &hMouse ) || !input.addDevice( SIR_INPUT_DEVICE_GAMEPAD, 2, &hPad ) ) {
		return false;
	}
	if ( !input.pushPointer( hMouse, 8, -6, 12 ) || !input.pushPointer( hMouse, 2, 4, 13 ) || !input.pushWheel( hMouse, 0, 120, 14 ) ||
	     !input.pushWheel( hMouse, 0, 120, 15 ) || !input.pushAxis( hPad, 2, -1234, 16 ) ||
	     input.pushAxis( hPad, SIR_INPUT_MAX_AXES, 1, 17 ) ) {
		return false;
	}
	sirInputDeviceState_t mouse = {};
	sirInputDeviceState_t pad = {};
	if ( !input.getState( hMouse, &mouse ) || !input.getState( hPad, &pad ) || mouse.nMouseDeltaX != 10 || mouse.nMouseDeltaY != -2 ||
	     pad.nAxes[2] != -1234 ) {
		return false;
	}
	uint32_t nWheelEvents = 0;
	sirInputEvent_t event = {};
	while ( input.pollEvent( &event ) ) {
		if ( event.eKind == SIR_INPUT_EVENT_WHEEL ) {
			++nWheelEvents;
		}
	}
	return nWheelEvents == 2;
}

static bool testQueuePressure( void )
{
	SIRInputSystem input;
	sirInputDeviceHandle_t hKeyboard = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 9, &hKeyboard ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_EVENTS - 1; ++i ) {
		if ( !input.pushButton( hKeyboard, 1, ( i & 1 ) == 0, 1 + i ) ) {
			return false;
		}
	}
	if ( input.pushButton( hKeyboard, 1, false, 100000 ) || !input.isOverflowed() || !input.resynchronizeDevice( hKeyboard ) ||
	     input.isOverflowed() ) {
		return false;
	}
	sirInputEvent_t event = {};
	if ( !input.pollEvent( &event ) || event.eKind != SIR_INPUT_EVENT_RESYNC || input.pollEvent( &event ) ) {
		return false;
	}
	return input.pushButton( hKeyboard, 1, true, 2 );
}

static bool testGamepadSamples( void )
{
	SIRInputSystem input;
	sirInputDeviceHandle_t hPad = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_GAMEPAD, 100, &hPad ) ) {
		return false;
	}
	sirInputEvent_t event = {};
	input.pollEvent( &event ); // Connected.
	sirInputGamepadSample_t sample = {};
	sample.nButtons = ( UINT32_C( 1 ) << SIR_GAMEPAD_BUTTON_SOUTH ) | ( UINT32_C( 1 ) << SIR_GAMEPAD_BUTTON_DPAD_LEFT );
	sample.nAxes[SIR_GAMEPAD_AXIS_LEFT_X] = -32767;
	sample.nAxes[SIR_GAMEPAD_AXIS_RIGHT_TRIGGER] = 32767;
	if ( !input.submitGamepadSample( hPad, sample, 101 ) ) {
		return false;
	}
	sirInputDeviceState_t state = {};
	if ( !input.getState( hPad, &state ) || ( state.nButtons[0] & sample.nButtons ) != sample.nButtons ||
	     state.nAxes[SIR_GAMEPAD_AXIS_LEFT_X] != -32767 || state.nAxes[SIR_GAMEPAD_AXIS_RIGHT_TRIGGER] != 32767 ) {
		return false;
	}
	uint32_t nTransitions = 0;
	while ( input.pollEvent( &event ) ) {
		if ( event.nTimestampNs != 101 ) {
			return false;
		}
		++nTransitions;
	}
	if ( nTransitions != 4 || !input.submitGamepadSample( hPad, sample, 102 ) || input.pollEvent( &event ) ) {
		return false;
	}
	sirInputGamepadSample_t invalid = sample;
	invalid.nAxes[SIR_GAMEPAD_AXIS_LEFT_X] = -32768;
	if ( input.submitGamepadSample( hPad, invalid, 103 ) ) {
		return false;
	}
	invalid = sample;
	invalid.nButtons = UINT32_C( 1 ) << SIR_GAMEPAD_BUTTON_COUNT;
	if ( input.submitGamepadSample( hPad, invalid, 104 ) ) {
		return false;
	}
	input.beginTick();
	sample.nButtons = 0;
	sample.nAxes[SIR_GAMEPAD_AXIS_RIGHT_TRIGGER] = 0;
	if ( !input.submitGamepadSample( hPad, sample, 105 ) || !input.getState( hPad, &state ) || state.nReleased[0] == 0 ||
	     state.nAxes[SIR_GAMEPAD_AXIS_RIGHT_TRIGGER] != 0 ) {
		return false;
	}
	return true;
}

static bool testAtomicSampleOverflow( void )
{
	SIRInputSystem input;
	sirInputDeviceHandle_t hPad = {};
	sirInputDeviceHandle_t hMouse = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_GAMEPAD, 1, &hPad ) || !input.addDevice( SIR_INPUT_DEVICE_MOUSE, 2, &hMouse ) ) {
		return false;
	}
	// Leave exactly two available event slots, then submit a three-change sample.
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_EVENTS - 4; ++i ) {
		if ( !input.pushPointer( hMouse, 1, 0, i + 1 ) ) {
			return false;
		}
	}
	sirInputGamepadSample_t sample = {};
	sample.nButtons = UINT32_C( 1 ) << SIR_GAMEPAD_BUTTON_NORTH;
	sample.nAxes[SIR_GAMEPAD_AXIS_LEFT_X] = 100;
	sample.nAxes[SIR_GAMEPAD_AXIS_RIGHT_X] = 200;
	if ( input.submitGamepadSample( hPad, sample, 50 ) || !input.isOverflowed() ) {
		return false;
	}
	sirInputDeviceState_t state = {};
	if ( !input.getState( hPad, &state ) || state.nButtons[0] != 0 || state.nAxes[SIR_GAMEPAD_AXIS_LEFT_X] != 0 ||
	     state.nAxes[SIR_GAMEPAD_AXIS_RIGHT_X] != 0 ) {
		return false;
	}
	// A global resynchronization invalidates the old saturated stream.
	if ( !input.resynchronizeDevice( hPad ) || input.isOverflowed() ) {
		return false;
	}
	uint32_t nResync = 0;
	sirInputEvent_t event = {};
	while ( input.pollEvent( &event ) ) {
		if ( event.eKind != SIR_INPUT_EVENT_RESYNC ) {
			return false;
		}
		++nResync;
	}
	return nResync == 2 && input.submitGamepadSample( hPad, sample, 60 );
}

static bool testNativeGamepadProfiles( void )
{
	SIRInputControllerProfiles profiles;
	sirInputControllerProfile_t profile = {};
	profile.nVendor = 0x054c;
	profile.nProduct = 0x09cc;
	profile.nBus = 0;
	profile.nButtonCount = 2;
	profile.buttons[0] = { 304, SIR_GAMEPAD_BUTTON_EAST };
	profile.buttons[1] = { 305, SIR_GAMEPAD_BUTTON_SOUTH };
	profile.nAxisCount = SIR_GAMEPAD_AXIS_COUNT;
	for ( uint16_t i = 0; i < SIR_GAMEPAD_AXIS_COUNT; ++i ) {
		profile.axes[i] = { ( uint16_t )( i + 12 ), i, i == SIR_GAMEPAD_AXIS_LEFT_Y };
	}
	if ( !profiles.addProfile( profile ) || profiles.addProfile( profile ) || profiles.getCount() != 1 ) {
		return false;
	}
	const sirInputControllerProfile_t *pGeneric = profiles.findProfile( 3, 0x054c, 0x09cc );
	if ( pGeneric == nullptr || SIRInputControllerProfiles::findButton( *pGeneric, 304 ) != SIR_GAMEPAD_BUTTON_EAST ||
	     SIRInputControllerProfiles::findAxis( *pGeneric, 15 ) != 3 || SIRInputControllerProfiles::findButton( *pGeneric, 999 ) != -1 ||
	     profiles.findProfile( 3, 1, 1 ) != nullptr ) {
		return false;
	}
	profile.nBus = 3;
	profile.buttons[0].nLogicalCode = SIR_GAMEPAD_BUTTON_WEST;
	if ( !profiles.addProfile( profile ) || profiles.findProfile( 3, 0x054c, 0x09cc ) == pGeneric ||
	     profiles.findProfile( 5, 0x054c, 0x09cc ) != pGeneric ) {
		return false;
	}
	sirInputControllerProfile_t invalid = profile;
	invalid.nBus = 5;
	invalid.axes[1].nNativeCode = invalid.axes[0].nNativeCode;
	if ( profiles.addProfile( invalid ) ) {
		return false;
	}
	invalid = profile;
	invalid.nBus = 5;
	invalid.buttons[1].nLogicalCode = invalid.buttons[0].nLogicalCode;
	if ( profiles.addProfile( invalid ) ) {
		return false;
	}
	invalid = profile;
	invalid.nBus = 5;
	invalid.axes[0].nLogicalCode = 99;
	if ( profiles.addProfile( invalid ) ) {
		return false;
	}
	invalid = profile;
	invalid.nBus = 5;
	invalid.axes[SIR_GAMEPAD_AXIS_LEFT_TRIGGER].bInvert = true;
	if ( profiles.addProfile( invalid ) ) {
		return false;
	}
	return profiles.getCount() == 2;
}

static bool testNativeGamepadFrame( void )
{
	SIRInputSystem input;
	sirInputDeviceHandle_t hPad = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_GAMEPAD, 77, &hPad ) ) {
		return false;
	}
	sirInputEvent_t out = {};
	input.pollEvent( &out );
	const sirInputGamepadFrameEvent_t events[] = {
	    { SIR_GAMEPAD_BUTTON_SOUTH, 1, SIR_INPUT_EVENT_BUTTON },  { SIR_GAMEPAD_BUTTON_SOUTH, 0, SIR_INPUT_EVENT_BUTTON },
	    { SIR_GAMEPAD_AXIS_LEFT_X, 22000, SIR_INPUT_EVENT_AXIS }, { SIR_GAMEPAD_AXIS_LEFT_X, 0, SIR_INPUT_EVENT_AXIS },
	    { SIR_GAMEPAD_BUTTON_EAST, 1, SIR_INPUT_EVENT_BUTTON },   { 23, -900000, SIR_INPUT_EVENT_AXIS } // Extended hardware axis.
	};
	if ( !input.submitGamepadFrame( hPad, events, 6, 405 ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < 6; ++i ) {
		if ( !input.pollEvent( &out ) || out.eKind != events[i].eKind || out.nCode != events[i].nCode || out.nValue != events[i].nValue ||
		     out.nTimestampNs != 405 ) {
			return false;
		}
	}
	if ( input.pollEvent( &out ) ) {
		return false;
	}
	sirInputDeviceState_t state = {};
	const uint32_t nSouthMask = UINT32_C( 1 ) << SIR_GAMEPAD_BUTTON_SOUTH;
	const uint32_t nEastMask = UINT32_C( 1 ) << SIR_GAMEPAD_BUTTON_EAST;
	if ( !input.getState( hPad, &state ) || ( state.nPressed[0] & nSouthMask ) == 0 || ( state.nReleased[0] & nSouthMask ) == 0 ||
	     ( state.nButtons[0] & nEastMask ) == 0 || state.nAxes[23] != -900000 ) {
		return false;
	}
	const sirInputGamepadFrameEvent_t invalid[] = { { SIR_GAMEPAD_BUTTON_NORTH, 1, SIR_INPUT_EVENT_BUTTON },
	                                                { SIR_GAMEPAD_AXIS_LEFT_TRIGGER, -1, SIR_INPUT_EVENT_AXIS } };
	if ( input.submitGamepadFrame( hPad, invalid, 2, 406 ) || input.submitGamepadFrame( hPad, nullptr, 1, 407 ) ||
	     input.pollEvent( &out ) || !input.getState( hPad, &state ) ||
	     ( state.nButtons[0] & ( UINT32_C( 1 ) << SIR_GAMEPAD_BUTTON_NORTH ) ) != 0 ) {
		return false;
	}
	if ( !input.submitGamepadFrame( hPad, nullptr, 0, 408 ) ) {
		return false;
	}
	// Flood a second device to leave just one event slot, then submit two edges.
	sirInputDeviceHandle_t hMouse = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_MOUSE, 88, &hMouse ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_EVENTS - 2; ++i ) {
		if ( !input.pushPointer( hMouse, 1, 1, i + 1 ) ) {
			return false;
		}
	}
	const sirInputGamepadFrameEvent_t overflow[] = { { SIR_GAMEPAD_BUTTON_WEST, 1, SIR_INPUT_EVENT_BUTTON },
	                                                 { SIR_GAMEPAD_BUTTON_WEST, 0, SIR_INPUT_EVENT_BUTTON } };
	if ( input.submitGamepadFrame( hPad, overflow, 2, 500 ) || !input.isOverflowed() || !input.getState( hPad, &state ) ||
	     ( state.nPressed[0] & ( UINT32_C( 1 ) << SIR_GAMEPAD_BUTTON_WEST ) ) != 0 || !input.resynchronizeDevice( hPad ) ||
	     input.isOverflowed() ) {
		return false;
	}
	return true;
}

#if defined( __linux__ )
static bool testEvdevMappings( void )
{
	return SIRLinuxEvdevInput::mapButton( BTN_SOUTH ) == SIR_GAMEPAD_BUTTON_SOUTH &&
	       SIRLinuxEvdevInput::mapButton( BTN_DPAD_UP ) == SIR_GAMEPAD_BUTTON_DPAD_UP &&
	       SIRLinuxEvdevInput::mapButton( BTN_TL2 ) == SIR_GAMEPAD_BUTTON_LEFT_TRIGGER_DIGITAL &&
	       SIRLinuxEvdevInput::mapButton( KEY_A ) == -1 && SIRLinuxEvdevInput::mapAxis( ABS_X ) == SIR_GAMEPAD_AXIS_LEFT_X &&
	       SIRLinuxEvdevInput::mapAxis( ABS_RZ ) == SIR_GAMEPAD_AXIS_RIGHT_TRIGGER && SIRLinuxEvdevInput::mapAxis( ABS_HAT0X ) == -1 &&
	       SIRLinuxEvdevInput::normalizeAxis( 0, -32768, 32767, false ) == 0 &&
	       SIRLinuxEvdevInput::normalizeAxis( -32768, -32768, 32767, false ) == -32767 &&
	       SIRLinuxEvdevInput::normalizeAxis( 32767, -32768, 32767, false ) == 32767 &&
	       SIRLinuxEvdevInput::normalizeAxis( 128, 0, 255, true ) == 16447 &&
	       SIRLinuxEvdevInput::normalizeAxis( 5000, 0, 255, true ) == 32767 && SIRLinuxEvdevInput::normalizeAxis( 50, 100, 0, false ) == 0;
}

static bool testEvdevWatchLifecycle( void )
{
	char szTemplate[] = "/tmp/sir-input-XXXXXX";
	char *pszDir = mkdtemp( szTemplate );
	if ( pszDir == nullptr ) {
		return false;
	}
	bool bSuccess = true;
	{
		SIRInputSystem input;
		SIRLinuxEvdevInput controller( input );
		bSuccess = controller.watchDirectory( pszDir ) && controller.isWatching() && !controller.watchDirectory( pszDir );
		char szEntry[128];
		const int nWritten = snprintf( szEntry, sizeof( szEntry ), "%s/event1", pszDir );
		if ( nWritten <= 0 || ( size_t )nWritten >= sizeof( szEntry ) ) {
			bSuccess = false;
		} else {
			const int nFd = open( szEntry, O_CREAT | O_EXCL | O_WRONLY, 0600 );
			if ( nFd < 0 ) {
				bSuccess = false;
			} else {
				close( nFd );
				controller.pump();
				bSuccess = bSuccess && controller.getDeviceCount() == 0;
				unlink( szEntry );
				controller.pump();
				bSuccess = bSuccess && controller.getDeviceCount() == 0;
			}
		}
		controller.shutdown();
		bSuccess =
		    bSuccess && !controller.isWatching() && controller.getDeviceCount() == 0 && !controller.setVibration( { 0, 1 }, 50, 60, 100 );
	}
	rmdir( pszDir );
	return bSuccess;
}
#endif

#if defined( SIR_INPUT_X11_AVAILABLE )
static bool testX11DesktopRouting( void )
{
	SIRInputSystem input;
	SIRLinuxX11Input x11( input );
	if ( SIRLinuxX11Input::enableDetectableAutoRepeat( nullptr ) ||
	     ( SIRLinuxX11Input::requiredEventMask() & ( KeyPressMask | KeyReleaseMask | FocusChangeMask ) ) !=
	         ( KeyPressMask | KeyReleaseMask | FocusChangeMask ) ) {
		return false;
	}
	if ( x11.bind( 0, 101, 102 ) || !x11.bind( ( Window )42, 101, 102 ) || x11.bind( ( Window )42, 101, 102 ) ||
	     input.getActiveDeviceCount() != 2 ) {
		return false;
	}
	sirInputDeviceHandle_t hKeyboard = {};
	sirInputDeviceHandle_t hMouse = {};
	if ( !input.findDevice( SIR_INPUT_DEVICE_KEYBOARD, 101, &hKeyboard ) || !input.findDevice( SIR_INPUT_DEVICE_MOUSE, 102, &hMouse ) ) {
		return false;
	}
	XEvent event = {};
	event.type = KeyPress;
	event.xkey.window = ( Window )42;
	event.xkey.keycode = 38;
	if ( x11.handleEvent( event ) ) {
		return false; // Focus is mandatory for desktop delivery.
	}
	event.type = FocusIn;
	event.xfocus.window = ( Window )7;
	if ( x11.handleEvent( event ) ) {
		return false; // Wrong window is ignored.
	}
	event.xfocus.window = ( Window )42;
	if ( !x11.handleEvent( event ) ) {
		return false;
	}
	event.type = KeyPress;
	event.xkey.window = ( Window )42;
	event.xkey.keycode = 38;
	if ( !x11.handleEvent( event ) ) {
		return false;
	}
	event.type = KeyRelease;
	if ( !x11.handleEvent( event ) ) {
		return false;
	}
	event.type = ButtonPress;
	event.xbutton.window = ( Window )42;
	event.xbutton.button = Button3;
	if ( !x11.handleEvent( event ) ) {
		return false;
	}
	event.type = ButtonRelease;
	if ( !x11.handleEvent( event ) ) {
		return false;
	}
	event.type = ButtonPress;
	event.xbutton.button = Button4;
	if ( !x11.handleEvent( event ) ) {
		return false;
	}
	event.type = MotionNotify;
	event.xmotion.window = ( Window )42;
	event.xmotion.x_root = 100;
	event.xmotion.y_root = 70;
	if ( !x11.handleEvent( event ) ) {
		return false;
	}
	event.xmotion.x_root = 111;
	event.xmotion.y_root = 65;
	if ( !x11.handleEvent( event ) ) {
		return false;
	}
	sirInputDeviceState_t keyboard = {};
	sirInputDeviceState_t mouse = {};
	if ( !input.getState( hKeyboard, &keyboard ) || !input.getState( hMouse, &mouse ) ||
	     ( keyboard.nPressed[38 / 32] & ( 1U << ( 38 % 32 ) ) ) == 0 || ( keyboard.nReleased[38 / 32] & ( 1U << ( 38 % 32 ) ) ) == 0 ||
	     mouse.nMouseDeltaX != 11 || mouse.nMouseDeltaY != -5 || ( mouse.nPressed[0] & ( 1U << 1 ) ) == 0 ||
	     ( mouse.nReleased[0] & ( 1U << 1 ) ) == 0 ) {
		return false;
	}
	uint32_t nKeyboardEdges = 0;
	uint32_t nWheel = 0;
	uint32_t nPointer = 0;
	sirInputEvent_t recorded = {};
	while ( input.pollEvent( &recorded ) ) {
		if ( recorded.eKind == SIR_INPUT_EVENT_BUTTON && recorded.eDeviceKind == SIR_INPUT_DEVICE_KEYBOARD ) {
			++nKeyboardEdges;
		}
		if ( recorded.eKind == SIR_INPUT_EVENT_WHEEL && recorded.nCode == 0 && recorded.nValue == 120 ) {
			++nWheel;
		}
		if ( recorded.eKind == SIR_INPUT_EVENT_POINTER && recorded.nValue == 11 && recorded.nValueY == -5 ) {
			++nPointer;
		}
	}
	if ( nKeyboardEdges != 2 || nWheel != 1 || nPointer != 1 ) {
		return false;
	}
	event.type = KeyPress;
	event.xkey.window = ( Window )42;
	event.xkey.keycode = 40;
	if ( !x11.handleEvent( event ) ) {
		return false;
	}
	event.type = FocusOut;
	event.xfocus.window = ( Window )42;
	if ( !x11.handleEvent( event ) || !input.getState( hKeyboard, &keyboard ) || keyboard.nButtons[40 / 32] != 0 ) {
		return false;
	}
	event.type = KeyPress;
	if ( x11.handleEvent( event ) ) {
		return false;
	}
	x11.shutdown();
	return input.getActiveDeviceCount() == 0 && !input.getState( hKeyboard, &keyboard );
}

static bool testX11FocusOverflowRecovery( void )
{
	SIRInputSystem input;
	SIRLinuxX11Input x11( input );
	if ( !x11.bind( ( Window )64, 601, 602 ) ) {
		return false;
	}
	sirInputDeviceHandle_t hKeyboard = {};
	sirInputDeviceHandle_t hMouse = {};
	if ( !input.findDevice( SIR_INPUT_DEVICE_KEYBOARD, 601, &hKeyboard ) || !input.findDevice( SIR_INPUT_DEVICE_MOUSE, 602, &hMouse ) ) {
		return false;
	}
	XEvent event = {};
	event.type = FocusIn;
	event.xfocus.window = ( Window )64;
	if ( !x11.handleEvent( event ) || !input.pushButton( hKeyboard, 38, true, 0 ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_EVENTS - 3; ++i ) {
		if ( !input.pushPointer( hMouse, 1, 0, 0 ) ) {
			return false;
		}
	}
	event.type = FocusOut;
	event.xfocus.window = ( Window )64;
	if ( !x11.handleEvent( event ) || input.isOverflowed() ) {
		return false;
	}
	sirInputDeviceState_t keyboard = {};
	if ( !input.getState( hKeyboard, &keyboard ) || keyboard.nButtons[38 / 32] != 0 ) {
		return false;
	}
	uint32_t nResync = 0;
	sirInputEvent_t received = {};
	while ( input.pollEvent( &received ) ) {
		if ( received.eKind != SIR_INPUT_EVENT_RESYNC ) {
			return false;
		}
		++nResync;
	}
	return nResync == 2;
}
#endif

int main( void )
{
	bool bPassed = testLifecycle() && testTransitions() && testPointerAndAnalog() && testQueuePressure() && testGamepadSamples() &&
	               testAtomicSampleOverflow() && testNativeGamepadFrame() && testNativeGamepadProfiles();
#if defined( __linux__ )
	SIRInputSystem input;
	SIRLinuxEvdevInput controller( input );
	bPassed = bPassed && !controller.openDevice( "/dev/null" ) && controller.discover( "/not/a/real/input/path" ) == 0 &&
	          controller.getDeviceCount() == 0 && testEvdevMappings() && testEvdevWatchLifecycle();
#endif
#if defined( SIR_INPUT_X11_AVAILABLE )
	bPassed = bPassed && testX11DesktopRouting() && testX11FocusOverflowRecovery();
#endif
	if ( !bPassed ) {
		fputs( "SIRInputTests failed\n", stderr );
		return 1;
	}
	return 0;
}
