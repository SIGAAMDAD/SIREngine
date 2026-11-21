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

#include <Core/Input/SIRInputService.h>
#include <Core/Input/SIRInputUIDispatch.h>
#include <Core/Input/SIRInputControllerProfiles.h>
#if defined( __linux__ )
#include <Core/Platform/Linux/SIRLinuxEvdevBackend.h>
#endif
#include <stdio.h>
#include <string.h>

class SIRInputUIHapticBackend final : public SIRInputBackend
{
public:
	sirInputBackendId_t id( void ) const noexcept override
	{
		return SIR_INPUT_TEST;
	}
	void sample( SIRInputService& service ) noexcept override
	{
		( void )service;
	}
	void resynchronize( SIRInputService& service ) noexcept override
	{
		( void )service;
	}
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		m_hLastDevice = hDevice;
		m_nLastLeft = nLeft;
		m_nLastRight = nRight;
		++m_nRumbleCalls;
		return true;
	}
	uint32_t hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept override
	{
		( void )hDevice;
		return SIR_INPUT_HAPTIC_CAP_RUMBLE;
	}
	void stop( SIRInputService& service ) noexcept override
	{
		( void )service;
		++m_nStops;
	}
	sirInputDeviceId_t m_hLastDevice = {};
	uint16_t m_nLastLeft = 0;
	uint16_t m_nLastRight = 0;
	uint32_t m_nRumbleCalls = 0;
	uint32_t m_nStops = 0;
};

typedef struct sirInputUIHapticRecorder_s
{
	uint32_t nTextCount;
	uint32_t nCodepoint;
	uint32_t nActions;
	uint32_t nPreeditCalls;
	uint32_t nPreeditLength;
	uint32_t nFocusCalls;
	uint32_t nPointerCalls;
	bool bFocused;
	bool bActionPressed;
	uint16_t nLeft;
	uint16_t nRight;
	uint32_t nRumbleCalls;
} sirInputUIHapticRecorder_t;

static void sirRecordUIAction( void *pContext, const sirInputActionState_t *pState ) noexcept
{
	sirInputUIHapticRecorder_t *pRecord = ( sirInputUIHapticRecorder_t * )pContext;
	++pRecord->nActions;
	pRecord->bActionPressed = pState->bPressed;
}

static void sirRecordUIText( void *pContext, uint32_t nCodepoint ) noexcept
{
	sirInputUIHapticRecorder_t *pRecord = ( sirInputUIHapticRecorder_t * )pContext;
	++pRecord->nTextCount;
	pRecord->nCodepoint = nCodepoint;
}

static void sirRecordUIPreedit( void *pContext, const uint32_t *pCodepoints, uint32_t nCount, uint32_t nCaret,
                                uint32_t nSelection ) noexcept
{
	( void )pCodepoints;
	( void )nCaret;
	( void )nSelection;
	sirInputUIHapticRecorder_t *pRecord = ( sirInputUIHapticRecorder_t * )pContext;
	++pRecord->nPreeditCalls;
	pRecord->nPreeditLength = nCount;
}

static void sirRecordUIFocus( void *pContext, bool bFocused ) noexcept
{
	sirInputUIHapticRecorder_t *pRecord = ( sirInputUIHapticRecorder_t * )pContext;
	++pRecord->nFocusCalls;
	pRecord->bFocused = bFocused;
}

static void sirRecordUIPointer( void *pContext, const sirInputEvent_t *pEvent ) noexcept
{
	sirInputUIHapticRecorder_t *pRecord = ( sirInputUIHapticRecorder_t * )pContext;
	if ( pEvent->eSignal == SIR_INPUT_MOTION ) {
		++pRecord->nPointerCalls;
	}
}

