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
#include <Tools/GeometryCooker/SIRDisplacementCooker.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Displacement check failed at %s:%u: %s\n", __FILE__, __LINE__, #expression );                                \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool testAdaptiveBake( void )
{
	const float xyz[12] = { 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0 };
	const float uv[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
	const float normals[12] = { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 };
	const uint32_t indices[6] = { 0, 1, 2, 1, 3, 2 };
	const sirGeometryCookMaterial_t materials[2] = { { 0, 3, 100 }, { 3, 3, 101 } };
	float heights[9] = { 0, 0, 0, 0, 1, 0, 0, 0, 0 };
	sirGeometryCookInput_t input = {};
	input.key = { 911, 1, 0, 0 };
	input.nResourceTypeId = 103;
	input.nPackageId = 42;
	input.nGeneration = 1;
	input.pPositionsXYZ = xyz;
	input.nVertexCount = 4;
	input.pTexcoordsUV = uv;
	input.pNormalsXYZ = normals;
	input.pIndices = indices;
	input.nIndexCount = 6;
	input.pMaterials = materials;
	input.nMaterialCount = 2;
	input.nMaxVerticesPerMeshlet = 32;
	input.nMaxTrianglesPerMeshlet = 16;
	input.nTargetRootTriangles = 2;
	input.fMaxSimplificationError = 0.3f;
	input.bCookCollision = true;
	const sirDisplacementImage_t image = { heights, 3, 3 };
	sirDisplacementRecipe_t recipe = { 1.0f, 0, 0.1f, 5, 10000, 10000, SIR_DISPLACEMENT_CLAMP };
	sirDisplacementBakeResult_t a = {};
	CHECK( SIRDisplacementCooker::bake( input, image, recipe, &a ) == SIR_PACK_OK );
	CHECK( a.indices.size() > 6 && a.positionsXYZ.size() / 3 > 4 );
	CHECK( a.materials.size() == 2 && a.materials[0].nFirstIndex == 0 );
	CHECK( a.materials[0].nIndexCount > 3 && a.materials[1].nFirstIndex == a.materials[0].nIndexCount );
	CHECK( a.materials[1].nIndexCount > 3 && a.indices.size() == a.materials[1].nFirstIndex + a.materials[1].nIndexCount );
	// The diagonal shared by both material regions has a single height-1
	// vertex identity; incident faces must use the exact same index.
	uint32_t high = UINT32_MAX;
	for ( uint32_t v = 0; v < a.positionsXYZ.size() / 3; ++v ) {
		if ( std::abs( a.positionsXYZ[v * 3] - 0.5f ) < 1.0e-6f && std::abs( a.positionsXYZ[v * 3 + 1] - 0.5f ) < 1.0e-6f &&
		     std::abs( a.positionsXYZ[v * 3 + 2] - 1.0f ) < 1.0e-6f ) {
			CHECK( high == UINT32_MAX );
			high = v;
		}
	}
	CHECK( high != UINT32_MAX );
	for ( uint32_t m = 0; m < 2; ++m ) {
		bool used = false;
		for ( uint32_t i = a.materials[m].nFirstIndex; i < a.materials[m].nFirstIndex + a.materials[m].nIndexCount; ++i )
			used |= a.indices[i] == high;
		CHECK( used );
	}
	sirDisplacementBakeResult_t b = {};
	CHECK( SIRDisplacementCooker::bake( input, image, recipe, &b ) == SIR_PACK_OK );
	CHECK( a.positionsXYZ == b.positionsXYZ && a.texcoordsUV == b.texcoordsUV && a.indices == b.indices );
	CHECK( a.nRevision == b.nRevision && memcmp( a.szAuthoringDigest, b.szAuthoringDigest, 32 ) == 0 );
	heights[4] = 0.75f;
	CHECK( SIRDisplacementCooker::bake( input, image, recipe, &b ) == SIR_PACK_OK );
	CHECK( a.nRevision != b.nRevision );
	heights[4] = 1;
	recipe.fAmplitude = 0.5f;
	CHECK( SIRDisplacementCooker::bake( input, image, recipe, &b ) == SIR_PACK_OK );
	CHECK( a.nRevision != b.nRevision );
	const std::vector<uint32_t> savedIndices = a.indices;
	recipe.nMaxTriangles = 2;
	CHECK( SIRDisplacementCooker::bake( input, image, recipe, &a ) == SIR_PACK_OUT_OF_CAPACITY );
	CHECK( a.indices == savedIndices );
	recipe.nMaxTriangles = 1000;
	heights[4] = std::numeric_limits<float>::quiet_NaN();
	CHECK( SIRDisplacementCooker::bake( input, image, recipe, &a ) == SIR_PACK_INVALID_ARGUMENT );
	CHECK( a.indices == savedIndices );
	return true;
}

static bool testNoSubdivisionAndRepeat( void )
{
	const float xyz[9] = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
	const float uv[6] = { 0, 0, 1, 0, 0, 1 };
	const uint32_t indices[3] = { 0, 1, 2 };
	const sirGeometryCookMaterial_t material = { 0, 3, 1 };
	const float heights[4] = { 0, 1, 1, 0 };
	sirGeometryCookInput_t input = {};
	input.key = { 101, 4, 0, 0 };
	input.pPositionsXYZ = xyz;
	input.nVertexCount = 3;
	input.pTexcoordsUV = uv;
	input.pIndices = indices;
	input.nIndexCount = 3;
	input.pMaterials = &material;
	input.nMaterialCount = 1;
	const sirDisplacementImage_t image = { heights, 2, 2 };
	const sirDisplacementRecipe_t recipe = { 0, 0.5f, 0.001f, 4, 128, 128, SIR_DISPLACEMENT_REPEAT };
	sirDisplacementBakeResult_t a = {};
	CHECK( SIRDisplacementCooker::bake( input, image, recipe, &a ) == SIR_PACK_OK );
	CHECK( a.indices.size() == 3 && a.positionsXYZ.size() == 9 );
	for ( uint32_t i = 0; i < 9; ++i )
		CHECK( a.positionsXYZ[i] == xyz[i] );
	return true;
}

static bool testIndependentMaterialCorners( void )
{
	// Two material faces use different OBJ vertex identities on the shared
	// diagonal. The physical edge must subdivide on both sides.
	const float xyz[18] = { 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0 };
	float uv[12] = { 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1 };
	const float normals[18] = { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 };
	const uint32_t indices[6] = { 0, 1, 2, 3, 4, 5 };
	const sirGeometryCookMaterial_t materials[2] = { { 0, 3, 100 }, { 3, 3, 101 } };
	const float heights[9] = { 0, 0, 0, 0, 1, 0, 0, 0, 0 };
	sirGeometryCookInput_t input = {};
	input.key = { 123, 5, 0, 0 };
	input.pPositionsXYZ = xyz;
	input.nVertexCount = 6;
	input.pTexcoordsUV = uv;
	input.pNormalsXYZ = normals;
	input.pIndices = indices;
	input.nIndexCount = 6;
	input.pMaterials = materials;
	input.nMaterialCount = 2;
	const sirDisplacementImage_t image = { heights, 3, 3 };
	const sirDisplacementRecipe_t recipe = { 1.0f, 0, 0.1f, 5, 10000, 10000, SIR_DISPLACEMENT_CLAMP };
	sirDisplacementBakeResult_t out = {};
	CHECK( SIRDisplacementCooker::bake( input, image, recipe, &out ) == SIR_PACK_OK );
	uint32_t seamVertices[2] = {};
	for ( uint32_t m = 0; m < 2; ++m ) {
		for ( uint32_t i = out.materials[m].nFirstIndex; i < out.materials[m].nFirstIndex + out.materials[m].nIndexCount; ++i ) {
			const uint32_t v = out.indices[i];
			if ( std::abs( out.positionsXYZ[3 * v] - 0.5f ) < 1.0e-6f && std::abs( out.positionsXYZ[3 * v + 1] - 0.5f ) < 1.0e-6f &&
			     std::abs( out.positionsXYZ[3 * v + 2] - 1.0f ) < 1.0e-6f ) {
				seamVertices[m] = v + 1;
			}
		}
		CHECK( seamVertices[m] != 0 );
	}
	CHECK( seamVertices[0] != seamVertices[1] );
	// Conflicting UV seams are rejected without partial output: the two
	// physically coincident midpoint samples otherwise produce a crack.
	uv[6] = 0;
	uv[7] = 0;
	uv[10] = 0;
	uv[11] = 0;
	const std::vector<uint32_t> prior = out.indices;
	CHECK( SIRDisplacementCooker::bake( input, image, recipe, &out ) == SIR_PACK_INVALID_FORMAT );
	CHECK( out.indices == prior );
	return true;
}

int main( void )
{
	if ( !testAdaptiveBake() || !testNoSubdivisionAndRepeat() || !testIndependentMaterialCorners() )
		return 1;
	puts( "Offline displacement bake checks passed" );
	return 0;
}
