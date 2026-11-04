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

#include <Scripting/Language/SIRLangLowering.h>
#include <limits.h>
#include <string.h>

static bool sirLangIsExecutableValue( sirLangValueKind_t eType ) noexcept
{
	return eType == SIR_LANG_VALUE_BOOL || ( eType >= SIR_LANG_VALUE_I8 && eType <= SIR_LANG_VALUE_U64 );
}

static bool sirLangHIRReference( const sirLangSemanticOutput_t *pSemantic, uint32_t iNode, uint32_t *pDeclaration ) noexcept
{
	for ( uint32_t i = 0; i < pSemantic->nReferenceCount; ++i ) {
		if ( pSemantic->pReferences[i].iUseNode == iNode ) {
			*pDeclaration = pSemantic->pReferences[i].iDeclarationNode;
			return true;
		}
	}
	return false;
}

static const sirLangResolvedCall_t *sirLangHIRCall( const sirLangSignatureOutput_t *pSignatures, uint32_t iNode ) noexcept
{
	for ( uint32_t i = 0; i < pSignatures->nCallCount; ++i ) {
		if ( pSignatures->pCalls[i].iCallNode == iNode ) {
			return &pSignatures->pCalls[i];
		}
	}
	return nullptr;
}

sirLangLowerError_t SIRLangBuildHIR( const sirLangSyntaxTree_t *pTree, const sirLangSemanticOutput_t *pSemantic,
                                     const sirLangSignatureOutput_t *pSignatures, sirLangHIRProgram_t *pOutput ) noexcept
{
	if ( pTree == nullptr || pSemantic == nullptr || pSignatures == nullptr || pOutput == nullptr || pOutput->pNodes == nullptr ||
	     pTree->pNodes == nullptr || pTree->pTokens == nullptr || pTree->bTruncated || pSemantic->bTruncated || pSignatures->bTruncated ||
	     pSemantic->nDiagnosticCount != 0 || pSignatures->nDiagnosticCount != 0 || pTree->nDiagnosticCount != 0 || pTree->nNodeCount == 0 ||
	     pSignatures->pNodeTypes == nullptr || pSignatures->nNodeTypeCapacity < pTree->nNodeCount ) {
		return SIR_LANG_LOWER_INVALID_INPUT;
	}
	pOutput->nCount = 0;
	pOutput->nAggregatedEffects = 0;
	pOutput->bUsesUnsupportedEffects = false;
	if ( pOutput->nCapacity < pTree->nNodeCount ) {
		return SIR_LANG_LOWER_CAPACITY;
	}
	for ( uint32_t iNode = 0; iNode < pTree->nNodeCount; ++iNode ) {
		const sirLangSyntaxNode_t& syntax = pTree->pNodes[iNode];
		if ( syntax.nStartToken > syntax.nEndToken || syntax.nEndToken > pTree->nTokenCount ) {
			return SIR_LANG_LOWER_INVALID_INPUT;
		}
		sirLangHIRNode_t& hir = pOutput->pNodes[iNode];
		hir = {};
		hir.iSourceNode = iNode;
		hir.iDeclarationNode = SIR_LANG_INVALID_INDEX;
		hir.eSyntax = syntax.eKind;
		hir.eType = pSignatures->pNodeTypes[iNode].eValue;
		if ( syntax.eKind == SIR_LANG_SYNTAX_NAME || syntax.eKind == SIR_LANG_SYNTAX_MEMBER ) {
			sirLangHIRReference( pSemantic, iNode, &hir.iDeclarationNode );
		}
		if ( syntax.eKind == SIR_LANG_SYNTAX_CALL ) {
			const sirLangResolvedCall_t *pCall = sirLangHIRCall( pSignatures, iNode );
			if ( pCall == nullptr ) {
				return SIR_LANG_LOWER_UNRESOLVED;
			}
			hir.iDeclarationNode = pCall->iDeclarationNode;
			hir.nResolvedFunctionId = pCall->nFunctionId;
			hir.nSignatureId = pCall->nSignatureFingerprint;
		}
		const uint32_t iLhs = syntax.nFirstChild;
		const bool bWorldAssignment =
		    syntax.eKind == SIR_LANG_SYNTAX_ASSIGNMENT && iLhs < pTree->nNodeCount && pTree->pNodes[iLhs].eKind == SIR_LANG_SYNTAX_MEMBER;
		if ( syntax.eKind == SIR_LANG_SYNTAX_MUTATE || bWorldAssignment || syntax.eKind == SIR_LANG_SYNTAX_SPAWN ||
		     syntax.eKind == SIR_LANG_SYNTAX_DESTROY || syntax.eKind == SIR_LANG_SYNTAX_EMIT || syntax.eKind == SIR_LANG_SYNTAX_AFTER ) {
			hir.nEffects |= syntax.eKind == SIR_LANG_SYNTAX_EMIT || syntax.eKind == SIR_LANG_SYNTAX_AFTER ? SIR_LANG_EFFECT_EMIT
			                                                                                              : SIR_LANG_EFFECT_STATE_WRITE;
		}
		pOutput->nAggregatedEffects |= hir.nEffects;
		if ( syntax.eKind == SIR_LANG_SYNTAX_ERROR ) {
			return SIR_LANG_LOWER_INVALID_INPUT;
		}
		++pOutput->nCount;
	}
	return SIR_LANG_LOWER_OK;
}

typedef struct sirLangLowerState_s
{
	const sirLangSyntaxTree_t *pTree;
	const sirLangSignatureOutput_t *pSignatures;
	const sirLangHIRProgram_t *pHIR;
	sirLangCIRStorage_t *pOutput;
	uint32_t szCurrentValues[4096];
	uint32_t nRegisters;
	uint32_t iFunction;
	uint32_t nDepth;
	bool bInConditional;
	bool bTerminated;
} sirLangLowerState_t;

static uint32_t sirLangFirstChild( const sirLangLowerState_t *pState, uint32_t iNode ) noexcept
{
	return iNode < pState->pTree->nNodeCount ? pState->pTree->pNodes[iNode].nFirstChild : SIR_LANG_INVALID_INDEX;
}

