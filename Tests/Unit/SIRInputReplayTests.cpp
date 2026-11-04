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

class SIRReplayTestBackend final : public SIRInputBackend
{
public:
	sirInputBackendId_t id( void ) const noexcept override
	{
		return SIR_INPUT_TEST;
	}
	void sample( SIRInputService &service ) noexcept override
	{
		( void )service;
	}
	void resynchronize( SIRInputService &service ) noexcept override
	{
		( void )service;
	}
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )hDevice;
		( void )nLeft;
		( void )nRight;
		return false;
	}
	void stop( SIRInputService &service ) noexcept override
	{
		( void )service;
	}
};

static bool sirTestReplayServiceTick( void ) noexcept
{
	SIRInputService input;
	SIRReplayTestBackend backend;
	if ( !input.attach( backend ) || !input.actions().addContext( 10, 0, 100, true ) ||
	     !input.actions().addAction( 7, 0 ) || !input.actions().addContext( 11, 1, 100, true ) ||
	     !input.actions().addAction( 8, 1 ) ) {
		return false;
	}
	const sirInputActionBinding_t first = { 7, 10, 42, SIR_INPUT_NO_CHORD, 0, 0, 32767,
	                                       SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE };
	const sirInputActionBinding_t second = { 8, 11, 43, SIR_INPUT_NO_CHORD, 1, 0, 32767,
	                                        SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE };
	if ( !input.actions().addBinding( first ) || !input.actions().addBinding( second ) ) {
		return false;
	}
	const sirInputReplayKey_t szKeys[] = { { 7, 0 }, { 8, 1 } };
	uint8_t szTape[4096] = {};
	SIRInputReplayTape writer;
	if ( !writer.beginRecord( szTape, sizeof( szTape ) ) || !input.begin( 21 ) ) {
		return false;
	}
	sirInputDeviceId_t hFirst = {};
	sirInputDeviceId_t hSecond = {};
	if ( !input.connect( { 0, 100, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &hFirst ) ||
	     !input.connect( { 0, 101, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &hSecond ) ||
	     !input.pairUser( hSecond, 1 ) ) {
		return false;
	}
	const sirInputControlChange_t szTap[] = { { SIR_INPUT_BUTTON, 42, 1 }, { SIR_INPUT_BUTTON, 42, 0 } };
	const sirInputControlChange_t szHeld = { SIR_INPUT_BUTTON, 43, 1 };
	if ( !input.emitControls( SIR_INPUT_TEST, hFirst, szTap, 2 ) ||
	     !input.emitControls( SIR_INPUT_TEST, hSecond, &szHeld, 1 ) ||
	     !input.publish() || !input.recordReplayTick( writer, szKeys, 2 ) ) {
		return false;
	}
	const uint32_t nFirstBytes = writer.bytesUsed();
	if ( nFirstBytes != SIR_INPUT_REPLAY_HEADER_BYTES + 2 * SIR_INPUT_REPLAY_RECORD_BYTES ||
	     input.recordReplayTick( writer, szKeys, 2 ) ) {
		return false;
	}
	const sirInputReplayKey_t szDuplicate[] = { { 7, 0 }, { 7, 0 } };
	if ( writer.append( 22, input.actions(), szDuplicate, 2 ) || writer.bytesUsed() != nFirstBytes ) {
		return false;
	}
	if ( !input.begin( 22 ) || input.recordReplayTick( writer, szKeys, 2 ) ) {
		return false;
	}
	const sirInputControlChange_t szRelease = { SIR_INPUT_BUTTON, 43, 0 };
	if ( !input.emitControls( SIR_INPUT_TEST, hSecond, &szRelease, 1 ) ||
	     !input.publish() || !input.recordReplayTick( writer, szKeys, 2 ) ) {
		return false;
	}

	const uint32_t nTapeBytes = writer.bytesUsed();
	if ( szTape[0] != 'S' || szTape[1] != 'I' || szTape[2] != 'R' || szTape[3] != 'P' ) {
		return false;
	}
	SIRInputReplayTape reader;
	sirInputReplayFrame_t frame = {};
	sirInputActionState_t state = {};
	if ( !reader.beginRead( szTape, nTapeBytes ) || !reader.next( &frame ) ||
	     frame.nTick != 21 || frame.nCount != 2 ||
	     !SIRInputReplayTape::query( frame, 7, 0, &state ) || state.bHeld ||
	     !state.bPressed || !state.bReleased || state.nTransitions != 2 ) {
		return false;
	}
	if ( !SIRInputReplayTape::query( frame, 8, 1, &state ) || state.nValue != 32767 || !state.bHeld ) {
		return false;
	}
	if ( !reader.next( &frame ) || frame.nTick != 22 || !SIRInputReplayTape::query( frame, 8, 1, &state ) ||
	     state.bHeld || !state.bReleased || !reader.finished() || reader.next( &frame ) ) {
		return false;
	}
	if ( !reader.seek( 21, &frame ) || frame.nTick != 21 || !reader.next( &frame ) || frame.nTick != 22 ) {
		return false;
	}
	uint8_t szCorrupt[4096] = {};
	memcpy( szCorrupt, szTape, nTapeBytes );
	szCorrupt[nFirstBytes - 1] ^= 0x40;
	frame.nTick = 999;
	if ( !reader.beginRead( szCorrupt, nTapeBytes ) || reader.next( &frame ) || frame.nTick != 999 ||
	     !reader.beginRead( szTape, nTapeBytes - 1 ) || !reader.next( &frame ) || reader.next( &frame ) ) {
		return false;
	}
	input.shutdown();
	return true;
}

static bool sirTestReplayInvalidRecord( void ) noexcept
{
	SIRInputReplayTape writer;
	uint8_t szBytes[SIR_INPUT_REPLAY_MAX_FRAME_BYTES] = {};
	SIRInputService input;
	if ( writer.beginRecord( szBytes, SIR_INPUT_REPLAY_HEADER_BYTES - 1 ) ||
	     !writer.beginRecord( szBytes, sizeof( szBytes ) ) ||
	     writer.append( 0, input.actions(), nullptr, 0 ) ||
	     !writer.append( 7, input.actions(), nullptr, 0 ) ||
	     writer.append( 7, input.actions(), nullptr, 0 ) ) {
		return false;
	}
	SIRInputReplayTape reader;
	if ( !reader.beginRead( szBytes, writer.bytesUsed() ) ) {
		return false;
	}
	sirInputReplayFrame_t frame = {};
	if ( !reader.next( &frame ) || frame.nTick != 7 || frame.nCount != 0 || !reader.finished() ||
	     SIRInputReplayTape::query( frame, 1, 0, nullptr ) ) {
		return false;
	}
	return true;
}

static bool sirTestAuthoritativePlayback( void ) noexcept
{
	SIRInputService playback;
	if ( !playback.startReplay() || playback.startReplay() || !playback.replayActive() ||
	     !playback.begin( 21 ) || playback.publish() ) {
		return false;
	}
	sirInputReplayFrame_t first = {};
	first.nTick = 21;
	first.nCount = 2;
	first.actions[0] = { 7, 0, 0, 2, false, true, true };
	first.actions[1] = { 8, 1, 32767, 1, true, true, false };
	sirInputReplayFrame_t invalid = first;
	invalid.actions[0].bHeld = true;
	if ( playback.queueReplayTick( invalid ) || !playback.queueReplayTick( first ) ||
	     playback.queueReplayTick( first ) || !playback.publish() ) {
		return false;
	}
	sirInputActionState_t state = {};
	if ( !playback.queryAction( 7, 0, &state ) || state.nTransitions != 2 || !state.bPressed || !state.bReleased ||
	     !playback.queryAction( 8, 1, &state ) || !state.bHeld || state.nValue != 32767 ) {
		return false;
	}
	uint8_t szBuffer[128] = {};
	SIRInputReplayTape writer;
	const sirInputReplayKey_t key = { 7, 0 };
	if ( !writer.beginRecord( szBuffer, sizeof( szBuffer ) ) ||
	     playback.recordReplayTick( writer, &key, 1 ) || !playback.begin( 22 ) ||
	     playback.queryAction( 7, 0, &state ) ) {
		return false;
	}
	invalid = first;
	if ( playback.queueReplayTick( invalid ) || playback.publish() ) {
		return false;
	}
	sirInputReplayFrame_t second = {};
	second.nTick = 22;
	second.nCount = 1;
	second.actions[0] = { 8, 1, 0, 1, false, false, true };
	if ( !playback.queueReplayTick( second ) || !playback.publish() ||
	     playback.queryAction( 7, 0, &state ) || !playback.queryAction( 8, 1, &state ) ||
	     state.bHeld || !state.bReleased ) {
		return false;
	}
	playback.stopReplay();
	if ( playback.replayActive() || playback.queryAction( 8, 1, &state ) ) {
		return false;
	}
	playback.shutdown();
	return true;
}

int main( void )
{
	if ( !sirTestReplayServiceTick() || !sirTestReplayInvalidRecord() ||
	     !sirTestAuthoritativePlayback() ) {
		fputs( "SIRInputReplayTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputReplayTests PASS" );
	return 0;
}
