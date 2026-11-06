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

#ifndef __SIRENGINE_SIRLANGSIGNATURES_H__
#define __SIRENGINE_SIRLANGSIGNATURES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Language/SIRLangSemantic.h>

// Fingerprints are deterministic *within one parsed module*; the compiler
// compares canonical token spellings before accepting equal composite types.
// They are not persistent SchemaIDs, native ABI keys or cryptographic hashes.
typedef struct sirLangTypeIdentity_s
{
	uint64_t nFingerprint;
	uint32_t iTypeNode;
	sirLangValueKind_t eValue;
} sirLangTypeIdentity_t;

typedef enum sirLangSignatureError_e : uint8_t
{
	SIR_LANG_SIGNATURE_OK = 0,
	SIR_LANG_SIGNATURE_INVALID_INPUT,
	SIR_LANG_SIGNATURE_CAPACITY,
	SIR_LANG_SIGNATURE_INVALID_SIGNATURE,
	SIR_LANG_SIGNATURE_UNTYPED_CALLEE,
	SIR_LANG_SIGNATURE_ARGUMENT_COUNT,
	SIR_LANG_SIGNATURE_UNKNOWN_ARGUMENT,
	SIR_LANG_SIGNATURE_DUPLICATE_ARGUMENT,
	SIR_LANG_SIGNATURE_POSITIONAL_AFTER_NAMED,
	SIR_LANG_SIGNATURE_ARGUMENT_TYPE,
	SIR_LANG_SIGNATURE_RETURN_TYPE,
	SIR_LANG_SIGNATURE_DEPTH_LIMIT
} sirLangSignatureError_t;

typedef struct sirLangSignatureParameter_s
{
	uint32_t iDeclarationNode;
	uint32_t iNameToken;
	sirLangTypeIdentity_t type;
} sirLangSignatureParameter_t;

typedef struct sirLangFunctionSignature_s
{
	uint32_t iDeclarationNode;
	uint32_t iNameToken;
	uint32_t iFirstParameter;
	uint32_t nParameterCount;
	uint64_t nFingerprint;
	sirLangTypeIdentity_t result;
} sirLangFunctionSignature_t;

typedef struct sirLangResolvedCall_s
{
	uint32_t iCallNode;
	uint32_t iDeclarationNode;
	uint64_t nFunctionId;
	uint64_t nSignatureFingerprint;
	sirLangTypeIdentity_t result;
} sirLangResolvedCall_t;

typedef struct sirLangSignatureDiagnostic_s
{
	sirLangSignatureError_t eError;
	uint32_t iNode;
	uint32_t nOffset;
	uint32_t nLine;
	uint32_t nColumn;
} sirLangSignatureDiagnostic_t;

// External signatures are supplied by an immutable host binding snapshot.
// This first scalar boundary does not accept unchecked composite host types.
typedef struct sirLangExternalSignature_s
{
	const char *pszName;
	const sirLangValueKind_t *pParameterTypes;
	const char *const *ppszParameterNames;
	uint32_t nParameterCount;
	sirLangValueKind_t eReturnType;
	uint64_t nFunctionId;
	uint64_t nSignatureId;
} sirLangExternalSignature_t;

typedef struct sirLangSignatureOutput_s
{
	sirLangFunctionSignature_t *pFunctions;
	uint32_t nFunctionCapacity;
	uint32_t nFunctionCount;
	sirLangSignatureParameter_t *pParameters;
	uint32_t nParameterCapacity;
	uint32_t nParameterCount;
	sirLangResolvedCall_t *pCalls;
	uint32_t nCallCapacity;
	uint32_t nCallCount;
	sirLangTypeIdentity_t *pNodeTypes;
	uint32_t nNodeTypeCapacity;
	sirLangSignatureDiagnostic_t *pDiagnostics;
	uint32_t nDiagnosticCapacity;
	uint32_t nDiagnosticCount;
	const sirLangExternalSignature_t *pExternals;
	uint32_t nExternalCount;
	bool bTruncated;
} sirLangSignatureOutput_t;

// Run only after a successful SIRLangAnalyze. Collect all signatures before
// resolving calls, so calls can target declarations appearing later in source.
// Invalid/unsupported host call signatures produce explicit diagnostics.
sirLangSignatureError_t SIRLangResolveSignatures( const sirLangSyntaxTree_t *pTree, const sirLangSemanticOutput_t *pSemantic,
                                                  sirLangSignatureOutput_t *pOutput ) noexcept;

#endif // __SIRENGINE_SIRLANGSIGNATURES_H__
