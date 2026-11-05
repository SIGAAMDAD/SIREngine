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

#include <Core/VFS/SIRVFSPath.h>

#include <string.h>

static bool sirVFSValidUtf8( const uint8_t *pBytes, uint32_t nLength ) noexcept
{
	for ( uint32_t i = 0; i < nLength; ) {
		const uint8_t nFirst = pBytes[i];
		if ( nFirst < 0x80 ) {
			if ( nFirst < 0x20 || nFirst == 0x7f ) {
				return false;
			}
			++i;
			continue;
		}

		uint32_t nWidth = 0;
		if ( nFirst >= 0xc2 && nFirst <= 0xdf ) {
			nWidth = 2;
		} else if ( nFirst >= 0xe0 && nFirst <= 0xef ) {
			nWidth = 3;
		} else if ( nFirst >= 0xf0 && nFirst <= 0xf4 ) {
			nWidth = 4;
		} else {
			return false;
		}
		if ( nWidth > nLength - i ) {
			return false;
		}
		for ( uint32_t nByte = 1; nByte < nWidth; ++nByte ) {
			if ( ( pBytes[i + nByte] & 0xc0 ) != 0x80 ) {
				return false;
			}
		}
		const uint8_t nSecond = pBytes[i + 1];
		if ( ( nFirst == 0xe0 && nSecond < 0xa0 ) || ( nFirst == 0xed && nSecond >= 0xa0 ) || ( nFirst == 0xf0 && nSecond < 0x90 ) ||
		     ( nFirst == 0xf4 && nSecond >= 0x90 ) ) {
			return false;
		}
		i += nWidth;
	}
	return true;
}

static uint8_t sirVFSFoldAscii( uint8_t nByte ) noexcept
{
	return nByte >= 'A' && nByte <= 'Z' ? ( uint8_t )( nByte + ( 'a' - 'A' ) ) : nByte;
}

static bool sirVFSReservedSegment( const char *pszSegment, uint32_t nLength ) noexcept
{
	if ( nLength == 0 || pszSegment[nLength - 1] == ' ' || pszSegment[nLength - 1] == '.' ) {
		return true;
	}
	uint32_t nStem = 0;
	while ( nStem < nLength && pszSegment[nStem] != '.' ) {
		++nStem;
	}
	if ( nStem == 3 ) {
		const uint8_t a = sirVFSFoldAscii( ( uint8_t )pszSegment[0] );
		const uint8_t b = sirVFSFoldAscii( ( uint8_t )pszSegment[1] );
		const uint8_t c = sirVFSFoldAscii( ( uint8_t )pszSegment[2] );
		if ( ( a == 'c' && b == 'o' && c == 'n' ) || ( a == 'p' && b == 'r' && c == 'n' ) || ( a == 'a' && b == 'u' && c == 'x' ) ||
		     ( a == 'n' && b == 'u' && c == 'l' ) ) {
			return true;
		}
	}
	if ( nStem == 4 ) {
		const uint8_t a = sirVFSFoldAscii( ( uint8_t )pszSegment[0] );
		const uint8_t b = sirVFSFoldAscii( ( uint8_t )pszSegment[1] );
		const uint8_t c = sirVFSFoldAscii( ( uint8_t )pszSegment[2] );
		const uint8_t d = ( uint8_t )pszSegment[3];
		if ( d >= '1' && d <= '9' && ( ( a == 'c' && b == 'o' && c == 'm' ) || ( a == 'l' && b == 'p' && c == 't' ) ) ) {
			return true;
		}
	}
	return false;
}

