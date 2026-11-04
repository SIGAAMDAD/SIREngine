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

static bool sirLangDiagnosticForNode( const sirLangSyntaxTree_t *pTree, uint32_t iNode,
                                      sirLangEditorDiagnostic_t *pDiagnostic ) noexcept
{
	if ( pTree == nullptr || iNode >= pTree->nNodeCount ) {
		return false;
	}
	sirLangSourceSpan_t span = {};
	if ( !SIRLangSyntaxSpan( pTree, iNode, &span ) ) {
		return false;
	}

	pDiagnostic->nOffset = span.nOffset;
	pDiagnostic->nLength = span.nLength;
	pDiagnostic->bHasSourceLocation = true;
	if ( pTree->pNodes == nullptr || pTree->pTokens == nullptr ) {
		return true;
	}
	const uint32_t iFirstToken = pTree->pNodes[iNode].nStartToken;
	if ( iFirstToken < pTree->nTokenCount ) {
		const sirLangToken_t& token = pTree->pTokens[iFirstToken];
		pDiagnostic->nLine = token.nLine;
		pDiagnostic->nColumn = token.nColumn;
	}
	return true;
}

bool SIRLangGetCompileDiagnostic( const sirLangCompileWorkspace_t *pWorkspace,
                                  sirLangEditorDiagnostic_t *pDiagnostic ) noexcept
{
	if ( pWorkspace == nullptr || pDiagnostic == nullptr ) {
		return false;
	}
	*pDiagnostic = {};
	pDiagnostic->ePhase = pWorkspace->report.ePhase;
	pDiagnostic->nCode = pWorkspace->report.nStageResult;
	if ( pWorkspace->report.ePhase == SIR_LANG_COMPILE_COMPLETE ) {
		return false;
	}

	const sirLangSyntaxTree_t& tree = pWorkspace->syntax;
	switch ( pWorkspace->report.ePhase ) {
	case SIR_LANG_COMPILE_PARSE:
		if ( tree.pDiagnostics != nullptr && tree.nDiagnosticCount > 0 ) {
			const sirLangParseDiagnostic_t& source = tree.pDiagnostics[0];
			pDiagnostic->nOffset = source.nOffset;
			pDiagnostic->nLength = source.nLength;
			pDiagnostic->nLine = source.nLine;
			pDiagnostic->nColumn = source.nColumn;
			pDiagnostic->bHasSourceLocation = true;
		}
		break;
	case SIR_LANG_COMPILE_SEMANTIC:
		if ( pWorkspace->semantic.pDiagnostics != nullptr && pWorkspace->semantic.nDiagnosticCount > 0 ) {
			const sirLangSemanticDiagnostic_t& source = pWorkspace->semantic.pDiagnostics[0];
			pDiagnostic->nOffset = source.nOffset;
			pDiagnostic->nLine = source.nLine;
			pDiagnostic->nColumn = source.nColumn;
			pDiagnostic->bHasSourceLocation = true;
		}
		break;
	case SIR_LANG_COMPILE_SIGNATURES:
		if ( pWorkspace->signatures.pDiagnostics != nullptr && pWorkspace->signatures.nDiagnosticCount > 0 ) {
			const sirLangSignatureDiagnostic_t& source = pWorkspace->signatures.pDiagnostics[0];
			pDiagnostic->nOffset = source.nOffset;
			pDiagnostic->nLine = source.nLine;
			pDiagnostic->nColumn = source.nColumn;
			pDiagnostic->bHasSourceLocation = true;
		}
		break;
	case SIR_LANG_COMPILE_HIR:
	case SIR_LANG_COMPILE_CIR:
	case SIR_LANG_COMPILE_EFFECTS:
		( void )sirLangDiagnosticForNode( &tree, pWorkspace->report.iErrorNode, pDiagnostic );
		break;
	case SIR_LANG_COMPILE_INPUT:
	case SIR_LANG_COMPILE_COMPLETE:
		break;
	}
	return true;
}
