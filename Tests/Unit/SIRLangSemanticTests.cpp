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

#include <Scripting/Language/SIRLangSemantic.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static constexpr uint32_t SIR_LANG_TEST_NO_NODE = UINT32_MAX;

typedef struct sirLangSemanticFixture_s
{
	char szSource[4096];
	sirLangToken_t szTokens[256];
	sirLangSyntaxNode_t szNodes[256];
	sirLangParseDiagnostic_t szParseDiagnostics[16];
	sirLangSemanticSymbol_t szSymbols[256];
	sirLangSemanticDiagnostic_t szDiagnostics[128];
	sirLangResolvedReference_t szReferences[256];
	sirLangSyntaxTree_t tree;
	sirLangSemanticOutput_t output;
	uint32_t nSourceBytes;
} sirLangSemanticFixture_t;

static void initializeFixture( sirLangSemanticFixture_t *pFixture ) noexcept
{
	memset( pFixture, 0, sizeof( *pFixture ) );
	pFixture->tree.source = { ( const uint8_t * )pFixture->szSource, 0 };
	pFixture->tree.pTokens = pFixture->szTokens;
	pFixture->tree.nTokenCapacity = 256;
	pFixture->tree.pNodes = pFixture->szNodes;
	pFixture->tree.nNodeCapacity = 256;
	pFixture->tree.pDiagnostics = pFixture->szParseDiagnostics;
	pFixture->tree.nDiagnosticCapacity = 16;
	pFixture->output.pReferences = pFixture->szReferences;
	pFixture->output.nReferenceCapacity = 256;
	pFixture->output.pSymbols = pFixture->szSymbols;
	pFixture->output.nSymbolCapacity = 256;
	pFixture->output.pDiagnostics = pFixture->szDiagnostics;
	pFixture->output.nDiagnosticCapacity = 128;
}

static uint32_t addToken( sirLangSemanticFixture_t *pFixture, const char *pszText ) noexcept
{
	const uint32_t iToken = pFixture->tree.nTokenCount++;
	const uint32_t nLength = ( uint32_t )strlen( pszText );
	assert( iToken < 256 && pFixture->nSourceBytes + nLength + 2 < sizeof( pFixture->szSource ) );
	const uint32_t nOffset = pFixture->nSourceBytes;
	memcpy( pFixture->szSource + nOffset, pszText, nLength );
	pFixture->nSourceBytes += nLength;
	pFixture->szSource[pFixture->nSourceBytes++] = ' ';
	pFixture->tree.source.nBytes = pFixture->nSourceBytes;
	sirLangTokenKind_t eKind = SIR_LANG_TOKEN_IDENTIFIER;
	if ( pszText[0] >= '0' && pszText[0] <= '9' ) {
		eKind = SIR_LANG_TOKEN_NUMBER;
	}
	pFixture->szTokens[iToken] = { eKind, SIR_LANG_LEX_OK, nOffset, nLength, 1, nOffset + 1 };
	return iToken;
}

static void addTokens( sirLangSemanticFixture_t *pFixture, const char *const *pszTokens, uint32_t nCount ) noexcept
{
	for ( uint32_t iToken = 0; iToken < nCount; ++iToken ) {
		addToken( pFixture, pszTokens[iToken] );
	}
}

static uint32_t newNode( sirLangSemanticFixture_t *pFixture, sirLangSyntaxKind_t eKind, uint32_t iStart, uint32_t iEnd ) noexcept
{
	const uint32_t iNode = pFixture->tree.nNodeCount++;
	assert( iNode < 256 );
	pFixture->szNodes[iNode] = { eKind, iStart, iEnd, SIR_LANG_TEST_NO_NODE, SIR_LANG_TEST_NO_NODE, SIR_LANG_TEST_NO_NODE };
	return iNode;
}

static void appendChild( sirLangSemanticFixture_t *pFixture, uint32_t iParent, uint32_t iChild ) noexcept
{
	assert( iParent < pFixture->tree.nNodeCount && iChild < pFixture->tree.nNodeCount );
	sirLangSyntaxNode_t& parent = pFixture->szNodes[iParent];
	if ( parent.nFirstChild == SIR_LANG_TEST_NO_NODE ) {
		parent.nFirstChild = iChild;
	} else {
		pFixture->szNodes[parent.nLastChild].nNextSibling = iChild;
	}
	parent.nLastChild = iChild;
}

