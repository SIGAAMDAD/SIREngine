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

#ifndef __SIRENGINE_SIRLANGPARSER_H__
#define __SIRENGINE_SIRLANGPARSER_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Scripting/Language/SIRLangToken.h>

// This is a source-backed, lossless syntax view: tokens retain exact byte spans,
// while the original borrowed source retains whitespace, comments and spelling.
// Nodes reference half-open token ranges, and children link by stable indexes.
// The caller owns every buffer and keeps the source bytes alive until finished.
typedef enum sirLangSyntaxKind_e : uint8_t
{
	SIR_LANG_SYNTAX_MODULE = 0,
	SIR_LANG_SYNTAX_IMPORT,
	SIR_LANG_SYNTAX_SCHEMA,
	SIR_LANG_SYNTAX_BEHAVIOR,
	SIR_LANG_SYNTAX_CLASS,
	SIR_LANG_SYNTAX_INTERFACE,
	SIR_LANG_SYNTAX_RECORD,
	SIR_LANG_SYNTAX_ENUM,
	SIR_LANG_SYNTAX_FUNCTION,
	SIR_LANG_SYNTAX_EVENT,
	SIR_LANG_SYNTAX_STATE,
	SIR_LANG_SYNTAX_CONDITION_BANK,
	SIR_LANG_SYNTAX_FIELD,
	SIR_LANG_SYNTAX_BLOCK,
	SIR_LANG_SYNTAX_PARAMETER,
	SIR_LANG_SYNTAX_NAMED_ARGUMENT,
	SIR_LANG_SYNTAX_TYPE,
	SIR_LANG_SYNTAX_LET,
	SIR_LANG_SYNTAX_WHEN,
	SIR_LANG_SYNTAX_OTHERWISE,
	SIR_LANG_SYNTAX_FOR,
	SIR_LANG_SYNTAX_MUTATE,
	SIR_LANG_SYNTAX_RETURN,
	SIR_LANG_SYNTAX_EMIT,
	SIR_LANG_SYNTAX_AFTER,
	SIR_LANG_SYNTAX_SPAWN,
	SIR_LANG_SYNTAX_DESTROY,
	SIR_LANG_SYNTAX_TRANSITION,
	SIR_LANG_SYNTAX_BREAK,
	SIR_LANG_SYNTAX_CONTINUE,
	SIR_LANG_SYNTAX_ASSIGNMENT,
	SIR_LANG_SYNTAX_EXPRESSION_STATEMENT,
	SIR_LANG_SYNTAX_LITERAL,
	SIR_LANG_SYNTAX_NAME,
	SIR_LANG_SYNTAX_UNARY,
	SIR_LANG_SYNTAX_BINARY,
	SIR_LANG_SYNTAX_MEMBER,
	SIR_LANG_SYNTAX_CALL,
	SIR_LANG_SYNTAX_INDEX,
	SIR_LANG_SYNTAX_GROUP,
	SIR_LANG_SYNTAX_ERROR
} sirLangSyntaxKind_t;

typedef enum sirLangParseError_e : uint8_t
{
	SIR_LANG_PARSE_OK = 0,
	SIR_LANG_PARSE_INVALID_INPUT,
	SIR_LANG_PARSE_CAPACITY,
	SIR_LANG_PARSE_LEXICAL_ERROR,
	SIR_LANG_PARSE_EXPECTED_NAME,
	SIR_LANG_PARSE_EXPECTED_TYPE,
	SIR_LANG_PARSE_EXPECTED_EXPRESSION,
	SIR_LANG_PARSE_EXPECTED_TOKEN,
	SIR_LANG_PARSE_EXPECTED_COLON,
	SIR_LANG_PARSE_EXPECTED_NEWLINE,
	SIR_LANG_PARSE_EXPECTED_INDENT,
	SIR_LANG_PARSE_UNEXPECTED_TOKEN,
	SIR_LANG_PARSE_NESTING_LIMIT
} sirLangParseError_t;

typedef struct sirLangSyntaxNode_s
{
	sirLangSyntaxKind_t eKind;
	uint32_t nStartToken;
	uint32_t nEndToken;
	uint32_t nFirstChild;
	uint32_t nLastChild;
	uint32_t nNextSibling;
} sirLangSyntaxNode_t;

typedef struct sirLangParseDiagnostic_s
{
	sirLangParseError_t eError;
	sirLangLexError_t eLexError;
	uint32_t nOffset;
	uint32_t nLength;
	uint32_t nLine;
	uint32_t nColumn;
} sirLangParseDiagnostic_t;

typedef struct sirLangSyntaxTree_s
{
	sirLangSourceView_t source;
	sirLangToken_t *pTokens;
	uint32_t nTokenCapacity;
	uint32_t nTokenCount;
	sirLangSyntaxNode_t *pNodes;
	uint32_t nNodeCapacity;
	uint32_t nNodeCount;
	sirLangParseDiagnostic_t *pDiagnostics;
	uint32_t nDiagnosticCapacity;
	uint32_t nDiagnosticCount;
	uint32_t nRoot;
	bool bTruncated;
} sirLangSyntaxTree_t;

typedef struct sirLangSourceSpan_s
{
	uint32_t nOffset;
	uint32_t nLength;
} sirLangSourceSpan_t;

// Returns the exact source-byte extent of a syntax node. The module covers
// the full original buffer, including whitespace and comments before/after
// all tokens. Invalid node indexes return false and leave output unchanged.
bool SIRLangSyntaxSpan( const sirLangSyntaxTree_t *pTree, uint32_t iNode, sirLangSourceSpan_t *pSpan ) noexcept;

// Syntax only. No engine bindings, persistent state or runtime code are created.
// Returns OK for a syntactically clean module, otherwise the first error;
// error recovery retains later independent declarations when capacity permits.
sirLangParseError_t SIRLangParse( const sirLangSourceView_t &source, sirLangSyntaxTree_t *pTree ) noexcept;

#endif // __SIRENGINE_SIRLANGPARSER_H__
