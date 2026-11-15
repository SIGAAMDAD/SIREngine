/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRSHAREDBVHCOOKER_H__
#define __SIRENGINE_SIRSHAREDBVHCOOKER_H__

#include <Mesh/SIRSharedGeometryBVH.h>

typedef struct sirStaticBvhCookInput_s
{
	uint64_t nAssetId;
	uint64_t nRevision;
	const float *pPositionsXYZ;
	const uint32_t *pTriangleIndices;
	const uint32_t *pTriangleMaterialIds;
	const uint32_t *pTriangleSemanticIds;
	const sirSharedSurfaceMaterial_t *pMaterials;
	uint32_t nVertexCount;
	uint32_t nTriangleCount;
	uint32_t nMaterialCount;
} sirStaticBvhCookInput_t;

typedef struct sirStaticBvhCookWorkspace_s
{
	sirSharedBvhNode_t *pNodes;
	sirSharedBvhTriangle_t *pTriangles;
	uint32_t *pTriangleOrder;
	uint32_t *pValidationScratch;
	void *pOutput;
	uint64_t nOutputCapacity;
	uint32_t nNodeCapacity;
	uint32_t nTriangleCapacity;
	uint32_t nOrderCapacity;
	uint32_t nValidationCapacity;
} sirStaticBvhCookWorkspace_t;

// Deterministic offline canonical BVH cooking from indexed static geometry.
// Does not emit a second physics/nav/audio BVH, allocate on runtime heaps or
// depend on GPU drivers. Caller owns all scratch and output storage.
class SIRSharedBvhCooker final
{
public:
	static uint32_t requiredNodes( uint32_t nTriangles ) noexcept;
	static sirSharedBvhResult_t cook( const sirStaticBvhCookInput_t& input, const sirStaticBvhCookWorkspace_t& workspace ) noexcept;
};

#endif // __SIRENGINE_SIRSHAREDBVHCOOKER_H__
