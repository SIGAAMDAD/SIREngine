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

#include <Scripting/Language/SIRLangEffects.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void testTypedEffects( void ) noexcept
{
	const char szSource[] = "fn source(): mutate World: call native()";
	sirLangToken_t szTokens[] = { { SIR_LANG_TOKEN_IDENTIFIER, SIR_LANG_LEX_OK, 3, 6, 1, 4 } };
	sirLangSyntaxNode_t szNodes[] = { { SIR_LANG_SYNTAX_FUNCTION, 0, 10, UINT32_MAX, UINT32_MAX, UINT32_MAX },
	                                  { SIR_LANG_SYNTAX_MUTATE, 1, 8, UINT32_MAX, UINT32_MAX, UINT32_MAX },
	                                  { SIR_LANG_SYNTAX_CALL, 4, 7, UINT32_MAX, UINT32_MAX, UINT32_MAX } };
	sirLangSyntaxTree_t tree = {};
	tree.source = { ( const uint8_t * )szSource, ( uint32_t )strlen( szSource ) };
	tree.pNodes = szNodes;
	tree.nNodeCount = 3;
	tree.pTokens = szTokens;
	tree.nTokenCount = 1;
	sirLangHIRNode_t szHIR[] = { { 0, UINT32_MAX, 0, 0, 0, SIR_LANG_VALUE_UNKNOWN, SIR_LANG_SYNTAX_FUNCTION },
	                             { 1, UINT32_MAX, 0, 0, SIR_LANG_EFFECT_STATE_WRITE, SIR_LANG_VALUE_UNKNOWN, SIR_LANG_SYNTAX_MUTATE },
	                             { 2, UINT32_MAX, 999, 123, 0, SIR_LANG_VALUE_U16, SIR_LANG_SYNTAX_CALL } };
	sirLangHIRProgram_t hir = { szHIR, 3, 3, 0, false };
	sirLangFunctionSignature_t function = {};
	function.iDeclarationNode = 0;
	function.result.iTypeNode = UINT32_MAX;
	sirLangSignatureOutput_t signatures = {};
	signatures.pFunctions = &function;
	signatures.nFunctionCount = 1;
	sirLangCIRCapability_t capability = {
	    999, 123, 1, 2, SIR_LANG_EFFECT_SUSPEND | SIR_LANG_EFFECT_NONDETERMINISTIC, 0, 0, SIR_LANG_VALUE_U16 };
	sirLangCIRFunction_t programFunction = { 1, 0, 1, 1, 0, 0, SIR_LANG_VALUE_U16 };
	sirLangCIRProgram_t program = {};
	program.pFunctions = &programFunction;
	program.nFunctionCount = 1;
	program.pCapabilities = &capability;
	program.nCapabilityCount = 1;
	sirLangEffectInputs_t inputs = { &tree, &hir, &signatures, &program };
	sirLangEffectFunction_t szFunctions[2] = {};
	sirLangEffectOutput_t output = { szFunctions, 2, 0, UINT32_MAX, SIR_LANG_EFFECT_OK, false };
	assert( SIRLangAnalyzeEffects( inputs, &output ) == SIR_LANG_EFFECT_UNSAFE_SUSPEND );
	capability.nEffects = SIR_LANG_EFFECT_STATE_READ | SIR_LANG_EFFECT_NONDETERMINISTIC;
	assert( SIRLangAnalyzeEffects( inputs, &output ) == SIR_LANG_EFFECT_OK );
	assert( output.nCount == 1 );
	assert( output.pFunctions[0].nDirectEffects & SIR_LANG_EFFECT_STATE_READ );
	assert( output.pFunctions[0].nOwnerLane == 2 );
	output.bRequireDeterministic = true;
	assert( SIRLangAnalyzeEffects( inputs, &output ) == SIR_LANG_EFFECT_NONDETERMINISTIC_CALL );
	capability.nEffects = SIR_LANG_EFFECT_STATE_READ;
	assert( SIRLangAnalyzeEffects( inputs, &output ) == SIR_LANG_EFFECT_OK );
	assert( output.pFunctions[0].bDeterministic );
	capability.nFunctionId = 1000;
	assert( SIRLangAnalyzeEffects( inputs, &output ) == SIR_LANG_EFFECT_UNKNOWN_CAPABILITY );
}

int main( void )
{
	testTypedEffects();
	puts( "SIR-Lang effect analysis tests passed" );
	return 0;
}
