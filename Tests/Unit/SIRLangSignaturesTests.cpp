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

#include <Scripting/Language/SIRLangSignatures.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct sirLangSignatureFixture_s
{
	char szSource[8192];
	sirLangToken_t szTokens[128];
	sirLangSyntaxNode_t szNodes[128];
	sirLangParseDiagnostic_t szParseDiagnostics[16];
	sirLangSemanticSymbol_t szSymbols[128];
	sirLangResolvedReference_t szReferences[128];
	sirLangSemanticDiagnostic_t szSemanticDiagnostics[64];
	sirLangFunctionSignature_t szFunctions[32];
	sirLangSignatureParameter_t szParameters[64];
	sirLangResolvedCall_t szCalls[32];
	sirLangTypeIdentity_t szTypes[128];
	sirLangSignatureDiagnostic_t szSignatureDiagnostics[64];
	sirLangSyntaxTree_t tree;
	sirLangSemanticOutput_t semantic;
	sirLangSignatureOutput_t signatures;
	uint32_t nSourceBytes;
	uint32_t iCall;
	uint32_t iCallee;
	uint32_t iArgument;
	uint32_t iCallerReturnType;
} sirLangSignatureFixture_t;

static void sirLangInitFixture( sirLangSignatureFixture_t *pFixture ) noexcept
{
	memset( pFixture, 0, sizeof( *pFixture ) );
	pFixture->tree.pTokens = pFixture->szTokens;
	pFixture->tree.nTokenCapacity = 128;
	pFixture->tree.pNodes = pFixture->szNodes;
	pFixture->tree.nNodeCapacity = 128;
	pFixture->tree.pDiagnostics = pFixture->szParseDiagnostics;
	pFixture->tree.nDiagnosticCapacity = 16;
	pFixture->semantic.pSymbols = pFixture->szSymbols;
	pFixture->semantic.nSymbolCapacity = 128;
	pFixture->semantic.pReferences = pFixture->szReferences;
	pFixture->semantic.nReferenceCapacity = 128;
	pFixture->semantic.pDiagnostics = pFixture->szSemanticDiagnostics;
	pFixture->semantic.nDiagnosticCapacity = 64;
	pFixture->signatures.pFunctions = pFixture->szFunctions;
	pFixture->signatures.nFunctionCapacity = 32;
	pFixture->signatures.pParameters = pFixture->szParameters;
	pFixture->signatures.nParameterCapacity = 64;
	pFixture->signatures.pCalls = pFixture->szCalls;
	pFixture->signatures.nCallCapacity = 32;
	pFixture->signatures.pNodeTypes = pFixture->szTypes;
	pFixture->signatures.nNodeTypeCapacity = 128;
	pFixture->signatures.pDiagnostics = pFixture->szSignatureDiagnostics;
	pFixture->signatures.nDiagnosticCapacity = 64;
}

static uint32_t sirLangAddToken( sirLangSignatureFixture_t *pFixture, const char *pszText ) noexcept
{
	const size_t nLength = strlen( pszText );
	assert( pFixture->tree.nTokenCount < 128 );
	assert( pFixture->nSourceBytes + nLength + 2 < sizeof( pFixture->szSource ) );
	const uint32_t iToken = pFixture->tree.nTokenCount++;
	const uint32_t nOffset = pFixture->nSourceBytes;
	memcpy( pFixture->szSource + nOffset, pszText, nLength );
	pFixture->nSourceBytes += ( uint32_t )nLength;
	pFixture->szSource[pFixture->nSourceBytes++] = ' ';
	pFixture->tree.source = { ( const uint8_t * )pFixture->szSource, pFixture->nSourceBytes };
	sirLangTokenKind_t eKind = SIR_LANG_TOKEN_IDENTIFIER;
	if ( pszText[0] >= '0' && pszText[0] <= '9' ) {
		eKind = SIR_LANG_TOKEN_NUMBER;
	}
	pFixture->szTokens[iToken] = { eKind, SIR_LANG_LEX_OK, nOffset, ( uint32_t )nLength, 1, nOffset + 1 };
	return iToken;
}