static uint32_t sirLangNextChild( const sirLangLowerState_t *pState, uint32_t iNode ) noexcept
{
	return iNode < pState->pTree->nNodeCount ? pState->pTree->pNodes[iNode].nNextSibling : SIR_LANG_INVALID_INDEX;
}

static bool sirLangNodeText( const sirLangLowerState_t *pState, uint32_t iToken, const char *pszText ) noexcept
{
	if ( iToken >= pState->pTree->nTokenCount || pszText == nullptr ) {
		return false;
	}
	const sirLangToken_t& token = pState->pTree->pTokens[iToken];
	const size_t nBytes = strlen( pszText );
	return token.nLength == nBytes && token.nOffset <= pState->pTree->source.nBytes &&
	       token.nLength <= pState->pTree->source.nBytes - token.nOffset &&
	       memcmp( pState->pTree->source.pBytes + token.nOffset, pszText, nBytes ) == 0;
}

static sirLangLowerError_t sirLangError( sirLangLowerState_t *pState, sirLangLowerError_t eError, uint32_t iNode ) noexcept
{
	if ( pState->pOutput->eError == SIR_LANG_LOWER_OK ) {
		pState->pOutput->eError = eError;
		pState->pOutput->iErrorNode = iNode;
	}
	return eError;
}

static uint32_t sirLangEmit( sirLangLowerState_t *pState, const sirLangCIRInstruction_t& instruction ) noexcept
{
	const uint32_t iInstruction = pState->pOutput->program.nInstructionCount;
	if ( iInstruction >= pState->pOutput->nInstructionCapacity ) {
		sirLangError( pState, SIR_LANG_LOWER_CAPACITY, instruction.iSourceNode );
		return SIR_LANG_INVALID_INDEX;
	}
	pState->pOutput->pInstructions[iInstruction] = instruction;
	++pState->pOutput->program.nInstructionCount;
	return iInstruction;
}

static uint32_t sirLangNewRegister( sirLangLowerState_t *pState, uint32_t iNode ) noexcept
{
	if ( pState->nRegisters >= SIR_LANG_VM_MAX_REGISTERS ) {
		sirLangError( pState, SIR_LANG_LOWER_CAPACITY, iNode );
		return SIR_LANG_INVALID_INDEX;
	}
	return pState->nRegisters++;
}

static bool sirLangParseUnsigned( const uint8_t *pData, uint32_t nBytes, uint64_t *pValue ) noexcept
{
	if ( pData == nullptr || pValue == nullptr || nBytes == 0 ) {
		return false;
	}
	uint32_t nBase = 10;
	uint32_t i = 0;
	if ( nBytes >= 2 && pData[0] == '0' && ( pData[1] == 'x' || pData[1] == 'b' ) ) {
		nBase = pData[1] == 'x' ? 16 : 2;
		i = 2;
	}
	bool bDigits = false;
	uint64_t nValue = 0;
	for ( ; i < nBytes; ++i ) {
		const uint8_t nByte = pData[i];
		uint32_t nDigit = UINT32_MAX;
		if ( nByte >= '0' && nByte <= '9' ) {
			nDigit = nByte - '0';
		}
		if ( nByte >= 'a' && nByte <= 'f' ) {
			nDigit = nByte - 'a' + 10;
		}
		if ( nByte >= 'A' && nByte <= 'F' ) {
			nDigit = nByte - 'A' + 10;
		}
		if ( nByte == '_' ) {
			continue;
		}
		if ( nDigit >= nBase ) {
			if ( !bDigits ) {
				return false;
			}
			break;
		}
		if ( nValue > ( UINT64_MAX - nDigit ) / nBase ) {
			return false;
		}
		nValue = nValue * nBase + nDigit;
		bDigits = true;
	}
	if ( !bDigits ) {
		return false;
	}
	*pValue = nValue;
	return true;
}

static bool sirLangEqualNodeToken( const sirLangLowerState_t *pState, uint32_t iNode, const char *pszText ) noexcept
{
	return iNode < pState->pTree->nNodeCount && sirLangNodeText( pState, pState->pTree->pNodes[iNode].nStartToken, pszText );
}

static bool sirLangEnter( sirLangLowerState_t *pState, uint32_t iNode ) noexcept
{
	if ( pState->nDepth >= 128 ) {
		sirLangError( pState, SIR_LANG_LOWER_DEPTH, iNode );
		return false;
	}
	++pState->nDepth;
	return true;
}

static uint32_t sirLangLowerExpression( sirLangLowerState_t *pState, uint32_t iNode ) noexcept;
static void sirLangLowerStatement( sirLangLowerState_t *pState, uint32_t iNode ) noexcept;

static sirLangValueKind_t sirLangRegisterType( const sirLangLowerState_t *pState, uint32_t iRegister ) noexcept
{
	const sirLangCIRFunction_t& function = pState->pOutput->pFunctions[pState->iFunction];
	for ( uint32_t i = function.iFirstInstruction; i < pState->pOutput->program.nInstructionCount; ++i ) {
		const sirLangCIRInstruction_t& ins = pState->pOutput->pInstructions[i];
		if ( ins.iDestination == iRegister && ins.eOpcode != SIR_LANG_OP_RETURN && ins.eOpcode != SIR_LANG_OP_JUMP &&
		     ins.eOpcode != SIR_LANG_OP_JUMP_FALSE ) {
			return ins.eType;
		}
	}
	return SIR_LANG_VALUE_UNKNOWN;
}

