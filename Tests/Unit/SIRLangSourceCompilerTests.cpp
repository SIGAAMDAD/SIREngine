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

#include <Scripting/Language/SIRLangSourceCompiler.h>
#include <Scripting/Language/SIRLangBytecode.h>

#include <assert.h>
#include <string.h>

typedef struct sirLangCompilerFixture_s
{
	sirLangToken_t szTokens[256];
	sirLangSyntaxNode_t szNodes[256];
	sirLangParseDiagnostic_t szParseDiagnostics[32];
	sirLangSemanticSymbol_t szSymbols[256];
	sirLangResolvedReference_t szReferences[256];
	sirLangSemanticDiagnostic_t szSemanticDiagnostics[32];
	sirLangFunctionSignature_t szFunctions[32];
	sirLangSignatureParameter_t szParameters[64];
	sirLangResolvedCall_t szCalls[64];
	sirLangTypeIdentity_t szNodeTypes[256];
	sirLangSignatureDiagnostic_t szSignatureDiagnostics[32];
	sirLangHIRNode_t szHIR[256];
	sirLangCIRFunction_t szCIRFunctions[32];
	sirLangCIRInstruction_t szInstructions[512];
	sirLangValueKind_t szParameterTypes[128];
	uint32_t szCallArguments[128];
	sirLangEffectFunction_t szEffects[32];
	sirLangCompileWorkspace_t workspace;
} sirLangCompilerFixture_t;

static void setupFixture( sirLangCompilerFixture_t *pFixture ) noexcept
{
	*pFixture = {};
	sirLangCompileWorkspace_t& workspace = pFixture->workspace;
	workspace.syntax.pTokens = pFixture->szTokens;
	workspace.syntax.nTokenCapacity = 256;
	workspace.syntax.pNodes = pFixture->szNodes;
	workspace.syntax.nNodeCapacity = 256;
	workspace.syntax.pDiagnostics = pFixture->szParseDiagnostics;
	workspace.syntax.nDiagnosticCapacity = 32;
	workspace.semantic.pSymbols = pFixture->szSymbols;
	workspace.semantic.nSymbolCapacity = 256;
	workspace.semantic.pReferences = pFixture->szReferences;
	workspace.semantic.nReferenceCapacity = 256;
	workspace.semantic.pDiagnostics = pFixture->szSemanticDiagnostics;
	workspace.semantic.nDiagnosticCapacity = 32;
	workspace.signatures.pFunctions = pFixture->szFunctions;
	workspace.signatures.nFunctionCapacity = 32;
	workspace.signatures.pParameters = pFixture->szParameters;
	workspace.signatures.nParameterCapacity = 64;
	workspace.signatures.pCalls = pFixture->szCalls;
	workspace.signatures.nCallCapacity = 64;
	workspace.signatures.pNodeTypes = pFixture->szNodeTypes;
	workspace.signatures.nNodeTypeCapacity = 256;
	workspace.signatures.pDiagnostics = pFixture->szSignatureDiagnostics;
	workspace.signatures.nDiagnosticCapacity = 32;
	workspace.hir.pNodes = pFixture->szHIR;
	workspace.hir.nCapacity = 256;
	workspace.cir.pFunctions = pFixture->szCIRFunctions;
	workspace.cir.nFunctionCapacity = 32;
	workspace.cir.pInstructions = pFixture->szInstructions;
	workspace.cir.nInstructionCapacity = 512;
	workspace.cir.pParameterTypes = pFixture->szParameterTypes;
	workspace.cir.nParameterTypeCapacity = 128;
	workspace.cir.pCallArguments = pFixture->szCallArguments;
	workspace.cir.nCallArgumentCapacity = 128;
	workspace.cir.nModuleId = 101;
	workspace.cir.nSchemaId = 201;
	workspace.cir.nLayoutDigest = 301;
	workspace.cir.nBindingDigest = 401;
	workspace.effects.pFunctions = pFixture->szEffects;
	workspace.effects.nCapacity = 32;
}

static void testSourceToVM( void ) noexcept
{
	sirLangCompilerFixture_t fixture = {};
	setupFixture( &fixture );
	static constexpr char szSource[] = "fn echo(value: u16) -> u16:\n"
	                                    "    return value\n";
	const sirLangSourceView_t source = { ( const uint8_t * )szSource, sizeof( szSource ) - 1 };
	assert( SIRLangCompileSource( source, &fixture.workspace ) );
	assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_COMPLETE );
	assert( fixture.workspace.cir.program.nFunctionCount == 1 );
	assert( SIRLangVerifyCIR( &fixture.workspace.cir.program ) == SIR_LANG_VM_OK );

	const sirLangVMValue_t argument = { 42, SIR_LANG_VALUE_U16 };
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = fixture.workspace.cir.program.pFunctions[0].nFunctionId;
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	invocation.nFuel = 64;
	invocation.nExpectedLayoutDigest = 301;
	invocation.nExpectedBindingDigest = 401;
	const sirLangVMRunResult_t result = SIRLangRunGraphVM( &fixture.workspace.cir.program, invocation );
	assert( result.eError == SIR_LANG_VM_OK );
	assert( result.result.eType == SIR_LANG_VALUE_U16 );
	assert( result.result.nBits == 42 );

	uint8_t szBytecode[4096] = {};
	uint32_t nBytes = 0;
	assert( SIRLangEncodeGraphVM( &fixture.workspace.cir.program, szBytecode, sizeof( szBytecode ), &nBytes ) == SIR_LANG_VM_OK );
	assert( nBytes > SIR_LANG_BYTECODE_HEADER_BYTES );
	szBytecode[nBytes - 1] ^= 1;
	assert( SIRLangDecodeGraphVM( szBytecode, nBytes, &fixture.workspace.cir ) == SIR_LANG_VM_INVALID_FORMAT );
}

