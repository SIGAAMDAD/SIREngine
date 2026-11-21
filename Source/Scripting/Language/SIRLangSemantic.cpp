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

#include <Scripting/Language/SIRLangSemantic.h>
#include <string.h>

typedef struct sirLangDeclarationSpec_s
{
	uint32_t iScope;
	sirLangSymbolKind_t eKind;
	sirLangValueKind_t eValue;
} sirLangDeclarationSpec_t;

typedef struct sirLangSemanticState_s
{
	const sirLangSyntaxTree_t *pTree;
	sirLangSemanticOutput_t *pOutput;
	uint32_t szScopes[SIR_LANG_MAX_SEMANTIC_DEPTH];
	uint32_t nScopeDepth;
	uint32_t nDepth;
	uint32_t iSchema;
	sirLangValueKind_t eReturnType;
	sirLangSemanticError_t eFirstError;
} sirLangSemanticState_t;

static bool sirLangEqualToken( const sirLangSyntaxTree_t *pTree, uint32_t iToken, const char *pszText ) noexcept
{
	if ( pszText == nullptr || iToken >= pTree->nTokenCount ) {
		return false;
	}
	const sirLangToken_t& token = pTree->pTokens[iToken];
	const size_t nLength = strlen( pszText );
	return token.nLength == nLength && token.nOffset <= pTree->source.nBytes && token.nLength <= pTree->source.nBytes - token.nOffset &&
	       memcmp( pTree->source.pBytes + token.nOffset, pszText, nLength ) == 0;
}

static bool sirLangSameTokens( const sirLangSyntaxTree_t *pTree, uint32_t iFirst, uint32_t iSecond ) noexcept
{
	if ( iFirst >= pTree->nTokenCount || iSecond >= pTree->nTokenCount ) {
		return false;
	}
	const sirLangToken_t& a = pTree->pTokens[iFirst];
	const sirLangToken_t& b = pTree->pTokens[iSecond];
	return a.nLength == b.nLength && memcmp( pTree->source.pBytes + a.nOffset, pTree->source.pBytes + b.nOffset, a.nLength ) == 0;
}

static uint32_t sirLangFirstChild( const sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount ) {
		return SIR_LANG_INVALID_INDEX;
	}
	return pState->pTree->pNodes[iNode].nFirstChild;
}

static uint32_t sirLangNextChild( const sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount ) {
		return SIR_LANG_INVALID_INDEX;
	}
	return pState->pTree->pNodes[iNode].nNextSibling;
}

static void sirLangSemanticReport( sirLangSemanticState_t *pState, sirLangSemanticError_t eError, uint32_t iNode ) noexcept
{
	if ( pState->eFirstError == SIR_LANG_SEMANTIC_OK ) {
		pState->eFirstError = eError;
	}
	sirLangSemanticOutput_t *pOutput = pState->pOutput;
	if ( pOutput->nDiagnosticCount >= pOutput->nDiagnosticCapacity ) {
		pOutput->bTruncated = true;
		pState->eFirstError = SIR_LANG_SEMANTIC_CAPACITY;
		return;
	}
	uint32_t iToken = 0;
	if ( iNode < pState->pTree->nNodeCount ) {
		iToken = pState->pTree->pNodes[iNode].nStartToken;
	}
	const sirLangToken_t& token = pState->pTree->pTokens[iToken < pState->pTree->nTokenCount ? iToken : 0];
	pOutput->pDiagnostics[pOutput->nDiagnosticCount++] = { eError, iNode, token.nOffset, token.nLine, token.nColumn };
}

static void sirLangRecordReference( sirLangSemanticState_t *pState, uint32_t iNode, uint32_t iDeclaration,
                                    sirLangValueKind_t eValue ) noexcept
{
	sirLangSemanticOutput_t *pOutput = pState->pOutput;
	if ( pOutput->nReferenceCount >= pOutput->nReferenceCapacity ) {
		pOutput->bTruncated = true;
		pState->eFirstError = SIR_LANG_SEMANTIC_CAPACITY;
		return;
	}
	pOutput->pReferences[pOutput->nReferenceCount++] = { iNode, iDeclaration, eValue };
	++pOutput->nResolvedReferences;
}

static bool sirLangPushScope( sirLangSemanticState_t *pState, uint32_t iScope ) noexcept
{
	if ( pState->nScopeDepth >= SIR_LANG_MAX_SEMANTIC_DEPTH ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_DEPTH_LIMIT, iScope );
		return false;
	}
	pState->szScopes[pState->nScopeDepth++] = iScope;
	return true;
}

static bool sirLangEnterNode( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	if ( ++pState->nDepth > SIR_LANG_MAX_SEMANTIC_DEPTH ) {
		--pState->nDepth;
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_DEPTH_LIMIT, iNode );
		return false;
	}
	return true;
}

static sirLangValueKind_t sirLangPrimitiveType( const sirLangSyntaxTree_t *pTree, uint32_t iToken ) noexcept
{
	static constexpr struct
	{
		const char *pszName;
		sirLangValueKind_t eKind;
	} s_szTypes[] = { { "void", SIR_LANG_VALUE_VOID }, { "bool", SIR_LANG_VALUE_BOOL }, { "i8", SIR_LANG_VALUE_I8 },
	                  { "u8", SIR_LANG_VALUE_U8 },     { "i16", SIR_LANG_VALUE_I16 },   { "u16", SIR_LANG_VALUE_U16 },
	                  { "i32", SIR_LANG_VALUE_I32 },   { "u32", SIR_LANG_VALUE_U32 },   { "i64", SIR_LANG_VALUE_I64 },
	                  { "u64", SIR_LANG_VALUE_U64 },   { "f16", SIR_LANG_VALUE_F16 },   { "f32", SIR_LANG_VALUE_F32 },
	                  { "f64", SIR_LANG_VALUE_F64 },   { "atom", SIR_LANG_VALUE_ATOM } };
	for ( uint32_t iType = 0; iType < sizeof( s_szTypes ) / sizeof( s_szTypes[0] ); ++iType ) {
		if ( sirLangEqualToken( pTree, iToken, s_szTypes[iType].pszName ) ) {
			return s_szTypes[iType].eKind;
		}
	}
	return SIR_LANG_VALUE_UNKNOWN;
}

static bool sirLangGenericTypeName( const sirLangSyntaxTree_t *pTree, uint32_t iToken ) noexcept
{
	static constexpr const char *s_pszGeneric[] = { "handle", "option", "result", "variant", "stack_list", "slice", "vec", "fixed", "fn" };
	for ( uint32_t iName = 0; iName < sizeof( s_pszGeneric ) / sizeof( s_pszGeneric[0] ); ++iName ) {
		if ( sirLangEqualToken( pTree, iToken, s_pszGeneric[iName] ) ) {
			return true;
		}
	}
	return false;
}

static bool sirLangOtherBuiltinType( const sirLangSyntaxTree_t *pTree, uint32_t iToken ) noexcept
{
	static constexpr const char *s_pszTypes[] = { "vec2", "vec3", "vec4",     "quat",   "mat3",    "mat4",   "color",  "color8",
	                                              "text", "rune", "duration", "unorm8", "unorm16", "snorm8", "snorm16" };
	for ( uint32_t iName = 0; iName < sizeof( s_pszTypes ) / sizeof( s_pszTypes[0] ); ++iName ) {
		if ( sirLangEqualToken( pTree, iToken, s_pszTypes[iName] ) ) {
			return true;
		}
	}
	return false;
}

static bool sirLangMatchExternal( const sirLangSemanticState_t *pState, uint32_t iToken, sirLangSymbolKind_t eKind,
                                  sirLangValueKind_t *pValue ) noexcept
{
	for ( uint32_t iItem = 0; iItem < pState->pOutput->nExternalCount; ++iItem ) {
		const sirLangExternalSymbol_t& ext = pState->pOutput->pExternals[iItem];
		if ( ext.eKind == eKind && sirLangEqualToken( pState->pTree, iToken, ext.pszName ) ) {
			*pValue = ext.eValue;
			return true;
		}
	}
	return false;
}

