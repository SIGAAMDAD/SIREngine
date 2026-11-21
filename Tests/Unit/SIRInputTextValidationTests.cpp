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

#include <Core/Input/SIRInputText.h>
#include <stdio.h>

static bool sirTestUtf8DecoderBoundaries( void ) noexcept
{
	uint32_t szScalars[4] = {};
	uint32_t nCount = 99;
	const char szAscii[] = { 'A' };
	const char szTwo[] = { ( char )0xc2, ( char )0xa2 };
	const char szThree[] = { ( char )0xe2, ( char )0x82, ( char )0xac };
	const char szFour[] = { ( char )0xf4, ( char )0x8f, ( char )0xbf, ( char )0xbf };
	if ( !SIRInputText::decodeUtf8( nullptr, 0, szScalars, 4, &nCount ) || nCount != 0 ||
	     !SIRInputText::decodeUtf8( szAscii, 1, szScalars, 4, &nCount ) || nCount != 1 || szScalars[0] != 'A' ||
	     !SIRInputText::decodeUtf8( szTwo, 2, szScalars, 4, &nCount ) || nCount != 1 || szScalars[0] != 0xa2 ||
	     !SIRInputText::decodeUtf8( szThree, 3, szScalars, 4, &nCount ) || nCount != 1 || szScalars[0] != 0x20ac ||
	     !SIRInputText::decodeUtf8( szFour, 4, szScalars, 4, &nCount ) || nCount != 1 || szScalars[0] != 0x10ffff ) {
		return false;
	}
	const char szCombined[] = { 'Q',          ( char )0xc2, ( char )0xa2, ( char )0xe2, ( char )0x82,
	                            ( char )0xac, ( char )0xf4, ( char )0x8f, ( char )0xbf, ( char )0xbf };
	return SIRInputText::decodeUtf8( szCombined, sizeof( szCombined ), szScalars, 4, &nCount ) && nCount == 4 && szScalars[0] == 'Q' &&
	       szScalars[1] == 0xa2 && szScalars[2] == 0x20ac && szScalars[3] == 0x10ffff;
}

static bool sirTestUtf8RejectsMalformedInput( void ) noexcept
{
	uint32_t szScalars[4] = {};
	uint32_t nCount = 37;
	const char szInvalid[][4] = { { 0, 0, 0, 0 },
	                              { ( char )0x80, 0, 0, 0 },
	                              { ( char )0xc0, ( char )0x80, 0, 0 },
	                              { ( char )0xff, 0, 0, 0 },
	                              { ( char )0xf5, ( char )0x80, ( char )0x80, ( char )0x80 },
	                              { ( char )0xc2, 'A', 0, 0 },
	                              { ( char )0xe0, ( char )0x80, ( char )0x80, 0 },
	                              { ( char )0xed, ( char )0xa0, ( char )0x80, 0 },
	                              { ( char )0xf0, ( char )0x80, ( char )0x80, ( char )0x80 },
	                              { ( char )0xf4, ( char )0x90, ( char )0x80, ( char )0x80 } };
	const uint32_t szLengths[] = { 1, 1, 2, 1, 4, 2, 3, 3, 4, 4 };
	for ( uint32_t i = 0; i < sizeof( szLengths ) / sizeof( szLengths[0] ); ++i ) {
		if ( SIRInputText::decodeUtf8( szInvalid[i], szLengths[i], szScalars, 4, &nCount ) || nCount != 37 ) {
			return false;
		}
	}
	const char szShortTwo[] = { ( char )0xc2 };
	const char szShortThree[] = { ( char )0xe2, ( char )0x82 };
	const char szShortFour[] = { ( char )0xf0, ( char )0x9f, ( char )0x8c };
	const char szAscii[] = { 'B' };
	return !SIRInputText::decodeUtf8( szShortTwo, 1, szScalars, 4, &nCount ) &&
	       !SIRInputText::decodeUtf8( szShortThree, 2, szScalars, 4, &nCount ) &&
	       !SIRInputText::decodeUtf8( szShortFour, 3, szScalars, 4, &nCount ) &&
	       !SIRInputText::decodeUtf8( nullptr, 1, szScalars, 4, &nCount ) && !SIRInputText::decodeUtf8( szAscii, 1, nullptr, 4, &nCount ) &&
	       !SIRInputText::decodeUtf8( szAscii, 1, szScalars, 4, nullptr ) && !SIRInputText::decodeUtf8( szAscii, 1, szScalars, 0, &nCount );
}

