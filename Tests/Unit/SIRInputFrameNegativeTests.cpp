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

#include <Core/Platform/Input/SIRInputFrameState.h>
#include <Core/Platform/Input/SIRInputDeviceRegistry.h>
#include <stdio.h>

static bool sirFrameNegativeParameters( void ) noexcept
{
	SIRInputFrameState state;
	SIRInputDeviceRegistry registry;
	const sirInputDeviceId_t hDevice = { 0, 1 };
	const sirInputDeviceId_t hBadDevice = { SIR_INPUT_DEVICE_CAPACITY, 1 };
	const sirInputTimestamp_t time = { 100, 120, SIR_INPUT_CLOCK_GAMEINPUT, 1 };
	const sirInputControlChange_t press = { SIR_INPUT_BUTTON, 7, 1 };
	if ( state.postDevice( hDevice, SIR_INPUT_KEYBOARD, SIR_INPUT_CONNECTED, time ) || !state.begin( 1 ) || state.begin( 2 ) ||
	     state.postDevice( hDevice, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, time ) ||
	     state.postDevice( hBadDevice, SIR_INPUT_KEYBOARD, SIR_INPUT_CONNECTED, time ) ) {
		return false;
	}
	if ( state.postPointer( hBadDevice, SIR_INPUT_POINTER, SIR_INPUT_MOTION, 0, 1, 2, time ) ||
	     state.postPointer( hDevice, SIR_INPUT_KEYBOARD, SIR_INPUT_MOTION, 0, 1, 2, time ) ||
	     state.postPointer( hDevice, SIR_INPUT_POINTER, SIR_INPUT_BUTTON, 0, 1, 2, time ) ||
	     state.postChanges( hBadDevice, SIR_INPUT_KEYBOARD, &press, 1, time ) ||
	     state.postChanges( hDevice, SIR_INPUT_KEYBOARD, nullptr, 1, time ) ||
	     state.postChanges( hDevice, SIR_INPUT_KEYBOARD, &press, SIR_INPUT_EVENT_CAPACITY + 1, time ) ) {
		return false;
	}
	const sirInputControlChange_t invalidSingle[] = { { SIR_INPUT_BUTTON, SIR_INPUT_BUTTON_CAPACITY, 1 },
	                                                  { SIR_INPUT_BUTTON, 7, 2 },
	                                                  { SIR_INPUT_AXIS, SIR_INPUT_AXIS_CAPACITY, 0 },
	                                                  { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LX, -32768 },
	                                                  { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LX, 32768 },
	                                                  { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LT, -1 },
	                                                  { SIR_INPUT_MOTION, 0, 1 } };
	for ( uint32_t i = 0; i < sizeof( invalidSingle ) / sizeof( invalidSingle[0] ); ++i ) {
		if ( state.postChanges( hDevice, SIR_INPUT_KEYBOARD, &invalidSingle[i], 1, time ) ) {
			return false;
		}
	}
	const sirInputControlChange_t invalidBatches[][2] = { { press, { SIR_INPUT_BUTTON, SIR_INPUT_BUTTON_CAPACITY, 1 } },
	                                                      { press, { SIR_INPUT_BUTTON, 8, 2 } },
	                                                      { press, { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LT, -1 } },
	                                                      { press, { SIR_INPUT_AXIS, SIR_INPUT_AXIS_CAPACITY, 1 } },
	                                                      { press, { SIR_INPUT_WHEEL, 0, 1 } } };
	for ( uint32_t i = 0; i < sizeof( invalidBatches ) / sizeof( invalidBatches[0] ); ++i ) {
		if ( state.postChanges( hDevice, SIR_INPUT_KEYBOARD, invalidBatches[i], 2, time ) ) {
			return false;
		}
	}
	const sirInputControlChange_t changes[] = {
	    { SIR_INPUT_BUTTON, 7, 1 }, { SIR_INPUT_BUTTON, 7, 0 }, { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LX, 100 } };
	if ( !state.postDevice( hDevice, SIR_INPUT_KEYBOARD, SIR_INPUT_RESYNC, time ) ||
	     !state.postChanges( hDevice, SIR_INPUT_KEYBOARD, changes, 3, time ) ||
	     !state.postPointer( hDevice, SIR_INPUT_POINTER, SIR_INPUT_MOTION, 0, 2, -3, time ) || !state.publish( registry ) ) {
		return false;
	}
	const sirInputFrame_t *pFrame = state.frame();
	if ( pFrame == nullptr || pFrame->nCount != 5 || pFrame->nResyncCount != 1 ||
	     pFrame->events[0].nTimestampFlags != SIR_INPUT_TIMESTAMP_NATIVE || pFrame->events[0].nTimestampNs != 100 ||
	     pFrame->state[hDevice.nSlot].nMotionX != 2 || pFrame->state[hDevice.nSlot].nMotionY != -3 ||
	     pFrame->state[hDevice.nSlot].nHeld[0] != 0 || pFrame->state[hDevice.nSlot].nAxes[SIR_INPUT_AXIS_LX] != 100 ) {
		return false;
	}
	if ( !state.begin( 2 ) || state.recover( registry ) || !state.publish( registry ) || state.frame()->nCount != 0 ||
	     state.frame()->state[hDevice.nSlot].nMotionX != 0 ) {
		return false;
	}
	return pFrame->nCount == 5 && pFrame->nTick == 1 && state.lastPublishedTick() == 2;
}

