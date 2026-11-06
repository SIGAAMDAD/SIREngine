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

#include <Scripting/Language/SIRLangLexer.h>
#include <string.h>

typedef struct sirLangCursor_s
{
	uint32_t nOffset;
	uint32_t nLine;
	uint32_t nColumn;
} sirLangCursor_t;

static sirLangCursor_t sirLangCursor( const sirLangLexer_t *pLexer ) noexcept
{
	return { pLexer->nOffset, pLexer->nLine, pLexer->nColumn };
}

static sirLangToken_t sirLangToken( sirLangCursor_t start, sirLangTokenKind_t eKind, uint32_t nLength, sirLangLexError_t eError ) noexcept
{
	return { eKind, eError, start.nOffset, nLength, start.nLine, start.nColumn };
}

static bool sirLangIsDigit( uint8_t iByte ) noexcept
{

	return iByte >= '0' && iByte <= '9';
}

static bool sirLangIsAsciiLetter( uint8_t iByte ) noexcept
{
	return ( iByte >= 'A' && iByte <= 'Z' ) || ( iByte >= 'a' && iByte <= 'z' );
}

static bool sirLangIsIdentifierByte( uint8_t iByte ) noexcept
{
	return sirLangIsAsciiLetter( iByte ) || sirLangIsDigit( iByte ) || iByte == '_' || iByte >= 0x80;
}

static bool sirLangIsPunctuation( uint8_t iByte ) noexcept
{
	static constexpr char s_szPunctuation[] = "()[]{}.,:;+-*/%&|^!?=<>@~";
	for ( uint32_t iChar = 0; s_szPunctuation[iChar] != '\0'; ++iChar ) {
		if ( iByte == ( uint8_t )s_szPunctuation[iChar] ) {
			return true;
		}
	}
	return false;
}

static bool sirLangIsNewline( uint8_t iByte ) noexcept
{
	return iByte == '\n' || iByte == '\r';
}

static void sirLangConsumeByte( sirLangLexer_t *pLexer ) noexcept
{
	++pLexer->nOffset;
	++pLexer->nColumn;
}

static void sirLangConsumeNewline( sirLangLexer_t *pLexer ) noexcept
{
	const uint8_t iFirst = pLexer->source.pBytes[pLexer->nOffset];
	sirLangConsumeByte( pLexer );
	if ( iFirst == '\r' && pLexer->nOffset < pLexer->source.nBytes && pLexer->source.pBytes[pLexer->nOffset] == '\n' ) {
		sirLangConsumeByte( pLexer );
	}
	++pLexer->nLine;
	pLexer->nColumn = 1;
	pLexer->bLineStart = true;
}

// Zero indicates an invalid UTF-8 encoding or non-scalar code point.
static uint32_t sirLangUtf8Bytes( const uint8_t *pData, uint32_t nRemaining ) noexcept
{
	if ( nRemaining == 0 ) {
		return 0;
	}

	const uint8_t iFirst = pData[0];
	if ( iFirst < 0x80 ) {
		return 1;
	}
	uint32_t nLength = 0;
	if ( iFirst >= 0xc2 && iFirst <= 0xdf ) {
		nLength = 2;
	} else if ( iFirst >= 0xe0 && iFirst <= 0xef ) {
		nLength = 3;
	} else if ( iFirst >= 0xf0 && iFirst <= 0xf4 ) {
		nLength = 4;
	} else {
		return 0;
	}
	if ( nRemaining < nLength ) {
		return 0;
	}

	for ( uint32_t iByte = 1; iByte < nLength; ++iByte ) {
		if ( ( pData[iByte] & 0xc0 ) != 0x80 ) {
			return 0;
		}
	}
	if ( nLength == 3 ) {
		if ( ( iFirst == 0xe0 && pData[1] < 0xa0 ) || ( iFirst == 0xed && pData[1] >= 0xa0 ) ) {
			return 0;
		}
	}
	if ( nLength == 4 ) {
		if ( ( iFirst == 0xf0 && pData[1] < 0x90 ) || ( iFirst == 0xf4 && pData[1] >= 0x90 ) ) {
			return 0;
		}
	}
	return nLength;
}

