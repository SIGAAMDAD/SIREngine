/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Mesh/SIRMeshGRDGIntegration.h>
#include <Mesh/SIRMeshPackSchema.h>
#include <Mesh/SIRCommonGeometryQueries.h>
#include <Resources/SIRResourceGraph.h>
#include <Resources/SIRPackCatalog.h>
#include <Tools/GeometryCooker/SIRGeometryCooker.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

#define CHECK( condition )                                                                                                                 \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Cooked GRDG shared backing failure %s:%u: %s\n", __FILE__, __LINE__, #condition );                           \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

struct sirCookedAllocatorCounters_t
{
	uint64_t nLiveBytes;
	uint32_t nCalls;
};

static void *sirCookedAlloc( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	if ( nBytes == 0 || nBytes > SIZE_MAX )
		return nullptr;
	void *p = nullptr;
#ifdef _WIN32
	p = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	if ( posix_memalign( &p, nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment, ( size_t )nBytes ) != 0 )
		return nullptr;
#endif
	if ( p ) {
		sirCookedAllocatorCounters_t *pCounts = ( sirCookedAllocatorCounters_t * )pContext;
		pCounts->nLiveBytes += nBytes;
		++pCounts->nCalls;
	}
	return p;
}

static void sirCookedFree( void *pContext, void *pData, uint64_t nBytes, uint32_t, uint32_t ) noexcept
{
	( ( sirCookedAllocatorCounters_t * )pContext )->nLiveBytes -= nBytes;
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static bool sirCookedPublish( SIRResourceGraph& graph, const sirGeometryCookOutput_t& cooked, const sirGeometryCookInput_t& input,
                              SIRResourceHandle *pHandle )
{
	std::vector<uint8_t> scratch( ( size_t )SIRPackCatalog::requiredScratch( 1, 1 ) );
	SIRPackCatalog catalog;
	CHECK( catalog.open( cooked.catalog.data(), cooked.catalog.size(), scratch.data(), scratch.size() ) == SIR_PACK_OK );
	sirPackResource_t resource = {};
	CHECK( catalog.findResource( { input.key.nAssetId, input.nResourceTypeId, input.key.nVariant }, &resource ) == SIR_PACK_OK );
	SIRResourceGraphWriter *pWriter = graph.getWriter();
	CHECK( pWriter != nullptr );
	std::vector<SIRResourceChunkPublication> publications( resource.nChunkCount );
	for ( uint32_t i = 0; i < resource.nChunkCount; ++i ) {
		sirPackChunk_t chunk = {};
		CHECK( catalog.chunkAt( i, &chunk ) == SIR_PACK_OK );
		CHECK( chunk.nEncodedBytes == chunk.nDecodedBytes );
		SIRResourceStagingHandle staging = {};
		SIRResourceMutableBytes bytes = {};
		CHECK( pWriter->allocateStaging( chunk.nDecodedBytes, 16, 7, &staging, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( chunk.nOffset <= cooked.shard.size() && chunk.nEncodedBytes <= cooked.shard.size() - chunk.nOffset );
		memcpy( bytes.m_pData, cooked.shard.data() + chunk.nOffset, chunk.nDecodedBytes );
		publications[i] = { { i }, { 1000ULL + i, 2000ULL + i, SIR_MESH_PACK_FORMAT_ID, 0 }, staging, false };
	}
	const SIRResourcePublication publication = { { input.key.nAssetId, input.nResourceTypeId, input.key.nVariant },
	                                             SIR_RESOURCE_KIND_GEOMETRY,
	                                             SIR_MESH_PACK_VERSION,
	                                             SIR_MESH_PACK_FORMAT_ID,
	                                             publications.data(),
	                                             ( uint32_t )publications.size(),
	                                             nullptr,
	                                             0 };
	return pWriter->publish( publication, pHandle ) == SIR_RESOURCE_RESULT_SUCCESS;
}

static bool testCookOnceBorrowFourTimes( void )
{
	float xyz[9] = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
	uint32_t indices[3] = { 0, 1, 2 };
	const sirGeometryCookMaterial_t material = { 0, 3, 17 };
	sirGeometryCookInput_t input = {};
	input.key = { 1551, 1, 7, 0 };
	input.nResourceTypeId = 103;
	input.nPackageId = 300;
	input.nGeneration = 1;
	input.pPositionsXYZ = xyz;
	input.nVertexCount = 3;
	input.pIndices = indices;
	input.nIndexCount = 3;
	input.pMaterials = &material;
	input.nMaterialCount = 1;
	input.nMaxVerticesPerMeshlet = 32;
	input.nMaxTrianglesPerMeshlet = 16;
	input.nTargetRootTriangles = 3;
	input.fMaxSimplificationError = 0.5f;
	input.bCookCollision = true;
	sirGeometryCookOutput_t cooked;
	CHECK( SIRGeometryCooker::cook( input, &cooked ) == SIR_PACK_OK );
	SIRMeshPackSchema schema;
	CHECK( schema.open( cooked.manifest.data(), cooked.manifest.size() ) == SIR_MESH_PACK_OK );
	sirMeshPackCollision_t collision = {};
	CHECK( schema.collisionAt( 0, &collision ) == SIR_MESH_PACK_OK );
	CHECK( collision.nBVHNodeCount == 1 && collision.nBVHOrderCount == 1 );
	sirCookedAllocatorCounters_t counters = {};
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &counters, sirCookedAlloc, sirCookedFree };
	config.m_nMetadataDomain = 7;
	config.m_nMaxResources = 2;
	config.m_nMaxStaging = 12;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 8 * 1024 * 1024;
	{
		SIRResourceGraph graph( config );
		CHECK( graph.isReady() );
		SIRResourceHandle resourceHandle = {};
		CHECK( sirCookedPublish( graph, cooked, input, &resourceHandle ) );
		CHECK( graph.getStats().m_nUniqueChunks == schema.getPageCount() + 3 );
		const uint64_t nPayloadBytes = graph.getStats().m_nPayloadBytes;
		std::vector<uint8_t> storage( ( size_t )SIRMeshCache::requiredBytes( 1 ) + SIRMeshCache::requiredAlignment() );
		const uintptr_t nAligned = ( ( uintptr_t )storage.data() + SIRMeshCache::requiredAlignment() - 1 ) &
		                           ~( ( uintptr_t )SIRMeshCache::requiredAlignment() - 1 );
		SIRMeshCache cache;
		CHECK( cache.initialize( ( void * )nAligned, storage.size() - ( nAligned - ( uintptr_t )storage.data() ), 1 ) == SIR_MESH_OK );
		sirMeshGRDGSourceLease_t lease = {};
		sirMeshHandle_t handle = {};
		const uint8_t *pCanonicalCollision = nullptr;
		const uint8_t *pCanonicalBVH = nullptr;
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			CHECK( graph.resolve( scope, resourceHandle, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRResourceConstBytes collBytes = {}, bvhBytes = {};
			CHECK( view.getChunk( { collision.nChunkId }, &collBytes ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( view.getChunk( { collision.nBVHChunkId }, &bvhBytes ) == SIR_RESOURCE_RESULT_SUCCESS );
			pCanonicalCollision = ( const uint8_t * )collBytes.m_pData;
			pCanonicalBVH = ( const uint8_t * )bvhBytes.m_pData;
			sirMeshGRDGSourceRequest_t request = {};
			request.pGraph = &graph;
			request.pScope = &scope;
			request.pView = &view;
			request.key = input.key;
			request.nResourceTypeId = input.nResourceTypeId;
			request.nProviderId = 99;
			request.nVertexCount = input.nVertexCount;
			request.nIndexCount = input.nIndexCount;
			request.packedCollisionChunk = { collision.nChunkId };
			request.packedBVHChunk = { collision.nBVHChunkId };
			CHECK( cache.insertGRDGResource( request, &lease, &handle ) == SIR_MESH_OK );
			CHECK( lease.bActive );
			request.packedBVHChunk = { collision.nChunkId };
			sirMeshGRDGSourceLease_t invalidLease = {};
			sirMeshHandle_t invalidHandle = {};
			CHECK( cache.insertGRDGResource( request, &invalidLease, &invalidHandle ) == SIR_MESH_INVALID_ARGUMENT );
		}
		const uint32_t nBeforeReaders = counters.nCalls;
		sirMeshGeometryRead_t readers[4] = {};
		for ( uint32_t i = 0; i < 4; ++i ) {
			CHECK( cache.acquire( handle, &readers[i] ) == SIR_MESH_OK );
			CHECK( readers[i].pPositionsXYZ == ( const float * )( pCanonicalCollision + 16 ) );
			CHECK( readers[i].pStaticBVH == pCanonicalBVH + SIR_MESH_PACK_BVH_HEADER_BYTES );
			CHECK( readers[i].pPositionsXYZ == readers[0].pPositionsXYZ );
			CHECK( readers[i].pSurfaceMaterials == readers[0].pSurfaceMaterials );
			CHECK( readers[i].pBVHTriangleOrder == readers[0].pBVHTriangleOrder );
			const sirCommonGeometryRay_t query = { { 0.2f, 0.2f, 5.0f }, { 0, 0, -1 }, 10.0f, true };
			sirCommonGeometryHit_t hit = {};
			CHECK( sirCommonGeometryTraceRay( readers[i], query, &hit ) == SIR_COMMON_GEOMETRY_HIT );
			CHECK( hit.nTriangle == 0 && hit.nSurfaceMaterialId == 17 && hit.fDistance == 5.0f );
		}
		CHECK( counters.nCalls == nBeforeReaders && graph.getStats().m_nPayloadBytes == nPayloadBytes );
		CHECK( graph.getWriter()->retire( resourceHandle ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( graph.getWriter()->collectRetired( 8 ) == 0 );
		CHECK( cache.retire( handle ) == SIR_MESH_OK );
		CHECK( cache.releaseGRDGResource( &lease ) == SIR_MESH_BUSY );
		for ( uint32_t i = 0; i < 4; ++i )
			CHECK( cache.release( handle ) == SIR_MESH_OK );
		CHECK( lease.bActive );
		CHECK( cache.releaseGRDGResource( &lease ) == SIR_MESH_OK );
		CHECK( !lease.bActive && lease.eLastRelease == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( graph.getWriter()->collectRetired( 8 ) == 1 );
		CHECK( graph.getStats().m_nPayloadBytes == 0 );
		CHECK( cache.shutdown() == SIR_MESH_OK );
	}
	CHECK( counters.nLiveBytes == 0 );
	return true;
}

int main( void )
{
	if ( !testCookOnceBorrowFourTimes() )
		return 1;
	puts( "One cooked static BVH, source mesh and material backing shared by four GRDG readers" );
	return 0;
}
