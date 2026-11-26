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

#ifndef __SIR_LANGSEMANTIC_H__
#define __SIR_LANGSEMANTIC_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Language/SIRLangParser.h>

constexpr uint32_t SIR_LANG_INVALID_INDEX = UINT32_MAX;
constexpr uint32_t SIR_LANG_MAX_SEMANTIC_DEPTH = 128;

enum class SIRLangSemanticError : uint8_t {
	SIR_LANG_SEMANTIC_OK = 0,
	SIR_LANG_SEMANTIC_INVALID_INPUT,
	SIR_LANG_SEMANTIC_CAPACITY,
	SIR_LANG_SEMANTIC_INVALID_TREE,
	SIR_LANG_SEMANTIC_DUPLICATE_SYMBOL,
	SIR_LANG_SEMANTIC_UNKNOWN_SYMBOL,
	SIR_LANG_SEMANTIC_UNKNOWN_TYPE,
	SIR_LANG_SEMANTIC_INVALID_GLOBAL,
	SIR_LANG_SEMANTIC_TYPE_MISMATCH,
	SIR_LANG_SEMANTIC_INVALID_LITERAL,
	SIR_LANG_SEMANTIC_INVALID_ASSIGNMENT,
	SIR_LANG_SEMANTIC_INVALID_RETURN,
	SIR_LANG_SEMANTIC_UNKNOWN_FIELD,
	SIR_LANG_SEMANTIC_DEPTH_LIMIT
};

enum class SIRLangSymbolKind : uint8_t {
	SIR_LANG_SYMBOL_TYPE = 0,
	SIR_LANG_SYMBOL_BEHAVIOR,
	SIR_LANG_SYMBOL_FUNCTION,
	SIR_LANG_SYMBOL_EVENT,
	SIR_LANG_SYMBOL_FIELD,
	SIR_LANG_SYMBOL_PARAMETER,
	SIR_LANG_SYMBOL_LOCAL
};

enum class SIRLangValueKind : uint8_t {
	SIR_LANG_VALUE_UNKNOWN = 0,
	SIR_LANG_VALUE_VOID,
	SIR_LANG_VALUE_BOOL,
	SIR_LANG_VALUE_I8,
	SIR_LANG_VALUE_U8,
	SIR_LANG_VALUE_I16,
	SIR_LANG_VALUE_U16,
	SIR_LANG_VALUE_I32,
	SIR_LANG_VALUE_U32,
	SIR_LANG_VALUE_I64,
	SIR_LANG_VALUE_U64,
	SIR_LANG_VALUE_F16,
	SIR_LANG_VALUE_F32,
	SIR_LANG_VALUE_F64,
	SIR_LANG_VALUE_ATOM,
	SIR_LANG_VALUE_OTHER,
	SIR_LANG_VALUE_FUNCTION
};

typedef struct sirLangSemanticSymbol_s
{
	uint32_t iNameToken;
	uint32_t iDeclarationNode;
	uint32_t iScopeNode;
	uint32_t iTypeNode;
	SIRLangSymbolKind eKind;
	SIRLangValueKind eValue;
} sirLangSemanticSymbol_t;

typedef struct sirLangSemanticDiagnostic_s
{
	SIRLangSemanticError eError;
	uint32_t iNode;
	uint32_t nOffset;
	uint32_t nLine;
	uint32_t nColumn;
} sirLangSemanticDiagnostic_t;

// Host-provided declarations (native binding registry, imported types, or
// external package symbols). The analyzer never owns the pointed-to bytes.
typedef struct sirLangExternalSymbol_s
{
	const char *pszName;
	SIRLangSymbolKind eKind;
	SIRLangValueKind eValue;
} sirLangExternalSymbol_t;

typedef struct sirLangResolvedReference_s
{
	uint32_t iUseNode;
	uint32_t iDeclarationNode; // UINT32_MAX for a registered external/intrinsic.
	SIRLangValueKind eValue;
} sirLangResolvedReference_t;

typedef struct sirLangSemanticOutput_s
{
	sirLangSemanticSymbol_t *pSymbols;
	uint32_t nSymbolCapacity;
	uint32_t nSymbolCount;
	sirLangResolvedReference_t *pReferences;
	uint32_t nReferenceCapacity;
	uint32_t nReferenceCount;
	sirLangSemanticDiagnostic_t *pDiagnostics;
	uint32_t nDiagnosticCapacity;
	uint32_t nDiagnosticCount;
	const sirLangExternalSymbol_t *pExternals;
	uint32_t nExternalCount;
	uint32_t nResolvedReferences;
	bool bTruncated;
} sirLangSemanticOutput_t;

// Consumes a successful parser tree with source bytes still alive. All outputs
// are caller-owned. A forward-declared type/function can be resolved within
// its lexical scope; local variables become visible only after declaration.
// This stage validates types and local expressions, not native effect safety.
SIRLangSemanticError SIRLangAnalyze( const sirLangSyntaxTree_t *pTree, sirLangSemanticOutput_t *pOutput ) noexcept;

#endif // __SIR_LANGSEMANTIC_H__