static uint32_t sirLangDeclarationName( const sirLangSyntaxTree_t *pTree, uint32_t iNode ) noexcept
{
	const sirLangSyntaxNode_t& node = pTree->pNodes[iNode];
	const uint32_t iStart = node.nStartToken;
	if ( iStart >= node.nEndToken || iStart >= pTree->nTokenCount ) {
		return SIR_LANG_INVALID_INDEX;
	}
	uint32_t iToken = iStart;
	if ( node.eKind == SIR_LANG_SYNTAX_CONDITION_BANK ) {
		iToken += 2;
	} else if ( node.eKind == SIR_LANG_SYNTAX_EVENT ) {
		// Legacy hand-authored AST fixtures use "on Name"; the modern parser
		// requires "on event Name". This does not accept legacy source syntax.
		iToken += sirLangEqualToken( pTree, iStart + 1, "event" ) ? 2 : 1;
	} else if ( node.eKind == SIR_LANG_SYNTAX_PARAMETER || node.eKind == SIR_LANG_SYNTAX_FIELD ) {
		const bool bFieldPrefix = sirLangEqualToken( pTree, iStart, "field" );
		if ( bFieldPrefix ) {
			iToken = iStart + 1;
		}
		// A typed declaration emitted by the C-style parser carries the type
		// as its first child and the name after that type. Older AST fixtures
		// may begin with the name and a colon instead.
		uint32_t iType = node.nFirstChild;
		for ( uint32_t n = 0; iType < pTree->nNodeCount && n < pTree->nNodeCount; iType = pTree->pNodes[iType].nNextSibling, ++n ) {
			const sirLangSyntaxNode_t& child = pTree->pNodes[iType];
			if ( child.eKind == SIR_LANG_SYNTAX_TYPE && child.nStartToken == iToken ) {
				iToken = child.nEndToken;
				break;
			}
		}
	} else {
		++iToken;
	}
	return iToken < node.nEndToken && iToken < pTree->nTokenCount && pTree->pTokens[iToken].eKind == SIR_LANG_TOKEN_IDENTIFIER
	           ? iToken
			   : SIR_LANG_INVALID_INDEX;
}

static sirLangSemanticSymbol_t *sirLangFindSymbol( sirLangSemanticState_t *pState, uint32_t iToken ) noexcept
{
	for ( uint32_t iDepth = pState->nScopeDepth; iDepth > 0; --iDepth ) {
		const uint32_t iScope = pState->szScopes[iDepth - 1];
		for ( uint32_t iSymbol = pState->pOutput->nSymbolCount; iSymbol > 0; --iSymbol ) {
			sirLangSemanticSymbol_t& symbol = pState->pOutput->pSymbols[iSymbol - 1];
			if ( symbol.iScopeNode == iScope && sirLangSameTokens( pState->pTree, symbol.iNameToken, iToken ) ) {
				return &symbol;
			}
		}
	}
	return nullptr;
}

static sirLangSemanticSymbol_t *sirLangFindInScope( sirLangSemanticState_t *pState, uint32_t iToken, uint32_t iScope ) noexcept
{
	for ( uint32_t iSymbol = 0; iSymbol < pState->pOutput->nSymbolCount; ++iSymbol ) {
		sirLangSemanticSymbol_t& symbol = pState->pOutput->pSymbols[iSymbol];
		if ( symbol.iScopeNode == iScope && sirLangSameTokens( pState->pTree, symbol.iNameToken, iToken ) ) {
			return &symbol;
		}
	}
	return nullptr;
}

static sirLangSemanticSymbol_t *sirLangFindTypeDeclaration( sirLangSemanticState_t *pState, uint32_t iToken ) noexcept
{
	for ( uint32_t iSymbol = 0; iSymbol < pState->pOutput->nSymbolCount; ++iSymbol ) {
		sirLangSemanticSymbol_t& symbol = pState->pOutput->pSymbols[iSymbol];
		if ( symbol.iScopeNode == pState->pTree->nRoot &&
		     ( symbol.eKind == SIR_LANG_SYMBOL_TYPE || symbol.eKind == SIR_LANG_SYMBOL_BEHAVIOR ) &&
		     sirLangSameTokens( pState->pTree, symbol.iNameToken, iToken ) ) {
			return &symbol;
		}
	}
	return nullptr;
}

static sirLangSemanticSymbol_t *sirLangDeclare( sirLangSemanticState_t *pState, uint32_t iNode,
                                                const sirLangDeclarationSpec_t& spec ) noexcept
{
	const uint32_t iToken = sirLangDeclarationName( pState->pTree, iNode );
	if ( iToken == SIR_LANG_INVALID_INDEX ) {
		return nullptr;
	}
	sirLangSemanticSymbol_t *pPrevious = sirLangFindInScope( pState, iToken, spec.iScope );
	if ( pPrevious != nullptr ) {
		if ( pPrevious->iDeclarationNode == iNode ) {
			return pPrevious;
		}
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_DUPLICATE_SYMBOL, iNode );
		return nullptr;
	}
	if ( pState->pOutput->nSymbolCount >= pState->pOutput->nSymbolCapacity ) {
		pState->pOutput->bTruncated = true;
		pState->eFirstError = SIR_LANG_SEMANTIC_CAPACITY;
		return nullptr;
	}
	sirLangSemanticSymbol_t& symbol = pState->pOutput->pSymbols[pState->pOutput->nSymbolCount++];
	symbol = { iToken, iNode, spec.iScope, SIR_LANG_INVALID_INDEX, spec.eKind, spec.eValue };
	return &symbol;
}

static bool sirLangDeclarationKind( sirLangSyntaxKind_t eSyntax, sirLangSymbolKind_t *pKind ) noexcept
{
	switch ( eSyntax ) {
	case SIR_LANG_SYNTAX_SCHEMA:
	case SIR_LANG_SYNTAX_INTERFACE:
	case SIR_LANG_SYNTAX_RECORD:
	case SIR_LANG_SYNTAX_ENUM:
		*pKind = SIR_LANG_SYMBOL_TYPE;
		return true;
	case SIR_LANG_SYNTAX_CLASS:
	case SIR_LANG_SYNTAX_BEHAVIOR:
		*pKind = SIR_LANG_SYMBOL_BEHAVIOR;
		return true;
	case SIR_LANG_SYNTAX_FUNCTION:
		*pKind = SIR_LANG_SYMBOL_FUNCTION;
		return true;
	case SIR_LANG_SYNTAX_EVENT:
		*pKind = SIR_LANG_SYMBOL_EVENT;
		return true;
	default:
		return false;
	}
}

static void sirLangPredeclare( sirLangSemanticState_t *pState, uint32_t iScope ) noexcept
{
	uint32_t iChild = sirLangFirstChild( pState, iScope );
	for ( uint32_t nSteps = 0; iChild != SIR_LANG_INVALID_INDEX && nSteps < pState->pTree->nNodeCount; ++nSteps ) {
		sirLangSymbolKind_t eKind = SIR_LANG_SYMBOL_LOCAL;
		const sirLangSyntaxKind_t eSyntax = pState->pTree->pNodes[iChild].eKind;
		if ( sirLangDeclarationKind( eSyntax, &eKind ) ) {
			sirLangDeclare(
			    pState, iChild,
			    { iScope, eKind,
				  eKind == SIR_LANG_SYMBOL_FUNCTION || eKind == SIR_LANG_SYMBOL_EVENT ? SIR_LANG_VALUE_FUNCTION : SIR_LANG_VALUE_OTHER } );
		}
		iChild = sirLangNextChild( pState, iChild );
	}
}

