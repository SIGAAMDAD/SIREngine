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
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

static bool sirCVarTextEqual( const char *pszLeft, const char *pszRight ) noexcept
{
	while ( *pszLeft != 0 && *pszRight != 0 ) {
		char nA = *pszLeft++;
		char nB = *pszRight++;
		if ( nA >= 'A' && nA <= 'Z' ) {
			nA = ( char )( nA + 32 );
		}
		if ( nB >= 'A' && nB <= 'Z' ) {
			nB = ( char )( nB + 32 );
		}
		if ( nA != nB ) {
			return false;
		}
	}
	return *pszLeft == *pszRight;
}

sirRegistrationResult_t sirCVarParseText( sirCVarType_t eType, const char *pszInput, uint32_t nLength, sirCVarValue_t *pValue ) noexcept
{
	if ( pValue == nullptr || ( pszInput == nullptr && nLength != 0 ) || eType >= SIR_CVAR_TYPE_COUNT ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pValue = {};
	if ( eType == SIR_CVAR_UTF8_STRING ) {
		pValue->pszText = pszInput;
		pValue->nTextBytes = nLength;
		return SIR_REGISTRATION_OK;
	}
	if ( nLength == 0 || nLength >= 128 ) {
		return SIR_REGISTRATION_TYPE_MISMATCH;
	}
	char szText[128] = {};
	memcpy( szText, pszInput, nLength );
	for ( uint32_t iByte = 0; iByte < nLength; ++iByte ) {
		if ( szText[iByte] == 0 ) {
			return SIR_REGISTRATION_TYPE_MISMATCH;
		}
	}
	if ( eType == SIR_CVAR_BOOL ) {
		if ( sirCVarTextEqual( szText, "true" ) || sirCVarTextEqual( szText, "on" ) || strcmp( szText, "1" ) == 0 ) {
			pValue->nBits = 1;
			return SIR_REGISTRATION_OK;
		}
		return sirCVarTextEqual( szText, "false" ) || sirCVarTextEqual( szText, "off" ) || strcmp( szText, "0" ) == 0
		           ? SIR_REGISTRATION_OK
				   : SIR_REGISTRATION_TYPE_MISMATCH;
	}
	char *pszEnd = nullptr;
	errno = 0;
	if ( eType == SIR_CVAR_I32 || eType == SIR_CVAR_I64 ) {
		const long long nValue = strtoll( szText, &pszEnd, 0 );
		if ( errno == ERANGE || pszEnd == szText || *pszEnd != 0 ||
		     ( eType == SIR_CVAR_I32 && ( nValue < INT32_MIN || nValue > INT32_MAX ) ) ) {
			return SIR_REGISTRATION_TYPE_MISMATCH;
		}
		pValue->nBits = eType == SIR_CVAR_I32 ? ( uint32_t )( int32_t )nValue : ( uint64_t )nValue;
		return SIR_REGISTRATION_OK;
	}
	if ( eType == SIR_CVAR_U32 || eType == SIR_CVAR_U64 || eType == SIR_CVAR_ENUM_U32 ) {
		if ( szText[0] == '-' ) {
			return SIR_REGISTRATION_TYPE_MISMATCH;
		}
		const unsigned long long nValue = strtoull( szText, &pszEnd, 0 );
		if ( errno == ERANGE || pszEnd == szText || *pszEnd != 0 || ( eType != SIR_CVAR_U64 && nValue > UINT32_MAX ) ) {
			return SIR_REGISTRATION_TYPE_MISMATCH;
		}
		pValue->nBits = ( uint64_t )nValue;
		return SIR_REGISTRATION_OK;
	}
	if ( eType == SIR_CVAR_F32 ) {
		const float fValue = strtof( szText, &pszEnd );
		if ( errno == ERANGE || pszEnd == szText || *pszEnd != 0 || !std::isfinite( fValue ) ) {
			return SIR_REGISTRATION_TYPE_MISMATCH;
		}
		uint32_t nBits = 0;
		memcpy( &nBits, &fValue, sizeof( nBits ) );
		pValue->nBits = nBits;
		return SIR_REGISTRATION_OK;
	}
	if ( eType == SIR_CVAR_F64 ) {
		const double fValue = strtod( szText, &pszEnd );
		if ( errno == ERANGE || pszEnd == szText || *pszEnd != 0 || !std::isfinite( fValue ) ) {
			return SIR_REGISTRATION_TYPE_MISMATCH;
		}
		memcpy( &pValue->nBits, &fValue, sizeof( pValue->nBits ) );
		return SIR_REGISTRATION_OK;
	}
	return SIR_REGISTRATION_TYPE_MISMATCH;
}

sirRegistrationResult_t sirCVarFormatText( sirCVarType_t eType, const sirCVarValue_t& value, sirCVarTextBuffer_t *pBuffer ) noexcept
{
	if ( pBuffer == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	char *pszOutput = pBuffer->pszOutput;
	const uint32_t nCapacity = pBuffer->nCapacity;
	uint32_t *pLength = &pBuffer->nLength;
	if ( pszOutput == nullptr || pLength == nullptr || nCapacity == 0 || eType >= SIR_CVAR_TYPE_COUNT ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pLength = 0;
	int nWritten = -1;
	if ( eType == SIR_CVAR_UTF8_STRING ) {
		if ( value.nTextBytes >= nCapacity || ( value.nTextBytes != 0 && value.pszText == nullptr ) ) {
			return SIR_REGISTRATION_CAPACITY_EXCEEDED;
		}
		if ( value.nTextBytes != 0 ) {
			memcpy( pszOutput, value.pszText, value.nTextBytes );
		}
		pszOutput[value.nTextBytes] = 0;
		*pLength = value.nTextBytes;
		return SIR_REGISTRATION_OK;
	}
	if ( eType == SIR_CVAR_BOOL ) {
		nWritten = snprintf( pszOutput, nCapacity, "%s", value.nBits == 0 ? "false" : "true" );
	} else if ( eType == SIR_CVAR_I32 ) {
		nWritten = snprintf( pszOutput, nCapacity, "%" PRId32, ( int32_t )( uint32_t )value.nBits );
	} else if ( eType == SIR_CVAR_U32 || eType == SIR_CVAR_ENUM_U32 ) {
		nWritten = snprintf( pszOutput, nCapacity, "%" PRIu32, ( uint32_t )value.nBits );
	} else if ( eType == SIR_CVAR_I64 ) {
		nWritten = snprintf( pszOutput, nCapacity, "%" PRId64, ( int64_t )value.nBits );
	} else if ( eType == SIR_CVAR_U64 ) {
		nWritten = snprintf( pszOutput, nCapacity, "%" PRIu64, value.nBits );
	} else if ( eType == SIR_CVAR_F32 ) {
		const uint32_t nBits = ( uint32_t )value.nBits;
		float fValue = 0.0f;
		memcpy( &fValue, &nBits, sizeof( fValue ) );
		nWritten = snprintf( pszOutput, nCapacity, "%.9g", ( double )fValue );
	} else if ( eType == SIR_CVAR_F64 ) {
		double fValue = 0.0;
		memcpy( &fValue, &value.nBits, sizeof( fValue ) );
		nWritten = snprintf( pszOutput, nCapacity, "%.17g", fValue );
	}
	if ( nWritten < 0 || ( uint32_t )nWritten >= nCapacity ) {
		return SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	*pLength = ( uint32_t )nWritten;
	return SIR_REGISTRATION_OK;
}