static uint32_t sirLangAddNode( sirLangSignatureFixture_t *pFixture, sirLangSyntaxKind_t eKind, uint32_t iStart, uint32_t iEnd ) noexcept
{
	assert( pFixture->tree.nNodeCount < 128 );
	const uint32_t iNode = pFixture->tree.nNodeCount++;
	pFixture->szNodes[iNode] = { eKind, iStart, iEnd, UINT32_MAX, UINT32_MAX, UINT32_MAX };
	return iNode;
}

static void sirLangAppendChild( sirLangSignatureFixture_t *pFixture, uint32_t iParent, uint32_t iChild ) noexcept
{
	sirLangSyntaxNode_t& parent = pFixture->szNodes[iParent];
	if ( parent.nFirstChild == UINT32_MAX ) {
		parent.nFirstChild = iChild;
	} else {
		pFixture->szNodes[parent.nLastChild].nNextSibling = iChild;
	}
	parent.nLastChild = iChild;
}

static void sirLangBuildForwardCall( sirLangSignatureFixture_t *pFixture ) noexcept
{
	sirLangInitFixture( pFixture );
	static constexpr const char *s_pszWords[] = { "fn", "caller", "(",     "x", ":", "u16", ")",  "-",      ">", "u16",
	                                              ":",  "return", "later", "(", "x", ")",   "fn", "later",  "(", "v",
	                                              ":",  "u16",    ")",     "-", ">", "u16", ":",  "return", "v" };
	for ( uint32_t iWord = 0; iWord < sizeof( s_pszWords ) / sizeof( s_pszWords[0] ); ++iWord ) {
		sirLangAddToken( pFixture, s_pszWords[iWord] );
	}
	const uint32_t iRoot = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_MODULE, 0, 29 );
	const uint32_t iCaller = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_FUNCTION, 0, 16 );
	const uint32_t iCallerParam = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_PARAMETER, 3, 6 );
	const uint32_t iCallerParamType = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_TYPE, 5, 6 );
	pFixture->iCallerReturnType = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_TYPE, 9, 10 );
	const uint32_t iCallerBlock = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_BLOCK, 11, 16 );
	const uint32_t iReturn = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_RETURN, 11, 16 );
	pFixture->iCall = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_CALL, 12, 16 );
	pFixture->iCallee = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_NAME, 12, 13 );
	pFixture->iArgument = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_NAME, 14, 15 );
	const uint32_t iLater = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_FUNCTION, 16, 29 );
	const uint32_t iLaterParam = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_PARAMETER, 19, 22 );
	const uint32_t iLaterParamType = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_TYPE, 21, 22 );
	const uint32_t iLaterReturnType = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_TYPE, 25, 26 );
	const uint32_t iLaterBlock = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_BLOCK, 27, 29 );
	const uint32_t iLaterReturn = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_RETURN, 27, 29 );
	const uint32_t iLaterValue = sirLangAddNode( pFixture, SIR_LANG_SYNTAX_NAME, 28, 29 );
	sirLangAppendChild( pFixture, iRoot, iCaller );
	sirLangAppendChild( pFixture, iRoot, iLater );
	sirLangAppendChild( pFixture, iCaller, iCallerParam );
	sirLangAppendChild( pFixture, iCaller, pFixture->iCallerReturnType );
	sirLangAppendChild( pFixture, iCaller, iCallerBlock );
	sirLangAppendChild( pFixture, iCallerParam, iCallerParamType );
	sirLangAppendChild( pFixture, iCallerBlock, iReturn );
	sirLangAppendChild( pFixture, iReturn, pFixture->iCall );
	sirLangAppendChild( pFixture, pFixture->iCall, pFixture->iCallee );
	sirLangAppendChild( pFixture, pFixture->iCall, pFixture->iArgument );
	sirLangAppendChild( pFixture, iLater, iLaterParam );
	sirLangAppendChild( pFixture, iLater, iLaterReturnType );
	sirLangAppendChild( pFixture, iLater, iLaterBlock );
	sirLangAppendChild( pFixture, iLaterParam, iLaterParamType );
	sirLangAppendChild( pFixture, iLaterBlock, iLaterReturn );
	sirLangAppendChild( pFixture, iLaterReturn, iLaterValue );
	pFixture->tree.nRoot = iRoot;
}

