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

static void testBehaviorAndSchema( void ) noexcept
{
	static constexpr const char *pszSource = "import Game.State as GS\n"
	                                         "schema GuardState:\n"
	                                         "    condition bank Flags: u16\n"
	                                         "        Alerted\n"
	                                         "        HasTarget\n"
	                                         "    field Health: u16\n"
	                                         "    Goal: GoalID\n"
	                                         "behavior Guard uses GuardState:\n"
	                                         "    on DamageTaken(amount: u16):\n"
	                                         "        let next: u16 = saturating_sub(self.Health, amount)\n"
	                                         "        self.Health = next\n"
	                                         "        when next == 0u16 and self.Alerted:\n"
	                                         "            mutate World.City:\n"
	                                         "                AlertLevel max= 2u8\n"
	                                         "            emit GuardAlarm(source: self.id)\n"
	                                         "        otherwise:\n"
	                                         "            spawn Reinforcement at self.Position\n"
	                                         "    fn is_alive() -> bool:\n"
	                                         "        return self.Health > 0u16\n";
	sirLangTestWorkspace_t workspace = {};
	const sirLangParseError_t eResult = parseSource( &workspace, pszSource );
	if ( eResult != SIR_LANG_PARSE_OK ) {
		for ( uint32_t iDiag = 0; iDiag < workspace.tree.nDiagnosticCount; ++iDiag ) {
			const sirLangParseDiagnostic_t& diag = workspace.tree.pDiagnostics[iDiag];
			fprintf( stderr, "Parser error %u at %u:%u\n", ( unsigned )diag.eError, diag.nLine, diag.nColumn );
		}
	}
	assert( eResult == SIR_LANG_PARSE_OK );
	assert( !workspace.tree.bTruncated );
	assert( workspace.tree.nDiagnosticCount == 0 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_IMPORT ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_SCHEMA ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_BEHAVIOR ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_CONDITION_BANK ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_EVENT ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_FUNCTION ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_LET ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_WHEN ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_OTHERWISE ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_NAMED_ARGUMENT ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_MUTATE ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_ASSIGNMENT ) == 2 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_BINARY ) >= 2 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_CALL ) == 2 );
	assertTreeIntegrity( &workspace.tree );
}

static void testPrecedenceGenericsAndControl( void ) noexcept
{
	const char *pszSource = "fn choose(a: i32, b: i32) -> result<i32, ArithmeticError>:\n"
	                        "    let values: stack_list<i32, 8>\n"
	                        "    let local: [u8;16]\n"
	                        "    for item in collect(a, b):\n"
	                        "        when not (a + b * 3 > 12) or a != b:\n"
	                        "            break\n"
	                        "        otherwise:\n"
	                        "            continue\n"
	                        "    after 10s emit Deadline(entity: self.id)\n"
	                        "    return values[0]\n";
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, pszSource ) == SIR_LANG_PARSE_OK );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_TYPE ) >= 6 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_FOR ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_AFTER ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_INDEX ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_UNARY ) >= 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_BINARY ) >= 5 );
	assertTreeIntegrity( &workspace.tree );
}

static void testComparisonsAndNoPayloadEvent( void ) noexcept
{
	const char *pszSource = "behavior Watcher uses WatchState:\n"
	                        "    on ThreatLost:\n"
	                        "        when a <= b and b >= c and a == c:\n"
	                        "            emit Done()\n";
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, pszSource ) == SIR_LANG_PARSE_OK );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_EVENT ) == 1 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_BINARY ) == 5 );
	assertTreeIntegrity( &workspace.tree );
}

static void testLosslessSourceAndEmptyInput( void ) noexcept
{
	const char *pszSource = "# upper comment\r\n\r\nfn alpha():\r\n    return 3u16  # trailing comment\r\n";
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, pszSource ) == SIR_LANG_PARSE_OK );
	assert( workspace.tree.source.nBytes == strlen( pszSource ) );
	assert( memcmp( workspace.tree.source.pBytes, pszSource, workspace.tree.source.nBytes ) == 0 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_FUNCTION ) == 1 );
	assertTreeIntegrity( &workspace.tree );
	sirLangSourceSpan_t span = {};
	assert( SIRLangSyntaxSpan( &workspace.tree, workspace.tree.nRoot, &span ) );
	assert( span.nOffset == 0 && span.nLength == strlen( pszSource ) );
	assert( !SIRLangSyntaxSpan( &workspace.tree, UINT32_MAX, &span ) );
	assert( parseSource( &workspace, "" ) == SIR_LANG_PARSE_OK );
	assert( workspace.tree.nNodeCount == 1 );
	assert( workspace.tree.nTokenCount == 1 );
	assertTreeIntegrity( &workspace.tree );
}

