/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_DIAGNOSTICBUFFER_H__
#define __SIR_DIAGNOSTICBUFFER_H__

#if defined( SIR_PRAGMA_ONCE_SUPPORTED ) && SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/SIRPCH.h>

// A stack-backed text writer. Capacity includes the trailing NUL byte.
typedef struct sirDiagnosticBuffer_s
{
	char *pData;
	uint32_t nCapacity;
	uint32_t nSize;

	constexpr void put( char nByte ) noexcept;
	constexpr void append( const char *pText, uint32_t nLimit = UINT32_MAX ) noexcept;
	constexpr void repeat( char nByte, uint32_t nCount ) noexcept;
	void appendNumber( uint64_t nValue, uint8_t nBase = 10 ) noexcept;
} sirDiagnosticBuffer_t;

constexpr void sirDiagnosticBuffer_s::put( const char nByte ) noexcept
{
	if ( nSize < nCapacity - 1 ) {
		pData[nSize++] = nByte;
		pData[nSize] = '\0';
	}
}

constexpr void sirDiagnosticBuffer_s::append( const char *pText, uint32_t nLimit ) noexcept
{
	if ( pText == nullptr ) {
		return;
	}
	for ( uint32_t i = 0; i < nLimit && nSize < nCapacity - 1 && pText[i] != '\0'; ++i ) {
		put( pText[i] );
	}
}

constexpr void sirDiagnosticBuffer_s::repeat( const char nByte, const uint32_t nCount ) noexcept
{
	for ( uint32_t i = 0; i < nCount && nSize < nCapacity - 1; ++i ) {
		put( nByte );
	}
}

#endif // __SIR_DIAGNOSTICBUFFER_H__
