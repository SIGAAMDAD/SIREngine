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

#include <Core/Input/SIRInputHaptics.h>
#include <stdio.h>

typedef struct sirInputHapticTestSink_s
{
	uint32_t nCalls;
	uint32_t nStops;
	uint32_t nStarts;
	uint32_t nLastGeneration;
	uint16_t nLeft;
	uint16_t nRight;
	bool bRejectStops;
	bool bRejectStarts;
} sirInputHapticTestSink_t;

static bool sirHapticTestSink( void *pContext, sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept
{
	sirInputHapticTestSink_t *pSink = ( sirInputHapticTestSink_t * )pContext;
	++pSink->nCalls;
	if ( nLeft == 0 && nRight == 0 ) {
		++pSink->nStops;
		if ( pSink->bRejectStops ) {
			return false;
		}
	} else {
		++pSink->nStarts;
		if ( pSink->bRejectStarts ) {
			return false;
		}
	}
	pSink->nLastGeneration = hDevice.nGeneration;
	pSink->nLeft = nLeft;
	pSink->nRight = nRight;
	return true;
}

static bool sirHapticInvalidScheduleAndCapacity( void ) noexcept
{
	SIRInputHaptics haptics;
	sirInputHapticRequest_t request = { { 0, 1 }, 5000, 10000, 0, 100, 0, 1 };
	sirInputHapticHandle_t hEffect = {};
	sirInputHapticRequest_t invalid = request;
	invalid.hDevice.nSlot = SIR_INPUT_DEVICE_CAPACITY;
	if ( haptics.schedule( invalid, 1, &hEffect ) ) {
		return false;
	}
	invalid = request;
	invalid.hDevice.nGeneration = 0;
	if ( haptics.schedule( invalid, 1, &hEffect ) ) {
		return false;
	}
	invalid = request;
	invalid.nSustainMs = 0;
	if ( haptics.schedule( invalid, 1, &hEffect ) ) {
		return false;
	}
	invalid = request;
	invalid.nSustainMs = SIR_INPUT_HAPTIC_MAX_DURATION_MS + 1;
	if ( haptics.schedule( invalid, 1, &hEffect ) ) {
		return false;
	}
	invalid = request;
	invalid.nLeft = 0;
	invalid.nRight = 0;
	if ( haptics.schedule( invalid, 1, &hEffect ) || haptics.schedule( request, 1, nullptr ) ||
	     haptics.cancel( { SIR_INPUT_HAPTIC_EFFECT_CAPACITY, 1 } ) ) {
		return false;
	}
	sirInputHapticHandle_t szEffects[SIR_INPUT_HAPTIC_EFFECT_CAPACITY] = {};
	for ( uint32_t i = 0; i < SIR_INPUT_HAPTIC_EFFECT_CAPACITY; ++i ) {
		if ( !haptics.schedule( request, 1000, &szEffects[i] ) || szEffects[i].nSlot != i ) {
			return false;
		}
	}
	if ( haptics.activeCount() != SIR_INPUT_HAPTIC_EFFECT_CAPACITY || haptics.schedule( request, 1000, &hEffect ) ||
	     !haptics.cancel( szEffects[0] ) || haptics.cancel( szEffects[0] ) || !haptics.schedule( request, 1000, &hEffect ) ||
	     hEffect.nSlot != 0 || hEffect.nGeneration == szEffects[0].nGeneration || haptics.cancel( szEffects[0] ) ) {
		return false;
	}
	haptics.stopAll( nullptr, nullptr );
	return haptics.activeCount() == 0 && !haptics.hasWork();
}

static bool sirHapticSinkErrorsAndTransitions( void ) noexcept
{
	SIRInputHaptics haptics;
	sirInputHapticTestSink_t sink = {};
	const sirInputDeviceId_t hDevice = { 2, 5 };
	const sirInputHapticRequest_t request = { hDevice, 40000, 15000, 0, 100, 0, 1 };
	sirInputHapticHandle_t hLow = {};
	if ( haptics.advance( 0, false, nullptr, &sink ) || !haptics.schedule( request, 1000000000ULL, &hLow ) ) {
		return false;
	}
	sink.bRejectStarts = true;
	if ( haptics.advance( 1000000000ULL, false, sirHapticTestSink, &sink ) || sink.nStarts != 1 || sink.nLeft != 0 || sink.nRight != 0 ) {
		return false;
	}
	sink.bRejectStarts = false;
	if ( !haptics.advance( 1000000000ULL, false, sirHapticTestSink, &sink ) || sink.nStarts != 2 || sink.nLeft != 40000 ||
	     sink.nRight != 15000 || !haptics.hasWork() ) {
		return false;
	}
	const sirInputHapticRequest_t high = { hDevice, 10000, 50000, 0, 100, 0, 10 };
	sirInputHapticHandle_t hHigh = {};
	if ( !haptics.schedule( high, 1000000000ULL, &hHigh ) || !haptics.advance( 1000000000ULL, false, sirHapticTestSink, &sink ) ||
	     sink.nLeft != 10000 || sink.nRight != 50000 || !haptics.cancel( hHigh ) ||
	     !haptics.advance( 1000000000ULL, false, sirHapticTestSink, &sink ) || sink.nLeft != 40000 ) {
		return false;
	}
	sink.bRejectStops = true;
	if ( haptics.advance( 1000000000ULL, true, sirHapticTestSink, &sink ) || sink.nStops != 1 ) {
		return false;
	}
	sink.bRejectStops = false;
	if ( !haptics.advance( 1000000000ULL, true, sirHapticTestSink, &sink ) || sink.nStops != 2 || !haptics.hasWork() ||
	     !haptics.advance( 1000000000ULL, false, sirHapticTestSink, &sink ) || sink.nLeft != 40000 ||
	     !haptics.advance( 1200000000ULL, false, sirHapticTestSink, &sink ) || haptics.activeCount() != 0 || sink.nLeft != 0 ||
	     haptics.hasWork() || haptics.cancel( hLow ) ) {
		return false;
	}
	return true;
}

static bool sirHapticDeviceForgetAndLayering( void ) noexcept
{
	SIRInputHaptics haptics;
	sirInputHapticTestSink_t sink = {};
	const sirInputDeviceId_t hDevice = { 5, 7 };
	const sirInputDeviceId_t hOldDevice = { 5, 6 };
	const sirInputHapticRequest_t request = { hDevice, 30000, 20000, 10, 10, 10, 2 };
	sirInputHapticHandle_t hFirst = {};
	if ( !haptics.schedule( request, 1000000000ULL, &hFirst ) || !haptics.advance( 1005000000ULL, false, sirHapticTestSink, &sink ) ||
	     sink.nLeft == 0 || sink.nLeft >= 30000 ) {
		return false;
	}
	haptics.forgetDevice( hOldDevice, sirHapticTestSink, &sink );
	if ( haptics.activeCount() != 1 || !haptics.hasWork() ) {
		return false;
	}
	haptics.forgetDevice( hDevice, nullptr, nullptr );
	if ( haptics.activeCount() != 0 || haptics.hasWork() ) {
		return false;
	}
	const sirInputHapticRequest_t second = { hDevice, 50000, 1000, 0, 100, 0, 1 };
	sirInputHapticHandle_t hSecond = {};
	if ( !haptics.schedule( second, 2000000000ULL, &hSecond ) || !haptics.advance( 2000000000ULL, false, sirHapticTestSink, &sink ) ||
	     sink.nLeft != 50000 ) {
		return false;
	}
	haptics.forgetDevice( hDevice, sirHapticTestSink, &sink );
	if ( haptics.activeCount() != 0 || haptics.hasWork() || sink.nLeft != 0 ) {
		return false;
	}
	if ( !haptics.schedule( second, 2000000000ULL, &hSecond ) || !haptics.advance( 2000000000ULL, false, sirHapticTestSink, &sink ) ) {
		return false;
	}
	haptics.stopAll( sirHapticTestSink, &sink );
	return haptics.activeCount() == 0 && !haptics.hasWork() && sink.nLeft == 0;
}

static bool sirHapticPriorityMix( void ) noexcept
{
	SIRInputHaptics haptics;
	sirInputHapticTestSink_t sink = {};
	const sirInputDeviceId_t hDevice = { 4, 3 };
	const sirInputHapticRequest_t first = { hDevice, 5000, 30000, 0, 1000, 0, 5 };
	const sirInputHapticRequest_t lower = { hDevice, 60000, 60000, 0, 1000, 0, 3 };
	const sirInputHapticRequest_t equalLeft = { hDevice, 30000, 5000, 0, 1000, 0, 5 };
	const sirInputHapticRequest_t equalRight = { hDevice, 5000, 50000, 0, 1000, 0, 5 };
	sirInputHapticHandle_t szHandles[4] = {};
	if ( !haptics.schedule( first, 1000000000ULL, &szHandles[0] ) || !haptics.schedule( lower, 1000000000ULL, &szHandles[1] ) ||
	     !haptics.schedule( equalLeft, 1000000000ULL, &szHandles[2] ) || !haptics.schedule( equalRight, 1000000000ULL, &szHandles[3] ) ||
	     !haptics.advance( 1000000000ULL, false, sirHapticTestSink, &sink ) || sink.nLeft != 30000 || sink.nRight != 50000 ||
	     sink.nLastGeneration != 3 ) {
		return false;
	}
	for ( uint32_t i = 0; i < 4; ++i ) {
		if ( !haptics.cancel( szHandles[i] ) ) {
			return false;
		}
	}
	return haptics.advance( 1000000000ULL, false, sirHapticTestSink, &sink ) && sink.nLeft == 0 && sink.nRight == 0 &&
	       haptics.activeCount() == 0 && !haptics.hasWork();
}

int main( void )
{
	if ( !sirHapticInvalidScheduleAndCapacity() ) {
		fputs( "input haptics: invalid capacity FAILED\n", stderr );
		return 1;
	}
	if ( !sirHapticSinkErrorsAndTransitions() ) {
		fputs( "input haptics: sink transition FAILED\n", stderr );
		return 1;
	}
	if ( !sirHapticDeviceForgetAndLayering() ) {
		fputs( "input haptics: device forget FAILED\n", stderr );
		return 1;
	}
	if ( !sirHapticPriorityMix() ) {
		fputs( "input haptics: priority mix FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputHapticNegativeTests PASS" );
	return 0;
}
