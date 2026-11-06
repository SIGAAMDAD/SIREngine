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
#include <string.h>

static bool sirLangTypeSpelling( const sirLangSyntaxTree_t *pTree, uint32_t iTypeNode, const char *pszText ) noexcept
{
	if ( iTypeNode >= pTree->nNodeCount || pszText == nullptr ) {
		return false;
	}
	const uint32_t iToken = pTree->pNodes[iTypeNode].nStartToken;
	if ( iToken >= pTree->nTokenCount ) {
		return false;
	}
	const sirLangToken_t& token = pTree->pTokens[iToken];
	const size_t nLength = strlen( pszText );
	return nLength == token.nLength && token.nOffset <= pTree->source.nBytes && token.nLength <= pTree->source.nBytes - token.nOffset &&
	       memcmp( pTree->source.pBytes + token.nOffset, pszText, nLength ) == 0;
}

static uint32_t sirLangContainingFunction( const sirLangEffectInputs_t& inputs, uint32_t iNode ) noexcept
{
	const sirLangSyntaxNode_t& node = inputs.pTree->pNodes[iNode];
	uint32_t iMatch = SIR_LANG_INVALID_INDEX;
	uint32_t nNarrowest = UINT32_MAX;
	for ( uint32_t i = 0; i < inputs.pSignatures->nFunctionCount; ++i ) {
		const uint32_t iDeclaration = inputs.pSignatures->pFunctions[i].iDeclarationNode;
		if ( iDeclaration >= inputs.pTree->nNodeCount ) {
			continue;
		}
		const sirLangSyntaxNode_t& function = inputs.pTree->pNodes[iDeclaration];
		if ( node.nStartToken < function.nStartToken || node.nEndToken > function.nEndToken ) {
			continue;
		}
		const uint32_t nSpan = function.nEndToken - function.nStartToken;
		if ( nSpan < nNarrowest ) {
			iMatch = i;
			nNarrowest = nSpan;
		}
	}
	return iMatch;
}

static sirLangEffectError_t sirLangEffectReport( sirLangEffectOutput_t *pOutput, sirLangEffectError_t eError, uint32_t iNode ) noexcept
{
	pOutput->eError = eError;
	pOutput->iErrorNode = iNode;
	return eError;
}

static const sirLangCIRCapability_t *sirLangFindCapability( const sirLangEffectInputs_t& inputs, const sirLangHIRNode_t& hir ) noexcept
{
	for ( uint32_t i = 0; i < inputs.pProgram->nCapabilityCount; ++i ) {
		const sirLangCIRCapability_t& capability = inputs.pProgram->pCapabilities[i];
		if ( capability.nFunctionId == hir.nResolvedFunctionId && capability.nSignatureId == hir.nSignatureId ) {
			return &capability;
		}
	}
	return nullptr;
}

static uint32_t sirLangFunctionByDeclaration( const sirLangEffectInputs_t& inputs, uint32_t iDeclaration ) noexcept
{
	for ( uint32_t i = 0; i < inputs.pSignatures->nFunctionCount; ++i ) {
		if ( inputs.pSignatures->pFunctions[i].iDeclarationNode == iDeclaration ) {
			return i;
		}
	}
	return SIR_LANG_INVALID_INDEX;
}

