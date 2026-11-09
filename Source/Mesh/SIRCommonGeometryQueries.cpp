/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Mesh/SIRCommonGeometryQueries.h>

#include <math.h>
#include <string.h>

static bool sirCommonRayBounds( const sirCommonBVHNode_t& rNode, const sirCommonGeometryRay_t& rRay, float fLimit ) noexcept
{
	float fNear = 0.0f;
	float fFar = fLimit;
	for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
		const float fStart = rRay.szOrigin[iAxis];
		const float fDir = rRay.szDirection[iAxis];
		if ( fDir == 0.0f ) {
			if ( fStart < rNode.szMinimum[iAxis] || fStart > rNode.szMaximum[iAxis] ) {
				return false;
			}
			continue;
		}
		const float fInv = 1.0f / fDir;
		float fA = ( rNode.szMinimum[iAxis] - fStart ) * fInv;
		float fB = ( rNode.szMaximum[iAxis] - fStart ) * fInv;
		if ( fA > fB ) {
			const float fSwap = fA;
			fA = fB;
			fB = fSwap;
		}
		fNear = fA > fNear ? fA : fNear;
		fFar = fB < fFar ? fB : fFar;
		if ( fNear > fFar ) {
			return false;
		}
	}
	return true;
}

static bool sirCommonRayTriangle( const sirMeshGeometryRead_t& rGeometry, uint32_t nTriangle, const sirCommonGeometryRay_t& rRay,
                                  float *pDistance, float *pNormal ) noexcept
{
	const uint32_t *pIndices = rGeometry.pTriangleIndices + nTriangle * 3;
	const float *pA = rGeometry.pPositionsXYZ + pIndices[0] * 3;
	const float *pB = rGeometry.pPositionsXYZ + pIndices[1] * 3;
	const float *pC = rGeometry.pPositionsXYZ + pIndices[2] * 3;
	const float e1[3] = { pB[0] - pA[0], pB[1] - pA[1], pB[2] - pA[2] };
	const float e2[3] = { pC[0] - pA[0], pC[1] - pA[1], pC[2] - pA[2] };
	const float p[3] = { rRay.szDirection[1] * e2[2] - rRay.szDirection[2] * e2[1],
	                     rRay.szDirection[2] * e2[0] - rRay.szDirection[0] * e2[2],
	                     rRay.szDirection[0] * e2[1] - rRay.szDirection[1] * e2[0] };
	const float fDet = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
	if ( rRay.bTwoSided ? fabsf( fDet ) <= 1e-9f : fDet <= 1e-9f ) {
		return false;
	}
	const float fInvDet = 1.0f / fDet;
	const float t[3] = { rRay.szOrigin[0] - pA[0], rRay.szOrigin[1] - pA[1], rRay.szOrigin[2] - pA[2] };
	const float fU = ( t[0] * p[0] + t[1] * p[1] + t[2] * p[2] ) * fInvDet;
	if ( fU < 0.0f || fU > 1.0f ) {
		return false;
	}
	const float q[3] = { t[1] * e1[2] - t[2] * e1[1], t[2] * e1[0] - t[0] * e1[2], t[0] * e1[1] - t[1] * e1[0] };
	const float fV = ( rRay.szDirection[0] * q[0] + rRay.szDirection[1] * q[1] + rRay.szDirection[2] * q[2] ) * fInvDet;
	if ( fV < 0.0f || fU + fV > 1.0f ) {
		return false;
	}
	const float fDistance = ( e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2] ) * fInvDet;
	if ( !isfinite( fDistance ) || fDistance < 0.0f || fDistance > *pDistance ) {
		return false;
	}
	const float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
	const float fLength = sqrtf( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
	if ( !( fLength > 0.0f ) || !isfinite( fLength ) ) {
		return false;
	}
	*pDistance = fDistance;
	for ( uint32_t i = 0; i < 3; ++i ) {
		pNormal[i] = n[i] / fLength;
	}
	return true;
}

