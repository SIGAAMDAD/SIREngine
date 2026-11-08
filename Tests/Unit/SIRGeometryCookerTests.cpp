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
#include <Tools/GeometryCooker/SIRGeometryCooker.h>
#include <Resources/SIRPackCatalog.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "GeometryCooker check failed at %s:%u: %s\n", __FILE__, __LINE__, #expression );                              \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool testGrid( void )
{
	std::vector<float> xyz;
	std::vector<uint32_t> indices;
	sirGeometryCookMaterial_t materials[2] = {};
	static constexpr uint32_t nGrid = 9;
	for ( uint32_t y = 0; y < nGrid; ++y ) {
		for ( uint32_t x = 0; x < nGrid; ++x ) {
			xyz.push_back( ( float )x );
			xyz.push_back( ( float )y );
			xyz.push_back( ( float )( ( x * y ) % 7 ) * 0.13f );
		}
	}
	for ( uint32_t material = 0; material < 2; ++material ) {
		materials[material].nFirstIndex = ( uint32_t )indices.size();
		materials[material].nMaterialId = 100 + material;
		for ( uint32_t y = 0; y < nGrid - 1; ++y ) {
			for ( uint32_t x = material * 4; x < material * 4 + 4; ++x ) {
				const uint32_t a = y * nGrid + x;
				const uint32_t b = a + 1;
				const uint32_t c = a + nGrid;
				const uint32_t d = c + 1;
				indices.insert( indices.end(), { a, b, c, b, d, c } );
			}
		}
		materials[material].nIndexCount = ( uint32_t )indices.size() - materials[material].nFirstIndex;
	}
	sirGeometryCookInput_t input = {};
	input.key = { 0x7531ULL, 42, 3, 0 };
	input.nResourceTypeId = 103;
	input.nPackageId = 0x123456;
	input.nGeneration = 11;
	input.pPositionsXYZ = xyz.data();
	input.nVertexCount = ( uint32_t )xyz.size() / 3;
	input.pIndices = indices.data();
	input.nIndexCount = ( uint32_t )indices.size();
	input.pMaterials = materials;
	input.nMaterialCount = 2;
	input.nMaxVerticesPerMeshlet = 64;
	input.nMaxTrianglesPerMeshlet = 124;
	input.nTargetRootTriangles = 24;
	input.fMaxSimplificationError = 0.5f;
	input.bCookCollision = true;
	sirGeometryCookOutput_t cooked;
	CHECK( SIRGeometryCooker::cook( input, &cooked ) == SIR_PACK_OK );
	CHECK( cooked.pages.size() > 1 );
	CHECK( !cooked.catalog.empty() && !cooked.shard.empty() );
	SIRMeshPackSchema mesh;
	CHECK( mesh.open( cooked.manifest.data(), cooked.manifest.size() ) == SIR_MESH_PACK_OK );
	CHECK( mesh.getKey().nAssetId == input.key.nAssetId );
	CHECK( mesh.getPageCount() == cooked.pages.size() );
	CHECK( mesh.getGroupCount() == cooked.pages.size() );
	sirMeshPackGroup_t root = {};
	CHECK( mesh.groupAt( 0, &root ) == SIR_MESH_PACK_OK );
	CHECK( root.nParent == SIR_MESH_PACK_NO_PARENT && root.nPage == 0 );
	CHECK( root.nChildCount == 2 && root.nLevel == 2 );
	CHECK( root.fError >= 0 );
	for ( uint32_t i = 0; i < cooked.pages.size(); ++i ) {
		sirMeshPackPage_t page = {};
		CHECK( mesh.pageAt( i, &page ) == SIR_MESH_PACK_OK );
		CHECK( i == 0 ? page.nParent == SIR_MESH_PACK_NO_PARENT : page.nParent < i );
		CHECK( mesh.verifyPage( i, cooked.pages[i].data(), cooked.pages[i].size() ) == SIR_MESH_PACK_OK );
	}
	const uint64_t nScratch = SIRPackCatalog::requiredScratch( 1, 1 );
	std::vector<uint8_t> scratch( ( size_t )nScratch );
	SIRPackCatalog catalog;
	CHECK( catalog.open( cooked.catalog.data(), cooked.catalog.size(), scratch.data(), scratch.size() ) == SIR_PACK_OK );
	CHECK( catalog.getResourceCount() == 1 && catalog.getShardCount() == 1 );
	CHECK( catalog.validateShard( 0, cooked.shard.data(), cooked.shard.size() ) == SIR_PACK_OK );
	sirPackResource_t resource = {};
	CHECK( catalog.findResource( { input.key.nAssetId, input.nResourceTypeId, input.key.nVariant }, &resource ) == SIR_PACK_OK );
	CHECK( resource.nRevision == input.key.nRevision && resource.nFormatId == SIR_MESH_PACK_FORMAT_ID );
	CHECK( resource.nSchemaVersion == SIR_MESH_PACK_VERSION && resource.nKind == SIR_RESOURCE_KIND_GEOMETRY );
	CHECK( resource.nChunkCount == cooked.pages.size() + 2 );
	sirMeshPackCollision_t collision = {};
	CHECK( mesh.collisionAt( 0, &collision ) == SIR_MESH_PACK_OK );
	CHECK( collision.nFlags == SIR_MESH_PACK_COLLISION_SOURCE_FLAGS );
	for ( uint32_t i = 0; i < resource.nChunkCount; ++i ) {
		sirPackChunk_t chunk = {};
		CHECK( catalog.chunkAt( i, &chunk ) == SIR_PACK_OK );
		CHECK( chunk.nShardIndex == 0 && chunk.nOffset <= cooked.shard.size() );
		CHECK( chunk.nEncodedBytes <= cooked.shard.size() - chunk.nOffset );
		CHECK( catalog.verifyDecodedChunk( i, cooked.shard.data() + chunk.nOffset, chunk.nEncodedBytes ) == SIR_PACK_OK );
		if ( i == collision.nChunkId ) {
			CHECK( mesh.verifyCollision( 0, cooked.shard.data() + chunk.nOffset, chunk.nEncodedBytes ) == SIR_MESH_PACK_OK );
		}
	}
	sirGeometryCookOutput_t repeat;
	CHECK( SIRGeometryCooker::cook( input, &repeat ) == SIR_PACK_OK );
	CHECK( cooked.manifest == repeat.manifest && cooked.catalog == repeat.catalog && cooked.shard == repeat.shard );
	const std::vector<uint8_t> original = cooked.catalog;
	input.nIndexCount -= 1;
	CHECK( SIRGeometryCooker::cook( input, &cooked ) == SIR_PACK_INVALID_ARGUMENT );
	CHECK( cooked.catalog == original );
	input.nIndexCount += 1;
	input.nMaxVerticesPerMeshlet = 17;
	CHECK( SIRGeometryCooker::cook( input, &cooked ) == SIR_PACK_INVALID_ARGUMENT );
	input.nMaxVerticesPerMeshlet = 64;
	xyz[0] = std::numeric_limits<float>::quiet_NaN();
	CHECK( SIRGeometryCooker::cook( input, &cooked ) == SIR_PACK_INVALID_ARGUMENT );
	xyz[0] = 0;
	const uint32_t nPrior = indices[0];
	indices[0] = input.nVertexCount + 1;
	CHECK( SIRGeometryCooker::cook( input, &cooked ) == SIR_PACK_INVALID_ARGUMENT );
	indices[0] = nPrior;
	input.bCookCollision = false;
	sirGeometryCookOutput_t noCollision;
	CHECK( SIRGeometryCooker::cook( input, &noCollision ) == SIR_PACK_OK );
	SIRPackCatalog collisionCatalog;
	CHECK( collisionCatalog.open( noCollision.catalog.data(), noCollision.catalog.size(), scratch.data(), scratch.size() ) == SIR_PACK_OK );
	CHECK( collisionCatalog.findResource( { input.key.nAssetId, input.nResourceTypeId, input.key.nVariant }, &resource ) == SIR_PACK_OK );
	CHECK( resource.nChunkCount == noCollision.pages.size() + 1 );
	return true;
}

int main( void )
{
	if ( !testGrid() )
		return 1;
	puts( "SIRGeometryCooker integration checks passed" );
	return 0;
}
