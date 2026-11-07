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
	static constexpr char szSource[] = "function echo(u16 value) returns u16 {\n"
	                                   "    return value;\n}";
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

static void testFailedLoweringDiscardsProgram( void ) noexcept
{
	sirLangCompilerFixture_t fixture = {};
	setupFixture( &fixture );
	static constexpr char szSource[] = "function echo(u16 value) returns u16 { return value; }";
	const sirLangSourceView_t source = { ( const uint8_t * )szSource, sizeof( szSource ) - 1 };
	fixture.workspace.cir.nInstructionCapacity = 1;
	assert( !SIRLangCompileSource( source, &fixture.workspace ) );
	assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_CIR );
	assert( fixture.workspace.report.nStageResult == SIR_LANG_LOWER_CAPACITY );
	assert( fixture.workspace.cir.program.nFunctionCount == 0 );
	assert( fixture.workspace.cir.program.nInstructionCount == 0 );
	assert( fixture.workspace.cir.program.pFunctions == nullptr );
	assert( fixture.workspace.cir.program.pInstructions == nullptr );
	fixture.workspace.cir.nInstructionCapacity = 512;
	assert( SIRLangCompileSource( source, &fixture.workspace ) );
	assert( fixture.workspace.cir.program.nFunctionCount == 1 );
}

static void testRejectedSource( void ) noexcept
{
	sirLangCompilerFixture_t fixture = {};
	setupFixture( &fixture );
	static constexpr char szInvalid[] = "function broken(u16 value returns u16 {\n"
	                                    "    return value;\n}";
	assert( !SIRLangCompileSource( { ( const uint8_t * )szInvalid, sizeof( szInvalid ) - 1 }, &fixture.workspace ) );
	assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_PARSE );
	assert( fixture.workspace.report.nStageResult != 0 );
	assert( fixture.workspace.cir.program.nFunctionCount == 0 );

	setupFixture( &fixture );
	fixture.workspace.cir.nLayoutDigest = 0;
	static constexpr char szValid[] = "function pass(u16 value) returns u16 {\n"
	                                  "    return value;\n}";
	assert( !SIRLangCompileSource( { ( const uint8_t * )szValid, sizeof( szValid ) - 1 }, &fixture.workspace ) );
	assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_INPUT );
}

static void testBooleanShortCircuit( void ) noexcept
{
	sirLangCompilerFixture_t fixture = {};
	setupFixture( &fixture );
	static constexpr char szSource[] = "function lazy_and(bool flag) returns bool {\n"
	                                   "    return flag and ((1u16 / 0u16) == 1u16);\n}\n"
	                                   "function lazy_or(bool flag) returns bool {\n"
	                                   "    return flag or ((1u16 / 0u16) == 1u16);\n}\n"
	                                   "function lazy_xor(bool a, bool b) returns bool {\n"
	                                   "    return a xor b;\n}";

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
	static constexpr char szInvalid[] = "function invalid(u16 value) returns bool {\n"
	                                    "    return value xor 1u16;\n}";
	assert( !SIRLangCompileSource( { ( const uint8_t * )szInvalid, sizeof( szInvalid ) - 1 }, &fixture.workspace ) );
	assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_SEMANTIC );
}

static void testRejectInvalidScalarExpressions( void ) noexcept
{
	static constexpr const char *szInvalid[] = {
	    "function bad(bool value) returns bool { return value + value; }", "function bad(u16 value) returns bool { return value == true; }",
	    "function bad(u16 value) returns bool { return not value; }", "function bad(u16 value) returns bool { return value < true; }",
	    "function bad() returns u16 { return 1u16 + 2u8; }" };
	for ( uint32_t iSource = 0; iSource < sizeof( szInvalid ) / sizeof( szInvalid[0] ); ++iSource ) {
		sirLangCompilerFixture_t fixture = {};
		setupFixture( &fixture );
		const char *pszSource = szInvalid[iSource];
		assert( !SIRLangCompileSource( { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) }, &fixture.workspace ) );
		assert( fixture.workspace.report.ePhase == SIR_LANG_COMPILE_SEMANTIC );
	}
}