static bool sirTestTextInputStates( void ) noexcept
{
	SIRInputText text;
	const char szAscii[] = { 'a' };
	if ( text.appendCodepoint( 'a' ) || text.appendUtf16( 0x61 ) || text.appendUtf8( szAscii, 1 ) ) {
		return false;
	}
	text.begin( 1 );
	if ( text.appendCodepoint( 'a' ) || text.appendUtf16( 0x61 ) || text.appendUtf8( szAscii, 1 ) ) {
		return false;
	}
	text.setEnabled( true );
	if ( text.appendCodepoint( 'a' ) || text.appendUtf16( 0x61 ) || text.appendUtf8( szAscii, 1 ) ) {
		return false;
	}
	text.setFocused( true );
	if ( text.appendCodepoint( 0 ) || text.appendCodepoint( 0xd800 ) || text.appendCodepoint( 0xdfff ) ||
	     text.appendCodepoint( 0x110000 ) || !text.appendCodepoint( 'A' ) || !text.appendCodepoint( 0x10ffff ) ) {
		return false;
	}
	if ( !text.appendUtf16( 0xd83d ) || text.appendUtf16( 'B' ) || !text.appendUtf16( 0xd83d ) || !text.appendUtf16( 0xde00 ) ||
	     text.snapshot().szCodepoints[2] != 0x1f600 || text.appendUtf16( 0xdc00 ) || !text.appendUtf16( 0xd83d ) ||
	     text.appendUtf16( 0xe000 ) ) {
		return false;
	}
	text.setEnabled( true );
	text.setFocused( true );
	text.setFocused( false );
	if ( text.appendCodepoint( 'a' ) ) {
		return false;
	}
	text.setFocused( true );
	if ( !text.appendUtf16( 0xd83d ) ) {
		return false;
	}
	text.setEnabled( false );
	text.setEnabled( true );
	if ( text.appendUtf16( 0xde00 ) || !text.appendUtf16( 'C' ) ) {
		return false;
	}
	text.publish();
	if ( text.appendCodepoint( 'D' ) || text.appendUtf16( 'D' ) || text.appendUtf8( szAscii, 1 ) ) {
		return false;
	}
	text.reset();
	return text.snapshot().nTick == 0 && !text.snapshot().bEnabled && !text.snapshot().bFocused;
}

static bool sirTestTextAtomicCapacity( void ) noexcept
{
	SIRInputText text;
	text.setEnabled( true );
	text.setFocused( true );
	text.begin( 10 );
	for ( uint32_t i = 0; i < SIR_INPUT_TEXT_CAPACITY - 1; ++i ) {
		if ( !text.appendCodepoint( 'a' ) ) {
			return false;
		}
	}
	const char szTwo[] = { 'b', 'c' };
	const char szBad[] = { ( char )0xed, ( char )0xa0, ( char )0x80 };
	if ( text.appendUtf8( szBad, sizeof( szBad ) ) || text.appendUtf8( szTwo, sizeof( szTwo ) ) ||
	     text.snapshot().nCount != SIR_INPUT_TEXT_CAPACITY - 1 || !text.snapshot().bOverflow || !text.appendUtf8( szTwo, 1 ) ||
	     text.snapshot().nCount != SIR_INPUT_TEXT_CAPACITY || text.appendCodepoint( 'd' ) || !text.snapshot().bOverflow ) {
		return false;
	}
	text.begin( 11 );
	if ( text.snapshot().nCount != 0 || text.snapshot().bOverflow || !text.appendUtf8( nullptr, 0 ) ) {
		return false;
	}
	return true;
}

