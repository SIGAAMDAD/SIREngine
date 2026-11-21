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

#include <Scripting/Language/SIRLangParser.h>
#include <Scripting/Language/SIRLangLexer.h>

#include <stdio.h>
#include <string.h>

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirLangAdversarialWorkspace_s
{
	sirLangToken_t szTokens[1024];
	sirLangSyntaxNode_t szNodes[1024];
	sirLangParseDiagnostic_t szDiagnostics[128];
	sirLangSyntaxTree_t tree;
} sirLangAdversarialWorkspace_t;

static void initializeWorkspace( sirLangAdversarialWorkspace_t *pWorkspace ) noexcept
{
	*pWorkspace = {};
	pWorkspace->tree.pTokens = pWorkspace->szTokens;
	pWorkspace->tree.nTokenCapacity = 1024;
	pWorkspace->tree.pNodes = pWorkspace->szNodes;
	pWorkspace->tree.nNodeCapacity = 1024;
	pWorkspace->tree.pDiagnostics = pWorkspace->szDiagnostics;
	pWorkspace->tree.nDiagnosticCapacity = 128;
}

static bool checkParserDeterminism( const uint8_t *pBytes, uint32_t nBytes ) noexcept
{
	sirLangAdversarialWorkspace_t first = {};
	sirLangAdversarialWorkspace_t second = {};
	initializeWorkspace( &first );
	initializeWorkspace( &second );
	const sirLangSourceView_t source = { pBytes, nBytes };
	const sirLangParseError_t eFirst = SIRLangParse( source, &first.tree );
	const sirLangParseError_t eSecond = SIRLangParse( source, &second.tree );
	SIR_TEST_CHECK( eFirst == eSecond );
	SIR_TEST_CHECK( first.tree.nTokenCount == second.tree.nTokenCount );
	SIR_TEST_CHECK( first.tree.nNodeCount == second.tree.nNodeCount );
	SIR_TEST_CHECK( first.tree.nDiagnosticCount == second.tree.nDiagnosticCount );
	SIR_TEST_CHECK( first.tree.bTruncated == second.tree.bTruncated );
	SIR_TEST_CHECK( first.tree.nTokenCount <= 1024 && first.tree.nNodeCount <= 1024 && first.tree.nDiagnosticCount <= 128 );
	for ( uint32_t i = 0; i < first.tree.nTokenCount; ++i ) {
		const sirLangToken_t& a = first.szTokens[i];
		const sirLangToken_t& b = second.szTokens[i];
		SIR_TEST_CHECK( a.eKind == b.eKind && a.eError == b.eError && a.nOffset == b.nOffset && a.nLength == b.nLength &&
		                a.nLine == b.nLine && a.nColumn == b.nColumn );
		SIR_TEST_CHECK( a.nOffset <= nBytes && a.nLength <= nBytes - a.nOffset );
	}
	for ( uint32_t i = 0; i < first.tree.nDiagnosticCount; ++i ) {
		const sirLangParseDiagnostic_t& a = first.szDiagnostics[i];
		const sirLangParseDiagnostic_t& b = second.szDiagnostics[i];
		SIR_TEST_CHECK( a.eError == b.eError && a.eLexError == b.eLexError && a.nOffset == b.nOffset && a.nLength == b.nLength &&
		                a.nLine == b.nLine && a.nColumn == b.nColumn );
		SIR_TEST_CHECK( a.nOffset <= nBytes && a.nLength <= nBytes - a.nOffset );
	}
	if ( eFirst == SIR_LANG_PARSE_OK ) {
		SIR_TEST_CHECK( first.tree.nNodeCount > 0 && first.tree.nRoot < first.tree.nNodeCount );
		SIR_TEST_CHECK( first.szNodes[first.tree.nRoot].eKind == SIR_LANG_SYNTAX_MODULE );
		for ( uint32_t i = 0; i < first.tree.nNodeCount; ++i ) {
			const sirLangSyntaxNode_t& node = first.szNodes[i];
			SIR_TEST_CHECK( node.nStartToken <= node.nEndToken && node.nEndToken <= first.tree.nTokenCount );
			SIR_TEST_CHECK( node.nFirstChild == UINT32_MAX || node.nFirstChild < first.tree.nNodeCount );
			SIR_TEST_CHECK( node.nNextSibling == UINT32_MAX || node.nNextSibling < first.tree.nNodeCount );
		}
	}
	return true;
}

