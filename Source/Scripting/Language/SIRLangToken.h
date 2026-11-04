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

#ifndef __SIRENGINE_SIRLANGTOKEN_H__
#define __SIRENGINE_SIRLANGTOKEN_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <stdint.h>

// Source is borrowed for the entire lexing session. Offsets are byte offsets,
// not UTF-8 code-point indexes. Input need not be NUL-terminated.
typedef struct sirLangSourceView_s
{
	const uint8_t *pBytes;
	uint32_t nBytes;
} sirLangSourceView_t;

typedef enum sirLangTokenKind_e : uint8_t
{
	SIR_LANG_TOKEN_END = 0,
	SIR_LANG_TOKEN_ERROR = 1,
	SIR_LANG_TOKEN_NEWLINE = 2,
	SIR_LANG_TOKEN_INDENT = 3,
	SIR_LANG_TOKEN_DEDENT = 4,
	SIR_LANG_TOKEN_IDENTIFIER = 5,
	SIR_LANG_TOKEN_KEYWORD = 6,
	SIR_LANG_TOKEN_NUMBER = 7,
	SIR_LANG_TOKEN_STRING = 8,
	SIR_LANG_TOKEN_PUNCTUATION = 9
} sirLangTokenKind_t;

typedef enum sirLangLexError_e : uint8_t
{
	SIR_LANG_LEX_OK = 0,
	SIR_LANG_LEX_INVALID_SOURCE = 1,
	SIR_LANG_LEX_INVALID_UTF8 = 2,
	SIR_LANG_LEX_INVALID_CHARACTER = 3,
	SIR_LANG_LEX_UNTERMINATED_STRING = 4,
	SIR_LANG_LEX_TAB_INDENT = 5,
	SIR_LANG_LEX_INCONSISTENT_INDENT = 6,
	SIR_LANG_LEX_INDENT_OVERFLOW = 7
} sirLangLexError_t;

typedef struct sirLangToken_s
{
	sirLangTokenKind_t eKind;
	sirLangLexError_t eError;
	uint32_t nOffset;
	uint32_t nLength;
	uint32_t nLine;
	uint32_t nColumn;
} sirLangToken_t;

// The stack is compile-time bounded. This is lexer scratch, not runtime state.
typedef struct sirLangLexer_s
{
	sirLangSourceView_t source;
	uint32_t nOffset;
	uint32_t nLine;
	uint32_t nColumn;
	uint32_t szIndent[64];
	uint32_t nIndentDepth;
	uint32_t nPendingDedents;
	bool bLineStart;
	bool bFinished;
	bool bInvalidSource;
} sirLangLexer_t;

#endif // __SIRENGINE_SIRLANGTOKEN_H__
