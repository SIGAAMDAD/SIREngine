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
#include <Tools/GeometryCooker/SIRDisplacementCooker.h>
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

// Every material-region boundary vertex must survive both coarse LOD cuts.
// The quantized position decode is used, rather than trusting raw source indices.
static bool testMaterialSeam( const SIRMeshPackSchema& mesh, const sirGeometryCookOutput_t& cooked )
{
	bool rootSeam[2][9] = {};
	bool middleSeam[2][9] = {};
	for ( uint32_t iPage = 0; iPage < 3; ++iPage ) {
		sirMeshPackPage_t page = {};
		CHECK( mesh.pageAt( iPage, &page ) == SIR_MESH_PACK_OK );
		const std::vector<uint8_t>& bytes = cooked.pages[iPage];
		for ( uint32_t i = 0; i < page.nClusterCount; ++i ) {
			const uint32_t iCluster = page.nFirstCluster + i;
			sirMeshPackCluster_t cluster = {};
			CHECK( mesh.clusterAt( iCluster, &cluster ) == SIR_MESH_PACK_OK );
			CHECK( cluster.nByteOffset + 8 <= bytes.size() );
			const uint32_t material = sirPackRead32( bytes.data() + cluster.nByteOffset + 4 );
			CHECK( material == 100 || material == 101 );
			for ( uint32_t v = 0; v < cluster.nVertexCount; ++v ) {
				sirMeshPackVertex_t decoded = {};
				const sirMeshPackVertexQuery_t query = { bytes.data(), bytes.size(), iPage, iCluster, v };
				CHECK( mesh.decodeVertex( query, &decoded ) == SIR_MESH_PACK_OK );
				const float y = decoded.szPosition[1];
				const int row = ( int )std::round( y );
				if ( row < 0 || row > 8 || std::abs( y - ( float )row ) > 0.001f || std::abs( decoded.szPosition[0] - 4.0f ) > 0.001f ||
				     std::abs( decoded.szPosition[2] - ( float )( ( 4 * row ) % 7 ) * 0.13f ) > 0.001f )
					continue;
				if ( iPage == 0 )
					rootSeam[material - 100][row] = true;
				else
					middleSeam[material - 100][row] = true;
			}
		}
	}
	for ( uint32_t m = 0; m < 2; ++m ) {
		for ( uint32_t row = 0; row < 9; ++row ) {
			CHECK( rootSeam[m][row] );
			CHECK( middleSeam[m][row] );
		}
	}
	return true;
}

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
	input.nMaxVerticesPerMeshlet = 32;
	input.nMaxTrianglesPerMeshlet = 16;
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
	CHECK( mesh.getGroupCount() > cooked.pages.size() );
	sirMeshPackGroup_t root = {};
	CHECK( mesh.groupAt( 0, &root ) == SIR_MESH_PACK_OK );
	CHECK( root.nParent == SIR_MESH_PACK_NO_PARENT && root.nPage == 0 );
	CHECK( root.nChildCount == 2 && root.nLevel == 2 );
	CHECK( root.fError >= 0 );
	bool hasPackedLeafPage = false;
	for ( uint32_t i = 0; i < cooked.pages.size(); ++i ) {
		sirMeshPackPage_t page = {};
		CHECK( mesh.pageAt( i, &page ) == SIR_MESH_PACK_OK );
		CHECK( i == 0 ? page.nParent == SIR_MESH_PACK_NO_PARENT : page.nParent < i );
		CHECK( mesh.verifyPage( i, cooked.pages[i].data(), cooked.pages[i].size() ) == SIR_MESH_PACK_OK );
		if ( i > input.nMaterialCount ) {
			CHECK( page.nByteCount <= 64ULL * 1024 );
			hasPackedLeafPage |= page.nClusterCount > 1;
		}
	}
	CHECK( hasPackedLeafPage );
	CHECK( testMaterialSeam( mesh, cooked ) );
	const uint64_t nScratch = SIRPackCatalog::requiredScratch( 1, 1 );
	std::vector<uint8_t> scratch( ( size_t )nScratch );
	SIRPackCatalog catalog;
	CHECK( catalog.open( cooked.catalog.data(), cooked.catalog.size(), scratch.data(), scratch.size() ) == SIR_PACK_OK );
	CHECK( catalog.getResourceCount() == 1 && catalog.getShardCount() == 1 );
	CHECK( catalog.validateShard( 0, cooked.shard.data(), cooked.shard.size() ) == SIR_PACK_OK );
	sirPackResource_t resource = {};
	CHECK( catalog.findResource( { input.key.nAssetId, input.nResourceTypeId, input.key.nVariant }, &resource ) == SIR_PACK_OK );
	CHECK( resource.nRevision == input.key.nRevision && resource.nFormatId == SIR_MESH_PACK_FORMAT_ID );
	CHECK( mesh.validateResource( resource ) == SIR_MESH_PACK_OK );
	CHECK( resource.nSchemaVersion == SIR_MESH_PACK_VERSION && resource.nKind == SIR_RESOURCE_KIND_GEOMETRY );
	CHECK( resource.nChunkCount == cooked.pages.size() + 2 );
	sirMeshPackCollision_t collision = {};
	CHECK( mesh.collisionAt( 0, &collision ) == SIR_MESH_PACK_OK );
	CHECK( collision.nFlags == SIR_MESH_PACK_COLLISION_SOURCE_FLAGS );
	for ( uint32_t i = 0; i < resource.nChunkCount; ++i ) {
		sirPackChunk_t chunk = {};
		CHECK( catalog.chunkAt( i, &chunk ) == SIR_PACK_OK );
		CHECK( mesh.validateChunk( i, chunk ) == SIR_MESH_PACK_OK );
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
	input.nMaxVerticesPerMeshlet = 32;
	xyz[0] = std::numeric_limits<float>::quiet_NaN();
	CHECK( SIRGeometryCooker::cook( input, &cooked ) == SIR_PACK_INVALID_ARGUMENT );
	xyz[0] = 0;
	const uint32_t nPrior = indices[0];
	indices[0] = input.nVertexCount + 1;
	CHECK( SIRGeometryCooker::cook( input, &cooked ) == SIR_PACK_INVALID_ARGUMENT );
	indices[0] = nPrior;
	const uint32_t nOldMaterial = materials[0].nMaterialId;
	materials[0].nMaterialId = 0xffffffffU;
	CHECK( SIRGeometryCooker::cook( input, &cooked ) == SIR_PACK_INVALID_ARGUMENT );
	materials[0].nMaterialId = nOldMaterial;
	input.bCookCollision = false;
	sirGeometryCookOutput_t noCollision;
	CHECK( SIRGeometryCooker::cook( input, &noCollision ) == SIR_PACK_OK );
	SIRPackCatalog collisionCatalog;
	CHECK( collisionCatalog.open( noCollision.catalog.data(), noCollision.catalog.size(), scratch.data(), scratch.size() ) == SIR_PACK_OK );
	CHECK( collisionCatalog.findResource( { input.key.nAssetId, input.nResourceTypeId, input.key.nVariant }, &resource ) == SIR_PACK_OK );
	CHECK( resource.nChunkCount == noCollision.pages.size() + 1 );

	// Exercise the production offline displacement -> meshoptimizer -> SIRPack
	// pipeline, with the authoring heightmap absent during runtime readback.
	std::vector<float> uv;
	for ( uint32_t y = 0; y < nGrid; ++y ) {
		for ( uint32_t x = 0; x < nGrid; ++x ) {
			uv.push_back( ( float )x / ( nGrid - 1 ) );
			uv.push_back( ( float )y / ( nGrid - 1 ) );
		}
	}
	std::vector<float> heights = { 0, 0, 0, 0, 1, 0, 0, 0, 0 };
	input.pTexcoordsUV = uv.data();
	input.bCookCollision = true;
	sirDisplacementImage_t image = { heights.data(), 3, 3 };
	sirDisplacementRecipe_t recipe = { 0.05f, 0, 0.15f, 3, 10000, 10000, SIR_DISPLACEMENT_CLAMP };
	sirGeometryCookOutput_t displaced = {};
	CHECK( SIRGeometryCooker::cookDisplaced( input, image, recipe, &displaced ) == SIR_PACK_OK );
	SIRMeshPackSchema displacedSchema;
	CHECK( displacedSchema.open( displaced.manifest.data(), displaced.manifest.size() ) == SIR_MESH_PACK_OK );
	CHECK( displacedSchema.getKey().nRevision != input.key.nRevision );
	sirGeometryCookOutput_t duplicate = {};
	CHECK( SIRGeometryCooker::cookDisplaced( input, image, recipe, &duplicate ) == SIR_PACK_OK );
	CHECK( displaced.manifest == duplicate.manifest && displaced.catalog == duplicate.catalog && displaced.shard == duplicate.shard );
	const std::vector<uint8_t> originalDisplacedManifest = displaced.manifest;
	heights[4] = 0.75f;
	CHECK( SIRGeometryCooker::cookDisplaced( input, image, recipe, &duplicate ) == SIR_PACK_OK );
	CHECK( duplicate.manifest != originalDisplacedManifest );
	// Authoring pixels are discarded; runtime validation consumes only the
	// immutable cooked catalog/shard and the portable collision source.
	heights.clear();
	heights.shrink_to_fit();
	image.pHeights = nullptr;
	SIRPackCatalog displacedCatalog;
	CHECK( displacedCatalog.open( displaced.catalog.data(), displaced.catalog.size(), scratch.data(), scratch.size() ) == SIR_PACK_OK );
	CHECK( displacedCatalog.validateShard( 0, displaced.shard.data(), displaced.shard.size() ) == SIR_PACK_OK );
	sirPackResource_t displacedResource = {};
	CHECK( displacedCatalog.findResource( { input.key.nAssetId, input.nResourceTypeId, input.key.nVariant }, &displacedResource ) ==
	       SIR_PACK_OK );
	CHECK( displacedSchema.validateResource( displacedResource ) == SIR_MESH_PACK_OK );
	sirMeshPackCollision_t displacedCollision = {};
	CHECK( displacedSchema.collisionAt( 0, &displacedCollision ) == SIR_MESH_PACK_OK );
	CHECK( displacedCollision.nRevision == displacedSchema.getKey().nRevision );
	sirPackChunk_t displacedChunk = {};
	CHECK( displacedCatalog.chunkAt( displacedCollision.nChunkId, &displacedChunk ) == SIR_PACK_OK );
	CHECK( displacedSchema.verifyCollision( 0, displaced.shard.data() + displacedChunk.nOffset, displacedChunk.nEncodedBytes ) ==
	       SIR_MESH_PACK_OK );
	return true;
}

int main( void )
{
	if ( !testGrid() )
		return 1;
	puts( "SIRGeometryCooker integration checks passed" );
	return 0;
}