static sirLangVMRunResult_t executeFixture( const sirLangCompilerFixture_t *pFixture, uint64_t nFunctionId,
                                            sirLangVMValue_t argument ) noexcept
{
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = nFunctionId;
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	invocation.nFuel = 128;
	invocation.nExpectedLayoutDigest = pFixture->workspace.cir.program.nLayoutDigest;
	invocation.nExpectedBindingDigest = pFixture->workspace.cir.program.nBindingDigest;
	return SIRLangRunGraphVM( &pFixture->workspace.cir.program, invocation );
}

static void testModernSwitchAndTypedLocals( void ) noexcept
{
	static constexpr char szSource[] = "function Choose(u16 value) returns u16 { u16 original = value; "
	                                   "switch (original) { case 42u16: { return 7u16; } default: { return original; } } }";
	sirLangCompilerFixture_t fixture = {};
	setupFixture( &fixture );
	assert( SIRLangCompileSource( { ( const uint8_t * )szSource, sizeof( szSource ) - 1 }, &fixture.workspace ) );
	assert( fixture.workspace.cir.program.nFunctionCount == 1 );
	const uint64_t nFunction = fixture.workspace.cir.program.pFunctions[0].nFunctionId;
	sirLangVMRunResult_t result = executeFixture( &fixture, nFunction, { 42, SIR_LANG_VALUE_U16 } );
	assert( result.eError == SIR_LANG_VM_OK && result.result.nBits == 7 );
	result = executeFixture( &fixture, nFunction, { 12, SIR_LANG_VALUE_U16 } );
	assert( result.eError == SIR_LANG_VM_OK && result.result.nBits == 12 );
}

static void testModernEventAndSwitchBreak( void ) noexcept
{
	static constexpr char szSource[] = "on event Pulse(u16 value) returns void { "
	                                   "switch (value) { case 0u16: { break; } default: { break; } } return; }";
	sirLangCompilerFixture_t fixture = {};
	setupFixture( &fixture );
	assert( SIRLangCompileSource( { ( const uint8_t * )szSource, sizeof( szSource ) - 1 }, &fixture.workspace ) );
	assert( fixture.workspace.cir.program.nFunctionCount == 1 );
	const uint64_t nFunction = fixture.workspace.cir.program.pFunctions[0].nFunctionId;
	assert( executeFixture( &fixture, nFunction, { 0, SIR_LANG_VALUE_U16 } ).eError == SIR_LANG_VM_OK );
	assert( executeFixture( &fixture, nFunction, { 10, SIR_LANG_VALUE_U16 } ).eError == SIR_LANG_VM_OK );
}

static void testModernSourceRejections( void ) noexcept
{
	static constexpr const char *s_pszInvalid[] = {
	    "function Legacy(u16 value) -> u16 { return value; }", "function Wrong(u16 value) returns u16 { var copy = value return copy; }",
	    "on event Wrong(u16 value) { return; }", "function Wrong(u16 value) returns u16 { var copy = value; copy = 2u8; return copy; }",
	    "function Wrong(u16 value) returns u16 { switch(value) { case 1u16: { return value; } } }" };
	for ( const char *pszSource : s_pszInvalid ) {
		sirLangCompilerFixture_t fixture = {};
		setupFixture( &fixture );
		assert( !SIRLangCompileSource( { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) }, &fixture.workspace ) );
		assert( fixture.workspace.report.ePhase != SIR_LANG_COMPILE_COMPLETE );
	}
}

int main( void )
{
	testSourceToVM();
	testFailedLoweringDiscardsProgram();
	testBooleanShortCircuit();
	testRejectInvalidScalarExpressions();
	testRejectedSource();
	testModernSwitchAndTypedLocals();
	testModernEventAndSwitchBreak();
	testModernSourceRejections();
	return 0;
}
