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

#ifndef __SIRENGINE_SIRSCRIPTOPTIMIZATIONRECIPE_H__
#define __SIRENGINE_SIRSCRIPTOPTIMIZATIONRECIPE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptOptimizationProtocol.h>

// All four digests are canonical 32-byte SHA-256 values selected by the
// frontend: complete dependency closure, frontend/tool version, native
// optimizer/tool version, and all code generation options/target features.
typedef struct sirScriptOptimizerRecipeIngredients_s
{
	uint8_t szDependencies[32];
	uint8_t szFrontend[32];
	uint8_t szOptimizer[32];
	uint8_t szOptions[32];
} sirScriptOptimizerRecipeIngredients_t;

typedef struct sirScriptOptimizerRecipe_s
{
	uint8_t szBytes[248];
	sirScriptCacheKey_t key;
} sirScriptOptimizerRecipe_t;

class SIRScriptOptimizationRecipe
{
public:
	static constexpr uint32_t RECIPE_BYTES = 248;

	static sirScriptResult_t build( const sirScriptOptimizationRequest_t& request, const sirScriptOptimizerRecipeIngredients_t& ingredients,
	                                const void *pInput, sirScriptOptimizerRecipe_t *pRecipe ) noexcept;
	static sirScriptResult_t verify( const sirScriptOptimizationRequest_t& request, const void *pInput, const void *pRecipe,
	                                 uint64_t nRecipeBytes ) noexcept;

protected:
	// Only canonical bytes participate in the recipe digest; never pointers.

private:
	static bool validRequest( const sirScriptOptimizationRequest_t& request ) noexcept;
};

#endif // __SIRENGINE_SIRSCRIPTOPTIMIZATIONRECIPE_H__