static void testSyntaxRecovery( void ) noexcept
{
	const char *pszSource = "fn broken(a: u16:\n"
	                        "    let missing: u16 =\n"
	                        "fn good(x: i32):\n"
	                        "    return x\n";
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, pszSource ) != SIR_LANG_PARSE_OK );
	assert( workspace.tree.nDiagnosticCount >= 2 );
	assert( countKind( &workspace.tree, SIR_LANG_SYNTAX_FUNCTION ) == 2 );
	assertTreeIntegrity( &workspace.tree );
	assert( parseSource( &workspace, "otherwise:\n    return 1\n" ) == SIR_LANG_PARSE_UNEXPECTED_TOKEN );
	assert( workspace.tree.nDiagnosticCount >= 1 );
	assertTreeIntegrity( &workspace.tree );
}

static void testMalformedSourceAndBudget( void ) noexcept
{
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, "fn fail():\n    let x = '\xff'\n" ) == SIR_LANG_PARSE_LEXICAL_ERROR );
	assert( workspace.tree.pDiagnostics[0].eLexError == SIR_LANG_LEX_INVALID_UTF8 );
	assertTreeIntegrity( &workspace.tree );
	assert( parseSource( &workspace, "fn fail():\n    let x = \"unterminated\n" ) == SIR_LANG_PARSE_LEXICAL_ERROR );
	assertTreeIntegrity( &workspace.tree );
	assert( parseSource( &workspace, "fn fail():\n   return 3\n  return 4\n" ) == SIR_LANG_PARSE_LEXICAL_ERROR );
	assertTreeIntegrity( &workspace.tree );
	assert( SIRLangParse( { nullptr, 1 }, &workspace.tree ) == SIR_LANG_PARSE_INVALID_INPUT );
	assert( SIRLangParse( { nullptr, 0 }, nullptr ) == SIR_LANG_PARSE_INVALID_INPUT );

	initWorkspace( &workspace );
	workspace.tree.nTokenCapacity = 2;
	const char *pszSource = "fn count():\n    return 4\n";
	assert( SIRLangParse( { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) }, &workspace.tree ) == SIR_LANG_PARSE_CAPACITY );
	assert( workspace.tree.bTruncated );

	initWorkspace( &workspace );
	workspace.tree.nNodeCapacity = 2;
	assert( SIRLangParse( { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) }, &workspace.tree ) == SIR_LANG_PARSE_CAPACITY );
	assert( workspace.tree.bTruncated );
	assert( workspace.tree.nNodeCount <= 2 );

	initWorkspace( &workspace );
	workspace.tree.nDiagnosticCapacity = 1;
	const char *pszMalformed = "fn bad(a:):\n    let x =\n";
	assert( SIRLangParse( { ( const uint8_t * )pszMalformed, ( uint32_t )strlen( pszMalformed ) }, &workspace.tree ) ==
	        SIR_LANG_PARSE_CAPACITY );
	assert( workspace.tree.bTruncated );
	assert( workspace.tree.nDiagnosticCount == 1 );
}

static void testNestingLimit( void ) noexcept
{
	char szSource[1024] = {};
	uint32_t nUsed = 0;
	const char *pszStart = "fn many():\n    return ";
	memcpy( szSource, pszStart, strlen( pszStart ) );
	nUsed = ( uint32_t )strlen( pszStart );
	for ( uint32_t iItem = 0; iItem < 100; ++iItem ) {
		memcpy( szSource + nUsed, "not ", 4 );
		nUsed += 4;
	}
	memcpy( szSource + nUsed, "true\n", 6 );
	sirLangTestWorkspace_t workspace = {};
	assert( parseSource( &workspace, szSource ) == SIR_LANG_PARSE_NESTING_LIMIT );
	assertTreeIntegrity( &workspace.tree );
}

static void testDeterministicMalformedInputs( void ) noexcept
{
	static constexpr char s_szAlphabet[] = "()[]{}.,:;+-*/%&|^!?=<>@~ \n\t#'\"abcdefghijklmnopqrstuvwxyz0123456789";
	sirLangTestWorkspace_t workspace = {};
	char szSource[257] = {};
	uint32_t nSeed = 0x3ae45127U;
	for ( uint32_t iCase = 0; iCase < 1200; ++iCase ) {
		nSeed = nSeed * 1664525U + 1013904223U;
		const uint32_t nBytes = nSeed % 256;
		for ( uint32_t iByte = 0; iByte < nBytes; ++iByte ) {
			nSeed = nSeed * 1664525U + 1013904223U;
			szSource[iByte] = s_szAlphabet[nSeed % ( sizeof( s_szAlphabet ) - 1 )];
		}
		szSource[nBytes] = '\0';
		initWorkspace( &workspace );
		const sirLangParseError_t eResult = SIRLangParse( { ( const uint8_t * )szSource, nBytes }, &workspace.tree );
		assert( eResult <= SIR_LANG_PARSE_NESTING_LIMIT );
		if ( workspace.tree.nRoot < workspace.tree.nNodeCount ) {
			assertTreeIntegrity( &workspace.tree );
		}
	}
}

int main( void )
{
	testBehaviorAndSchema();
	testPrecedenceGenericsAndControl();
	testComparisonsAndNoPayloadEvent();
	testLosslessSourceAndEmptyInput();
	testSyntaxRecovery();
	testMalformedSourceAndBudget();
	testNestingLimit();
	testDeterministicMalformedInputs();
	puts( "SIR-Lang parser regression tests passed" );
	return 0;
}