static sirLangSemanticError_t analyzeFixture( sirLangSemanticFixture_t *pFixture ) noexcept
{
	return SIRLangAnalyze( &pFixture->tree, &pFixture->output );
}

static bool hasDiagnostic( const sirLangSemanticFixture_t *pFixture, sirLangSemanticError_t eError ) noexcept
{
	for ( uint32_t iDiag = 0; iDiag < pFixture->output.nDiagnosticCount; ++iDiag ) {
		if ( pFixture->szDiagnostics[iDiag].eError == eError ) {
			return true;
		}
	}
	return false;
}

// Test ASTs use the exact indexed child-link, source-span and token contracts
// emitted by SIRLangParse; no private semantic analyzer internals are exposed.
static void buildBehaviorFixture( sirLangSemanticFixture_t *pFixture ) noexcept
{
	initializeFixture( pFixture );
	static constexpr const char *s_pszWords[] = {
	    "schema", "GuardState", ":", "Health", ":",    "u16", "Alerted", ":", "bool",   "behavior", "Guard",
	    "uses",   "GuardState", ":", "fn",     "get",  "(",   "amount",  ":", "u16",    ")",        "-",
	    ">",      "u16",        ":", "let",    "next", ":",   "u16",     "=", "amount", "return",   "next" };
	addTokens( pFixture, s_pszWords, sizeof( s_pszWords ) / sizeof( s_pszWords[0] ) );
	const uint32_t iRoot = newNode( pFixture, SIR_LANG_SYNTAX_MODULE, 0, 33 );
	const uint32_t iSchema = newNode( pFixture, SIR_LANG_SYNTAX_SCHEMA, 0, 9 );
	const uint32_t iSchemaBlock = newNode( pFixture, SIR_LANG_SYNTAX_BLOCK, 2, 9 );
	const uint32_t iHealth = newNode( pFixture, SIR_LANG_SYNTAX_FIELD, 3, 6 );
	const uint32_t iHealthType = newNode( pFixture, SIR_LANG_SYNTAX_TYPE, 5, 6 );
	const uint32_t iAlerted = newNode( pFixture, SIR_LANG_SYNTAX_FIELD, 6, 9 );
	const uint32_t iAlertedType = newNode( pFixture, SIR_LANG_SYNTAX_TYPE, 8, 9 );
	const uint32_t iBehavior = newNode( pFixture, SIR_LANG_SYNTAX_BEHAVIOR, 9, 33 );
	const uint32_t iUses = newNode( pFixture, SIR_LANG_SYNTAX_TYPE, 12, 13 );
	const uint32_t iBehaviorBlock = newNode( pFixture, SIR_LANG_SYNTAX_BLOCK, 13, 33 );
	const uint32_t iFunction = newNode( pFixture, SIR_LANG_SYNTAX_FUNCTION, 14, 33 );
	const uint32_t iParameter = newNode( pFixture, SIR_LANG_SYNTAX_PARAMETER, 17, 20 );
	const uint32_t iParameterType = newNode( pFixture, SIR_LANG_SYNTAX_TYPE, 19, 20 );
	const uint32_t iReturnType = newNode( pFixture, SIR_LANG_SYNTAX_TYPE, 23, 24 );
	const uint32_t iFunctionBlock = newNode( pFixture, SIR_LANG_SYNTAX_BLOCK, 24, 33 );
	const uint32_t iLet = newNode( pFixture, SIR_LANG_SYNTAX_LET, 25, 31 );
	const uint32_t iLocalType = newNode( pFixture, SIR_LANG_SYNTAX_TYPE, 28, 29 );
	const uint32_t iParameterUse = newNode( pFixture, SIR_LANG_SYNTAX_NAME, 30, 31 );
	const uint32_t iReturn = newNode( pFixture, SIR_LANG_SYNTAX_RETURN, 31, 33 );
	const uint32_t iLocalUse = newNode( pFixture, SIR_LANG_SYNTAX_NAME, 32, 33 );
	appendChild( pFixture, iRoot, iSchema );
	appendChild( pFixture, iRoot, iBehavior );
	appendChild( pFixture, iSchema, iSchemaBlock );
	appendChild( pFixture, iSchemaBlock, iHealth );
	appendChild( pFixture, iSchemaBlock, iAlerted );
	appendChild( pFixture, iHealth, iHealthType );
	appendChild( pFixture, iAlerted, iAlertedType );
	appendChild( pFixture, iBehavior, iUses );
	appendChild( pFixture, iBehavior, iBehaviorBlock );
	appendChild( pFixture, iBehaviorBlock, iFunction );
	appendChild( pFixture, iFunction, iParameter );
	appendChild( pFixture, iFunction, iReturnType );
	appendChild( pFixture, iFunction, iFunctionBlock );
	appendChild( pFixture, iParameter, iParameterType );
	appendChild( pFixture, iFunctionBlock, iLet );
	appendChild( pFixture, iFunctionBlock, iReturn );
	appendChild( pFixture, iLet, iLocalType );
	appendChild( pFixture, iLet, iParameterUse );
	appendChild( pFixture, iReturn, iLocalUse );
	pFixture->tree.nRoot = iRoot;
}