static sirLangValueKind_t sirLangCheckType( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount || !sirLangEnterNode( pState, iNode ) ) {
		return SIR_LANG_VALUE_UNKNOWN;
	}
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	const uint32_t iStart = node.nStartToken;
	sirLangValueKind_t eValue = SIR_LANG_VALUE_UNKNOWN;
	if ( sirLangEqualToken( pState->pTree, iStart, "[" ) ) {
		eValue = SIR_LANG_VALUE_OTHER;
	} else {
		eValue = sirLangPrimitiveType( pState->pTree, iStart );
		if ( eValue == SIR_LANG_VALUE_UNKNOWN &&
		     ( sirLangGenericTypeName( pState->pTree, iStart ) || sirLangOtherBuiltinType( pState->pTree, iStart ) ) ) {
			eValue = SIR_LANG_VALUE_OTHER;
		}
		if ( eValue == SIR_LANG_VALUE_UNKNOWN ) {
			sirLangSemanticSymbol_t *pDeclared = sirLangFindTypeDeclaration( pState, iStart );
			if ( pDeclared != nullptr ) {
				eValue = SIR_LANG_VALUE_OTHER;
				sirLangRecordReference( pState, iNode, pDeclared->iDeclarationNode, eValue );
			}
		}
		if ( eValue == SIR_LANG_VALUE_UNKNOWN && sirLangMatchExternal( pState, iStart, SIR_LANG_SYMBOL_TYPE, &eValue ) ) {
			sirLangRecordReference( pState, iNode, SIR_LANG_INVALID_INDEX, eValue );
		}
	}
	if ( eValue == SIR_LANG_VALUE_UNKNOWN ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_UNKNOWN_TYPE, iNode );
	}
	uint32_t iChild = node.nFirstChild;
	for ( uint32_t nSteps = 0; iChild != SIR_LANG_INVALID_INDEX && nSteps < pState->pTree->nNodeCount; ++nSteps ) {
		if ( pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_TYPE ) {
			sirLangCheckType( pState, iChild );
		}
		iChild = sirLangNextChild( pState, iChild );
	}
	--pState->nDepth;
	return eValue;
}

static uint64_t sirLangIntegerMaximum( sirLangValueKind_t eKind ) noexcept
{
	switch ( eKind ) {
	case SIR_LANG_VALUE_I8:
		return INT8_MAX;
	case SIR_LANG_VALUE_U8:
		return UINT8_MAX;
	case SIR_LANG_VALUE_I16:
		return INT16_MAX;
	case SIR_LANG_VALUE_U16:
		return UINT16_MAX;
	case SIR_LANG_VALUE_I32:
		return INT32_MAX;
	case SIR_LANG_VALUE_U32:
		return UINT32_MAX;
	case SIR_LANG_VALUE_I64:
		return INT64_MAX;
	case SIR_LANG_VALUE_U64:
		return UINT64_MAX;
	default:
		return UINT64_MAX;
	}
}

static bool sirLangValidInteger( const uint8_t *pBytes, uint32_t nEnd, uint32_t nBase, sirLangValueKind_t eKind ) noexcept
{
	const uint64_t nMaximum = sirLangIntegerMaximum( eKind );
	uint64_t nValue = 0;
	uint32_t iDigit = nBase == 10 ? 0 : 2;
	if ( iDigit >= nEnd ) {
		return false;
	}
	for ( ; iDigit < nEnd; ++iDigit ) {
		const uint8_t c = pBytes[iDigit];
		uint32_t nDigit = UINT32_MAX;
		if ( c >= '0' && c <= '9' ) {
			nDigit = c - '0';
		} else if ( c >= 'a' && c <= 'f' ) {
			nDigit = c - 'a' + 10;
		} else if ( c >= 'A' && c <= 'F' ) {
			nDigit = c - 'A' + 10;
		}
		if ( nDigit >= nBase || nValue > ( nMaximum - nDigit ) / nBase ) {
			return false;
		}
		nValue = nValue * nBase + nDigit;
	}
	return true;
}

static sirLangValueKind_t sirLangLiteralType( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	const uint32_t iToken = pState->pTree->pNodes[iNode].nStartToken;
	const sirLangToken_t& token = pState->pTree->pTokens[iToken];
	if ( token.eKind == SIR_LANG_TOKEN_STRING ) {
		return SIR_LANG_VALUE_ATOM;
	}
	if ( sirLangEqualToken( pState->pTree, iToken, "true" ) || sirLangEqualToken( pState->pTree, iToken, "false" ) ||
	     sirLangEqualToken( pState->pTree, iToken, "ON" ) || sirLangEqualToken( pState->pTree, iToken, "OFF" ) ) {
		return SIR_LANG_VALUE_BOOL;
	}
	if ( token.eKind != SIR_LANG_TOKEN_NUMBER ) {
		return SIR_LANG_VALUE_UNKNOWN;
	}
	const uint8_t *pBytes = pState->pTree->source.pBytes + token.nOffset;
	const uint32_t nBytes = token.nLength;
	uint32_t iSuffix = 0;
	bool bFloat = false;
	const bool bHex = nBytes > 2 && pBytes[0] == '0' && ( pBytes[1] == 'x' || pBytes[1] == 'b' );
	if ( bHex ) {
		iSuffix = 2;
		while ( iSuffix < nBytes ) {
			const uint8_t c = pBytes[iSuffix];
			if ( ( c >= '0' && c <= '9' && ( pBytes[1] == 'x' || c <= '1' ) ) ||
			     ( pBytes[1] == 'x' && ( ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' ) ) ) ) {
				++iSuffix;
			} else {
				break;
			}
		}
	} else {
		while ( iSuffix < nBytes && pBytes[iSuffix] >= '0' && pBytes[iSuffix] <= '9' ) {
			++iSuffix;
		}
		if ( iSuffix < nBytes && pBytes[iSuffix] == '.' ) {
			bFloat = true;
			++iSuffix;
			while ( iSuffix < nBytes && pBytes[iSuffix] >= '0' && pBytes[iSuffix] <= '9' ) {
				++iSuffix;
			}
		}
		if ( iSuffix < nBytes && ( pBytes[iSuffix] == 'e' || pBytes[iSuffix] == 'E' ) ) {
			uint32_t iPower = iSuffix + 1;
			if ( iPower < nBytes && ( pBytes[iPower] == '+' || pBytes[iPower] == '-' ) ) {
				++iPower;
			}
			if ( iPower < nBytes && pBytes[iPower] >= '0' && pBytes[iPower] <= '9' ) {
				bFloat = true;
				iSuffix = iPower + 1;
				while ( iSuffix < nBytes && pBytes[iSuffix] >= '0' && pBytes[iSuffix] <= '9' ) {
					++iSuffix;
				}
			}
		}
	}
	const uint32_t nBase = bHex ? ( pBytes[1] == 'b' ? 2U : 16U ) : 10U;
	if ( iSuffix == nBytes ) {
		if ( !bFloat && !sirLangValidInteger( pBytes, iSuffix, nBase, SIR_LANG_VALUE_U64 ) ) {
			sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_LITERAL, iNode );
			return SIR_LANG_VALUE_UNKNOWN;
		}
		return bFloat ? SIR_LANG_VALUE_F32 : SIR_LANG_VALUE_I32;
	}
	static constexpr struct
	{
		const char *pszSuffix;
		sirLangValueKind_t eValue;
	} s_szSuffixes[] = { { "i8", SIR_LANG_VALUE_I8 },   { "u8", SIR_LANG_VALUE_U8 },   { "i16", SIR_LANG_VALUE_I16 },
	                     { "u16", SIR_LANG_VALUE_U16 }, { "i32", SIR_LANG_VALUE_I32 }, { "u32", SIR_LANG_VALUE_U32 },
	                     { "i64", SIR_LANG_VALUE_I64 }, { "u64", SIR_LANG_VALUE_U64 }, { "f16", SIR_LANG_VALUE_F16 },
	                     { "f32", SIR_LANG_VALUE_F32 }, { "f64", SIR_LANG_VALUE_F64 } };
	for ( uint32_t iItem = 0; iItem < sizeof( s_szSuffixes ) / sizeof( s_szSuffixes[0] ); ++iItem ) {
		const size_t nSuffix = strlen( s_szSuffixes[iItem].pszSuffix );
		if ( nBytes - iSuffix == nSuffix && memcmp( pBytes + iSuffix, s_szSuffixes[iItem].pszSuffix, nSuffix ) == 0 ) {
			const sirLangValueKind_t eLiteral = s_szSuffixes[iItem].eValue;
			if ( ( bFloat && eLiteral < SIR_LANG_VALUE_F16 ) ||
			     ( eLiteral < SIR_LANG_VALUE_F16 && !sirLangValidInteger( pBytes, iSuffix, nBase, eLiteral ) ) ) {
				sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_LITERAL, iNode );
				return SIR_LANG_VALUE_UNKNOWN;
			}
			return eLiteral;
		}
	}
	sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_LITERAL, iNode );
	return SIR_LANG_VALUE_UNKNOWN;
}

