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

#ifndef __SIRENGINE_SIRMATERIALIREMITTER_H__
#define __SIRENGINE_SIRMATERIALIREMITTER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Tools/ShaderCooker/MaterialIR/SIRMaterialIR.h>

// Offline HLSL expression/function emission. Leaf names refer to symbols
// declared by the caller's material template. Texture samples use explicit
// mip-zero SampleLevel pending the derivative-aware lowering path.
typedef struct sirMaterialIREmitRequest_s
{
	const sirMaterialIRGraph_t *pGraph;
	const char *const *pSymbols;
	uint32_t nSymbolCount;
	uint32_t nPermutationMask;
	uint64_t nAvailableFeatures;
	sirShaderPass_t ePass;
	const char *pszSampler;
} sirMaterialIREmitRequest_t;

typedef struct sirMaterialIREmitOutput_s
{
	char *pszBuffer;
	uint32_t nCapacity;
	uint32_t nRequiredBytes;
} sirMaterialIREmitOutput_t;

// The caller's buffer is unchanged on failure. An undersized output buffer
// reports the exact required byte count, including its terminating NUL.
sirShaderCookResult_t sirMaterialIREmitHLSL( const sirMaterialIREmitRequest_t& request, sirMaterialIREmitOutput_t *pOutput ) noexcept;

#endif // __SIRENGINE_SIRMATERIALIREMITTER_H__
