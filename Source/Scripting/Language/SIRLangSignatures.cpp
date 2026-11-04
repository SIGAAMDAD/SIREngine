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

#include <Scripting/Language/SIRLangSignatures.h>
#include <string.h>

typedef struct sirLangSignatureState_s
{
	const sirLangSyntaxTree_t *pTree;
	const sirLangSemanticOutput_t *pSemantic;
	sirLangSignatureOutput_t *pOutput;
	sirLangSignatureError_t eFirstError;
	uint32_t nDepth;
} sirLangSignatureState_t;

static sirLangTypeIdentity_t sirLangUnknownType( void ) noexcept
{
	return { 0, SIR_LANG_INVALID_INDEX, SIR_LANG_VALUE_UNKNOWN };
}

static sirLangTypeIdentity_t sirLangScalarType( sirLangValueKind_t eValue ) noexcept
{
	return { ( uint64_t )eValue + 1U, SIR_LANG_INVALID_INDEX, eValue };
}

static uint32_t sirLangNextNode( const sirLangSignatureState_t *pState, uint32_t iNode ) noexcept
{
	return iNode < pState->pTree->nNodeCount ? pState->pTree->pNodes[iNode].nNextSibling : SIR_LANG_INVALID_INDEX;
}

static uint32_t sirLangFirstNode( const sirLangSignatureState_t *pState, uint32_t iNode ) noexcept
{
	return iNode < pState->pTree->nNodeCount ? pState->pTree->pNodes[iNode].nFirstChild : SIR_LANG_INVALID_INDEX;
}

static bool sirLangTextIs( const sirLangSignatureState_t *pState, uint32_t iToken, const char *pszText ) noexcept
{
	if ( pszText == nullptr || iToken >= pState->pTree->nTokenCount ) {
		return false;
	}
	const sirLangToken_t& token = pState->pTree->pTokens[iToken];
	const size_t nLength = strlen( pszText );
	return token.nLength == nLength && memcmp( pState->pTree->source.pBytes + token.nOffset, pszText, nLength ) == 0;
}

static bool sirLangTokensEqual( const sirLangSignatureState_t *pState, uint32_t iLeft, uint32_t iRight ) noexcept
{
	if ( iLeft >= pState->pTree->nTokenCount || iRight >= pState->pTree->nTokenCount ) {
		return false;
	}
	const sirLangToken_t& left = pState->pTree->pTokens[iLeft];
	const sirLangToken_t& right = pState->pTree->pTokens[iRight];
	return left.nLength == right.nLength &&
	       memcmp( pState->pTree->source.pBytes + left.nOffset, pState->pTree->source.pBytes + right.nOffset, left.nLength ) == 0;
}

static uint64_t sirLangMix( uint64_t nHash, uint8_t iByte ) noexcept
{
	return ( nHash ^ iByte ) * UINT64_C( 1099511628211 );
}

static uint64_t sirLangHashType( const sirLangSignatureState_t *pState, uint32_t iNode ) noexcept
{
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( uint32_t iToken = node.nStartToken; iToken < node.nEndToken; ++iToken ) {
		const sirLangToken_t& token = pState->pTree->pTokens[iToken];
		if ( token.eKind == SIR_LANG_TOKEN_NEWLINE ) {
			continue;
		}
		nHash = sirLangMix( nHash, 0xff );
		const uint8_t *pData = pState->pTree->source.pBytes + token.nOffset;
		for ( uint32_t iByte = 0; iByte < token.nLength; ++iByte ) {
			nHash = sirLangMix( nHash, pData[iByte] );
		}
	}
	return nHash;
}