// Resolve a bare integral literal against a known scalar storage type. The
// literal's default i32 type does not constrain explicitly typed consumers.
static bool sirLangContextualInteger( const sirLangSemanticState_t *pState, uint32_t iNode, sirLangValueKind_t eExpected ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount || eExpected < SIR_LANG_VALUE_I8 || eExpected > SIR_LANG_VALUE_U64 ) {
		return false;
	}
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	if ( node.eKind == SIR_LANG_SYNTAX_UNARY && node.nStartToken < pState->pTree->nTokenCount ) {
		const sirLangToken_t& sign = pState->pTree->pTokens[node.nStartToken];
		const uint8_t *pSign = pState->pTree->source.pBytes + sign.nOffset;
		if ( sign.nLength == 1 && pSign[0] == '+' ) {
			return sirLangContextualInteger( pState, node.nFirstChild, eExpected );
		}
		if ( sign.nLength == 1 && pSign[0] == '-' &&
		     ( eExpected == SIR_LANG_VALUE_I8 || eExpected == SIR_LANG_VALUE_I16 || eExpected == SIR_LANG_VALUE_I32 ||
		       eExpected == SIR_LANG_VALUE_I64 ) ) {
			uint32_t iChild = node.nFirstChild;
			while ( iChild < pState->pTree->nNodeCount && pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_GROUP ) {
				iChild = pState->pTree->pNodes[iChild].nFirstChild;
			}
			if ( iChild >= pState->pTree->nNodeCount || pState->pTree->pNodes[iChild].eKind != SIR_LANG_SYNTAX_LITERAL ) {
				return false;
			}
			const uint32_t iToken = pState->pTree->pNodes[iChild].nStartToken;
			if ( iToken >= pState->pTree->nTokenCount ) {
				return false;
			}
			const sirLangToken_t& token = pState->pTree->pTokens[iToken];
			if ( token.eKind != SIR_LANG_TOKEN_NUMBER || token.nLength == 0 ) {
				return false;
			}
			const uint8_t *pData = pState->pTree->source.pBytes + token.nOffset;
			const bool bPrefix = token.nLength > 2 && pData[0] == '0' && ( pData[1] == 'x' || pData[1] == 'b' );
			const uint32_t nBase = bPrefix ? ( pData[1] == 'x' ? 16U : 2U ) : 10U;
			const uint64_t nLimit = sirLangIntegerMaximum( eExpected ) + 1;
			uint64_t nValue = 0;
			if ( !sirLangValidInteger( pData, token.nLength, nBase, SIR_LANG_VALUE_U64 ) ) {
				return false;
			}
			for ( uint32_t i = bPrefix ? 2 : 0; i < token.nLength; ++i ) {
				const uint8_t c = pData[i];
				const uint32_t nDigit = c <= '9' ? c - '0' : c <= 'F' ? c - 'A' + 10 : c - 'a' + 10;
				if ( nValue > ( nLimit - nDigit ) / nBase ) {
					return false;
				}
				nValue = nValue * nBase + nDigit;
			}
			return true;
		}
	}
	if ( node.eKind == SIR_LANG_SYNTAX_GROUP ) {
		return sirLangContextualInteger( pState, node.nFirstChild, eExpected );
	}
	if ( node.eKind != SIR_LANG_SYNTAX_LITERAL || node.nStartToken >= pState->pTree->nTokenCount ) {
		return false;
	}
	const sirLangToken_t& token = pState->pTree->pTokens[node.nStartToken];
	if ( token.eKind != SIR_LANG_TOKEN_NUMBER || token.nLength == 0 ) {
		return false;
	}
	const uint8_t *pBytes = pState->pTree->source.pBytes + token.nOffset;
	const bool bPrefix = token.nLength > 2 && pBytes[0] == '0' && ( pBytes[1] == 'x' || pBytes[1] == 'b' );
	const uint32_t nBase = !bPrefix ? 10U : pBytes[1] == 'x' ? 16U : 2U;
	return sirLangValidInteger( pBytes, token.nLength, nBase, eExpected );
}

// Bare decimal fractions may adopt a known floating-point destination width.
// A suffix fixes the literal type and must never be reinterpreted.
static bool sirLangContextualFraction( const sirLangSemanticState_t *pState, uint32_t iNode, sirLangValueKind_t eExpected ) noexcept
{
	if ( iNode >= pState->pTree->nNodeCount || ( eExpected != SIR_LANG_VALUE_F32 && eExpected != SIR_LANG_VALUE_F64 ) ) {
		return false;
	}
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	if ( node.eKind == SIR_LANG_SYNTAX_UNARY && node.nStartToken < pState->pTree->nTokenCount ) {
		const sirLangToken_t& sign = pState->pTree->pTokens[node.nStartToken];
		const uint8_t *pSign = pState->pTree->source.pBytes + sign.nOffset;
		if ( sign.nLength == 1 && ( pSign[0] == '-' || pSign[0] == '+' ) ) {
			return sirLangContextualFraction( pState, node.nFirstChild, eExpected );
		}
	}
	if ( node.eKind == SIR_LANG_SYNTAX_GROUP ) {
		return sirLangContextualFraction( pState, node.nFirstChild, eExpected );
	}
	if ( node.eKind != SIR_LANG_SYNTAX_LITERAL || node.nStartToken >= pState->pTree->nTokenCount ) {
		return false;
	}
	const sirLangToken_t& token = pState->pTree->pTokens[node.nStartToken];
	if ( token.eKind != SIR_LANG_TOKEN_NUMBER || token.nLength == 0 || token.nLength > 96 ) {
		return false;
	}
	const uint8_t *pData = pState->pTree->source.pBytes + token.nOffset;
	bool bFraction = false;
	for ( uint32_t i = 0; i < token.nLength; ++i ) {
		const uint8_t c = pData[i];
		if ( c == '.' || c == 'e' || c == 'E' ) {
			bFraction = true;
		} else if ( ( c < '0' || c > '9' ) && c != '+' && c != '-' ) {
			return false;
		}
	}
	return bFraction;
}

static bool sirLangNumeric( sirLangValueKind_t eValue ) noexcept
{
	return eValue >= SIR_LANG_VALUE_I8 && eValue <= SIR_LANG_VALUE_F64;
}

static bool sirLangCompatible( sirLangValueKind_t eLeft, sirLangValueKind_t eRight ) noexcept
{
	return eLeft == SIR_LANG_VALUE_UNKNOWN || eRight == SIR_LANG_VALUE_UNKNOWN || eLeft == eRight ||
	       ( eLeft == SIR_LANG_VALUE_OTHER && eRight == SIR_LANG_VALUE_OTHER );
}