static bool sirTestPreeditValidation( void ) noexcept
{
	SIRInputText text;
	const uint32_t szScalars[] = { 'A', 0x1f600 };
	const uint32_t szInvalid[] = { 0xd800 };
	const uint16_t szValid[] = { 0xd83d, 0xde00 };
	const uint16_t szUnpaired[] = { 0xd83d, 'A' };
	const uint16_t szDangling[] = { 0xd83d };
	const uint16_t szLow[] = { 0xdc00 };
	const uint16_t szTooHigh[] = { 0xd83d, 0xe000 };
	const uint16_t szAscii[] = { 'A' };
	uint16_t szOversized[SIR_INPUT_PREEDIT_CAPACITY + 1] = {};
	for ( uint32_t i = 0; i < SIR_INPUT_PREEDIT_CAPACITY + 1; ++i ) {
		szOversized[i] = 'a';
	}
	const char szValidUtf8[] = { ( char )0xf0, ( char )0x9f, ( char )0x98, ( char )0x80 };
	const char szInvalidUtf8[] = { ( char )0xed, ( char )0xa0, ( char )0x80 };
	text.begin( 1 );
	if ( text.setPreeditCodepoints( szScalars, 2, 1, 0 ) || text.setPreeditUtf16( szValid, 2, 0, 0 ) ||
	     text.setPreeditUtf8( szValidUtf8, 4, 0, 0 ) ) {
		return false;
	}
	text.setEnabled( true );
	if ( text.setPreeditCodepoints( szScalars, 2, 1, 0 ) || text.setPreeditUtf16( szValid, 2, 0, 0 ) ||
	     text.setPreeditUtf8( szValidUtf8, 4, 0, 0 ) ) {
		return false;
	}
	text.setFocused( true );
	if ( text.setPreeditCodepoints( nullptr, 1, 0, 0 ) || text.setPreeditCodepoints( szScalars, SIR_INPUT_PREEDIT_CAPACITY + 1, 0, 0 ) ||
	     text.setPreeditCodepoints( szScalars, 2, 3, 0 ) || text.setPreeditCodepoints( szScalars, 2, 1, 2 ) ||
	     text.setPreeditCodepoints( szInvalid, 1, 0, 0 ) ) {
		return false;
	}
	if ( text.setPreeditUtf16( nullptr, 1, 0, 0 ) || text.setPreeditUtf16( szUnpaired, 2, 0, 0 ) ||
	     text.setPreeditUtf16( szDangling, 1, 0, 0 ) || text.setPreeditUtf16( szLow, 1, 0, 0 ) ||
	     text.setPreeditUtf16( szTooHigh, 2, 0, 0 ) || text.setPreeditUtf16( szOversized, SIR_INPUT_PREEDIT_CAPACITY + 1, 0, 0 ) ||
	     text.setPreeditUtf16( szValid, 2, 2, 0 ) || text.setPreeditUtf16( szValid, 2, 1, 1 ) ||
	     !text.setPreeditUtf16( szAscii, 1, 0, 0 ) ) {
		return false;
	}
	if ( text.setPreeditUtf8( szInvalidUtf8, sizeof( szInvalidUtf8 ), 0, 0 ) ||
	     text.setPreeditUtf8( szValidUtf8, sizeof( szValidUtf8 ), 2, 0 ) ||
	     text.setPreeditUtf8( szValidUtf8, sizeof( szValidUtf8 ), 0, 2 ) ||
	     !text.setPreeditUtf8( szValidUtf8, sizeof( szValidUtf8 ), 0, 1 ) || text.snapshot().nPreeditCount != 1 ||
	     text.snapshot().szPreedit[0] != 0x1f600 ) {
		return false;
	}
	const uint32_t nRevision = text.snapshot().nPreeditRevision;
	text.clearPreedit();
	if ( text.snapshot().nPreeditCount != 0 || text.snapshot().nPreeditRevision != nRevision + 1 ) {
		return false;
	}
	text.clearPreedit();
	if ( text.snapshot().nPreeditRevision != nRevision + 1 || !text.setPreeditCodepoints( szScalars, 2, 1, 1 ) ||
	     text.snapshot().nPreeditCount != 2 || text.snapshot().nCaret != 1 || text.snapshot().nSelection != 1 ||
	     !text.setPreeditUtf16( szValid, 2, 1, 0 ) || text.snapshot().szPreedit[0] != 0x1f600 ) {
		return false;
	}
	text.setFocused( false );
	if ( text.snapshot().nPreeditCount != 0 || text.setPreeditCodepoints( szScalars, 2, 0, 0 ) ) {
		return false;
	}
	text.setFocused( true );
	if ( !text.setPreeditCodepoints( nullptr, 0, 0, 0 ) ) {
		return false;
	}
	text.publish();
	return !text.setPreeditUtf8( szValidUtf8, sizeof( szValidUtf8 ), 0, 0 ) && !text.setPreeditUtf16( szValid, 2, 0, 0 ) &&
	       !text.setPreeditCodepoints( szScalars, 2, 0, 0 );
}

int main( void )
{
	if ( !sirTestUtf8DecoderBoundaries() ) {
		fputs( "input text: valid UTF-8 boundaries FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestUtf8RejectsMalformedInput() ) {
		fputs( "input text: malformed UTF-8 acceptance FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestTextInputStates() ) {
		fputs( "input text: state/surrogate handling FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestTextAtomicCapacity() ) {
		fputs( "input text: atomic overflow handling FAILED\n", stderr );
		return 1;
	}
	if ( !sirTestPreeditValidation() ) {
		fputs( "input text: preedit validation FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputTextValidationTests PASS" );
	return 0;
}
