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

#include <Scripting/Language/SIRLangParser.h>
#include <Scripting/Language/SIRLangLexer.h>
#include <limits.h>
#include <string.h>

typedef struct sirLangParserState_s
{
	sirLangSyntaxTree_t *pTree;
	uint32_t iToken;
	uint32_t nDepth;
	sirLangParseError_t eFirstError;
} sirLangParserState_t;

static constexpr uint32_t SIR_LANG_NO_NODE = UINT32_MAX;
static constexpr uint32_t SIR_LANG_MAX_PARSE_DEPTH = 64;

typedef struct sirLangKeywordSyntax_s
{
	const char *pszText;
	sirLangSyntaxKind_t eKind;
} sirLangKeywordSyntax_t;

static constexpr sirLangKeywordSyntax_t s_szHeaderKeywords[] = {
    { "schema", SIR_LANG_SYNTAX_SCHEMA },       { "behavior", SIR_LANG_SYNTAX_BEHAVIOR },       { "class", SIR_LANG_SYNTAX_CLASS },
    { "interface", SIR_LANG_SYNTAX_INTERFACE }, { "record", SIR_LANG_SYNTAX_RECORD },           { "enum", SIR_LANG_SYNTAX_ENUM },
    { "fn", SIR_LANG_SYNTAX_FUNCTION },         { "function", SIR_LANG_SYNTAX_FUNCTION },       { "on", SIR_LANG_SYNTAX_EVENT },
    { "state", SIR_LANG_SYNTAX_STATE },         { "condition", SIR_LANG_SYNTAX_CONDITION_BANK } };
static constexpr sirLangKeywordSyntax_t s_szSimpleKeywords[] = {
    { "return", SIR_LANG_SYNTAX_RETURN }, { "emit", SIR_LANG_SYNTAX_EMIT },        { "after", SIR_LANG_SYNTAX_AFTER },
    { "spawn", SIR_LANG_SYNTAX_SPAWN },   { "destroy", SIR_LANG_SYNTAX_DESTROY },  { "transition", SIR_LANG_SYNTAX_TRANSITION },
    { "break", SIR_LANG_SYNTAX_BREAK },   { "continue", SIR_LANG_SYNTAX_CONTINUE } };

static const sirLangToken_t *sirLangCurrent( const sirLangParserState_t *pState ) noexcept
{
	const uint32_t iToken = pState->iToken < pState->pTree->nTokenCount ? pState->iToken : pState->pTree->nTokenCount - 1;
	return &pState->pTree->pTokens[iToken];
}

static const sirLangToken_t *sirLangLookahead( const sirLangParserState_t *pState, uint32_t nAhead ) noexcept
{
	const uint64_t nIndex = ( uint64_t )pState->iToken + nAhead;
	const uint32_t iToken = nIndex < pState->pTree->nTokenCount ? ( uint32_t )nIndex : pState->pTree->nTokenCount - 1;
	return &pState->pTree->pTokens[iToken];
}

static bool sirLangTokenText( const sirLangParserState_t *pState, uint32_t nAhead, const char *pszText ) noexcept
{
	const sirLangToken_t *pToken = sirLangLookahead( pState, nAhead );
	const uint32_t nLength = ( uint32_t )strlen( pszText );
	return pToken->nLength == nLength && pToken->eKind != SIR_LANG_TOKEN_END &&
	       memcmp( pState->pTree->source.pBytes + pToken->nOffset, pszText, nLength ) == 0;
}

static bool sirLangAt( const sirLangParserState_t *pState, sirLangTokenKind_t eKind ) noexcept
{
	return sirLangCurrent( pState )->eKind == eKind;
}

static void sirLangAdvance( sirLangParserState_t *pState ) noexcept
{
	if ( !sirLangAt( pState, SIR_LANG_TOKEN_END ) ) {
		++pState->iToken;
	}
}

static bool sirLangTake( sirLangParserState_t *pState, const char *pszText ) noexcept
{
	if ( !sirLangTokenText( pState, 0, pszText ) ) {
		return false;
	}
	sirLangAdvance( pState );
	return true;
}

static bool sirLangTakeKind( sirLangParserState_t *pState, sirLangTokenKind_t eKind ) noexcept
{
	if ( !sirLangAt( pState, eKind ) ) {
		return false;
	}
	sirLangAdvance( pState );
	return true;
}