static bool sirRecordRumble( void *pContext, sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept
{
	( void )hDevice;
	sirInputUIHapticRecorder_t *pRecord = ( sirInputUIHapticRecorder_t * )pContext;
	pRecord->nLeft = nLeft;
	pRecord->nRight = nRight;
	++pRecord->nRumbleCalls;
	return true;
}

static bool sirTestTextAndUIRoute( void )
{
	SIRInputService input;
	SIRInputUIHapticBackend backend;
	SIRInputUIDispatch bridge;
	sirInputUIHapticRecorder_t recorder = {};
	if ( !input.attach( backend ) || !input.actions().addContext( 1, 0, 10, true ) || !input.actions().addAction( 42, 0 ) ) {
		return false;
	}
	const sirInputActionBinding_t binding = {
	    42, 1, 15, SIR_INPUT_NO_CHORD, 0, 0, 32767, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE };
	if ( !input.actions().addBinding( binding ) ) {
		return false;
	}
	input.text().setEnabled( true );
	input.text().setFocused( true );
	if ( !input.begin( 1 ) || input.text().appendCodepoint( 0xd800 ) ) {
		return false;
	}
	sirInputDeviceId_t hKeyboard = {};
	sirInputDeviceId_t hPointer = {};
	const sirInputControlChange_t change = { SIR_INPUT_BUTTON, 15, 1 };
	if ( !input.connect( { 0, 1234, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &hKeyboard ) ||
	     !input.connect( { 0, 1235, SIR_INPUT_TEST, SIR_INPUT_POINTER }, &hPointer ) ||
	     !input.emitControls( SIR_INPUT_TEST, hKeyboard, &change, 1 ) ||
	     !input.emitPointer( SIR_INPUT_TEST, hPointer, SIR_INPUT_MOTION, 0, 2, -3 ) ) {
		return false;
	}
	const char szUtf8[4] = { 'A', ( char )0xe2, ( char )0x82, ( char )0xac };
	const uint16_t szPreedit[2] = { 0x4e2d, 0x6587 };
	if ( !input.text().appendUtf8( szUtf8, sizeof( szUtf8 ) ) || !input.text().appendUtf16( 0xd83d ) ||
	     !input.text().appendUtf16( 0xde00 ) || !input.text().setPreeditUtf16( szPreedit, 2, 1, 1 ) || !input.publish() ) {
		return false;
	}

	const sirInputTextSnapshot_t& text = input.text().snapshot();
	if ( text.nTick != 1 || text.nCount != 3 || text.szCodepoints[0] != 'A' || text.szCodepoints[1] != 0x20ac ||
	     text.szCodepoints[2] != 0x1f600 || text.nPreeditCount != 2 || text.nCaret != 1 || text.nSelection != 1 ) {
		return false;
	}
	const uint32_t nActionId = 42;
	sirInputUIConsumer_t consumer = { &recorder,        &nActionId,        1, 0, sirRecordUIAction, sirRecordUIText, sirRecordUIPreedit,
	                                  sirRecordUIFocus, sirRecordUIPointer };
	if ( !bridge.dispatch( input, consumer ) || bridge.dispatch( input, consumer ) || recorder.nTextCount != 3 ||
	     recorder.nCodepoint != 0x1f600 || !recorder.bActionPressed || recorder.nActions != 1 || recorder.nPreeditCalls != 1 ||
	     recorder.nPreeditLength != 2 || recorder.nFocusCalls != 1 || !recorder.bFocused || recorder.nPointerCalls != 1 ) {
		return false;
	}

	input.text().setFocused( false );
	if ( !input.begin( 2 ) || input.text().appendCodepoint( 'Z' ) || !input.publish() || !bridge.dispatch( input, consumer ) ||
	     recorder.nTextCount != 3 || recorder.nPreeditLength != 0 || recorder.nFocusCalls != 2 || recorder.bFocused ) {
		return false;
	}
	input.shutdown();
	return backend.m_nStops == 1;
}

static bool sirTestTextValidationAndOverflow( void )
{
	SIRInputText text;
	text.setFocused( true );
	text.setEnabled( true );
	text.begin( 1 );
	const char szBadUtf8[] = { ( char )0xc0, ( char )0x80 };
	const char szGoodUtf8[] = { ( char )0xf0, ( char )0x9f, ( char )0x8c, ( char )0x8d };
	if ( text.appendUtf8( szBadUtf8, 2 ) || !text.appendUtf8( szGoodUtf8, 4 ) || text.snapshot().nCount != 1 ||
	     text.snapshot().szCodepoints[0] != 0x1f30d ) {
		return false;
	}
	for ( uint32_t i = 1; i < SIR_INPUT_TEXT_CAPACITY; ++i ) {
		if ( !text.appendCodepoint( 'a' ) ) {
			return false;
		}
	}
	if ( text.appendCodepoint( 'b' ) || !text.snapshot().bOverflow ) {
		return false;
	}
	text.publish();
	if ( text.appendCodepoint( 'c' ) || text.snapshot().nCount != SIR_INPUT_TEXT_CAPACITY ) {
		return false;
	}
	text.begin( 2 );
	if ( text.snapshot().nCount != 0 || text.snapshot().bOverflow ) {
		return false;
	}
	const uint16_t szInvalid[] = { 0xd800, 0x0041 };
	if ( text.setPreeditUtf16( szInvalid, 2, 0, 0 ) || text.snapshot().nPreeditCount != 0 ) {
		return false;
	}
	text.reset();
	return text.snapshot().nTick == 0;
}

static bool sirTestCookedControllerProfiles( void )
{
	SIRInputControllerProfiles profiles;
	sirInputControllerProfile_t profile = {};
	profile.nVendor = 0x054c;
	profile.nProduct = 0x0ce6;
	profile.nButtonCount = 2;
	profile.buttons[0] = { 304, SIR_INPUT_PAD_EAST };
	profile.buttons[1] = { 305, SIR_INPUT_PAD_SOUTH };
	profile.nAxisCount = 1;
	profile.axes[0] = { 7, SIR_INPUT_AXIS_LY, true };
	profile.bReplaceButtons = true;
	if ( !profiles.add( profile ) ) {
		return false;
	}
	uint8_t szBytes[SIR_INPUT_COOKED_PROFILE_MAX_BYTES] = {};
	uint32_t nSize = 0;
	if ( profiles.saveCooked( nullptr, 0, &nSize ) || nSize <= SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ||
	     !profiles.saveCooked( szBytes, sizeof( szBytes ), &nSize ) ) {
		return false;
	}

	SIRInputControllerProfiles restored;
	if ( !restored.loadCooked( szBytes, nSize ) || restored.count() != 1 ) {
		return false;
	}
	const sirInputControllerProfile_t *pFound = restored.find( 5, profile.nVendor, profile.nProduct );
	if ( pFound == nullptr || pFound->buttons[1].nLogicalCode != SIR_INPUT_PAD_SOUTH || !pFound->bReplaceButtons ||
	     !pFound->axes[0].bInvert ) {
		return false;
	}
	uint8_t szRoundTrip[SIR_INPUT_COOKED_PROFILE_MAX_BYTES] = {};
	uint32_t nRoundTrip = 0;
	if ( !restored.saveCooked( szRoundTrip, sizeof( szRoundTrip ), &nRoundTrip ) || nSize != nRoundTrip ||
	     memcmp( szBytes, szRoundTrip, nSize ) != 0 ) {
		return false;
	}
	szBytes[nSize - 1] ^= 0x80;
	if ( restored.loadCooked( szBytes, nSize ) || restored.loadCooked( szRoundTrip, nSize - 1 ) || restored.count() != 1 ) {
		return false;
	}
#if defined( __linux__ )
	SIRLinuxEvdevBackend backend;
	if ( !backend.importCookedProfiles( szRoundTrip, nRoundTrip ) || backend.importCookedProfiles( szRoundTrip, nRoundTrip ) ) {
		return false;
	}
#endif
	return true;
}

static bool sirTestHapticMixing( void )
{
	SIRInputHaptics haptics;
	sirInputUIHapticRecorder_t recorder = {};
	const sirInputDeviceId_t hDevice = { 3, 5 };
	const sirInputHapticRequest_t slow = { hDevice, 60000, 10000, 10, 20, 10, 1 };
	sirInputHapticHandle_t hLow = {};
	if ( !haptics.schedule( slow, 1000000000ULL, &hLow ) || haptics.activeCount() != 1 ||
	     !haptics.advance( 1010000000ULL, false, sirRecordRumble, &recorder ) || recorder.nLeft != 60000 || recorder.nRight != 10000 ) {
		return false;
	}

	const sirInputHapticRequest_t high = { hDevice, 5000, 40000, 0, 20, 0, 10 };
	sirInputHapticHandle_t hHigh = {};
	if ( !haptics.schedule( high, 1010000000ULL, &hHigh ) || !haptics.advance( 1011000000ULL, false, sirRecordRumble, &recorder ) ||
	     recorder.nLeft != 5000 || recorder.nRight != 40000 || !haptics.cancel( hHigh ) || haptics.cancel( hHigh ) ) {
		return false;
	}
	if ( !haptics.advance( 1020000000ULL, false, sirRecordRumble, &recorder ) || recorder.nLeft != 60000 ||
	     !haptics.advance( 1035000000ULL, false, sirRecordRumble, &recorder ) || recorder.nLeft == 0 || recorder.nLeft >= 60000 ||
	     !haptics.advance( 1040000000ULL, false, sirRecordRumble, &recorder ) || recorder.nLeft != 0 || recorder.nRight != 0 ||
	     haptics.activeCount() != 0 ) {
		return false;
	}

	const sirInputHapticRequest_t invalid = { hDevice, 65535, 65535, 0, 61000, 0, 0 };
	if ( haptics.schedule( invalid, 0, &hHigh ) || haptics.cancel( hLow ) ) {
		return false;
	}
	return true;
}

static bool sirTestHapticDeviceLifecycle( void )
{
	SIRInputService input;
	SIRInputUIHapticBackend backend;
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		fputs( "haptic-lifecycle step 1 failed\n", stderr );
		return false;
	}
	sirInputDeviceId_t hDevice = {};
	if ( !input.connect( { 0, 51, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &hDevice ) ||
	     input.hapticCapabilities( hDevice ) != SIR_INPUT_HAPTIC_CAP_RUMBLE ) {
		fputs( "haptic-lifecycle step 2 failed\n", stderr );
		return false;
	}
	const sirInputHapticRequest_t request = { hDevice, 32000, 9000, 0, 1000, 0, 1 };
	sirInputHapticHandle_t hEffect = {};
	if ( !input.scheduleRumble( request, &hEffect ) || input.scheduleRumble( request, nullptr ) ) {
		fputs( "haptic-lifecycle step 3 failed\n", stderr );
		return false;
	}
	input.sample();
	if ( backend.m_nLastLeft != 32000 || input.activeHaptics() != 1 ) {
		fputs( "haptic-lifecycle step 4 failed\n", stderr );
		return false;
	}
	input.setHapticsPaused( true );
	if ( backend.m_nLastLeft != 0 || backend.m_nLastRight != 0 ) {
		fputs( "haptic-lifecycle step 5 failed\n", stderr );
		return false;
	}
	input.setHapticsPaused( false );
	input.sample();
	if ( backend.m_nLastLeft != 32000 || !input.disconnect( SIR_INPUT_TEST, hDevice ) || input.activeHaptics() != 0 ||
	     backend.m_nLastLeft != 0 ) {
		fputs( "haptic-lifecycle step 6 failed\n", stderr );
		return false;
	}
	input.shutdown();
	if ( backend.m_nStops != 1 ) {
		fputs( "haptic-lifecycle step 7 failed\n", stderr );
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirTestTextAndUIRoute() ) {
		fputs( "SIRInputPhase45Tests text-and-ui FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestTextValidationAndOverflow() ) {
		fputs( "SIRInputPhase45Tests text-validation FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestCookedControllerProfiles() ) {
		fputs( "SIRInputPhase45Tests cooked-profiles FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestHapticMixing() ) {
		fputs( "SIRInputPhase45Tests haptic-mixing FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestHapticDeviceLifecycle() ) {
		fputs( "SIRInputPhase45Tests haptic-lifecycle FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputPhase45Tests PASS" );
	return 0;
}
