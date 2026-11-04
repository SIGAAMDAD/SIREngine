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

#include <Core/Platform/Input/SIRInputHistory.h>
#include <stdio.h>
#include <string.h>

static bool sirTestHistoryRing( void ) noexcept
{
	sirInputReplayFrame_t szStorage[3] = {};
	SIRInputHistory history;
	if ( history.initialize( nullptr, 3 ) || history.initialize( szStorage, 0 ) || !history.initialize( szStorage, 3 ) ) {
		return false;
	}
	for ( uint64_t nTick = 100; nTick < 105; ++nTick ) {
		sirInputReplayFrame_t frame = {};
		frame.nTick = nTick;
		frame.nCount = 1;
		frame.actions[0] = { 10, 0, 32767, 1, true, true, false };
		if ( !history.append( frame ) ) {
			return false;
		}
	}
	if ( history.count() != 3 || history.oldestTick() != 102 || history.newestTick() != 104 ) {
		return false;
	}
	sirInputReplayFrame_t frame = {};
	if ( history.read( 101, &frame ) || !history.read( 103, &frame ) || frame.nTick != 103 || history.rewindFrom( 101 ) ||
	     history.rewindFrom( 106 ) || !history.rewindFrom( 103 ) || history.count() != 1 || history.newestTick() != 102 ) {
		return false;
	}
	frame.nTick = 103;
	frame.actions[0].nValue = -16384;
	sirInputReplayFrame_t gap = frame;
	gap.nTick = 105;
	if ( !history.append( frame ) || history.append( frame ) || history.append( gap ) ) {
		return false;
	}
	if ( !history.read( 103, &frame ) || frame.actions[0].nValue != -16384 ) {
		return false;
	}
	frame.nTick = 102;
	frame.actions[0].nValue = 0;
	frame.actions[0].bHeld = false;
	frame.actions[0].bReleased = true;
	if ( !history.replaceFrom( frame ) || history.count() != 1 || history.newestTick() != 102 || history.read( 103, &frame ) ) {
		return false;
	}
	uint64_t nHash = 0;
	if ( !history.digest( 102, &nHash ) || nHash == 0 || history.digest( 103, &nHash ) ) {
		return false;
	}
	if ( !history.rewindFrom( 102 ) || history.count() != 0 ) {
		return false;
	}
	return true;
}

static bool sirTestHistoryStress( void ) noexcept
{
	sirInputReplayFrame_t szStorage[64] = {};
	SIRInputHistory history;
	if ( !history.initialize( szStorage, 64 ) ) {
		return false;
	}
	for ( uint64_t nTick = 1; nTick <= 10000; ++nTick ) {
		sirInputReplayFrame_t frame = {};
		frame.nTick = nTick;
		frame.nCount = 1;
		frame.actions[0] = { 18, 4, nTick % 2 == 0 ? 32767 : 0, 1, nTick % 2 == 0, nTick % 2 == 0, nTick % 2 != 0 };
		if ( !history.append( frame ) ) {
			return false;
		}
	}
	if ( history.count() != 64 || history.oldestTick() != 9937 || history.newestTick() != 10000 ) {
		return false;
	}
	for ( uint64_t nTick = 9937; nTick <= 10000; ++nTick ) {
		sirInputReplayFrame_t result = {};
		if ( !history.read( nTick, &result ) || result.nTick != nTick || result.actions[0].bHeld != ( nTick % 2 == 0 ) ) {
			return false;
		}
	}
	return true;
}

static bool sirTestHistoryHash( void ) noexcept
{
	sirInputReplayFrame_t first = {};
	first.nTick = 1;
	first.nCount = 1;
	first.actions[0] = { 1, 0, 32767, 1, true, true, false };
	sirInputReplayFrame_t second = first;
	memset( &second.actions[1], 0xab, sizeof( second.actions[1] ) );
	uint64_t nA = 0;
	uint64_t nB = 0;
	if ( !SIRInputHistory::hashFrame( first, &nA ) || !SIRInputHistory::hashFrame( second, &nB ) || nA != nB ) {
		return false;
	}
	second.actions[0].nTransitions = 2;
	if ( !SIRInputHistory::hashFrame( second, &nB ) || nA == nB ) {
		return false;
	}
	second.actions[0].bHeld = false;
	if ( SIRInputHistory::hashFrame( second, &nB ) ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirTestHistoryRing() || !sirTestHistoryHash() || !sirTestHistoryStress() ) {
		fputs( "SIRInputHistoryTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputHistoryTests PASS" );
	return 0;
}