static sirLangSignatureError_t sirLangRunFixture( sirLangSignatureFixture_t *pFixture ) noexcept
{
	const sirLangSemanticError_t eSemantic = SIRLangAnalyze( &pFixture->tree, &pFixture->semantic );
	if ( eSemantic != SIR_LANG_SEMANTIC_OK ) {
		fprintf( stderr, "Semantic fixture failure: %u\n", ( unsigned )eSemantic );
	}
	assert( eSemantic == SIR_LANG_SEMANTIC_OK );
	return SIRLangResolveSignatures( &pFixture->tree, &pFixture->semantic, &pFixture->signatures );
}

static bool sirLangHasError( const sirLangSignatureFixture_t *pFixture, sirLangSignatureError_t eError ) noexcept
{
	for ( uint32_t iDiag = 0; iDiag < pFixture->signatures.nDiagnosticCount; ++iDiag ) {
		if ( pFixture->szSignatureDiagnostics[iDiag].eError == eError ) {
			return true;
		}
	}
	return false;
}

static void sirLangReplaceToken( sirLangSignatureFixture_t *pFixture, uint32_t iToken, const char *pszReplacement ) noexcept
{
	const uint32_t iSource = sirLangAddToken( pFixture, pszReplacement );
	pFixture->szTokens[iToken] = pFixture->szTokens[iSource];
}

static void sirLangTestForwardCalls( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
	assert( fixture.signatures.nFunctionCount == 2 );
	assert( fixture.signatures.nParameterCount == 2 );
	assert( fixture.signatures.nCallCount == 1 );
	assert( fixture.szCalls[0].iCallNode == fixture.iCall );
	assert( fixture.szCalls[0].iDeclarationNode == 10 );
	assert( fixture.szCalls[0].result.eValue == SIR_LANG_VALUE_U16 );
	assert( fixture.szCalls[0].nSignatureFingerprint != 0 );
	assert( fixture.szTypes[fixture.iCall].eValue == SIR_LANG_VALUE_U16 );
	const uint64_t nFirstHash = fixture.szFunctions[1].nFingerprint;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
	assert( fixture.szFunctions[1].nFingerprint == nFirstHash );
}

static void sirLangTestBadArgumentType( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	sirLangReplaceToken( &fixture, 14, "7u8" );
	fixture.szNodes[fixture.iArgument].eKind = SIR_LANG_SYNTAX_LITERAL;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_ARGUMENT_TYPE );
	assert( sirLangHasError( &fixture, SIR_LANG_SIGNATURE_ARGUMENT_TYPE ) );
}

static void sirLangTestMissingArgument( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	fixture.szNodes[fixture.iCallee].nNextSibling = UINT32_MAX;
	fixture.szNodes[fixture.iCall].nLastChild = fixture.iCallee;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_ARGUMENT_COUNT );
}

static void sirLangTestBadReturnType( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	sirLangReplaceToken( &fixture, 9, "u8" );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_RETURN_TYPE );
}

static void sirLangTestNamedArgument( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const uint32_t iNameToken = sirLangAddToken( &fixture, "v" );
	const uint32_t iNamed = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAMED_ARGUMENT, iNameToken, iNameToken + 1 );
	fixture.szNodes[fixture.iCallee].nNextSibling = iNamed;
	fixture.szNodes[fixture.iCall].nLastChild = iNamed;
	fixture.szNodes[fixture.iArgument].nNextSibling = UINT32_MAX;
	sirLangAppendChild( &fixture, iNamed, fixture.iArgument );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
}

