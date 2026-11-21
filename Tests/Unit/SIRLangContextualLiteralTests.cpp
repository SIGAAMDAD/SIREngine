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
#include <stdio.h>
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

static bool sirTrySource( const char *pszSource, bool bExpected, uint64_t nExpectedResult ) noexcept
{
	sirLangCompilerFixture_t fixture = {};
	setupFixture( &fixture );
	const sirLangSourceView_t view = { ( const uint8_t * )pszSource, ( uint32_t )strlen( pszSource ) };
	const bool bCompiled = SIRLangCompileSource( view, &fixture.workspace );
	if ( bCompiled != bExpected ) {
		fprintf( stderr, "unexpected compile=%d phase=%u code=%u for %s\n", ( int )bCompiled, ( unsigned )fixture.workspace.report.ePhase,
		         fixture.workspace.report.nStageResult, pszSource );
		for ( uint32_t i = 0; i < fixture.workspace.signatures.nDiagnosticCount; ++i ) {
			const auto& d = fixture.workspace.signatures.pDiagnostics[i];
			fprintf( stderr, "signature error=%u node=%u kind=%u\n", ( unsigned )d.eError, d.iNode,
			         ( unsigned )fixture.workspace.syntax.pNodes[d.iNode].eKind );
		}
		for ( uint32_t i = 0; i < fixture.workspace.semantic.nDiagnosticCount; ++i ) {
			const auto& d = fixture.workspace.semantic.pDiagnostics[i];
			fprintf( stderr, "semantic error=%u node=%u kind=%u\n", ( unsigned )d.eError, d.iNode,
			         ( unsigned )fixture.workspace.syntax.pNodes[d.iNode].eKind );
		}

		return false;
	}
	if ( !bExpected ) {
		if ( fixture.workspace.cir.program.nFunctionCount != 0 ) {
			fprintf( stderr, "rejected with partial functions=%u phase=%u code=%u\n", fixture.workspace.cir.program.nFunctionCount,
			         ( unsigned )fixture.workspace.report.ePhase, fixture.workspace.report.nStageResult );
		}
		return fixture.workspace.cir.program.nFunctionCount == 0;
	}
	const sirLangCIRProgram_t& program = fixture.workspace.cir.program;
	if ( SIRLangVerifyCIR( &program ) != SIR_LANG_VM_OK ) {
		fprintf( stderr, "invalid CIR: %s\n", pszSource );
		return false;
	}
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = program.pFunctions[program.nFunctionCount - 1].nFunctionId;
	invocation.nFuel = 100;
	invocation.nExpectedLayoutDigest = program.nLayoutDigest;
	invocation.nExpectedBindingDigest = program.nBindingDigest;
	const sirLangVMRunResult_t actual = SIRLangRunGraphVM( &program, invocation );
	if ( actual.eError != SIR_LANG_VM_OK || actual.result.nBits != nExpectedResult ) {
		fprintf( stderr, "wrong execution result error=%u bits=%llu for %s\n", ( unsigned )actual.eError,
		         ( unsigned long long )actual.result.nBits, pszSource );
		return false;
	}
	uint8_t szBytecode[32768] = {};
	uint8_t szRepeated[32768] = {};
	uint32_t nBytes = 0;
	uint32_t nRepeated = 0;
	if ( SIRLangEncodeGraphVM( &program, szBytecode, sizeof( szBytecode ), &nBytes ) != SIR_LANG_VM_OK ||
	     SIRLangEncodeGraphVM( &program, szRepeated, sizeof( szRepeated ), &nRepeated ) != SIR_LANG_VM_OK || nBytes == 0 ||
	     nBytes != nRepeated || memcmp( szBytecode, szRepeated, nBytes ) != 0 ) {
		fprintf( stderr, "nondeterministic bytecode encoding: %s\n", pszSource );
		return false;
	}
	sirLangCompilerFixture_t decoded = {};
	setupFixture( &decoded );
	if ( SIRLangDecodeGraphVM( szBytecode, nBytes, &decoded.workspace.cir ) != SIR_LANG_VM_OK ) {
		fprintf( stderr, "valid bytecode rejected: %s\n", pszSource );
		return false;
	}
	const sirLangVMRunResult_t restored = SIRLangRunGraphVM( &decoded.workspace.cir.program, invocation );
	if ( restored.eError != SIR_LANG_VM_OK || restored.result.eType != actual.result.eType || restored.result.nBits != nExpectedResult ) {
		fprintf( stderr, "cooked bytecode changed execution: %s\n", pszSource );
		return false;
	}
	szBytecode[nBytes - 1] ^= 1;
	if ( SIRLangDecodeGraphVM( szBytecode, nBytes, &decoded.workspace.cir ) == SIR_LANG_VM_OK ) {
		fprintf( stderr, "tampered bytecode accepted: %s\n", pszSource );
		return false;
	}
	return true;
}

