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
#include <stdio.h>
#include <string.h>

class SIRInputActionTestBackend final : public SIRInputBackend
{
public:
	sirInputBackendId_t id( void ) const noexcept override
	{
		return SIR_INPUT_TEST;
	}
	void sample( SIRInputService &input ) noexcept override
	{
		( void )input;
	}
	void resynchronize( SIRInputService &input ) noexcept override
	{
		( void )input;
	}
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )hDevice;
		( void )nLeft;
		( void )nRight;
		return false;
	}
	void stop( SIRInputService &input ) noexcept override
	{
		( void )input;
		++m_nStops;
	}
	uint32_t m_nStops = 0;
};

static sirInputActionBinding_t sirTestBinding( uint32_t nAction, uint32_t nContext, uint16_t nKey, uint16_t nChord,
                                               sirInputDeviceClass_t eClass, sirInputSignal_t eSignal, uint8_t nFlags )
{
	return { nAction, nContext, nKey, nChord, 0, 0, 32767, eClass, eSignal, nFlags };
}

static bool sirReadAction( SIRInputService &input, uint32_t nAction, bool bHeld, bool bPressed, bool bReleased )
{
	sirInputActionState_t state = {};
	return input.actions().query( nAction, 0, &state ) && state.bHeld == bHeld && state.bPressed == bPressed &&
	       state.bReleased == bReleased;
}

static bool sirEmitKey( SIRInputService &input, sirInputDeviceId_t hDevice, uint16_t nCode, bool bDown )
{
	const sirInputControlChange_t event = { SIR_INPUT_BUTTON, nCode, bDown ? 1 : 0 };
	return input.emitControls( SIR_INPUT_TEST, hDevice, &event, 1 );
}