static void sirLangTestUnknownNamedArgument( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const uint32_t iNameToken = sirLangAddToken( &fixture, "missing" );
	const uint32_t iNamed = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAMED_ARGUMENT, iNameToken, iNameToken + 1 );
	fixture.szNodes[fixture.iCallee].nNextSibling = iNamed;
	fixture.szNodes[fixture.iCall].nLastChild = iNamed;
	sirLangAppendChild( &fixture, iNamed, fixture.iArgument );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_UNKNOWN_ARGUMENT );
}

static void sirLangTestTypedExternal( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	sirLangReplaceToken( &fixture, 12, "native_answer" );
	const sirLangExternalSymbol_t externalSymbol = { "native_answer", SIR_LANG_SYMBOL_FUNCTION, SIR_LANG_VALUE_FUNCTION };
	static constexpr sirLangValueKind_t s_szNativeTypes[] = { SIR_LANG_VALUE_U16 };
	static constexpr const char *s_pszNativeNames[] = { "arg" };
	const sirLangExternalSignature_t externalSignature = { "native_answer", s_szNativeTypes, s_pszNativeNames, 1, SIR_LANG_VALUE_U16,
	                                                       0x1234,          0x5678 };
	fixture.semantic.pExternals = &externalSymbol;
	fixture.semantic.nExternalCount = 1;
	fixture.signatures.pExternals = &externalSignature;
	fixture.signatures.nExternalCount = 1;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
	assert( fixture.signatures.nCallCount == 1 );
	assert( fixture.szCalls[0].nFunctionId == 0x1234 );
	assert( fixture.szCalls[0].nSignatureFingerprint == 0x5678 );
	assert( fixture.szCalls[0].iDeclarationNode == UINT32_MAX );
}

static void sirLangTestUntypedExternalAndCapacity( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	sirLangReplaceToken( &fixture, 12, "abs" );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_UNTYPED_CALLEE );

	sirLangBuildForwardCall( &fixture );
	fixture.signatures.nCallCapacity = 0;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_CAPACITY );
	assert( fixture.signatures.bTruncated );

	sirLangBuildForwardCall( &fixture );
	fixture.signatures.nNodeTypeCapacity = 1;
	const sirLangSemanticError_t eSemantic = SIRLangAnalyze( &fixture.tree, &fixture.semantic );
	assert( eSemantic == SIR_LANG_SEMANTIC_OK );
	assert( SIRLangResolveSignatures( &fixture.tree, &fixture.semantic, &fixture.signatures ) == SIR_LANG_SIGNATURE_INVALID_INPUT );
}

static void sirLangTestHexLiteral( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	sirLangReplaceToken( &fixture, 14, "0xFEu16" );
	fixture.szNodes[fixture.iArgument].eKind = SIR_LANG_SYNTAX_LITERAL;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
	assert( fixture.szTypes[fixture.iArgument].eValue == SIR_LANG_VALUE_U16 );
}

static void sirLangTestDuplicateNamedArgument( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const uint32_t iName = sirLangAddToken( &fixture, "v" );
	const uint32_t iFirst = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAMED_ARGUMENT, iName, iName + 1 );
	const uint32_t iSecond = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAMED_ARGUMENT, iName, iName + 1 );
	const uint32_t iAdditional = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAME, 14, 15 );
	fixture.szNodes[fixture.iCallee].nNextSibling = iFirst;
	fixture.szNodes[fixture.iCall].nLastChild = iSecond;
	sirLangAppendChild( &fixture, iFirst, fixture.iArgument );
	sirLangAppendChild( &fixture, iSecond, iAdditional );
	fixture.szNodes[iFirst].nNextSibling = iSecond;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_DUPLICATE_ARGUMENT );
}

static void sirLangTestPositionalAfterNamed( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const uint32_t iName = sirLangAddToken( &fixture, "v" );
	const uint32_t iNamed = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAMED_ARGUMENT, iName, iName + 1 );
	const uint32_t iPositional = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAME, 14, 15 );
	fixture.szNodes[fixture.iCallee].nNextSibling = iNamed;
	fixture.szNodes[fixture.iCall].nLastChild = iPositional;
	sirLangAppendChild( &fixture, iNamed, fixture.iArgument );
	fixture.szNodes[iNamed].nNextSibling = iPositional;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_POSITIONAL_AFTER_NAMED );
}

