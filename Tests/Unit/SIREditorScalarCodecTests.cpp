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

#include <Editor/Scene/SIREditorScalarCodec.h>

#include <assert.h>
#include <cmath>
#include <limits>
#include <string.h>

static void sirTestScalarRoundtrip( sirPropertyKind_t eKind, const char *pszSource ) noexcept
{
	sirPropertyValue_t value = {};
	assert( sirEditorDecodeScalar( pszSource, strlen( pszSource ), eKind, &value ) == SIR_EDITOR_SCALAR_OK );
	size_t nEncoded = 0;
	assert( sirEditorEncodeScalar( value, nullptr, 0, &nEncoded ) == SIR_EDITOR_SCALAR_OK );
	char szResult[128] = {};
	assert( sirEditorEncodeScalar( value, szResult, sizeof( szResult ), &nEncoded ) == SIR_EDITOR_SCALAR_OK );
	assert( strlen( szResult ) == nEncoded );
	sirPropertyValue_t decoded = {};
	assert( sirEditorDecodeScalar( szResult, nEncoded, eKind, &decoded ) == SIR_EDITOR_SCALAR_OK );
	assert( decoded.eKind == value.eKind && decoded.nPayload == value.nPayload );
}

int main( void )
{
	sirTestScalarRoundtrip( SIR_PROPERTY_BOOL, "true" );
	sirTestScalarRoundtrip( SIR_PROPERTY_BOOL, "false" );
	sirTestScalarRoundtrip( SIR_PROPERTY_INT64, "-9223372036854775808" );
	sirTestScalarRoundtrip( SIR_PROPERTY_INT64, "+9223372036854775807" );
	sirTestScalarRoundtrip( SIR_PROPERTY_UINT64, "18446744073709551615" );
	sirTestScalarRoundtrip( SIR_PROPERTY_REAL64, "0.12345678901234566" );
	sirTestScalarRoundtrip( SIR_PROPERTY_REAL64, "-2.5e-8" );
	sirTestScalarRoundtrip( SIR_PROPERTY_REAL64, "+2.125" );

	sirPropertyValue_t value = {};
	assert( sirEditorDecodeScalar( "TRUE", 4, SIR_PROPERTY_BOOL, &value ) == SIR_EDITOR_SCALAR_INVALID_TEXT );
	assert( sirEditorDecodeScalar( "true ", 5, SIR_PROPERTY_BOOL, &value ) == SIR_EDITOR_SCALAR_INVALID_TEXT );
	assert( sirEditorDecodeScalar( "8x", 2, SIR_PROPERTY_INT64, &value ) == SIR_EDITOR_SCALAR_INVALID_TEXT );
	assert( sirEditorDecodeScalar( "-1", 2, SIR_PROPERTY_UINT64, &value ) == SIR_EDITOR_SCALAR_INVALID_TEXT );
	assert( sirEditorDecodeScalar( "18446744073709551616", 20, SIR_PROPERTY_UINT64, &value ) == SIR_EDITOR_SCALAR_INVALID_TEXT );
	assert( sirEditorDecodeScalar( "1e999", 5, SIR_PROPERTY_REAL64, &value ) == SIR_EDITOR_SCALAR_INVALID_TEXT );
	assert( sirEditorDecodeScalar( "nan", 3, SIR_PROPERTY_REAL64, &value ) == SIR_EDITOR_SCALAR_INVALID_TEXT );
	assert( sirEditorDecodeScalar( "123", 3, SIR_PROPERTY_STRING_ATOM, &value ) == SIR_EDITOR_SCALAR_UNSUPPORTED_KIND );
	assert( sirEditorDecodeScalar( nullptr, 3, SIR_PROPERTY_INT64, &value ) == SIR_EDITOR_SCALAR_INVALID_ARGUMENT );

	assert( sirEditorDecodeScalar( "2", 1, SIR_PROPERTY_INT64, &value ) == SIR_EDITOR_SCALAR_OK );
	size_t nBytes = 0;
	char szOutput[16] = {};
	memset( szOutput, 0x56, sizeof( szOutput ) );
	assert( sirEditorEncodeScalar( value, szOutput, 1, &nBytes ) == SIR_EDITOR_SCALAR_OUT_OF_CAPACITY );
	assert( szOutput[0] == 0x56 && nBytes == 1 );
	assert( sirEditorEncodeScalar( value, szOutput, 2, &nBytes ) == SIR_EDITOR_SCALAR_OK );
	assert( nBytes == 1 && szOutput[0] == '2' && szOutput[1] == '\0' );
	assert( sirEditorEncodeScalar( value, nullptr, 1, &nBytes ) == SIR_EDITOR_SCALAR_INVALID_ARGUMENT );

	value.eKind = SIR_PROPERTY_BOOL;
	value.nPayload = 2;
	assert( sirEditorEncodeScalar( value, szOutput, sizeof( szOutput ), &nBytes ) == SIR_EDITOR_SCALAR_INVALID_ARGUMENT );
	value.eKind = SIR_PROPERTY_REAL64;
	const double fNan = std::numeric_limits<double>::quiet_NaN();
	memcpy( &value.nPayload, &fNan, sizeof( fNan ) );
	assert( sirEditorEncodeScalar( value, szOutput, sizeof( szOutput ), &nBytes ) == SIR_EDITOR_SCALAR_INVALID_ARGUMENT );
	value.eKind = SIR_PROPERTY_RESOURCE_ID;
	assert( sirEditorEncodeScalar( value, szOutput, sizeof( szOutput ), &nBytes ) == SIR_EDITOR_SCALAR_UNSUPPORTED_KIND );
	return 0;
}
