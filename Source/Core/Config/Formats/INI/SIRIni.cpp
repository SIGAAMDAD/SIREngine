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

#include <Core/Config/Formats/INI/SIRIni.h>
#include <string.h>

static bool sirIniSpace( uint8_t nByte ) noexcept
{
	return nByte == ' ' || nByte == '\t';
}

static bool sirIniValidUtf8( const char *pszValue, uint32_t nLength ) noexcept
{
	const uint8_t *pBytes = ( const uint8_t * )pszValue;
	for ( uint32_t iByte = 0; iByte < nLength; ) {
		const uint8_t nHead = pBytes[iByte];
		if ( nHead == 0 ) {
			return false;
		}
		if ( nHead < 128 ) {
			++iByte;
			continue;
		}
		uint32_t nParts = 0;
		uint32_t nCode = 0;
		if ( nHead >= 0xc2 && nHead <= 0xdf ) {
			nParts = 1;
			nCode = nHead & 31;
		} else if ( nHead >= 0xe0 && nHead <= 0xef ) {
			nParts = 2;
			nCode = nHead & 15;
		} else if ( nHead >= 0xf0 && nHead <= 0xf4 ) {
			nParts = 3;
			nCode = nHead & 7;
		} else {
			return false;
		}
		if ( nParts > nLength - iByte - 1 ) {
			return false;
		}
		for ( uint32_t iPart = 0; iPart < nParts; ++iPart ) {
			const uint8_t nPart = pBytes[iByte + iPart + 1];
			if ( ( nPart & 0xc0 ) != 0x80 ) {
				return false;
			}
			nCode = ( nCode << 6 ) | ( nPart & 63 );
		}
		if ( ( nParts == 2 && nCode < 0x800 ) || ( nParts == 3 && nCode < 0x10000 ) || ( nCode >= 0xd800 && nCode <= 0xdfff ) ||
		     nCode > 0x10ffff ) {
			return false;
		}
		iByte += nParts + 1;
	}
	return true;
}

typedef struct sirIniLocation_s
{
	uint64_t nOffset;
	uint32_t nLine;
	uint32_t nColumn;
} sirIniLocation_t;

static sirConfigResult_t sirIniFail( sirConfigResult_t eError, sirIniLocation_t location, sirConfigDiagnostic_t *pDiagnostic ) noexcept
{
	if ( pDiagnostic != nullptr ) {
		*pDiagnostic = {};
		pDiagnostic->eResult = eError;
		pDiagnostic->nLine = location.nLine;
		pDiagnostic->nColumn = location.nColumn;
		pDiagnostic->nByteOffset = location.nOffset;
	}
	return eError;
}

static bool sirIniSameAscii( const char *pszLeft, const char *pszRight ) noexcept
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

sirConfigResult_t SIRIniCodec::parse( const sirConfigInput_t& input, const sirConfigLimits_t& limits, const sirIniStorage_t& storage,
                                      sirIniParseResult_t *pResult ) noexcept
{
	if ( pResult == nullptr ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}
	*pResult = {};
	uint32_t *pCount = &pResult->nCount;
	sirConfigDiagnostic_t *pDiagnostic = &pResult->diagnostic;
	if ( ( input.nBytes != 0 && input.pBytes == nullptr ) || storage.pEntries == nullptr || storage.pszValues == nullptr ||
	     storage.nEntryCapacity == 0 || storage.nValueStride < 2 || storage.nEntryCapacity > SIZE_MAX / storage.nValueStride ||
	     storage.nValueCapacity < ( size_t )storage.nEntryCapacity * storage.nValueStride || limits.nMaxInputBytes == 0 ||
	     limits.nMaxScalarBytes == 0 || limits.nMaxNodes == 0 ) {
		return sirIniFail( SIR_CONFIG_INVALID_ARGUMENT, { 0, 1, 1 }, pDiagnostic );
	}
	*pCount = 0;
	if ( input.nBytes > limits.nMaxInputBytes ) {
		return sirIniFail( SIR_CONFIG_LIMIT_EXCEEDED, { 0, 1, 1 }, pDiagnostic );
	}
	char szSection[SIR_INI_MAX_SECTION_BYTES + 1] = {};
	uint64_t nOffset = 0;
	uint32_t nLine = 1;
	while ( nOffset < input.nBytes ) {
		const uint64_t nStart = nOffset;
		while ( nOffset < input.nBytes && input.pBytes[nOffset] != '\n' && input.pBytes[nOffset] != '\r' ) {
			if ( input.pBytes[nOffset] == 0 ) {
				return sirIniFail( SIR_CONFIG_INVALID_ENCODING, { nOffset, nLine, ( uint32_t )( nOffset - nStart + 1 ) }, pDiagnostic );
			}
			++nOffset;
		}
		const uint64_t nEnd = nOffset;
		if ( nOffset < input.nBytes ) {
			const uint8_t nBreak = input.pBytes[nOffset++];
			if ( nOffset < input.nBytes && input.pBytes[nOffset] != nBreak &&
			     ( input.pBytes[nOffset] == '\r' || input.pBytes[nOffset] == '\n' ) ) {
				++nOffset;
			}
		}
		if ( limits.nMaxRowBytes != 0 && nEnd - nStart > limits.nMaxRowBytes ) {
			return sirIniFail( SIR_CONFIG_LIMIT_EXCEEDED, { nStart, nLine, 1 }, pDiagnostic );
		}
		uint64_t nHead = nStart;
		while ( nHead < nEnd && sirIniSpace( input.pBytes[nHead] ) ) {
			++nHead;
		}
		if ( nHead == nEnd || input.pBytes[nHead] == ';' || input.pBytes[nHead] == '#' ) {
			++nLine;
			continue;
		}
		if ( input.pBytes[nHead] == '[' ) {
			uint64_t nClose = nHead + 1;
			while ( nClose < nEnd && input.pBytes[nClose] != ']' ) {
				++nClose;
			}
			const size_t nLength = ( size_t )( nClose - nHead - 1 );
			if ( nClose == nEnd || nLength == 0 || nLength > SIR_INI_MAX_SECTION_BYTES ) {
				return sirIniFail( SIR_CONFIG_SYNTAX_ERROR, { nHead, nLine, 1 }, pDiagnostic );
			}
			for ( uint64_t iByte = nClose + 1; iByte < nEnd; ++iByte ) {
				if ( !sirIniSpace( input.pBytes[iByte] ) ) {
					return sirIniFail( SIR_CONFIG_SYNTAX_ERROR, { iByte, nLine, 1 }, pDiagnostic );
				}
			}
			memcpy( szSection, input.pBytes + nHead + 1, nLength );
			szSection[nLength] = 0;
			++nLine;
			continue;
		}
		uint64_t nEqual = nHead;
		while ( nEqual < nEnd && input.pBytes[nEqual] != '=' ) {
			++nEqual;
		}
		if ( nEqual == nEnd ) {
			return sirIniFail( SIR_CONFIG_SYNTAX_ERROR, { nHead, nLine, 1 }, pDiagnostic );
		}
		uint64_t nKeyEnd = nEqual;
		while ( nKeyEnd > nHead && sirIniSpace( input.pBytes[nKeyEnd - 1] ) ) {
			--nKeyEnd;
		}
		const size_t nKeyLen = ( size_t )( nKeyEnd - nHead );
		if ( nKeyLen == 0 || nKeyLen > SIR_INI_MAX_KEY_BYTES ) {
			return sirIniFail( SIR_CONFIG_LIMIT_EXCEEDED, { nHead, nLine, 1 }, pDiagnostic );
		}
		if ( *pCount >= storage.nEntryCapacity || *pCount >= limits.nMaxNodes ) {
			return sirIniFail( SIR_CONFIG_LIMIT_EXCEEDED, { nHead, nLine, 1 }, pDiagnostic );
		}
		sirIniEntry_t& entry = storage.pEntries[*pCount];
		entry = {};
		entry.nLine = nLine;
		memcpy( entry.szKey, input.pBytes + nHead, nKeyLen );
		memcpy( entry.szSection, szSection, strlen( szSection ) + 1 );
		for ( uint32_t iPrev = 0; iPrev < *pCount; ++iPrev ) {
			const sirIniEntry_t& prev = storage.pEntries[iPrev];
			if ( sirIniSameAscii( prev.szKey, entry.szKey ) && sirIniSameAscii( prev.szSection, entry.szSection ) ) {
				return sirIniFail( SIR_CONFIG_DUPLICATE_KEY, { nHead, nLine, 1 }, pDiagnostic );
			}
		}
		uint64_t nValueStart = nEqual + 1;
		while ( nValueStart < nEnd && sirIniSpace( input.pBytes[nValueStart] ) ) {
			++nValueStart;
		}
		char *pszValue = storage.pszValues + ( size_t )( *pCount ) * storage.nValueStride;
		uint32_t nBytes = 0;
		if ( nValueStart < nEnd && input.pBytes[nValueStart] == '"' ) {
			entry.bQuoted = true;
			uint64_t iByte = nValueStart + 1;
			bool bClosed = false;
			for ( ; iByte < nEnd; ++iByte ) {
				uint8_t nByte = input.pBytes[iByte];
				if ( nByte == '"' ) {
					bClosed = true;
					++iByte;
					break;
				}
				if ( nByte == '\\' ) {
					if ( ++iByte == nEnd ) {
						return sirIniFail( SIR_CONFIG_SYNTAX_ERROR, { nStart, nLine, 1 }, pDiagnostic );
					}
					const uint8_t nEscape = input.pBytes[iByte];
					if ( nEscape == 'n' ) {
						nByte = '\n';
					} else if ( nEscape == 'r' ) {
						nByte = '\r';
					} else if ( nEscape == 't' ) {
						nByte = '\t';
					} else if ( nEscape == '"' || nEscape == '\\' ) {
						nByte = nEscape;
					} else {
						return sirIniFail( SIR_CONFIG_SYNTAX_ERROR, { iByte, nLine, 1 }, pDiagnostic );
					}
				}
				if ( nBytes + 1 >= storage.nValueStride || nBytes + 1 > limits.nMaxScalarBytes ) {
					return sirIniFail( SIR_CONFIG_LIMIT_EXCEEDED, { nStart, nLine, 1 }, pDiagnostic );
				}
				pszValue[nBytes++] = ( char )nByte;
			}
			if ( !bClosed ) {
				return sirIniFail( SIR_CONFIG_SYNTAX_ERROR, { nStart, nLine, 1 }, pDiagnostic );
			}
			while ( iByte < nEnd && sirIniSpace( input.pBytes[iByte] ) ) {
				++iByte;
			}
			if ( iByte < nEnd && input.pBytes[iByte] != ';' && input.pBytes[iByte] != '#' ) {
				return sirIniFail( SIR_CONFIG_SYNTAX_ERROR, { iByte, nLine, 1 }, pDiagnostic );
			}
		} else {
			uint64_t nStop = nValueStart;
			while ( nStop < nEnd && input.pBytes[nStop] != ';' && input.pBytes[nStop] != '#' ) {
				++nStop;
			}
			while ( nStop > nValueStart && sirIniSpace( input.pBytes[nStop - 1] ) ) {
				--nStop;
			}
			if ( nStop - nValueStart >= storage.nValueStride || nStop - nValueStart > limits.nMaxScalarBytes ) {
				return sirIniFail( SIR_CONFIG_LIMIT_EXCEEDED, { nStart, nLine, 1 }, pDiagnostic );
			}
			nBytes = ( uint32_t )( nStop - nValueStart );
			if ( nBytes != 0 ) {
				memcpy( pszValue, input.pBytes + nValueStart, nBytes );
			}
		}
		if ( !sirIniValidUtf8( pszValue, nBytes ) ) {
			return sirIniFail( SIR_CONFIG_INVALID_ENCODING, { nStart, nLine, 1 }, pDiagnostic );
		}
		pszValue[nBytes] = 0;
		entry.pszValue = pszValue;
		entry.nValueBytes = nBytes;
		++*pCount;
		++nLine;
	}
	if ( pDiagnostic != nullptr ) {
		*pDiagnostic = {};
		pDiagnostic->eResult = SIR_CONFIG_OK;
	}
	return SIR_CONFIG_OK;
}