sirLangEffectError_t SIRLangAnalyzeEffects( const sirLangEffectInputs_t& inputs, sirLangEffectOutput_t *pOutput ) noexcept
{
	if ( pOutput == nullptr || inputs.pTree == nullptr || inputs.pHIR == nullptr || inputs.pSignatures == nullptr ||
	     inputs.pProgram == nullptr || pOutput->pFunctions == nullptr || inputs.pHIR->pNodes == nullptr ||
	     inputs.pTree->pNodes == nullptr || inputs.pTree->pTokens == nullptr || inputs.pHIR->nCount != inputs.pTree->nNodeCount ||
	     inputs.pSignatures->nFunctionCount == 0 || inputs.pSignatures->nFunctionCount > pOutput->nCapacity ||
	     inputs.pSignatures->nFunctionCount != inputs.pProgram->nFunctionCount ||
	     ( inputs.pProgram->nCapabilityCount && inputs.pProgram->pCapabilities == nullptr ) ) {
		return SIR_LANG_EFFECT_INVALID_INPUT;
	}
	pOutput->nCount = inputs.pSignatures->nFunctionCount;
	pOutput->eError = SIR_LANG_EFFECT_OK;
	pOutput->iErrorNode = SIR_LANG_INVALID_INDEX;
	for ( uint32_t i = 0; i < pOutput->nCount; ++i ) {
		const sirLangFunctionSignature_t& signature = inputs.pSignatures->pFunctions[i];
		pOutput->pFunctions[i] = { signature.iDeclarationNode, 0, 0, 0, true };
		if ( sirLangTypeSpelling( inputs.pTree, signature.result.iTypeNode, "slice" ) ) {
			return sirLangEffectReport( pOutput, SIR_LANG_EFFECT_BORROW_ESCAPE, signature.iDeclarationNode );
		}
	}
	for ( uint32_t iNode = 0; iNode < inputs.pHIR->nCount; ++iNode ) {
		const sirLangHIRNode_t& hir = inputs.pHIR->pNodes[iNode];
		if ( hir.iSourceNode != iNode ) {
			return sirLangEffectReport( pOutput, SIR_LANG_EFFECT_MALFORMED_TREE, iNode );
		}
		const uint32_t iOwner = sirLangContainingFunction( inputs, iNode );
		if ( iOwner == SIR_LANG_INVALID_INDEX ) {
			continue;
		}
		sirLangEffectFunction_t& summary = pOutput->pFunctions[iOwner];
		summary.nDirectEffects |= hir.nEffects;
		if ( hir.eSyntax == SIR_LANG_SYNTAX_CALL && hir.iDeclarationNode == SIR_LANG_INVALID_INDEX ) {
			const sirLangCIRCapability_t *pCapability = sirLangFindCapability( inputs, hir );
			if ( pCapability == nullptr || pCapability->nBindingRevision == 0 ) {
				return sirLangEffectReport( pOutput, SIR_LANG_EFFECT_UNKNOWN_CAPABILITY, iNode );
			}
			if ( summary.nOwnerLane != 0 && pCapability->nOwnerLane != 0 && summary.nOwnerLane != pCapability->nOwnerLane ) {
				return sirLangEffectReport( pOutput, SIR_LANG_EFFECT_INVALID_LANE, iNode );
			}
			if ( pCapability->nOwnerLane != 0 ) {
				summary.nOwnerLane = pCapability->nOwnerLane;
			}
			summary.nDirectEffects |= pCapability->nEffects;
			if ( pCapability->nEffects & SIR_LANG_EFFECT_SUSPEND ) {
				for ( uint32_t iAncestor = 0; iAncestor < inputs.pTree->nNodeCount; ++iAncestor ) {
					const sirLangSyntaxNode_t& ancestor = inputs.pTree->pNodes[iAncestor];
					const sirLangSyntaxNode_t& callNode = inputs.pTree->pNodes[iNode];
					if ( ancestor.eKind == SIR_LANG_SYNTAX_MUTATE && ancestor.nStartToken <= callNode.nStartToken &&
					     ancestor.nEndToken >= callNode.nEndToken ) {
						return sirLangEffectReport( pOutput, SIR_LANG_EFFECT_UNSAFE_SUSPEND, iNode );
					}
				}
			}
		}
	}
	for ( uint32_t i = 0; i < pOutput->nCount; ++i ) {
		pOutput->pFunctions[i].nTransitiveEffects = pOutput->pFunctions[i].nDirectEffects;
	}
	for ( uint32_t nPass = 0; nPass < pOutput->nCount; ++nPass ) {
		bool bChanged = false;
		for ( uint32_t iNode = 0; iNode < inputs.pHIR->nCount; ++iNode ) {
			const sirLangHIRNode_t& hir = inputs.pHIR->pNodes[iNode];
			if ( hir.eSyntax != SIR_LANG_SYNTAX_CALL || hir.iDeclarationNode == SIR_LANG_INVALID_INDEX ) {
				continue;
			}
			const uint32_t iCaller = sirLangContainingFunction( inputs, iNode );
			const uint32_t iCallee = sirLangFunctionByDeclaration( inputs, hir.iDeclarationNode );
			if ( iCaller == SIR_LANG_INVALID_INDEX || iCallee == SIR_LANG_INVALID_INDEX ) {
				return sirLangEffectReport( pOutput, SIR_LANG_EFFECT_MALFORMED_TREE, iNode );
			}
			const uint32_t nPrior = pOutput->pFunctions[iCaller].nTransitiveEffects;
			pOutput->pFunctions[iCaller].nTransitiveEffects |= pOutput->pFunctions[iCallee].nTransitiveEffects;
			bChanged |= nPrior != pOutput->pFunctions[iCaller].nTransitiveEffects;
		}
		if ( !bChanged ) {
			break;
		}
	}
	for ( uint32_t i = 0; i < pOutput->nCount; ++i ) {
		sirLangEffectFunction_t& summary = pOutput->pFunctions[i];
		summary.bDeterministic = ( summary.nTransitiveEffects & SIR_LANG_EFFECT_NONDETERMINISTIC ) == 0;
		if ( pOutput->bRequireDeterministic && !summary.bDeterministic ) {
			return sirLangEffectReport( pOutput, SIR_LANG_EFFECT_NONDETERMINISTIC_CALL, summary.iDeclarationNode );
		}
	}
	return SIR_LANG_EFFECT_OK;
}