static bool sirLangBuiltinCall( const sirLangSyntaxTree_t *pTree, uint32_t iToken ) noexcept
{
	static constexpr const char *s_pszBuiltins[] = { "abs",
	                                                 "min",
	                                                 "max",
	                                                 "clamp",
	                                                 "sign",
	                                                 "checked_add",
	                                                 "checked_sub",
	                                                 "checked_mul",
	                                                 "saturating_add",
	                                                 "saturating_sub",
	                                                 "saturating_mul",
	                                                 "wrapping_add",
	                                                 "wrapping_sub",
	                                                 "wrapping_mul",
	                                                 "popcount",
	                                                 "sqrt",
	                                                 "rsqrt",
	                                                 "sin",
	                                                 "cos",
	                                                 "tan",
	                                                 "atan2",
	                                                 "floor",
	                                                 "ceil",
	                                                 "round",
	                                                 "fma",
	                                                 "lerp",
	                                                 "smoothstep",
	                                                 "dot",
	                                                 "cross",
	                                                 "length",
	                                                 "distance",
	                                                 "normalize_checked",
	                                                 "len",
	                                                 "capacity",
	                                                 "contains",
	                                                 "is_empty" };
	for ( uint32_t iWord = 0; iWord < sizeof( s_pszBuiltins ) / sizeof( s_pszBuiltins[0] ); ++iWord ) {
		if ( sirLangEqualToken( pTree, iToken, s_pszBuiltins[iWord] ) ) {
			return true;
		}
	}
	return false;
}

static sirLangValueKind_t sirLangInferExpression( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept;

static sirLangValueKind_t sirLangInferName( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	const uint32_t iToken = pState->pTree->pNodes[iNode].nStartToken;
	if ( sirLangEqualToken( pState->pTree, iToken, "this" ) && pState->iSchema != SIR_LANG_INVALID_INDEX ) {
		return SIR_LANG_VALUE_OTHER;
	}
	sirLangSemanticSymbol_t *pSymbol = sirLangFindSymbol( pState, iToken );
	if ( pSymbol != nullptr ) {
		sirLangRecordReference( pState, iNode, pSymbol->iDeclarationNode, pSymbol->eValue );
		return pSymbol->eValue;
	}
	sirLangValueKind_t eValue = SIR_LANG_VALUE_UNKNOWN;
	if ( sirLangMatchExternal( pState, iToken, SIR_LANG_SYMBOL_FUNCTION, &eValue ) ||
	     sirLangMatchExternal( pState, iToken, SIR_LANG_SYMBOL_LOCAL, &eValue ) || sirLangBuiltinCall( pState->pTree, iToken ) ) {
		sirLangRecordReference( pState, iNode, SIR_LANG_INVALID_INDEX, eValue );
		return eValue;
	}
	sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_UNKNOWN_SYMBOL, iNode );
	return SIR_LANG_VALUE_UNKNOWN;
}

static sirLangValueKind_t sirLangInferMember( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	const uint32_t iChild = node.nFirstChild;
	if ( iChild == SIR_LANG_INVALID_INDEX ) {
		return SIR_LANG_VALUE_UNKNOWN;
	}
	const sirLangSyntaxNode_t& object = pState->pTree->pNodes[iChild];
	if ( object.eKind == SIR_LANG_SYNTAX_NAME && sirLangEqualToken( pState->pTree, object.nStartToken, "this" ) &&
	     pState->iSchema != SIR_LANG_INVALID_INDEX ) {
		const uint32_t iMemberToken = node.nEndToken > 0 ? node.nEndToken - 1 : SIR_LANG_INVALID_INDEX;
		for ( uint32_t iSymbol = 0; iSymbol < pState->pOutput->nSymbolCount; ++iSymbol ) {
			const sirLangSemanticSymbol_t& symbol = pState->pOutput->pSymbols[iSymbol];
			if ( symbol.iScopeNode == pState->iSchema && symbol.eKind == SIR_LANG_SYMBOL_FIELD &&
			     sirLangSameTokens( pState->pTree, symbol.iNameToken, iMemberToken ) ) {
				sirLangRecordReference( pState, iNode, symbol.iDeclarationNode, symbol.eValue );
				return symbol.eValue;
			}
		}
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_UNKNOWN_FIELD, iNode );
		return SIR_LANG_VALUE_UNKNOWN;
	}
	// The host binding registry supplies native member signatures in a later
	// integration stage. Unknown external member types remain conservative.
	sirLangInferExpression( pState, iChild );
	return SIR_LANG_VALUE_UNKNOWN;
}

static sirLangValueKind_t sirLangInferBinary( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	const uint32_t iLeft = node.nFirstChild;
	const uint32_t iRight = sirLangNextChild( pState, iLeft );
	sirLangValueKind_t eLeft = sirLangInferExpression( pState, iLeft );
	sirLangValueKind_t eRight = sirLangInferExpression( pState, iRight );
	if ( eLeft != eRight && sirLangContextualInteger( pState, iRight, eLeft ) ) {
		eRight = eLeft;
	} else if ( eLeft != eRight && sirLangContextualInteger( pState, iLeft, eRight ) ) {
		eLeft = eRight;
	}
	const uint32_t iOperator = iLeft < pState->pTree->nNodeCount ? pState->pTree->pNodes[iLeft].nEndToken : node.nStartToken;
	const bool bLogical = sirLangEqualToken( pState->pTree, iOperator, "and" ) || sirLangEqualToken( pState->pTree, iOperator, "or" ) ||
	                      sirLangEqualToken( pState->pTree, iOperator, "xor" );
	const bool bCompare = sirLangEqualToken( pState->pTree, iOperator, "<" ) || sirLangEqualToken( pState->pTree, iOperator, ">" ) ||
	                      sirLangEqualToken( pState->pTree, iOperator, "=" ) || sirLangEqualToken( pState->pTree, iOperator, "!" );
	if ( ( sirLangContextualInteger( pState, iLeft, SIR_LANG_VALUE_U64 ) && !sirLangContextualInteger( pState, iLeft, eRight ) ) ||
	     ( sirLangContextualInteger( pState, iRight, SIR_LANG_VALUE_U64 ) && !sirLangContextualInteger( pState, iRight, eLeft ) ) ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_LITERAL, iNode );
	}
	if ( !sirLangCompatible( eLeft, eRight ) || ( bLogical && eLeft != SIR_LANG_VALUE_BOOL && eLeft != SIR_LANG_VALUE_UNKNOWN ) ||
	     ( bLogical && eRight != SIR_LANG_VALUE_BOOL && eRight != SIR_LANG_VALUE_UNKNOWN ) ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_TYPE_MISMATCH, iNode );
	}
	if ( bLogical || bCompare ) {
		return SIR_LANG_VALUE_BOOL;
	}
	if ( eLeft != SIR_LANG_VALUE_UNKNOWN && !sirLangNumeric( eLeft ) ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_TYPE_MISMATCH, iNode );
	}
	return eLeft;
}

