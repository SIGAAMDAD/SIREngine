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

#ifndef __SIR_LANGLOWERING_H__
#define __SIR_LANGLOWERING_H__

#include <Core/SIRPCH.h>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Scripting/Language/SIRLangGraphVM.h>

enum class SIRLangLowerError : uint8_t {
	SIR_LANG_LOWER_OK = 0,
	SIR_LANG_LOWER_INVALID_INPUT,
	SIR_LANG_LOWER_UNRESOLVED,
	SIR_LANG_LOWER_UNSUPPORTED,
	SIR_LANG_LOWER_CAPACITY,
	SIR_LANG_LOWER_TYPE,
	SIR_LANG_LOWER_INVALID_LITERAL,
	SIR_LANG_LOWER_MISSING_RETURN,
	SIR_LANG_LOWER_UNVERIFIABLE,
	SIR_LANG_LOWER_DEPTH
};

typedef struct sirLangHIRNode_s
{
	uint32_t iSourceNode;
	uint32_t iDeclarationNode;
	uint64_t nResolvedFunctionId;
	uint64_t nSignatureId;
	uint32_t nEffects;
	SIRLangValueKind eType;
	SIRLangSyntaxKind eSyntax;
} sirLangHIRNode_t;

typedef struct sirLangHIRProgram_s
{
	sirLangHIRNode_t *pNodes;
	uint32_t nCapacity;
	uint32_t nCount;
	uint32_t nAggregatedEffects;
	bool bUsesUnsupportedEffects;
} sirLangHIRProgram_t;

typedef struct sirLangCIRStorage_s
{
	sirLangCIRFunction_t *pFunctions;
	uint32_t nFunctionCapacity;
	sirLangCIRInstruction_t *pInstructions;
	uint32_t nInstructionCapacity;
	SIRLangValueKind *pParameterTypes;
	uint32_t nParameterTypeCapacity;
	uint32_t *pCallArguments;
	uint32_t nCallArgumentCapacity;
	const sirLangCIRCapability_t *pCapabilities;
	uint32_t nCapabilityCount;
	uint64_t nModuleId;
	uint64_t nSchemaId;
	uint64_t nLayoutDigest;
	uint64_t nBindingDigest;
	sirLangCIRProgram_t program;
	SIRLangLowerError eError;
	uint32_t iErrorNode;
} sirLangCIRStorage_t;

// HIR records are node-index aligned with the lossless syntax tree and retain
// resolved bindings. Unknown executable types fail closed, not 'dynamic any'.
SIRLangLowerError SIRLangBuildHIR( const sirLangSyntaxTree_t *pTree, const sirLangSemanticOutput_t *pSemantic,
                                     const sirLangSignatureOutput_t *pSignatures, sirLangHIRProgram_t *pOutput ) noexcept;

// Compiles statically typed scalar functions; unsupported effects and types
// produce an explicit error. The caller owns all CIR storage and identifiers.
SIRLangLowerError SIRLangBuildCIR( const sirLangSyntaxTree_t *pTree, const sirLangSignatureOutput_t *pSignatures,
                                     const sirLangHIRProgram_t *pHIR, sirLangCIRStorage_t *pOutput ) noexcept;

#endif // __SIR_LANGLOWERING_H__