static void sirLangSetCompositeType( sirLangSignatureFixture_t *pFixture, uint32_t iType, const char *pszElement ) noexcept
{
	const uint32_t iStart = sirLangAddToken( pFixture, "option" );
	sirLangAddToken( pFixture, "<" );
	sirLangAddToken( pFixture, pszElement );
	sirLangAddToken( pFixture, ">" );
	pFixture->szNodes[iType].nStartToken = iStart;
	pFixture->szNodes[iType].nEndToken = iStart + 4;
}

static void sirLangTestCompositeTypeIdentity( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	sirLangSetCompositeType( &fixture, 3, "u16" );
	sirLangSetCompositeType( &fixture, fixture.iCallerReturnType, "u16" );
	sirLangSetCompositeType( &fixture, 12, "u16" );
	sirLangSetCompositeType( &fixture, 13, "u16" );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
	assert( fixture.szFunctions[0].result.eValue == SIR_LANG_VALUE_OTHER );
	assert( fixture.szFunctions[0].result.nFingerprint == fixture.szFunctions[1].result.nFingerprint );
	const uint64_t nIdentity = fixture.szFunctions[0].result.nFingerprint;

	sirLangSetCompositeType( &fixture, 12, "u32" );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_ARGUMENT_TYPE );
	assert( fixture.szFunctions[0].result.nFingerprint == nIdentity );
}

static void sirLangTestTypedCallResultInLet( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const uint32_t iBlock = fixture.szNodes[1].nLastChild;
	const uint32_t iLet = fixture.szNodes[iBlock].nFirstChild;
	const uint32_t iTypeToken = sirLangAddToken( &fixture, "u8" );
	const uint32_t iType = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_TYPE, iTypeToken, iTypeToken + 1 );
	assert( iLet < fixture.tree.nNodeCount );
	fixture.szNodes[iLet].eKind = SIR_LANG_SYNTAX_LET;
	sirLangReplaceToken( &fixture, 11, "let" );
	fixture.szNodes[iLet].nFirstChild = iType;
	fixture.szNodes[iLet].nLastChild = fixture.iCall;
	fixture.szNodes[iType].nNextSibling = fixture.iCall;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_ARGUMENT_TYPE );
}

static void sirLangTestInvalidNativeSignature( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const sirLangExternalSignature_t bad = { "later", nullptr, nullptr, 1, SIR_LANG_VALUE_U16, 1, 2 };
	fixture.signatures.pExternals = &bad;
	fixture.signatures.nExternalCount = 1;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_INVALID_SIGNATURE );
}

static void sirLangTestCompositeExpressions( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const uint32_t iStart = sirLangAddToken( &fixture, "x" );
	sirLangAddToken( &fixture, "+" );
	sirLangAddToken( &fixture, "x" );
	fixture.szNodes[fixture.iArgument].eKind = SIR_LANG_SYNTAX_BINARY;
	fixture.szNodes[fixture.iArgument].nStartToken = iStart;
	fixture.szNodes[fixture.iArgument].nEndToken = iStart + 3;
	const uint32_t iLeft = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAME, iStart, iStart + 1 );
	const uint32_t iRight = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAME, iStart + 2, iStart + 3 );
	sirLangAppendChild( &fixture, fixture.iArgument, iLeft );
	sirLangAppendChild( &fixture, fixture.iArgument, iRight );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
	assert( fixture.szTypes[fixture.iArgument].eValue == SIR_LANG_VALUE_U16 );

	sirLangBuildForwardCall( &fixture );
	const uint32_t iUnaryToken = sirLangAddToken( &fixture, "+" );
	const uint32_t iOperand = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAME, 14, 15 );
	fixture.szNodes[fixture.iArgument].eKind = SIR_LANG_SYNTAX_UNARY;
	fixture.szNodes[fixture.iArgument].nStartToken = iUnaryToken;
	fixture.szNodes[fixture.iArgument].nEndToken = iUnaryToken + 1;
	sirLangAppendChild( &fixture, fixture.iArgument, iOperand );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );

	sirLangBuildForwardCall( &fixture );
	const uint32_t iGroupValue = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAME, 14, 15 );
	fixture.szNodes[fixture.iArgument].eKind = SIR_LANG_SYNTAX_GROUP;
	sirLangAppendChild( &fixture, fixture.iArgument, iGroupValue );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
}