static sirLangValueKind_t sirLangInferExpression( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	if ( iNode == SIR_LANG_INVALID_INDEX || iNode >= pState->pTree->nNodeCount || !sirLangEnterNode( pState, iNode ) ) {
		return SIR_LANG_VALUE_UNKNOWN;
	}
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	sirLangValueKind_t eValue = SIR_LANG_VALUE_UNKNOWN;
	switch ( node.eKind ) {
	case SIR_LANG_SYNTAX_LITERAL:
		eValue = sirLangLiteralType( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_NAME:
		eValue = sirLangInferName( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_MEMBER:
		eValue = sirLangInferMember( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_BINARY:
		eValue = sirLangInferBinary( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_UNARY:
	case SIR_LANG_SYNTAX_GROUP:
		eValue = sirLangInferExpression( pState, node.nFirstChild );
		if ( node.eKind == SIR_LANG_SYNTAX_UNARY && sirLangEqualToken( pState->pTree, node.nStartToken, "not" ) ) {
			if ( eValue != SIR_LANG_VALUE_UNKNOWN && eValue != SIR_LANG_VALUE_BOOL ) {
				sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_TYPE_MISMATCH, iNode );
			}
			eValue = SIR_LANG_VALUE_BOOL;
		}
		break;
	case SIR_LANG_SYNTAX_CALL: {
		uint32_t iArgument = node.nFirstChild;
		const sirLangValueKind_t eCallee = sirLangInferExpression( pState, iArgument );
		if ( eCallee != SIR_LANG_VALUE_FUNCTION && eCallee != SIR_LANG_VALUE_UNKNOWN ) {
			sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_TYPE_MISMATCH, iNode );
		}
		iArgument = sirLangNextChild( pState, iArgument );
		for ( uint32_t nSteps = 0; iArgument != SIR_LANG_INVALID_INDEX && nSteps < pState->pTree->nNodeCount; ++nSteps ) {
			const sirLangSyntaxNode_t& argument = pState->pTree->pNodes[iArgument];
			if ( argument.eKind == SIR_LANG_SYNTAX_NAMED_ARGUMENT ) {
				sirLangInferExpression( pState, argument.nFirstChild );
			} else {
				sirLangInferExpression( pState, iArgument );
			}
			iArgument = argument.nNextSibling;
		}
		break;
	}
	case SIR_LANG_SYNTAX_INDEX:
		for ( uint32_t iChild = node.nFirstChild, nSteps = 0; iChild != SIR_LANG_INVALID_INDEX && nSteps < pState->pTree->nNodeCount;
		      iChild = sirLangNextChild( pState, iChild ), ++nSteps ) {
			sirLangInferExpression( pState, iChild );
		}
		break;
	default:
		break;
	}
	--pState->nDepth;
	return eValue;
}

static void sirLangCheckDeclarationType( sirLangSemanticState_t *pState, uint32_t iNode, sirLangSemanticSymbol_t *pSymbol ) noexcept
{
	if ( pSymbol == nullptr ) {
		return;
	}
	uint32_t iChild = sirLangFirstChild( pState, iNode );
	for ( uint32_t nSteps = 0; iChild != SIR_LANG_INVALID_INDEX && nSteps < pState->pTree->nNodeCount; ++nSteps ) {
		if ( pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_TYPE ) {
			if ( pSymbol->iTypeNode != iChild ) {
				pSymbol->iTypeNode = iChild;
				pSymbol->eValue = sirLangCheckType( pState, iChild );
			}
			return;
		}
		iChild = sirLangNextChild( pState, iChild );
	}
}

// Schema field declarations are registered before any behavior body is
// analyzed, so type ordering in a module does not affect this.Field lookup.
static void sirLangPredeclareFields( sirLangSemanticState_t *pState, uint32_t iBlock ) noexcept
{
	for ( uint32_t iChild = sirLangFirstChild( pState, iBlock ), nSteps = 0;
	      iChild != SIR_LANG_INVALID_INDEX && nSteps < pState->pTree->nNodeCount; iChild = sirLangNextChild( pState, iChild ), ++nSteps ) {
		const sirLangSyntaxKind_t eKind = pState->pTree->pNodes[iChild].eKind;
		if ( eKind == SIR_LANG_SYNTAX_FIELD ) {
			sirLangSemanticSymbol_t *pSymbol = sirLangDeclare( pState, iChild, { iBlock, SIR_LANG_SYMBOL_FIELD, SIR_LANG_VALUE_BOOL } );
			sirLangCheckDeclarationType( pState, iChild, pSymbol );
		} else if ( eKind == SIR_LANG_SYNTAX_CONDITION_BANK ) {
			for ( uint32_t iFlag = sirLangFirstChild( pState, iChild ), nFlags = 0;
			      iFlag != SIR_LANG_INVALID_INDEX && nFlags < pState->pTree->nNodeCount;
			      iFlag = sirLangNextChild( pState, iFlag ), ++nFlags ) {
				if ( pState->pTree->pNodes[iFlag].eKind == SIR_LANG_SYNTAX_FIELD ) {
					sirLangDeclare( pState, iFlag, { iBlock, SIR_LANG_SYMBOL_FIELD, SIR_LANG_VALUE_BOOL } );
				}
			}
		}
	}
}

static void sirLangPredeclareSchemaFields( sirLangSemanticState_t *pState ) noexcept
{
	for ( uint32_t iSchema = sirLangFirstChild( pState, pState->pTree->nRoot ), nSchemas = 0;
	      iSchema != SIR_LANG_INVALID_INDEX && nSchemas < pState->pTree->nNodeCount;
	      iSchema = sirLangNextChild( pState, iSchema ), ++nSchemas ) {
		if ( pState->pTree->pNodes[iSchema].eKind != SIR_LANG_SYNTAX_SCHEMA ) {
			continue;
		}
		for ( uint32_t iBlock = sirLangFirstChild( pState, iSchema ); iBlock != SIR_LANG_INVALID_INDEX;
		      iBlock = sirLangNextChild( pState, iBlock ) ) {
			if ( pState->pTree->pNodes[iBlock].eKind == SIR_LANG_SYNTAX_BLOCK ) {
				sirLangPredeclareFields( pState, iBlock );
			}
		}
	}
}

static void sirLangAnalyzeNode( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept;

static void sirLangAnalyzeChildren( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	uint32_t iChild = sirLangFirstChild( pState, iNode );
	for ( uint32_t nSteps = 0; iChild != SIR_LANG_INVALID_INDEX && nSteps < pState->pTree->nNodeCount; ++nSteps ) {
		sirLangAnalyzeNode( pState, iChild );
		iChild = sirLangNextChild( pState, iChild );
	}
}

static void sirLangAnalyzeBlock( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	if ( !sirLangPushScope( pState, iNode ) ) {
		return;
	}
	sirLangPredeclare( pState, iNode );
	sirLangAnalyzeChildren( pState, iNode );
	--pState->nScopeDepth;
}

static void sirLangAnalyzeFunction( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	const sirLangValueKind_t ePreviousReturn = pState->eReturnType;
	pState->eReturnType = SIR_LANG_VALUE_VOID;
	if ( !sirLangPushScope( pState, iNode ) ) {
		return;
	}
	uint32_t iChild = sirLangFirstChild( pState, iNode );
	for ( uint32_t nSteps = 0; iChild != SIR_LANG_INVALID_INDEX && nSteps < pState->pTree->nNodeCount; ++nSteps ) {
		const sirLangSyntaxNode_t& child = pState->pTree->pNodes[iChild];
		if ( child.eKind == SIR_LANG_SYNTAX_PARAMETER ) {
			sirLangSemanticSymbol_t *pSymbol =
			    sirLangDeclare( pState, iChild, { iNode, SIR_LANG_SYMBOL_PARAMETER, SIR_LANG_VALUE_UNKNOWN } );
			sirLangCheckDeclarationType( pState, iChild, pSymbol );
		} else if ( child.eKind == SIR_LANG_SYNTAX_TYPE ) {
			pState->eReturnType = sirLangCheckType( pState, iChild );
		}
		iChild = child.nNextSibling;
	}
	for ( iChild = sirLangFirstChild( pState, iNode ); iChild != SIR_LANG_INVALID_INDEX; iChild = sirLangNextChild( pState, iChild ) ) {
		if ( pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_BLOCK ) {
			sirLangAnalyzeBlock( pState, iChild );
		}
	}
	--pState->nScopeDepth;
	pState->eReturnType = ePreviousReturn;
}

static void sirLangAnalyzeLet( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	if ( pState->nScopeDepth == 1 ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_GLOBAL, iNode );
	}
	uint32_t iType = SIR_LANG_INVALID_INDEX;
	uint32_t iExpression = SIR_LANG_INVALID_INDEX;
	for ( uint32_t iChild = node.nFirstChild; iChild != SIR_LANG_INVALID_INDEX; iChild = sirLangNextChild( pState, iChild ) ) {
		if ( pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_TYPE ) {
			iType = iChild;
		} else {
			iExpression = iChild;
		}
	}
	const sirLangValueKind_t eDeclared = iType != SIR_LANG_INVALID_INDEX ? sirLangCheckType( pState, iType ) : SIR_LANG_VALUE_UNKNOWN;
	const sirLangValueKind_t eInferred =
	    iExpression != SIR_LANG_INVALID_INDEX ? sirLangInferExpression( pState, iExpression ) : SIR_LANG_VALUE_UNKNOWN;
	if ( iType != SIR_LANG_INVALID_INDEX && iExpression != SIR_LANG_INVALID_INDEX ) {
		const bool bBareInteger = sirLangContextualInteger( pState, iExpression, SIR_LANG_VALUE_U64 );
		const bool bFits =
		    sirLangContextualInteger( pState, iExpression, eDeclared ) || sirLangContextualFraction( pState, iExpression, eDeclared );
		if ( bBareInteger && !bFits ) {
			sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_LITERAL, iExpression );
		} else if ( !sirLangCompatible( eDeclared, eInferred ) && !bFits ) {
			sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_TYPE_MISMATCH, iNode );
		}
	}
	if ( iType == SIR_LANG_INVALID_INDEX && iExpression != SIR_LANG_INVALID_INDEX && eInferred == SIR_LANG_VALUE_I32 &&
	     sirLangContextualInteger( pState, iExpression, SIR_LANG_VALUE_U64 ) &&
	     !sirLangContextualInteger( pState, iExpression, SIR_LANG_VALUE_I32 ) ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_LITERAL, iExpression );
	}
	const uint32_t iScope = pState->szScopes[pState->nScopeDepth - 1];
	sirLangSemanticSymbol_t *pSymbol =
	    sirLangDeclare( pState, iNode, { iScope, SIR_LANG_SYMBOL_LOCAL, iType == SIR_LANG_INVALID_INDEX ? eInferred : eDeclared } );
	if ( pSymbol != nullptr ) {
		pSymbol->iTypeNode = iType;
	}
}