static void sirLangReport( sirLangParserState_t *pState, sirLangParseError_t eError ) noexcept
{
	if ( pState->eFirstError == SIR_LANG_PARSE_OK ) {
		pState->eFirstError = eError;
	}
	sirLangSyntaxTree_t *pTree = pState->pTree;
	if ( pTree->nDiagnosticCount >= pTree->nDiagnosticCapacity ) {
		pTree->bTruncated = true;
		return;
	}
	const sirLangToken_t *pToken = sirLangCurrent( pState );
	pTree->pDiagnostics[pTree->nDiagnosticCount++] = {
	    eError,          pToken->eKind == SIR_LANG_TOKEN_ERROR ? pToken->eError : SIR_LANG_LEX_OK,
	    pToken->nOffset, pToken->nLength,
	    pToken->nLine,   pToken->nColumn };
}

static uint32_t sirLangNewNode( sirLangParserState_t *pState, sirLangSyntaxKind_t eKind, uint32_t iStart ) noexcept
{
	sirLangSyntaxTree_t *pTree = pState->pTree;
	if ( pTree->nNodeCount >= pTree->nNodeCapacity ) {
		pTree->bTruncated = true;
		if ( pState->eFirstError == SIR_LANG_PARSE_OK ) {
			pState->eFirstError = SIR_LANG_PARSE_CAPACITY;
		}
		return SIR_LANG_NO_NODE;
	}
	const uint32_t iNode = pTree->nNodeCount++;
	pTree->pNodes[iNode] = { eKind, iStart, iStart, SIR_LANG_NO_NODE, SIR_LANG_NO_NODE, SIR_LANG_NO_NODE };
	return iNode;
}

static void sirLangFinishNode( sirLangParserState_t *pState, uint32_t iNode ) noexcept
{
	if ( iNode != SIR_LANG_NO_NODE ) {
		pState->pTree->pNodes[iNode].nEndToken = pState->iToken;
	}
}

static void sirLangAppend( sirLangParserState_t *pState, uint32_t iParent, uint32_t iChild ) noexcept
{
	if ( iParent == SIR_LANG_NO_NODE || iChild == SIR_LANG_NO_NODE ) {
		return;
	}
	sirLangSyntaxNode_t *pParent = &pState->pTree->pNodes[iParent];
	if ( pParent->nLastChild == SIR_LANG_NO_NODE ) {
		pParent->nFirstChild = iChild;
	} else {
		pState->pTree->pNodes[pParent->nLastChild].nNextSibling = iChild;
	}
	pParent->nLastChild = iChild;
}

static void sirLangRecoverLine( sirLangParserState_t *pState ) noexcept
{
	while ( !sirLangAt( pState, SIR_LANG_TOKEN_END ) && !sirLangAt( pState, SIR_LANG_TOKEN_DEDENT ) &&
	        !sirLangAt( pState, SIR_LANG_TOKEN_NEWLINE ) ) {
		sirLangAdvance( pState );
	}
	sirLangTakeKind( pState, SIR_LANG_TOKEN_NEWLINE );
}

static bool sirLangTakeName( sirLangParserState_t *pState ) noexcept
{
	if ( !sirLangAt( pState, SIR_LANG_TOKEN_IDENTIFIER ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_NAME );
		return false;
	}
	sirLangAdvance( pState );
	return true;
}

static bool sirLangTakeDottedName( sirLangParserState_t *pState ) noexcept
{
	if ( !sirLangTakeName( pState ) ) {
		return false;
	}
	while ( sirLangTake( pState, "." ) ) {
		if ( !sirLangTakeName( pState ) ) {
			return false;
		}
	}
	return true;
}

static uint32_t sirLangParseType( sirLangParserState_t *pState ) noexcept;
static uint32_t sirLangParseExpression( sirLangParserState_t *pState, uint32_t nMinPrecedence ) noexcept;
static uint32_t sirLangParseStatement( sirLangParserState_t *pState ) noexcept;

static bool sirLangEnter( sirLangParserState_t *pState ) noexcept
{
	if ( pState->nDepth >= SIR_LANG_MAX_PARSE_DEPTH ) {
		sirLangReport( pState, SIR_LANG_PARSE_NESTING_LIMIT );
		return false;
	}
	++pState->nDepth;
	return true;
}

static void sirLangExit( sirLangParserState_t *pState ) noexcept
{
	--pState->nDepth;
}

