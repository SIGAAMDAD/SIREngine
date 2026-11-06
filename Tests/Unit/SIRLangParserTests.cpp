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
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct sirLangTestWorkspace_s
{
	sirLangToken_t szTokens[2048];
	sirLangSyntaxNode_t szNodes[2048];
	sirLangParseDiagnostic_t szDiagnostics[128];
	sirLangSyntaxTree_t tree;
} sirLangTestWorkspace_t;

static void initWorkspace( sirLangTestWorkspace_t *pWorkspace ) noexcept
{
	memset( pWorkspace, 0, sizeof( *pWorkspace ) );
	pWorkspace->tree.pTokens = pWorkspace->szTokens;
	pWorkspace->tree.nTokenCapacity = 2048;
	pWorkspace->tree.pNodes = pWorkspace->szNodes;
	pWorkspace->tree.nNodeCapacity = 2048;
	pWorkspace->tree.pDiagnostics = pWorkspace->szDiagnostics;
	pWorkspace->tree.nDiagnosticCapacity = 128;
}

static sirLangParseError_t parseSource( sirLangTestWorkspace_t *pWorkspace, const char *pszSource ) noexcept
{
	initWorkspace( pWorkspace );
	const sirLangSourceView_t source = { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) };
	return SIRLangParse( source, &pWorkspace->tree );
}

static uint32_t countKind( const sirLangSyntaxTree_t *pTree, sirLangSyntaxKind_t eKind ) noexcept
{
	uint32_t nFound = 0;
	for ( uint32_t iNode = 0; iNode < pTree->nNodeCount; ++iNode ) {
		if ( pTree->pNodes[iNode].eKind == eKind ) {
			++nFound;
		}
	}
	return nFound;
}

static void assertTreeIntegrity( const sirLangSyntaxTree_t *pTree ) noexcept
{
	assert( pTree->nRoot < pTree->nNodeCount );
	assert( pTree->pNodes[pTree->nRoot].eKind == SIR_LANG_SYNTAX_MODULE );
	for ( uint32_t iNode = 0; iNode < pTree->nNodeCount; ++iNode ) {
		const sirLangSyntaxNode_t& node = pTree->pNodes[iNode];
		assert( node.nStartToken <= node.nEndToken );
		assert( node.nEndToken <= pTree->nTokenCount );
		uint32_t iChild = node.nFirstChild;
		uint32_t nSteps = 0;
		while ( iChild != UINT32_MAX ) {
			assert( iChild < pTree->nNodeCount );
			assert( ++nSteps <= pTree->nNodeCount );
			iChild = pTree->pNodes[iChild].nNextSibling;
		}
	}
	for ( uint32_t iToken = 0; iToken < pTree->nTokenCount; ++iToken ) {
		const sirLangToken_t& token = pTree->pTokens[iToken];
		assert( token.nOffset <= pTree->source.nBytes );
		assert( token.nLength <= pTree->source.nBytes - token.nOffset );
	}
}

