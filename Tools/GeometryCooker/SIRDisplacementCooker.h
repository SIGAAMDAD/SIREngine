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
#ifndef __SIRENGINE_SIRDISPLACEMENTCOOKER_H__
#define __SIRENGINE_SIRDISPLACEMENTCOOKER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Tools/GeometryCooker/SIRGeometryCooker.h>

#include <vector>

typedef enum sirDisplacementSampler_e : uint8_t
{
	SIR_DISPLACEMENT_CLAMP = 0,
	SIR_DISPLACEMENT_REPEAT = 1
} sirDisplacementSampler_t;

typedef struct sirDisplacementImage_s
{
	const float *pHeights;
	uint32_t nWidth;
	uint32_t nHeight;
} sirDisplacementImage_t;

typedef struct sirDisplacementRecipe_s
{
	float fAmplitude;
	float fMidpoint;
	float fMaxGeometricError;
	uint32_t nMaxDepth;
	uint32_t nMaxVertices;
	uint32_t nMaxTriangles;
	sirDisplacementSampler_t sampler;
} sirDisplacementRecipe_t;

typedef struct sirDisplacementBakeResult_s
{
	std::vector<float> positionsXYZ;
	std::vector<float> texcoordsUV;
	std::vector<uint32_t> indices;
	std::vector<sirGeometryCookMaterial_t> materials;
	uint64_t nRevision;
	uint8_t szAuthoringDigest[32];
} sirDisplacementBakeResult_t;

// Offline-only. A conforming edge split is shared by every incident triangle,
// including triangles belonging to different materials. Output owns all data.
// The image, sampler and recipe never become SIRPack runtime dependencies.
class SIRDisplacementCooker
{
public:
	static sirPackResult_t bake( const sirGeometryCookInput_t& input, const sirDisplacementImage_t& image,
	                             const sirDisplacementRecipe_t& recipe, sirDisplacementBakeResult_t *pResult ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRDISPLACEMENTCOOKER_H__
