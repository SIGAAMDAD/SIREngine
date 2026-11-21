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
#include <stdio.h>
#include <string.h>

class SIRInputSnapshotTestBackend final : public SIRInputBackend
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
	bool vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )id;
		( void )nLeft;
		( void )nRight;
		return false;
	}
	void stop( SIRInputService& service ) noexcept override
	{
		( void )service;
	}
};

static uint32_t nextInputTestRandom( uint32_t& nSeed ) noexcept
{
	nSeed ^= nSeed << 13;
	nSeed ^= nSeed >> 17;
	nSeed ^= nSeed << 5;
	return nSeed;
}

static bool testSparseSnapshotAcrossBothBuffers( void ) noexcept
{
	SIRInputService input;
	SIRInputSnapshotTestBackend backend;
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t szDevices[SIR_INPUT_DEVICE_CAPACITY] = {};
	uint32_t szUsers[SIR_INPUT_DEVICE_CAPACITY] = {};
	sirInputDeviceState_t szReference[SIR_INPUT_DEVICE_CAPACITY] = {};
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		const sirInputDeviceClass_t eClass = i == SIR_INPUT_DEVICE_CAPACITY - 1 ? SIR_INPUT_POINTER : SIR_INPUT_CONTROLLER;
		if ( !input.connect( { i + 100, i + 200, SIR_INPUT_TEST, eClass }, &szDevices[i] ) ||
		     !input.pairUser( szDevices[i], i % SIR_INPUT_USER_CAPACITY ) ) {
			return false;
		}
		szUsers[i] = i % SIR_INPUT_USER_CAPACITY;
	}
	if ( !input.publish() || input.frame() == nullptr ) {
		return false;
	}
	uint32_t nSeed = UINT32_C( 0x4b89df1e );
	uint64_t nLastSequence = 0;
	for ( uint64_t nTick = 2; nTick < 4002; ++nTick ) {
		const sirInputFrame_t *pPrevious = input.frame();
		const uint64_t nPreviousTick = pPrevious->nTick;
		if ( !input.begin( nTick ) ) {
			return false;
		}
		for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
			memset( szReference[i].nPressed, 0, sizeof( szReference[i].nPressed ) );
			memset( szReference[i].nReleased, 0, sizeof( szReference[i].nReleased ) );
			szReference[i].nMotionX = 0;
			szReference[i].nMotionY = 0;
		}
		uint32_t nExpectedEvents = 0;
		const uint32_t nChanges = nTick % 37 == 0 ? SIR_INPUT_DEVICE_CAPACITY - 1 : ( nTick % 17 == 0 ? 8 : 1 );
		for ( uint32_t i = 0; i < nChanges; ++i ) {
			const uint32_t nRandom = nextInputTestRandom( nSeed );
			const uint32_t iDevice = nChanges == SIR_INPUT_DEVICE_CAPACITY - 1 ? i : ( nRandom % ( SIR_INPUT_DEVICE_CAPACITY - 1 ) );
			const bool bAxis = ( nRandom & 7 ) == 0;
			sirInputControlChange_t change = {};
			if ( bAxis ) {
				change.eSignal = SIR_INPUT_AXIS;
				change.nControl = ( uint16_t )( ( nRandom >> 8 ) % SIR_INPUT_AXIS_COUNT );
				change.nValue = ( int32_t )( ( nRandom >> 16 ) % 32767 );
				if ( change.nControl < SIR_INPUT_AXIS_LT && ( ( nRandom >> 7 ) & 1 ) != 0 ) {
					change.nValue = -change.nValue;
				}
				if ( szReference[iDevice].nAxes[change.nControl] != change.nValue ) {
					++nExpectedEvents;
				}
				szReference[iDevice].nAxes[change.nControl] = change.nValue;
			} else {
				change.eSignal = SIR_INPUT_BUTTON;
				change.nControl = ( uint16_t )( ( nRandom >> 8 ) % SIR_INPUT_BUTTON_CAPACITY );
				const uint32_t iWord = change.nControl / 32;
				const uint32_t nBit = UINT32_C( 1 ) << ( change.nControl % 32 );
				const bool bHeld = ( szReference[iDevice].nHeld[iWord] & nBit ) != 0;
				change.nValue = ( ( nRandom >> 30 ) & 1 ) != 0 ? 1 : 0;
				if ( bHeld != ( change.nValue != 0 ) ) {
					++nExpectedEvents;
					if ( change.nValue != 0 ) {
						szReference[iDevice].nHeld[iWord] |= nBit;
						szReference[iDevice].nPressed[iWord] |= nBit;
					} else {
						szReference[iDevice].nHeld[iWord] &= ~nBit;
						szReference[iDevice].nReleased[iWord] |= nBit;
					}
				}
			}
			if ( !input.emitControls( SIR_INPUT_TEST, szDevices[iDevice], &change, 1, nTick * 100 + i ) ) {
				return false;
			}
		}
		if ( nTick % 3 == 0 ) {
			const uint32_t iPointer = SIR_INPUT_DEVICE_CAPACITY - 1;
			if ( !input.emitPointer( SIR_INPUT_TEST, szDevices[iPointer], SIR_INPUT_MOTION, 0, 3, -5, nTick * 100 + 99 ) ) {
				return false;
			}
			++nExpectedEvents;
			szReference[iPointer].nMotionX = 3;
			szReference[iPointer].nMotionY = -5;
		}
		if ( nTick % 53 == 0 ) {
			const uint32_t iDevice = ( uint32_t )( nTick % SIR_INPUT_DEVICE_CAPACITY );
			szUsers[iDevice] = ( szUsers[iDevice] + 1 ) % SIR_INPUT_USER_CAPACITY;
			if ( !input.pairUser( szDevices[iDevice], szUsers[iDevice] ) ) {
				return false;
			}
		}
		if ( pPrevious->nTick != nPreviousTick || !input.publish() ) {
			return false;
		}
		const sirInputFrame_t *pFrame = input.frame();
		if ( pFrame == nullptr || pFrame->nCount != nExpectedEvents || pFrame->nTick != nTick || pFrame->nResyncCount != 0 ) {
			fprintf( stderr, "input delta: mismatch tick=%llu actual events=%u expected=%u\n", ( unsigned long long )nTick,
			         pFrame != nullptr ? pFrame->nCount : 0, nExpectedEvents );
			return false;
		}
		for ( uint32_t i = 0; i < pFrame->nCount; ++i ) {
			if ( pFrame->events[i].nSequence <= nLastSequence ) {
				return false;
			}
			nLastSequence = pFrame->events[i].nSequence;
		}
		for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
			if ( pFrame->nConnected[i] != 1 || pFrame->nUsers[i] != szUsers[i] ||
			     pFrame->devices[i].nGeneration != szDevices[i].nGeneration ||
			     memcmp( &pFrame->state[i], &szReference[i], sizeof( szReference[i] ) ) != 0 ) {
				fprintf( stderr, "input delta: stale snapshot tick=%llu device=%u\n", ( unsigned long long )nTick, i );
				return false;
			}
		}
	}
	return true;
}

