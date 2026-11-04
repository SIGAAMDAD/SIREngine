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

#ifndef __SIRENGINE_SIRLANGEFFECTS_H__
#define __SIRENGINE_SIRLANGEFFECTS_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Scripting/Language/SIRLangLowering.h>

typedef enum sirLangEffectError_e : uint8_t
{
	SIR_LANG_EFFECT_OK = 0,
	SIR_LANG_EFFECT_INVALID_INPUT,
	SIR_LANG_EFFECT_CAPACITY,
	SIR_LANG_EFFECT_UNKNOWN_CAPABILITY,
	SIR_LANG_EFFECT_UNSAFE_SUSPEND,
	SIR_LANG_EFFECT_NONDETERMINISTIC_CALL,
	SIR_LANG_EFFECT_BORROW_ESCAPE,
	SIR_LANG_EFFECT_INVALID_LANE,
	SIR_LANG_EFFECT_MALFORMED_TREE
} sirLangEffectError_t;

typedef struct sirLangEffectFunction_s
{
	uint32_t iDeclarationNode;
	uint32_t nDirectEffects;
	uint32_t nTransitiveEffects;
	uint32_t nOwnerLane;
	bool bDeterministic;
} sirLangEffectFunction_t;

typedef struct sirLangEffectOutput_s
{
	sirLangEffectFunction_t *pFunctions;
	uint32_t nCapacity;
	uint32_t nCount;
	uint32_t iErrorNode;
	sirLangEffectError_t eError;
	bool bRequireDeterministic;
} sirLangEffectOutput_t;

typedef struct sirLangEffectInputs_s
{
	const sirLangSyntaxTree_t *pTree;
	const sirLangHIRProgram_t *pHIR;
	const sirLangSignatureOutput_t *pSignatures;
	const sirLangCIRProgram_t *pProgram;
} sirLangEffectInputs_t;

// Performs a bounded interprocedural effects fixed-point. No mutable global
// registry or speculative native effect classification is introduced.
sirLangEffectError_t SIRLangAnalyzeEffects( const sirLangEffectInputs_t& inputs, sirLangEffectOutput_t *pOutput ) noexcept;

#endif // __SIRENGINE_SIRLANGEFFECTS_H__
