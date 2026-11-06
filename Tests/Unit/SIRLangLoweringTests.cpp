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

#include <Scripting/Language/SIRLangLowering.h>
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

static void testSourceSignatureToGraphVM( void ) noexcept
{
	sirLangSignatureFixture_t fixture = {};
	sirLangBuildForwardCall( &fixture );
	assert( sirLangRunFixture( &fixture ) == SIR_LANG_SIGNATURE_OK );
	sirLangHIRNode_t szHIR[128] = {};
	sirLangHIRProgram_t hir = { szHIR, 128, 0, 0, false };
	assert( SIRLangBuildHIR( &fixture.tree, &fixture.semantic, &fixture.signatures, &hir ) == SIR_LANG_LOWER_OK );
	assert( hir.nCount == fixture.tree.nNodeCount );
	assert( szHIR[fixture.iCall].eType == SIR_LANG_VALUE_U16 );
	sirLangCIRFunction_t szFunctions[8] = {};
	sirLangCIRInstruction_t szInstructions[128] = {};
	sirLangValueKind_t szParameterTypes[32] = {};
	uint32_t szCallArguments[32] = {};
	sirLangCIRStorage_t builder = {};
	builder.pFunctions = szFunctions;
	builder.nFunctionCapacity = 8;
	builder.pInstructions = szInstructions;
	builder.nInstructionCapacity = 128;
	builder.pParameterTypes = szParameterTypes;
	builder.nParameterTypeCapacity = 32;
	builder.pCallArguments = szCallArguments;
	builder.nCallArgumentCapacity = 32;
	builder.nModuleId = 0xabcdefU;
	builder.nSchemaId = 17;
	builder.nLayoutDigest = 29;
	builder.nBindingDigest = 31;
	const sirLangLowerError_t eBuild = SIRLangBuildCIR( &fixture.tree, &fixture.signatures, &hir, &builder );
	if ( eBuild != SIR_LANG_LOWER_OK ) {
		fprintf( stderr, "CIR lower failure: %u at node %u\n", ( unsigned )eBuild, builder.iErrorNode );
	}
	assert( eBuild == SIR_LANG_LOWER_OK );
	assert( builder.program.nFunctionCount == 2 );
	assert( SIRLangVerifyCIR( &builder.program ) == SIR_LANG_VM_OK );
	const sirLangVMValue_t argument = { 42, SIR_LANG_VALUE_U16 };
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = builder.program.pFunctions[0].nFunctionId;
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	invocation.nFuel = 128;
	invocation.nExpectedLayoutDigest = builder.nLayoutDigest;
	invocation.nExpectedBindingDigest = builder.nBindingDigest;
	const sirLangVMRunResult_t result = SIRLangRunGraphVM( &builder.program, invocation );
	assert( result.eError == SIR_LANG_VM_OK );
	assert( result.result.eType == SIR_LANG_VALUE_U16 && result.result.nBits == 42 );
}

int main( void )
{
	testSourceSignatureToGraphVM();
	puts( "SIR-Lang HIR -> CIR -> GraphVM pipeline tests passed" );
	return 0;
}
