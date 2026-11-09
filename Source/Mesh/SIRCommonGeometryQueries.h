/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRCOMMONGEOMETRYQUERIES_H__
#define __SIRENGINE_SIRCOMMONGEOMETRYQUERIES_H__

#include <Mesh/SIRMeshCache.h>

// Shared canonical static-BVH cooked layout, stored once in the GRDG BVH
// chunk. Internal nodes have nTriangleCount=0, leaves have a nonzero count.
// All child/primitive indexes address the same pinned mesh source; this
// structure contains no system-specific physics/audio/navigation objects.
typedef struct sirCommonBVHNode_s
{
	float szMinimum[3];
	float szMaximum[3];
	uint32_t nLeftChild;
	uint32_t nRightChild;
	uint32_t nFirstTriangle;
	uint32_t nTriangleCount;
} sirCommonBVHNode_t;

typedef struct sirCommonGeometryRay_s
{
	float szOrigin[3];
	float szDirection[3];
	float fMaximumDistance;
	bool bTwoSided;
} sirCommonGeometryRay_t;

typedef struct sirCommonGeometryHit_s
{
	float fDistance;
	float szPosition[3];
	float szNormal[3];
	uint32_t nTriangle;
	uint32_t nSurfaceMaterialId;
	sirMeshKey_t sourceKey;
} sirCommonGeometryHit_t;

typedef enum sirCommonGeometryResult_e : uint8_t
{
	SIR_COMMON_GEOMETRY_HIT = 0,
	SIR_COMMON_GEOMETRY_MISS = 1,
	SIR_COMMON_GEOMETRY_INVALID_ARGUMENT = 2,
	SIR_COMMON_GEOMETRY_INVALID_BVH = 3,
	SIR_COMMON_GEOMETRY_OUT_OF_BUDGET = 4
} sirCommonGeometryResult_t;

// The caller owns an active SIRMeshCache borrow, which pins these exact
// canonical arrays. Rendering, physics, acoustic rays and navigation all
// run this same read-only evaluator without private BVHs or scratch heaps.
sirCommonGeometryResult_t sirCommonGeometryTraceRay( const sirMeshGeometryRead_t& rGeometry, const sirCommonGeometryRay_t& rRay,
                                                     sirCommonGeometryHit_t *pHit ) noexcept;

#endif // __SIRENGINE_SIRCOMMONGEOMETRYQUERIES_H__
