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

class SIRInputServiceBoundaryBackend final : public SIRInputBackend
{
public:
	explicit SIRInputServiceBoundaryBackend( sirInputBackendId_t eBackend = SIR_INPUT_TEST ) noexcept : m_eBackend( eBackend )
	{
	}

	sirInputBackendId_t id( void ) const noexcept override
	{
		return m_eBackend;
	}

	void sample( SIRInputService &input ) noexcept override
	{
		( void )input;
		++m_nSamples;
	}

	void resynchronize( SIRInputService &input ) noexcept override
	{
		( void )input;
		++m_nResyncs;
	}

	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )hDevice;
		m_nLastLeft = nLeft;
		m_nLastRight = nRight;
		++m_nVibrations;
		return true;
	}

	uint32_t hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept override
	{
		( void )hDevice;
		return SIR_INPUT_HAPTIC_CAP_RUMBLE | SIR_INPUT_HAPTIC_CAP_TRIGGER_RUMBLE;
	}

	bool vibrateTriggers( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )hDevice;
		m_nTriggerLeft = nLeft;
		m_nTriggerRight = nRight;
		++m_nTriggerCalls;
		return true;
	}

	void stop( SIRInputService &input ) noexcept override
	{
		( void )input;
		++m_nStops;
	}

	sirInputBackendId_t m_eBackend;
	uint32_t m_nSamples = 0;
	uint32_t m_nResyncs = 0;
	uint32_t m_nVibrations = 0;
	uint32_t m_nTriggerCalls = 0;
	uint32_t m_nStops = 0;
	uint16_t m_nLastLeft = 0;
	uint16_t m_nLastRight = 0;
	uint16_t m_nTriggerLeft = 0;
	uint16_t m_nTriggerRight = 0;
};