static sirLangTypeIdentity_t sirLangTypeNode( const sirLangSignatureState_t *pState, uint32_t iNode ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount || pState->pTree->pNodes[iNode].eKind != SIR_LANG_SYNTAX_TYPE ) {
		return sirLangUnknownType();
	}
	const uint32_t iToken = pState->pTree->pNodes[iNode].nStartToken;
	for ( uint32_t iSymbol = 0; iSymbol < pState->pSemantic->nSymbolCount; ++iSymbol ) {
		const sirLangSemanticSymbol_t& symbol = pState->pSemantic->pSymbols[iSymbol];
		if ( symbol.iTypeNode == iNode ) {
			if ( symbol.eValue == SIR_LANG_VALUE_OTHER ) {
				return { sirLangHashType( pState, iNode ), iNode, SIR_LANG_VALUE_OTHER };
			}
			return sirLangScalarType( symbol.eValue );
		}
	}
	static constexpr struct
	{
		const char *pszText;
		sirLangValueKind_t eValue;
	} s_szTypes[] = { { "void", SIR_LANG_VALUE_VOID }, { "bool", SIR_LANG_VALUE_BOOL }, { "i8", SIR_LANG_VALUE_I8 },
	                  { "u8", SIR_LANG_VALUE_U8 },     { "i16", SIR_LANG_VALUE_I16 },   { "u16", SIR_LANG_VALUE_U16 },
	                  { "i32", SIR_LANG_VALUE_I32 },   { "u32", SIR_LANG_VALUE_U32 },   { "i64", SIR_LANG_VALUE_I64 },
	                  { "u64", SIR_LANG_VALUE_U64 },   { "f16", SIR_LANG_VALUE_F16 },   { "f32", SIR_LANG_VALUE_F32 },
	                  { "f64", SIR_LANG_VALUE_F64 },   { "atom", SIR_LANG_VALUE_ATOM } };
	for ( uint32_t iItem = 0; iItem < sizeof( s_szTypes ) / sizeof( s_szTypes[0] ); ++iItem ) {
		if ( sirLangTextIs( pState, iToken, s_szTypes[iItem].pszText ) ) {
			return sirLangScalarType( s_szTypes[iItem].eValue );
		}
	}
	return { sirLangHashType( pState, iNode ), iNode, SIR_LANG_VALUE_OTHER };
}

static bool sirLangTypeEqual( const sirLangSignatureState_t *pState, sirLangTypeIdentity_t left, sirLangTypeIdentity_t right ) noexcept
{
	if ( left.eValue == SIR_LANG_VALUE_UNKNOWN || right.eValue == SIR_LANG_VALUE_UNKNOWN ) {
		return false;
	}
	if ( left.eValue != right.eValue || left.nFingerprint != right.nFingerprint ) {
		return false;
	}
	if ( left.eValue != SIR_LANG_VALUE_OTHER ) {
		return true;
	}
	if ( left.iTypeNode == right.iTypeNode ) {
		return true;
	}
	if ( left.iTypeNode >= pState->pTree->nNodeCount || right.iTypeNode >= pState->pTree->nNodeCount ) {
		return false;
	}
	const sirLangSyntaxNode_t& a = pState->pTree->pNodes[left.iTypeNode];
	const sirLangSyntaxNode_t& b = pState->pTree->pNodes[right.iTypeNode];
	if ( a.nEndToken - a.nStartToken != b.nEndToken - b.nStartToken ) {
		return false;
	}
	for ( uint32_t iToken = 0; iToken < a.nEndToken - a.nStartToken; ++iToken ) {
		if ( !sirLangTokensEqual( pState, a.nStartToken + iToken, b.nStartToken + iToken ) ) {
			return false;
		}
	}
	return true;
}

static void sirLangSignatureReport( sirLangSignatureState_t *pState, sirLangSignatureError_t eError, uint32_t iNode ) noexcept
{
	if ( pState->eFirstError == SIR_LANG_SIGNATURE_OK ) {
		pState->eFirstError = eError;
	}
	if ( pState->pOutput->nDiagnosticCount >= pState->pOutput->nDiagnosticCapacity ) {
		pState->pOutput->bTruncated = true;
		pState->eFirstError = SIR_LANG_SIGNATURE_CAPACITY;
		return;
	}
	uint32_t iToken = 0;
	if ( iNode < pState->pTree->nNodeCount ) {
		iToken = pState->pTree->pNodes[iNode].nStartToken;
	}
	const sirLangToken_t& token = pState->pTree->pTokens[iToken];
	pState->pOutput->pDiagnostics[pState->pOutput->nDiagnosticCount++] = { eError, iNode, token.nOffset, token.nLine, token.nColumn };
}

static const sirLangSemanticSymbol_t *sirLangSymbolAt( const sirLangSignatureState_t *pState, uint32_t iNode ) noexcept
{
	for ( uint32_t iSymbol = 0; iSymbol < pState->pSemantic->nSymbolCount; ++iSymbol ) {
		const sirLangSemanticSymbol_t& symbol = pState->pSemantic->pSymbols[iSymbol];
		if ( symbol.iDeclarationNode == iNode ) {
			return &symbol;
		}
	}
	return nullptr;
}

static const sirLangResolvedReference_t *sirLangReferenceAt( const sirLangSignatureState_t *pState, uint32_t iNode ) noexcept
{
	for ( uint32_t iRef = 0; iRef < pState->pSemantic->nReferenceCount; ++iRef ) {
		if ( pState->pSemantic->pReferences[iRef].iUseNode == iNode ) {
			return &pState->pSemantic->pReferences[iRef];
		}
	}
	return nullptr;
}