static void testRejectedSource( void ) noexcept
{
	sirLangCompilerFixture_t fixture = {};
	setupFixture( &fixture );
	static constexpr char szInvalid[] = "fn broken(value: u16:\n"
	                                    "    return value\n";
	assert( !SIRLangCompileSource( { ( const uint8_t * )szInvalid, sizeof( szInvalid ) - 1 }, &fixture.workspace ) );
	assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_PARSE );
	assert( fixture.workspace.report.nStageResult != 0 );
	assert( fixture.workspace.cir.program.nFunctionCount == 0 );

	setupFixture( &fixture );
	fixture.workspace.cir.nLayoutDigest = 0;
	static constexpr char szValid[] = "fn pass(value: u16) -> u16:\n"
	                                  "    return value\n";
	assert( !SIRLangCompileSource( { ( const uint8_t * )szValid, sizeof( szValid ) - 1 }, &fixture.workspace ) );
	assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_INPUT );
}

static void testBooleanShortCircuit( void ) noexcept
{
	sirLangCompilerFixture_t fixture = {};
	setupFixture( &fixture );
	static constexpr char szSource[] =
	    "fn lazy_and(flag: bool) -> bool:\n"
	    "    return flag and ((1u16 / 0u16) == 1u16)\n"
	    "fn lazy_or(flag: bool) -> bool:\n"
	    "    return flag or ((1u16 / 0u16) == 1u16)\n"
	    "fn lazy_xor(a: bool, b: bool) -> bool:\n"
	    "    return a xor b\n";

	const sirLangSourceView_t source = { ( const uint8_t * )szSource, sizeof( szSource ) - 1 };
	assert( SIRLangCompileSource( source, &fixture.workspace ) );
	const sirLangCIRProgram_t& program = fixture.workspace.cir.program;
	assert( program.nFunctionCount == 3 );
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );

	sirLangVMValue_t argument = { 0, SIR_LANG_VALUE_BOOL };
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = program.pFunctions[0].nFunctionId;
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	invocation.nFuel = 128;
	invocation.nExpectedLayoutDigest = program.nLayoutDigest;
	invocation.nExpectedBindingDigest = program.nBindingDigest;

	sirLangVMRunResult_t result = SIRLangRunGraphVM( &program, invocation );
	assert( result.eError == SIR_LANG_VM_OK && result.result.eType == SIR_LANG_VALUE_BOOL && result.result.nBits == 0 );
	argument.nBits = 1;
	assert( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_DIVIDE_ZERO );

	invocation.nFunctionId = program.pFunctions[1].nFunctionId;
	result = SIRLangRunGraphVM( &program, invocation );
	assert( result.eError == SIR_LANG_VM_OK && result.result.eType == SIR_LANG_VALUE_BOOL && result.result.nBits == 1 );
	argument.nBits = 0;
	assert( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_DIVIDE_ZERO );

	sirLangVMValue_t szXorArguments[2] = { { 0, SIR_LANG_VALUE_BOOL }, { 0, SIR_LANG_VALUE_BOOL } };
	invocation.nFunctionId = program.pFunctions[2].nFunctionId;
	invocation.pArguments = szXorArguments;
	invocation.nArgumentCount = 2;
	result = SIRLangRunGraphVM( &program, invocation );
	assert( result.eError == SIR_LANG_VM_OK && result.result.nBits == 0 );
	szXorArguments[1].nBits = 1;
	result = SIRLangRunGraphVM( &program, invocation );
	assert( result.eError == SIR_LANG_VM_OK && result.result.nBits == 1 );
	szXorArguments[0].nBits = 1;
	result = SIRLangRunGraphVM( &program, invocation );
	assert( result.eError == SIR_LANG_VM_OK && result.result.nBits == 0 );

	setupFixture( &fixture );
	static constexpr char szInvalid[] = "fn invalid(value: u16) -> bool:\n"
	                                    "    return value xor 1u16\n";
	assert( !SIRLangCompileSource( { ( const uint8_t * )szInvalid, sizeof( szInvalid ) - 1 }, &fixture.workspace ) );
	assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_SIGNATURES );
}


static void testRejectInvalidScalarExpressions( void ) noexcept
{
	static constexpr const char *szInvalid[] = {
	    "fn bad(value: bool) -> bool:\n    return value + value\n",
	    "fn bad(value: u16) -> bool:\n    return value == true\n",
	    "fn bad(value: u16) -> bool:\n    return not value\n",
	    "fn bad(value: u16) -> bool:\n    return value < true\n",
	    "fn bad() -> u16:\n    return 1u16 + 2u8\n"
	};
	for ( uint32_t iSource = 0; iSource < sizeof( szInvalid ) / sizeof( szInvalid[0] ); ++iSource ) {
		sirLangCompilerFixture_t fixture = {};
		setupFixture( &fixture );
		const char *pszSource = szInvalid[iSource];
		assert( !SIRLangCompileSource( { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) },
		                               &fixture.workspace ) );
		assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_SIGNATURES );
	}
}

int main( void )
{
	testSourceToVM();
	testBooleanShortCircuit();
	testRejectInvalidScalarExpressions();
	testRejectedSource();
	return 0;
}
