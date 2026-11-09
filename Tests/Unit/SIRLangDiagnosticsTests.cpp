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

#include <Scripting/Language/SIRLangDiagnostics.h>

#include <assert.h>
#include <string.h>

static void testCompilerSourceDiagnostics( void ) noexcept
{
	sirLangToken_t szTokens[64] = {};
	sirLangSyntaxNode_t szNodes[64] = {};
	sirLangParseDiagnostic_t szParse[8] = {};
	sirLangCompileWorkspace_t workspace = {};
	workspace.syntax.pTokens = szTokens;
	workspace.syntax.nTokenCapacity = 64;
	workspace.syntax.pNodes = szNodes;
	workspace.syntax.nNodeCapacity = 64;
	workspace.syntax.pDiagnostics = szParse;
	workspace.syntax.nDiagnosticCapacity = 8;
	workspace.cir.nModuleId = 1;
	workspace.cir.nLayoutDigest = 2;
	workspace.cir.nBindingDigest = 3;

	static constexpr char szBroken[] = "function broken(u16 a returns u16 { return a; }";
	const sirLangSourceView_t source = { ( const uint8_t * )szBroken, sizeof( szBroken ) - 1 };
	assert( !SIRLangCompileSource( source, &workspace ) );
	assert( workspace.report.ePhase == SIR_LANG_COMPILE_PARSE );

	sirLangEditorDiagnostic_t diagnostic = {};
	assert( SIRLangGetCompileDiagnostic( &workspace, &diagnostic ) );
	assert( diagnostic.ePhase == SIR_LANG_COMPILE_PARSE );
	assert( diagnostic.nCode != 0 );
	assert( diagnostic.bHasSourceLocation );
	assert( diagnostic.nOffset < sizeof( szBroken ) );

	workspace.report = { SIR_LANG_COMPILE_COMPLETE, 0, UINT32_MAX };
	assert( !SIRLangGetCompileDiagnostic( &workspace, &diagnostic ) );
	workspace.report = { SIR_LANG_COMPILE_INPUT, 1, UINT32_MAX };
	assert( SIRLangGetCompileDiagnostic( &workspace, &diagnostic ) );
	assert( !diagnostic.bHasSourceLocation );
	assert( !SIRLangGetCompileDiagnostic( nullptr, &diagnostic ) );
	assert( !SIRLangGetCompileDiagnostic( &workspace, nullptr ) );
}

static void testRecoveredDiagnostics( void ) noexcept
{
	sirLangToken_t szTokens[256] = {};
	sirLangSyntaxNode_t szNodes[256] = {};
	sirLangParseDiagnostic_t szParse[16] = {};
	sirLangCompileWorkspace_t workspace = {};
	workspace.syntax.pTokens = szTokens;
	workspace.syntax.nTokenCapacity = 256;
	workspace.syntax.pNodes = szNodes;
	workspace.syntax.nNodeCapacity = 256;
	workspace.syntax.pDiagnostics = szParse;
	workspace.syntax.nDiagnosticCapacity = 16;
	workspace.cir.nModuleId = 1;
	workspace.cir.nLayoutDigest = 2;
	workspace.cir.nBindingDigest = 3;

	static constexpr char szBroken[] = "function broken(u16 a returns u16 {\n"
	                                   "    var missing = ;\n"
	                                   "}\n"
	                                   "function good(i32 x) returns i32 { return x; }\n";
	const sirLangSourceView_t source = { ( const uint8_t * )szBroken, sizeof( szBroken ) - 1 };
	assert( !SIRLangCompileSource( source, &workspace ) );
	assert( workspace.report.ePhase == SIR_LANG_COMPILE_PARSE );
	assert( workspace.syntax.nDiagnosticCount >= 2 );

	sirLangEditorDiagnostic_t szCollected[4] = {};
	const uint32_t nCollected = SIRLangCollectCompileDiagnostics( &workspace, szCollected, 4 );
	assert( nCollected >= 2 && nCollected <= 4 );
	assert( szCollected[0].ePhase == SIR_LANG_COMPILE_PARSE );
	assert( szCollected[1].ePhase == SIR_LANG_COMPILE_PARSE );
	assert( szCollected[0].bHasSourceLocation && szCollected[1].bHasSourceLocation );
	assert( szCollected[0].nOffset <= szCollected[1].nOffset );
	assert( szCollected[0].nCode != 0 && szCollected[1].nCode != 0 );

	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 1 ) == 1 );
	assert( SIRLangCollectCompileDiagnostics( &workspace, nullptr, 4 ) == 0 );
	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 0 ) == 0 );
	assert( SIRLangCollectCompileDiagnostics( nullptr, szCollected, 4 ) == 0 );
}

