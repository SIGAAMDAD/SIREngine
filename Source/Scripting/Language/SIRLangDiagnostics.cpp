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

static bool sirLangDiagnosticForNode( const sirLangSyntaxTree_t *pTree, uint32_t iNode, sirLangEditorDiagnostic_t *pDiagnostic ) noexcept
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

uint32_t SIRLangCollectCompileDiagnostics( const sirLangCompileWorkspace_t *pWorkspace, sirLangEditorDiagnostic_t *pDiagnostics,
                                           uint32_t nCapacity ) noexcept
{
	if ( pWorkspace == nullptr || pDiagnostics == nullptr || nCapacity == 0 || pWorkspace->report.ePhase == SIR_LANG_COMPILE_COMPLETE ) {
		return 0;
	}

	const sirLangCompilePhase_t ePhase = pWorkspace->report.ePhase;
	const sirLangSyntaxTree_t& tree = pWorkspace->syntax;
	uint32_t nWritten = 0;
	switch ( ePhase ) {
	case SIR_LANG_COMPILE_PARSE:
		if ( tree.pDiagnostics != nullptr ) {
			for ( uint32_t i = 0; i < tree.nDiagnosticCount && nWritten < nCapacity; ++i ) {
				const sirLangParseDiagnostic_t& source = tree.pDiagnostics[i];
				sirLangEditorDiagnostic_t& output = pDiagnostics[nWritten++];
				output = { ePhase, ( uint32_t )source.eError, source.nOffset, source.nLength, source.nLine, source.nColumn, true };
			}
		}
		break;
	case SIR_LANG_COMPILE_SEMANTIC:
		if ( pWorkspace->semantic.pDiagnostics != nullptr ) {
			for ( uint32_t i = 0; i < pWorkspace->semantic.nDiagnosticCount && nWritten < nCapacity; ++i ) {
				const sirLangSemanticDiagnostic_t& source = pWorkspace->semantic.pDiagnostics[i];
				sirLangEditorDiagnostic_t& output = pDiagnostics[nWritten++];
				output = { ePhase, ( uint32_t )source.eError, source.nOffset, 0, source.nLine, source.nColumn, true };
				( void )sirLangDiagnosticForNode( &tree, source.iNode, &output );
			}
		}
		break;
	case SIR_LANG_COMPILE_SIGNATURES:
		if ( pWorkspace->signatures.pDiagnostics != nullptr ) {
			for ( uint32_t i = 0; i < pWorkspace->signatures.nDiagnosticCount && nWritten < nCapacity; ++i ) {
				const sirLangSignatureDiagnostic_t& source = pWorkspace->signatures.pDiagnostics[i];
				sirLangEditorDiagnostic_t& output = pDiagnostics[nWritten++];
				output = { ePhase, ( uint32_t )source.eError, source.nOffset, 0, source.nLine, source.nColumn, true };
				( void )sirLangDiagnosticForNode( &tree, source.iNode, &output );
			}
		}
		break;
	case SIR_LANG_COMPILE_INPUT:
	case SIR_LANG_COMPILE_HIR:
	case SIR_LANG_COMPILE_CIR:
	case SIR_LANG_COMPILE_EFFECTS:
	case SIR_LANG_COMPILE_COMPLETE:
		break;
	}

	if ( nWritten == 0 ) {
		sirLangEditorDiagnostic_t& output = pDiagnostics[0];
		output = {};
		output.ePhase = ePhase;
		output.nCode = pWorkspace->report.nStageResult;
		( void )sirLangDiagnosticForNode( &tree, pWorkspace->report.iErrorNode, &output );
		return 1;
	}
	return nWritten;
}

bool SIRLangGetCompileDiagnostic( const sirLangCompileWorkspace_t *pWorkspace, sirLangEditorDiagnostic_t *pDiagnostic ) noexcept
{
	return SIRLangCollectCompileDiagnostics( pWorkspace, pDiagnostic, 1 ) == 1;
}
