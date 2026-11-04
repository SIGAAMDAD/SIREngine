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

#include <charconv>
#include <cmath>
#include <limits>
#include <string.h>
#include <system_error>

static bool sirEditorCodecReservedZero( const sirPropertyValue_t& value ) noexcept
{
	for ( uint32_t iByte = 0; iByte < sizeof( value.szReserved ); ++iByte ) {
		if ( value.szReserved[iByte] != 0 ) {
			return false;
		}
	}
	return true;
}

sirEditorScalarResult_t sirEditorDecodeScalar( const char *pszText, size_t nBytes, sirPropertyKind_t eKind,
                                               sirPropertyValue_t *pOutValue ) noexcept
{
	if ( pszText == nullptr || pOutValue == nullptr || nBytes == 0 || nBytes > 256 ) {
		return SIR_EDITOR_SCALAR_INVALID_ARGUMENT;
	}
	sirPropertyValue_t parsed = {};
	parsed.eKind = eKind;
	const char *pFirst = pszText;
	const char *pLast = pszText + nBytes;
	if ( eKind == SIR_PROPERTY_BOOL ) {
		if ( nBytes == 4 && memcmp( pszText, "true", 4 ) == 0 ) {
			parsed.nPayload = 1;
		} else if ( nBytes == 5 && memcmp( pszText, "false", 5 ) == 0 ) {
			parsed.nPayload = 0;
		} else {
			return SIR_EDITOR_SCALAR_INVALID_TEXT;
		}
	} else if ( eKind == SIR_PROPERTY_INT64 ) {
		int64_t nValue = 0;
		if ( *pFirst == '+' && nBytes > 1 ) {
			++pFirst;
			if ( *pFirst == '-' || *pFirst == '+' ) {
				return SIR_EDITOR_SCALAR_INVALID_TEXT;
			}
		}
		const std::from_chars_result result = std::from_chars( pFirst, pLast, nValue, 10 );
		if ( result.ec != std::errc{} || result.ptr != pLast ) {
			return SIR_EDITOR_SCALAR_INVALID_TEXT;
		}
		memcpy( &parsed.nPayload, &nValue, sizeof( nValue ) );
	} else if ( eKind == SIR_PROPERTY_UINT64 ) {
		uint64_t nValue = 0;
		if ( *pFirst == '+' && nBytes > 1 ) {
			++pFirst;
			if ( *pFirst == '-' || *pFirst == '+' ) {
				return SIR_EDITOR_SCALAR_INVALID_TEXT;
			}
		}
		const std::from_chars_result result = std::from_chars( pFirst, pLast, nValue, 10 );
		if ( result.ec != std::errc{} || result.ptr != pLast ) {
			return SIR_EDITOR_SCALAR_INVALID_TEXT;
		}
		parsed.nPayload = nValue;
	} else if ( eKind == SIR_PROPERTY_REAL64 ) {
		double fValue = 0;
		if ( *pFirst == '+' && nBytes > 1 ) {
			++pFirst;
			if ( *pFirst == '-' || *pFirst == '+' ) {
				return SIR_EDITOR_SCALAR_INVALID_TEXT;
			}
		}
		const std::from_chars_result result = std::from_chars( pFirst, pLast, fValue, std::chars_format::general );
		if ( result.ec != std::errc{} || result.ptr != pLast || !std::isfinite( fValue ) ) {
			return SIR_EDITOR_SCALAR_INVALID_TEXT;
		}
		memcpy( &parsed.nPayload, &fValue, sizeof( fValue ) );
	} else {
		return SIR_EDITOR_SCALAR_UNSUPPORTED_KIND;
	}
	*pOutValue = parsed;
	return SIR_EDITOR_SCALAR_OK;
}

sirEditorScalarResult_t sirEditorEncodeScalar( const sirPropertyValue_t& value, char *pszOutput, size_t nCapacity,
                                               size_t *pOutBytes ) noexcept
{
	if ( pOutBytes == nullptr || ( pszOutput == nullptr && nCapacity != 0 ) ) {
		return SIR_EDITOR_SCALAR_INVALID_ARGUMENT;
	}
	*pOutBytes = 0;
	if ( !sirEditorCodecReservedZero( value ) ) {
		return SIR_EDITOR_SCALAR_INVALID_ARGUMENT;
	}
	char szEncoded[128] = {};
	size_t nBytes = 0;
	if ( value.eKind == SIR_PROPERTY_BOOL ) {
		if ( value.nPayload > 1 ) {
			return SIR_EDITOR_SCALAR_INVALID_ARGUMENT;
		}
		const char *pszBoolean = value.nPayload == 0 ? "false" : "true";
		nBytes = value.nPayload == 0 ? 5 : 4;
		memcpy( szEncoded, pszBoolean, nBytes );
	} else if ( value.eKind == SIR_PROPERTY_INT64 ) {
		int64_t nValue = 0;
		memcpy( &nValue, &value.nPayload, sizeof( nValue ) );
		const std::to_chars_result result = std::to_chars( szEncoded, szEncoded + sizeof( szEncoded ) - 1, nValue );
		if ( result.ec != std::errc{} ) {
			return SIR_EDITOR_SCALAR_OUT_OF_CAPACITY;
		}
		nBytes = ( size_t )( result.ptr - szEncoded );
	} else if ( value.eKind == SIR_PROPERTY_UINT64 ) {
		const std::to_chars_result result = std::to_chars( szEncoded, szEncoded + sizeof( szEncoded ) - 1, value.nPayload );
		if ( result.ec != std::errc{} ) {
			return SIR_EDITOR_SCALAR_OUT_OF_CAPACITY;
		}
		nBytes = ( size_t )( result.ptr - szEncoded );
	} else if ( value.eKind == SIR_PROPERTY_REAL64 ) {
		double fValue = 0;
		memcpy( &fValue, &value.nPayload, sizeof( fValue ) );
		if ( !std::isfinite( fValue ) ) {
			return SIR_EDITOR_SCALAR_INVALID_ARGUMENT;
		}
		const std::to_chars_result result = std::to_chars( szEncoded, szEncoded + sizeof( szEncoded ) - 1,
		                                                  fValue, std::chars_format::general, std::numeric_limits<double>::max_digits10 );
		if ( result.ec != std::errc{} ) {
			return SIR_EDITOR_SCALAR_OUT_OF_CAPACITY;
		}
		nBytes = ( size_t )( result.ptr - szEncoded );
	} else {
		return SIR_EDITOR_SCALAR_UNSUPPORTED_KIND;
	}
	*pOutBytes = nBytes;
	if ( pszOutput == nullptr && nCapacity == 0 ) {
		return SIR_EDITOR_SCALAR_OK;
	}
	if ( nCapacity <= nBytes ) {
		return SIR_EDITOR_SCALAR_OUT_OF_CAPACITY;
	}
	memcpy( pszOutput, szEncoded, nBytes );
	pszOutput[nBytes] = '\0';
	return SIR_EDITOR_SCALAR_OK;
}