static void testModernBehaviorAndSchema( void ) noexcept
{
	static constexpr const char *pszSource = "import Game.State as GS;\n"
	                                         "schema GuardState {\n"
	                                         "    condition bank Flags : u32 { Alerted; HasTarget; }\n"
	                                         "    field u16 Health;\n"
	                                         "}\n"
	                                         "behavior Guard uses GuardState {\n"
	                                         "    on event DamageTaken(u16 amount) returns void {\n"
	                                         "        u16 next = amount;\n"
	                                         "        this.Health = next;\n"
	                                         "        if (next == 0u16 and this.Alerted) {\n"
	                                         "            this.Alerted = OFF;\n"
	                                         "            emit GuardAlarm(this.id);\n"
	                                         "        } else {\n"
	                                         "            this.Alerted = ON;\n"
	                                         "        }\n"
	                                         "        switch (next) { case 2u16: { break; } default: { break; } }\n"
	                                         "    }\n"
	                                         "    function IsAlive() returns bool { return this.Health > 0u16; }\n"
	                                         "}\n";
	sirLangTestWorkspace_t workspace = {};
	const sirLangParseError_t eResult = parseSource( &workspace, pszSource );
	if ( eResult != SIR_LANG_PARSE_OK ) {
		for ( uint32_t i = 0; i < workspace.tree.nDiagnosticCount; ++i ) {
			const sirLangParseDiagnostic_t& diag = workspace.tree.pDiagnostics[i];
			fprintf( stderr, "modern grammar diagnostic %u at %u:%u\n", ( unsigned )diag.eError, diag.nLine, diag.nColumn );
		}
	}
	assert( eResult == SIR_LANG_PARSE_OK );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_SCHEMA ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_BEHAVIOR ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_CONDITION_BANK ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_EVENT ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_FUNCTION ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_LET ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_SWITCH ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_CASE ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_DEFAULT ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_WHEN ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_OTHERWISE ) == 1 );
	assertTreeIntegrity( &workspace.tree );
}

static void testExplicitAndInferredLocals( void ) noexcept
{
	const char *pszSource = "function Scale(u16 x) returns u16 { u16 value = x; var next = value; return next; }";
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, pszSource ) == SIR_LANG_PARSE_OK );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_LET ) == 2 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_PARAMETER ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_RETURN ) == 1 );
	assertTreeIntegrity( &workspace.tree );
}

static void testBracesSemicolonsAndRetiredGrammar( void ) noexcept
{
	const char *const szInvalid[] = { "function Bad() returns void { return }",
	                                  "function Bad() returns void { if (true) return; }",
	                                  "function Bad() returns void { var x = 1u16 }",
	                                  "fn Old() -> void: return",
	                                  "on event Wrong(u16 arg) { return; }",
	                                  "function Old() returns void { let x = 2u8; }",
	                                  "function Old() returns void { when true { return; } }",
	                                  "function Old() returns void { self.Hidden = ON; }",
	                                  "function Old() returns void { match (true) { case true: { break; } } }",
	                                  "function Bad() returns void { switch (true) { case true: return; } }" };
	sirLangTestWorkspace_t workspace = {};
	for ( const char *pszSource : szInvalid ) {
		const sirLangParseError_t eResult = parseSource( &workspace, pszSource );
		if ( eResult == SIR_LANG_PARSE_OK ) {
			fprintf( stderr, "invalid grammar accepted: %s\n", pszSource );
		}
		assert( eResult != SIR_LANG_PARSE_OK );
		assertTreeIntegrity( &workspace.tree );
	}
}

static void testUnicodeAndLosslessSource( void ) noexcept
{
	const char *pszSource = "# head\r\nfunction T() returns u16 {\r\n    return 3u16; // tail\r\n}\r\n";
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, pszSource ) == SIR_LANG_PARSE_OK );
	assert( workspace.tree.source.nBytes == strlen( pszSource ) );
	assert( memcmp( workspace.tree.source.pBytes, pszSource, workspace.tree.source.nBytes ) == 0 );
	sirLangSourceSpan_t span = {};
	assert( SIRLangSyntaxSpan( &workspace.tree, workspace.tree.nRoot, &span ) );
	assert( span.nOffset == 0 && span.nLength == strlen( pszSource ) );
	assert( !SIRLangSyntaxSpan( &workspace.tree, UINT32_MAX, &span ) );
	assertTreeIntegrity( &workspace.tree );
	assert( parseSource( &workspace, "" ) == SIR_LANG_PARSE_OK );
	assert( workspace.tree.nNodeCount == 1 && workspace.tree.nTokenCount == 1 );
}