int main( void )
{
	const struct
	{
		const char *pszSource;
		bool bShouldCompile;
		uint64_t nResult;
	} szCases[] = {
	    { "function f() returns u16 { return 100; }", true, 100 },
	    { "function f() returns u16 { u16 x = 42; switch (x) { case 42: { return 7; } default: { return 0; } } }", true, 7 },
	    { "function f() returns u16 { u16 x = 9; switch (x) { case 42: { return 7; } default: { return 0; } } }", true, 0 },
	    { "function f() returns u8 { u8 x = 255; switch (x) { case 255: { return 1; } default: { return 0; } } }", true, 1 },
	    { "function f() returns u8 { u8 x = 255; switch (x) { case 256: { return 1; } default: { return 0; } } }", false, 0 },
	    { "function f() returns u8 { u8 x = 255; switch (x) { case 1u16: { return 1; } default: { return 0; } } }", false, 0 },

	    { "function f() returns i8 { return -1; }", true, UINT64_MAX },
	    { "function f() returns i8 { return -128; }", true, UINT64_C( 0xffffffffffffff80 ) },
	    { "function f() returns i8 { return -(128); }", true, UINT64_C( 0xffffffffffffff80 ) },
	    { "function f() returns i8 { i8 x = -128; return x; }", true, UINT64_C( 0xffffffffffffff80 ) },
	    { "function f() returns bool { i8 x = -128; return x == -128; }", true, 1 },
	    { "function f() returns i8 { i8 x = -129; return x; }", false, 0 },
	    { "function f() returns i16 { return -32768; }", true, UINT64_C( 0xffffffffffff8000 ) },
	    { "function f() returns i32 { return -2147483648; }", true, UINT64_C( 0xffffffff80000000 ) },
	    { "function f() returns i64 { return -9223372036854775808; }", true, UINT64_C( 0x8000000000000000 ) },
	    { "function f() returns i8 { return -129; }", false, 0 },
	    { "function f() returns i16 { return -32769; }", false, 0 },
	    { "function f() returns i32 { return -2147483649; }", false, 0 },
	    { "function f() returns i64 { return -9223372036854775809; }", false, 0 },
	    { "function f() returns u8 { return -1; }", false, 0 },
	    { "function f() returns i8 { return +127; }", true, 127 },
	    { "function f() returns f32 { return 7.5; }", true, 0x40f00000ULL },
	    { "function f() returns f32 { return -7.5; }", true, 0xc0f00000ULL },
	    { "function f() returns f64 { f64 x = -7.5; return x; }", true, 0xc01e000000000000ULL },
	    { "function f() returns f32 { f32 x = +7.5; return x; }", true, 0x40f00000ULL },
	    { "function f() returns f64 { return -7.5; }", true, 0xc01e000000000000ULL },
	    { "function f() returns f64 { return (-7.5); }", true, 0xc01e000000000000ULL },
	    { "function f() returns f64 { return +7.5; }", true, 0x401e000000000000ULL },
	    { "function f() returns f32 { return -0.0; }", true, 0x80000000ULL },
	    { "function f() returns f64 { return 7.5; }", true, 0x401e000000000000ULL },
	    { "function f() returns f32 { f32 x = 7.5; return x; }", true, 0x40f00000ULL },
	    { "function f() returns f64 { f64 x = 7.5; return x; }", true, 0x401e000000000000ULL },
	    { "function f() returns f64 { f64 x = (7.5); return x; }", true, 0x401e000000000000ULL },
	    { "function f() returns f32 { return 7.5f32; }", true, 0x40f00000ULL },
	    { "function f() returns f64 { return 7.5f64; }", true, 0x401e000000000000ULL },
	    { "function f() returns f64 { return 7.5f32; }", false, 0 },
	    { "function f() returns f32 { return 7.5f64; }", false, 0 },
	    { "function f() returns f32 { return 1e40; }", false, 0 },
	    { "function f() returns f64 { return 1e400; }", false, 0 },
	    { "function f() returns f64 { return 0.0; }", true, 0 },
	    { "function callee(f64 x) returns f64 { return x; } function f() returns f64 { return callee(7.5); }", true,
		  0x401e000000000000ULL },

	    { "function f() returns u16 { return (100); }", true, 100 },
	    { "function f() returns u16 { u16 x = (100); return x; }", true, 100 },
	    { "function f() returns u16 { u16 x = 100; return x; }", true, 100 },
	    { "function f() returns bool { u16 x = 0; return x == 0; }", true, 1 },
	    { "function f() returns bool { u16 x = 0; return 0 == x; }", true, 1 },
	    { "function f() returns u16 { u16 x = 5; return x + 1; }", true, 6 },
	    { "function f() returns u16 { u16 x = 5; return 1 + x; }", true, 6 },
	    { "function f() returns bool { u8 x = 255; return x == 256; }", false, 0 },
	    { "function f() returns u32 { return 2147483648; }", true, 2147483648ULL },
	    { "function f() returns u64 { u64 x = 4294967296; return x; }", true, 4294967296ULL },
	    { "function f() returns u16 { return 0xffff; }", true, 65535 },
	    { "function f() returns u16 { return 0b101001; }", true, 41 },
	    { "function f() returns u8 { return 0x100; }", false, 0 },
	    { "function f() returns i32 { i32 x = 2147483648; return x; }", false, 0 },
	    { "function f() returns bool { u8 x = 255; return x == 256; }", false, 0 },
	    { "function f() returns bool { u8 x = 255; return x == 255; }", true, 1 },
	    { "function callee(i32 x) returns i32 { return x; } function f() returns i32 { return callee(2147483648); }", false, 0 },
	    { "function callee(u8 x) returns u8 { return x; } function f() returns u8 { return callee(256); }", false, 0 },
	    { "function f() returns u64 { return 18446744073709551615; }", true, UINT64_MAX },
	    { "function f() returns u8 { return 256; }", false, 0 },
	    { "function f() returns u16 { return 65536; }", false, 0 },
	    { "function f() returns u64 { return 18446744073709551616; }", false, 0 },
	    { "function f() returns i32 { return 2147483648; }", false, 0 },
	    { "function f() returns bool { return 1; }", false, 0 },
	    { "function f() returns u8 { return 1u16; }", false, 0 },
	    { "function f() returns u16 { u16 x = 1; x = 2; return x; }", true, 2 },
	    { "function callee(u8 x) returns u8 { return x; } function f() returns u8 { return callee(42); }", true, 42 },
	};
	for ( const auto& item : szCases ) {
		if ( !sirTrySource( item.pszSource, item.bShouldCompile, item.nResult ) ) {
			fprintf( stderr, "failed case: %s\n", item.pszSource );
			return 1;
		}
	}
	puts( "Sir Scroll contextual integer literals: PASS" );
	return 0;
}
