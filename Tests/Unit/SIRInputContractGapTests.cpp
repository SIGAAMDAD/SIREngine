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
	puts( "SIRInputContractGapTests PASS" );
	return 0;
}