static uint32_t sirLangParseType( sirLangParserState_t *pState ) noexcept
{
	const uint32_t iStart = pState->iToken;
	if ( !sirLangEnter( pState ) ) {
		return SIR_LANG_NO_NODE;
	}
	const uint32_t iType = sirLangNewNode( pState, SIR_LANG_SYNTAX_TYPE, iStart );
	if ( sirLangTake( pState, "[" ) ) {
		sirLangAppend( pState, iType, sirLangParseType( pState ) );
		if ( !sirLangTake( pState, ";" ) ) {
			sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
		}
		if ( !sirLangAt( pState, SIR_LANG_TOKEN_NUMBER ) ) {
			sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
		} else {
			sirLangAdvance( pState );
		}
		if ( !sirLangTake( pState, "]" ) ) {
			sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
		}
	} else if ( sirLangAt( pState, SIR_LANG_TOKEN_IDENTIFIER ) || sirLangTokenText( pState, 0, "fn" ) ) {
		sirLangAdvance( pState );
		while ( sirLangTake( pState, "." ) ) {
			sirLangTakeName( pState );
		}
		if ( sirLangTake( pState, "<" ) ) {
			if ( !sirLangTokenText( pState, 0, ">" ) ) {
				for ( ;; ) {
					if ( sirLangAt( pState, SIR_LANG_TOKEN_NUMBER ) ) {
						sirLangAdvance( pState );
					} else {
						sirLangAppend( pState, iType, sirLangParseType( pState ) );
					}
					if ( !sirLangTake( pState, "," ) ) {
						break;
					}
				}
			}
			if ( !sirLangTake( pState, ">" ) ) {
				sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
			}
		}
	} else {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TYPE );
	}
	sirLangFinishNode( pState, iType );
	sirLangExit( pState );
	return iType;
}

static uint32_t sirLangParseArguments( sirLangParserState_t *pState, uint32_t iCall ) noexcept
{
	if ( !sirLangTake( pState, "(" ) ) {
		return iCall;
	}
	if ( !sirLangTokenText( pState, 0, ")" ) ) {
		for ( ;; ) {
			const uint32_t iBefore = pState->iToken;
			if ( sirLangAt( pState, SIR_LANG_TOKEN_IDENTIFIER ) && sirLangTokenText( pState, 1, ":" ) ) {
				const uint32_t iNamed = sirLangNewNode( pState, SIR_LANG_SYNTAX_NAMED_ARGUMENT, pState->iToken );
				sirLangAdvance( pState );
				sirLangAdvance( pState );
				sirLangAppend( pState, iNamed, sirLangParseExpression( pState, 1 ) );
				sirLangFinishNode( pState, iNamed );
				sirLangAppend( pState, iCall, iNamed );
			} else {
				sirLangAppend( pState, iCall, sirLangParseExpression( pState, 1 ) );
			}
			if ( pState->iToken == iBefore || !sirLangTake( pState, "," ) ) {
				break;
			}
		}
	}
	if ( !sirLangTake( pState, ")" ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
	}
	return iCall;
}

static uint32_t sirLangParseAtom( sirLangParserState_t *pState ) noexcept
{
	const uint32_t iStart = pState->iToken;
	uint32_t iNode = SIR_LANG_NO_NODE;
	if ( sirLangTake( pState, "(" ) ) {
		iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_GROUP, iStart );
		sirLangAppend( pState, iNode, sirLangParseExpression( pState, 1 ) );
		if ( !sirLangTake( pState, ")" ) ) {
			sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
		}
	} else if ( sirLangTokenText( pState, 0, "not" ) || sirLangTokenText( pState, 0, "-" ) || sirLangTokenText( pState, 0, "+" ) ||
	            sirLangTokenText( pState, 0, "!" ) ) {
		iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_UNARY, iStart );
		sirLangAdvance( pState );
		sirLangAppend( pState, iNode, sirLangParseExpression( pState, 8 ) );
	} else if ( sirLangAt( pState, SIR_LANG_TOKEN_IDENTIFIER ) || sirLangTokenText( pState, 0, "self" ) ) {
		iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_NAME, iStart );
		sirLangAdvance( pState );
	} else if ( sirLangAt( pState, SIR_LANG_TOKEN_NUMBER ) || sirLangAt( pState, SIR_LANG_TOKEN_STRING ) ||
	            sirLangTokenText( pState, 0, "true" ) || sirLangTokenText( pState, 0, "false" ) || sirLangTokenText( pState, 0, "ON" ) ||
	            sirLangTokenText( pState, 0, "OFF" ) ) {
		iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_LITERAL, iStart );
		sirLangAdvance( pState );
	} else {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_EXPRESSION );
		return SIR_LANG_NO_NODE;
	}
	sirLangFinishNode( pState, iNode );
	while ( !pState->pTree->bTruncated ) {
		if ( sirLangTokenText( pState, 0, "." ) ) {
			const uint32_t iParent = sirLangNewNode( pState, SIR_LANG_SYNTAX_MEMBER, iStart );
			sirLangAdvance( pState );
			sirLangTakeName( pState );
			sirLangAppend( pState, iParent, iNode );
			iNode = iParent;
		} else if ( sirLangTokenText( pState, 0, "(" ) ) {
			const uint32_t iParent = sirLangNewNode( pState, SIR_LANG_SYNTAX_CALL, iStart );
			sirLangAppend( pState, iParent, iNode );
			sirLangParseArguments( pState, iParent );
			iNode = iParent;
		} else if ( sirLangTake( pState, "[" ) ) {
			const uint32_t iParent = sirLangNewNode( pState, SIR_LANG_SYNTAX_INDEX, iStart );
			sirLangAppend( pState, iParent, iNode );
			sirLangAppend( pState, iParent, sirLangParseExpression( pState, 1 ) );
			if ( !sirLangTake( pState, "]" ) ) {
				sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
			}
			iNode = iParent;
		} else {
			break;
		}
		sirLangFinishNode( pState, iNode );
	}
	return iNode;
}