// Writer validates the entire size bound before emitting any bytes to a sink.
static sirConfigResult_t sirIniEmit( const sirConfigSink_t& sink, const void *pBytes, uint64_t nBytes ) noexcept
{
	return nBytes == 0 ? SIR_CONFIG_OK : sink.pfnWrite( sink.pContext, ( const uint8_t * )pBytes, nBytes );
}

sirConfigResult_t SIRIniCodec::write( const sirIniEntry_t *pEntries, uint32_t nCount, const sirConfigSink_t& sink,
                                      uint64_t nMaxBytes ) noexcept
{
	if ( sink.pfnWrite == nullptr || ( nCount != 0 && pEntries == nullptr ) || nMaxBytes == 0 ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}
	uint64_t nTotal = 0;
	const char *pszSection = "";
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const sirIniEntry_t& entry = pEntries[i];
		const size_t nSection = strlen( entry.szSection );
		const size_t nKey = strlen( entry.szKey );
		if ( nSection == 0 || nSection > SIR_INI_MAX_SECTION_BYTES || nKey == 0 || nKey > SIR_INI_MAX_KEY_BYTES ||
		     ( entry.nValueBytes != 0 && entry.pszValue == nullptr ) || !sirIniValidUtf8( entry.pszValue, entry.nValueBytes ) ) {
			return SIR_CONFIG_INVALID_ARGUMENT;
		}
		if ( i > 0 ) {
			const int nCmp = strcmp( pEntries[i - 1].szSection, entry.szSection );
			if ( nCmp > 0 || ( nCmp == 0 && strcmp( pEntries[i - 1].szKey, entry.szKey ) >= 0 ) ) {
				return SIR_CONFIG_DUPLICATE_KEY;
			}
		}
		if ( strcmp( pszSection, entry.szSection ) != 0 ) {
			nTotal += nSection + 3;
			pszSection = entry.szSection;
		}
		nTotal += nKey + 6;
		for ( uint32_t j = 0; j < entry.nValueBytes; ++j ) {
			const uint8_t nByte = ( uint8_t )entry.pszValue[j];
			nTotal += nByte == '\n' || nByte == '\r' || nByte == '\t' || nByte == '\\' || nByte == '"' ? 2 : 1;
		}
		if ( nTotal > nMaxBytes ) {
			return SIR_CONFIG_LIMIT_EXCEEDED;
		}
	}
	pszSection = "";
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const sirIniEntry_t& entry = pEntries[i];
		if ( strcmp( pszSection, entry.szSection ) != 0 ) {
			if ( sirIniEmit( sink, "[", 1 ) != SIR_CONFIG_OK ||
			     sirIniEmit( sink, entry.szSection, strlen( entry.szSection ) ) != SIR_CONFIG_OK ||
			     sirIniEmit( sink, "]\n", 2 ) != SIR_CONFIG_OK ) {
				return SIR_CONFIG_IO_ERROR;
			}
			pszSection = entry.szSection;
		}
		if ( sirIniEmit( sink, entry.szKey, strlen( entry.szKey ) ) != SIR_CONFIG_OK || sirIniEmit( sink, " = \"", 4 ) != SIR_CONFIG_OK ) {
			return SIR_CONFIG_IO_ERROR;
		}
		for ( uint32_t j = 0; j < entry.nValueBytes; ++j ) {
			const char nByte = entry.pszValue[j];
			char szEscape[2] = { '\\', 0 };
			if ( nByte == '\n' ) {
				szEscape[1] = 'n';
			} else if ( nByte == '\r' ) {
				szEscape[1] = 'r';
			} else if ( nByte == '\t' ) {
				szEscape[1] = 't';
			} else if ( nByte == '\\' || nByte == '"' ) {
				szEscape[1] = nByte;
			}
			if ( szEscape[1] != 0 ) {
				if ( sirIniEmit( sink, szEscape, 2 ) != SIR_CONFIG_OK ) {
					return SIR_CONFIG_IO_ERROR;
				}
			} else if ( sirIniEmit( sink, &nByte, 1 ) != SIR_CONFIG_OK ) {
				return SIR_CONFIG_IO_ERROR;
			}
		}
		if ( sirIniEmit( sink, "\"\n", 2 ) != SIR_CONFIG_OK ) {
			return SIR_CONFIG_IO_ERROR;
		}
	}
	return SIR_CONFIG_OK;
}