static uint32_t sirLangLowerLiteral( sirLangLowerState_t *pState, uint32_t iNode ) noexcept
{
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	const sirLangToken_t& token = pState->pTree->pTokens[node.nStartToken];
	const sirLangValueKind_t eType = pState->pHIR->pNodes[iNode].eType;
	if ( !sirLangIsExecutableValue( eType ) ) {
		sirLangError( pState, SIR_LANG_LOWER_UNSUPPORTED, iNode );
		return SIR_LANG_INVALID_INDEX;
	}
	uint64_t nValue = 0;
	if ( eType == SIR_LANG_VALUE_BOOL ) {
		if ( sirLangEqualNodeToken( pState, iNode, "true" ) || sirLangEqualNodeToken( pState, iNode, "ON" ) ) {
			nValue = 1;
		} else if ( !sirLangEqualNodeToken( pState, iNode, "false" ) && !sirLangEqualNodeToken( pState, iNode, "OFF" ) ) {
			sirLangError( pState, SIR_LANG_LOWER_INVALID_LITERAL, iNode );
			return SIR_LANG_INVALID_INDEX;
		}
	} else if ( !sirLangParseUnsigned( pState->pTree->source.pBytes + token.nOffset, token.nLength, &nValue ) ) {
		sirLangError( pState, SIR_LANG_LOWER_INVALID_LITERAL, iNode );
		return SIR_LANG_INVALID_INDEX;
	}
	const uint32_t iDest = sirLangNewRegister( pState, iNode );
	if ( iDest == SIR_LANG_INVALID_INDEX ) {
		return iDest;
	}
	sirLangEmit( pState, { SIR_LANG_OP_CONST, eType, 0, iDest, 0, 0, 0, nValue, iNode } );
	return iDest;
}

static const sirLangResolvedCall_t *sirLangFindCall( const sirLangLowerState_t *pState, uint32_t iNode ) noexcept
{
	for ( uint32_t i = 0; i < pState->pSignatures->nCallCount; ++i ) {
		if ( pState->pSignatures->pCalls[i].iCallNode == iNode ) {
			return &pState->pSignatures->pCalls[i];
		}
	}
	return nullptr;
}

static uint32_t sirLangTargetIndex( const sirLangLowerState_t *pState, uint32_t iDeclaration ) noexcept
{
	for ( uint32_t i = 0; i < pState->pOutput->program.nFunctionCount; ++i ) {
		if ( pState->pSignatures->pFunctions[i].iDeclarationNode == iDeclaration ) {
			return i;
		}
	}
	return SIR_LANG_INVALID_INDEX;
}

static bool sirLangSameTokenBytes( const sirLangLowerState_t *pState, uint32_t iFirst, uint32_t iSecond ) noexcept
{
	if ( iFirst >= pState->pTree->nTokenCount || iSecond >= pState->pTree->nTokenCount ) {
		return false;
	}
	const sirLangToken_t& first = pState->pTree->pTokens[iFirst];
	const sirLangToken_t& second = pState->pTree->pTokens[iSecond];
	return first.nLength == second.nLength &&
	       memcmp( pState->pTree->source.pBytes + first.nOffset, pState->pTree->source.pBytes + second.nOffset, first.nLength ) == 0;
}

static uint32_t sirLangNamedArgumentIndex( const sirLangLowerState_t *pState, uint32_t iNameToken, uint32_t iTarget,
                                           bool bExternal ) noexcept
{
	if ( !bExternal ) {
		const sirLangFunctionSignature_t& function = pState->pSignatures->pFunctions[iTarget];
		for ( uint32_t iParam = 0; iParam < function.nParameterCount; ++iParam ) {
			const uint32_t iExpected = pState->pSignatures->pParameters[function.iFirstParameter + iParam].iNameToken;
			if ( sirLangSameTokenBytes( pState, iNameToken, iExpected ) ) {
				return iParam;
			}
		}
		return SIR_LANG_INVALID_INDEX;
	}
	const sirLangCIRCapability_t& capability = pState->pOutput->pCapabilities[iTarget];
	for ( uint32_t iExternal = 0; iExternal < pState->pSignatures->nExternalCount; ++iExternal ) {
		const sirLangExternalSignature_t& external = pState->pSignatures->pExternals[iExternal];
		if ( external.nFunctionId != capability.nFunctionId || external.nSignatureId != capability.nSignatureId ) {
			continue;
		}
		for ( uint32_t iParam = 0; iParam < external.nParameterCount; ++iParam ) {
			if ( sirLangNodeText( pState, iNameToken, external.ppszParameterNames[iParam] ) ) {
				return iParam;
			}
		}
	}
	return SIR_LANG_INVALID_INDEX;
}