static uint32_t sirLangBinaryPrecedence( const sirLangParserState_t *pState ) noexcept
{
	if ( sirLangTokenText( pState, 0, "or" ) ) {
		return 1;
	}
	if ( sirLangTokenText( pState, 0, "xor" ) ) {
		return 2;
	}
	if ( sirLangTokenText( pState, 0, "and" ) ) {
		return 3;
	}
	if ( sirLangTokenText( pState, 1, "=" ) && ( sirLangTokenText( pState, 0, "=" ) || sirLangTokenText( pState, 0, "!" ) ) ) {
		return 4;
	}
	if ( sirLangTokenText( pState, 0, "<" ) || sirLangTokenText( pState, 0, ">" ) ) {
		return 5;
	}
	if ( sirLangTokenText( pState, 0, "+" ) || sirLangTokenText( pState, 0, "-" ) ) {
		return 6;
	}
	if ( sirLangTokenText( pState, 0, "*" ) || sirLangTokenText( pState, 0, "/" ) || sirLangTokenText( pState, 0, "%" ) ) {
		return 7;
	}
	return 0;
}

static uint32_t sirLangParseExpression( sirLangParserState_t *pState, uint32_t nMinPrecedence ) noexcept
{
	if ( !sirLangEnter( pState ) ) {
		return SIR_LANG_NO_NODE;
	}
	const uint32_t iStart = pState->iToken;
	uint32_t iLeft = sirLangParseAtom( pState );
	while ( iLeft != SIR_LANG_NO_NODE && !pState->pTree->bTruncated ) {
		const uint32_t nPrecedence = sirLangBinaryPrecedence( pState );
		if ( nPrecedence == 0 || nPrecedence < nMinPrecedence ) {
			break;
		}
		const bool bCompoundOperator =
		    sirLangTokenText( pState, 1, "=" ) && ( sirLangTokenText( pState, 0, "=" ) || sirLangTokenText( pState, 0, "!" ) ||
			                                        sirLangTokenText( pState, 0, "<" ) || sirLangTokenText( pState, 0, ">" ) );
		const uint32_t iParent = sirLangNewNode( pState, SIR_LANG_SYNTAX_BINARY, iStart );
		sirLangAdvance( pState );
		if ( bCompoundOperator ) {
			sirLangAdvance( pState );
		}
		sirLangAppend( pState, iParent, iLeft );
		sirLangAppend( pState, iParent, sirLangParseExpression( pState, nPrecedence + 1 ) );
		sirLangFinishNode( pState, iParent );
		iLeft = iParent;
	}
	sirLangExit( pState );
	return iLeft;
}

