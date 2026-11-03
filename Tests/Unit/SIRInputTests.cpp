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
#include <Core/Platform/SIRInputCoordinator.h>
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
	if ( input.pushButton( hKeyboard, 1, false, 100000 ) || !input.isOverflowed() || input.publishTick( 1 ) ||
	     !input.resynchronizeDevice( hKeyboard ) || input.isOverflowed() ) {
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

// Source Engine-style current/queued frame semantics: the published frame
// remains stable while the owner changes live state for the following tick.
static bool testPublishedTickState( void )
{
	SIRInputSystem input;
	sirInputDeviceHandle_t hKeyboard = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 990, &hKeyboard ) || !input.isDeviceActive( hKeyboard ) ||
	     input.getPublishedTick() != 0 ) {
		return false;
	}
	input.beginTick();
	if ( !input.pushButton( hKeyboard, 42, true, 100 ) || !input.publishTick( 1 ) || input.publishTick( 1 ) ||
	     input.getPublishedTick() != 1 ) {
		return false;
	}
	const uint32_t nMask = UINT32_C( 1 ) << ( 42 % 32 );
	sirInputDeviceState_t first = {};
	if ( !input.getPublishedState( hKeyboard, &first ) || ( first.nPressed[42 / 32] & nMask ) == 0 ||
	     ( first.nButtons[42 / 32] & nMask ) == 0 ) {
		return false;
	}
	input.beginTick();
	if ( !input.pushButton( hKeyboard, 42, false, 101 ) ) {
		return false;
	}
	sirInputDeviceState_t published = {};
	sirInputDeviceState_t live = {};
	if ( !input.getPublishedState( hKeyboard, &published ) || !input.getState( hKeyboard, &live ) ||
	     ( published.nButtons[42 / 32] & nMask ) == 0 || ( live.nButtons[42 / 32] & nMask ) != 0 ||
	     ( live.nReleased[42 / 32] & nMask ) == 0 ) {
		return false;
	}
	// Removing and reusing a device slot must not alias the earlier frame.
	if ( !input.removeDevice( hKeyboard ) || input.isDeviceActive( hKeyboard ) || !input.getPublishedState( hKeyboard, &published ) ) {
		return false;
	}
	sirInputDeviceHandle_t hNew = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 990, &hNew ) || hNew.nSlot != hKeyboard.nSlot ||
	     hNew.nGeneration == hKeyboard.nGeneration || input.getPublishedState( hNew, &published ) ) {
		return false;
	}
	if ( !input.publishTick( 2 ) || input.getPublishedState( hKeyboard, &published ) || !input.getPublishedState( hNew, &published ) ||
	     input.publishTick( 1 ) ) {
		return false;
	}
	input.shutdown();
	return input.getPublishedTick() == 0 && !input.getPublishedState( hNew, &published ) && !input.isDeviceActive( hNew );
}