static void sirLangTestExternalNamedArgument( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	sirLangReplaceToken( &fixture, 12, "native_answer" );
	const uint32_t iNamedToken = sirLangAddToken( &fixture, "arg" );
	const uint32_t iNamed = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAMED_ARGUMENT, iNamedToken, iNamedToken + 1 );
	fixture.szNodes[fixture.iCallee].nNextSibling = iNamed;
	fixture.szNodes[fixture.iCall].nLastChild = iNamed;
	sirLangAppendChild( &fixture, iNamed, fixture.iArgument );
	const sirLangExternalSymbol_t externalSymbol = { "native_answer", SIR_LANG_SYMBOL_FUNCTION, SIR_LANG_VALUE_FUNCTION };
	static constexpr sirLangValueKind_t s_szNativeTypes[] = { SIR_LANG_VALUE_U16 };
	static constexpr const char *s_pszNativeNames[] = { "arg" };
	const sirLangExternalSignature_t native = { "native_answer", s_szNativeTypes, s_pszNativeNames, 1, SIR_LANG_VALUE_U16, 0x1234, 0x5678 };
	fixture.semantic.pExternals = &externalSymbol;
	fixture.semantic.nExternalCount = 1;
	fixture.signatures.pExternals = &native;
	fixture.signatures.nExternalCount = 1;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
}

static void sirLangTestFunctionAndParameterCapacity( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	fixture.signatures.nFunctionCapacity = 1;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_CAPACITY );
	assert( fixture.signatures.bTruncated );

	sirLangBuildForwardCall( &fixture );
	fixture.signatures.nParameterCapacity = 1;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_CAPACITY );
	assert( fixture.signatures.bTruncated );
}

static void sirLangTestNestedCalls( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const uint32_t iInner = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_CALL, 12, 16 );
	const uint32_t iInnerCallee = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAME, 12, 13 );
	const uint32_t iInnerArg = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAME, 14, 15 );
	fixture.szNodes[fixture.iArgument].nNextSibling = UINT32_MAX;
	fixture.szNodes[fixture.iCallee].nNextSibling = iInner;
	fixture.szNodes[fixture.iCall].nLastChild = iInner;
	sirLangAppendChild( &fixture, iInner, iInnerCallee );
	sirLangAppendChild( &fixture, iInner, iInnerArg );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
	assert( fixture.signatures.nCallCount == 2 );
	assert( fixture.szTypes[fixture.iCall].eValue == SIR_LANG_VALUE_U16 );
}

static void sirLangTestLiteralKinds( void ) noexcept
{
	static constexpr const char *s_pszValues[] = { "1", "1.5f32", "true", "'atom'", "1.5" };
	for ( uint32_t iCase = 0; iCase < sizeof( s_pszValues ) / sizeof( s_pszValues[0] ); ++iCase ) {
		sirLangSignatureFixture_t fixture = {};
		sirLangBuildForwardCall( &fixture );
		sirLangReplaceToken( &fixture, 14, s_pszValues[iCase] );
		fixture.szNodes[fixture.iArgument].eKind = SIR_LANG_SYNTAX_LITERAL;
		if ( s_pszValues[iCase][0] == '\'' ) {
			fixture.szTokens[14].eKind = SIR_LANG_TOKEN_STRING;
		}
		assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_ARGUMENT_TYPE );
	}
}

