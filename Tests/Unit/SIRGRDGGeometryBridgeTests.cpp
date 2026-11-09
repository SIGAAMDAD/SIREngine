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
	SIRResourceStagingHandle staging[2] = {};
	SIRResourceMutableBytes bytes = {};
	CHECK( rWriter.allocateStaging( sizeof( positions ), 16, 1, &staging[0], &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( bytes.m_pData, positions, sizeof( positions ) );
	CHECK( rWriter.allocateStaging( sizeof( indices ), 16, 1, &staging[1], &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( bytes.m_pData, indices, sizeof( indices ) );
	const SIRResourceChunkPublication chunks[2] = {
	    { { 1 }, { ( uint64_t )100 + nPattern, 0xdeadbeef, 12, 0 }, staging[0], false },
	    { { 2 }, { 400, 0xfacefeed, 12, 0 }, staging[1], false },
	};
	const SIRResourcePublication publication = { { 900, 19, 0 }, SIR_RESOURCE_KIND_GEOMETRY, 1, 12, chunks, 2, nullptr, 0 };
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
		config.m_nMaxStaging = 4;
		config.m_nMaxProviderPins = 8;
		config.m_nMaxCommittedBytes = 1024 * 1024;
		SIRResourceGraph graph( config );
		CHECK( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		CHECK( pWriter );
		alignas( 64 ) uint8_t cacheStorage[4096] = {};
		SIRMeshCache cache;
		CHECK( cache.initialize( cacheStorage, sizeof( cacheStorage ), 3 ) == SIR_MESH_OK );
		const sirGRDGMeshChunkSelection_t chunks = { { 1 }, { 2 }, 0, 7 };
		sirGRDGMeshPin_t firstPin = {}, duplicatePin = {}, secondPin = {};
		const SIRResourceHandle firstSource = publish( *pWriter, 3 );
		sirMeshHandle_t firstMesh = {}, duplicateMesh = {}, secondMesh = {};
		sirMeshGeometryRead_t readers[4] = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView source;
			SIRResourceConstBytes expectedPositions = {}, expectedIndices = {};
			CHECK( graph.resolve( scope, firstSource, &source ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( source.getChunk( { 1 }, &expectedPositions ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( source.getChunk( { 2 }, &expectedIndices ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( SIRGRDGGeometryBridge::attach( graph, cache, scope, source, chunks, &firstPin, &firstMesh ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( firstPin.bActive );
			CHECK( SIRGRDGGeometryBridge::attach( graph, cache, scope, source, chunks, &duplicatePin, &duplicateMesh ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( !duplicatePin.bActive && duplicateMesh.nSlot == firstMesh.nSlot && duplicateMesh.nOwnerId == firstMesh.nOwnerId );
			CHECK( SIRGRDGGeometryBridge::attach( graph, cache, scope, source, { { 1 }, { 99 }, 0, 7 }, &duplicatePin, &duplicateMesh ) ==
			       SIR_RESOURCE_RESULT_MISSING_CHUNK );
			const uint32_t beforeReaders = allocator.nAllocations;
			// Renderer, physics, audio-ray and navigation borrow the same native
			// CPU pointer; no separate vertex/index or BVH-owner arrays arise.
			for ( uint32_t i = 0; i < 4; ++i ) {
				CHECK( cache.acquire( firstMesh, &readers[i] ) == SIR_MESH_OK );
				CHECK( ( const void * )readers[i].pPositionsXYZ == expectedPositions.m_pData );
				CHECK( ( const void * )readers[i].pTriangleIndices == expectedIndices.m_pData );
				CHECK( readers[i].key.nAssetId == 900 && readers[i].key.nRevision == 1 );
			}
			CHECK( allocator.nAllocations == beforeReaders && cache.getBorrowCount() == 4 && cache.getResidentCount() == 1 );
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
