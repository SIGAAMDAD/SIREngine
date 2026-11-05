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

#include <Core/Platform/SIRPlatformRootDirectory.h>

static uint8_t sirRootLowerAscii( uint8_t nByte ) noexcept
{
	return nByte >= 'A' && nByte <= 'Z' ? ( uint8_t )( nByte + 'a' - 'A' ) : nByte;
}

static bool sirRootReservedDevice( const char *pszComponent, uint32_t nLength ) noexcept
{
	uint32_t nStem = 0;
	while ( nStem < nLength && pszComponent[nStem] != '.' ) {
		++nStem;
	}
	if ( nStem == 3 ) {
		const uint8_t a = sirRootLowerAscii( ( uint8_t )pszComponent[0] );
		const uint8_t b = sirRootLowerAscii( ( uint8_t )pszComponent[1] );
		const uint8_t c = sirRootLowerAscii( ( uint8_t )pszComponent[2] );
		return ( a == 'c' && b == 'o' && c == 'n' ) || ( a == 'p' && b == 'r' && c == 'n' ) || ( a == 'a' && b == 'u' && c == 'x' ) ||
		       ( a == 'n' && b == 'u' && c == 'l' );
	}
	if ( nStem == 4 ) {
		const uint8_t a = sirRootLowerAscii( ( uint8_t )pszComponent[0] );
		const uint8_t b = sirRootLowerAscii( ( uint8_t )pszComponent[1] );
		const uint8_t c = sirRootLowerAscii( ( uint8_t )pszComponent[2] );
		const uint8_t d = ( uint8_t )pszComponent[3];
		return d >= '1' && d <= '9' && ( ( a == 'c' && b == 'o' && c == 'm' ) || ( a == 'l' && b == 'p' && c == 't' ) );
	}
	return false;
}

bool SIRPlatformRootDirectory::validRelative( const char *pszRelativePath ) noexcept
{
	if ( pszRelativePath == nullptr || pszRelativePath[0] == '\0' ) {
		return false;
	}
	uint32_t nStart = 0;
	for ( uint32_t i = 0; i < SIR_PLATFORM_ROOTED_MAX_RELATIVE_BYTES; ++i ) {
		const uint8_t nByte = ( uint8_t )pszRelativePath[i];
		if ( nByte == '\0' || nByte == '/' ) {
			const uint32_t nLength = i - nStart;
			if ( nLength == 0 || ( nLength == 1 && pszRelativePath[nStart] == '.' ) ||
			     ( nLength == 2 && pszRelativePath[nStart] == '.' && pszRelativePath[nStart + 1] == '.' ) ||
			     pszRelativePath[i - 1] == '.' || pszRelativePath[i - 1] == ' ' ||
			     sirRootReservedDevice( pszRelativePath + nStart, nLength ) ) {
				return false;
			}
			if ( nByte == '\0' ) {
				return true;
			}
			nStart = i + 1;
			continue;
		}
		if ( nByte < 0x20 || nByte == 0x7f || nByte == '\\' || nByte == ':' || nByte == '%' || nByte == '"' || nByte == '<' ||
		     nByte == '>' || nByte == '|' || nByte == '?' || nByte == '*' ) {
			return false;
		}
	}
	return false;
}
