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

#include <Core/CVars/SIRCVarValueCodec.h>

#include <initializer_list>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sirCodecRequire( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "CVar value codec coverage failed at line %u\n", nLine );
		abort();
	}
}

#define REQUIRE( condition ) sirCodecRequire( ( condition ), __LINE__ )

static void sirCodecAccept( sirCVarType_t eType, const char *pszText ) noexcept
{
	sirCVarValue_t value = {};
	const uint32_t nBytes = ( uint32_t )strlen( pszText );
	REQUIRE( sirCVarParseText( eType, pszText, nBytes, &value ) == SIR_REGISTRATION_OK );
	char szOutput[128] = {};
	sirCVarTextBuffer_t output = { szOutput, sizeof( szOutput ), UINT32_MAX };
	REQUIRE( sirCVarFormatText( eType, value, &output ) == SIR_REGISTRATION_OK );
	REQUIRE( output.nLength == strlen( szOutput ) );
}

static void sirCodecReject( sirCVarType_t eType, const char *pszText ) noexcept
{
	sirCVarValue_t value = {};
	REQUIRE( sirCVarParseText( eType, pszText, ( uint32_t )strlen( pszText ), &value ) == SIR_REGISTRATION_TYPE_MISMATCH );
}

int main( void )
{
	sirCVarValue_t value = { 59, "original", 8 };
	REQUIRE( sirCVarParseText( SIR_CVAR_BOOL, "true", 4, nullptr ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	REQUIRE( sirCVarParseText( SIR_CVAR_BOOL, nullptr, 4, &value ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	REQUIRE( sirCVarParseText( ( sirCVarType_t )SIR_CVAR_TYPE_COUNT, "1", 1, &value ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	REQUIRE( sirCVarParseText( SIR_CVAR_UTF8_STRING, nullptr, 0, &value ) == SIR_REGISTRATION_OK );
	REQUIRE( value.pszText == nullptr && value.nTextBytes == 0 );
	REQUIRE( sirCVarParseText( SIR_CVAR_UTF8_STRING, "hello", 5, &value ) == SIR_REGISTRATION_OK );
	REQUIRE( value.nTextBytes == 5 && value.pszText[0] == 'h' );

	const char szNul[3] = { '1', '\0', '2' };
	REQUIRE( sirCVarParseText( SIR_CVAR_I32, szNul, 3, &value ) == SIR_REGISTRATION_TYPE_MISMATCH );
	char szMax[128] = {};
	memset( szMax, '9', sizeof( szMax ) );
	REQUIRE( sirCVarParseText( SIR_CVAR_I32, szMax, 128, &value ) == SIR_REGISTRATION_TYPE_MISMATCH );
	REQUIRE( sirCVarParseText( SIR_CVAR_I32, "", 0, &value ) == SIR_REGISTRATION_TYPE_MISMATCH );

	for ( const char *pszText : { "true", "TRUE", "TrUe", "on", "ON", "1", "false", "FALSE", "off", "OFF", "0" } ) {
		sirCodecAccept( SIR_CVAR_BOOL, pszText );
	}
	for ( const char *pszText : { "TRUEe", "yes", "2", "", "false ", "truE!" } ) {
		sirCodecReject( SIR_CVAR_BOOL, pszText );
	}
	for ( const char *pszText : { "-2147483648", "2147483647", "0", "-1", "+43", "0x7fffffff" } ) {
		sirCodecAccept( SIR_CVAR_I32, pszText );
	}
	for ( const char *pszText :
	      { "2147483648", "-2147483649", "0x100000000", "abc", "1trailing", "999999999999999999999999999999999999" } ) {
		sirCodecReject( SIR_CVAR_I32, pszText );
	}
	for ( const char *pszText : { "-9223372036854775808", "9223372036854775807", "0", "-1" } ) {
		sirCodecAccept( SIR_CVAR_I64, pszText );
	}
	for ( const char *pszText : { "9223372036854775808", "-9223372036854775809", "oops" } ) {
		sirCodecReject( SIR_CVAR_I64, pszText );
	}
	for ( sirCVarType_t eType : { SIR_CVAR_U32, SIR_CVAR_ENUM_U32 } ) {
		for ( const char *pszText : { "0", "4294967295", "0xFFFFFFFF", "+1" } ) {
			sirCodecAccept( eType, pszText );
		}
		for ( const char *pszText : { "-1", "4294967296", "garbage", "1.5", "999999999999999999999999999" } ) {
			sirCodecReject( eType, pszText );
		}
	}
	for ( const char *pszText : { "0", "18446744073709551615", "0xFFFFFFFFFFFFFFFF" } ) {
		sirCodecAccept( SIR_CVAR_U64, pszText );
	}
	for ( const char *pszText : { "-1", "18446744073709551616", "0xZZ" } ) {
		sirCodecReject( SIR_CVAR_U64, pszText );
	}
	for ( sirCVarType_t eType : { SIR_CVAR_F32, SIR_CVAR_F64 } ) {
		for ( const char *pszText : { "0", "-1.25", "3.125", "+1e2", "0x1.8p+1" } ) {
			sirCodecAccept( eType, pszText );
		}
		for ( const char *pszText : { "nan", "inf", "-infinity", "bogus", "1f", "" } ) {
			sirCodecReject( eType, pszText );
		}
	}
	sirCodecReject( SIR_CVAR_F32, "1e+100" );
	sirCodecReject( SIR_CVAR_F64, "1e+999" );

	char szOut[64] = {};
	sirCVarTextBuffer_t output = { szOut, sizeof( szOut ), 71 };
	REQUIRE( sirCVarFormatText( SIR_CVAR_BOOL, value, nullptr ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	output.pszOutput = nullptr;
	REQUIRE( sirCVarFormatText( SIR_CVAR_BOOL, value, &output ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	output = { szOut, 0, 71 };
	REQUIRE( sirCVarFormatText( SIR_CVAR_BOOL, value, &output ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	output = { szOut, sizeof( szOut ), 71 };
	REQUIRE( sirCVarFormatText( ( sirCVarType_t )SIR_CVAR_TYPE_COUNT, value, &output ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	value.pszText = "hello";
	value.nTextBytes = 5;
	output = { szOut, 5, 71 };
	REQUIRE( sirCVarFormatText( SIR_CVAR_UTF8_STRING, value, &output ) == SIR_REGISTRATION_CAPACITY_EXCEEDED );
	output = { szOut, 6, 71 };
	REQUIRE( sirCVarFormatText( SIR_CVAR_UTF8_STRING, value, &output ) == SIR_REGISTRATION_OK );
	REQUIRE( output.nLength == 5 && strcmp( szOut, "hello" ) == 0 );
	value.pszText = nullptr;
	output = { szOut, sizeof( szOut ), 71 };
	REQUIRE( sirCVarFormatText( SIR_CVAR_UTF8_STRING, value, &output ) == SIR_REGISTRATION_CAPACITY_EXCEEDED );
	value.nTextBytes = 0;
	REQUIRE( sirCVarFormatText( SIR_CVAR_UTF8_STRING, value, &output ) == SIR_REGISTRATION_OK );
	REQUIRE( output.nLength == 0 && szOut[0] == 0 );
	value.nBits = 1;
	output = { szOut, 3, 71 };
	REQUIRE( sirCVarFormatText( SIR_CVAR_BOOL, value, &output ) == SIR_REGISTRATION_CAPACITY_EXCEEDED );
	REQUIRE( output.nLength == 0 );
	value.nBits = 0;
	output = { szOut, sizeof( szOut ), 71 };
	REQUIRE( sirCVarFormatText( SIR_CVAR_BOOL, value, &output ) == SIR_REGISTRATION_OK );
	REQUIRE( strcmp( szOut, "false" ) == 0 );
	return 0;
}