static uint32_t sirLangLowerCall( sirLangLowerState_t *pState, uint32_t iNode ) noexcept
{
	const sirLangResolvedCall_t *pCall = sirLangFindCall( pState, iNode );
	if ( pCall == nullptr ) {
		sirLangError( pState, SIR_LANG_LOWER_UNRESOLVED, iNode );
		return SIR_LANG_INVALID_INDEX;
	}
	const bool bExternal = pCall->iDeclarationNode == SIR_LANG_INVALID_INDEX;
	uint32_t iTarget = SIR_LANG_INVALID_INDEX;
	if ( bExternal ) {
		for ( uint32_t i = 0; i < pState->pOutput->nCapabilityCount; ++i ) {
			const sirLangCIRCapability_t& capability = pState->pOutput->pCapabilities[i];
			if ( capability.nFunctionId == pCall->nFunctionId && capability.nSignatureId == pCall->nSignatureFingerprint ) {
				iTarget = i;
				break;
			}
		}
	} else {
		iTarget = sirLangTargetIndex( pState, pCall->iDeclarationNode );
	}
	if ( iTarget == SIR_LANG_INVALID_INDEX ) {
		sirLangError( pState, SIR_LANG_LOWER_UNRESOLVED, iNode );
		return iTarget;
	}
	const uint32_t nExpected =
	    bExternal ? pState->pOutput->pCapabilities[iTarget].nParameterCount : pState->pSignatures->pFunctions[iTarget].nParameterCount;
	if ( nExpected > SIR_LANG_VM_MAX_PARAMETERS ) {
		sirLangError( pState, SIR_LANG_LOWER_CAPACITY, iNode );
		return SIR_LANG_INVALID_INDEX;
	}
	const uint32_t iCallee = sirLangFirstChild( pState, iNode );
	uint32_t szArguments[SIR_LANG_VM_MAX_PARAMETERS] = {};
	for ( uint32_t i = 0; i < SIR_LANG_VM_MAX_PARAMETERS; ++i ) {
		szArguments[i] = SIR_LANG_INVALID_INDEX;
	}
	uint32_t nPositional = 0;
	uint32_t nActual = 0;
	for ( uint32_t iArg = sirLangNextChild( pState, iCallee ); iArg != SIR_LANG_INVALID_INDEX; iArg = sirLangNextChild( pState, iArg ) ) {
		const bool bNamed = pState->pTree->pNodes[iArg].eKind == SIR_LANG_SYNTAX_NAMED_ARGUMENT;
		const uint32_t iParam =
		    bNamed ? sirLangNamedArgumentIndex( pState, pState->pTree->pNodes[iArg].nStartToken, iTarget, bExternal ) : nPositional++;
		if ( iParam >= nExpected || szArguments[iParam] != SIR_LANG_INVALID_INDEX ) {
			sirLangError( pState, SIR_LANG_LOWER_UNRESOLVED, iArg );
			return SIR_LANG_INVALID_INDEX;
		}
		const uint32_t iValue = bNamed ? sirLangFirstChild( pState, iArg ) : iArg;
		szArguments[iParam] = sirLangLowerExpression( pState, iValue );
		if ( pState->pOutput->eError != SIR_LANG_LOWER_OK ) {
			return SIR_LANG_INVALID_INDEX;
		}
		++nActual;
	}
	if ( nActual != nExpected ) {
		sirLangError( pState, SIR_LANG_LOWER_UNRESOLVED, iNode );
		return SIR_LANG_INVALID_INDEX;
	}
	const uint32_t iCallArg = pState->pOutput->program.nCallArgumentCount;
	if ( iCallArg > pState->pOutput->nCallArgumentCapacity || nActual > pState->pOutput->nCallArgumentCapacity - iCallArg ) {
		sirLangError( pState, SIR_LANG_LOWER_CAPACITY, iNode );
		return SIR_LANG_INVALID_INDEX;
	}
	for ( uint32_t i = 0; i < nExpected; ++i ) {
		if ( szArguments[i] == SIR_LANG_INVALID_INDEX ) {
			sirLangError( pState, SIR_LANG_LOWER_UNRESOLVED, iNode );
			return SIR_LANG_INVALID_INDEX;
		}
		pState->pOutput->pCallArguments[iCallArg + i] = szArguments[i];
	}
	pState->pOutput->program.nCallArgumentCount += nExpected;
	const uint32_t iDestination = sirLangNewRegister( pState, iNode );
	if ( iDestination == SIR_LANG_INVALID_INDEX ) {
		return iDestination;
	}
	const sirLangValueKind_t eType = pState->pHIR->pNodes[iNode].eType;
	sirLangEmit( pState, { bExternal ? SIR_LANG_OP_CALL_NATIVE : SIR_LANG_OP_CALL, eType, 0, iDestination, iTarget, nExpected, iCallArg, 0,
	                       iNode } );
	return iDestination;
}

typedef struct sirLangBinaryModifiers_s
{
	bool bReverse;
	bool bNegate;
} sirLangBinaryModifiers_t;

static sirLangOpcode_t sirLangBinaryOp( const sirLangLowerState_t *pState, uint32_t iLeft, uint32_t iRight,
                                        sirLangBinaryModifiers_t *pModifiers ) noexcept
{
	const uint32_t iStart = pState->pTree->pNodes[iLeft].nEndToken;
	const uint32_t iEnd = pState->pTree->pNodes[iRight].nStartToken;
	pModifiers->bReverse = false;
	pModifiers->bNegate = false;
	for ( uint32_t iToken = iStart; iToken < iEnd; ++iToken ) {
		if ( sirLangNodeText( pState, iToken, "+" ) ) {
			return SIR_LANG_OP_ADD;
		}
		if ( sirLangNodeText( pState, iToken, "-" ) ) {
			return SIR_LANG_OP_SUBTRACT;
		}
		if ( sirLangNodeText( pState, iToken, "*" ) ) {
			return SIR_LANG_OP_MULTIPLY;
		}
		if ( sirLangNodeText( pState, iToken, "/" ) ) {
			return SIR_LANG_OP_DIVIDE;
		}
		if ( sirLangNodeText( pState, iToken, "and" ) ) {
			return SIR_LANG_OP_AND;
		}
		if ( sirLangNodeText( pState, iToken, "or" ) ) {
			return SIR_LANG_OP_OR;
		}
		if ( sirLangNodeText( pState, iToken, "=" ) ) {
			return SIR_LANG_OP_EQUAL;
		}
		if ( sirLangNodeText( pState, iToken, "!" ) ) {
			pModifiers->bNegate = true;
			return SIR_LANG_OP_EQUAL;
		}
		if ( sirLangNodeText( pState, iToken, "<" ) ) {
			return iToken + 1 < iEnd && sirLangNodeText( pState, iToken + 1, "=" ) ? SIR_LANG_OP_LESS_EQUAL : SIR_LANG_OP_LESS;
		}
		if ( sirLangNodeText( pState, iToken, ">" ) ) {
			pModifiers->bReverse = true;
			return iToken + 1 < iEnd && sirLangNodeText( pState, iToken + 1, "=" ) ? SIR_LANG_OP_LESS_EQUAL : SIR_LANG_OP_LESS;
		}
	}
	return SIR_LANG_OP_PHI;
}