sirCommonGeometryResult_t sirCommonGeometryTraceRay( const sirMeshGeometryRead_t& rGeometry, const sirCommonGeometryRay_t& rRay,
                                                     sirCommonGeometryHit_t *pHit ) noexcept
{
	if ( pHit == nullptr || rGeometry.pPositionsXYZ == nullptr || rGeometry.pTriangleIndices == nullptr || rGeometry.nVertexCount == 0 ||
	     rGeometry.nIndexCount == 0 || rGeometry.nIndexCount % 3 != 0 || rRay.fMaximumDistance <= 0.0f ||
	     !isfinite( rRay.fMaximumDistance ) ) {
		return SIR_COMMON_GEOMETRY_INVALID_ARGUMENT;
	}
	float fSquaredDirection = 0.0f;
	for ( uint32_t axis = 0; axis < 3; ++axis ) {
		if ( !isfinite( rRay.szOrigin[axis] ) || !isfinite( rRay.szDirection[axis] ) ) {
			return SIR_COMMON_GEOMETRY_INVALID_ARGUMENT;
		}
		fSquaredDirection += rRay.szDirection[axis] * rRay.szDirection[axis];
	}
	if ( !( fSquaredDirection > 0.0f ) || !isfinite( fSquaredDirection ) ) {
		return SIR_COMMON_GEOMETRY_INVALID_ARGUMENT;
	}
	const uint32_t nTriangles = rGeometry.nIndexCount / 3;
	if ( rGeometry.pStaticBVH == nullptr || rGeometry.nStaticBVHBytes == 0 ||
	     rGeometry.nStaticBVHBytes % sizeof( sirCommonBVHNode_t ) != 0 ||
	     rGeometry.nStaticBVHBytes / sizeof( sirCommonBVHNode_t ) > UINT32_MAX || rGeometry.pSurfaceMaterials == nullptr ||
	     rGeometry.nSurfaceMaterialBytes < ( uint64_t )nTriangles * sizeof( uint32_t ) ) {
		return SIR_COMMON_GEOMETRY_INVALID_BVH;
	}
	const uint32_t nNodeCount = ( uint32_t )( rGeometry.nStaticBVHBytes / sizeof( sirCommonBVHNode_t ) );
	constexpr uint32_t MAX_STACK = 64;
	uint32_t szStack[MAX_STACK] = { 0 };
	uint32_t nStackCount = 1;
	uint64_t nVisitsRemaining = ( uint64_t )nNodeCount * 2 + 1;
	float fNearest = rRay.fMaximumDistance;
	bool bFound = false;
	sirCommonGeometryHit_t result = {};
	while ( nStackCount != 0 ) {
		if ( nVisitsRemaining-- == 0 ) {
			return SIR_COMMON_GEOMETRY_INVALID_BVH;
		}
		const uint32_t iNode = szStack[--nStackCount];
		if ( iNode >= nNodeCount ) {
			return SIR_COMMON_GEOMETRY_INVALID_BVH;
		}
		sirCommonBVHNode_t node = {};
		memcpy( &node, ( const uint8_t * )rGeometry.pStaticBVH + ( uint64_t )iNode * sizeof( node ), sizeof( node ) );
		for ( uint32_t i = 0; i < 3; ++i ) {
			if ( !isfinite( node.szMinimum[i] ) || !isfinite( node.szMaximum[i] ) || node.szMinimum[i] > node.szMaximum[i] ) {
				return SIR_COMMON_GEOMETRY_INVALID_BVH;
			}
		}
		if ( node.nTriangleCount == 0 ) {
			if ( node.nLeftChild >= nNodeCount || node.nRightChild >= nNodeCount || node.nLeftChild == node.nRightChild ||
			     node.nLeftChild == iNode || node.nRightChild == iNode ) {
				return SIR_COMMON_GEOMETRY_INVALID_BVH;
			}
		} else if ( node.nFirstTriangle >= nTriangles || node.nTriangleCount > nTriangles - node.nFirstTriangle ) {
			return SIR_COMMON_GEOMETRY_INVALID_BVH;
		}
		if ( !sirCommonRayBounds( node, rRay, fNearest ) ) {
			continue;
		}
		if ( node.nTriangleCount == 0 ) {
			if ( nStackCount + 2 > MAX_STACK ) {
				return SIR_COMMON_GEOMETRY_OUT_OF_BUDGET;
			}
			szStack[nStackCount++] = node.nRightChild;
			szStack[nStackCount++] = node.nLeftChild;
			continue;
		}
		for ( uint32_t iTriangle = node.nFirstTriangle; iTriangle < node.nFirstTriangle + node.nTriangleCount; ++iTriangle ) {
			const uint32_t *pIndices = rGeometry.pTriangleIndices + iTriangle * 3;
			if ( pIndices[0] >= rGeometry.nVertexCount || pIndices[1] >= rGeometry.nVertexCount || pIndices[2] >= rGeometry.nVertexCount ) {
				return SIR_COMMON_GEOMETRY_INVALID_BVH;
			}
			float normal[3] = {};
			if ( !sirCommonRayTriangle( rGeometry, iTriangle, rRay, &fNearest, normal ) ) {
				continue;
			}
			bFound = true;
			result.fDistance = fNearest;
			result.nTriangle = iTriangle;
			result.sourceKey = rGeometry.key;
			memcpy( result.szNormal, normal, sizeof( normal ) );
			memcpy( &result.nSurfaceMaterialId, ( const uint8_t * )rGeometry.pSurfaceMaterials + ( uint64_t )iTriangle * sizeof( uint32_t ),
			        sizeof( result.nSurfaceMaterialId ) );
			for ( uint32_t axis = 0; axis < 3; ++axis ) {
				result.szPosition[axis] = rRay.szOrigin[axis] + fNearest * rRay.szDirection[axis];
			}
		}
	}
	if ( bFound ) {
		*pHit = result;
	}
	return bFound ? SIR_COMMON_GEOMETRY_HIT : SIR_COMMON_GEOMETRY_MISS;
}