static bool sirLangRegisterSignature( sirLangSignatureState_t *pState, const sirLangSemanticSymbol_t& symbol ) noexcept
{
	if ( pState->pOutput->nFunctionCount == pState->pOutput->nFunctionCapacity ) {
		pState->pOutput->bTruncated = true;
		pState->eFirstError = SIR_LANG_SIGNATURE_CAPACITY;
		return false;
	}
	const uint32_t iFunction = symbol.iDeclarationNode;
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iFunction];
	sirLangFunctionSignature_t signature = {};
	signature.iDeclarationNode = iFunction;
	signature.iNameToken = symbol.iNameToken;
	signature.iFirstParameter = pState->pOutput->nParameterCount;
	signature.result = sirLangScalarType( SIR_LANG_VALUE_VOID );
	for ( uint32_t iChild = node.nFirstChild; iChild != SIR_LANG_INVALID_INDEX; iChild = sirLangNextNode( pState, iChild ) ) {
		const sirLangSyntaxNode_t& child = pState->pTree->pNodes[iChild];
		if ( child.eKind == SIR_LANG_SYNTAX_TYPE ) {
			signature.result = sirLangTypeNode( pState, iChild );
			continue;
		}
		if ( child.eKind != SIR_LANG_SYNTAX_PARAMETER ) {
			continue;
		}
		if ( signature.nParameterCount >= 32 ) {
			sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_INVALID_SIGNATURE, iFunction );
			return false;
		}
		const sirLangSemanticSymbol_t *pSymbol = sirLangSymbolAt( pState, iChild );
		if ( pSymbol == nullptr || pSymbol->iTypeNode == SIR_LANG_INVALID_INDEX ||
		     pState->pOutput->nParameterCount >= pState->pOutput->nParameterCapacity ) {
			pState->pOutput->bTruncated = pState->pOutput->nParameterCount >= pState->pOutput->nParameterCapacity;
			sirLangSignatureReport(
			    pState, pState->pOutput->bTruncated ? SIR_LANG_SIGNATURE_CAPACITY : SIR_LANG_SIGNATURE_INVALID_SIGNATURE, iChild );
			return false;
		}
		sirLangSignatureParameter_t& parameter = pState->pOutput->pParameters[pState->pOutput->nParameterCount++];
		parameter = { iChild, pSymbol->iNameToken, sirLangTypeNode( pState, pSymbol->iTypeNode ) };
		if ( parameter.type.eValue == SIR_LANG_VALUE_UNKNOWN ) {
			sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_INVALID_SIGNATURE, iChild );
			return false;
		}
		++signature.nParameterCount;
	}
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( uint32_t iToken = 0; iToken < pState->pTree->pTokens[signature.iNameToken].nLength; ++iToken ) {
		const sirLangToken_t& name = pState->pTree->pTokens[signature.iNameToken];
		nHash = sirLangMix( nHash, pState->pTree->source.pBytes[name.nOffset + iToken] );
	}
	for ( uint32_t iParam = 0; iParam < signature.nParameterCount; ++iParam ) {
		const sirLangSignatureParameter_t& param = pState->pOutput->pParameters[signature.iFirstParameter + iParam];
		const uint64_t nType = param.type.nFingerprint;
		for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
			nHash = sirLangMix( nHash, ( uint8_t )( nType >> ( iByte * 8 ) ) );
		}
		const sirLangToken_t& name = pState->pTree->pTokens[param.iNameToken];
		for ( uint32_t iByte = 0; iByte < name.nLength; ++iByte ) {
			nHash = sirLangMix( nHash, pState->pTree->source.pBytes[name.nOffset + iByte] );
		}
		nHash = sirLangMix( nHash, 0xff );
	}
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nHash = sirLangMix( nHash, ( uint8_t )( signature.result.nFingerprint >> ( iByte * 8 ) ) );
	}
	signature.nFingerprint = nHash;
	pState->pOutput->pFunctions[pState->pOutput->nFunctionCount++] = signature;
	return true;
}

static bool sirLangEnter( sirLangSignatureState_t *pState, uint32_t iNode ) noexcept
{
	if ( pState->nDepth >= 128 ) {
		sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_DEPTH_LIMIT, iNode );
		return false;
	}
	++pState->nDepth;
	return true;
}

static sirLangTypeIdentity_t sirLangInfer( sirLangSignatureState_t *pState, uint32_t iNode ) noexcept;

