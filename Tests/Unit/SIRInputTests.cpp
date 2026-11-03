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

int main( void )
{
	bool bPassed = testLifecycle() && testTransitions() && testPointerAndAnalog() && testQueuePressure();
#if defined( __linux__ )
	SIRInputSystem input;
	SIRLinuxEvdevInput controller( input );
	bPassed = bPassed && !controller.openDevice( "/dev/null" ) && controller.discover( "/not/a/real/input/path" ) == 0 &&
	          controller.getDeviceCount() == 0;
#endif
	if ( !bPassed ) {
		fputs( "SIRInputTests failed\n", stderr );
		return 1;
	}
	return 0;
}
