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

#include <cstdio>
#include <cstring>
#include <vector>

#define CHECK( condition )                                                                                                                 \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Shared BVH query failed at %s:%d: %s\n", __FILE__, __LINE__, #condition );                                   \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static void releasePin( void *pPin ) noexcept
{
	++*( ( uint32_t * )pPin );
}

static bool testCanonicalSourceAndBvh( void ) noexcept
{
	const float positions[18] = { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 5, 1, 0, 5, 0, 1, 5 };
	const uint32_t indices[6] = { 0, 1, 2, 3, 4, 5 };
	const uint32_t materials[2] = { 101, 202 };
	const uint32_t order[2] = { 1, 0 };
	const sirMeshBVHNode_t nodes[3] = {
	    { { 0, 0, 0 }, { 1, 1, 5 }, 1, 2, 0, 0 },
	    { { 0, 0, 5 }, { 1, 1, 5 }, UINT32_MAX, UINT32_MAX, 0, 1 },
	    { { 0, 0, 0 }, { 1, 1, 0 }, UINT32_MAX, UINT32_MAX, 1, 1 },
	};
	uint32_t releases = 0;
	const sirMeshGeometrySource_t source = { { 321, 5, 1, 0 }, positions, indices, 6,         6, &releases, releasePin, nodes,
	                                         sizeof( nodes ),  order,     2,       materials, 2 };
	std::vector<uint8_t> storage( ( size_t )SIRMeshCache::requiredBytes( 1 ) + SIRMeshCache::requiredAlignment() );
	const uintptr_t alignment = SIRMeshCache::requiredAlignment();
	const uintptr_t aligned = ( ( uintptr_t )storage.data() + alignment - 1 ) & ~( alignment - 1 );
	SIRMeshCache cache;
	CHECK( cache.initialize( ( void * )aligned, storage.size() - ( aligned - ( uintptr_t )storage.data() ), 1 ) == SIR_MESH_OK );
	sirMeshGeometrySource_t bad = source;
	bad.nBVHTriangleOrderCount = 0;
	sirMeshHandle_t handle = {};
	CHECK( cache.insert( &bad, &handle ) == SIR_MESH_INVALID_ARGUMENT );
	bad = source;
	bad.nStaticBVHBytes--;
	CHECK( cache.insert( &bad, &handle ) == SIR_MESH_INVALID_ARGUMENT );
	CHECK( cache.insert( &source, &handle ) == SIR_MESH_OK );
	sirMeshGeometryRead_t reader = {};
	CHECK( cache.acquire( handle, &reader ) == SIR_MESH_OK );
	CHECK( reader.pStaticBVH == nodes && reader.pBVHTriangleOrder == order && reader.pSurfaceMaterials == materials );
	CHECK( reader.key.nRevision == 5 && reader.nSurfaceMaterialCount == 2 );
	const sirCommonGeometryRay_t ray = { { 0.25f, 0.25f, 10.0f }, { 0, 0, -1 }, 20.0f, true };
	sirCommonGeometryHit_t hit = {};
	CHECK( sirCommonGeometryTraceRay( reader, ray, &hit ) == SIR_COMMON_GEOMETRY_HIT );
	CHECK( hit.nTriangle == 1 && hit.nSurfaceMaterialId == 202 && hit.fDistance == 5.0f );
	CHECK( hit.sourceKey.nAssetId == source.key.nAssetId && hit.szPosition[2] == 5 );
	sirCommonGeometryRay_t miss = ray;
	miss.szOrigin[0] = 10.0f;
	const sirCommonGeometryHit_t previous = hit;
	CHECK( sirCommonGeometryTraceRay( reader, miss, &hit ) == SIR_COMMON_GEOMETRY_MISS );
	CHECK( memcmp( &hit, &previous, sizeof( hit ) ) == 0 );
	bad.nStaticBVHBytes = source.nStaticBVHBytes;
	sirMeshGeometryRead_t malformed = reader;
	malformed.nBVHTriangleOrderCount = 1;
	CHECK( sirCommonGeometryTraceRay( malformed, ray, &hit ) == SIR_COMMON_GEOMETRY_INVALID_BVH );
	const uint32_t invalidOrder[2] = { 200, 0 };
	malformed = reader;
	malformed.pBVHTriangleOrder = invalidOrder;
	CHECK( sirCommonGeometryTraceRay( malformed, ray, &hit ) == SIR_COMMON_GEOMETRY_INVALID_BVH );
	const sirMeshBVHNode_t cyclic[3] = { { { 0, 0, 0 }, { 1, 1, 5 }, 0, 2, 0, 0 }, nodes[1], nodes[2] };
	malformed = reader;
	malformed.pStaticBVH = cyclic;
	CHECK( sirCommonGeometryTraceRay( malformed, ray, &hit ) == SIR_COMMON_GEOMETRY_INVALID_BVH );
	CHECK( cache.retire( handle ) == SIR_MESH_OK );
	CHECK( releases == 0 );
	CHECK( cache.release( handle ) == SIR_MESH_OK );
	CHECK( releases == 1 );
	CHECK( cache.shutdown() == SIR_MESH_OK );
	return true;
}

int main( void )
{
	if ( !testCanonicalSourceAndBvh() )
		return 1;
	puts( "Shared canonical mesh/BVH/material borrowed views and ray parity passed" );
	return 0;
}