// Boolean operators are control flow: the RHS may contain a native call,
// effect, or failing arithmetic operation and must never be evaluated eagerly.
static uint32_t sirLangLowerLogical( sirLangLowerState_t *pState, uint32_t iNode, uint32_t iLeft, uint32_t iRight ) noexcept
{
	const uint32_t iLeftValue = sirLangLowerExpression( pState, iLeft );
	if ( iLeftValue == SIR_LANG_INVALID_INDEX ) {
		return iLeftValue;
	}

	sirLangBinaryModifiers_t modifiers = {};
	const sirLangOpcode_t eOpcode = sirLangBinaryOp( pState, iLeft, iRight, &modifiers );
	if ( eOpcode != SIR_LANG_OP_AND && eOpcode != SIR_LANG_OP_OR ) {
		sirLangError( pState, SIR_LANG_LOWER_UNSUPPORTED, iNode );
		return SIR_LANG_INVALID_INDEX;
	}

	const uint32_t iBranch = sirLangEmit( pState, { SIR_LANG_OP_JUMP_FALSE, SIR_LANG_VALUE_VOID, 0, UINT32_MAX,
	                                               iLeftValue, 0, 0, 0, iNode } );
	if ( iBranch == SIR_LANG_INVALID_INDEX ) {
		return SIR_LANG_INVALID_INDEX;
	}

	// On a true LHS, 'and' evaluates its RHS while 'or' returns true.
	uint32_t iTrueValue = SIR_LANG_INVALID_INDEX;
	if ( eOpcode == SIR_LANG_OP_AND ) {
		iTrueValue = sirLangLowerExpression( pState, iRight );
	} else {
		iTrueValue = sirLangNewRegister( pState, iNode );
		if ( iTrueValue != SIR_LANG_INVALID_INDEX ) {
			sirLangEmit( pState, { SIR_LANG_OP_CONST, SIR_LANG_VALUE_BOOL, 0, iTrueValue, 0, 0, 0, 1, iNode } );
		}
	}
	if ( iTrueValue == SIR_LANG_INVALID_INDEX || pState->pOutput->eError != SIR_LANG_LOWER_OK ) {
		return SIR_LANG_INVALID_INDEX;
	}

	const uint32_t iTrueExit = sirLangEmit( pState, { SIR_LANG_OP_JUMP, SIR_LANG_VALUE_VOID, 0, UINT32_MAX, 0, 0, 0, 0,
	                                               iNode } );
	if ( iTrueExit == SIR_LANG_INVALID_INDEX ) {
		return SIR_LANG_INVALID_INDEX;
	}

	// The false LHS skips RHS for 'and', evaluates RHS for 'or'.
	pState->pOutput->pInstructions[iBranch].nAuxiliary = pState->pOutput->program.nInstructionCount;
	uint32_t iFalseValue = SIR_LANG_INVALID_INDEX;
	if ( eOpcode == SIR_LANG_OP_OR ) {
		iFalseValue = sirLangLowerExpression( pState, iRight );
	} else {
		iFalseValue = sirLangNewRegister( pState, iNode );
		if ( iFalseValue != SIR_LANG_INVALID_INDEX ) {
			sirLangEmit( pState, { SIR_LANG_OP_CONST, SIR_LANG_VALUE_BOOL, 0, iFalseValue, 0, 0, 0, 0, iNode } );
		}
	}
	if ( iFalseValue == SIR_LANG_INVALID_INDEX || pState->pOutput->eError != SIR_LANG_LOWER_OK ) {
		return SIR_LANG_INVALID_INDEX;
	}

	pState->pOutput->pInstructions[iTrueExit].nAuxiliary = pState->pOutput->program.nInstructionCount;
	const uint32_t iPhi = sirLangNewRegister( pState, iNode );
	if ( iPhi != SIR_LANG_INVALID_INDEX ) {
		sirLangEmit( pState, { SIR_LANG_OP_PHI, SIR_LANG_VALUE_BOOL, 0, iPhi, iTrueValue, iFalseValue,
	                       iTrueExit, 0, iNode } );
	}
	return iPhi;
}

