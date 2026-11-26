/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
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

	pWorkspace->report = { SIRLangCompilePhase::SIR_LANG_COMPILE_INPUT, 0, SIR_LANG_INVALID_INDEX };
	pWorkspace->cir.program = {};
	pWorkspace->hir.nCount = 0;
	pWorkspace->effects.nCount = 0;
	if ( source.nBytes != 0 && source.pBytes == nullptr ) {
		return false;
	}
	if ( pWorkspace->cir.nModuleId == 0 || pWorkspace->cir.nLayoutDigest == 0 || pWorkspace->cir.nBindingDigest == 0 ) {
		return false;
	}

	pWorkspace->report.ePhase = SIRLangCompilePhase::SIR_LANG_COMPILE_PARSE;
	const SIRLangParseError eParse = SIRLangParse( source, &pWorkspace->syntax );
	if ( eParse != SIRLangParseError::SIR_LANG_PARSE_OK ) {
		pWorkspace->report.nStageResult = static_cast<uint32_t>(eParse);
		return false;
	}

	pWorkspace->report.ePhase = SIRLangCompilePhase::SIR_LANG_COMPILE_SEMANTIC;
	const SIRLangSemanticError eSemantic = SIRLangAnalyze( &pWorkspace->syntax, &pWorkspace->semantic );
	if ( eSemantic != SIRLangSemanticError::SIR_LANG_SEMANTIC_OK ) {
		pWorkspace->report.nStageResult = static_cast<uint32_t>(eSemantic);
		return false;
	}

	pWorkspace->report.ePhase = SIRLangCompilePhase::SIR_LANG_COMPILE_SIGNATURES;
	const SIRLangSignatureError eSignatures =
	    SIRLangResolveSignatures( &pWorkspace->syntax, &pWorkspace->semantic, &pWorkspace->signatures );
	if ( eSignatures != SIRLangSignatureError::SIR_LANG_SIGNATURE_OK ) {
		pWorkspace->report.nStageResult = static_cast<uint32_t>(eSignatures);
		return false;
	}

	pWorkspace->report.ePhase = SIRLangCompilePhase::SIR_LANG_COMPILE_HIR;
	const SIRLangLowerError eHIR =
	    SIRLangBuildHIR( &pWorkspace->syntax, &pWorkspace->semantic, &pWorkspace->signatures, &pWorkspace->hir );
	if ( eHIR != SIRLangLowerError::SIR_LANG_LOWER_OK ) {
		pWorkspace->report.nStageResult = static_cast<uint32_t>(eHIR);
		return false;
	}

	pWorkspace->report.ePhase = SIRLangCompilePhase::SIR_LANG_COMPILE_CIR;
	const SIRLangLowerError eCIR = SIRLangBuildCIR( &pWorkspace->syntax, &pWorkspace->signatures, &pWorkspace->hir, &pWorkspace->cir );
	if ( eCIR != SIRLangLowerError::SIR_LANG_LOWER_OK ) {
		pWorkspace->report.nStageResult = static_cast<uint32_t>(eCIR);
		pWorkspace->report.iErrorNode = pWorkspace->cir.iErrorNode;
		pWorkspace->cir.program = {};
		return false;
	}

	pWorkspace->report.ePhase = SIRLangCompilePhase::SIR_LANG_COMPILE_EFFECTS;
	const sirLangEffectInputs_t inputs = { &pWorkspace->syntax, &pWorkspace->hir, &pWorkspace->signatures, &pWorkspace->cir.program };
	const SIRLangEffectError eEffects = SIRLangAnalyzeEffects( inputs, &pWorkspace->effects );
	if ( eEffects != SIRLangEffectError::SIR_LANG_EFFECT_OK ) {
		pWorkspace->cir.program = {};
		pWorkspace->report.nStageResult = static_cast<uint32_t>(eEffects);
		pWorkspace->report.iErrorNode = pWorkspace->effects.iErrorNode;
		return false;
	}

	pWorkspace->report.ePhase = SIRLangCompilePhase::SIR_LANG_COMPILE_COMPLETE;
	return true;
}
