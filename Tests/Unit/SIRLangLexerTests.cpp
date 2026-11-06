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
#include <assert.h>
#include <stdio.h>
#include <string.h>

static sirLangSourceView_t sourceFromCString( const char *pszSource ) noexcept
{
	return { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) };
}

static sirLangToken_t expectToken( sirLangLexer_t *pLexer, sirLangTokenKind_t eExpected, const char *pszText ) noexcept
{
	const sirLangToken_t token = SIRLangLexerNext( pLexer );
	if ( token.eKind != eExpected ) {
		fprintf( stderr, "kind mismatch at %u:%u: expected %u, got %u (error %u)\n", token.nLine, token.nColumn, ( unsigned )eExpected,
		         ( unsigned )token.eKind, ( unsigned )token.eError );
		assert( token.eKind == eExpected );
	}
	if ( pszText != nullptr ) {
		const uint32_t nExpectedBytes = ( uint32_t )strlen( pszText );
		assert( token.nLength == nExpectedBytes );
		assert( memcmp( pLexer->source.pBytes + token.nOffset, pszText, nExpectedBytes ) == 0 );
	}
	return token;
}

static void testKeywordsAndIndentation( void ) noexcept
{
	const char *pszSource = "behavior Guard uses GuardState:\n"
	                        "    on Hit(amount: u16):\n"
	                        "        let next: u16 = 5u16\n"
	                        "        when next == 0u16:\n"
	                        "            emit Down()\n";
	sirLangLexer_t lexer = {};
	SIRLangLexerInit( &lexer, sourceFromCString( pszSource ) );
	const sirLangToken_t first = expectToken( &lexer, SIR_LANG_TOKEN_KEYWORD, "behavior" );
	assert( first.nLine == 1 && first.nColumn == 1 && first.nOffset == 0 );
	expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "Guard" );
	expectToken( &lexer, SIR_LANG_TOKEN_KEYWORD, "uses" );
	expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "GuardState" );
	expectToken( &lexer, SIR_LANG_TOKEN_PUNCTUATION, ":" );
	expectToken( &lexer, SIR_LANG_TOKEN_NEWLINE, "\n" );
	expectToken( &lexer, SIR_LANG_TOKEN_INDENT, "    " );
	const sirLangToken_t event = expectToken( &lexer, SIR_LANG_TOKEN_KEYWORD, "on" );
	assert( event.nLine == 2 && event.nColumn == 5 );

	uint32_t nIndents = 1;
	uint32_t nDedents = 0;
	bool bSawLiteral = false;
	bool bSawEmit = false;
	for ( uint32_t iToken = 0; iToken < 128; ++iToken ) {
		const sirLangToken_t token = SIRLangLexerNext( &lexer );
		assert( token.eKind != SIR_LANG_TOKEN_ERROR );
		if ( token.eKind == SIR_LANG_TOKEN_INDENT ) {
			++nIndents;
		}
		if ( token.eKind == SIR_LANG_TOKEN_DEDENT ) {
			++nDedents;
		}
		if ( token.eKind == SIR_LANG_TOKEN_NUMBER ) {
			bSawLiteral = true;
		}
		if ( token.eKind == SIR_LANG_TOKEN_KEYWORD && token.nLength == 4 &&
		     memcmp( lexer.source.pBytes + token.nOffset, "emit", 4 ) == 0 ) {
			bSawEmit = true;
		}
		if ( token.eKind == SIR_LANG_TOKEN_END ) {
			break;
		}
	}
	assert( nIndents == 3 && nDedents == 3 );
	assert( bSawLiteral && bSawEmit );
	assert( SIRLangLexerNext( &lexer ).eKind == SIR_LANG_TOKEN_END );
	assert( SIRLangIsKeyword( ( const uint8_t * )"transition", 10 ) );
	assert( !SIRLangIsKeyword( ( const uint8_t * )"transitions", 11 ) );
}

static void testNumbersStringsAndComments( void ) noexcept
{
	const char *pszSource = "# header\r\nnum 0xFF 1.5e-2f32 5u16 # inline\r\n    text \"escaped \\\"quote\\\"\"\r\n";
	sirLangLexer_t lexer = {};
	SIRLangLexerInit( &lexer, sourceFromCString( pszSource ) );
	const sirLangToken_t name = expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "num" );
	assert( name.nLine == 2 && name.nColumn == 1 );
	expectToken( &lexer, SIR_LANG_TOKEN_NUMBER, "0xFF" );
	expectToken( &lexer, SIR_LANG_TOKEN_NUMBER, "1.5e-2f32" );
	expectToken( &lexer, SIR_LANG_TOKEN_NUMBER, "5u16" );
	expectToken( &lexer, SIR_LANG_TOKEN_NEWLINE, "\r\n" );
	expectToken( &lexer, SIR_LANG_TOKEN_INDENT, "    " );
	expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "text" );
	expectToken( &lexer, SIR_LANG_TOKEN_STRING, "\"escaped \\\"quote\\\"\"" );
	expectToken( &lexer, SIR_LANG_TOKEN_NEWLINE, "\r\n" );
	expectToken( &lexer, SIR_LANG_TOKEN_DEDENT, "" );
	expectToken( &lexer, SIR_LANG_TOKEN_END, "" );
}