static uint32_t sirLangParseParameter( sirLangParserState_t *pState ) noexcept
{
	const uint32_t iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_PARAMETER, pState->iToken );
	sirLangTakeName( pState );
	if ( sirLangTake( pState, ":" ) ) {
		sirLangAppend( pState, iNode, sirLangParseType( pState ) );
	} else {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_COLON );
	}
	sirLangFinishNode( pState, iNode );
	return iNode;
}

static void sirLangParseParameters( sirLangParserState_t *pState, uint32_t iParent ) noexcept
{
	if ( !sirLangTake( pState, "(" ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
		return;
	}
	if ( !sirLangTokenText( pState, 0, ")" ) ) {
		for ( ;; ) {
			const uint32_t iBefore = pState->iToken;
			sirLangAppend( pState, iParent, sirLangParseParameter( pState ) );
			if ( pState->iToken == iBefore || !sirLangTake( pState, "," ) ) {
				break;
			}
		}
	}
	if ( !sirLangTake( pState, ")" ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
	}
}

static uint32_t sirLangParseBlock( sirLangParserState_t *pState ) noexcept
{
	const uint32_t iBlock = sirLangNewNode( pState, SIR_LANG_SYNTAX_BLOCK, pState->iToken );
	if ( !sirLangTake( pState, ":" ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_COLON );
		sirLangRecoverLine( pState );
		sirLangFinishNode( pState, iBlock );
		return iBlock;
	}
	if ( !sirLangTakeKind( pState, SIR_LANG_TOKEN_NEWLINE ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_NEWLINE );
		sirLangRecoverLine( pState );
	}
	if ( !sirLangTakeKind( pState, SIR_LANG_TOKEN_INDENT ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_INDENT );
		sirLangFinishNode( pState, iBlock );
		return iBlock;
	}
	if ( !sirLangEnter( pState ) ) {
		sirLangRecoverLine( pState );
	} else {
		while ( !sirLangAt( pState, SIR_LANG_TOKEN_END ) && !sirLangAt( pState, SIR_LANG_TOKEN_DEDENT ) && !pState->pTree->bTruncated ) {
			const uint32_t iBefore = pState->iToken;
			const uint32_t iStatement = sirLangParseStatement( pState );
			sirLangAppend( pState, iBlock, iStatement );
			if ( pState->iToken == iBefore ) {
				sirLangReport( pState, SIR_LANG_PARSE_UNEXPECTED_TOKEN );
				sirLangRecoverLine( pState );
				if ( pState->iToken == iBefore ) {
					sirLangAdvance( pState );
				}
			}
		}
		sirLangExit( pState );
	}
	sirLangTakeKind( pState, SIR_LANG_TOKEN_DEDENT );
	sirLangFinishNode( pState, iBlock );
	return iBlock;
}

static uint32_t sirLangParseHeader( sirLangParserState_t *pState, sirLangSyntaxKind_t eKind ) noexcept
{
	const uint32_t iNode = sirLangNewNode( pState, eKind, pState->iToken );
	sirLangAdvance( pState );
	if ( eKind == SIR_LANG_SYNTAX_CONDITION_BANK ) {
		if ( !sirLangTake( pState, "bank" ) ) {
			sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
		}
	}
	sirLangTakeName( pState );
	if ( sirLangTake( pState, "uses" ) ) {
		sirLangAppend( pState, iNode, sirLangParseType( pState ) );
	}
	if ( eKind == SIR_LANG_SYNTAX_FUNCTION || eKind == SIR_LANG_SYNTAX_EVENT ) {
		if ( eKind == SIR_LANG_SYNTAX_FUNCTION || sirLangTokenText( pState, 0, "(" ) ) {
			sirLangParseParameters( pState, iNode );
		}
		if ( sirLangTake( pState, "-" ) ) {
			if ( !sirLangTake( pState, ">" ) ) {
				sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
			}
			sirLangAppend( pState, iNode, sirLangParseType( pState ) );
		}
	}
	if ( eKind == SIR_LANG_SYNTAX_CONDITION_BANK ) {
		if ( sirLangTake( pState, ":" ) ) {
			sirLangAppend( pState, iNode, sirLangParseType( pState ) );
		}
		// A bank may include nested named condition declarations.
		if ( sirLangTakeKind( pState, SIR_LANG_TOKEN_NEWLINE ) && sirLangTakeKind( pState, SIR_LANG_TOKEN_INDENT ) ) {
			while ( !sirLangAt( pState, SIR_LANG_TOKEN_DEDENT ) && !sirLangAt( pState, SIR_LANG_TOKEN_END ) &&
			        !pState->pTree->bTruncated ) {
				const uint32_t iFlag = sirLangNewNode( pState, SIR_LANG_SYNTAX_FIELD, pState->iToken );
				sirLangTakeName( pState );
				sirLangRecoverLine( pState );
				sirLangFinishNode( pState, iFlag );
				sirLangAppend( pState, iNode, iFlag );
			}
			sirLangTakeKind( pState, SIR_LANG_TOKEN_DEDENT );
		}
	} else {
		sirLangAppend( pState, iNode, sirLangParseBlock( pState ) );
	}
	sirLangFinishNode( pState, iNode );
	return iNode;
}

static uint32_t sirLangParseField( sirLangParserState_t *pState ) noexcept
{
	const uint32_t iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_FIELD, pState->iToken );
	sirLangAdvance( pState );
	sirLangTakeName( pState );
	if ( !sirLangTake( pState, ":" ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_COLON );
	} else {
		sirLangAppend( pState, iNode, sirLangParseType( pState ) );
	}
	sirLangRecoverLine( pState );
	sirLangFinishNode( pState, iNode );
	return iNode;
}

static uint32_t sirLangParseLet( sirLangParserState_t *pState ) noexcept
{
	const uint32_t iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_LET, pState->iToken );
	sirLangAdvance( pState );
	sirLangTakeName( pState );
	if ( sirLangTake( pState, ":" ) ) {
		sirLangAppend( pState, iNode, sirLangParseType( pState ) );
	}
	if ( sirLangTake( pState, "=" ) ) {
		sirLangAppend( pState, iNode, sirLangParseExpression( pState, 1 ) );
	}
	if ( !sirLangTakeKind( pState, SIR_LANG_TOKEN_NEWLINE ) && !sirLangAt( pState, SIR_LANG_TOKEN_END ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_NEWLINE );
		sirLangRecoverLine( pState );
	}
	sirLangFinishNode( pState, iNode );
	return iNode;
}