static uint32_t sirLangLowerExpression( sirLangLowerState_t *pState, uint32_t iNode ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount || !sirLangEnter( pState, iNode ) ) {
		return SIR_LANG_INVALID_INDEX;
	}
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	uint32_t iValue = SIR_LANG_INVALID_INDEX;
	switch ( node.eKind ) {
	case SIR_LANG_SYNTAX_LITERAL:
		iValue = sirLangLowerLiteral( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_NAME: {
		const uint32_t iDecl = pState->pHIR->pNodes[iNode].iDeclarationNode;
		if ( iDecl < 4096 ) {
			iValue = pState->szCurrentValues[iDecl];
		}
		if ( iValue == SIR_LANG_INVALID_INDEX ) {
			sirLangError( pState, SIR_LANG_LOWER_UNRESOLVED, iNode );
		}
		break;
	}
	case SIR_LANG_SYNTAX_CALL:
		iValue = sirLangLowerCall( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_GROUP:
		iValue = sirLangLowerExpression( pState, sirLangFirstChild( pState, iNode ) );
		break;
	case SIR_LANG_SYNTAX_UNARY: {
		const uint32_t iOperand = sirLangLowerExpression( pState, sirLangFirstChild( pState, iNode ) );
		if ( iOperand == SIR_LANG_INVALID_INDEX ) {
			break;
		}
		const sirLangValueKind_t eType = pState->pHIR->pNodes[iNode].eType;
		if ( sirLangEqualNodeToken( pState, iNode, "+" ) ) {
			iValue = iOperand;
			break;
		}
		if ( sirLangEqualNodeToken( pState, iNode, "not" ) || sirLangEqualNodeToken( pState, iNode, "!" ) ) {
			iValue = sirLangNewRegister( pState, iNode );
			if ( iValue != SIR_LANG_INVALID_INDEX ) {
				sirLangEmit( pState, { SIR_LANG_OP_NOT, SIR_LANG_VALUE_BOOL, 0, iValue, iOperand, 0, 0, 0, iNode } );
			}
			break;
		}
		if ( sirLangEqualNodeToken( pState, iNode, "-" ) && eType >= SIR_LANG_VALUE_I8 && eType <= SIR_LANG_VALUE_U64 ) {
			const uint32_t iZero = sirLangNewRegister( pState, iNode );
			iValue = sirLangNewRegister( pState, iNode );
			if ( iValue != SIR_LANG_INVALID_INDEX && iZero != SIR_LANG_INVALID_INDEX ) {
				sirLangEmit( pState, { SIR_LANG_OP_CONST, eType, 0, iZero, 0, 0, 0, 0, iNode } );
				sirLangEmit( pState, { SIR_LANG_OP_SUBTRACT, eType, 0, iValue, iZero, iOperand, 0, 0, iNode } );
			}
			break;
		}
		sirLangError( pState, SIR_LANG_LOWER_UNSUPPORTED, iNode );
		break;
	}
	case SIR_LANG_SYNTAX_BINARY: {
		const uint32_t iLeft = sirLangFirstChild( pState, iNode );
		const uint32_t iRight = sirLangNextChild( pState, iLeft );
		if ( iRight >= pState->pTree->nNodeCount ) {
			sirLangError( pState, SIR_LANG_LOWER_INVALID_INPUT, iNode );
			break;
		}
		sirLangBinaryModifiers_t modifiers = {};
		const sirLangOpcode_t eOp = sirLangBinaryOp( pState, iLeft, iRight, &modifiers );
		if ( eOp == SIR_LANG_OP_AND || eOp == SIR_LANG_OP_OR ) {
			iValue = sirLangLowerLogical( pState, iNode, iLeft, iRight );
			break;
		}
		uint32_t iA = sirLangLowerExpression( pState, iLeft );
		uint32_t iB = sirLangLowerExpression( pState, iRight );
		if ( iA == SIR_LANG_INVALID_INDEX || iB == SIR_LANG_INVALID_INDEX ) {
			break;
		}
		if ( eOp == SIR_LANG_OP_PHI ) {
			sirLangError( pState, SIR_LANG_LOWER_UNSUPPORTED, iNode );
			break;
		}
		if ( modifiers.bReverse ) {
			const uint32_t iTemp = iA;
			iA = iB;
			iB = iTemp;
		}
		iValue = sirLangNewRegister( pState, iNode );
		if ( iValue != SIR_LANG_INVALID_INDEX ) {
			sirLangEmit( pState, { eOp, pState->pHIR->pNodes[iNode].eType, 0, iValue, iA, iB, 0, 0, iNode } );
			if ( modifiers.bNegate ) {
				const uint32_t iNot = sirLangNewRegister( pState, iNode );
				if ( iNot != SIR_LANG_INVALID_INDEX ) {
					sirLangEmit( pState, { SIR_LANG_OP_NOT, SIR_LANG_VALUE_BOOL, 0, iNot, iValue, 0, 0, 0, iNode } );
					iValue = iNot;
				}
			}
		}
		break;
	}
	default:
		sirLangError( pState, SIR_LANG_LOWER_UNSUPPORTED, iNode );
		break;
	}
	--pState->nDepth;
	return iValue;
}

static void sirLangLowerBlock( sirLangLowerState_t *pState, uint32_t iBlock ) noexcept
{
	if ( iBlock >= pState->pTree->nNodeCount || pState->pTree->pNodes[iBlock].eKind != SIR_LANG_SYNTAX_BLOCK ||
	     !sirLangEnter( pState, iBlock ) ) {
		sirLangError( pState, SIR_LANG_LOWER_INVALID_INPUT, iBlock );
		return;
	}
	for ( uint32_t iStmt = sirLangFirstChild( pState, iBlock ); iStmt != SIR_LANG_INVALID_INDEX;
	      iStmt = sirLangNextChild( pState, iStmt ) ) {
		if ( pState->bTerminated || pState->pOutput->eError != SIR_LANG_LOWER_OK ) {
			break;
		}
		sirLangLowerStatement( pState, iStmt );
	}
	--pState->nDepth;
}

static void sirLangLowerStatement( sirLangLowerState_t *pState, uint32_t iNode ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount ) {
		sirLangError( pState, SIR_LANG_LOWER_INVALID_INPUT, iNode );
		return;
	}
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	switch ( node.eKind ) {
	case SIR_LANG_SYNTAX_LET: {
		uint32_t iExpression = sirLangFirstChild( pState, iNode );
		if ( iExpression < pState->pTree->nNodeCount && pState->pTree->pNodes[iExpression].eKind == SIR_LANG_SYNTAX_TYPE ) {
			iExpression = sirLangNextChild( pState, iExpression );
		}
		if ( iExpression >= pState->pTree->nNodeCount ) {
			sirLangError( pState, SIR_LANG_LOWER_UNSUPPORTED, iNode );
			return;
		}
		const uint32_t iValue = sirLangLowerExpression( pState, iExpression );
		if ( iNode < 4096 ) {
			pState->szCurrentValues[iNode] = iValue;
		}
		return;
	}
	case SIR_LANG_SYNTAX_RETURN: {
		const uint32_t iExpression = sirLangFirstChild( pState, iNode );
		const bool bVoid = pState->pOutput->pFunctions[pState->iFunction].eReturnType == SIR_LANG_VALUE_VOID;
		const uint32_t iValue =
		    iExpression == SIR_LANG_INVALID_INDEX ? SIR_LANG_INVALID_INDEX : sirLangLowerExpression( pState, iExpression );
		if ( bVoid != ( iExpression == SIR_LANG_INVALID_INDEX ) ) {
			sirLangError( pState, SIR_LANG_LOWER_TYPE, iNode );
			return;
		}
		sirLangEmit( pState, { SIR_LANG_OP_RETURN, pState->pOutput->pFunctions[pState->iFunction].eReturnType, 0, UINT32_MAX, iValue, 0, 0,
		                       0, iNode } );
		pState->bTerminated = true;
		return;
	}
	case SIR_LANG_SYNTAX_EXPRESSION_STATEMENT:
		sirLangLowerExpression( pState, sirLangFirstChild( pState, iNode ) );
		return;
	case SIR_LANG_SYNTAX_ASSIGNMENT: {
		const uint32_t iLhs = sirLangFirstChild( pState, iNode );
		const uint32_t iRhs = sirLangNextChild( pState, iLhs );
		if ( iLhs >= pState->pTree->nNodeCount || iRhs >= pState->pTree->nNodeCount ||
		     pState->pTree->pNodes[iLhs].eKind != SIR_LANG_SYNTAX_NAME ) {
			sirLangError( pState, SIR_LANG_LOWER_UNSUPPORTED, iNode );
			return;
		}
		const uint32_t iDecl = pState->pHIR->pNodes[iLhs].iDeclarationNode;
		if ( iDecl >= 4096 || pState->szCurrentValues[iDecl] == SIR_LANG_INVALID_INDEX ) {
			sirLangError( pState, SIR_LANG_LOWER_UNRESOLVED, iNode );
			return;
		}
		pState->szCurrentValues[iDecl] = sirLangLowerExpression( pState, iRhs );
		return;
	}
	case SIR_LANG_SYNTAX_WHEN: {
		const uint32_t iCondition = sirLangFirstChild( pState, iNode );
		const uint32_t iThen = sirLangNextChild( pState, iCondition );
		const uint32_t iElse = sirLangNextChild( pState, iThen );
		const uint32_t iCondValue = sirLangLowerExpression( pState, iCondition );
		if ( iCondValue == SIR_LANG_INVALID_INDEX ) {
			return;
		}
		const uint32_t iBranch =
		    sirLangEmit( pState, { SIR_LANG_OP_JUMP_FALSE, SIR_LANG_VALUE_VOID, 0, UINT32_MAX, iCondValue, 0, 0, 0, iNode } );
		const bool bOldConditional = pState->bInConditional;
		pState->bInConditional = true;
		uint32_t szBefore[4096] = {};
		uint32_t szThen[4096] = {};
		uint32_t szElse[4096] = {};
		memcpy( szBefore, pState->szCurrentValues, sizeof( szBefore ) );
		sirLangLowerBlock( pState, iThen );
		const bool bThenReturns = pState->bTerminated;
		memcpy( szThen, pState->szCurrentValues, sizeof( szThen ) );
		uint32_t iExit = SIR_LANG_INVALID_INDEX;
		if ( !bThenReturns ) {
			iExit = sirLangEmit( pState, { SIR_LANG_OP_JUMP, SIR_LANG_VALUE_VOID, 0, UINT32_MAX, 0, 0, 0, 0, iNode } );
		}
		if ( iBranch != SIR_LANG_INVALID_INDEX ) {
			pState->pOutput->pInstructions[iBranch].nAuxiliary = pState->pOutput->program.nInstructionCount;
		}
		memcpy( pState->szCurrentValues, szBefore, sizeof( szBefore ) );
		pState->bTerminated = false;
		bool bElseReturns = false;
		if ( iElse < pState->pTree->nNodeCount && pState->pTree->pNodes[iElse].eKind == SIR_LANG_SYNTAX_OTHERWISE ) {
			sirLangLowerBlock( pState, sirLangFirstChild( pState, iElse ) );
			bElseReturns = pState->bTerminated;
		}
		memcpy( szElse, pState->szCurrentValues, sizeof( szElse ) );
		if ( iExit != SIR_LANG_INVALID_INDEX ) {
			pState->pOutput->pInstructions[iExit].nAuxiliary = pState->pOutput->program.nInstructionCount;
		}
		if ( bThenReturns && !bElseReturns ) {
			memcpy( pState->szCurrentValues, szElse, sizeof( szElse ) );
		} else if ( bElseReturns && !bThenReturns ) {
			memcpy( pState->szCurrentValues, szThen, sizeof( szThen ) );
		} else if ( !bThenReturns && !bElseReturns ) {
			for ( uint32_t iDecl = 0; iDecl < pState->pTree->nNodeCount; ++iDecl ) {
				const uint32_t iA = szThen[iDecl];
				const uint32_t iB = szElse[iDecl];
				if ( iA == iB ) {
					pState->szCurrentValues[iDecl] = iA;
					continue;
				}
				if ( szBefore[iDecl] == SIR_LANG_INVALID_INDEX || iA == SIR_LANG_INVALID_INDEX || iB == SIR_LANG_INVALID_INDEX ) {
					pState->szCurrentValues[iDecl] = SIR_LANG_INVALID_INDEX;
					continue;
				}
				const sirLangValueKind_t eType = sirLangRegisterType( pState, iA );
				if ( eType == SIR_LANG_VALUE_UNKNOWN || eType != sirLangRegisterType( pState, iB ) ) {
					sirLangError( pState, SIR_LANG_LOWER_TYPE, iNode );
					break;
				}
				const uint32_t iPhi = sirLangNewRegister( pState, iNode );
				if ( iPhi == SIR_LANG_INVALID_INDEX ) {
					break;
				}
				sirLangEmit( pState, { SIR_LANG_OP_PHI, eType, 0, iPhi, iA, iB, iExit, 0, iNode } );
				pState->szCurrentValues[iDecl] = iPhi;
			}
		}
		pState->bTerminated = bThenReturns && bElseReturns;
		pState->bInConditional = bOldConditional;
		return;
	}

	default:
		sirLangError( pState, SIR_LANG_LOWER_UNSUPPORTED, iNode );
		return;
	}
}

