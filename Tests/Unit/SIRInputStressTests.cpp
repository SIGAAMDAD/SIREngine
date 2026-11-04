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
#include <Core/Platform/Input/SIRInputControllerProfiles.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

class SIRInputStressBackend final : public SIRInputBackend
{
public:
	explicit SIRInputStressBackend( sirInputBackendId_t eBackend ) noexcept
	    : m_nResyncs( 0 ), m_nStops( 0 ), m_nVibrations( 0 ), m_eBackend( eBackend )
	{
	}

	sirInputBackendId_t id( void ) const noexcept override
	{
		return m_eBackend;
	}

	void sample( SIRInputService &service ) noexcept override
	{
		( void )service;
	}

	void resynchronize( SIRInputService &service ) noexcept override
	{
		( void )service;
		++m_nResyncs;
	}

	bool vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )id;
		( void )nLeft;
		( void )nRight;
		++m_nVibrations;
		return true;
	}

	void stop( SIRInputService &service ) noexcept override
	{
		( void )service;
		++m_nStops;
	}

	uint32_t m_nResyncs;
	uint32_t m_nStops;
	uint32_t m_nVibrations;

private:
	sirInputBackendId_t m_eBackend;
};

static uint32_t randomInput( uint32_t &nSeed ) noexcept
{
	nSeed ^= nSeed << 13;
	nSeed ^= nSeed >> 17;
	nSeed ^= nSeed << 5;
	return nSeed;
}

