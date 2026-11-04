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
#include <Core/Platform/Input/SIRInputUIDispatch.h>
#if defined( SIR_INPUT_X11_AVAILABLE )
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>
#endif
#include <stdio.h>

class SIRInputGapBackend final : public SIRInputBackend
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
		++m_nResyncs;
	}

	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )hDevice;
		( void )nLeft;
		( void )nRight;
		return true;
	}

	uint32_t hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept override
	{
		( void )hDevice;
		return SIR_INPUT_HAPTIC_CAP_RUMBLE;
	}

	void stop( SIRInputService &input ) noexcept override
	{
		( void )input;
	}

	uint32_t m_nResyncs = 0;
};

typedef struct sirInputGapFocus_s
{
	uint32_t nCalls;
	bool bLastFocused;
} sirInputGapFocus_t;

static void sirInputGapOnFocus( void *pContext, bool bFocused ) noexcept
{
	sirInputGapFocus_t *pFocus = ( sirInputGapFocus_t * )pContext;
	++pFocus->nCalls;
	pFocus->bLastFocused = bFocused;
}

static bool sirInputGapHistoryAndTape( void ) noexcept
{
	sirInputReplayFrame_t szStorage[2] = {};
	SIRInputHistory history;
	if ( history.capacity() != 0 || !history.initialize( szStorage, 2 ) || history.capacity() != 2 ) {
		return false;
	}

	sirInputReplayFrame_t frame = {};
	frame.nTick = 1;
	if ( !history.append( frame ) || history.count() != 1 ) {
		return false;
	}
	history.clear();
	if ( history.count() != 0 || history.oldestTick() != 0 || history.newestTick() != 0 || history.capacity() != 2 ||
	     history.read( 1, &frame ) ) {
		return false;
	}

	uint8_t szBytes[128] = {};
	SIRInputReplayTape tape;
	if ( tape.mode() != SIR_INPUT_REPLAY_IDLE || !tape.beginRecord( szBytes, sizeof( szBytes ) ) ||
	     tape.mode() != SIR_INPUT_REPLAY_RECORD ) {
		return false;
	}
	tape.reset();
	return tape.mode() == SIR_INPUT_REPLAY_IDLE;
}

static bool sirInputGapFrameAndUIDispatch( void ) noexcept
{
	SIRInputFrameState state;
	SIRInputDeviceRegistry registry;
	if ( state.overflowed() || state.lastPublishedTick() != 0 || state.frame() != nullptr || !state.begin( 1 ) || state.overflowed() ||
	     !state.publish( registry ) || state.lastPublishedTick() != 1 || state.frame()->nTick != 1 ) {
		return false;
	}

	SIRInputService input;
	if ( !input.begin( 1 ) ) {
		return false;
	}
	input.text().setEnabled( true );
	input.text().setFocused( true );
	if ( !input.publish() ) {
		return false;
	}
	sirInputGapFocus_t focus = {};
	sirInputUIConsumer_t consumer = {};
	consumer.pContext = &focus;
	consumer.pOnFocus = sirInputGapOnFocus;
	SIRInputUIDispatch dispatch;
	if ( !dispatch.dispatch( input, consumer ) || focus.nCalls != 1 || !focus.bLastFocused || dispatch.dispatch( input, consumer ) ) {
		return false;
	}
	dispatch.reset();
	return dispatch.dispatch( input, consumer ) && focus.nCalls == 2 && focus.bLastFocused;
}