static void sirLangTestWhenConditionReturnType( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const uint32_t iBlock = fixture.szNodes[1].nLastChild;
	const uint32_t iWhen = fixture.szNodes[iBlock].nFirstChild;
	const uint32_t iWhenBlock = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_BLOCK, 15, 16 );
	fixture.szNodes[iWhen].eKind = SIR_LANG_SYNTAX_WHEN;
	sirLangReplaceToken( &fixture, 11, "when" );
	sirLangAppendChild( &fixture, iWhen, iWhenBlock );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_ARGUMENT_TYPE );
}

static void sirLangTestAssignmentTypedCall( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	const uint32_t iBlock = fixture.szNodes[1].nLastChild;
	const uint32_t iAssignment = fixture.szNodes[iBlock].nFirstChild;
	const uint32_t iLeft = sirLangAddNode( &fixture, SIR_LANG_SYNTAX_NAME, 14, 15 );
	fixture.szNodes[iAssignment].eKind = SIR_LANG_SYNTAX_ASSIGNMENT;
	fixture.szNodes[iAssignment].nFirstChild = iLeft;
	fixture.szNodes[iAssignment].nLastChild = fixture.iCall;
	fixture.szNodes[iLeft].nNextSibling = fixture.iCall;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
}

static void sirLangTestExternalDescriptorValidation( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	static constexpr sirLangValueKind_t s_szOneType[] = { SIR_LANG_VALUE_U16 };
	static constexpr const char *s_pszOneName[] = { "arg" };
	static constexpr sirLangValueKind_t s_szTwoTypes[] = { SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16 };
	static constexpr const char *s_pszDuplicateNames[] = { "arg", "arg" };
	sirLangExternalSignature_t szExternals[2] = { { "native", s_szOneType, s_pszOneName, 1, SIR_LANG_VALUE_U16, 2, 3 },
	                                              { "native", s_szOneType, s_pszOneName, 1, SIR_LANG_VALUE_U16, 4, 5 } };

	sirLangBuildForwardCall( &fixture );
	fixture.signatures.pExternals = szExternals;
	fixture.signatures.nExternalCount = 2;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_INVALID_SIGNATURE );

	szExternals[0] = { "native", s_szTwoTypes, s_pszDuplicateNames, 2, SIR_LANG_VALUE_U16, 2, 3 };
	sirLangBuildForwardCall( &fixture );
	fixture.signatures.pExternals = szExternals;
	fixture.signatures.nExternalCount = 1;
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_INVALID_SIGNATURE );

	szExternals[0] = { "native", s_szOneType, s_pszOneName, 1, SIR_LANG_VALUE_OTHER, 2, 3 };
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_INVALID_SIGNATURE );

	static constexpr sirLangValueKind_t s_szVoid[] = { SIR_LANG_VALUE_VOID };
	szExternals[0] = { "native", s_szVoid, s_pszOneName, 1, SIR_LANG_VALUE_U16, 2, 3 };
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_INVALID_SIGNATURE );

	szExternals[0] = { nullptr, s_szOneType, s_pszOneName, 1, SIR_LANG_VALUE_U16, 2, 3 };
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_INVALID_SIGNATURE );
}

int main( void )
{
	sirLangTestForwardCalls();
	sirLangTestBadArgumentType();
	sirLangTestMissingArgument();
	sirLangTestBadReturnType();
	sirLangTestNamedArgument();
	sirLangTestUnknownNamedArgument();
	sirLangTestTypedExternal();
	sirLangTestUntypedExternalAndCapacity();
	sirLangTestHexLiteral();
	sirLangTestDuplicateNamedArgument();
	sirLangTestPositionalAfterNamed();
	sirLangTestCompositeTypeIdentity();
	sirLangTestTypedCallResultInLet();
	sirLangTestInvalidNativeSignature();
	sirLangTestCompositeExpressions();
	sirLangTestExternalNamedArgument();
	sirLangTestFunctionAndParameterCapacity();
	sirLangTestNestedCalls();
	sirLangTestLiteralKinds();
	sirLangTestWhenConditionReturnType();
	sirLangTestAssignmentTypedCall();
	sirLangTestExternalDescriptorValidation();
	puts( "SIR-Lang signature resolver regression tests passed" );
	return 0;
}