static void testUnicodeAndErrors( void ) noexcept
{
	const uint8_t szValid[] = { 'a', 0xc3, 0xa9, '\n' };
	sirLangLexer_t lexer = {};
	SIRLangLexerInit( &lexer, { szValid, sizeof( szValid ) } );
	const sirLangToken_t unicode = expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "a\xc3\xa9" );
	assert( unicode.nLength == 3 && unicode.nLine == 1 );
	const sirLangToken_t eol = expectToken( &lexer, SIR_LANG_TOKEN_NEWLINE, "\n" );
	assert( eol.nColumn == 3 );
	expectToken( &lexer, SIR_LANG_TOKEN_END, "" );

	const uint8_t szInvalid[] = { 0xed, 0xa0, 0x80 };
	SIRLangLexerInit( &lexer, { szInvalid, sizeof( szInvalid ) } );
	const sirLangToken_t badUtf8 = SIRLangLexerNext( &lexer );
	assert( badUtf8.eKind == SIR_LANG_TOKEN_ERROR && badUtf8.eError == SIR_LANG_LEX_INVALID_UTF8 );

	SIRLangLexerInit( &lexer, { nullptr, 1 } );
	const sirLangToken_t badSource = SIRLangLexerNext( &lexer );
	assert( badSource.eKind == SIR_LANG_TOKEN_ERROR && badSource.eError == SIR_LANG_LEX_INVALID_SOURCE );
	assert( SIRLangLexerNext( &lexer ).eKind == SIR_LANG_TOKEN_END );

	SIRLangLexerInit( &lexer, sourceFromCString( "    alpha\n  beta\n" ) );
	expectToken( &lexer, SIR_LANG_TOKEN_INDENT, "    " );
	expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "alpha" );
	expectToken( &lexer, SIR_LANG_TOKEN_NEWLINE, "\n" );
	const sirLangToken_t badIndent = SIRLangLexerNext( &lexer );
	assert( badIndent.eKind == SIR_LANG_TOKEN_ERROR && badIndent.eError == SIR_LANG_LEX_INCONSISTENT_INDENT );

	SIRLangLexerInit( &lexer, sourceFromCString( " \talpha\n" ) );
	const sirLangToken_t tabIndent = SIRLangLexerNext( &lexer );
	assert( tabIndent.eKind == SIR_LANG_TOKEN_ERROR && tabIndent.eError == SIR_LANG_LEX_TAB_INDENT );

	SIRLangLexerInit( &lexer, sourceFromCString( "\"unterminated\n" ) );
	const sirLangToken_t badString = SIRLangLexerNext( &lexer );
	assert( badString.eKind == SIR_LANG_TOKEN_ERROR && badString.eError == SIR_LANG_LEX_UNTERMINATED_STRING );
	assert( SIRLangLexerNext( &lexer ).eKind == SIR_LANG_TOKEN_NEWLINE );
}

static void testIndentLimit( void ) noexcept
{
	char szSource[4096] = {};
	uint32_t nBytes = 0;
	for ( uint32_t iLine = 0; iLine < 65; ++iLine ) {
		for ( uint32_t iSpace = 0; iSpace <= iLine; ++iSpace ) {
			szSource[nBytes++] = ' ';
		}
		szSource[nBytes++] = 'x';
		szSource[nBytes++] = '\n';
	}
	assert( nBytes < sizeof( szSource ) );
	sirLangLexer_t lexer = {};
	SIRLangLexerInit( &lexer, { ( const uint8_t * )szSource, nBytes } );
	bool bOverflow = false;
	for ( uint32_t iToken = 0; iToken < 256; ++iToken ) {
		const sirLangToken_t token = SIRLangLexerNext( &lexer );
		if ( token.eError == SIR_LANG_LEX_INDENT_OVERFLOW ) {
			bOverflow = true;
			break;
		}
		assert( token.eKind != SIR_LANG_TOKEN_END );
	}
	assert( bOverflow );
}

int main( void )
{
	testKeywordsAndIndentation();
	testNumbersStringsAndComments();
	testUnicodeAndErrors();
	testIndentLimit();
	puts( "SIR-Lang lexer foundation: PASS" );
	return 0;
}