static bool sirFrameNegativeOverflow( void ) noexcept
{
	SIRInputFrameState state;
	SIRInputDeviceRegistry registry;
	const sirInputDeviceId_t hDevice = { 0, 1 };
	const sirInputTimestamp_t time = { 1234, 1234, SIR_INPUT_CLOCK_ENGINE_MONOTONIC, 0 };
	if ( !state.begin( 1 ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_EVENT_CAPACITY; ++i ) {
		const sirInputControlChange_t change = { SIR_INPUT_BUTTON, 42, ( i & 1 ) == 0 ? 1 : 0 };
		if ( !state.postChanges( hDevice, SIR_INPUT_CONTROLLER, &change, 1, time ) ) {
			return false;
		}
	}
	if ( state.canPost( 1 ) || !state.canPost( 0 ) || state.postDevice( hDevice, SIR_INPUT_CONTROLLER, SIR_INPUT_CONNECTED, time ) ||
	     !state.overflowed() || state.publish( registry ) ||
	     state.postPointer( hDevice, SIR_INPUT_POINTER, SIR_INPUT_WHEEL, 0, 120, 0, time ) ||
	     state.postChanges( hDevice, SIR_INPUT_CONTROLLER, nullptr, 0, time ) || !state.recover( registry ) || state.overflowed() ||
	     !state.publish( registry ) ) {
		return false;
	}
	return state.frame()->nCount == 0 && state.frame()->nResyncCount == 0;
}

static bool sirFrameNegativeFullAxisAndPointer( void ) noexcept
{
	SIRInputFrameState state;
	SIRInputDeviceRegistry registry;
	const sirInputDeviceId_t hDevice = { 0, 1 };
	const sirInputTimestamp_t time = { 100, 101, SIR_INPUT_CLOCK_ENGINE_MONOTONIC, 0 };
	for ( uint32_t nTick = 1; nTick <= 2; ++nTick ) {
		if ( !state.begin( nTick ) ) {
			return false;
		}
		for ( uint32_t i = 0; i < SIR_INPUT_EVENT_CAPACITY; ++i ) {
			const sirInputControlChange_t change = { SIR_INPUT_BUTTON, 4, ( i & 1 ) == 0 ? 1 : 0 };
			if ( !state.postChanges( hDevice, SIR_INPUT_CONTROLLER, &change, 1, time ) ) {
				return false;
			}
		}
		if ( nTick == 1 ) {
			const sirInputControlChange_t axis = { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LX, 1200 };
			if ( state.postChanges( hDevice, SIR_INPUT_CONTROLLER, &axis, 1, time ) || !state.overflowed() ) {
				return false;
			}
		} else if ( state.postPointer( hDevice, SIR_INPUT_POINTER, SIR_INPUT_WHEEL, 0, 120, 0, time ) || !state.overflowed() ) {
			return false;
		}
		if ( !state.recover( registry ) || !state.publish( registry ) || state.frame()->nCount != 0 ) {
			return false;
		}
	}
	return state.lastPublishedTick() == 2;
}

int main( void )
{
	if ( !sirFrameNegativeParameters() ) {
		fputs( "input frame negative: parameters FAILED\n", stderr );
		return 1;
	}
	if ( !sirFrameNegativeOverflow() ) {
		fputs( "input frame negative: overflow FAILED\n", stderr );
		return 1;
	}
	if ( !sirFrameNegativeFullAxisAndPointer() ) {
		fputs( "input frame negative: full axis/pointer FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputFrameNegativeTests PASS" );
	return 0;
}