static bool testMutatedGrammarInputs( void ) noexcept
{
	// Parser recovery, arbitrary binary input, exact source spans and
	// repeatability are tested without encoding acceptance of legacy grammar.
	static const char *s_szSamples[] = { "function echo(u16 value) returns u16 { return value; }\n",
	                                     "on event Hit(u16 damage) returns void { return; }\n",
	                                     "class Player { var health = 100; }\n",
	                                     "switch (value) { case 0: break; default: break; }\n",
	                                     "function broken( returns u8 { return 4; }\n",
	                                     "function nested() returns bool { if (true) { return false; } }\n",
	                                     "\"unterminated\n",
	                                     " \tinvalid-indent\n",
	                                     "# comment\r\n\r\n",
	                                     "\xff\xfe\xed\xa0\x80",
	                                     "schema Example:\n    condition bank Flags: u16\n        Visible\n" };
	static constexpr uint8_t s_szMutations[] = { 0, '\n', '\r', '\t', ' ', '{', '}', ':', ';', '"', 0xff, 0x80, 0xed };
	uint8_t szBuffer[256] = {};
	for ( const char *pszSeed : s_szSamples ) {
		const uint32_t nLength = ( uint32_t )strlen( pszSeed );
		SIR_TEST_CHECK( nLength < sizeof( szBuffer ) );
		memcpy( szBuffer, pszSeed, nLength );
		SIR_TEST_CHECK( checkParserDeterminism( szBuffer, nLength ) );
		for ( uint32_t i = 0; i < nLength; ++i ) {
			const uint8_t nOriginal = szBuffer[i];
			for ( uint8_t nReplacement : s_szMutations ) {
				szBuffer[i] = nReplacement;
				SIR_TEST_CHECK( checkParserDeterminism( szBuffer, nLength ) );
			}
			szBuffer[i] = nOriginal;
		}
		// Every source prefix must terminate within its declared byte span.
		for ( uint32_t nPrefix = 0; nPrefix < nLength; ++nPrefix ) {
			SIR_TEST_CHECK( checkParserDeterminism( szBuffer, nPrefix ) );
		}
	}
	return true;
}

static bool testLexerInvalidSource( void ) noexcept
{
	sirLangLexer_t lexer = {};
	SIRLangLexerInit( &lexer, { nullptr, 1 } );
	const sirLangToken_t first = SIRLangLexerNext( &lexer );
	SIR_TEST_CHECK( first.eKind == SIR_LANG_TOKEN_ERROR && first.eError == SIR_LANG_LEX_INVALID_SOURCE );
	SIR_TEST_CHECK( SIRLangLexerNext( &lexer ).eKind == SIR_LANG_TOKEN_END );

	// No terminating NUL is required. Bytes after the declared span must
	// never influence tokenization, including binary zero and high UTF-8.
	const uint8_t szInput[] = { 'v', 'a', 'r', ' ', 'x', ' ', '=', ' ', '1', ';', '\n', 0xff, 0 };
	SIRLangLexerInit( &lexer, { szInput, 11 } );
	for ( uint32_t iToken = 0; iToken < 20; ++iToken ) {
		const sirLangToken_t token = SIRLangLexerNext( &lexer );
		SIR_TEST_CHECK( token.nOffset <= 11 && token.nLength <= 11 - token.nOffset );
		SIR_TEST_CHECK( token.eError != SIR_LANG_LEX_INVALID_UTF8 );
		if ( token.eKind == SIR_LANG_TOKEN_END ) {
			return true;
		}
	}
	return false;
}

static bool testTamperedSyntaxSpans( void ) noexcept
{
	static constexpr uint8_t szSource[] = "0123456789";
	sirLangToken_t szTokens[3] = {};
	szTokens[1].nOffset = 2;
	szTokens[1].nLength = 1;
	szTokens[2].nOffset = 7;
	szTokens[2].nLength = 2;
	sirLangSyntaxNode_t szNodes[2] = {};
	szNodes[1].eKind = SIR_LANG_SYNTAX_NAME;
	szNodes[1].nStartToken = 1;
	szNodes[1].nEndToken = 3;
	sirLangSyntaxTree_t tree = {};
	tree.source = { szSource, 10 };
	tree.pTokens = szTokens;
	tree.nTokenCount = 3;
	tree.pNodes = szNodes;
	tree.nNodeCount = 2;
	tree.nRoot = 0;
	sirLangSourceSpan_t span = { 100, 200 };
	SIR_TEST_CHECK( SIRLangSyntaxSpan( &tree, 1, &span ) );
	SIR_TEST_CHECK( span.nOffset == 2 && span.nLength == 7 );

	// Uint32 overflow previously wrapped the ending offset back into the source.
	szTokens[2].nOffset = UINT32_MAX;
	szTokens[2].nLength = 5;
	span = { 100, 200 };
	SIR_TEST_CHECK( !SIRLangSyntaxSpan( &tree, 1, &span ) );
	SIR_TEST_CHECK( span.nOffset == 100 && span.nLength == 200 );

	szTokens[2].nOffset = 8;
	szTokens[2].nLength = 3;
	SIR_TEST_CHECK( !SIRLangSyntaxSpan( &tree, 1, &span ) );
	szTokens[2].nOffset = 1;
	szTokens[2].nLength = 2;
	SIR_TEST_CHECK( !SIRLangSyntaxSpan( &tree, 1, &span ) );

	// Zero-length nodes must also validate the starting token offset.
	szNodes[1].nEndToken = 1;
	szTokens[1].nOffset = 11;
	SIR_TEST_CHECK( !SIRLangSyntaxSpan( &tree, 1, &span ) );
	SIR_TEST_CHECK( span.nOffset == 100 && span.nLength == 200 );
	return true;
}

int main( void )
{
	if ( !testMutatedGrammarInputs() || !testLexerInvalidSource() || !testTamperedSyntaxSpans() ) {
		return 1;
	}
	puts( "SIRLang parser adversarial regressions: PASS" );
	return 0;
}