static uint32_t sirLangParseExpressionLine( sirLangParserState_t *pState, sirLangSyntaxKind_t eKind ) noexcept
{
	const uint32_t iNode = sirLangNewNode( pState, eKind, pState->iToken );
	sirLangAdvance( pState );
	if ( !sirLangAt( pState, SIR_LANG_TOKEN_NEWLINE ) && !sirLangAt( pState, SIR_LANG_TOKEN_END ) ) {
		sirLangAppend( pState, iNode, sirLangParseExpression( pState, 1 ) );
	}
	if ( eKind == SIR_LANG_SYNTAX_SPAWN && sirLangTake( pState, "at" ) ) {
		sirLangAppend( pState, iNode, sirLangParseExpression( pState, 1 ) );
	}
	if ( eKind == SIR_LANG_SYNTAX_AFTER && sirLangTake( pState, "emit" ) ) {
		sirLangAppend( pState, iNode, sirLangParseExpression( pState, 1 ) );
	}
	if ( !sirLangTakeKind( pState, SIR_LANG_TOKEN_NEWLINE ) && !sirLangAt( pState, SIR_LANG_TOKEN_END ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_NEWLINE );
		sirLangRecoverLine( pState );
	}
	sirLangFinishNode( pState, iNode );
	return iNode;
}

static uint32_t sirLangParseConditional( sirLangParserState_t *pState, sirLangSyntaxKind_t eKind ) noexcept
{
	const uint32_t iNode = sirLangNewNode( pState, eKind, pState->iToken );
	sirLangAdvance( pState );
	if ( eKind == SIR_LANG_SYNTAX_FOR ) {
		sirLangTakeName( pState );
		if ( !sirLangTake( pState, "in" ) ) {
			sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_TOKEN );
		}
	}
	if ( eKind != SIR_LANG_SYNTAX_OTHERWISE ) {
		sirLangAppend( pState, iNode, sirLangParseExpression( pState, 1 ) );
	}
	sirLangAppend( pState, iNode, sirLangParseBlock( pState ) );
	if ( eKind == SIR_LANG_SYNTAX_WHEN && sirLangTokenText( pState, 0, "otherwise" ) ) {
		sirLangAppend( pState, iNode, sirLangParseConditional( pState, SIR_LANG_SYNTAX_OTHERWISE ) );
	}
	sirLangFinishNode( pState, iNode );
	return iNode;
}