static void testLocalScopesAndTypes( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	buildBehaviorFixture( &fixture );
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_OK );
	assert( fixture.output.nDiagnosticCount == 0 );
	assert( fixture.output.nSymbolCount == 7 );
	assert( fixture.output.nResolvedReferences >= 2 );
	assert( fixture.output.nReferenceCount == fixture.output.nResolvedReferences );
	bool bParameterBound = false;
	for ( uint32_t iRef = 0; iRef < fixture.output.nReferenceCount; ++iRef ) {
		const sirLangResolvedReference_t& ref = fixture.output.pReferences[iRef];
		if ( ref.iUseNode == 17 && ref.iDeclarationNode == 11 && ref.eValue == SIR_LANG_VALUE_U16 ) {
			bParameterBound = true;
		}
	}
	assert( bParameterBound );
	assert( !fixture.output.bTruncated );
}

static void testUseBeforeDeclaration( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	buildBehaviorFixture( &fixture );
	// Make the initializer refer to next, before next is entered in scope.
	fixture.szTokens[30].nOffset = fixture.szTokens[32].nOffset;
	fixture.szTokens[30].nLength = fixture.szTokens[32].nLength;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_UNKNOWN_SYMBOL );
	assert( hasDiagnostic( &fixture, SIR_LANG_SEMANTIC_UNKNOWN_SYMBOL ) );
}

static void testUnknownReturnType( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	buildBehaviorFixture( &fixture );
	// Swap the declared return type from u16 to an unregistered name.
	fixture.szTokens[23].nOffset = fixture.szTokens[12].nOffset;
	fixture.szTokens[23].nLength = fixture.szTokens[12].nLength;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_INVALID_RETURN );
	// GuardState is a valid nominal type but a u16 return is incompatible.
	assert( hasDiagnostic( &fixture, SIR_LANG_SEMANTIC_INVALID_RETURN ) );
}

static void testUnknownLocalType( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	buildBehaviorFixture( &fixture );
	fixture.szTokens[28].nOffset = fixture.szTokens[26].nOffset;
	fixture.szTokens[28].nLength = fixture.szTokens[26].nLength;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_UNKNOWN_TYPE );
	assert( hasDiagnostic( &fixture, SIR_LANG_SEMANTIC_UNKNOWN_TYPE ) );
}

static void testDuplicateDeclaration( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	buildBehaviorFixture( &fixture );
	const uint32_t iSchemaBlock = fixture.szNodes[1].nFirstChild;
	const uint32_t iDuplicate = newNode( &fixture, SIR_LANG_SYNTAX_FIELD, 3, 6 );
	appendChild( &fixture, iSchemaBlock, iDuplicate );
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_DUPLICATE_SYMBOL );
}