static sirLangTypeIdentity_t sirLangLiteral( const sirLangSignatureState_t *pState, uint32_t iNode ) noexcept
{
	const uint32_t iToken = pState->pTree->pNodes[iNode].nStartToken;
	const sirLangToken_t& token = pState->pTree->pTokens[iToken];
	if ( token.eKind == SIR_LANG_TOKEN_STRING ) {
		return sirLangScalarType( SIR_LANG_VALUE_ATOM );
	}
	if ( sirLangTextIs( pState, iToken, "true" ) || sirLangTextIs( pState, iToken, "false" ) || sirLangTextIs( pState, iToken, "ON" ) ||
	     sirLangTextIs( pState, iToken, "OFF" ) ) {
		return sirLangScalarType( SIR_LANG_VALUE_BOOL );
	}
	static constexpr struct
	{
		const char *pszSuffix;
		sirLangValueKind_t eKind;
	} s_szNumeric[] = { { "u8", SIR_LANG_VALUE_U8 },   { "i8", SIR_LANG_VALUE_I8 },   { "u16", SIR_LANG_VALUE_U16 },
	                    { "i16", SIR_LANG_VALUE_I16 }, { "u32", SIR_LANG_VALUE_U32 }, { "i32", SIR_LANG_VALUE_I32 },
	                    { "u64", SIR_LANG_VALUE_U64 }, { "i64", SIR_LANG_VALUE_I64 }, { "f16", SIR_LANG_VALUE_F16 },
	                    { "f32", SIR_LANG_VALUE_F32 }, { "f64", SIR_LANG_VALUE_F64 } };
	const uint8_t *pData = pState->pTree->source.pBytes + token.nOffset;
	for ( uint32_t iType = 0; iType < sizeof( s_szNumeric ) / sizeof( s_szNumeric[0] ); ++iType ) {
		const size_t nLength = strlen( s_szNumeric[iType].pszSuffix );
		if ( token.nLength >= nLength && memcmp( pData + token.nLength - nLength, s_szNumeric[iType].pszSuffix, nLength ) == 0 ) {
			return sirLangScalarType( s_szNumeric[iType].eKind );
		}
	}
	const bool bBasePrefixed = token.nLength >= 2 && pData[0] == '0' && ( pData[1] == 'x' || pData[1] == 'b' );
	if ( !bBasePrefixed ) {
		for ( uint32_t iByte = 0; iByte < token.nLength; ++iByte ) {
			if ( pData[iByte] == '.' || pData[iByte] == 'e' || pData[iByte] == 'E' ) {
				return sirLangScalarType( SIR_LANG_VALUE_F32 );
			}
		}
	}
	return sirLangScalarType( SIR_LANG_VALUE_I32 );
}

static const sirLangFunctionSignature_t *sirLangFindFunction( const sirLangSignatureState_t *pState, uint32_t iDeclaration ) noexcept
{
	for ( uint32_t iFunction = 0; iFunction < pState->pOutput->nFunctionCount; ++iFunction ) {
		const sirLangFunctionSignature_t& function = pState->pOutput->pFunctions[iFunction];
		if ( function.iDeclarationNode == iDeclaration ) {
			return &function;
		}
	}
	return nullptr;
}

static const sirLangExternalSignature_t *sirLangFindExternal( const sirLangSignatureState_t *pState, uint32_t iToken ) noexcept
{
	for ( uint32_t iExtern = 0; iExtern < pState->pOutput->nExternalCount; ++iExtern ) {
		if ( sirLangTextIs( pState, iToken, pState->pOutput->pExternals[iExtern].pszName ) ) {
			return &pState->pOutput->pExternals[iExtern];
		}
	}
	return nullptr;
}

static uint32_t sirLangMatchParameter( const sirLangSignatureState_t *pState, const sirLangFunctionSignature_t *pFunction,
                                       uint32_t iName ) noexcept
{
	for ( uint32_t iParam = 0; iParam < pFunction->nParameterCount; ++iParam ) {
		if ( sirLangTokensEqual( pState, iName, pState->pOutput->pParameters[pFunction->iFirstParameter + iParam].iNameToken ) ) {
			return iParam;
		}
	}
	return SIR_LANG_INVALID_INDEX;
}

static uint32_t sirLangMatchExternalParameter( const sirLangSignatureState_t *pState, const sirLangExternalSignature_t *pFunction,
                                               uint32_t iName ) noexcept
{
	for ( uint32_t iParam = 0; iParam < pFunction->nParameterCount; ++iParam ) {
		if ( sirLangTextIs( pState, iName, pFunction->ppszParameterNames[iParam] ) ) {
			return iParam;
		}
	}
	return SIR_LANG_INVALID_INDEX;
}

static sirLangTypeIdentity_t sirLangInferCall( sirLangSignatureState_t *pState, uint32_t iCall ) noexcept
{
	const uint32_t iCallee = sirLangFirstNode( pState, iCall );
	if ( iCallee >= pState->pTree->nNodeCount || pState->pTree->pNodes[iCallee].eKind != SIR_LANG_SYNTAX_NAME ) {
		sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_UNTYPED_CALLEE, iCall );
		return sirLangUnknownType();
	}
	const uint32_t iName = pState->pTree->pNodes[iCallee].nStartToken;
	const sirLangResolvedReference_t *pRef = sirLangReferenceAt( pState, iCallee );
	const sirLangFunctionSignature_t *pFunction = pRef != nullptr ? sirLangFindFunction( pState, pRef->iDeclarationNode ) : nullptr;
	// A resolved local declaration always wins over an external with the same
	// spelling, even when the local declaration is not callable.
	const bool bLocal = pRef != nullptr && pRef->iDeclarationNode != SIR_LANG_INVALID_INDEX;
	const sirLangExternalSignature_t *pExternal = pFunction == nullptr && !bLocal ? sirLangFindExternal( pState, iName ) : nullptr;
	if ( pFunction == nullptr && pExternal == nullptr ) {
		sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_UNTYPED_CALLEE, iCall );
		return sirLangUnknownType();
	}
	const uint32_t nParams = pFunction != nullptr ? pFunction->nParameterCount : pExternal->nParameterCount;
	if ( nParams > 32 ) {
		sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_INVALID_SIGNATURE, iCall );
		return sirLangUnknownType();
	}
	bool szUsed[32] = {};
	uint32_t nPosition = 0;
	uint32_t nActual = 0;
	bool bNamed = false;
	for ( uint32_t iArg = sirLangNextNode( pState, iCallee ); iArg != SIR_LANG_INVALID_INDEX; iArg = sirLangNextNode( pState, iArg ) ) {
		const sirLangSyntaxNode_t& arg = pState->pTree->pNodes[iArg];
		const bool bIsNamed = arg.eKind == SIR_LANG_SYNTAX_NAMED_ARGUMENT;
		uint32_t iParam = nPosition;
		if ( bIsNamed ) {
			bNamed = true;
			iParam = pFunction != nullptr ? sirLangMatchParameter( pState, pFunction, arg.nStartToken )
			                              : sirLangMatchExternalParameter( pState, pExternal, arg.nStartToken );
		} else if ( bNamed ) {
			sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_POSITIONAL_AFTER_NAMED, iArg );
		}
		if ( iParam >= nParams ) {
			sirLangSignatureReport( pState, bIsNamed ? SIR_LANG_SIGNATURE_UNKNOWN_ARGUMENT : SIR_LANG_SIGNATURE_ARGUMENT_COUNT, iArg );
		} else if ( szUsed[iParam] ) {
			sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_DUPLICATE_ARGUMENT, iArg );
		} else {
			szUsed[iParam] = true;
			const uint32_t iExpression = bIsNamed ? arg.nFirstChild : iArg;
			const sirLangTypeIdentity_t actual = sirLangInfer( pState, iExpression );
			const sirLangTypeIdentity_t expected = pFunction != nullptr
			                                           ? pState->pOutput->pParameters[pFunction->iFirstParameter + iParam].type
			                                           : sirLangScalarType( pExternal->pParameterTypes[iParam] );
			if ( !sirLangTypeEqual( pState, actual, expected ) ) {
				sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_ARGUMENT_TYPE, iArg );
			}
		}
		if ( !bIsNamed ) {
			++nPosition;
		}
		++nActual;
	}
	for ( uint32_t iParam = 0; iParam < nParams; ++iParam ) {
		if ( !szUsed[iParam] ) {
			sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_ARGUMENT_COUNT, iCall );
			break;
		}
	}
	if ( nActual != nParams ) {
		sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_ARGUMENT_COUNT, iCall );
	}
	if ( pState->pOutput->nCallCount >= pState->pOutput->nCallCapacity ) {
		pState->pOutput->bTruncated = true;
		pState->eFirstError = SIR_LANG_SIGNATURE_CAPACITY;
		return sirLangUnknownType();
	}
	const sirLangTypeIdentity_t result = pFunction != nullptr ? pFunction->result : sirLangScalarType( pExternal->eReturnType );
	pState->pOutput->pCalls[pState->pOutput->nCallCount++] = {
	    iCall, pFunction != nullptr ? pFunction->iDeclarationNode : SIR_LANG_INVALID_INDEX,
	    pFunction != nullptr ? 0 : pExternal->nFunctionId, pFunction != nullptr ? pFunction->nFingerprint : pExternal->nSignatureId,
	    result };
	return result;
}

static sirLangTypeIdentity_t sirLangInfer( sirLangSignatureState_t *pState, uint32_t iNode ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount || !sirLangEnter( pState, iNode ) ) {
		return sirLangUnknownType();
	}
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	sirLangTypeIdentity_t result = sirLangUnknownType();
	switch ( node.eKind ) {
	case SIR_LANG_SYNTAX_LITERAL:
		result = sirLangLiteral( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_NAME:
	case SIR_LANG_SYNTAX_MEMBER: {
		const sirLangResolvedReference_t *pRef = sirLangReferenceAt( pState, iNode );
		if ( pRef != nullptr ) {
			result = sirLangScalarType( pRef->eValue );
			const sirLangSemanticSymbol_t *pSymbol = sirLangSymbolAt( pState, pRef->iDeclarationNode );
			if ( pSymbol != nullptr && pSymbol->iTypeNode < pState->pTree->nNodeCount ) {
				result = sirLangTypeNode( pState, pSymbol->iTypeNode );
			} else if ( pSymbol != nullptr && pSymbol->eKind == SIR_LANG_SYMBOL_LOCAL &&
			            pState->pOutput->pNodeTypes[pRef->iDeclarationNode].eValue != SIR_LANG_VALUE_UNKNOWN ) {
				result = pState->pOutput->pNodeTypes[pRef->iDeclarationNode];
			}
		}
		break;
	}
	case SIR_LANG_SYNTAX_CALL:
		result = sirLangInferCall( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_GROUP:
		result = sirLangInfer( pState, node.nFirstChild );
		break;
	case SIR_LANG_SYNTAX_UNARY:
		result = sirLangInfer( pState, node.nFirstChild );
		if ( sirLangTextIs( pState, node.nStartToken, "not" ) || sirLangTextIs( pState, node.nStartToken, "!" ) ) {
			if ( result.eValue != SIR_LANG_VALUE_UNKNOWN && result.eValue != SIR_LANG_VALUE_BOOL ) {
				sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_ARGUMENT_TYPE, iNode );
			}
			result = sirLangScalarType( SIR_LANG_VALUE_BOOL );
		}
		break;
	case SIR_LANG_SYNTAX_BINARY: {
		const uint32_t iLeft = node.nFirstChild;
		const sirLangTypeIdentity_t left = sirLangInfer( pState, iLeft );
		const uint32_t iRight = sirLangNextNode( pState, iLeft );
		const sirLangTypeIdentity_t right = sirLangInfer( pState, iRight );
		result = left;
		const uint32_t iOperator = pState->pTree->pNodes[iLeft].nEndToken;
		const bool bLogical = sirLangTextIs( pState, iOperator, "and" ) ||
		                      sirLangTextIs( pState, iOperator, "or" ) || sirLangTextIs( pState, iOperator, "xor" );
		const bool bArithmetic = sirLangTextIs( pState, iOperator, "+" ) ||
		                         sirLangTextIs( pState, iOperator, "-" ) || sirLangTextIs( pState, iOperator, "*" ) ||
		                         sirLangTextIs( pState, iOperator, "/" );
		const bool bOrdered = sirLangTextIs( pState, iOperator, "<" ) || sirLangTextIs( pState, iOperator, ">" );
		const bool bEquality = sirLangTextIs( pState, iOperator, "!" ) || sirLangTextIs( pState, iOperator, "=" );
		const bool bBothKnown = left.eValue != SIR_LANG_VALUE_UNKNOWN && right.eValue != SIR_LANG_VALUE_UNKNOWN;
		const bool bIntegerOperands = left.eValue >= SIR_LANG_VALUE_I8 && left.eValue <= SIR_LANG_VALUE_U64 &&
		                              right.eValue >= SIR_LANG_VALUE_I8 && right.eValue <= SIR_LANG_VALUE_U64;
		if ( bLogical && bBothKnown && ( left.eValue != SIR_LANG_VALUE_BOOL || right.eValue != SIR_LANG_VALUE_BOOL ) ) {
			sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_ARGUMENT_TYPE, iNode );
		}
		if ( bBothKnown && ( bArithmetic || bOrdered ) &&
		     ( !bIntegerOperands || !sirLangTypeEqual( pState, left, right ) ) ) {
			sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_ARGUMENT_TYPE, iNode );
		}
		if ( bBothKnown && bEquality ) {
			const bool bComparable = bIntegerOperands || ( left.eValue == SIR_LANG_VALUE_BOOL && right.eValue == SIR_LANG_VALUE_BOOL );
			if ( !bComparable || !sirLangTypeEqual( pState, left, right ) ) {
				sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_ARGUMENT_TYPE, iNode );
			}
		}
		if ( bLogical || bOrdered || bEquality ) {
			result = sirLangScalarType( SIR_LANG_VALUE_BOOL );
		}
		break;
	}
	default:
		break;
	}
	pState->pOutput->pNodeTypes[iNode] = result;
	--pState->nDepth;
	return result;
}