// A minimal backend proves the coordinator's lifecycle and ownership contracts
// without needing a controller or desktop session on the test host.
class SIRInputMockBackend : public SIRInputBackend
{
public:
	SIRInputMockBackend( SIRInputSystem &input, sirInputBackendKind_t eKind, uint64_t nPhysicalKey ) noexcept
	    : m_Input( input ), m_hDevice{}, m_nPhysicalKey( nPhysicalKey ), m_eKind( eKind ), m_nPumps( 0 ), m_nRumbles( 0 ),
	      m_bOwned( false ), m_bPendingPress( false ), m_bShutdown( false )
	{
	}
	bool attachDevice( uint64_t nNativeId ) noexcept
	{
		m_bOwned = m_Input.addDevice( SIR_INPUT_DEVICE_GAMEPAD, nNativeId, &m_hDevice );
		return m_bOwned;
	}
	void setDevice( sirInputDeviceHandle_t hDevice ) noexcept
	{
		m_hDevice = hDevice;
		m_bOwned = true;
	}
	void setPhysicalKey( uint64_t nPhysicalKey ) noexcept
	{
		m_nPhysicalKey = nPhysicalKey;
	}
	void setPendingPress( bool bPressed ) noexcept
	{
		m_bPendingPress = bPressed;
	}
	sirInputDeviceHandle_t getDevice( void ) const noexcept
	{
		return m_hDevice;
	}
	sirInputBackendKind_t getBackendKind( void ) const noexcept override
	{
		return m_eKind;
	}
	uint32_t pump( void ) noexcept override
	{
		++m_nPumps;
		if ( m_bPendingPress ) {
			m_bPendingPress = false;
			return m_Input.pushButton( m_hDevice, SIR_GAMEPAD_BUTTON_SOUTH, true, 501 ) ? 1 : 0;
		}
		return 0;
	}
	bool ownsDevice( sirInputDeviceHandle_t hDevice ) const noexcept override
	{
		return m_bOwned && m_Input.isDeviceActive( hDevice ) && hDevice.nSlot == m_hDevice.nSlot &&
		       hDevice.nGeneration == m_hDevice.nGeneration;
	}
	uint64_t getPhysicalKey( sirInputDeviceHandle_t hDevice ) const noexcept override
	{
		return ownsDevice( hDevice ) ? m_nPhysicalKey : 0;
	}
	bool setVibration( sirInputDeviceHandle_t hDevice, uint16_t nStrong, uint16_t nWeak, uint16_t nDurationMs ) noexcept override
	{
		if ( !ownsDevice( hDevice ) || nStrong == 0 || nWeak == 0 || nDurationMs == 0 ) {
			return false;
		}
		++m_nRumbles;
		return true;
	}
	void shutdown( void ) noexcept override
	{
		m_bShutdown = true;
		if ( m_bOwned && m_Input.isDeviceActive( m_hDevice ) ) {
			m_Input.removeDevice( m_hDevice );
		}
		m_bOwned = false;
	}
	bool wasShutdown( void ) const noexcept
	{
		return m_bShutdown;
	}
	uint32_t getPumps( void ) const noexcept
	{
		return m_nPumps;
	}
	uint32_t getRumbles( void ) const noexcept
	{
		return m_nRumbles;
	}

private:
	SIRInputSystem &m_Input;
	sirInputDeviceHandle_t m_hDevice;
	uint64_t m_nPhysicalKey;
	sirInputBackendKind_t m_eKind;
	uint32_t m_nPumps;
	uint32_t m_nRumbles;
	bool m_bOwned;
	bool m_bPendingPress;
	bool m_bShutdown;
};

static bool testPublishedInputState( void )
{
	SIRInputSystem input;
	sirInputDeviceHandle_t hKeyboard = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 900, &hKeyboard ) || !input.isDeviceActive( hKeyboard ) ) {
		return false;
	}
	input.beginTick();
	if ( !input.pushButton( hKeyboard, 42, true, 100 ) || !input.pushButton( hKeyboard, 42, false, 101 ) || !input.publishTick( 1 ) ||
	     input.publishTick( 1 ) || input.getPublishedTick() != 1 ) {
		return false;
	}
	const uint32_t nMask = UINT32_C( 1 ) << ( 42 % 32 );
	sirInputDeviceState_t snapshot = {};
	if ( !input.getPublishedState( hKeyboard, &snapshot ) || ( snapshot.nPressed[42 / 32] & nMask ) == 0 ||
	     ( snapshot.nReleased[42 / 32] & nMask ) == 0 || ( snapshot.nButtons[42 / 32] & nMask ) != 0 ||
	     input.getPublishedEventCount() != 3 || input.copyPublishedEvents( nullptr, 0 ) != 3 ) {
		return false;
	}
	sirInputEvent_t events[4] = {};
	if ( input.copyPublishedEvents( events, 4 ) != 3 || events[0].eKind != SIR_INPUT_EVENT_CONNECTED ||
	     events[1].eKind != SIR_INPUT_EVENT_BUTTON || events[1].nValue != 1 || events[2].nValue != 0 ) {
		return false;
	}
	sirInputEvent_t transient = {};
	if ( input.pollEvent( &transient ) ) {
		return false;
	}
	input.beginTick();
	if ( !input.pushButton( hKeyboard, 41, true, 102 ) || !input.getPublishedState( hKeyboard, &snapshot ) ||
	     ( snapshot.nButtons[41 / 32] & ( UINT32_C( 1 ) << ( 41 % 32 ) ) ) != 0 || !input.publishTick( 2 ) ||
	     input.getPublishedEventCount() != 1 || input.copyPublishedEvents( events, 4 ) != 1 || events[0].nCode != 41 ) {
		return false;
	}
	if ( !input.removeDevice( hKeyboard ) || !input.getPublishedState( hKeyboard, &snapshot ) ) {
		return false;
	}
	sirInputDeviceHandle_t hNew = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 900, &hNew ) || hNew.nSlot != hKeyboard.nSlot ||
	     hNew.nGeneration == hKeyboard.nGeneration || input.getPublishedState( hNew, &snapshot ) || !input.publishTick( 3 ) ||
	     input.getPublishedState( hKeyboard, &snapshot ) || !input.getPublishedState( hNew, &snapshot ) ) {
		return false;
	}
	sirInputDeviceKind_t eKind = SIR_INPUT_DEVICE_GAMEPAD;
	if ( !input.getDeviceKind( hNew, &eKind ) || eKind != SIR_INPUT_DEVICE_KEYBOARD || input.getDeviceKind( hKeyboard, &eKind ) ||
	     input.getDeviceKind( hNew, nullptr ) ) {
		return false;
	}
	input.shutdown();
	return input.getPublishedTick() == 0 && input.getPublishedEventCount() == 0;
}