static bool sirTestContextsAndRebinding( void )
{
	SIRInputService input;
	SIRInputActionTestBackend backend;
	SIRInputActions &actions = input.actions();
	if ( !input.attach( backend ) || !actions.addAction( 1, 0 ) || !actions.addAction( 2, 0 ) ||
	     !actions.addContext( 10, 0, 100, true ) || !actions.addContext( 20, 0, 0, true ) ) {
		return false;
	}

	const sirInputActionBinding_t ui = sirTestBinding( 1, 10, 42, SIR_INPUT_NO_CHORD, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON,
	                                                   SIR_INPUT_BIND_CONSUME );
	const sirInputActionBinding_t game =
	    sirTestBinding( 2, 20, 42, SIR_INPUT_NO_CHORD, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE );
	if ( !actions.addBinding( ui ) || !actions.addBinding( game ) || actions.addBinding( ui ) ||
	     actions.addContext( 10, 0, 1, true ) || actions.addAction( 1, 0 ) ) {
		return false;
	}

	const sirInputActionBinding_t conflict =
	    sirTestBinding( 2, 20, 42, SIR_INPUT_NO_CHORD, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_CONSUME );
	if ( actions.addBinding( conflict ) || actions.rebind( 1, ui ) || actions.removeBinding( 2 ) ) {
		return false;
	}
	if ( !input.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t hKeyboard = {};
	if ( !input.connect( { 0, 17, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &hKeyboard ) ||
	     !sirEmitKey( input, hKeyboard, 42, true ) || !input.publish() ||
	     !sirReadAction( input, 1, true, true, false ) || !sirReadAction( input, 2, false, false, false ) ) {
		return false;
	}

	if ( !input.begin( 2 ) || !sirEmitKey( input, hKeyboard, 42, false ) || !input.publish() ||
	     !sirReadAction( input, 1, false, false, true ) || !actions.setContextActive( 10, 0, false ) ) {
		return false;
	}

	if ( !input.begin( 3 ) || !sirEmitKey( input, hKeyboard, 42, true ) || !input.publish() ||
	     !sirReadAction( input, 2, true, true, false ) ) {
		return false;
	}

	if ( !input.begin( 4 ) || !sirEmitKey( input, hKeyboard, 42, false ) || !input.publish() ||
	     !sirReadAction( input, 2, false, false, true ) ) {
		return false;
	}

	sirInputActionBinding_t changed = game;
	changed.nControl = 43;
	if ( !actions.rebind( 1, changed ) || actions.rebind( 2, game ) || actions.addBinding( changed ) ) {
		return false;
	}
	if ( !input.begin( 5 ) || !sirEmitKey( input, hKeyboard, 42, true ) || !input.publish() ||
	     !sirReadAction( input, 2, false, false, false ) ) {
		return false;
	}
	if ( !input.begin( 6 ) || !sirEmitKey( input, hKeyboard, 43, true ) || !input.publish() ||
	     !sirReadAction( input, 2, true, true, false ) ) {
		return false;
	}
	input.shutdown();
	return backend.m_nStops == 1;
}

static bool sirTestChordShortTransitionsAndToggle( void )
{
	SIRInputService input;
	SIRInputActionTestBackend backend;
	SIRInputActions &actions = input.actions();
	if ( !input.attach( backend ) || !actions.addContext( 20, 0, 10, true ) || !actions.addAction( 3, 0 ) ||
	     !actions.addAction( 4, 0 ) || !actions.addAction( 5, 0 ) ) {
		return false;
	}

	const sirInputActionBinding_t chord = sirTestBinding( 3, 20, 33, 17, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE );
	const sirInputActionBinding_t toggle =
	    sirTestBinding( 4, 20, 50, SIR_INPUT_NO_CHORD, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_TOGGLE );
	const sirInputActionBinding_t tap =
	    sirTestBinding( 5, 20, 60, SIR_INPUT_NO_CHORD, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE );
	if ( !actions.addBinding( chord ) || !actions.addBinding( toggle ) || !actions.addBinding( tap ) || !input.begin( 1 ) ) {
		return false;
	}

	sirInputDeviceId_t hKeyboard = {};
	if ( !input.connect( { 0, 19, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &hKeyboard ) ||
	     !sirEmitKey( input, hKeyboard, 33, true ) || !input.publish() ||
	     !sirReadAction( input, 3, false, false, false ) ) {
		return false;
	}

	if ( !input.begin( 2 ) || !sirEmitKey( input, hKeyboard, 17, true ) || !input.publish() ||
	     !sirReadAction( input, 3, true, true, false ) ) {
		return false;
	}

	if ( !input.begin( 3 ) || !sirEmitKey( input, hKeyboard, 17, false ) || !input.publish() ||
	     !sirReadAction( input, 3, false, false, true ) ) {
		return false;
	}

	if ( !input.begin( 4 ) || !sirEmitKey( input, hKeyboard, 60, true ) ||
	     !sirEmitKey( input, hKeyboard, 60, false ) || !sirEmitKey( input, hKeyboard, 50, true ) || !input.publish() ||
	     !sirReadAction( input, 5, false, true, true ) || !sirReadAction( input, 4, true, true, false ) ) {
		return false;
	}
	sirInputActionState_t tapState = {};
	if ( !actions.query( 5, 0, &tapState ) || tapState.nTransitions != 2 ) {
		return false;
	}

	if ( !input.begin( 5 ) || !sirEmitKey( input, hKeyboard, 50, false ) || !input.publish() ||
	     !sirReadAction( input, 4, true, false, false ) ) {
		return false;
	}
	if ( !input.begin( 6 ) || !sirEmitKey( input, hKeyboard, 50, true ) || !input.publish() ||
	     !sirReadAction( input, 4, false, false, true ) ) {
		return false;
	}
	input.shutdown();
	return backend.m_nStops == 1;
}

static bool sirTestControllerAxis( void )
{
	SIRInputService input;
	SIRInputActionTestBackend backend;
	SIRInputActions &actions = input.actions();
	if ( !input.attach( backend ) || !actions.addAction( 9, 0 ) || !actions.addContext( 30, 0, 0, true ) ) {
		return false;
	}
	sirInputActionBinding_t axis =
	    sirTestBinding( 9, 30, SIR_INPUT_AXIS_LX, SIR_INPUT_NO_CHORD, SIR_INPUT_CONTROLLER, SIR_INPUT_AXIS, SIR_INPUT_BIND_NONE );
	axis.nDeadzone = 8000;
	axis.nScale = -32767;
	if ( !actions.addBinding( axis ) || !input.begin( 1 ) ) {
		return false;
	}

	sirInputDeviceId_t hPad = {};
	if ( !input.connect( { 0, 61, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &hPad ) || !input.pairUser( hPad, 0 ) ) {
		return false;
	}
	const sirInputControlChange_t below = { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LX, 4000 };
	if ( !input.emitControls( SIR_INPUT_TEST, hPad, &below, 1 ) || !input.publish() ||
	     !sirReadAction( input, 9, false, false, false ) ) {
		return false;
	}

	const sirInputControlChange_t above = { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LX, 25000 };
	if ( !input.begin( 2 ) || !input.emitControls( SIR_INPUT_TEST, hPad, &above, 1 ) || !input.publish() ) {
		return false;
	}
	sirInputActionState_t state = {};
	if ( !actions.query( 9, 0, &state ) || !state.bHeld || !state.bPressed || state.nValue >= -20000 ) {
		return false;
	}
	if ( !input.begin( 3 ) || !input.disconnect( SIR_INPUT_TEST, hPad ) || !input.publish() ||
	     !sirReadAction( input, 9, false, false, true ) ) {
		return false;
	}
	input.shutdown();
	return true;
}

static bool sirTestProfileRoundTripAndRejection( void )
{
	SIRInputActions source;
	if ( !source.addAction( 12, 0 ) || !source.addContext( 55, 0, 20, true ) ) {
		return false;
	}
	const sirInputActionBinding_t binding =
	    sirTestBinding( 12, 55, 61, 17, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_CONSUME );
	if ( !source.addBinding( binding ) ) {
		return false;
	}
	uint8_t szProfile[8192] = {};
	uint32_t nRequired = 0;
	if ( source.saveProfile( nullptr, 0, &nRequired ) || nRequired < SIR_INPUT_PROFILE_HEADER_BYTES ||
	     !source.saveProfile( szProfile, sizeof( szProfile ), &nRequired ) ) {
		return false;
	}

	SIRInputActions decoded;
	if ( !decoded.loadProfile( szProfile, nRequired ) || decoded.bindingCount() != 1 || decoded.contextCount() != 1 ) {
		return false;
	}
	sirInputActionBinding_t actual = {};
	if ( !decoded.describeBinding( 0, &actual ) || actual.nActionId != binding.nActionId ||
	     actual.nControl != binding.nControl || actual.nChord != binding.nChord || actual.nFlags != binding.nFlags ) {
		return false;
	}

	uint8_t szRoundTrip[8192] = {};
	uint32_t nSaved = 0;
	if ( !decoded.saveProfile( szRoundTrip, sizeof( szRoundTrip ), &nSaved ) || nSaved != nRequired ||
	     memcmp( szProfile, szRoundTrip, nRequired ) != 0 ) {
		return false;
	}

	szProfile[nRequired - 1] ^= 0x40;
	if ( decoded.loadProfile( szProfile, nRequired ) || decoded.loadProfile( szRoundTrip, nRequired - 1 ) ||
	     decoded.loadProfile( nullptr, nRequired ) || decoded.bindingCount() != 1 ) {
		return false;
	}
	return true;
}

static bool sirTestPhysicalRebindCapture( void )
{
	SIRInputService input;
	SIRInputActionTestBackend backend;
	SIRInputActions &actions = input.actions();
	if ( !input.attach( backend ) || !actions.addAction( 10, 0 ) || !actions.addAction( 11, 0 ) ||
	     !actions.addContext( 77, 0, 20, true ) ) {
		return false;
	}
	const sirInputActionBinding_t first =
	    sirTestBinding( 10, 77, 70, SIR_INPUT_NO_CHORD, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE );
	const sirInputActionBinding_t second =
	    sirTestBinding( 11, 77, 71, SIR_INPUT_NO_CHORD, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE );
	if ( !actions.addBinding( first ) || !actions.addBinding( second ) || !actions.beginCapture( 0 ) ||
	     actions.beginCapture( 0 ) || actions.captureStatus() != SIR_INPUT_CAPTURE_PENDING || !input.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t hKeyboard = {};
	if ( !input.connect( { 0, 19, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &hKeyboard ) ||
	     !sirEmitKey( input, hKeyboard, 71, true ) || !input.publish() ||
	     actions.captureStatus() != SIR_INPUT_CAPTURE_CONFLICT ) {
		return false;
	}

	sirInputActionBinding_t current = {};
	if ( !actions.describeBinding( 0, &current ) || current.nControl != 70 || !actions.beginCapture( 0 ) ||
	     !input.begin( 2 ) || !sirEmitKey( input, hKeyboard, 72, true ) || !input.publish() ||
	     actions.captureStatus() != SIR_INPUT_CAPTURE_ACCEPTED || !actions.describeBinding( 0, &current ) ||
	     current.nControl != 72 ) {
		return false;
	}
	actions.cancelCapture();
	if ( actions.captureStatus() != SIR_INPUT_CAPTURE_IDLE || !input.begin( 3 ) ||
	     !sirEmitKey( input, hKeyboard, 72, false ) || !input.publish() ||
	     !sirReadAction( input, 10, false, false, true ) ) {
		return false;
	}
	input.shutdown();
	return true;
}

int main( void )
{
	if ( !sirTestContextsAndRebinding() || !sirTestChordShortTransitionsAndToggle() || !sirTestControllerAxis() ||
	     !sirTestProfileRoundTripAndRejection() || !sirTestPhysicalRebindCapture() ) {
		fputs( "SIRInputActionTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputActionTests PASS" );
	return 0;
}