static void sirLangVisit( sirLangSignatureState_t *pState, uint32_t iNode, sirLangTypeIdentity_t eReturn ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount || pState->pOutput->bTruncated || !sirLangEnter( pState, iNode ) ) {
		return;
	}
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	if ( node.eKind == SIR_LANG_SYNTAX_FUNCTION ) {
		const sirLangFunctionSignature_t *pFunction = sirLangFindFunction( pState, iNode );
		if ( pFunction != nullptr ) {
			eReturn = pFunction->result;
		}
	}
	if ( node.eKind == SIR_LANG_SYNTAX_CALL ) {
		sirLangInfer( pState, iNode );
		--pState->nDepth;
		return;
	}
	if ( node.eKind == SIR_LANG_SYNTAX_TYPE ) {
		pState->pOutput->pNodeTypes[iNode] = sirLangTypeNode( pState, iNode );
		--pState->nDepth;
		return;
	}
	if ( node.eKind == SIR_LANG_SYNTAX_LET || node.eKind == SIR_LANG_SYNTAX_ASSIGNMENT || node.eKind == SIR_LANG_SYNTAX_RETURN ) {
		uint32_t iType = SIR_LANG_INVALID_INDEX;
		uint32_t iExpression = SIR_LANG_INVALID_INDEX;
		for ( uint32_t iChild = node.nFirstChild; iChild != SIR_LANG_INVALID_INDEX; iChild = sirLangNextNode( pState, iChild ) ) {
			if ( pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_TYPE ) {
				iType = iChild;
			} else {
				iExpression = iChild;
			}
		}
		if ( node.eKind == SIR_LANG_SYNTAX_ASSIGNMENT ) {
			iType = node.nFirstChild;
			iExpression = sirLangNextNode( pState, iType );
		}
		const sirLangTypeIdentity_t expected = node.eKind == SIR_LANG_SYNTAX_RETURN ? eReturn
		                                       : node.eKind == SIR_LANG_SYNTAX_LET && iType != SIR_LANG_INVALID_INDEX
		                                           ? sirLangTypeNode( pState, iType )
		                                       : node.eKind == SIR_LANG_SYNTAX_ASSIGNMENT ? sirLangInfer( pState, iType )
		                                                                                  : sirLangUnknownType();
		const sirLangTypeIdentity_t actual =
		    iExpression != SIR_LANG_INVALID_INDEX ? sirLangInfer( pState, iExpression ) : sirLangScalarType( SIR_LANG_VALUE_VOID );
		if ( expected.eValue != SIR_LANG_VALUE_UNKNOWN && actual.eValue != SIR_LANG_VALUE_UNKNOWN &&
		     !sirLangTypeEqual( pState, expected, actual ) ) {
			sirLangSignatureReport(
			    pState, node.eKind == SIR_LANG_SYNTAX_RETURN ? SIR_LANG_SIGNATURE_RETURN_TYPE : SIR_LANG_SIGNATURE_ARGUMENT_TYPE, iNode );
		}
		pState->pOutput->pNodeTypes[iNode] = expected.eValue == SIR_LANG_VALUE_UNKNOWN ? actual : expected;
		--pState->nDepth;
		return;
	}
	if ( node.eKind == SIR_LANG_SYNTAX_WHEN ) {
		const uint32_t iCondition = node.nFirstChild;
		const sirLangTypeIdentity_t condition = sirLangInfer( pState, iCondition );
		if ( condition.eValue != SIR_LANG_VALUE_UNKNOWN && condition.eValue != SIR_LANG_VALUE_BOOL ) {
			sirLangSignatureReport( pState, SIR_LANG_SIGNATURE_ARGUMENT_TYPE, iCondition );
		}
		for ( uint32_t iChild = sirLangNextNode( pState, iCondition ); iChild != SIR_LANG_INVALID_INDEX;
		      iChild = sirLangNextNode( pState, iChild ) ) {
			sirLangVisit( pState, iChild, eReturn );
		}
	} else {
		for ( uint32_t iChild = node.nFirstChild; iChild != SIR_LANG_INVALID_INDEX; iChild = sirLangNextNode( pState, iChild ) ) {
			sirLangVisit( pState, iChild, eReturn );
		}
	}
	--pState->nDepth;
}