static bool testCapacityAndIdentity( void ) noexcept
{
	SIRInputService input;
	SIRInputStressBackend backend( SIR_INPUT_TEST );
	SIRInputStressBackend other( SIR_INPUT_LINUX_EVDEV );
	if ( !input.attach( backend ) || !input.attach( other ) || !input.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t devices[SIR_INPUT_DEVICE_CAPACITY] = {};
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		const sirInputHardwareKey_t key = { 1000 + i, 2000 + i, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER };
		if ( !input.connect( key, &devices[i] ) || devices[i].nSlot >= SIR_INPUT_DEVICE_CAPACITY ||
		     !input.pairUser( devices[i], i % SIR_INPUT_USER_CAPACITY ) ) {
			return false;
		}
	}
	sirInputDeviceId_t extra = {};
	if ( input.connect( { 9000, 9000, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &extra ) ||
	     input.connect( { 1000, 9001, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &extra ) ||
	     input.activeDeviceCount() != SIR_INPUT_DEVICE_CAPACITY ) {
		return false;
	}
	if ( !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *pFrame = input.frame();
	if ( pFrame == nullptr || pFrame->nCount != SIR_INPUT_DEVICE_CAPACITY ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( pFrame->nConnected[i] != 1 || pFrame->nUsers[i] != i % SIR_INPUT_USER_CAPACITY ||
		     pFrame->devices[i].nGeneration != devices[i].nGeneration ) {
			return false;
		}
	}
	if ( !input.begin( 2 ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; i += 2 ) {
		if ( !input.disconnect( SIR_INPUT_TEST, devices[i] ) || input.describe( devices[i], nullptr ) ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; i += 2 ) {
		const sirInputHardwareKey_t key = { 1000 + i, 3000 + i, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER };
		sirInputDeviceId_t next = {};
		if ( !input.connect( key, &next ) || next.nSlot != devices[i].nSlot || next.nGeneration == devices[i].nGeneration ||
		     input.vibrate( devices[i], 100, 100 ) ) {
			return false;
		}
		devices[i] = next;
	}
	if ( !input.publish() || input.frame()->nCount != SIR_INPUT_DEVICE_CAPACITY ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( !input.vibrate( devices[i], 100, 200 ) ) {
			return false;
		}
	}
	input.shutdown();
	return backend.m_nVibrations == SIR_INPUT_DEVICE_CAPACITY + SIR_INPUT_DEVICE_CAPACITY / 2 && backend.m_nStops == 1 && other.m_nStops == 1;
}

static bool testTransactionalAndOverflow( void ) noexcept
{
	SIRInputService input;
	SIRInputStressBackend backend( SIR_INPUT_TEST );
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t pad = {};
	if ( !input.connect( { 0, 22, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &pad ) ) {
		return false;
	}
	const sirInputControlChange_t invalid[] = { { SIR_INPUT_BUTTON, 30, 1 }, { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LT, -1 } };
	if ( input.emitControls( SIR_INPUT_TEST, pad, invalid, 2 ) || input.emitControls( SIR_INPUT_TEST, pad, nullptr, 1 ) ||
	     !input.publish() || input.frame()->nCount != 1 ) {
		return false;
	}
	if ( !input.begin( 2 ) ) {
		return false;
	}
	const sirInputControlChange_t edges[] = { { SIR_INPUT_BUTTON, 30, 1 }, { SIR_INPUT_BUTTON, 30, 0 } };
	for ( uint32_t i = 0; i < SIR_INPUT_EVENT_CAPACITY / 2; ++i ) {
		if ( !input.emitControls( SIR_INPUT_TEST, pad, edges, 2, 77 ) ) {
			return false;
		}
	}
	if ( input.emitControls( SIR_INPUT_TEST, pad, edges, 2, 78 ) || input.publish() || !input.recover() || backend.m_nResyncs != 1 ||
	     !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *pFrame = input.frame();
	if ( pFrame == nullptr || pFrame->nResyncCount != 1 || pFrame->nCount != 1 || pFrame->events[0].eSignal != SIR_INPUT_RESYNC ||
	     pFrame->state[pad.nSlot].nHeld[0] != 0 ) {
		return false;
	}
	if ( !input.begin( 3 ) || !input.emitControls( SIR_INPUT_TEST, pad, edges, 2 ) || !input.publish() ) {
		return false;
	}
	if ( input.frame()->nCount != 2 || input.frame()->state[pad.nSlot].nPressed[0] != ( UINT32_C( 1 ) << 30 ) ||
	     input.frame()->state[pad.nSlot].nReleased[0] != ( UINT32_C( 1 ) << 30 ) || !input.begin( 4 ) ) {
		return false;
	}
	// Exercise the specialized one-change path at exactly full capacity.
	for ( uint32_t i = 0; i < SIR_INPUT_EVENT_CAPACITY; ++i ) {
		const sirInputControlChange_t single = { SIR_INPUT_BUTTON, 40, ( i & 1 ) == 0 ? 1 : 0 };
		if ( !input.emitControls( SIR_INPUT_TEST, pad, &single, 1 ) ) {
			return false;
		}
	}
	const sirInputControlChange_t rejected = { SIR_INPUT_BUTTON, 40, 1 };
	if ( input.emitControls( SIR_INPUT_TEST, pad, &rejected, 1 ) || input.publish() || !input.recover() || !input.publish() ) {
		return false;
	}
	return backend.m_nResyncs == 2 && input.frame()->nResyncCount == 1 && input.frame()->nCount == 1;
}

static bool testPointerSaturationAndSnapshotLifetime( void ) noexcept
{
	SIRInputService input;
	SIRInputStressBackend backend( SIR_INPUT_TEST );
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t pointer = {};
	if ( !input.connect( { 0, 55, SIR_INPUT_TEST, SIR_INPUT_POINTER }, &pointer ) ||
	     !input.emitPointer( SIR_INPUT_TEST, pointer, SIR_INPUT_MOTION, 0, INT32_MAX, INT32_MIN, 10 ) ||
	     !input.emitPointer( SIR_INPUT_TEST, pointer, SIR_INPUT_MOTION, 0, 1024, -1024, 11 ) ||
	     !input.emitPointer( SIR_INPUT_TEST, pointer, SIR_INPUT_WHEEL, 0, 120, 0, 12 ) ||
	     input.emitPointer( SIR_INPUT_LINUX_EVDEV, pointer, SIR_INPUT_MOTION, 0, 1, 1, 13 ) || !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *pPublished = input.frame();
	if ( pPublished == nullptr || pPublished->state[pointer.nSlot].nMotionX != INT32_MAX ||
	     pPublished->state[pointer.nSlot].nMotionY != INT32_MIN || pPublished->nCount != 4 ) {
		return false;
	}
	if ( !input.begin( 2 ) || pPublished->nCount != 4 || pPublished->state[pointer.nSlot].nMotionX != INT32_MAX || !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *pNext = input.frame();
	return pNext != nullptr && pNext != pPublished && pNext->nCount == 0 && pNext->state[pointer.nSlot].nMotionX == 0 &&
	       pNext->state[pointer.nSlot].nMotionY == 0;
}

static bool testDeterministicEvents( uint32_t nTicks, uint32_t nEventsPerTick ) noexcept
{
	SIRInputService input;
	SIRInputStressBackend backend( SIR_INPUT_TEST );
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t ids[4] = {};
	for ( uint32_t i = 0; i < 4; ++i ) {
		if ( !input.connect( { 0, 10 + i, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &ids[i] ) ) {
			return false;
		}
	}
	if ( !input.publish() ) {
		return false;
	}
	uint32_t nSeed = 0x9e3779b9u;
	uint32_t nHeld[4][SIR_INPUT_BUTTON_WORDS] = {};
	int32_t nAxes[4][SIR_INPUT_AXIS_COUNT] = {};
	uint64_t nLastSequence = 0;
	uint64_t nActualTransitions = 0;
	for ( uint32_t iTick = 0; iTick < nTicks; ++iTick ) {
		if ( !input.begin( ( uint64_t )iTick + 2 ) ) {
			return false;
		}
		uint32_t nPressed[4][SIR_INPUT_BUTTON_WORDS] = {};
		uint32_t nReleased[4][SIR_INPUT_BUTTON_WORDS] = {};
		uint32_t nExpected = 0;
		for ( uint32_t iEvent = 0; iEvent < nEventsPerTick; ++iEvent ) {
			const uint32_t nRandom = randomInput( nSeed );
			const uint32_t iDevice = ( nRandom >> 16 ) & 3;
			const uint32_t nControl = ( nRandom >> 8 ) & 255;
			const bool bAxis = ( nRandom & 15 ) == 0;
			sirInputControlChange_t change = {};
			if ( bAxis ) {
				change.eSignal = SIR_INPUT_AXIS;
				change.nControl = ( uint16_t )( ( nRandom >> 8 ) % SIR_INPUT_AXIS_COUNT );
				change.nValue = ( int32_t )( ( nRandom >> 14 ) % 65535 ) - 32767;
				if ( change.nControl >= SIR_INPUT_AXIS_LT && change.nValue < 0 ) {
					change.nValue = -change.nValue;
				}
				if ( nAxes[iDevice][change.nControl] != change.nValue ) {
					++nExpected;
				}
				nAxes[iDevice][change.nControl] = change.nValue;
			} else {
				change.eSignal = SIR_INPUT_BUTTON;
				change.nControl = ( uint16_t )nControl;
				const uint32_t nWord = nControl / 32;
				const uint32_t nMask = UINT32_C( 1 ) << ( nControl % 32 );
				const bool bWasHeld = ( nHeld[iDevice][nWord] & nMask ) != 0;
				const bool bPress = ( ( nRandom >> 18 ) & 1 ) != 0;
				change.nValue = bPress ? 1 : 0;
				if ( bWasHeld != bPress ) {
					++nExpected;
					if ( bPress ) {
						nPressed[iDevice][nWord] |= nMask;
						nHeld[iDevice][nWord] |= nMask;
					} else {
						nReleased[iDevice][nWord] |= nMask;
						nHeld[iDevice][nWord] &= ~nMask;
					}
				}
			}
			if ( !input.emitControls( SIR_INPUT_TEST, ids[iDevice], &change, 1, iEvent + 100 ) ) {
				return false;
			}
		}
		if ( !input.publish() ) {
			return false;
		}
		const sirInputFrame_t *pFrame = input.frame();
		if ( pFrame == nullptr || pFrame->nCount != nExpected || pFrame->nTick != ( uint64_t )iTick + 2 || pFrame->nResyncCount != 0 ) {
			return false;
		}
		nActualTransitions += pFrame->nCount;
		for ( uint32_t i = 0; i < pFrame->nCount; ++i ) {
			if ( pFrame->events[i].nSequence <= nLastSequence ) {
				return false;
			}
			nLastSequence = pFrame->events[i].nSequence;
		}
		for ( uint32_t i = 0; i < 4; ++i ) {
			const sirInputDeviceState_t &state = pFrame->state[ids[i].nSlot];
			if ( pFrame->nConnected[ids[i].nSlot] != 1 || memcmp( state.nHeld, nHeld[i], sizeof( state.nHeld ) ) != 0 ||
			     memcmp( state.nPressed, nPressed[i], sizeof( state.nPressed ) ) != 0 ||
			     memcmp( state.nReleased, nReleased[i], sizeof( state.nReleased ) ) != 0 ||
			     memcmp( state.nAxes, nAxes[i], sizeof( nAxes[i] ) ) != 0 ) {
				return false;
			}
		}
	}
	printf( "input trace: %u ticks, %u requests/tick, %llu transitions, seed=0x9e3779b9\n", nTicks, nEventsPerTick,
	        ( unsigned long long )nActualTransitions );
	return true;
}

static bool testProfileCapacity( void ) noexcept
{
	SIRInputControllerProfiles profiles;
	sirInputControllerProfile_t profile = {};
	profile.nBus = 0;
	profile.nProduct = 0x1000;
	for ( uint32_t i = 0; i < SIR_INPUT_CONTROLLER_PROFILE_CAPACITY; ++i ) {
		profile.nVendor = ( uint16_t )( i + 1 );
		if ( !profiles.add( profile ) || profiles.find( 7, profile.nVendor, profile.nProduct ) == nullptr ) {
			return false;
		}
	}
	profile.nVendor = 0x9999;
	if ( profiles.add( profile ) || profiles.count() != SIR_INPUT_CONTROLLER_PROFILE_CAPACITY ) {
		return false;
	}
	return profiles.find( 7, 0x9999, profile.nProduct ) == nullptr;
}

int main( int argc, char **argv )
{
	const bool bSoak = argc == 2 && strcmp( argv[1], "--soak" ) == 0;
	if ( argc > 2 || ( argc == 2 && !bSoak ) ) {
		fprintf( stderr, "Usage: SIRInputStressTests [--soak]\n" );
		return 2;
	}
	if ( !testCapacityAndIdentity() ) {
		fprintf( stderr, "input stress: capacity/identity failed\n" );
		return 1;
	}
	if ( !testTransactionalAndOverflow() ) {
		fprintf( stderr, "input stress: transaction/overflow failed\n" );
		return 1;
	}
	if ( !testPointerSaturationAndSnapshotLifetime() ) {
		fprintf( stderr, "input stress: pointer saturation/snapshot failed\n" );
		return 1;
	}
	if ( !testProfileCapacity() ) {
		fprintf( stderr, "input stress: profile capacity failed\n" );
		return 1;
	}
	const uint32_t nTicks = bSoak ? 30000 : 300;
	const uint32_t nEvents = bSoak ? 320 : 96;
	if ( !testDeterministicEvents( nTicks, nEvents ) ) {
		fprintf( stderr, "input stress: reference trace mismatch\n" );
		return 1;
	}
	printf( "PASS: input deterministic stress %s\n", bSoak ? "soak" : "standard" );
	return 0;
}
