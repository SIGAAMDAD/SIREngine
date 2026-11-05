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
	if ( !input.attach( backend ) || !input.actions().addContext( 10, 0, 100, true ) || !input.actions().addAction( 7, 0 ) ||
	     !input.actions().addContext( 11, 1, 100, true ) || !input.actions().addAction( 8, 1 ) ) {
		return false;
	}
	const sirInputActionBinding_t first = {
	    7, 10, 42, SIR_INPUT_NO_CHORD, 0, 0, 32767, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE };
	const sirInputActionBinding_t second = {
	    8, 11, 43, SIR_INPUT_NO_CHORD, 1, 0, 32767, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE };
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
	     !input.connect( { 0, 101, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &hSecond ) || !input.pairUser( hSecond, 1 ) ) {
		return false;
	}
	const sirInputControlChange_t szTap[] = { { SIR_INPUT_BUTTON, 42, 1 }, { SIR_INPUT_BUTTON, 42, 0 } };
	const sirInputControlChange_t szHeld = { SIR_INPUT_BUTTON, 43, 1 };
	if ( !input.emitControls( SIR_INPUT_TEST, hFirst, szTap, 2 ) || !input.emitControls( SIR_INPUT_TEST, hSecond, &szHeld, 1 ) ||
	     !input.publish() || !input.recordReplayTick( writer, szKeys, 2 ) ) {
		return false;
	}
	const uint32_t nFirstBytes = writer.bytesUsed();
	if ( nFirstBytes != SIR_INPUT_REPLAY_HEADER_BYTES + 2 * SIR_INPUT_REPLAY_RECORD_BYTES || input.recordReplayTick( writer, szKeys, 2 ) ) {
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
	if ( !input.emitControls( SIR_INPUT_TEST, hSecond, &szRelease, 1 ) || !input.publish() ||
	     !input.recordReplayTick( writer, szKeys, 2 ) ) {
		return false;
	}

	const uint32_t nTapeBytes = writer.bytesUsed();
	if ( szTape[0] != 'S' || szTape[1] != 'I' || szTape[2] != 'R' || szTape[3] != 'P' ) {
		return false;
	}
	SIRInputReplayTape reader;
	sirInputReplayFrame_t frame = {};
	sirInputActionState_t state = {};
	if ( !reader.beginRead( szTape, nTapeBytes ) || !reader.next( &frame ) || frame.nTick != 21 || frame.nCount != 2 ||
	     !SIRInputReplayTape::query( frame, 7, 0, &state ) || state.bHeld || !state.bPressed || !state.bReleased ||
	     state.nTransitions != 2 ) {
		return false;
	}
	if ( !SIRInputReplayTape::query( frame, 8, 1, &state ) || state.nValue != 32767 || !state.bHeld ) {
		return false;
	}
	if ( !reader.next( &frame ) || frame.nTick != 22 || !SIRInputReplayTape::query( frame, 8, 1, &state ) || state.bHeld ||
	     !state.bReleased || !reader.finished() || reader.next( &frame ) ) {
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
	if ( writer.beginRecord( szBytes, SIR_INPUT_REPLAY_HEADER_BYTES - 1 ) || !writer.beginRecord( szBytes, sizeof( szBytes ) ) ||
	     writer.append( 0, input.actions(), nullptr, 0 ) || !writer.append( 7, input.actions(), nullptr, 0 ) ||
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
	if ( !playback.startReplay() || playback.startReplay() || !playback.replayActive() || !playback.begin( 21 ) || playback.publish() ) {
		return false;
	}
	sirInputReplayFrame_t first = {};
	first.nTick = 21;
	first.nCount = 2;
	first.actions[0] = { 7, 0, 0, 2, false, true, true };
	first.actions[1] = { 8, 1, 32767, 1, true, true, false };
	sirInputReplayFrame_t invalid = first;
	invalid.actions[0].bHeld = true;
	if ( playback.queueReplayTick( invalid ) || !playback.queueReplayTick( first ) || playback.queueReplayTick( first ) ||
	     !playback.publish() ) {
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
	if ( !writer.beginRecord( szBuffer, sizeof( szBuffer ) ) || playback.recordReplayTick( writer, &key, 1 ) || !playback.begin( 22 ) ||
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
	if ( !playback.queueReplayTick( second ) || !playback.publish() || playback.queryAction( 7, 0, &state ) ||
	     !playback.queryAction( 8, 1, &state ) || state.bHeld || !state.bReleased ) {
		return false;
	}
	playback.stopReplay();
	if ( playback.replayActive() || playback.queryAction( 8, 1, &state ) ) {
		return false;
	}
	playback.shutdown();
	return true;
}

static bool sirTestHistoryServiceCapture( void ) noexcept
{
	sirInputReplayFrame_t szStorage[4] = {};
	SIRInputHistory history;
	SIRInputService input;
	SIRReplayTestBackend backend;
	if ( !history.initialize( szStorage, 4 ) || !input.attach( backend ) || !input.actions().addContext( 31, 0, 10, true ) ||
	     !input.actions().addAction( 91, 0 ) ) {
		return false;
	}
	const sirInputActionBinding_t binding = {
	    91, 31, 66, SIR_INPUT_NO_CHORD, 0, 0, 32767, SIR_INPUT_KEYBOARD, SIR_INPUT_BUTTON, SIR_INPUT_BIND_NONE };
	if ( !input.actions().addBinding( binding ) || !input.begin( 51 ) ) {
		return false;
	}
	sirInputDeviceId_t hKeyboard = {};
	const sirInputControlChange_t pressed = { SIR_INPUT_BUTTON, 66, 1 };
	const sirInputReplayKey_t key = { 91, 0 };
	if ( !input.connect( { 0, 801, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &hKeyboard ) ||
	     !input.emitControls( SIR_INPUT_TEST, hKeyboard, &pressed, 1 ) || input.captureHistoryTick( history, &key, 1 ) ||
	     !input.publish() || !input.captureHistoryTick( history, &key, 1 ) || history.count() != 1 ) {
		return false;
	}
	const sirInputControlChange_t released = { SIR_INPUT_BUTTON, 66, 0 };
	if ( !input.begin( 52 ) || !input.emitControls( SIR_INPUT_TEST, hKeyboard, &released, 1 ) || !input.publish() ||
	     !input.captureHistoryTick( history, &key, 1 ) || history.count() != 2 ) {
		return false;
	}
	SIRInputService playback;
	if ( !playback.startReplay() || !playback.begin( 51 ) || playback.queueHistoryTick( history, 52 ) ||
	     !playback.queueHistoryTick( history, 51 ) || !playback.publish() ) {
		return false;
	}
	sirInputActionState_t state = {};
	if ( !playback.queryAction( 91, 0, &state ) || !state.bHeld || !state.bPressed || !playback.begin( 52 ) ||
	     !playback.queueHistoryTick( history, 52 ) || !playback.publish() || !playback.queryAction( 91, 0, &state ) || state.bHeld ||
	     !state.bReleased ) {
		return false;
	}
	input.shutdown();
	playback.shutdown();
	return true;
}

static void sirReplayWrite32( uint8_t *pData, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pData[i] = ( uint8_t )( nValue >> ( 8 * i ) );
	}
}

static uint32_t sirReplayChecksum( const uint8_t *pData, uint32_t nSize ) noexcept
{
	uint32_t nHash = UINT32_C( 2166136261 );
	for ( uint32_t i = 0; i < nSize; ++i ) {
		nHash = ( nHash ^ pData[i] ) * UINT32_C( 16777619 );
	}
	return nHash;
}

static bool sirReplayRejectsPacket( const uint8_t *pData, uint32_t nSize ) noexcept
{
	SIRInputReplayTape reader;
	sirInputReplayFrame_t frame = {};
	frame.nTick = 9999;
	return reader.beginRead( pData, nSize ) && !reader.next( &frame ) && reader.bytesUsed() == 0 && frame.nTick == 9999;
}

static bool sirTestReplayMalformedHeaders( void ) noexcept
{
	SIRInputReplayTape writer;
	SIRInputActions actions;
	uint8_t szBytes[128] = {};
	if ( writer.beginRecord( nullptr, sizeof( szBytes ) ) || !writer.beginRecord( szBytes, sizeof( szBytes ) ) ||
	     !writer.append( 10, actions, nullptr, 0 ) || !writer.append( 11, actions, nullptr, 0 ) || writer.bytesUsed() != 64 ) {
		return false;
	}

	SIRInputReplayTape reader;
	sirInputReplayFrame_t frame = {};
	if ( reader.beginRead( nullptr, sizeof( szBytes ) ) || !reader.beginRead( szBytes, 0 ) || reader.next( &frame ) ||
	     !reader.beginRead( szBytes, 64 ) || reader.next( nullptr ) || !reader.next( &frame ) || frame.nTick != 10 ||
	     !reader.next( &frame ) || frame.nTick != 11 || !reader.finished() || !reader.seek( 11, &frame ) || frame.nTick != 11 ||
	     reader.seek( 12, &frame ) || reader.seek( 1, &frame ) ) {
		return false;
	}
	reader.reset();
	if ( reader.seek( 10, &frame ) || reader.seek( 10, nullptr ) ) {
		return false;
	}
	const uint32_t szOffsets[] = { 0, 4, 6, 8, 16, 20, 24, 28 };
	for ( uint32_t i = 0; i < sizeof( szOffsets ) / sizeof( szOffsets[0] ); ++i ) {
		uint8_t szCorrupt[64] = {};
		memcpy( szCorrupt, szBytes, sizeof( szCorrupt ) );
		const uint32_t nOffset = szOffsets[i];
		if ( nOffset == 8 ) {
			szCorrupt[nOffset] = 0;
		} else if ( nOffset == 24 ) {
			szCorrupt[nOffset] ^= 0x80;
		} else {
			szCorrupt[nOffset] ^= 0xff;
		}
		if ( !sirReplayRejectsPacket( szCorrupt, sizeof( szCorrupt ) ) ) {
			return false;
		}
	}
	uint8_t szDuplicate[64] = {};
	memcpy( szDuplicate, szBytes, sizeof( szDuplicate ) );
	szDuplicate[32 + 8] = 10;
	if ( !reader.beginRead( szDuplicate, sizeof( szDuplicate ) ) || !reader.next( &frame ) || reader.next( &frame ) ) {
		return false;
	}
	return frame.nTick == 10;
}

static bool sirTestReplayMalformedActionRecords( void ) noexcept
{
	SIRInputActions actions;
	const sirInputReplayKey_t key = { 10, 0 };
	if ( !actions.addAction( key.nActionId, key.nUser ) ) {
		return false;
	}
	uint8_t szBytes[128] = {};
	SIRInputReplayTape writer;
	if ( !writer.beginRecord( szBytes, sizeof( szBytes ) ) || !writer.append( 100, actions, &key, 1 ) ||
	     writer.bytesUsed() != SIR_INPUT_REPLAY_HEADER_BYTES + SIR_INPUT_REPLAY_RECORD_BYTES ) {
		return false;
	}
	const uint32_t nBytes = writer.bytesUsed();
	SIRInputReplayTape reader;
	sirInputReplayFrame_t frame = {};
	if ( !reader.beginRead( szBytes, nBytes ) || !reader.next( &frame ) || frame.nTick != 100 || frame.nCount != 1 ||
	     frame.actions[0].nActionId != 10 || frame.actions[0].bHeld ) {
		return false;
	}
	const uint32_t szOffsets[] = { 32, 36, 40, 48, 48 };
	const uint32_t szValues[] = { 0, SIR_INPUT_USER_CAPACITY, 0x80000000U, 0x80, 1 };
	for ( uint32_t i = 0; i < sizeof( szOffsets ) / sizeof( szOffsets[0] ); ++i ) {
		uint8_t szCorrupt[128] = {};
		memcpy( szCorrupt, szBytes, nBytes );
		sirReplayWrite32( szCorrupt + szOffsets[i], szValues[i] );
		sirReplayWrite32( szCorrupt + 24,
		                  sirReplayChecksum( szCorrupt + SIR_INPUT_REPLAY_HEADER_BYTES, nBytes - SIR_INPUT_REPLAY_HEADER_BYTES ) );
		if ( !sirReplayRejectsPacket( szCorrupt, nBytes ) ) {
			return false;
		}
	}
	return !SIRInputReplayTape::validateFrame( { 0, 0, {} } );
}

static bool sirTestReplayTapeValidationBoundaries( void ) noexcept
{
	SIRInputActions actions;
	const sirInputReplayKey_t key = { 7, 0 };
	const sirInputReplayKey_t missing = { 9, 0 };
	if ( !actions.addAction( 7, 0 ) ) {
		return false;
	}
	uint8_t szSmall[64] = {};
	SIRInputReplayTape writer;
	if ( !writer.beginRecord( szSmall, sizeof( szSmall ) ) || !writer.append( 1, actions, &key, 1 ) ||
	     writer.bytesUsed() != SIR_INPUT_REPLAY_HEADER_BYTES + SIR_INPUT_REPLAY_RECORD_BYTES || writer.append( 2, actions, &key, 1 ) ||
	     writer.bytesUsed() != SIR_INPUT_REPLAY_HEADER_BYTES + SIR_INPUT_REPLAY_RECORD_BYTES ) {
		return false;
	}
	uint8_t szLarge[256] = {};
	if ( !writer.beginRecord( szLarge, sizeof( szLarge ) ) || writer.append( 1, actions, &missing, 1 ) || writer.bytesUsed() != 0 ||
	     !writer.append( 1, actions, &key, 1 ) ) {
		return false;
	}
	sirInputReplayFrame_t frame = {};
	frame.nTick = 1;
	frame.nCount = 2;
	frame.actions[0] = { 7, 0, 0, 0, false, false, false };
	frame.actions[1] = { 7, 0, 0, 0, false, false, false };
	if ( SIRInputReplayTape::validateFrame( frame ) || SIRInputReplayTape::query( frame, 7, 0, nullptr ) ) {
		return false;
	}
	frame.actions[1].nUser = 1;
	if ( !SIRInputReplayTape::validateFrame( frame ) ) {
		return false;
	}
	frame.actions[0].nValue = 32768;
	frame.actions[0].bHeld = true;
	if ( SIRInputReplayTape::validateFrame( frame ) ) {
		return false;
	}
	frame.actions[0].nValue = -32768;
	if ( SIRInputReplayTape::validateFrame( frame ) ) {
		return false;
	}
	frame.actions[0].nValue = 0;
	frame.actions[0].bHeld = false;
	sirInputActionState_t output = {};
	if ( !SIRInputReplayTape::validateFrame( frame ) || !SIRInputReplayTape::query( frame, 7, 1, &output ) || output.nUser != 1 ||
	     SIRInputReplayTape::query( frame, 7, 2, &output ) ) {
		return false;
	}
	frame.nCount = SIR_INPUT_ACTION_CAPACITY + 1;
	return !SIRInputReplayTape::query( frame, 7, 0, &output ) && !SIRInputReplayTape::validateFrame( frame );
}

static bool sirTestReplayModeAndMultiuserBoundaries( void ) noexcept
{
	SIRInputReplayTape writer;
	if ( writer.finished() || writer.bytesUsed() != 0 ) {
		return false;
	}
	writer.rewind();
	SIRInputActions actions;
	const sirInputReplayKey_t keys[] = { { 7, 0 }, { 7, 1 } };
	if ( !actions.addAction( 7, 0 ) || !actions.addAction( 7, 1 ) ) {
		return false;
	}
	uint8_t buffer[256] = {};
	if ( !writer.beginRecord( buffer, sizeof( buffer ) ) || writer.finished() || writer.append( 1, actions, nullptr, 1 ) ||
	     writer.append( 1, actions, keys, SIR_INPUT_ACTION_CAPACITY + 1 ) || !writer.append( 1, actions, keys, 2 ) ||
	     writer.bytesUsed() != SIR_INPUT_REPLAY_HEADER_BYTES + 2 * SIR_INPUT_REPLAY_RECORD_BYTES ) {
		return false;
	}
	const uint32_t nBytes = writer.bytesUsed();
	writer.rewind();
	if ( writer.bytesUsed() != nBytes || writer.finished() ) {
		return false;
	}
	SIRInputReplayTape reader;
	sirInputReplayFrame_t frame = {};
	if ( reader.next( &frame ) || reader.seek( 1, &frame ) || !reader.beginRead( buffer, nBytes ) || reader.finished() ||
	     reader.seek( 1, nullptr ) || !reader.next( &frame ) || frame.nCount != 2 || frame.actions[0].nUser != 0 ||
	     frame.actions[1].nUser != 1 || !reader.finished() || reader.next( &frame ) ) {
		return false;
	}
	reader.rewind();
	if ( reader.finished() || reader.next( nullptr ) || !reader.seek( 1, &frame ) || !reader.finished() ) {
		return false;
	}
	reader.reset();
	return !reader.finished() && !reader.next( &frame ) && !reader.seek( 1, &frame ) && reader.bytesUsed() == 0;
}

static bool sirTestReplayRejectsOutOfRangeActionState( void ) noexcept
{
	SIRInputActions actions;
	if ( !actions.addAction( 77, 0 ) || !actions.addContext( 33, 0, 1, true ) ) {
		return false;
	}
	const sirInputActionBinding_t binding = {
	    77, 33, 0, SIR_INPUT_NO_CHORD, 0, 0, 32767, SIR_INPUT_CONTROLLER, SIR_INPUT_AXIS, SIR_INPUT_BIND_NONE };
	if ( !actions.addBinding( binding ) ) {
		return false;
	}
	// Deliberately bypass the physical collector's Q15 checks to prove that
	// the replay writer rejects a malformed logical action before serialization.
	sirInputFrame_t frame = {};
	frame.nTick = 1;
	frame.nCount = 1;
	frame.devices[0] = { 0, 1 };
	frame.events[0].hDevice = { 0, 1 };
	frame.events[0].eClass = SIR_INPUT_CONTROLLER;
	frame.events[0].eSignal = SIR_INPUT_CONNECTED;
	frame.nConnected[0] = 1;
	frame.nUsers[0] = 0;
	frame.state[0].nAxes[0] = 65535;
	actions.evaluate( frame );
	sirInputActionState_t action = {};
	if ( !actions.query( 77, 0, &action ) || action.nValue != 65535 || !action.bHeld ) {
		return false;
	}
	const sirInputReplayKey_t key = { 77, 0 };
	uint8_t bytes[128] = {};
	SIRInputReplayTape writer;
	return writer.beginRecord( bytes, sizeof( bytes ) ) && !writer.append( 1, actions, &key, 1 ) && writer.bytesUsed() == 0 &&
	       bytes[0] == 0;
}

int main( void )
{
	if ( !sirTestReplayServiceTick() || !sirTestReplayInvalidRecord() || !sirTestAuthoritativePlayback() ||
	     !sirTestHistoryServiceCapture() || !sirTestReplayMalformedHeaders() || !sirTestReplayMalformedActionRecords() ||
	     !sirTestReplayTapeValidationBoundaries() || !sirTestReplayModeAndMultiuserBoundaries() ||
	     !sirTestReplayRejectsOutOfRangeActionState() ) {
		fputs( "SIRInputReplayTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputReplayTests PASS" );
	return 0;
}
