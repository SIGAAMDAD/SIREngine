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

#include <Core/Platform/Windows/SIRWindowsGameInputCodec.h>
#include <stdio.h>
#include <string.h>

static bool testStateAndEdges( void )
{
	sirGameInputDecodedGamepad_t previous = {};
	sirGameInputDecodedGamepad_t next = {};
	sirInputControlChange_t changes[SIRWindowsGameInputCodec::MAX_CHANGES] = {};
	uint32_t nCount = 0;
	sirGameInputRawGamepad_t raw = { 0x00000004U, -1.0f, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f };
	if ( !SIRWindowsGameInputCodec::decode( raw, previous, &next, changes, 20, &nCount ) || nCount != 20 ||
	     changes[0].eSignal != SIR_INPUT_BUTTON || changes[0].nControl != SIR_INPUT_PAD_SOUTH || changes[0].nValue != 1 ||
	     next.nAxes[SIR_INPUT_AXIS_LX] != -32767 || next.nAxes[SIR_INPUT_AXIS_LT] != 16384 ) {
		return false;
	}
	previous = next;
	if ( !SIRWindowsGameInputCodec::decode( raw, previous, &next, changes, 20, &nCount ) || nCount != 0 ) {
		return false;
	}
	raw.nButtons = 0x00000008U;
	if ( !SIRWindowsGameInputCodec::decode( raw, previous, &next, changes, 20, &nCount ) || nCount != 2 ||
	     changes[0].nControl != SIR_INPUT_PAD_SOUTH || changes[0].nValue != 0 || changes[1].nControl != SIR_INPUT_PAD_EAST ||
	     changes[1].nValue != 1 ) {
		return false;
	}
	previous = next;
	raw.nButtons = 0x00000004U;
	return SIRWindowsGameInputCodec::decode( raw, previous, &next, changes, 20, &nCount ) && nCount == 2 && changes[0].nValue == 1 &&
	       changes[1].nValue == 0;
}

static bool testValidation( void )
{
	sirGameInputDecodedGamepad_t before = {};
	sirGameInputDecodedGamepad_t next = {};
	sirInputControlChange_t changes[20] = {};
	uint32_t nCount = 777;
	const sirGameInputRawGamepad_t raw = {};
	return !SIRWindowsGameInputCodec::decode( raw, before, &next, changes, 19, &nCount ) && nCount == 777 && !next.bValid &&
	       !SIRWindowsGameInputCodec::decode( raw, before, nullptr, changes, 20, &nCount ) &&
	       SIRWindowsGameInputCodec::normalizeAxis( 2.0f, false ) == 32767 && SIRWindowsGameInputCodec::normalizeAxis( -2.0f, true ) == 0 &&
	       SIRWindowsGameInputCodec::timestampNanoseconds( 1234567 ) == UINT64_C( 1234567000 ) &&
	       SIRWindowsGameInputCodec::timestampNanoseconds( UINT64_MAX ) == 0;
}

int main( void )
{
	if ( !testStateAndEdges() || !testValidation() ) {
		fputs( "SIRGameInputCodecTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRGameInputCodecTests PASS" );
	return 0;
}
