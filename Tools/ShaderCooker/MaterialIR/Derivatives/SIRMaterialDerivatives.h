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

#ifndef __SIRENGINE_SIRMATERIALDERIVATIVES_H__
#define __SIRENGINE_SIRMATERIALDERIVATIVES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Tools/ShaderCooker/MaterialIR/SIRMaterialIR.h>

// Screen coordinates are post-viewport pixel coordinates. UVs are authored,
// triangle-local unwrapped coordinates; adjacent material/UV islands never
// enter this calculation. Reciprocal-W is the clip-space 1/w of each vertex.
typedef struct sirMaterialScreenVertex_s
{
	double fScreenX;
	double fScreenY;
	double fU;
	double fV;
	double fReciprocalW;
} sirMaterialScreenVertex_t;

typedef struct sirMaterialTriangleSample_s
{
	sirMaterialScreenVertex_t vertices[3];
	double fScreenX;
	double fScreenY;
} sirMaterialTriangleSample_t;

// All component values/gradients are finite when bValid is true. A shader
// consuming an invalid gradient must select an explicit fallback, not ddx/ddy
// over a quad whose neighboring lanes can belong to another triangle.
typedef struct sirMaterialGradient_s
{
	double szValue[4];
	double szDdx[4];
	double szDdy[4];
	uint32_t nComponents;
	bool bValid;
	bool bDiscontinuous;
} sirMaterialGradient_t;

typedef struct sirMaterialGradientEval_s
{
	const sirMaterialIRNode_t *pNode;
	const sirMaterialGradient_t *pPrior;
	uint32_t nPriorCount;
	uint32_t nEnabledSwitches;
} sirMaterialGradientEval_t;

typedef enum sirMaterialSampleMode_e : uint32_t
{
	SIR_MATERIAL_SAMPLE_GRADIENT = 0,
	SIR_MATERIAL_SAMPLE_EXPLICIT_LOD = 1
} sirMaterialSampleMode_t;

typedef struct sirMaterialTextureFootprint_s
{
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nMaximumMip;
	uint32_t nReserved;
} sirMaterialTextureFootprint_t;

typedef struct sirMaterialSamplePlan_s
{
	sirMaterialSampleMode_t eMode;
	double fMipLevel;
	double fMajorAxis;
	double fMinorAxis;
	double fAnisotropy;
	bool bDerivativeValid;
} sirMaterialSamplePlan_t;

// Identifiers are validated HLSL identifiers; the emitted text references
// existing symbols and contains no runtime shader compilation. The optional
// LOD fallback is explicit (SampleLevel with zero gradient reliance).
typedef struct sirMaterialSampleHLSL_s
{
	const char *pszTexture;
	const char *pszSampler;
	const char *pszUV;
	const char *pszDdx;
	const char *pszDdy;
	sirMaterialSampleMode_t eMode;
} sirMaterialSampleHLSL_t;

typedef struct sirMaterialTriplanarGradients_s
{
	sirMaterialGradient_t xy;
	sirMaterialGradient_t xz;
	sirMaterialGradient_t yz;
} sirMaterialTriplanarGradients_t;

sirShaderCookResult_t sirMaterialTriangleUVGradients( const sirMaterialTriangleSample_t& input, sirMaterialGradient_t *pOutput ) noexcept;
sirShaderCookResult_t sirMaterialPropagateGradients( const sirMaterialGradientEval_t& input, sirMaterialGradient_t *pOutput ) noexcept;
sirShaderCookResult_t sirMaterialTriplanarGradients( const sirMaterialGradient_t& worldPosition,
                                                     sirMaterialTriplanarGradients_t *pOutput ) noexcept;
sirShaderCookResult_t sirMaterialPlanTextureSample( const sirMaterialGradient_t& uv, const sirMaterialTextureFootprint_t& texture,
                                                    sirMaterialSamplePlan_t *pOutput ) noexcept;
sirShaderCookResult_t sirMaterialEmitSampleHLSL( const sirMaterialSampleHLSL_t& input, char *pszOutput, uint32_t nCapacity,
                                                 uint32_t *pRequiredBytes ) noexcept;

#endif // __SIRENGINE_SIRMATERIALDERIVATIVES_H__