sirLangSignatureError_t SIRLangResolveSignatures( const sirLangSyntaxTree_t *pTree, const sirLangSemanticOutput_t *pSemantic,
                                                  sirLangSignatureOutput_t *pOutput ) noexcept
{
	if ( pTree == nullptr || pSemantic == nullptr || pOutput == nullptr || pTree->pNodes == nullptr || pTree->pTokens == nullptr ||
	     pTree->nNodeCount == 0 || pTree->nTokenCount == 0 || pTree->nRoot >= pTree->nNodeCount || pSemantic->pSymbols == nullptr ||
	     pSemantic->pReferences == nullptr || pSemantic->nSymbolCount > pSemantic->nSymbolCapacity ||
	     pSemantic->nReferenceCount > pSemantic->nReferenceCapacity || pSemantic->nDiagnosticCount != 0 || pSemantic->bTruncated ||
	     pOutput->pFunctions == nullptr || pOutput->pParameters == nullptr || pOutput->pCalls == nullptr ||
	     pOutput->pNodeTypes == nullptr || pOutput->nNodeTypeCapacity < pTree->nNodeCount || pOutput->pDiagnostics == nullptr ||
	     pOutput->nDiagnosticCapacity == 0 || ( pOutput->nExternalCount > 0 && pOutput->pExternals == nullptr ) ) {
		return SIR_LANG_SIGNATURE_INVALID_INPUT;
	}
	pOutput->nFunctionCount = 0;
	pOutput->nParameterCount = 0;
	pOutput->nCallCount = 0;
	pOutput->nDiagnosticCount = 0;
	pOutput->bTruncated = false;
	for ( uint32_t iNode = 0; iNode < pTree->nNodeCount; ++iNode ) {
		pOutput->pNodeTypes[iNode] = sirLangUnknownType();
	}
	sirLangSignatureState_t state = { pTree, pSemantic, pOutput, SIR_LANG_SIGNATURE_OK, 0 };
	for ( uint32_t iSymbol = 0; iSymbol < pSemantic->nSymbolCount; ++iSymbol ) {
		const sirLangSemanticSymbol_t& symbol = pSemantic->pSymbols[iSymbol];
		if ( symbol.eKind == SIR_LANG_SYMBOL_FUNCTION && symbol.iDeclarationNode < pTree->nNodeCount ) {
			sirLangRegisterSignature( &state, symbol );
		}
	}
	for ( uint32_t iExtern = 0; iExtern < pOutput->nExternalCount && state.eFirstError == SIR_LANG_SIGNATURE_OK; ++iExtern ) {
		const sirLangExternalSignature_t& ext = pOutput->pExternals[iExtern];
		if ( ext.pszName == nullptr || ext.nFunctionId == 0 || ext.nSignatureId == 0 || ext.nParameterCount > 32 ||
		     ( ext.nParameterCount > 0 && ( ext.pParameterTypes == nullptr || ext.ppszParameterNames == nullptr ) ) ||
		     ext.eReturnType == SIR_LANG_VALUE_UNKNOWN || ext.eReturnType >= SIR_LANG_VALUE_OTHER ) {
			sirLangSignatureReport( &state, SIR_LANG_SIGNATURE_INVALID_SIGNATURE, pTree->nRoot );
			continue;
		}
		for ( uint32_t iPrevious = 0; iPrevious < iExtern; ++iPrevious ) {
			if ( strcmp( ext.pszName, pOutput->pExternals[iPrevious].pszName ) == 0 ) {
				sirLangSignatureReport( &state, SIR_LANG_SIGNATURE_INVALID_SIGNATURE, pTree->nRoot );
			}
		}
		for ( uint32_t iParam = 0; iParam < ext.nParameterCount; ++iParam ) {
			if ( ext.ppszParameterNames[iParam] == nullptr || ext.pParameterTypes[iParam] <= SIR_LANG_VALUE_VOID ||
			     ext.pParameterTypes[iParam] >= SIR_LANG_VALUE_OTHER ) {
				sirLangSignatureReport( &state, SIR_LANG_SIGNATURE_INVALID_SIGNATURE, pTree->nRoot );
				break;
			}
			for ( uint32_t iEarlier = 0; iEarlier < iParam; ++iEarlier ) {
				if ( strcmp( ext.ppszParameterNames[iParam], ext.ppszParameterNames[iEarlier] ) == 0 ) {
					sirLangSignatureReport( &state, SIR_LANG_SIGNATURE_INVALID_SIGNATURE, pTree->nRoot );
				}
			}
		}
	}
	if ( state.eFirstError == SIR_LANG_SIGNATURE_OK && !pOutput->bTruncated ) {
		sirLangVisit( &state, pTree->nRoot, sirLangScalarType( SIR_LANG_VALUE_VOID ) );
	}
	return state.eFirstError;
}