static bool testCoordinatorLifecycle( void )
{
	SIRInputSystem input;
	SIRInputMockBackend backend( input, SIR_INPUT_BACKEND_WINDOWS_XINPUT, UINT64_C( 0x123456 ) );
	SIRInputCoordinator coordinator( input );
	if ( !coordinator.attachBackend( backend ) || coordinator.attachBackend( backend ) || !backend.attachDevice( 22 ) ||
	     !coordinator.beginTick( 1 ) || coordinator.attachBackend( backend ) ) {
		return false;
	}
	backend.setPendingPress( true );
	if ( !coordinator.publishTick() || coordinator.getLastResult() != SIR_INPUT_COORD_OK || backend.getPumps() != 1 ||
	     input.getPublishedEventCount() != 2 ) {
		return false;
	}
	sirInputDeviceRoute_t route = {};
	if ( !coordinator.getDeviceRoute( backend.getDevice(), &route ) || route.nPhysicalKey != UINT64_C( 0x123456 ) ||
	     route.nUserId != SIR_INPUT_UNPAIRED_USER || !coordinator.assignUser( backend.getDevice(), 2 ) ||
	     !coordinator.getDeviceRoute( backend.getDevice(), &route ) || route.nUserId != 2 ||
	     coordinator.assignUser( backend.getDevice(), SIR_INPUT_MAX_USERS ) ||
	     !coordinator.setVibration( backend.getDevice(), 200, 100, 30 ) || backend.getRumbles() != 1 || coordinator.beginTick( 1 ) ||
	     !coordinator.beginTick( 2 ) ) {
		return false;
	}
	const sirInputDeviceHandle_t hOriginal = backend.getDevice();
	if ( !input.removeDevice( hOriginal ) || !backend.attachDevice( 23 ) || !coordinator.publishTick() ||
	     coordinator.getDeviceRoute( hOriginal, &route ) || !coordinator.getDeviceRoute( backend.getDevice(), &route ) ||
	     route.nUserId != SIR_INPUT_UNPAIRED_USER ) {
		return false;
	}
	coordinator.shutdown();
	return backend.wasShutdown() && coordinator.getBackendCount() == 0 && !coordinator.beginTick( 3 ) &&
	       !coordinator.setVibration( backend.getDevice(), 10, 10, 100 );
}

static bool testCoordinatorOwnerConflicts( void )
{
	SIRInputSystem input;
	SIRInputMockBackend first( input, SIR_INPUT_BACKEND_WINDOWS_XINPUT, 1 );
	SIRInputMockBackend second( input, SIR_INPUT_BACKEND_WINDOWS_GAMEINPUT, 2 );
	SIRInputCoordinator coordinator( input );
	if ( !coordinator.attachBackend( first ) || !coordinator.attachBackend( second ) || !first.attachDevice( 91 ) ) {
		return false;
	}
	second.setDevice( first.getDevice() );
	if ( !coordinator.beginTick( 1 ) || coordinator.publishTick() || coordinator.getLastResult() != SIR_INPUT_COORD_DUPLICATE_OWNER ) {
		return false;
	}
	coordinator.shutdown();
	return first.wasShutdown() && second.wasShutdown();
}

static bool testCoordinatorPhysicalKeyConflict( void )
{
	SIRInputSystem input;
	SIRInputMockBackend first( input, SIR_INPUT_BACKEND_WINDOWS_XINPUT, 77 );
	SIRInputMockBackend second( input, SIR_INPUT_BACKEND_WINDOWS_GAMEINPUT, 77 );
	SIRInputCoordinator coordinator( input );
	if ( !coordinator.attachBackend( first ) || !coordinator.attachBackend( second ) || !first.attachDevice( 30 ) ||
	     !second.attachDevice( 31 ) || !coordinator.beginTick( 1 ) || coordinator.publishTick() ||
	     coordinator.getLastResult() != SIR_INPUT_COORD_DUPLICATE_PHYSICAL_DEVICE ) {
		return false;
	}
	// The failed tick cannot publish an owner for even the first device.
	sirInputDeviceRoute_t route = {};
	if ( coordinator.getDeviceRoute( first.getDevice(), &route ) || input.getPublishedTick() != 0 ) {
		return false;
	}
	coordinator.shutdown();
	return true;
}

