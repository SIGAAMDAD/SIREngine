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

	static constexpr char szBroken[] = "fn broken(a: u16:\n    return a\n";
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

int main( void )
{
	testCompilerSourceDiagnostics();
	return 0;
}
