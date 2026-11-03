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
#if defined( __linux__ )
#include <Core/Platform/Linux/SIRLinuxEvdevInput.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdio.h>

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

int main( void )
{
	bool bPassed = testLifecycle() && testTransitions() && testPointerAndAnalog() && testQueuePressure() && testGamepadSamples() &&
	               testAtomicSampleOverflow();
#if defined( __linux__ )
	SIRInputSystem input;
	SIRLinuxEvdevInput controller( input );
	bPassed = bPassed && !controller.openDevice( "/dev/null" ) && controller.discover( "/not/a/real/input/path" ) == 0 &&
	          controller.getDeviceCount() == 0 && testEvdevMappings() && testEvdevWatchLifecycle();
#endif
	if ( !bPassed ) {
		fputs( "SIRInputTests failed\n", stderr );
		return 1;
	}
	return 0;
}
