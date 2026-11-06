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

static void testModernTokens( void ) noexcept
{
	static constexpr char szSource[] = "schema PlayerState {\n"
	                                   "    condition bank Flags : u32 { Dead; Sprinting; }\n"
	                                   "    field u16 Health;\n"
	                                   "}\n"
	                                   "behavior Player uses PlayerState {\n"
	                                   "    on event DamageTaken(u16 amount) returns void {\n"
	                                   "        var next = amount;\n"
	                                   "        if (next == 0u16) { this.Dead = ON; }\n"
	                                   "        switch (next) { case 1u16: { break; } default: { break; } }\n"
	                                   "    }\n"
	                                   "}\n";
	sirLangLexer_t lexer = {};
	SIRLangLexerInit( &lexer, sourceFromCString( szSource ) );
	expectToken( &lexer, SIR_LANG_TOKEN_KEYWORD, "schema" );
	expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "PlayerState" );
	expectToken( &lexer, SIR_LANG_TOKEN_PUNCTUATION, "{" );
	expectToken( &lexer, SIR_LANG_TOKEN_KEYWORD, "condition" );
	expectToken( &lexer, SIR_LANG_TOKEN_KEYWORD, "bank" );
	bool bSawReturns = false;
	bool bSawEvent = false;
	bool bSawSwitch = false;
	bool bSawThis = false;
	uint32_t nBlocks = 1;
	for ( uint32_t i = 0; i < 256; ++i ) {
		const sirLangToken_t token = SIRLangLexerNext( &lexer );
		assert( token.eKind != SIR_LANG_TOKEN_INDENT && token.eKind != SIR_LANG_TOKEN_DEDENT && token.eKind != SIR_LANG_TOKEN_NEWLINE &&
		        token.eKind != SIR_LANG_TOKEN_ERROR );
		if ( token.eKind == SIR_LANG_TOKEN_END ) {
			break;
		}
		if ( token.eKind == SIR_LANG_TOKEN_PUNCTUATION && token.nLength == 1 && lexer.source.pBytes[token.nOffset] == '{' ) {
			++nBlocks;
		}
		if ( token.eKind == SIR_LANG_TOKEN_KEYWORD ) {
			const char *pszStart = ( const char * )lexer.source.pBytes + token.nOffset;
			bSawReturns |= token.nLength == 7 && memcmp( pszStart, "returns", 7 ) == 0;
			bSawEvent |= token.nLength == 5 && memcmp( pszStart, "event", 5 ) == 0;
			bSawSwitch |= token.nLength == 6 && memcmp( pszStart, "switch", 6 ) == 0;
			bSawThis |= token.nLength == 4 && memcmp( pszStart, "this", 4 ) == 0;
		}
	}
	assert( bSawReturns && bSawEvent && bSawSwitch && bSawThis );
	assert( nBlocks == 8 );
	assert( SIRLangLexerNext( &lexer ).eKind == SIR_LANG_TOKEN_END );
	assert( SIRLangIsKeyword( ( const uint8_t * )"function", 8 ) );
	assert( !SIRLangIsKeyword( ( const uint8_t * )"fn", 2 ) );
	assert( !SIRLangIsKeyword( ( const uint8_t * )"self", 4 ) );
}

static void testStringsAndComments( void ) noexcept
{
	static constexpr char pszSource[] = "# header\r\nnum 0xFF 1.5e-2f32 5u16 // inline\r\n"
	                                    "\ttext \"escaped \\\"quote\\\"\"\r\n";
	sirLangLexer_t lexer = {};
	SIRLangLexerInit( &lexer, sourceFromCString( pszSource ) );
	const sirLangToken_t first = expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "num" );
	assert( first.nLine == 2 && first.nColumn == 1 );
	expectToken( &lexer, SIR_LANG_TOKEN_NUMBER, "0xFF" );
	expectToken( &lexer, SIR_LANG_TOKEN_NUMBER, "1.5e-2f32" );
	expectToken( &lexer, SIR_LANG_TOKEN_NUMBER, "5u16" );
	const sirLangToken_t text = expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "text" );
	assert( text.nLine == 3 && text.nColumn == 2 );
	expectToken( &lexer, SIR_LANG_TOKEN_STRING, "\"escaped \\\"quote\\\"\"" );
	expectToken( &lexer, SIR_LANG_TOKEN_END, "" );
}

static void testUTF8AndInvalidSource( void ) noexcept
{
	const uint8_t szValid[] = { 'a', 0xc3, 0xa9, '\r', '\n' };
	sirLangLexer_t lexer = {};
	SIRLangLexerInit( &lexer, { szValid, sizeof( szValid ) } );
	const sirLangToken_t unicode = expectToken( &lexer, SIR_LANG_TOKEN_IDENTIFIER, "a\xc3\xa9" );
	assert( unicode.nLength == 3 && unicode.nColumn == 1 );
	const sirLangToken_t end = expectToken( &lexer, SIR_LANG_TOKEN_END, "" );
	assert( end.nLine == 2 && end.nColumn == 1 );
	const uint8_t szInvalid[] = { 0xed, 0xa0, 0x80 };
	SIRLangLexerInit( &lexer, { szInvalid, sizeof( szInvalid ) } );
	const sirLangToken_t bad = SIRLangLexerNext( &lexer );
	assert( bad.eKind == SIR_LANG_TOKEN_ERROR && bad.eError == SIR_LANG_LEX_INVALID_UTF8 );
	SIRLangLexerInit( &lexer, { nullptr, 1 } );
	const sirLangToken_t nullSource = SIRLangLexerNext( &lexer );
	assert( nullSource.eKind == SIR_LANG_TOKEN_ERROR && nullSource.eError == SIR_LANG_LEX_INVALID_SOURCE );
	assert( SIRLangLexerNext( &lexer ).eKind == SIR_LANG_TOKEN_END );
	SIRLangLexerInit( &lexer, sourceFromCString( "\"unterminated\n" ) );
	const sirLangToken_t unclosed = SIRLangLexerNext( &lexer );
	assert( unclosed.eKind == SIR_LANG_TOKEN_ERROR && unclosed.eError == SIR_LANG_LEX_UNTERMINATED_STRING );
	assert( SIRLangLexerNext( &lexer ).eKind == SIR_LANG_TOKEN_END );
}

int main( void )
{
	testModernTokens();
	testStringsAndComments();
	testUTF8AndInvalidSource();
	puts( "SIRLang modern lexer: PASS" );
	return 0;
}
