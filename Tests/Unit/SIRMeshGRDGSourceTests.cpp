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
#include <Mesh/SIRMeshGRDGIntegration.h>
#include <Mesh/SIRMeshCollisionSource.h>
#include <Mesh/SIRCommonGeometryQueries.h>

#include <Resources/SIRResourceGraph.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Mesh GRDG source failed at %s:%u: %s\n", __FILE__, __LINE__, #expression );                                  \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

struct sirMeshTestAllocation_t
{
	uint64_t nLiveBytes;
	uint32_t nAllocations;
};

static void *sirMeshTestAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
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
		sirMeshTestAllocation_t *stats = ( sirMeshTestAllocation_t * )pContext;
		stats->nLiveBytes += nBytes;
		++stats->nAllocations;
	}
	return p;
}

static void sirMeshTestFree( void *pContext, void *pData, uint64_t nBytes, uint32_t, uint32_t ) noexcept
{
	( ( sirMeshTestAllocation_t * )pContext )->nLiveBytes -= nBytes;
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static bool publishMesh( SIRResourceGraphWriter *pWriter, uint32_t nSalt, SIRResourceHandle *pHandle )
{
	const float positions[9] = { 0, 0, ( float )nSalt, 1, 0, ( float )nSalt, 0, 1, ( float )nSalt };
	const uint32_t indices[3] = { 0, 1, 2 };
	const sirMeshBVHNode_t node = { { 0, 0, ( float )nSalt }, { 1, 1, ( float )nSalt }, UINT32_MAX, UINT32_MAX, 0, 1 };
	const uint32_t triangleOrder = 0;
	const uint32_t materialId = 7;
	SIRResourceStagingHandle staging[5] = {};
	SIRResourceMutableBytes bytes[5] = {};
	const void *const data[5] = { positions, indices, &node, &triangleOrder, &materialId };
	const uint64_t size[5] = { sizeof( positions ), sizeof( indices ), sizeof( node ), sizeof( triangleOrder ), sizeof( materialId ) };
	for ( uint32_t i = 0; i < 5; ++i ) {
		if ( pWriter->allocateStaging( size[i], 16, 7, &staging[i], &bytes[i] ) != SIR_RESOURCE_RESULT_SUCCESS )
			return false;
		memcpy( bytes[i].m_pData, data[i], size[i] );
	}
	const SIRResourceChunkPublication chunks[5] = {
	    { { 10 }, { ( uint64_t )100 + nSalt, ( uint64_t )200 + nSalt, 4, 0 }, staging[0], false },
	    { { 20 }, { ( uint64_t )300 + nSalt, ( uint64_t )400 + nSalt, 4, 0 }, staging[1], false },
	    { { 30 }, { ( uint64_t )500 + nSalt, ( uint64_t )600 + nSalt, 4, 0 }, staging[2], false },
	    { { 40 }, { ( uint64_t )700 + nSalt, ( uint64_t )800 + nSalt, 4, 0 }, staging[3], false },
	    { { 50 }, { ( uint64_t )900 + nSalt, ( uint64_t )1000 + nSalt, 4, 0 }, staging[4], false } };
	const SIRResourcePublication publication = { { 500, 77, 2 }, SIR_RESOURCE_KIND_GEOMETRY, 1, 4, chunks, 5, nullptr, 0 };
	return pWriter->publish( publication, pHandle ) == SIR_RESOURCE_RESULT_SUCCESS;
}

int main( void )
{
	sirMeshTestAllocation_t allocations = {};
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &allocations, sirMeshTestAllocate, sirMeshTestFree };
	config.m_nMetadataDomain = 7;
	config.m_nMaxResources = 4;
	config.m_nMaxStaging = 16;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 1024 * 1024;
	{
		SIRResourceGraph graph( config );
		CHECK( graph.isReady() );
		SIRResourceGraphWriter *writer = graph.getWriter();
		CHECK( writer != nullptr );
		SIRResourceHandle hResource = {};
		CHECK( publishMesh( writer, 1, &hResource ) );
		CHECK( graph.getStats().m_nUniqueChunks == 5 );
		const uint64_t sourceBytes = graph.getStats().m_nPayloadBytes;
		CHECK( sourceBytes == 9 * sizeof( float ) + 3 * sizeof( uint32_t ) + sizeof( sirMeshBVHNode_t ) + 2 * sizeof( uint32_t ) );
		std::vector<uint8_t> storage( SIRMeshCache::requiredBytes( 2 ) + SIRMeshCache::requiredAlignment() );
		const uintptr_t address = ( ( uintptr_t )storage.data() + SIRMeshCache::requiredAlignment() - 1 ) &
		                          ~( ( uintptr_t )SIRMeshCache::requiredAlignment() - 1 );
		SIRMeshCache cache;
		CHECK( cache.initialize( ( void * )address, storage.size() - ( address - ( uintptr_t )storage.data() ), 2 ) == SIR_MESH_OK );
		sirMeshGRDGSourceLease_t lease = {};
		sirMeshHandle_t handle = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			CHECK( graph.resolve( scope, hResource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirMeshGRDGSourceRequest_t request = { &graph, &scope, &view, { 500, 1, 2, 0 }, 77,     99,    { 10 },
			                                       { 20 }, 3,      3,     { 30 },           { 40 }, { 50 } };
			CHECK( cache.insertGRDGResource( request, &lease, &handle ) == SIR_MESH_OK );
			CHECK( lease.bActive );
			const uint32_t afterInsert = allocations.nAllocations;
			request.key.nRevision = 2;
			sirMeshGRDGSourceLease_t badLease = {};
			sirMeshHandle_t unused = {};
			CHECK( cache.insertGRDGResource( request, &badLease, &unused ) == SIR_MESH_STALE_HANDLE );
			request.key.nRevision = 1;
			request.indicesChunk = { 999 };
			CHECK( cache.insertGRDGResource( request, &badLease, &unused ) == SIR_MESH_INVALID_ARGUMENT );
			request.indicesChunk = { 20 };
			CHECK( cache.insertGRDGResource( request, &badLease, &unused ) == SIR_MESH_DUPLICATE );
			CHECK( !badLease.bActive && badLease.eLastRelease == SIR_RESOURCE_RESULT_SUCCESS );
			request.bvhOrderChunk = {};
			CHECK( cache.insertGRDGResource( request, &badLease, &unused ) == SIR_MESH_INVALID_ARGUMENT );
			request.bvhOrderChunk = { 40 };
			// A view from a different read-scope lifetime cannot mint a pin.
			SIRResourceReadScope otherScope( graph );
			request.pScope = &otherScope;
			CHECK( cache.insertGRDGResource( request, &badLease, &unused ) == SIR_MESH_INVALID_ARGUMENT );
			CHECK( !badLease.bActive );
			CHECK( allocations.nAllocations == afterInsert );
		}
		// Hot reload replaces the GRDG publication without replacing the old
		// CPU source backing under a live MeshCache lease.
		SIRResourceHandle hLatest = {};
		CHECK( publishMesh( writer, 2, &hLatest ) );
		CHECK( graph.getStats().m_nUniqueChunks == 10 );
		sirMeshGRDGSourceLease_t currentLease = {};
		sirMeshHandle_t currentHandle = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			CHECK( graph.resolve( scope, hLatest, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			const sirMeshGRDGSourceRequest_t request = { &graph, &scope, &view, { 500, 2, 2, 0 }, 77,     99,    { 10 },
			                                             { 20 }, 3,      3,     { 30 },           { 40 }, { 50 } };
			CHECK( cache.insertGRDGResource( request, &currentLease, &currentHandle ) == SIR_MESH_OK );
		}
		CHECK( writer->collectRetired( 8 ) == 0 );
		CHECK( writer->retire( hLatest ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( writer->collectRetired( 8 ) == 0 );
		sirMeshGeometryRead_t read = {};
		const uint64_t frozenBytes = graph.getStats().m_nPayloadBytes;
		const uint32_t allocationsBeforeReaders = allocations.nAllocations;
		for ( uint32_t i = 0; i < 1000; ++i ) {
			CHECK( cache.acquire( handle, &read ) == SIR_MESH_OK );
			CHECK( read.pPositionsXYZ[2] == 1 && read.pTriangleIndices[2] == 2 );
			CHECK( read.pStaticBVH != nullptr && read.nStaticBVHBytes == sizeof( sirMeshBVHNode_t ) );
			CHECK( read.nBVHTriangleOrderCount == 1 && read.pBVHTriangleOrder[0] == 0 && read.pSurfaceMaterials[0] == 7 );
			CHECK( graph.getStats().m_nPayloadBytes == frozenBytes );
		}
		CHECK( allocations.nAllocations == allocationsBeforeReaders );
		CHECK( cache.getBorrowCount() == 1000 );
		CHECK( cache.retire( handle ) == SIR_MESH_OK );
		CHECK( cache.releaseGRDGResource( &lease ) == SIR_MESH_BUSY );
		CHECK( writer->collectRetired( 8 ) == 0 );
		for ( uint32_t i = 0; i < 1000; ++i )
			CHECK( cache.release( handle ) == SIR_MESH_OK );
		CHECK( lease.bActive );
		CHECK( cache.releaseGRDGResource( &lease ) == SIR_MESH_OK );
		CHECK( !lease.bActive && lease.eLastRelease == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( writer->collectRetired( 8 ) == 1 );
		CHECK( graph.getStats().m_nUniqueChunks == 5 );
		SIRMeshCollisionSource collision;
		const sirMeshCollisionProfile_t profile = { 1, 7, 0, 0 };
		CHECK( collision.initialize( cache, currentHandle, profile ) == SIR_MESH_OK );
		const uint32_t beforeShapeInstances = allocations.nAllocations;
		for ( uint32_t i = 0; i < 1000; ++i )
			CHECK( collision.acquireInstance() == SIR_MESH_OK );
		CHECK( allocations.nAllocations == beforeShapeInstances );
		CHECK( collision.getInstanceCount() == 1000 );
		const sirMeshCollisionRay_t ray = { { 0.2f, 0.2f, 9.0f }, { 0.0f, 0.0f, -1.0f }, 20.0f };
		sirMeshCollisionHit_t hit = {};
		CHECK( collision.raycast( ray, &hit ) == SIR_MESH_OK );
		CHECK( hit.nTriangle == 0 && hit.fDistance == 7.0f && hit.nMaterialId == 7 );
		CHECK( cache.acquire( currentHandle, &read ) == SIR_MESH_OK );
		CHECK( read.key.nRevision == 2 && read.pPositionsXYZ[2] == 2 && read.pTriangleIndices[2] == 2 );
		CHECK( read.pBVHTriangleOrder != nullptr && read.pSurfaceMaterials[0] == 7 );
		const sirCommonGeometryRay_t query = { { 0.2f, 0.2f, 9.0f }, { 0, 0, -1 }, 20, true };
		sirCommonGeometryHit_t commonHit = {};
		CHECK( sirCommonGeometryTraceRay( read, query, &commonHit ) == SIR_COMMON_GEOMETRY_HIT );
		CHECK( commonHit.nSurfaceMaterialId == 7 && commonHit.nTriangle == 0 && commonHit.fDistance == 7.0f );
		CHECK( cache.release( currentHandle ) == SIR_MESH_OK );
		CHECK( cache.retire( currentHandle ) == SIR_MESH_OK );
		CHECK( currentLease.bActive );
		CHECK( writer->collectRetired( 8 ) == 0 );
		CHECK( collision.shutdown() == SIR_MESH_BUSY );
		for ( uint32_t i = 0; i < 1000; ++i )
			CHECK( collision.releaseInstance() == SIR_MESH_OK );
		CHECK( collision.shutdown() == SIR_MESH_OK );
		CHECK( currentLease.bActive );
		CHECK( cache.releaseGRDGResource( &currentLease ) == SIR_MESH_OK );
		CHECK( !currentLease.bActive && currentLease.eLastRelease == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( writer->collectRetired( 8 ) == 1 );
		CHECK( graph.getStats().m_nUniqueChunks == 0 );
		CHECK( cache.shutdown() == SIR_MESH_OK );
	}
	CHECK( allocations.nLiveBytes == 0 );
	puts( "MeshCache GRDG source-pin and 1000-reader lifetime checks passed" );
	return 0;
}