static void sirLangAnalyzeAssignment( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	const uint32_t iLeft = sirLangFirstChild( pState, iNode );
	const uint32_t iRight = sirLangNextChild( pState, iLeft );
	if ( iRight == SIR_LANG_INVALID_INDEX ) {
		return;
	}
	const sirLangSyntaxKind_t eLeftNode = pState->pTree->pNodes[iLeft].eKind;
	if ( eLeftNode != SIR_LANG_SYNTAX_NAME && eLeftNode != SIR_LANG_SYNTAX_MEMBER && eLeftNode != SIR_LANG_SYNTAX_INDEX ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_ASSIGNMENT, iNode );
	}
	const sirLangValueKind_t eLeft = sirLangInferExpression( pState, iLeft );
	const sirLangValueKind_t eRight = sirLangInferExpression( pState, iRight );
	if ( sirLangContextualInteger( pState, iRight, SIR_LANG_VALUE_U64 ) && !sirLangContextualInteger( pState, iRight, eLeft ) ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_LITERAL, iRight );
	} else if ( !sirLangCompatible( eLeft, eRight ) && !sirLangContextualInteger( pState, iRight, eLeft ) ) {
		sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_TYPE_MISMATCH, iNode );
	}
}

static void sirLangAnalyzeNode( sirLangSemanticState_t *pState, uint32_t iNode ) noexcept
{
	if ( pState->pOutput->bTruncated || iNode >= pState->pTree->nNodeCount || !sirLangEnterNode( pState, iNode ) ) {
		return;
	}
	const sirLangSyntaxNode_t& node = pState->pTree->pNodes[iNode];
	switch ( node.eKind ) {
	case SIR_LANG_SYNTAX_BLOCK:
		sirLangAnalyzeBlock( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_FUNCTION:
	case SIR_LANG_SYNTAX_EVENT:
		sirLangAnalyzeFunction( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_SCHEMA:
	case SIR_LANG_SYNTAX_RECORD:
	case SIR_LANG_SYNTAX_INTERFACE:
	case SIR_LANG_SYNTAX_ENUM:
	case SIR_LANG_SYNTAX_CLASS:
	case SIR_LANG_SYNTAX_BEHAVIOR: {
		const uint32_t iPreviousSchema = pState->iSchema;
		if ( node.eKind == SIR_LANG_SYNTAX_BEHAVIOR || node.eKind == SIR_LANG_SYNTAX_CLASS ) {
			for ( uint32_t iChild = node.nFirstChild; iChild != SIR_LANG_INVALID_INDEX; iChild = sirLangNextChild( pState, iChild ) ) {
				if ( pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_TYPE ) {
					sirLangCheckType( pState, iChild );
					const uint32_t iNameToken = pState->pTree->pNodes[iChild].nStartToken;
					sirLangSemanticSymbol_t *pSchema = sirLangFindTypeDeclaration( pState, iNameToken );
					pState->iSchema = SIR_LANG_INVALID_INDEX;
					if ( pSchema != nullptr && pState->pTree->pNodes[pSchema->iDeclarationNode].eKind == SIR_LANG_SYNTAX_SCHEMA ) {
						for ( uint32_t iFieldBlock = sirLangFirstChild( pState, pSchema->iDeclarationNode );
						      iFieldBlock != SIR_LANG_INVALID_INDEX; iFieldBlock = sirLangNextChild( pState, iFieldBlock ) ) {
							if ( pState->pTree->pNodes[iFieldBlock].eKind == SIR_LANG_SYNTAX_BLOCK ) {
								pState->iSchema = iFieldBlock;
								break;
							}
						}
					}
				}
			}
		}
		for ( uint32_t iChild = node.nFirstChild; iChild != SIR_LANG_INVALID_INDEX; iChild = sirLangNextChild( pState, iChild ) ) {
			if ( pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_BLOCK ) {
				sirLangAnalyzeBlock( pState, iChild );
			}
		}
		pState->iSchema = iPreviousSchema;
		break;
	}
	case SIR_LANG_SYNTAX_CONDITION_BANK:
		for ( uint32_t iChild = node.nFirstChild; iChild != SIR_LANG_INVALID_INDEX; iChild = sirLangNextChild( pState, iChild ) ) {
			if ( pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_TYPE ) {
				sirLangCheckType( pState, iChild );
			} else if ( pState->pTree->pNodes[iChild].eKind == SIR_LANG_SYNTAX_FIELD ) {
				sirLangDeclare( pState, iChild, { pState->szScopes[pState->nScopeDepth - 1], SIR_LANG_SYMBOL_FIELD, SIR_LANG_VALUE_BOOL } );
			}
		}
		break;
	case SIR_LANG_SYNTAX_FIELD: {
		const uint32_t iScope = pState->szScopes[pState->nScopeDepth - 1];
		sirLangSemanticSymbol_t *pSymbol = sirLangDeclare( pState, iNode, { iScope, SIR_LANG_SYMBOL_FIELD, SIR_LANG_VALUE_BOOL } );
		sirLangCheckDeclarationType( pState, iNode, pSymbol );
		break;
	}
	case SIR_LANG_SYNTAX_LET:
		sirLangAnalyzeLet( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_ASSIGNMENT:
		sirLangAnalyzeAssignment( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_RETURN: {
		const uint32_t iChild = node.nFirstChild;
		const sirLangValueKind_t eActual =
		    iChild != SIR_LANG_INVALID_INDEX ? sirLangInferExpression( pState, iChild ) : SIR_LANG_VALUE_VOID;
		const bool bBareInteger = sirLangContextualInteger( pState, iChild, SIR_LANG_VALUE_U64 );
		const bool bFits = sirLangContextualInteger( pState, iChild, pState->eReturnType ) ||
		                   sirLangContextualFraction( pState, iChild, pState->eReturnType );
		if ( bBareInteger && !bFits ) {
			sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_LITERAL, iChild );
		} else if ( !sirLangCompatible( pState->eReturnType, eActual ) && !bFits ) {
			sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_RETURN, iNode );
		}
		break;
	}
	case SIR_LANG_SYNTAX_WHEN:
	case SIR_LANG_SYNTAX_FOR:
	case SIR_LANG_SYNTAX_MUTATE: {
		const bool bLoop = node.eKind == SIR_LANG_SYNTAX_FOR;
		if ( bLoop && !sirLangPushScope( pState, iNode ) ) {
			break;
		}
		if ( bLoop ) {
			sirLangDeclare( pState, iNode, { iNode, SIR_LANG_SYMBOL_LOCAL, SIR_LANG_VALUE_UNKNOWN } );
		}
		const uint32_t iChild = node.nFirstChild;
		const sirLangValueKind_t eCondition = sirLangInferExpression( pState, iChild );
		if ( node.eKind == SIR_LANG_SYNTAX_WHEN && eCondition != SIR_LANG_VALUE_BOOL && eCondition != SIR_LANG_VALUE_UNKNOWN ) {
			sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_TYPE_MISMATCH, iNode );
		}
		for ( uint32_t iOther = sirLangNextChild( pState, iChild ); iOther != SIR_LANG_INVALID_INDEX;
		      iOther = sirLangNextChild( pState, iOther ) ) {
			sirLangAnalyzeNode( pState, iOther );
		}
		if ( bLoop ) {
			--pState->nScopeDepth;
		}
		break;
	}
	case SIR_LANG_SYNTAX_OTHERWISE:
		sirLangAnalyzeChildren( pState, iNode );
		break;
	case SIR_LANG_SYNTAX_SWITCH: {
		const uint32_t iSelector = node.nFirstChild;
		const sirLangValueKind_t eSelector = sirLangInferExpression( pState, iSelector );
		for ( uint32_t iCase = sirLangNextChild( pState, iSelector ); iCase != SIR_LANG_INVALID_INDEX;
		      iCase = sirLangNextChild( pState, iCase ) ) {
			const sirLangSyntaxNode_t& branch = pState->pTree->pNodes[iCase];
			uint32_t iBlock = branch.nFirstChild;
			if ( branch.eKind == SIR_LANG_SYNTAX_CASE ) {
				const sirLangValueKind_t eValue = sirLangInferExpression( pState, iBlock );
				const bool bBareInteger = sirLangContextualInteger( pState, iBlock, SIR_LANG_VALUE_U64 );
				const bool bFits = sirLangContextualInteger( pState, iBlock, eSelector );
				if ( bBareInteger && !bFits ) {
					sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_INVALID_LITERAL, iBlock );
				} else if ( !sirLangCompatible( eSelector, eValue ) && !bFits ) {
					sirLangSemanticReport( pState, SIR_LANG_SEMANTIC_TYPE_MISMATCH, iCase );
				}
				iBlock = sirLangNextChild( pState, iBlock );
			}
			if ( iBlock != SIR_LANG_INVALID_INDEX ) {
				sirLangAnalyzeNode( pState, iBlock );
			}
		}
		break;
	} break;
	case SIR_LANG_SYNTAX_EXPRESSION_STATEMENT:
	case SIR_LANG_SYNTAX_EMIT:
	case SIR_LANG_SYNTAX_AFTER:
	case SIR_LANG_SYNTAX_SPAWN:
	case SIR_LANG_SYNTAX_DESTROY:
	case SIR_LANG_SYNTAX_TRANSITION:
		for ( uint32_t iChild = node.nFirstChild; iChild != SIR_LANG_INVALID_INDEX; iChild = sirLangNextChild( pState, iChild ) ) {
			sirLangInferExpression( pState, iChild );
		}
		break;
	default:
		break;
	}
	--pState->nDepth;
}