static bool testCoordinatorPhysicalIdentitySwap( void )
{
	SIRInputSystem input;
	SIRInputMockBackend first( input, SIR_INPUT_BACKEND_WINDOWS_XINPUT, 91 );
	SIRInputMockBackend second( input, SIR_INPUT_BACKEND_WINDOWS_GAMEINPUT, 92 );
	SIRInputCoordinator coordinator( input );
	if ( !coordinator.attachBackend( first ) || !coordinator.attachBackend( second ) || !first.attachDevice( 1001 ) ||
	     !second.attachDevice( 1002 ) || !coordinator.beginTick( 1 ) || !coordinator.publishTick() ||
	     !coordinator.assignUser( first.getDevice(), 3 ) ) {
		return false;
	}
	first.setPhysicalKey( 92 );
	second.setPhysicalKey( 91 );
	if ( !coordinator.beginTick( 2 ) || !coordinator.publishTick() ) {
		return false;
	}
	sirInputDeviceRoute_t firstRoute = {};
	sirInputDeviceRoute_t secondRoute = {};
	if ( !coordinator.getDeviceRoute( first.getDevice(), &firstRoute ) || !coordinator.getDeviceRoute( second.getDevice(), &secondRoute ) ||
	     firstRoute.nPhysicalKey != 92 || firstRoute.nUserId != 3 || secondRoute.nPhysicalKey != 91 ) {
		return false;
	}
	coordinator.shutdown();
	return true;
}

static bool testCoordinatorDesktopAndUnownedRejection( void )
{
	SIRInputSystem input;
	SIRInputMockBackend x11( input, SIR_INPUT_BACKEND_LINUX_X11, 0 );
	SIRInputMockBackend wayland( input, SIR_INPUT_BACKEND_LINUX_WAYLAND, 0 );
	SIRInputCoordinator coordinator( input );
	if ( !coordinator.attachBackend( x11 ) || coordinator.attachBackend( wayland ) || coordinator.getBackendCount() != 1 ) {
		return false;
	}
	sirInputDeviceHandle_t hUnowned = {};
	if ( !input.addDevice( SIR_INPUT_DEVICE_KEYBOARD, 11, &hUnowned ) || !coordinator.beginTick( 1 ) || coordinator.publishTick() ||
	     coordinator.getLastResult() != SIR_INPUT_COORD_UNOWNED_DEVICE || input.getPublishedTick() != 0 ) {
		return false;
	}
	sirInputDeviceRoute_t route = {};
	if ( coordinator.getDeviceRoute( hUnowned, &route ) ) {
		return false;
	}
	coordinator.shutdown();
	return x11.wasShutdown() && !wayland.wasShutdown();
}

static bool testCoordinatorOverflowRecovery( void )
{
	SIRInputSystem input;
	SIRInputMockBackend backend( input, SIR_INPUT_BACKEND_WINDOWS_XINPUT, 0 );
	SIRInputCoordinator coordinator( input );
	if ( !coordinator.attachBackend( backend ) || !backend.attachDevice( 9 ) || !coordinator.beginTick( 1 ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_EVENTS - 1; ++i ) {
		if ( !input.pushButton( backend.getDevice(), 7, ( i & 1 ) == 0, i + 1 ) ) {
			return false;
		}
	}
	if ( input.pushButton( backend.getDevice(), 7, false, 90000 ) || !input.isOverflowed() || coordinator.publishTick() ||
	     coordinator.getLastResult() != SIR_INPUT_COORD_QUEUE_OVERFLOW || input.getPublishedTick() != 0 ||
	     !input.resynchronizeDevice( backend.getDevice() ) || !coordinator.publishTick() || input.getPublishedTick() != 1 ||
	     input.getPublishedEventCount() != 1 ) {
		return false;
	}
	sirInputEvent_t event = {};
	if ( input.copyPublishedEvents( &event, 1 ) != 1 || event.eKind != SIR_INPUT_EVENT_RESYNC ) {
		return false;
	}
	coordinator.shutdown();
	return true;
}

int main( void )
{
	bool bPassed = testPublishedTickState() && testLifecycle() && testTransitions() && testPointerAndAnalog() && testQueuePressure() &&
	               testGamepadSamples() && testAtomicSampleOverflow() && testNativeGamepadFrame() && testNativeGamepadProfiles() &&
	               testPublishedInputState() && testCoordinatorLifecycle() && testCoordinatorOwnerConflicts() &&
	               testCoordinatorPhysicalKeyConflict() && testCoordinatorPhysicalIdentitySwap() &&
	               testCoordinatorDesktopAndUnownedRejection() && testCoordinatorOverflowRecovery();
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