static void testWrongAssignmentType( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	buildBehaviorFixture( &fixture );
	const uint32_t iFnBlock = fixture.szNodes[10].nLastChild;
	const uint32_t iAssignment = newNode( &fixture, SIR_LANG_SYNTAX_ASSIGNMENT, 31, 33 );
	const uint32_t iName = newNode( &fixture, SIR_LANG_SYNTAX_NAME, 32, 33 );
	const uint32_t iLiteral = newNode( &fixture, SIR_LANG_SYNTAX_LITERAL, 8, 9 );
	// bool is a type keyword and not a literal; use a numeric token with a
	// different width by substituting the token for the expression.
	const uint32_t iNumber = addToken( &fixture, "12u8" );
	fixture.szNodes[iLiteral].nStartToken = iNumber;
	fixture.szNodes[iLiteral].nEndToken = iNumber + 1;
	appendChild( &fixture, iAssignment, iName );
	appendChild( &fixture, iAssignment, iLiteral );
	appendChild( &fixture, iFnBlock, iAssignment );
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_TYPE_MISMATCH );
	assert( hasDiagnostic( &fixture, SIR_LANG_SEMANTIC_TYPE_MISMATCH ) );
}

static void testIntegerWidthAndSuffix( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	initializeFixture( &fixture );
	static constexpr const char *s_pszWords[] = { "fn",  "f", "(", ")",  "-", ">",     "u8",     ":",
	                                              "let", "x", ":", "u8", "=", "256u8", "return", "x" };
	addTokens( &fixture, s_pszWords, sizeof( s_pszWords ) / sizeof( s_pszWords[0] ) );
	const uint32_t iModule = newNode( &fixture, SIR_LANG_SYNTAX_MODULE, 0, 16 );
	const uint32_t iFn = newNode( &fixture, SIR_LANG_SYNTAX_FUNCTION, 0, 16 );
	const uint32_t iReturnType = newNode( &fixture, SIR_LANG_SYNTAX_TYPE, 6, 7 );
	const uint32_t iBlock = newNode( &fixture, SIR_LANG_SYNTAX_BLOCK, 7, 16 );
	const uint32_t iLet = newNode( &fixture, SIR_LANG_SYNTAX_LET, 8, 14 );
	const uint32_t iLocalType = newNode( &fixture, SIR_LANG_SYNTAX_TYPE, 11, 12 );
	const uint32_t iNumber = newNode( &fixture, SIR_LANG_SYNTAX_LITERAL, 13, 14 );
	const uint32_t iReturn = newNode( &fixture, SIR_LANG_SYNTAX_RETURN, 14, 16 );
	const uint32_t iName = newNode( &fixture, SIR_LANG_SYNTAX_NAME, 15, 16 );
	appendChild( &fixture, iModule, iFn );
	appendChild( &fixture, iFn, iReturnType );
	appendChild( &fixture, iFn, iBlock );
	appendChild( &fixture, iBlock, iLet );
	appendChild( &fixture, iBlock, iReturn );
	appendChild( &fixture, iLet, iLocalType );
	appendChild( &fixture, iLet, iNumber );
	appendChild( &fixture, iReturn, iName );
	fixture.tree.nRoot = iModule;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_INVALID_LITERAL );

	uint8_t *pValue = ( uint8_t * )fixture.tree.source.pBytes + fixture.szTokens[13].nOffset;
	memcpy( pValue, "255u8", 5 );
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_OK );
	assert( fixture.output.nDiagnosticCount == 0 );
	memcpy( pValue, "254i8", 5 );
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_INVALID_LITERAL );
	memcpy( pValue, "127i8", 5 );
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_TYPE_MISMATCH );
}