static bool sirLangValidChildChains( const sirLangSyntaxTree_t *pTree ) noexcept
{
	for ( uint32_t iParent = 0; iParent < pTree->nNodeCount; ++iParent ) {
		const sirLangSyntaxNode_t& parent = pTree->pNodes[iParent];
		if ( ( parent.nFirstChild == SIR_LANG_INVALID_INDEX ) != ( parent.nLastChild == SIR_LANG_INVALID_INDEX ) ) {
			return false;
		}
		uint32_t iChild = parent.nFirstChild;
		uint32_t iLast = SIR_LANG_INVALID_INDEX;
		uint32_t nVisited = 0;
		while ( iChild != SIR_LANG_INVALID_INDEX && nVisited < pTree->nNodeCount ) {
			if ( iChild >= pTree->nNodeCount || iChild == iParent ) {
				return false;
			}
			iLast = iChild;
			iChild = pTree->pNodes[iChild].nNextSibling;
			++nVisited;
		}
		if ( iChild != SIR_LANG_INVALID_INDEX || iLast != parent.nLastChild ) {
			return false;
		}
	}
	return true;
}

sirLangSemanticError_t SIRLangAnalyze( const sirLangSyntaxTree_t *pTree, sirLangSemanticOutput_t *pOutput ) noexcept
{
	if ( pTree == nullptr || pOutput == nullptr || pTree->pTokens == nullptr || pTree->pNodes == nullptr || pTree->nTokenCount == 0 ||
	     pTree->nNodeCount == 0 || pTree->nRoot >= pTree->nNodeCount || ( pTree->source.nBytes > 0 && pTree->source.pBytes == nullptr ) ||
	     pTree->pDiagnostics == nullptr || pTree->nDiagnosticCount != 0 || pTree->bTruncated || pOutput->pSymbols == nullptr ||
	     pOutput->nSymbolCapacity == 0 || pOutput->pDiagnostics == nullptr || pOutput->nDiagnosticCapacity == 0 ||
	     pOutput->pReferences == nullptr || pOutput->nReferenceCapacity == 0 ||
	     ( pOutput->nExternalCount > 0 && pOutput->pExternals == nullptr ) ) {
		return SIR_LANG_SEMANTIC_INVALID_INPUT;
	}
	pOutput->nSymbolCount = 0;
	pOutput->nDiagnosticCount = 0;
	pOutput->nResolvedReferences = 0;
	pOutput->nReferenceCount = 0;
	pOutput->bTruncated = false;
	for ( uint32_t iToken = 0; iToken < pTree->nTokenCount; ++iToken ) {
		const sirLangToken_t& token = pTree->pTokens[iToken];
		if ( token.nOffset > pTree->source.nBytes || token.nLength > pTree->source.nBytes - token.nOffset ||
		     token.eKind == SIR_LANG_TOKEN_ERROR ) {
			return SIR_LANG_SEMANTIC_INVALID_TREE;
		}
	}
	for ( uint32_t iNode = 0; iNode < pTree->nNodeCount; ++iNode ) {
		const sirLangSyntaxNode_t& node = pTree->pNodes[iNode];
		if ( node.nStartToken > node.nEndToken || node.nEndToken > pTree->nTokenCount ||
		     ( node.nFirstChild != SIR_LANG_INVALID_INDEX && node.nFirstChild >= pTree->nNodeCount ) ||
		     ( node.nNextSibling != SIR_LANG_INVALID_INDEX && node.nNextSibling >= pTree->nNodeCount ) ) {
			return SIR_LANG_SEMANTIC_INVALID_TREE;
		}
	}
	if ( pTree->pNodes[pTree->nRoot].eKind != SIR_LANG_SYNTAX_MODULE || !sirLangValidChildChains( pTree ) ) {
		return SIR_LANG_SEMANTIC_INVALID_TREE;
	}
	sirLangSemanticState_t state = {};
	state.pTree = pTree;
	state.pOutput = pOutput;
	state.eFirstError = SIR_LANG_SEMANTIC_OK;
	state.eReturnType = SIR_LANG_VALUE_VOID;
	state.iSchema = SIR_LANG_INVALID_INDEX;
	if ( !sirLangPushScope( &state, pTree->nRoot ) ) {
		return state.eFirstError;
	}
	sirLangPredeclare( &state, pTree->nRoot );
	sirLangPredeclareSchemaFields( &state );
	sirLangAnalyzeChildren( &state, pTree->nRoot );
	return state.eFirstError;
}
