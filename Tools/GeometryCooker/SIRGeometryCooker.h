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
#ifndef __SIRENGINE_SIRGEOMETRYCOOKER_H__
#define __SIRENGINE_SIRGEOMETRYCOOKER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshPackSchema.h>

#include <vector>

typedef struct sirGeometryCookMaterial_s
{
	uint32_t nFirstIndex;
	uint32_t nIndexCount;
	uint32_t nMaterialId;
} sirGeometryCookMaterial_t;

typedef struct sirGeometryCookInput_s
{
	sirMeshKey_t key;
	uint32_t nResourceTypeId;
	uint64_t nPackageId;
	uint64_t nGeneration;
	const float *pPositionsXYZ;
	uint32_t nVertexCount;
	const uint32_t *pIndices;
	uint32_t nIndexCount;
	const sirGeometryCookMaterial_t *pMaterials;
	uint32_t nMaterialCount;
	uint32_t nMaxVerticesPerMeshlet;
	uint32_t nMaxTrianglesPerMeshlet;
	uint32_t nTargetRootTriangles;
	float fMaxSimplificationError;
	bool bCookCollision;
	const float *pTexcoordsUV; // Optional, two finite floats per vertex.
	const float *pNormalsXYZ;  // Optional, three finite floats per vertex.
} sirGeometryCookInput_t;

typedef struct sirGeometryCookOutput_s
{
	std::vector<uint8_t> catalog;
	std::vector<uint8_t> shard;
	std::vector<uint8_t> manifest;
	std::vector<std::vector<uint8_t>> pages;
} sirGeometryCookOutput_t;

struct sirDisplacementImage_s;
struct sirDisplacementRecipe_s;
struct sirPackShippingPolicy_s;

// Offline-only, deterministic and independent of runtime page/GRDG ownership.
// Requires the repository's pinned meshoptimizer revision; no alternate
// meshlet builder is linked or substituted.
class SIRGeometryCooker
{
public:
	static sirPackResult_t cook( const sirGeometryCookInput_t& input, sirGeometryCookOutput_t *pOutput ) noexcept;
	static sirPackResult_t cookShipping( const sirGeometryCookInput_t& input, const sirPackShippingPolicy_s& policy,
	                                     sirGeometryCookOutput_t *pOutput ) noexcept;
	static sirPackResult_t cookDisplaced( const sirGeometryCookInput_t& input, const sirDisplacementImage_s& image,
	                                      const sirDisplacementRecipe_s& recipe, sirGeometryCookOutput_t *pOutput ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRGEOMETRYCOOKER_H__
