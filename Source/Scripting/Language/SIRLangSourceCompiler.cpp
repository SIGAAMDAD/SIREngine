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

bool SIRLangCompileSource( const sirLangSourceView_t& source, sirLangCompileWorkspace_t *pWorkspace ) noexcept
{
	if ( pWorkspace == nullptr ) {
		return false;
	}

	pWorkspace->report = { SIR_LANG_COMPILE_INPUT, 0, SIR_LANG_INVALID_INDEX };
	if ( source.nBytes != 0 && source.pBytes == nullptr ) {
		return false;
	}
	if ( pWorkspace->cir.nModuleId == 0 || pWorkspace->cir.nLayoutDigest == 0 || pWorkspace->cir.nBindingDigest == 0 ) {
		return false;
	}

	pWorkspace->report.ePhase = SIR_LANG_COMPILE_PARSE;
	const sirLangParseError_t eParse = SIRLangParse( source, &pWorkspace->syntax );
	if ( eParse != SIR_LANG_PARSE_OK ) {
		pWorkspace->report.nStageResult = ( uint32_t )eParse;
		return false;
	}

	pWorkspace->report.ePhase = SIR_LANG_COMPILE_SEMANTIC;
	const sirLangSemanticError_t eSemantic = SIRLangAnalyze( &pWorkspace->syntax, &pWorkspace->semantic );
	if ( eSemantic != SIR_LANG_SEMANTIC_OK ) {
		pWorkspace->report.nStageResult = ( uint32_t )eSemantic;
		return false;
	}

	pWorkspace->report.ePhase = SIR_LANG_COMPILE_SIGNATURES;
	const sirLangSignatureError_t eSignatures =
	    SIRLangResolveSignatures( &pWorkspace->syntax, &pWorkspace->semantic, &pWorkspace->signatures );
	if ( eSignatures != SIR_LANG_SIGNATURE_OK ) {
		pWorkspace->report.nStageResult = ( uint32_t )eSignatures;
		return false;
	}

	pWorkspace->report.ePhase = SIR_LANG_COMPILE_HIR;
	const sirLangLowerError_t eHIR =
	    SIRLangBuildHIR( &pWorkspace->syntax, &pWorkspace->semantic, &pWorkspace->signatures, &pWorkspace->hir );
	if ( eHIR != SIR_LANG_LOWER_OK ) {
		pWorkspace->report.nStageResult = ( uint32_t )eHIR;
		return false;
	}

	pWorkspace->report.ePhase = SIR_LANG_COMPILE_CIR;
	const sirLangLowerError_t eCIR =
	    SIRLangBuildCIR( &pWorkspace->syntax, &pWorkspace->signatures, &pWorkspace->hir, &pWorkspace->cir );
	if ( eCIR != SIR_LANG_LOWER_OK ) {
		pWorkspace->report.nStageResult = ( uint32_t )eCIR;
		pWorkspace->report.iErrorNode = pWorkspace->cir.iErrorNode;
		return false;
	}

	pWorkspace->report.ePhase = SIR_LANG_COMPILE_EFFECTS;
	const sirLangEffectInputs_t inputs = { &pWorkspace->syntax, &pWorkspace->hir, &pWorkspace->signatures,
	                                     &pWorkspace->cir.program };
	const sirLangEffectError_t eEffects = SIRLangAnalyzeEffects( inputs, &pWorkspace->effects );
	if ( eEffects != SIR_LANG_EFFECT_OK ) {
		pWorkspace->report.nStageResult = ( uint32_t )eEffects;
		pWorkspace->report.iErrorNode = pWorkspace->effects.iErrorNode;
		return false;
	}

	pWorkspace->report.ePhase = SIR_LANG_COMPILE_COMPLETE;
	return true;
}