sirVFSPathResult_t SIRVFSPath::normalize( const char *pszInput, uint32_t nInputBytes, sirVFSPath_t *pOutput ) noexcept
{
	if ( pOutput == nullptr || pszInput == nullptr || nInputBytes == 0 ) {
		return SIR_VFS_PATH_INVALID_ARGUMENT;
	}
	// Reject oversized inputs before reading or computing a potentially
	// overflowing buffer length. Output is only changed on success.
	if ( nInputBytes >= SIR_VFS_MAX_PATH_BYTES ) {
		return SIR_VFS_PATH_TOO_LONG;
	}
	if ( !sirVFSValidUtf8( ( const uint8_t * )pszInput, nInputBytes ) ) {
		return SIR_VFS_PATH_INVALID_ENCODING;
	}

	uint32_t nNamespaceBytes = 0;
	while ( nNamespaceBytes < nInputBytes && pszInput[nNamespaceBytes] != ':' ) {
		const uint8_t nByte = sirVFSFoldAscii( ( uint8_t )pszInput[nNamespaceBytes] );
		if ( !( nByte >= 'a' && nByte <= 'z' ) &&
		     !( nNamespaceBytes != 0 && ( ( nByte >= '0' && nByte <= '9' ) || nByte == '_' || nByte == '-' ) ) ) {
			return SIR_VFS_PATH_INVALID_NAMESPACE;
		}
		++nNamespaceBytes;
	}
	if ( nNamespaceBytes < 2 || nNamespaceBytes > SIR_VFS_MAX_NAMESPACE_BYTES || nNamespaceBytes + 1 >= nInputBytes ||
	     pszInput[nNamespaceBytes + 1] != '/' ) {
		return SIR_VFS_PATH_INVALID_NAMESPACE;
	}
	sirVFSPath_t candidate = {};
	for ( uint32_t i = 0; i < nNamespaceBytes; ++i ) {
		candidate.szPath[i] = ( char )sirVFSFoldAscii( ( uint8_t )pszInput[i] );
	}
	candidate.szPath[nNamespaceBytes] = ':';
	candidate.szPath[nNamespaceBytes + 1] = '/';
	candidate.nNamespaceLength = ( uint8_t )nNamespaceBytes;
	uint32_t nWritten = nNamespaceBytes + 2;
	uint16_t szComponentStart[SIR_VFS_MAX_PATH_BYTES / 2] = {};
	uint32_t nComponents = 0;
	uint32_t nInput = nNamespaceBytes + 2;
	while ( nInput < nInputBytes ) {
		while ( nInput < nInputBytes && ( pszInput[nInput] == '/' || pszInput[nInput] == '\\' ) ) {
			++nInput;
		}
		if ( nInput == nInputBytes ) {
			break;
		}
		const uint32_t nStart = nInput;
		while ( nInput < nInputBytes && pszInput[nInput] != '/' && pszInput[nInput] != '\\' ) {
			const char ch = pszInput[nInput];
			if ( ch == ':' || ch == '%' || ch == '<' || ch == '>' || ch == '|' || ch == '"' || ch == '?' || ch == '*' ) {
				return SIR_VFS_PATH_FORBIDDEN;
			}
			++nInput;
		}
		const uint32_t nLength = nInput - nStart;
		if ( nLength == 1 && pszInput[nStart] == '.' ) {
			continue;
		}
		if ( nLength == 2 && pszInput[nStart] == '.' && pszInput[nStart + 1] == '.' ) {
			if ( nComponents == 0 ) {
				return SIR_VFS_PATH_FORBIDDEN;
			}
			nWritten = szComponentStart[--nComponents];
			continue;
		}
		if ( sirVFSReservedSegment( pszInput + nStart, nLength ) ) {
			return SIR_VFS_PATH_INVALID_SEGMENT;
		}
		if ( nWritten + ( nComponents != 0 ? 1u : 0u ) + nLength >= SIR_VFS_MAX_PATH_BYTES ) {
			return SIR_VFS_PATH_TOO_LONG;
		}
		szComponentStart[nComponents++] = ( uint16_t )nWritten;
		if ( nComponents > 1 ) {
			candidate.szPath[nWritten++] = '/';
		}
		memcpy( candidate.szPath + nWritten, pszInput + nStart, nLength );
		nWritten += nLength;
	}
	candidate.nLength = ( uint16_t )nWritten;
	candidate.szPath[nWritten] = '\0';
	*pOutput = candidate;
	return SIR_VFS_PATH_OK;
}