static uint32_t sirLangParseGenericLine( sirLangParserState_t *pState ) noexcept
{
	const uint32_t iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_EXPRESSION_STATEMENT, pState->iToken );
	sirLangAppend( pState, iNode, sirLangParseExpression( pState, 1 ) );
	if ( sirLangTake( pState, ":" ) ) {
		if ( iNode != SIR_LANG_NO_NODE ) {
			pState->pTree->pNodes[iNode].eKind = SIR_LANG_SYNTAX_FIELD;
		}
		sirLangAppend( pState, iNode, sirLangParseType( pState ) );
	} else {
		const bool bDirectAssign = sirLangTake( pState, "=" );
		const bool bReducerAssign =
		    !bDirectAssign && sirLangTokenText( pState, 1, "=" ) &&
		    ( sirLangTokenText( pState, 0, "sum" ) || sirLangTokenText( pState, 0, "min" ) || sirLangTokenText( pState, 0, "max" ) );
		if ( bDirectAssign || bReducerAssign ) {
			if ( iNode != SIR_LANG_NO_NODE ) {
				pState->pTree->pNodes[iNode].eKind = SIR_LANG_SYNTAX_ASSIGNMENT;
			}
			if ( bReducerAssign ) {
				sirLangAdvance( pState );
				sirLangTake( pState, "=" );
			}
			sirLangAppend( pState, iNode, sirLangParseExpression( pState, 1 ) );
		}
	}
	if ( !sirLangTakeKind( pState, SIR_LANG_TOKEN_NEWLINE ) && !sirLangAt( pState, SIR_LANG_TOKEN_END ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_EXPECTED_NEWLINE );
		sirLangRecoverLine( pState );
	}
	sirLangFinishNode( pState, iNode );
	return iNode;
}

static uint32_t sirLangParseImport( sirLangParserState_t *pState ) noexcept
{
	const uint32_t iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_IMPORT, pState->iToken );
	sirLangAdvance( pState );
	sirLangTakeDottedName( pState );
	if ( sirLangTake( pState, "as" ) ) {
		sirLangTakeName( pState );
	}
	sirLangRecoverLine( pState );
	sirLangFinishNode( pState, iNode );
	return iNode;
}

static uint32_t sirLangParseStatement( sirLangParserState_t *pState ) noexcept
{
	if ( sirLangTakeKind( pState, SIR_LANG_TOKEN_NEWLINE ) ) {
		return SIR_LANG_NO_NODE;
	}
	if ( sirLangAt( pState, SIR_LANG_TOKEN_ERROR ) ) {
		const uint32_t iNode = sirLangNewNode( pState, SIR_LANG_SYNTAX_ERROR, pState->iToken );
		sirLangAdvance( pState );
		sirLangFinishNode( pState, iNode );
		return iNode;
	}
	if ( sirLangTokenText( pState, 0, "import" ) ) {
		return sirLangParseImport( pState );
	}
	for ( uint32_t iKeyword = 0; iKeyword < sizeof( s_szHeaderKeywords ) / sizeof( s_szHeaderKeywords[0] ); ++iKeyword ) {
		if ( sirLangTokenText( pState, 0, s_szHeaderKeywords[iKeyword].pszText ) ) {
			return sirLangParseHeader( pState, s_szHeaderKeywords[iKeyword].eKind );
		}
	}
	if ( sirLangTokenText( pState, 0, "field" ) ) {
		return sirLangParseField( pState );
	}
	if ( sirLangTokenText( pState, 0, "let" ) ) {
		return sirLangParseLet( pState );
	}
	for ( uint32_t iKeyword = 0; iKeyword < sizeof( s_szSimpleKeywords ) / sizeof( s_szSimpleKeywords[0] ); ++iKeyword ) {
		if ( sirLangTokenText( pState, 0, s_szSimpleKeywords[iKeyword].pszText ) ) {
			return sirLangParseExpressionLine( pState, s_szSimpleKeywords[iKeyword].eKind );
		}
	}
	if ( sirLangTokenText( pState, 0, "when" ) ) {
		return sirLangParseConditional( pState, SIR_LANG_SYNTAX_WHEN );
	}
	if ( sirLangTokenText( pState, 0, "otherwise" ) ) {
		sirLangReport( pState, SIR_LANG_PARSE_UNEXPECTED_TOKEN );
		sirLangRecoverLine( pState );
		return SIR_LANG_NO_NODE;
	}
	if ( sirLangTokenText( pState, 0, "for" ) ) {
		return sirLangParseConditional( pState, SIR_LANG_SYNTAX_FOR );
	}
	if ( sirLangTokenText( pState, 0, "mutate" ) ) {
		return sirLangParseConditional( pState, SIR_LANG_SYNTAX_MUTATE );
	}
	return sirLangParseGenericLine( pState );
}

