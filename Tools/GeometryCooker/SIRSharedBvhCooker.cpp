/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Tools/GeometryCooker/SIRSharedBvhCooker.h>

#include <float.h>
#include <limits.h>
#include <math.h>

// No persistent triangle or vertex copy: order is an offline scratch permutation.
typedef struct sirStaticBvhCookState_s
{
	const sirStaticBvhCookInput_t *pInput;
	const sirStaticBvhCookWorkspace_t *pWorkspace;
	uint32_t nNextNode;
} sirStaticBvhCookState_t;

static double sirBvhCentroid( const sirStaticBvhCookInput_t& input, uint32_t iTriangle, uint32_t iAxis ) noexcept
{
	const uint32_t iStart = iTriangle * 3;
	const uint32_t iA = input.pTriangleIndices[iStart];
	const uint32_t iB = input.pTriangleIndices[iStart + 1];
	const uint32_t iC = input.pTriangleIndices[iStart + 2];
	return ( ( double )input.pPositionsXYZ[iA * 3 + iAxis] + ( double )input.pPositionsXYZ[iB * 3 + iAxis] +
	         ( double )input.pPositionsXYZ[iC * 3 + iAxis] ) /
	       3.0;
}

static bool sirBvhTriangleBefore( const sirStaticBvhCookInput_t& input, uint32_t iLeft, uint32_t iRight, uint32_t iAxis ) noexcept
{
	const double fLeft = sirBvhCentroid( input, iLeft, iAxis );
	const double fRight = sirBvhCentroid( input, iRight, iAxis );
	return fLeft < fRight || ( fLeft == fRight && iLeft < iRight );
}

static void sirBvhCookSift( uint32_t *pOrder, uint32_t iRoot, uint32_t nCount, uint32_t iAxis,
                            const sirStaticBvhCookInput_t& input ) noexcept
{
	while ( ( uint64_t )iRoot * 2U + 1U < nCount ) {
		const uint32_t iChild = iRoot * 2U + 1U;
		uint32_t iSwap = iRoot;
		if ( sirBvhTriangleBefore( input, pOrder[iSwap], pOrder[iChild], iAxis ) ) {
			iSwap = iChild;
		}
		if ( iChild + 1U < nCount && sirBvhTriangleBefore( input, pOrder[iSwap], pOrder[iChild + 1U], iAxis ) ) {
			iSwap = iChild + 1U;
		}
		if ( iSwap == iRoot ) {
			return;
		}
		const uint32_t nSaved = pOrder[iRoot];
		pOrder[iRoot] = pOrder[iSwap];
		pOrder[iSwap] = nSaved;
		iRoot = iSwap;
	}
}

static void sirBvhCookSort( uint32_t *pOrder, uint32_t nCount, uint32_t iAxis, const sirStaticBvhCookInput_t& input ) noexcept
{
	for ( uint32_t iStart = nCount / 2U; iStart != 0; --iStart ) {
		sirBvhCookSift( pOrder, iStart - 1U, nCount, iAxis, input );
	}
	for ( uint32_t iEnd = nCount; iEnd > 1; --iEnd ) {
		const uint32_t nSaved = pOrder[0];
		pOrder[0] = pOrder[iEnd - 1U];
		pOrder[iEnd - 1U] = nSaved;
		sirBvhCookSift( pOrder, 0, iEnd - 1U, iAxis, input );
	}
}

static bool sirBvhCookTriangleBounds( const sirStaticBvhCookInput_t& input, uint32_t iTriangle, float *pMinimum, float *pMaximum ) noexcept
{
	for ( uint32_t iCorner = 0; iCorner < 3; ++iCorner ) {
		const uint32_t iVertex = input.pTriangleIndices[iTriangle * 3U + iCorner];
		if ( iVertex >= input.nVertexCount ) {
			return false;
		}
		for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
			const float fValue = input.pPositionsXYZ[iVertex * 3U + iAxis];
			if ( !isfinite( fValue ) ) {
				return false;
			}
			if ( fValue < pMinimum[iAxis] ) {
				pMinimum[iAxis] = fValue;
			}
			if ( fValue > pMaximum[iAxis] ) {
				pMaximum[iAxis] = fValue;
			}
		}
	}
	return true;
}