static bool sirInputGapBackendAndHaptics( void ) noexcept
{
	SIRInputService input;
	SIRInputGapBackend backend;
	if ( !input.attach( backend ) || !input.begin( 1 ) || input.loseBackendState( SIR_INPUT_LINUX_EVDEV ) ) {
		return false;
	}
	const SIRInputService &reference = input;
	if ( reference.actions().contextCount() != 0 ) {
		return false;
	}
	sirInputDeviceId_t hDevice = {};
	if ( !input.connect( { 900, 901, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &hDevice ) ) {
		return false;
	}
	sirInputHapticHandle_t hEffect = {};
	const sirInputHapticRequest_t request = { hDevice, 10000, 20000, 0, 100, 0, 1 };
	if ( !input.scheduleRumble( request, &hEffect ) || !input.cancelRumble( hEffect ) || input.cancelRumble( hEffect ) ) {
		return false;
	}
	if ( !input.loseBackendState( SIR_INPUT_TEST ) || !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *pFrame = input.frame();
	if ( pFrame == nullptr || pFrame->nResyncCount != 1 || pFrame->nCount != 2 || pFrame->events[1].eSignal != SIR_INPUT_RESYNC ) {
		return false;
	}
	input.shutdown();
	return !input.loseBackendState( SIR_INPUT_TEST );
}

static bool sirInputGapX11Output( void ) noexcept
{
#if defined( SIR_INPUT_X11_AVAILABLE )
	SIRLinuxX11Backend desktop;
	const sirInputDeviceId_t invalid = {};
	if ( desktop.vibrate( invalid, 300, 500 ) ) {
		return false;
	}
#endif
	return true;
}

static bool sirInputGapDeviceRegistry( void ) noexcept
{
	SIRInputDeviceRegistry registry;
	sirInputDeviceDescriptor_t descriptor = {};
	sirInputDeviceId_t hDevice = {};
	sirInputDeviceId_t hOther = {};
	sirInputDeviceClass_t eClass = SIR_INPUT_KEYBOARD;
	const sirInputHardwareKey_t key = { 0x1234, 1, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER };
	if ( registry.count() != 0 || registry.valid( {} ) || registry.valid( { SIR_INPUT_DEVICE_CAPACITY, 1 } ) ||
	     registry.resolve( SIR_INPUT_TEST, 1, nullptr ) || registry.resolve( SIR_INPUT_TEST, 1, &hDevice ) ||
	     registry.describe( {}, &descriptor ) || registry.authorize( SIR_INPUT_TEST, {}, &eClass ) ||
	     registry.authorize( SIR_INPUT_TEST, { SIR_INPUT_DEVICE_CAPACITY, 1 }, &eClass ) || registry.inspectSlot( 0, nullptr ) ||
	     registry.inspectSlot( SIR_INPUT_DEVICE_CAPACITY, &descriptor ) || registry.pairUser( {}, 0 ) || registry.disconnect( {} ) ||
	     registry.connect( key, nullptr ) || registry.connect( { 0x1234, 0, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &hDevice ) ||
	     registry.connect( { 0x1234, 1, ( sirInputBackendId_t )0, SIR_INPUT_CONTROLLER }, &hDevice ) ||
	     registry.connect( { 0x1234, 1, SIR_INPUT_TEST, ( sirInputDeviceClass_t )255 }, &hDevice ) ) {
		return false;
	}
	if ( !registry.connect( key, &hDevice ) || !registry.valid( hDevice ) || registry.count() != 1 || registry.connect( key, &hOther ) ||
	     registry.connect( { 0x1234, 2, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &hOther ) ||
	     !registry.resolve( SIR_INPUT_TEST, 1, &hOther ) || hDevice.nSlot != hOther.nSlot ||
	     registry.resolve( SIR_INPUT_LINUX_EVDEV, 1, &hOther ) || registry.describe( hDevice, nullptr ) ||
	     !registry.describe( hDevice, &descriptor ) || !registry.inspectSlot( hDevice.nSlot, &descriptor ) ||
	     registry.authorize( SIR_INPUT_TEST, hDevice, nullptr ) || registry.authorize( SIR_INPUT_LINUX_EVDEV, hDevice, &eClass ) ||
	     registry.authorize( SIR_INPUT_TEST, { hDevice.nSlot, hDevice.nGeneration + 1 }, &eClass ) ||
	     !registry.authorize( SIR_INPUT_TEST, hDevice, &eClass ) || eClass != SIR_INPUT_CONTROLLER ||
	     registry.pairUser( hDevice, SIR_INPUT_USER_CAPACITY ) || !registry.pairUser( hDevice, 2 ) ||
	     !registry.describe( hDevice, &descriptor ) || descriptor.nUserId != 2 ) {
		return false;
	}
	sirInputDeviceId_t hPointer = {};
	if ( !registry.connect( { 0x1234, 2, SIR_INPUT_TEST, SIR_INPUT_POINTER }, &hPointer ) ||
	     !registry.pairUser( hPointer, SIR_INPUT_NO_USER ) || registry.count() != 2 ) {
		return false;
	}
	for ( uint32_t i = 2; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		sirInputDeviceId_t hNew = {};
		if ( !registry.connect( { 1000 + i, 1000 + i, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &hNew ) || !registry.valid( hNew ) ) {
			return false;
		}
	}
	if ( registry.count() != SIR_INPUT_DEVICE_CAPACITY ||
	     registry.connect( { 9999, 9999, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &hOther ) ) {
		return false;
	}
	sirInputFrame_t snapshot = {};
	registry.snapshot( snapshot );
	if ( snapshot.nConnected[hDevice.nSlot] != 1 || snapshot.nUsers[hDevice.nSlot] != 2 || snapshot.nConnected[hPointer.nSlot] != 1 ||
	     snapshot.nUsers[hPointer.nSlot] != SIR_INPUT_NO_USER ) {
		return false;
	}
	const sirInputDeviceId_t stale = hDevice;
	if ( !registry.disconnect( hDevice ) || registry.disconnect( hDevice ) || registry.valid( stale ) ||
	     registry.describe( stale, &descriptor ) || registry.pairUser( stale, 1 ) || registry.authorize( SIR_INPUT_TEST, stale, &eClass ) ||
	     !registry.connect( key, &hDevice ) || hDevice.nSlot != stale.nSlot || hDevice.nGeneration == stale.nGeneration ||
	     !registry.valid( hDevice ) || registry.count() != SIR_INPUT_DEVICE_CAPACITY ) {
		return false;
	}
	registry.reset();
	if ( registry.count() != 0 || registry.valid( hDevice ) || !registry.inspectSlot( hDevice.nSlot, &descriptor ) ||
	     descriptor.bConnected || descriptor.nUserId != SIR_INPUT_NO_USER ) {
		return false;
	}
	registry.snapshot( snapshot );
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( snapshot.nConnected[i] != 0 || snapshot.nUsers[i] != SIR_INPUT_NO_USER ) {
			return false;
		}
	}
	return true;
}

typedef struct sirInputGapUICounters_s
{
	uint32_t nFocus;
	uint32_t nActions;
	uint32_t nPreedit;
	bool bFocused;
} sirInputGapUICounters_t;

static void sirInputGapUICountFocus( void *pContext, bool bFocused ) noexcept
{
	sirInputGapUICounters_t *pCount = ( sirInputGapUICounters_t * )pContext;
	++pCount->nFocus;
	pCount->bFocused = bFocused;
}

static void sirInputGapUICountAction( void *pContext, const sirInputActionState_t *pState ) noexcept
{
	( void )pState;
	++( ( sirInputGapUICounters_t * )pContext )->nActions;
}

static void sirInputGapUICountPreedit( void *pContext, const uint32_t *pCodepoints, uint32_t nCount, uint32_t nCaret,
                                       uint32_t nSelection ) noexcept
{
	( void )pCodepoints;
	( void )nCount;
	( void )nCaret;
	( void )nSelection;
	++( ( sirInputGapUICounters_t * )pContext )->nPreedit;
}

static bool sirInputGapUIDispatchBoundaries( void ) noexcept
{
	SIRInputService input;
	SIRInputUIDispatch dispatch;
	sirInputGapUICounters_t counts = {};
	const uint32_t nMissingAction = UINT32_C( 0x12345678 );
	sirInputUIConsumer_t consumer = {};
	consumer.pContext = &counts;
	consumer.pActionIds = &nMissingAction;
	consumer.nActionCount = 1;
	consumer.pOnFocus = sirInputGapUICountFocus;
	consumer.pOnAction = sirInputGapUICountAction;
	consumer.pOnPreedit = sirInputGapUICountPreedit;
	if ( dispatch.dispatch( input, consumer ) ) {
		return false;
	}
	SIRInputService zeroTick;
	if ( zeroTick.begin( 0 ) || zeroTick.publish() || dispatch.dispatch( zeroTick, consumer ) ) {
		return false;
	}
	input.text().setEnabled( true );
	input.text().setFocused( true );
	if ( !input.begin( 1 ) || !input.publish() ) {
		return false;
	}
	sirInputUIConsumer_t invalid = consumer;
	invalid.nActionCount = SIR_INPUT_ACTION_CAPACITY + 1;
	if ( dispatch.dispatch( input, invalid ) ) {
		return false;
	}
	invalid = consumer;
	invalid.pActionIds = nullptr;
	if ( dispatch.dispatch( input, invalid ) ) {
		return false;
	}
	invalid = consumer;
	invalid.nUser = SIR_INPUT_USER_CAPACITY;
	if ( dispatch.dispatch( input, invalid ) ) {
		return false;
	}
	input.text().reset();
	if ( dispatch.dispatch( input, consumer ) ) {
		return false;
	}
	input.text().setEnabled( true );
	input.text().setFocused( true );
	if ( !input.begin( 2 ) || !input.publish() || !dispatch.dispatch( input, consumer ) || counts.nFocus != 1 || !counts.bFocused ||
	     counts.nActions != 0 || counts.nPreedit != 1 || dispatch.dispatch( input, consumer ) ) {
		return false;
	}
	uint32_t nCodepoint = 'x';
	if ( !input.begin( 3 ) || !input.text().setPreeditCodepoints( &nCodepoint, 1, 1, 0 ) || !input.publish() ||
	     !dispatch.dispatch( input, consumer ) || counts.nFocus != 1 || counts.nPreedit != 2 ) {
		return false;
	}
	sirInputUIConsumer_t noFocusCallback = consumer;
	noFocusCallback.pOnFocus = nullptr;
	if ( !input.begin( 4 ) || !input.publish() || !dispatch.dispatch( input, noFocusCallback ) || counts.nFocus != 1 ||
	     counts.nPreedit != 2 ) {
		return false;
	}
	input.text().setFocused( false );
	if ( !input.begin( 5 ) || !input.publish() || !dispatch.dispatch( input, consumer ) || counts.nFocus != 2 || counts.bFocused ||
	     counts.nPreedit != 3 ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirInputGapHistoryAndTape() ) {
		fputs( "input contracts: history/tape FAILED\n", stderr );
		return 1;
	}
	if ( !sirInputGapFrameAndUIDispatch() ) {
		fputs( "input contracts: frame/UI FAILED\n", stderr );
		return 1;
	}
	if ( !sirInputGapBackendAndHaptics() ) {
		fputs( "input contracts: backend/haptics FAILED\n", stderr );
		return 1;
	}
	if ( !sirInputGapX11Output() ) {
		fputs( "input contracts: X11 output FAILED\n", stderr );
		return 1;
	}
	if ( !sirInputGapDeviceRegistry() ) {
		fputs( "input contracts: device registry FAILED\n", stderr );
		return 1;
	}
	if ( !sirInputGapUIDispatchBoundaries() ) {
		fputs( "input contracts: UI boundary dispatch FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputContractGapTests PASS" );
	return 0;
}