static void testForwardSchemaFlags( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	initializeFixture( &fixture );
	static constexpr const char *s_pszWords[] = {
	    "behavior", "Guard", "uses",   "FlagsSchema", ":",           "fn", "run",       "(",    ")",     ":", "when", "this",   ".",
	    "Alerted",  ":",     "return", "schema",      "FlagsSchema", ":",  "condition", "bank", "Flags", ":", "u16",  "Alerted" };
	addTokens( &fixture, s_pszWords, sizeof( s_pszWords ) / sizeof( s_pszWords[0] ) );
	const uint32_t iRoot = newNode( &fixture, SIR_LANG_SYNTAX_MODULE, 0, 25 );
	const uint32_t iBehavior = newNode( &fixture, SIR_LANG_SYNTAX_BEHAVIOR, 0, 16 );
	const uint32_t iUses = newNode( &fixture, SIR_LANG_SYNTAX_TYPE, 3, 4 );
	const uint32_t iBehaviorBlock = newNode( &fixture, SIR_LANG_SYNTAX_BLOCK, 4, 16 );
	const uint32_t iFn = newNode( &fixture, SIR_LANG_SYNTAX_FUNCTION, 5, 16 );
	const uint32_t iFnBlock = newNode( &fixture, SIR_LANG_SYNTAX_BLOCK, 9, 16 );
	const uint32_t iWhen = newNode( &fixture, SIR_LANG_SYNTAX_WHEN, 10, 16 );
	const uint32_t iMember = newNode( &fixture, SIR_LANG_SYNTAX_MEMBER, 11, 14 );
	const uint32_t iSelf = newNode( &fixture, SIR_LANG_SYNTAX_NAME, 11, 12 );
	const uint32_t iWhenBlock = newNode( &fixture, SIR_LANG_SYNTAX_BLOCK, 14, 16 );
	const uint32_t iReturn = newNode( &fixture, SIR_LANG_SYNTAX_RETURN, 15, 16 );
	const uint32_t iSchema = newNode( &fixture, SIR_LANG_SYNTAX_SCHEMA, 16, 25 );
	const uint32_t iSchemaBlock = newNode( &fixture, SIR_LANG_SYNTAX_BLOCK, 18, 25 );
	const uint32_t iBank = newNode( &fixture, SIR_LANG_SYNTAX_CONDITION_BANK, 19, 25 );
	const uint32_t iBankType = newNode( &fixture, SIR_LANG_SYNTAX_TYPE, 23, 24 );
	const uint32_t iFlag = newNode( &fixture, SIR_LANG_SYNTAX_FIELD, 24, 25 );
	appendChild( &fixture, iRoot, iBehavior );
	appendChild( &fixture, iRoot, iSchema );
	appendChild( &fixture, iBehavior, iUses );
	appendChild( &fixture, iBehavior, iBehaviorBlock );
	appendChild( &fixture, iBehaviorBlock, iFn );
	appendChild( &fixture, iFn, iFnBlock );
	appendChild( &fixture, iFnBlock, iWhen );
	appendChild( &fixture, iWhen, iMember );
	appendChild( &fixture, iWhen, iWhenBlock );
	appendChild( &fixture, iMember, iSelf );
	appendChild( &fixture, iWhenBlock, iReturn );
	appendChild( &fixture, iSchema, iSchemaBlock );
	appendChild( &fixture, iSchemaBlock, iBank );
	appendChild( &fixture, iBank, iBankType );
	appendChild( &fixture, iBank, iFlag );
	fixture.tree.nRoot = iRoot;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_OK );
	assert( fixture.output.nDiagnosticCount == 0 );
	bool bResolvedFlag = false;
	for ( uint32_t iRef = 0; iRef < fixture.output.nReferenceCount; ++iRef ) {
		const sirLangResolvedReference_t& ref = fixture.output.pReferences[iRef];
		if ( ref.iUseNode == iMember && ref.iDeclarationNode == iFlag && ref.eValue == SIR_LANG_VALUE_BOOL ) {
			bResolvedFlag = true;
		}
	}
	assert( bResolvedFlag );
}