static void testStageDiagnosticProjection( void ) noexcept
{
	static constexpr char szSource[] = "0123456789abcdef";
	sirLangToken_t szTokens[2] = {};
	szTokens[1].nOffset = 7;
	szTokens[1].nLength = 4;
	szTokens[1].nLine = 3;
	szTokens[1].nColumn = 2;
	sirLangSyntaxNode_t szNodes[2] = {};
	szNodes[1].nStartToken = 1;
	szNodes[1].nEndToken = 2;
	sirLangSemanticDiagnostic_t szSemantic[2] = {};
	szSemantic[0] = { SIR_LANG_SEMANTIC_UNKNOWN_SYMBOL, 1, 8, 9, 10 };
	szSemantic[1] = { SIR_LANG_SEMANTIC_UNKNOWN_TYPE, 9, 12, 11, 12 };
	sirLangSignatureDiagnostic_t szSignatures[2] = {};
	szSignatures[0] = { SIR_LANG_SIGNATURE_UNTYPED_CALLEE, 1, 8, 9, 10 };
	szSignatures[1] = { SIR_LANG_SIGNATURE_RETURN_TYPE, 9, 12, 11, 12 };

	sirLangCompileWorkspace_t workspace = {};
	workspace.syntax.source = { ( const uint8_t * )szSource, sizeof( szSource ) - 1 };
	workspace.syntax.pTokens = szTokens;
	workspace.syntax.nTokenCount = 2;
	workspace.syntax.pNodes = szNodes;
	workspace.syntax.nNodeCount = 2;
	workspace.syntax.nRoot = 0;
	workspace.semantic.pDiagnostics = szSemantic;
	workspace.semantic.nDiagnosticCount = 2;
	workspace.signatures.pDiagnostics = szSignatures;
	workspace.signatures.nDiagnosticCount = 2;
	sirLangEditorDiagnostic_t szCollected[3] = {};

	workspace.report = { SIR_LANG_COMPILE_SEMANTIC, SIR_LANG_SEMANTIC_UNKNOWN_SYMBOL, 1 };
	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 3 ) == 2 );
	assert( szCollected[0].nCode == SIR_LANG_SEMANTIC_UNKNOWN_SYMBOL );
	assert( szCollected[0].nOffset == 7 && szCollected[0].nLength == 4 );
	assert( szCollected[0].nLine == 3 && szCollected[0].nColumn == 2 );
	assert( szCollected[0].bHasSourceLocation );
	assert( szCollected[1].nCode == SIR_LANG_SEMANTIC_UNKNOWN_TYPE );
	assert( szCollected[1].nOffset == 12 && szCollected[1].nLine == 11 );
	assert( szCollected[1].bHasSourceLocation );
	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 1 ) == 1 );

	workspace.report = { SIR_LANG_COMPILE_SIGNATURES, SIR_LANG_SIGNATURE_RETURN_TYPE, 9 };
	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 3 ) == 2 );
	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 1 ) == 1 );
	assert( szCollected[0].nCode == SIR_LANG_SIGNATURE_UNTYPED_CALLEE );
	assert( szCollected[0].nOffset == 7 && szCollected[0].nLength == 4 );
	assert( szCollected[1].nCode == SIR_LANG_SIGNATURE_RETURN_TYPE );
	assert( szCollected[1].nOffset == 12 && szCollected[1].nLine == 11 );

	// A missing parse-diagnostic buffer must fall back to the stage error.
	workspace.report = { SIR_LANG_COMPILE_PARSE, SIR_LANG_PARSE_CAPACITY, 9 };
	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 3 ) == 1 );
	assert( szCollected[0].nCode == SIR_LANG_PARSE_CAPACITY );
	assert( !szCollected[0].bHasSourceLocation );

	// An empty stage diagnostic array produces exactly one stage-level error.
	workspace.semantic.pDiagnostics = nullptr;
	workspace.report = { SIR_LANG_COMPILE_SEMANTIC, 97, 1 };
	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 3 ) == 1 );
	assert( szCollected[0].nCode == 97 && szCollected[0].nOffset == 7 );
	workspace.signatures.pDiagnostics = nullptr;
	workspace.report = { SIR_LANG_COMPILE_SIGNATURES, 98, 9 };
	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 3 ) == 1 );
	assert( szCollected[0].nCode == 98 && !szCollected[0].bHasSourceLocation );

	// Report-only failure phases must not read diagnostic arrays.
	const sirLangCompilePhase_t szPhases[] = { SIR_LANG_COMPILE_INPUT, SIR_LANG_COMPILE_HIR, SIR_LANG_COMPILE_CIR,
	                                           SIR_LANG_COMPILE_EFFECTS };
	for ( sirLangCompilePhase_t ePhase : szPhases ) {
		workspace.report = { ePhase, 99, 9 };
		assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 3 ) == 1 );
		assert( szCollected[0].ePhase == ePhase && szCollected[0].nCode == 99 );
		assert( !szCollected[0].bHasSourceLocation );
	}
	workspace.syntax.nTokenCount = 0;
	workspace.report = { SIR_LANG_COMPILE_CIR, 100, 1 };
	assert( SIRLangCollectCompileDiagnostics( &workspace, szCollected, 3 ) == 1 );
	assert( !szCollected[0].bHasSourceLocation );
}

int main( void )
{
	testCompilerSourceDiagnostics();
	testRecoveredDiagnostics();
	testStageDiagnosticProjection();
	return 0;
}