static void testMalformedSourceAndLimits( void ) noexcept
{
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, "function Bad() returns void { var x = '\xff'; }" ) == SIR_LANG_PARSE_LEXICAL_ERROR );
	assert( workspace.tree.pDiagnostics[0].eLexError == SIR_LANG_LEX_INVALID_UTF8 );
	assert( parseSource( &workspace, "function Bad() returns void { var x = \"unfinished\n }" ) == SIR_LANG_PARSE_LEXICAL_ERROR );
	assert( SIRLangParse( { nullptr, 1 }, &workspace.tree ) == SIR_LANG_PARSE_INVALID_INPUT );
	assert( SIRLangParse( { nullptr, 0 }, nullptr ) == SIR_LANG_PARSE_INVALID_INPUT );
	initWorkspace( &workspace );
	workspace.tree.nTokenCapacity = 2;
	const char *pszSource = "function Count() returns u16 { return 4u16; }";
	assert( SIRLangParse( { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) }, &workspace.tree ) == SIR_LANG_PARSE_CAPACITY );
	initWorkspace( &workspace );
	workspace.tree.nNodeCapacity = 2;
	assert( SIRLangParse( { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) }, &workspace.tree ) == SIR_LANG_PARSE_CAPACITY );
	assert( workspace.tree.bTruncated && workspace.tree.nNodeCount <= 2 );
	initWorkspace( &workspace );
	workspace.tree.nDiagnosticCapacity = 1;
	const char *pszBad = "function Bad() returns void { var x = ; var y = ; }";
	assert( SIRLangParse( { ( const uint8_t * )pszBad, ( uint32_t )strlen( pszBad ) }, &workspace.tree ) == SIR_LANG_PARSE_CAPACITY );
}

static void testNestingLimit( void ) noexcept
{
	char szSource[1024] = {};
	uint32_t nUsed = 0;
	const char *pszStart = "function Deep() returns bool { return ";
	memcpy( szSource, pszStart, strlen( pszStart ) );
	nUsed = ( uint32_t )strlen( pszStart );
	for ( uint32_t i = 0; i < 100; ++i ) {
		memcpy( szSource + nUsed, "not ", 4 );
		nUsed += 4;
	}
	memcpy( szSource + nUsed, "true; }", 8 );
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, szSource ) == SIR_LANG_PARSE_NESTING_LIMIT );
	assertTreeIntegrity( &workspace.tree );
}

static void testMalformedFuzz( void ) noexcept
{
	static constexpr char s_szAlphabet[] = "()[]{}.,:;+-*/%&|^!?=<>@~ \n\t#'\"abcdefghijklmnopqrstuvwxyz0123456789";
	sirLangTestWorkspace_t workspace = {};
	char szSource[257] = {};
	uint32_t nSeed = 0x3ae45127U;
	for ( uint32_t iCase = 0; iCase < 1200; ++iCase ) {
		nSeed = nSeed * 1664525U + 1013904223U;
		const uint32_t nBytes = nSeed % 256;
		for ( uint32_t i = 0; i < nBytes; ++i ) {
			nSeed = nSeed * 1664525U + 1013904223U;
			szSource[i] = s_szAlphabet[( nSeed = nSeed * 1664525U + 1013904223U ) % ( sizeof( s_szAlphabet ) - 1 )];
		}
		szSource[nBytes] = '\0';
		initWorkspace( &workspace );
		const sirLangParseError_t e = SIRLangParse( { ( const uint8_t * )szSource, nBytes }, &workspace.tree );
		assert( e <= SIR_LANG_PARSE_NESTING_LIMIT );
		if ( workspace.tree.nRoot < workspace.tree.nNodeCount ) {
			assertTreeIntegrity( &workspace.tree );
		}
	}
}

int main( void )
{
	testModernBehaviorAndSchema();
	testExplicitAndInferredLocals();
	testBracesSemicolonsAndRetiredGrammar();
	testUnicodeAndLosslessSource();
	testMalformedSourceAndLimits();
	testNestingLimit();
	testMalformedFuzz();
	puts( "SIRLang modern C-style grammar: PASS" );
	return 0;
}
