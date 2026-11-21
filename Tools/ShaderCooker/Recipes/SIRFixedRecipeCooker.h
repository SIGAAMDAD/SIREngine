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

#ifndef __SIRENGINE_SIRFIXEDRECIPECOOKER_H__
#define __SIRENGINE_SIRFIXEDRECIPECOOKER_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Renderer/SIRGraphicsABIOperations.h>
#include <Tools/ShaderCooker/Frontend/SIRShaderFrontend.h>

constexpr uint32_t SIR_FIXED_RECIPE_MAGIC = 0x31465853U; // SXF1
constexpr uint32_t SIR_FIXED_RECIPE_VERSION = 1;
constexpr uint32_t SIR_FIXED_RECIPE_MAX_STAGES = 16;
constexpr uint32_t SIR_FIXED_RECIPE_MAX_LIGHTS = 32;
constexpr uint32_t SIR_FIXED_RECIPE_HEADER_BYTES = 28;
constexpr uint32_t SIR_FIXED_RECIPE_BASE_BYTES = 332;
constexpr uint32_t SIR_FIXED_RECIPE_STAGE_BYTES = 48;
constexpr uint32_t SIR_FIXED_RECIPE_LIGHT_BYTES = 80;
constexpr uint32_t SIR_FIXED_RECIPE_MAX_BYTES = SIR_FIXED_RECIPE_BASE_BYTES + SIR_FIXED_RECIPE_MAX_STAGES * SIR_FIXED_RECIPE_STAGE_BYTES +
                                                SIR_FIXED_RECIPE_MAX_LIGHTS * SIR_FIXED_RECIPE_LIGHT_BYTES;

// A decoded recipe owns the stage and light arrays. Never copy the resulting
// object after decoding: recipe.pStages/pLights point into that exact object.
// GPU backend ownership and resource lifetimes are unchanged.
typedef struct sirShaderFixedDecoded_s
{
	sirGraphicsABIFixedRecipe_t recipe;
	sirGraphicsABIFixedStage_t szStages[SIR_FIXED_RECIPE_MAX_STAGES];
	sirGraphicsABIFixedLight_t szLights[SIR_FIXED_RECIPE_MAX_LIGHTS];
	uint64_t nContentHash;
} sirShaderFixedDecoded_t;

// Exact, endian-defined binary artifact. No pointers, allocation, shader
// source, padding, or host-size-dependent values are persisted.
// Sizing calls return CAPACITY_EXCEEDED plus required size; destination is
// never touched on validation or insufficient-capacity failure.
sirShaderCookResult_t sirShaderFixedCook( const sirGraphicsABIFixedRecipe_t& recipe, void *pDestination, uint32_t nCapacity,
                                          uint32_t *pRequiredBytes ) noexcept;
sirShaderCookResult_t sirShaderFixedLoad( const void *pBytes, uint32_t nBytes, sirShaderFixedDecoded_t *pOut ) noexcept;
sirShaderCookResult_t sirShaderFixedValidate( const void *pBytes, uint32_t nBytes ) noexcept;

#endif // __SIRENGINE_SIRFIXEDRECIPECOOKER_H__