static bool testHighRatePointerPublication( void ) noexcept
{
	SIRInputService input;
	SIRInputSnapshotTestBackend backend;
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t id = {};
	if ( !input.connect( { 1, 31337, SIR_INPUT_TEST, SIR_INPUT_POINTER }, &id ) || !input.publish() ) {
		return false;
	}
	for ( uint64_t nTick = 2; nTick < 20; ++nTick ) {
		if ( !input.begin( nTick ) ) {
			return false;
		}
		for ( uint32_t i = 0; i < 2000; ++i ) {
			if ( !input.emitPointer( SIR_INPUT_TEST, id, SIR_INPUT_MOTION, 0, 1, -1, nTick * 10000 + i ) ) {
				return false;
			}
		}
		if ( !input.publish() ) {
			return false;
		}
		const sirInputFrame_t *pFrame = input.frame();
		if ( pFrame == nullptr || pFrame->nCount != 2000 || pFrame->state[id.nSlot].nMotionX != 2000 ||
		     pFrame->state[id.nSlot].nMotionY != -2000 ) {
			return false;
		}
	}
	return true;
}

int main( void )
{
	if ( !testSparseSnapshotAcrossBothBuffers() || !testHighRatePointerPublication() ) {
		fprintf( stderr, "SIRInputSnapshotDeltaTests FAIL\n" );
		return 1;
	}
	printf( "SIRInputSnapshotDeltaTests PASS\n" );
	return 0;
}