static bool sirBvhCookBuildNode( sirStaticBvhCookState_t& state, uint32_t iNode, uint32_t iFirst, uint32_t nTriangles ) noexcept
{
	const sirStaticBvhCookInput_t& input = *state.pInput;
	const sirStaticBvhCookWorkspace_t& workspace = *state.pWorkspace;
	sirSharedBvhNode_t& node = workspace.pNodes[iNode];
	node = {};
	for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
		node.szMinimum[iAxis] = FLT_MAX;
		node.szMaximum[iAxis] = -FLT_MAX;
	}

	for ( uint32_t iTriangle = iFirst; iTriangle < iFirst + nTriangles; ++iTriangle ) {
		if ( !sirBvhCookTriangleBounds( input, workspace.pTriangleOrder[iTriangle], node.szMinimum, node.szMaximum ) ) {
			return false;
		}
	}

	uint32_t iLargestAxis = 0;
	double fLargestExtent = ( double )node.szMaximum[0] - node.szMinimum[0];
	for ( uint32_t iAxis = 1; iAxis < 3; ++iAxis ) {
		const double fExtent = ( double )node.szMaximum[iAxis] - node.szMinimum[iAxis];
		if ( fExtent > fLargestExtent ) {
			iLargestAxis = iAxis;
			fLargestExtent = fExtent;
		}
	}
	node.fGeometricError = fLargestExtent >= FLT_MAX ? FLT_MAX : ( float )fLargestExtent;
	if ( nTriangles == 1 ) {
		node.nFlags = SIR_SHARED_BVH_LEAF;
		node.nFirstChild = SIR_SHARED_BVH_NO_CHILD;
		node.nSecondChild = SIR_SHARED_BVH_NO_CHILD;
		node.nFirstTriangle = iFirst;
		node.nTriangleCount = 1;
		return true;
	}

	// Stable centroid ordering defines a reproducible spatial median even
	// when the source has coplanar/identical triangles.
	sirBvhCookSort( workspace.pTriangleOrder + iFirst, nTriangles, iLargestAxis, input );
	const uint32_t nLeft = nTriangles / 2U;
	const uint32_t nRight = nTriangles - nLeft;
	node.nFirstChild = state.nNextNode++;
	node.nSecondChild = state.nNextNode++;
	if ( !sirBvhCookBuildNode( state, node.nFirstChild, iFirst, nLeft ) ) {
		return false;
	}
	return sirBvhCookBuildNode( state, node.nSecondChild, iFirst + nLeft, nRight );
}

uint32_t SIRSharedBvhCooker::requiredNodes( uint32_t nTriangles ) noexcept
{
	if ( nTriangles == 0 || nTriangles > ( SIR_SHARED_BVH_MAX_RECORDS + 1U ) / 2U ) {
		return 0;
	}
	return nTriangles * 2U - 1U;
}

sirSharedBvhResult_t SIRSharedBvhCooker::cook( const sirStaticBvhCookInput_t& input, const sirStaticBvhCookWorkspace_t& workspace ) noexcept
{
	const uint32_t nNodes = requiredNodes( input.nTriangleCount );
	if ( input.nAssetId == 0 || input.nRevision == 0 || nNodes == 0 || input.nVertexCount == 0 || input.nMaterialCount == 0 ||
	     input.pPositionsXYZ == nullptr || input.pTriangleIndices == nullptr || input.pTriangleMaterialIds == nullptr ||
	     input.pMaterials == nullptr || workspace.pNodes == nullptr || workspace.pTriangles == nullptr ||
	     workspace.pTriangleOrder == nullptr || workspace.pValidationScratch == nullptr || workspace.pOutput == nullptr ) {
		return SIR_SHARED_BVH_INVALID_ARGUMENT;
	}
	if ( input.nVertexCount > UINT32_MAX / 3U || input.nTriangleCount > UINT32_MAX / 3U || workspace.nNodeCapacity < nNodes ||
	     workspace.nTriangleCapacity < input.nTriangleCount || workspace.nOrderCapacity < input.nTriangleCount ||
	     workspace.nValidationCapacity < SIRSharedGeometryBVH::requiredValidationWords( nNodes, input.nTriangleCount ) ||
	     workspace.nOutputCapacity < SIRSharedGeometryBVH::requiredBytes( nNodes, input.nTriangleCount, 1, input.nMaterialCount ) ) {
		return SIR_SHARED_BVH_OUT_OF_CAPACITY;
	}

	for ( uint32_t iTriangle = 0; iTriangle < input.nTriangleCount; ++iTriangle ) {
		workspace.pTriangleOrder[iTriangle] = iTriangle;
	}
	sirStaticBvhCookState_t state = { &input, &workspace, 1 };
	if ( !sirBvhCookBuildNode( state, 0, 0, input.nTriangleCount ) || state.nNextNode != nNodes ) {
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}

	for ( uint32_t iTriangle = 0; iTriangle < input.nTriangleCount; ++iTriangle ) {
		const uint32_t iSource = workspace.pTriangleOrder[iTriangle];
		workspace.pTriangles[iTriangle] = { iSource, input.pTriangleMaterialIds[iSource],
		                                    input.pTriangleSemanticIds != nullptr ? input.pTriangleSemanticIds[iSource] : 0, iSource };
	}
	const sirSharedBvhProxy_t proxy = { 15U, 0, 0, input.nTriangleCount, workspace.pNodes[0].fGeometricError, 0 };
	const sirSharedBvhDescriptor_t descriptor = { input.nAssetId,
	                                              input.nRevision,
	                                              workspace.pNodes,
	                                              workspace.pTriangles,
	                                              &proxy,
	                                              input.pMaterials,
	                                              nNodes,
	                                              input.nTriangleCount,
	                                              1,
	                                              input.nMaterialCount,
	                                              workspace.pValidationScratch,
	                                              workspace.nValidationCapacity };
	return SIRSharedGeometryBVH::write( descriptor, workspace.pOutput, workspace.nOutputCapacity );
}