static bool sirTestServiceBackendOwnership( void ) noexcept
{
	SIRInputService input;
	SIRInputServiceBoundaryBackend invalid( ( sirInputBackendId_t )0 );
	SIRInputServiceBoundaryBackend backends[SIR_INPUT_BACKEND_CAPACITY + 1];
	if ( input.attach( invalid ) || input.frame() != nullptr || input.activeDeviceCount() != 0 || !input.begin( 1 ) ||
	     input.attach( backends[0] ) || input.connect( { 1, 1, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, nullptr ) || !input.publish() ||
	     !input.attach( backends[0] ) || input.attach( backends[0] ) ) {
		return false;
	}
	for ( uint32_t i = 1; i < SIR_INPUT_BACKEND_CAPACITY; ++i ) {
		backends[i].m_eBackend = ( sirInputBackendId_t )( i + 1 );
		if ( !input.attach( backends[i] ) ) {
			return false;
		}
	}
	backends[SIR_INPUT_BACKEND_CAPACITY].m_eBackend = SIR_INPUT_LINUX_EVDEV;
	if ( input.attach( backends[SIR_INPUT_BACKEND_CAPACITY] ) || !input.begin( 2 ) ) {
		return false;
	}
	input.sample();
	for ( uint32_t i = 0; i < SIR_INPUT_BACKEND_CAPACITY; ++i ) {
		if ( backends[i].m_nSamples != 1 ) {
			return false;
		}
	}
	if ( !input.publish() ) {
		return false;
	}
	input.shutdown();
	input.shutdown();
	for ( uint32_t i = 0; i < SIR_INPUT_BACKEND_CAPACITY; ++i ) {
		if ( backends[i].m_nStops != 1 ) {
			return false;
		}
	}
	return !input.begin( 3 ) && !input.attach( invalid ) && input.frame() == nullptr && input.activeDeviceCount() == 0;
}

static bool sirTestServiceDeviceAndRumble( void ) noexcept
{
	SIRInputService input;
	SIRInputServiceBoundaryBackend backend;
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		return false;
	}
	const sirInputDeviceId_t invalid = {};
	sirInputDeviceId_t hPad = {};
	sirInputDeviceId_t hPointer = {};
	const sirInputHardwareKey_t padKey = { 100, 10, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER };
	const sirInputHardwareKey_t pointerKey = { 200, 20, SIR_INPUT_TEST, SIR_INPUT_POINTER };
	sirInputDeviceDescriptor_t descriptor = {};
	if ( input.connect( { 1, 1, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &hPad ) || input.connect( padKey, nullptr ) ||
	     !input.connect( padKey, &hPad ) || !input.connect( pointerKey, &hPointer ) || !input.findDevice( SIR_INPUT_TEST, 10, &hPad ) ||
	     !input.describe( hPad, &descriptor ) || input.describe( invalid, &descriptor ) || input.findDevice( SIR_INPUT_TEST, 404, &hPad ) ||
	     !input.pairUser( hPad, 0 ) || input.pairUser( invalid, 0 ) || input.activeDeviceCount() != 2 ) {
		return false;
	}
	const sirInputControlChange_t button = { SIR_INPUT_BUTTON, 0, 1 };
	if ( input.emitControls( SIR_INPUT_LINUX_EVDEV, hPad, &button, 1 ) || input.emitControls( SIR_INPUT_TEST, invalid, &button, 1 ) ||
	     !input.emitControls( SIR_INPUT_TEST, hPad, &button, 1, 1999, SIR_INPUT_CLOCK_ENGINE_MONOTONIC ) ||
	     input.emitPointer( SIR_INPUT_LINUX_EVDEV, hPointer, SIR_INPUT_MOTION, 0, 1, 1 ) ||
	     !input.emitPointer( SIR_INPUT_TEST, hPointer, SIR_INPUT_MOTION, 0, 20, -10, 1234, SIR_INPUT_CLOCK_EVDEV_MONOTONIC ) ||
	     input.loseState( SIR_INPUT_LINUX_EVDEV, hPad ) || input.loseBackendState( SIR_INPUT_LINUX_EVDEV ) ) {
		return false;
	}
	if ( input.hapticCapabilities( invalid ) != SIR_INPUT_HAPTIC_CAP_NONE ||
	     input.hapticCapabilities( hPointer ) != SIR_INPUT_HAPTIC_CAP_NONE || input.vibrate( hPointer, 10, 20 ) ||
	     input.vibrateTriggers( hPointer, 1, 2 ) || !input.vibrate( hPad, 10, 20 ) || backend.m_nLastLeft != 10 ||
	     backend.m_nLastRight != 20 || !input.vibrateTriggers( hPad, 30, 40 ) || backend.m_nTriggerLeft != 30 ||
	     backend.m_nTriggerRight != 40 ) {
		return false;
	}
	input.setHapticsPaused( true );
	if ( backend.m_nLastLeft != 0 || backend.m_nTriggerLeft != 0 || input.vibrate( hPad, 1, 0 ) || input.vibrateTriggers( hPad, 1, 0 ) ||
	     !input.vibrate( hPad, 0, 0 ) || !input.vibrateTriggers( hPad, 0, 0 ) ) {
		return false;
	}
	input.setHapticsPaused( false );
	if ( !input.loseBackendState( SIR_INPUT_TEST ) || !input.publish() || !input.frame() || input.frame()->nResyncCount != 2 ||
	     backend.m_nSamples != 0 ) {
		return false;
	}
	if ( !input.begin( 2 ) || input.disconnect( SIR_INPUT_LINUX_EVDEV, hPad ) || !input.disconnect( SIR_INPUT_TEST, hPad ) ||
	     input.describe( hPad, &descriptor ) || input.vibrate( hPad, 10, 0 ) || !input.disconnect( SIR_INPUT_TEST, hPointer ) ||
	     !input.publish() || input.activeDeviceCount() != 0 ) {
		return false;
	}
	input.shutdown();
	return backend.m_nStops == 1 && backend.m_nTriggerCalls >= 2;
}

static bool sirTestServiceReplayAndHistory( void ) noexcept
{
	SIRInputService input;
	SIRInputHistory history;
	sirInputReplayFrame_t storage[3] = {};
	if ( !history.initialize( storage, 3 ) || !input.startReplay() || !input.replayActive() || input.startReplay() || input.publish() ) {
		return false;
	}
	sirInputActionState_t state = {};
	if ( input.queryAction( 1, 0, &state ) || !input.begin( 1 ) || input.startReplay() || input.queueHistoryTick( history, 2 ) ||
	     input.queueHistoryTick( history, 1 ) || input.publish() ) {
		return false;
	}
	sirInputReplayFrame_t frame = {};
	frame.nTick = 2;
	if ( input.queueReplayTick( frame ) ) {
		return false;
	}
	frame.nTick = 1;
	SIRInputReplayTape rejectedTape;
	if ( !input.queueReplayTick( frame ) || input.queueReplayTick( frame ) || !input.publish() || input.queryAction( 1, 0, &state ) ||
	     input.recordReplayTick( rejectedTape, nullptr, 0 ) ) {
		return false;
	}
	input.stopReplay();
	if ( input.replayActive() || !input.captureHistoryTick( history, nullptr, 0 ) || history.count() != 1 ) {
		return false;
	}
	if ( !input.startReplay() || !input.begin( 2 ) || input.queueHistoryTick( history, 2 ) ||
	     input.captureHistoryTick( history, nullptr, 0 ) ) {
		return false;
	}
	frame.nTick = 2;
	if ( !history.append( frame ) || !input.queueHistoryTick( history, 2 ) || !input.publish() || input.queryAction( 2, 0, &state ) ) {
		return false;
	}
	input.stopReplay();
	uint8_t buffer[256] = {};
	SIRInputReplayTape tape;
	if ( !tape.beginRecord( buffer, sizeof( buffer ) ) || !input.recordReplayTick( tape, nullptr, 0 ) || !input.begin( 3 ) ||
	     input.recordReplayTick( tape, nullptr, 0 ) || !input.publish() || !input.captureHistoryTick( history, nullptr, 0 ) ||
	     history.count() != 3 ) {
		return false;
	}
	input.shutdown();
	return !input.startReplay() && !input.captureHistoryTick( history, nullptr, 0 );
}

int main( void )
{
	if ( !sirTestServiceBackendOwnership() ) {
		fputs( "SIRInputServiceBoundaryTests backend ownership FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestServiceDeviceAndRumble() ) {
		fputs( "SIRInputServiceBoundaryTests device and rumble FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestServiceReplayAndHistory() ) {
		fputs( "SIRInputServiceBoundaryTests replay and history FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputServiceBoundaryTests PASS" );
	return 0;
}
