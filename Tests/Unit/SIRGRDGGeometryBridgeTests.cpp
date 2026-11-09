/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Mesh/SIRGRDGGeometryBridge.h>
#include <Mesh/SIRCommonGeometryQueries.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <malloc.h>
#endif

#define CHECK( expr )                                                                                                                      \
	do {                                                                                                                                   \
		if ( !( expr ) ) {                                                                                                                 \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr );                                                                   \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

struct sirSharedGeometryTestAllocator_t
{
	uint32_t nLive = 0;
	uint32_t nAllocations = 0;
};

static void *allocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	sirSharedGeometryTestAllocator_t *pState = ( sirSharedGeometryTestAllocator_t * )pContext;
	void *pMemory = nullptr;
#ifdef _WIN32
	pMemory = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	if ( posix_memalign( &pMemory, nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pMemory ) {
		++pState->nLive;
		++pState->nAllocations;
	}
	return pMemory;
}

static void deallocate( void *pContext, void *pData, uint64_t, uint32_t, uint32_t ) noexcept
{
	--( ( sirSharedGeometryTestAllocator_t * )pContext )->nLive;
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static SIRResourceHandle publish( SIRResourceGraphWriter& rWriter, uint8_t nPattern ) noexcept
{
	const float positions[] = { ( float )nPattern, 0, 0, 1, 0, 0, 0, 1, 0 };
	const uint32_t indices[] = { 0, 1, 2 };
	const sirCommonBVHNode_t bvh[] = { { { 0.0f, -0.1f, -0.1f }, { ( float )nPattern + 1.0f, 1.0f, 0.1f }, 0, 0, 0, 1 } };
	const uint32_t surfaceMaterials[] = { 77u, 88u };
	const float lodErrors[] = { 0.0f, ( float )nPattern };
	SIRResourceStagingHandle staging[5] = {};
	SIRResourceMutableBytes bytes = {};
	CHECK( rWriter.allocateStaging( sizeof( positions ), 16, 1, &staging[0], &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( bytes.m_pData, positions, sizeof( positions ) );
	CHECK( rWriter.allocateStaging( sizeof( indices ), 16, 1, &staging[1], &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( bytes.m_pData, indices, sizeof( indices ) );
	CHECK( rWriter.allocateStaging( sizeof( bvh ), 16, 1, &staging[2], &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( bytes.m_pData, bvh, sizeof( bvh ) );
	CHECK( rWriter.allocateStaging( sizeof( surfaceMaterials ), 16, 1, &staging[3], &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( bytes.m_pData, surfaceMaterials, sizeof( surfaceMaterials ) );
	CHECK( rWriter.allocateStaging( sizeof( lodErrors ), 16, 1, &staging[4], &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( bytes.m_pData, lodErrors, sizeof( lodErrors ) );
	const SIRResourceChunkPublication chunks[5] = {
	    { { 1 }, { ( uint64_t )100 + nPattern, 0xdeadbeef, 12, 0 }, staging[0], false },
	    { { 2 }, { 400, 0xfacefeed, 12, 0 }, staging[1], false },
	    { { 3 }, { ( uint64_t )1000 + nPattern, 0xbabeba11, 12, 0 }, staging[2], false },
	    { { 4 }, { 1100, 0xfacebabe, 12, 0 }, staging[3], false },
	    { { 5 }, { ( uint64_t )1200 + nPattern, 0xc001cafe, 12, 0 }, staging[4], false },
	};
	const SIRResourcePublication publication = { { 900, 19, 0 }, SIR_RESOURCE_KIND_GEOMETRY, 1, 12, chunks, 5, nullptr, 0 };
	SIRResourceHandle handle = {};
	CHECK( rWriter.publish( publication, &handle ) == SIR_RESOURCE_RESULT_SUCCESS );
	return handle;
}

int main( void )
{
	sirSharedGeometryTestAllocator_t allocator = {};
	{
		SIRResourceGraphConfig config = {};
		config.m_Allocator = { &allocator, allocate, deallocate };
		config.m_nMetadataDomain = 1;
		config.m_nMaxResources = 4;
		config.m_nMaxStaging = 8;
		config.m_nMaxProviderPins = 8;
		config.m_nMaxCommittedBytes = 1024 * 1024;
		SIRResourceGraph graph( config );
		CHECK( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		CHECK( pWriter );
		alignas( 64 ) uint8_t cacheStorage[4096] = {};
		SIRMeshCache cache;
		CHECK( cache.initialize( cacheStorage, sizeof( cacheStorage ), 3 ) == SIR_MESH_OK );
		const sirGRDGMeshChunkSelection_t chunks = { { 1 }, { 2 }, 0, 7, { 3 }, { 4 }, { 5 } };
		sirGRDGMeshPin_t firstPin = {}, duplicatePin = {}, secondPin = {};
		const SIRResourceHandle firstSource = publish( *pWriter, 3 );
		sirMeshHandle_t firstMesh = {}, duplicateMesh = {}, secondMesh = {};
		sirMeshGeometryRead_t readers[4] = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView source;
			SIRResourceConstBytes expectedPositions = {}, expectedIndices = {}, expectedBVH = {}, expectedMaterials = {}, expectedLod = {};
			CHECK( graph.resolve( scope, firstSource, &source ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( source.getChunk( { 1 }, &expectedPositions ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( source.getChunk( { 2 }, &expectedIndices ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( source.getChunk( { 3 }, &expectedBVH ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( source.getChunk( { 4 }, &expectedMaterials ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( source.getChunk( { 5 }, &expectedLod ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( SIRGRDGGeometryBridge::attach( graph, cache, scope, source, chunks, &firstPin, &firstMesh ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( firstPin.bActive );
			CHECK( SIRGRDGGeometryBridge::attach( graph, cache, scope, source, chunks, &duplicatePin, &duplicateMesh ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( !duplicatePin.bActive && duplicateMesh.nSlot == firstMesh.nSlot && duplicateMesh.nOwnerId == firstMesh.nOwnerId );
			CHECK( SIRGRDGGeometryBridge::attach( graph, cache, scope, source, { { 1 }, { 99 }, 0, 7 }, &duplicatePin, &duplicateMesh ) ==
			       SIR_RESOURCE_RESULT_MISSING_CHUNK );
			sirGRDGMeshChunkSelection_t malformed = chunks;
			malformed.staticBVHChunk = { 4 };
			CHECK( SIRGRDGGeometryBridge::attach( graph, cache, scope, source, malformed, &duplicatePin, &duplicateMesh ) ==
			       SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			malformed = chunks;
			malformed.staticBVHChunk = { 99 };
			CHECK( SIRGRDGGeometryBridge::attach( graph, cache, scope, source, malformed, &duplicatePin, &duplicateMesh ) ==
			       SIR_RESOURCE_RESULT_MISSING_CHUNK );
			const uint32_t beforeReaders = allocator.nAllocations;
			// Renderer, physics, audio-ray and navigation borrow the same native
			// CPU pointer; no separate vertex/index or BVH-owner arrays arise.
			for ( uint32_t i = 0; i < 4; ++i ) {
				CHECK( cache.acquire( firstMesh, &readers[i] ) == SIR_MESH_OK );
				CHECK( ( const void * )readers[i].pPositionsXYZ == expectedPositions.m_pData );
				CHECK( ( const void * )readers[i].pTriangleIndices == expectedIndices.m_pData );
				CHECK( readers[i].pStaticBVH == expectedBVH.m_pData && readers[i].nStaticBVHBytes == expectedBVH.m_nSize );
				CHECK( readers[i].pSurfaceMaterials == expectedMaterials.m_pData &&
				       readers[i].nSurfaceMaterialBytes == expectedMaterials.m_nSize );
				CHECK( readers[i].pLodHierarchy == expectedLod.m_pData && readers[i].nLodHierarchyBytes == expectedLod.m_nSize );
				CHECK( readers[i].key.nAssetId == 900 && readers[i].key.nRevision == 1 && readers[i].key.nResourceTypeId == 19 &&
				       readers[i].key.nCookedSchemaVersion == 1 );
			}
			CHECK( allocator.nAllocations == beforeReaders && cache.getBorrowCount() == 4 && cache.getResidentCount() == 1 );
			// Four independent subsystem callers execute the exact same cooked BVH,
			// triangle and SurfaceMaterialID bytes without allocating a second copy.
			const sirCommonGeometryRay_t query = { { 1.0f, 0.25f, 2.0f }, { 0.0f, 0.0f, -1.0f }, 5.0f, true };
			for ( uint32_t i = 0; i < 4; ++i ) {
				sirCommonGeometryHit_t hit = {};
				CHECK( sirCommonGeometryTraceRay( readers[i], query, &hit ) == SIR_COMMON_GEOMETRY_HIT );
				CHECK( hit.nTriangle == 0 && hit.nSurfaceMaterialId == 77 && hit.sourceKey.nRevision == 1 );
				CHECK( hit.fDistance == 2.0f && hit.szPosition[2] == 0.0f );
			}
			CHECK( allocator.nAllocations == beforeReaders );
			const sirCommonGeometryRay_t miss = { { 50.0f, 50.0f, 2.0f }, { 0.0f, 0.0f, -1.0f }, 5.0f, true };
			sirCommonGeometryHit_t notTouched = {};
			notTouched.nTriangle = 1234;
			CHECK( sirCommonGeometryTraceRay( readers[0], miss, &notTouched ) == SIR_COMMON_GEOMETRY_MISS );
			CHECK( notTouched.nTriangle == 1234 );
			sirMeshGeometryRead_t malformedRead = readers[0];
			malformedRead.nStaticBVHBytes--;
			CHECK( sirCommonGeometryTraceRay( malformedRead, query, &notTouched ) == SIR_COMMON_GEOMETRY_INVALID_BVH );
			malformedRead = readers[0];
			sirCommonBVHNode_t malformedNode = {};
			malformedNode.nTriangleCount = 2;
			malformedRead.pStaticBVH = &malformedNode;
			CHECK( sirCommonGeometryTraceRay( malformedRead, query, &notTouched ) == SIR_COMMON_GEOMETRY_INVALID_BVH );
			const sirCommonGeometryRay_t zeroDirection = { { 1, 0, 1 }, { 0, 0, 0 }, 5, true };
			CHECK( sirCommonGeometryTraceRay( readers[0], zeroDirection, &notTouched ) == SIR_COMMON_GEOMETRY_INVALID_ARGUMENT );
		}
		const SIRResourceHandle updatedSource = publish( *pWriter, 4 );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView updated;
			CHECK( graph.resolve( scope, updatedSource, &updated ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( SIRGRDGGeometryBridge::attach( graph, cache, scope, updated, chunks, &secondPin, &secondMesh ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( firstMesh.nSlot != secondMesh.nSlot && cache.getResidentCount() == 2 );
			sirMeshGeometryRead_t recent = {};
			CHECK( cache.acquire( secondMesh, &recent ) == SIR_MESH_OK );
			CHECK( recent.key.nRevision == 2 && recent.pPositionsXYZ != readers[0].pPositionsXYZ );
			CHECK( recent.pStaticBVH != readers[0].pStaticBVH && recent.pLodHierarchy != readers[0].pLodHierarchy );
			CHECK( recent.pSurfaceMaterials == readers[0].pSurfaceMaterials );
			CHECK( cache.release( secondMesh ) == SIR_MESH_OK );
		}
		CHECK( cache.retire( firstMesh ) == SIR_MESH_OK );
		CHECK( firstPin.bActive );
		for ( uint32_t i = 0; i < 4; ++i ) {
			CHECK( cache.release( firstMesh ) == SIR_MESH_OK );
		}
		CHECK( !firstPin.bActive && secondPin.bActive );
		CHECK( cache.retire( secondMesh ) == SIR_MESH_OK );
		CHECK( !secondPin.bActive && cache.getResidentCount() == 0 );
		CHECK( cache.shutdown() == SIR_MESH_OK );
		CHECK( pWriter->retire( updatedSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( pWriter->collectRetired( 4 ) >= 1 );
	}
	CHECK( allocator.nLive == 0 );
	return 0;
}