static void testExternalTypeRegistry( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	initializeFixture( &fixture );
	static constexpr const char *s_pszWords[] = { "fn", "process", "(", "item", ":", "EntityID", ")", ":", "return" };
	addTokens( &fixture, s_pszWords, sizeof( s_pszWords ) / sizeof( s_pszWords[0] ) );
	const uint32_t iRoot = newNode( &fixture, SIR_LANG_SYNTAX_MODULE, 0, 9 );
	const uint32_t iFunction = newNode( &fixture, SIR_LANG_SYNTAX_FUNCTION, 0, 9 );
	const uint32_t iParameter = newNode( &fixture, SIR_LANG_SYNTAX_PARAMETER, 3, 6 );
	const uint32_t iParameterType = newNode( &fixture, SIR_LANG_SYNTAX_TYPE, 5, 6 );
	const uint32_t iBlock = newNode( &fixture, SIR_LANG_SYNTAX_BLOCK, 7, 9 );
	const uint32_t iReturn = newNode( &fixture, SIR_LANG_SYNTAX_RETURN, 8, 9 );
	appendChild( &fixture, iRoot, iFunction );
	appendChild( &fixture, iFunction, iParameter );
	appendChild( &fixture, iFunction, iBlock );
	appendChild( &fixture, iParameter, iParameterType );
	appendChild( &fixture, iBlock, iReturn );
	fixture.tree.nRoot = iRoot;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_UNKNOWN_TYPE );
	const sirLangExternalSymbol_t szExternals[] = { { "EntityID", SIR_LANG_SYMBOL_TYPE, SIR_LANG_VALUE_OTHER } };
	fixture.output.pExternals = szExternals;
	fixture.output.nExternalCount = 1;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_OK );
	assert( fixture.output.nReferenceCount == 1 );
	assert( fixture.output.pReferences[0].iUseNode == iParameterType );
	assert( fixture.output.pReferences[0].iDeclarationNode == SIR_LANG_INVALID_INDEX );
}

static void testInvalidGlobalState( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	initializeFixture( &fixture );
	static constexpr const char *s_pszWords[] = { "let", "value", ":", "i32", "=", "4i32" };
	addTokens( &fixture, s_pszWords, sizeof( s_pszWords ) / sizeof( s_pszWords[0] ) );
	const uint32_t iRoot = newNode( &fixture, SIR_LANG_SYNTAX_MODULE, 0, 6 );
	const uint32_t iLet = newNode( &fixture, SIR_LANG_SYNTAX_LET, 0, 6 );
	const uint32_t iType = newNode( &fixture, SIR_LANG_SYNTAX_TYPE, 3, 4 );
	const uint32_t iLiteral = newNode( &fixture, SIR_LANG_SYNTAX_LITERAL, 5, 6 );
	appendChild( &fixture, iRoot, iLet );
	appendChild( &fixture, iLet, iType );
	appendChild( &fixture, iLet, iLiteral );
	fixture.tree.nRoot = iRoot;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_INVALID_GLOBAL );
}

static void testDuplicateParameterAndReferenceBudget( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	buildBehaviorFixture( &fixture );
	const uint32_t iDuplicatedParameter = newNode( &fixture, SIR_LANG_SYNTAX_PARAMETER, 17, 20 );
	const uint32_t iFn = 10;
	appendChild( &fixture, iFn, iDuplicatedParameter );
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_DUPLICATE_SYMBOL );

	buildBehaviorFixture( &fixture );
	fixture.output.nReferenceCapacity = 1;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_CAPACITY );
	assert( fixture.output.bTruncated );
}

static void testCapacityAndBadTree( void ) noexcept
{
	sirLangSemanticFixture_t fixture = {};
	buildBehaviorFixture( &fixture );
	fixture.output.nSymbolCapacity = 2;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_CAPACITY );
	assert( fixture.output.bTruncated );
	buildBehaviorFixture( &fixture );
	fixture.szNodes[0].nFirstChild = UINT32_MAX - 1;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_INVALID_TREE );
	buildBehaviorFixture( &fixture );
	fixture.tree.nDiagnosticCount = 1;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_INVALID_INPUT );
	buildBehaviorFixture( &fixture );
	fixture.szNodes[1].nNextSibling = 1;
	assert( analyzeFixture( &fixture ) == SIR_LANG_SEMANTIC_INVALID_TREE );
}

int main( void )
{
	testLocalScopesAndTypes();
	testUseBeforeDeclaration();
	testUnknownReturnType();
	testUnknownLocalType();
	testDuplicateDeclaration();
	testWrongAssignmentType();
	testIntegerWidthAndSuffix();
	testForwardSchemaFlags();
	testExternalTypeRegistry();
	testInvalidGlobalState();
	testDuplicateParameterAndReferenceBudget();
	testCapacityAndBadTree();
	puts( "SIR-Lang semantic regression tests passed" );
	return 0;
}