static uint64_t sirLangModuleFunctionId( uint64_t nModuleId, uint64_t nFingerprint ) noexcept
{
	uint64_t nHash = nModuleId ^ UINT64_C( 0x9e3779b97f4a7c15 );
	nHash ^= nFingerprint + UINT64_C( 0x9e3779b97f4a7c15 ) + ( nHash << 6 ) + ( nHash >> 2 );
	return nHash == 0 ? 1 : nHash;
}

sirLangLowerError_t SIRLangBuildCIR( const sirLangSyntaxTree_t *pTree, const sirLangSignatureOutput_t *pSignatures,
                                     const sirLangHIRProgram_t *pHIR, sirLangCIRStorage_t *pOutput ) noexcept
{
	if ( pTree == nullptr || pSignatures == nullptr || pHIR == nullptr || pOutput == nullptr || pTree->pNodes == nullptr ||
	     pOutput->pFunctions == nullptr || pOutput->pInstructions == nullptr || pOutput->pParameterTypes == nullptr ||
	     pOutput->pCallArguments == nullptr || pHIR->nCount != pTree->nNodeCount || pTree->nNodeCount > 4096 ||
	     pSignatures->nFunctionCount == 0 || pSignatures->nFunctionCount > pOutput->nFunctionCapacity || pOutput->nModuleId == 0 ||
	     pOutput->nLayoutDigest == 0 || pOutput->nBindingDigest == 0 ||
	     ( pOutput->nCapabilityCount > 0 && pOutput->pCapabilities == nullptr ) ) {
		return SIR_LANG_LOWER_INVALID_INPUT;
	}
	pOutput->eError = SIR_LANG_LOWER_OK;
	pOutput->iErrorNode = SIR_LANG_INVALID_INDEX;
	pOutput->program = {};
	pOutput->program.pFunctions = pOutput->pFunctions;
	pOutput->program.pInstructions = pOutput->pInstructions;
	pOutput->program.pParameterTypes = pOutput->pParameterTypes;
	pOutput->program.pCallArguments = pOutput->pCallArguments;
	pOutput->program.pCapabilities = pOutput->pCapabilities;
	pOutput->program.nCapabilityCount = pOutput->nCapabilityCount;
	pOutput->program.nSchemaId = pOutput->nSchemaId;
	pOutput->program.nLayoutDigest = pOutput->nLayoutDigest;
	pOutput->program.nBindingDigest = pOutput->nBindingDigest;
	pOutput->program.nRevision = SIR_LANG_VM_VERSION;
	pOutput->program.nFunctionCount = pSignatures->nFunctionCount;
	sirLangLowerState_t state = {};
	state.pTree = pTree;
	state.pSignatures = pSignatures;
	state.pHIR = pHIR;
	state.pOutput = pOutput;
	for ( uint32_t iFunction = 0; iFunction < pSignatures->nFunctionCount; ++iFunction ) {
		const sirLangFunctionSignature_t& signature = pSignatures->pFunctions[iFunction];
		if ( signature.iDeclarationNode >= pTree->nNodeCount || signature.nParameterCount > SIR_LANG_VM_MAX_PARAMETERS ||
		     signature.result.eValue == SIR_LANG_VALUE_UNKNOWN ||
		     ( signature.result.eValue != SIR_LANG_VALUE_VOID && !sirLangIsExecutableValue( signature.result.eValue ) ) ) {
			return sirLangError( &state, SIR_LANG_LOWER_UNSUPPORTED, signature.iDeclarationNode );
		}
		sirLangCIRFunction_t& function = pOutput->pFunctions[iFunction];
		function = {};
		function.nFunctionId = sirLangModuleFunctionId( pOutput->nModuleId, signature.nFingerprint );
		function.nParameterCount = signature.nParameterCount;
		function.iFirstParameterType = pOutput->program.nParameterTypeCount;
		function.eReturnType = signature.result.eValue;
		if ( signature.nParameterCount > pOutput->nParameterTypeCapacity - pOutput->program.nParameterTypeCount ) {
			return sirLangError( &state, SIR_LANG_LOWER_CAPACITY, signature.iDeclarationNode );
		}
		for ( uint32_t iParam = 0; iParam < signature.nParameterCount; ++iParam ) {
			const sirLangSignatureParameter_t& parameter = pSignatures->pParameters[signature.iFirstParameter + iParam];
			if ( !sirLangIsExecutableValue( parameter.type.eValue ) ) {
				return sirLangError( &state, SIR_LANG_LOWER_UNSUPPORTED, parameter.iDeclarationNode );
			}
			pOutput->pParameterTypes[pOutput->program.nParameterTypeCount++] = parameter.type.eValue;
		}
	}
	for ( uint32_t iFunction = 0; iFunction < pSignatures->nFunctionCount; ++iFunction ) {
		const sirLangFunctionSignature_t& signature = pSignatures->pFunctions[iFunction];
		sirLangCIRFunction_t& function = pOutput->pFunctions[iFunction];
		state.iFunction = iFunction;
		state.nRegisters = 0;
		state.bTerminated = false;
		state.bInConditional = false;
		state.nDepth = 0;
		for ( uint32_t iNode = 0; iNode < 4096; ++iNode ) {
			state.szCurrentValues[iNode] = SIR_LANG_INVALID_INDEX;
		}
		function.iFirstInstruction = pOutput->program.nInstructionCount;
		for ( uint32_t iParam = 0; iParam < signature.nParameterCount; ++iParam ) {
			const sirLangSignatureParameter_t& parameter = pSignatures->pParameters[signature.iFirstParameter + iParam];
			const uint32_t iReg = sirLangNewRegister( &state, parameter.iDeclarationNode );
			if ( iReg == SIR_LANG_INVALID_INDEX ) {
				break;
			}
			state.szCurrentValues[parameter.iDeclarationNode] = iReg;
			sirLangEmit( &state, { SIR_LANG_OP_PARAMETER, parameter.type.eValue, 0, iReg, iParam, 0, 0, 0, parameter.iDeclarationNode } );
		}
		uint32_t iBlock = SIR_LANG_INVALID_INDEX;
		for ( uint32_t iChild = sirLangFirstChild( &state, signature.iDeclarationNode ); iChild != SIR_LANG_INVALID_INDEX;
		      iChild = sirLangNextChild( &state, iChild ) ) {
			if ( pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_BLOCK ) {
				iBlock = iChild;
				break;
			}
		}
		if ( iBlock == SIR_LANG_INVALID_INDEX ) {
			return sirLangError( &state, SIR_LANG_LOWER_INVALID_INPUT, signature.iDeclarationNode );
		}
		sirLangLowerBlock( &state, iBlock );
		if ( pOutput->eError != SIR_LANG_LOWER_OK ) {
			return pOutput->eError;
		}
		if ( !state.bTerminated && function.eReturnType == SIR_LANG_VALUE_VOID ) {
			sirLangEmit( &state,
			             { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_VOID, 0, UINT32_MAX, UINT32_MAX, 0, 0, 0, signature.iDeclarationNode } );
			state.bTerminated = true;
		}
		if ( !state.bTerminated ) {
			return sirLangError( &state, SIR_LANG_LOWER_MISSING_RETURN, signature.iDeclarationNode );
		}
		function.nRegisterCount = state.nRegisters > 0 ? state.nRegisters : 1;
		function.nInstructionCount = pOutput->program.nInstructionCount - function.iFirstInstruction;
	}
	if ( SIRLangVerifyCIR( &pOutput->program ) != SIR_LANG_VM_OK ) {
		return sirLangError( &state, SIR_LANG_LOWER_UNVERIFIABLE, pSignatures->pFunctions[0].iDeclarationNode );
	}
	return SIR_LANG_LOWER_OK;
}
