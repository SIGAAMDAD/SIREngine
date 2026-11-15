/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
This Source Code Form is subject to the Mozilla Public License, v2.
===========================================================================
*/

#include <Tools/GeometryCooker/SIRSharedBvhCooker.h>

#include <stdio.h>
#include <string.h>

#define REQUIRE( b )                                                                                                                       \
	do {                                                                                                                                   \
		if ( !( b ) ) {                                                                                                                    \
			fprintf( stderr, "BVH cooker assertion %d: %s\n", __LINE__, #b );                                                              \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

int main( void )
{
	const float szPositions[] = { 8, 0, 0, 9, 0, 0, 8, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0,
	                              4, 0, 0, 5, 0, 0, 4, 1, 0, 2, 0, 0, 3, 0, 0, 2, 1, 0 };
	const uint32_t szIndices[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	const uint32_t szMaterials[] = { 2, 1, 2, 1 };
	const uint32_t szSemantics[] = { 20, 10, 21, 11 };
	sirSharedSurfaceMaterial_t materials[2] = {};
	materials[0].nMaterialId = 1;
	materials[1].nMaterialId = 2;
	materials[0].fDensity = 1;
	materials[1].fDensity = 1;
	const sirStaticBvhCookInput_t input = { 100, 5, szPositions, szIndices, szMaterials, szSemantics, materials, 12, 4, 2 };
	sirSharedBvhNode_t szNodes[7] = {};
	sirSharedBvhTriangle_t szTriangles[4] = {};
	uint32_t szOrder[4] = {};
	uint32_t szScratch[15] = {};
	uint8_t szOutput[2048] = {};
	const sirStaticBvhCookWorkspace_t workspace = { szNodes, szTriangles, szOrder, szScratch, szOutput, sizeof( szOutput ), 7, 4, 4, 15 };
	REQUIRE( SIRSharedBvhCooker::requiredNodes( 4 ) == 7 );
	REQUIRE( SIRSharedBvhCooker::cook( input, workspace ) == SIR_SHARED_BVH_OK );
	const uint64_t nBytes = SIRSharedGeometryBVH::requiredBytes( 7, 4, 1, 2 );
	uint8_t szFirst[2048] = {};
	memcpy( szFirst, szOutput, ( size_t )nBytes );

	SIRSharedGeometryBVH view;
	REQUIRE( view.open( szOutput, nBytes, szScratch, 15 ) == SIR_SHARED_BVH_OK );
	REQUIRE( view.getAssetId() == 100 && view.getRevision() == 5 && view.getNodeCount() == 7 );
	sirSharedBvhTriangle_t triangle = {};
	REQUIRE( view.triangleAt( 0, &triangle ) == SIR_SHARED_BVH_OK );
	REQUIRE( triangle.nSourceTriangleIndex == 1 && triangle.nStableTriangleId == 1 && triangle.nSurfaceMaterialId == 1 );
	REQUIRE( view.triangleAt( 3, &triangle ) == SIR_SHARED_BVH_OK );
	REQUIRE( triangle.nSourceTriangleIndex == 0 && triangle.nStableTriangleId == 0 && triangle.nSurfaceMaterialId == 2 );
	REQUIRE( SIRSharedBvhCooker::cook( input, workspace ) == SIR_SHARED_BVH_OK );
	REQUIRE( memcmp( szFirst, szOutput, ( size_t )nBytes ) == 0 );

	uint32_t szInvalidIndices[12] = {};
	memcpy( szInvalidIndices, szIndices, sizeof( szIndices ) );
	szInvalidIndices[0] = 999;
	sirStaticBvhCookInput_t invalid = input;
	invalid.pTriangleIndices = szInvalidIndices;
	REQUIRE( SIRSharedBvhCooker::cook( invalid, workspace ) == SIR_SHARED_BVH_INVALID_FORMAT );
	return 0;
}