static sirLangToken_t sirLangReadIdentifier( sirLangLexer_t *pLexer ) noexcept
{
	const sirLangCursor_t start = sirLangCursor( pLexer );
	const uint8_t *pBytes = pLexer->source.pBytes;
	while ( pLexer->nOffset < pLexer->source.nBytes ) {
		const uint8_t iByte = pBytes[pLexer->nOffset];
		if ( !sirLangIsIdentifierByte( iByte ) ) {
			break;
		}
		if ( iByte >= 0x80 ) {
			const uint32_t nUtf8 = sirLangUtf8Bytes( pBytes + pLexer->nOffset, pLexer->source.nBytes - pLexer->nOffset );
			if ( nUtf8 == 0 ) {
				if ( pLexer->nOffset == start.nOffset ) {
					sirLangConsumeByte( pLexer );
					return sirLangToken( start, SIR_LANG_TOKEN_ERROR, 1, SIR_LANG_LEX_INVALID_UTF8 );
				}
				break;
			}
			pLexer->nOffset += nUtf8;
			++pLexer->nColumn;
			continue;
		}
		sirLangConsumeByte( pLexer );
	}

	const uint32_t nLength = pLexer->nOffset - start.nOffset;
	const sirLangTokenKind_t eKind =
	    SIRLangIsKeyword( pBytes + start.nOffset, nLength ) ? SIR_LANG_TOKEN_KEYWORD : SIR_LANG_TOKEN_IDENTIFIER;
	return sirLangToken( start, eKind, nLength, SIR_LANG_LEX_OK );
}

static sirLangToken_t sirLangReadNumber( sirLangLexer_t *pLexer ) noexcept
{
	const sirLangCursor_t start = sirLangCursor( pLexer );
	const uint8_t *pBytes = pLexer->source.pBytes;
	const bool bPrefixed = pLexer->nOffset + 1 < pLexer->source.nBytes && pBytes[pLexer->nOffset] == '0' &&
	                       ( pBytes[pLexer->nOffset + 1] == 'x' || pBytes[pLexer->nOffset + 1] == 'b' );
	bool bFraction = false;
	while ( pLexer->nOffset < pLexer->source.nBytes ) {
		const uint8_t iByte = pBytes[pLexer->nOffset];
		if ( sirLangIsIdentifierByte( iByte ) && iByte < 0x80 ) {
			sirLangConsumeByte( pLexer );
			continue;
		}
		if ( !bPrefixed && !bFraction && iByte == '.' && pLexer->nOffset + 1 < pLexer->source.nBytes &&
		     sirLangIsDigit( pBytes[pLexer->nOffset + 1] ) ) {
			bFraction = true;
			sirLangConsumeByte( pLexer );
			continue;
		}
		if ( !bPrefixed && ( iByte == '-' || iByte == '+' ) && pLexer->nOffset > start.nOffset &&
		     ( pBytes[pLexer->nOffset - 1] == 'e' || pBytes[pLexer->nOffset - 1] == 'E' ) && pLexer->nOffset + 1 < pLexer->source.nBytes &&
		     sirLangIsDigit( pBytes[pLexer->nOffset + 1] ) ) {
			sirLangConsumeByte( pLexer );
			continue;
		}
		break;
	}
	return sirLangToken( start, SIR_LANG_TOKEN_NUMBER, pLexer->nOffset - start.nOffset, SIR_LANG_LEX_OK );
}

static sirLangToken_t sirLangReadString( sirLangLexer_t *pLexer ) noexcept
{
	const sirLangCursor_t start = sirLangCursor( pLexer );
	const uint8_t *pBytes = pLexer->source.pBytes;
	const uint8_t iQuote = pBytes[pLexer->nOffset];
	sirLangConsumeByte( pLexer );

	while ( pLexer->nOffset < pLexer->source.nBytes ) {
		const uint8_t iByte = pBytes[pLexer->nOffset];
		if ( sirLangIsNewline( iByte ) ) {
			break;
		}
		if ( iByte == iQuote ) {
			sirLangConsumeByte( pLexer );
			return sirLangToken( start, SIR_LANG_TOKEN_STRING, pLexer->nOffset - start.nOffset, SIR_LANG_LEX_OK );
		}
		if ( iByte == '\\' ) {
			sirLangConsumeByte( pLexer );
			if ( pLexer->nOffset >= pLexer->source.nBytes || sirLangIsNewline( pBytes[pLexer->nOffset] ) ) {
				break;
			}
			// Escape grammar belongs to the string decoder. Lexing preserves it.
		}
		if ( pBytes[pLexer->nOffset] >= 0x80 ) {
			const uint32_t nUtf8 = sirLangUtf8Bytes( pBytes + pLexer->nOffset, pLexer->source.nBytes - pLexer->nOffset );
			if ( nUtf8 == 0 ) {
				sirLangConsumeByte( pLexer );
				return sirLangToken( start, SIR_LANG_TOKEN_ERROR, pLexer->nOffset - start.nOffset, SIR_LANG_LEX_INVALID_UTF8 );
			}
			pLexer->nOffset += nUtf8;
			++pLexer->nColumn;
			continue;
		}
		sirLangConsumeByte( pLexer );
	}
	return sirLangToken( start, SIR_LANG_TOKEN_ERROR, pLexer->nOffset - start.nOffset, SIR_LANG_LEX_UNTERMINATED_STRING );
}