sirLangParseError_t SIRLangParse( const sirLangSourceView_t& source, sirLangSyntaxTree_t *pTree ) noexcept
{
	if ( pTree == nullptr || pTree->pTokens == nullptr || pTree->pNodes == nullptr || pTree->pDiagnostics == nullptr ||
	     pTree->nTokenCapacity == 0 || pTree->nNodeCapacity == 0 || pTree->nDiagnosticCapacity == 0 ||
	     ( source.nBytes > 0 && source.pBytes == nullptr ) ) {
		return SIR_LANG_PARSE_INVALID_INPUT;
	}
	pTree->source = source;
	pTree->nTokenCount = 0;
	pTree->nNodeCount = 0;
	pTree->nDiagnosticCount = 0;
	pTree->nRoot = SIR_LANG_NO_NODE;
	pTree->bTruncated = false;

	sirLangLexer_t lexer = {};
	SIRLangLexerInit( &lexer, source );
	for ( ;; ) {
		if ( pTree->nTokenCount == pTree->nTokenCapacity ) {
			pTree->bTruncated = true;
			return SIR_LANG_PARSE_CAPACITY;
		}
		const sirLangToken_t token = SIRLangLexerNext( &lexer );
		pTree->pTokens[pTree->nTokenCount++] = token;
		if ( token.eKind == SIR_LANG_TOKEN_END ) {
			break;
		}
	}

	sirLangParserState_t state = { pTree, 0, 0, SIR_LANG_PARSE_OK };
	for ( uint32_t iToken = 0; iToken < pTree->nTokenCount; ++iToken ) {
		if ( pTree->pTokens[iToken].eKind == SIR_LANG_TOKEN_ERROR ) {
			state.iToken = iToken;
			sirLangReport( &state, SIR_LANG_PARSE_LEXICAL_ERROR );
		}
	}
	state.iToken = 0;
	pTree->nRoot = sirLangNewNode( &state, SIR_LANG_SYNTAX_MODULE, 0 );
	while ( !sirLangAt( &state, SIR_LANG_TOKEN_END ) && !pTree->bTruncated ) {
		const uint32_t iBefore = state.iToken;
		sirLangAppend( &state, pTree->nRoot, sirLangParseStatement( &state ) );
		if ( state.iToken == iBefore ) {
			sirLangReport( &state, SIR_LANG_PARSE_UNEXPECTED_TOKEN );
			sirLangRecoverLine( &state );
			if ( state.iToken == iBefore ) {
				sirLangAdvance( &state );
			}
		}
	}
	sirLangFinishNode( &state, pTree->nRoot );
	if ( pTree->bTruncated ) {
		return SIR_LANG_PARSE_CAPACITY;
	}
	return state.eFirstError;
}

bool SIRLangSyntaxSpan( const sirLangSyntaxTree_t *pTree, uint32_t iNode, sirLangSourceSpan_t *pSpan ) noexcept
{
	if ( pTree == nullptr || pSpan == nullptr || pTree->pNodes == nullptr || pTree->pTokens == nullptr || iNode >= pTree->nNodeCount ||
	     pTree->nTokenCount == 0 ) {
		return false;
	}
	if ( iNode == pTree->nRoot ) {
		*pSpan = { 0, pTree->source.nBytes };
		return true;
	}
	const sirLangSyntaxNode_t& node = pTree->pNodes[iNode];
	if ( node.nStartToken >= pTree->nTokenCount || node.nEndToken > pTree->nTokenCount || node.nStartToken > node.nEndToken ) {
		return false;
	}
	const uint32_t nStart = pTree->pTokens[node.nStartToken].nOffset;
	uint32_t nEnd = nStart;
	if ( node.nEndToken > node.nStartToken ) {
		const sirLangToken_t& last = pTree->pTokens[node.nEndToken - 1];
		nEnd = last.nOffset + last.nLength;
	}
	if ( nEnd < nStart || nEnd > pTree->source.nBytes ) {
		return false;
	}
	*pSpan = { nStart, nEnd - nStart };
	return true;
}