bool SIRLangIsKeyword( const uint8_t *pBytes, uint32_t nBytes ) noexcept
{
	if ( pBytes == nullptr || nBytes == 0 ) {
		return false;
	}
	static constexpr const char *s_pszKeywords[] = {
	    "behavior", "class", "uses",       "function", "returns",   "var",   "this",     "on",   "event", "if",   "else",
	    "switch",   "case",  "default",    "return",   "for",       "in",    "mutate",   "emit", "spawn", "at",   "destroy",
	    "after",    "state", "transition", "schema",   "condition", "bank",  "field",    "and",  "or",    "not",  "xor",
	    "import",   "as",    "interface",  "enum",     "record",    "break", "continue", "ON",   "OFF",   "true", "false" };
	for ( uint32_t iWord = 0; iWord < sizeof( s_pszKeywords ) / sizeof( s_pszKeywords[0] ); ++iWord ) {
		const size_t nKeywordBytes = strlen( s_pszKeywords[iWord] );
		if ( nBytes == nKeywordBytes && memcmp( pBytes, s_pszKeywords[iWord], nBytes ) == 0 ) {
			return true;
		}
	}
	return false;
}

void SIRLangLexerInit( sirLangLexer_t *pLexer, const sirLangSourceView_t& source ) noexcept
{
	if ( pLexer == nullptr ) {
		return;
	}
	memset( pLexer, 0, sizeof( *pLexer ) );
	pLexer->source = source;
	pLexer->nLine = 1;
	pLexer->nColumn = 1;
	pLexer->nIndentDepth = 1;
	pLexer->bLineStart = true;
	pLexer->bInvalidSource = source.pBytes == nullptr && source.nBytes > 0;
}

sirLangToken_t SIRLangLexerNext( sirLangLexer_t *pLexer ) noexcept
{
	if ( pLexer == nullptr ) {
		return { SIR_LANG_TOKEN_ERROR, SIR_LANG_LEX_INVALID_SOURCE, 0, 0, 0, 0 };
	}
	const sirLangCursor_t initial = sirLangCursor( pLexer );
	if ( pLexer->bInvalidSource ) {
		pLexer->bInvalidSource = false;
		pLexer->bFinished = true;
		return sirLangToken( initial, SIR_LANG_TOKEN_ERROR, 0, SIR_LANG_LEX_INVALID_SOURCE );
	}
	if ( pLexer->bFinished ) {
		return sirLangToken( initial, SIR_LANG_TOKEN_END, 0, SIR_LANG_LEX_OK );
	}

	const uint8_t *pBytes = pLexer->source.pBytes;
	for ( ;; ) {
		if ( pLexer->nOffset == pLexer->source.nBytes ) {
			const sirLangCursor_t end = sirLangCursor( pLexer );
			pLexer->bFinished = true;
			return sirLangToken( end, SIR_LANG_TOKEN_END, 0, SIR_LANG_LEX_OK );
		}
		const uint8_t iByte = pBytes[pLexer->nOffset];
		if ( sirLangIsNewline( iByte ) ) {
			sirLangConsumeNewline( pLexer );
			continue;
		}
		if ( iByte == ' ' || iByte == '\t' ) {
			sirLangConsumeByte( pLexer );
			continue;
		}
		if ( iByte == '#' || ( iByte == '/' && pLexer->nOffset + 1 < pLexer->source.nBytes && pBytes[pLexer->nOffset + 1] == '/' ) ) {
			while ( pLexer->nOffset < pLexer->source.nBytes && !sirLangIsNewline( pBytes[pLexer->nOffset] ) ) {
				sirLangConsumeByte( pLexer );
			}
			continue;
		}
		break;
	}
	const sirLangCursor_t start = sirLangCursor( pLexer );
	const uint8_t iByte = pBytes[pLexer->nOffset];
	if ( sirLangIsAsciiLetter( iByte ) || iByte == '_' || iByte >= 0x80 ) {
		return sirLangReadIdentifier( pLexer );
	}
	if ( sirLangIsDigit( iByte ) ) {
		return sirLangReadNumber( pLexer );
	}
	if ( iByte == '"' || iByte == '\'' ) {
		return sirLangReadString( pLexer );
	}
	if ( sirLangIsPunctuation( iByte ) ) {
		sirLangConsumeByte( pLexer );
		return sirLangToken( start, SIR_LANG_TOKEN_PUNCTUATION, 1, SIR_LANG_LEX_OK );
	}

	sirLangConsumeByte( pLexer );
	return sirLangToken( start, SIR_LANG_TOKEN_ERROR, 1, SIR_LANG_LEX_INVALID_CHARACTER );
}
