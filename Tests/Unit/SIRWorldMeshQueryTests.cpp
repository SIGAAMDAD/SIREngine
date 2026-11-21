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
#include <World/Queries/SIRWorldMeshQuery.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "World mesh query test failed at %s:%u: %s\n", __FILE__, __LINE__, #expression );                             \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static void released( void *pPin ) noexcept
{
	++*( uint32_t * )pPin;
}

int main( void )
{
	const float positions[] = { 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0 };
	const uint32_t indices[] = { 0, 1, 2, 1, 3, 2 };
	uint32_t releases = 0;
	std::vector<uint8_t> cacheStorage( SIRMeshCache::requiredBytes( 1 ) + SIRMeshCache::requiredAlignment() );
	const uintptr_t cacheAddress = ( ( uintptr_t )cacheStorage.data() + SIRMeshCache::requiredAlignment() - 1 ) &
	                               ~( ( uintptr_t )SIRMeshCache::requiredAlignment() - 1 );
	SIRMeshCache cache;
	CHECK( cache.initialize( ( void * )cacheAddress, cacheStorage.size() - ( cacheAddress - ( uintptr_t )cacheStorage.data() ), 1 ) ==
	       SIR_MESH_OK );
	const sirMeshGeometrySource_t geometry = { { 21, 7, 0, 0 }, positions, indices, 4, 6, &releases, released };
	sirMeshHandle_t meshHandle = {};
	CHECK( cache.insert( &geometry, &meshHandle ) == SIR_MESH_OK );
	SIRMeshCollisionSource collision;
	const sirMeshCollisionProfile_t profile = { 1, 77, 0, 0 };
	CHECK( collision.initialize( cache, meshHandle, profile ) == SIR_MESH_OK );
	const uint64_t bvhBytes = SIRMeshCollisionBVH::requiredBytes( 2 );
	std::vector<uint8_t> bvhStorage( bvhBytes + SIRMeshCollisionBVH::requiredAlignment() );
	const uintptr_t bvhAddress = ( ( uintptr_t )bvhStorage.data() + SIRMeshCollisionBVH::requiredAlignment() - 1 ) &
	                             ~( ( uintptr_t )SIRMeshCollisionBVH::requiredAlignment() - 1 );
	SIRMeshCollisionBVH bvh;
	CHECK( bvh.initialize( collision, ( void * )bvhAddress, bvhBytes ) == SIR_MESH_OK );
	CHECK( bvh.matchesSource( collision ) );
	const uint32_t capacity = 1000;
	std::vector<uint8_t> worldStorage( SIRWorldMeshQuery::requiredBytes( capacity ) + SIRWorldMeshQuery::requiredAlignment() );
	const uintptr_t worldAddress = ( ( uintptr_t )worldStorage.data() + SIRWorldMeshQuery::requiredAlignment() - 1 ) &
	                               ~( ( uintptr_t )SIRWorldMeshQuery::requiredAlignment() - 1 );
	SIRWorldMeshQuery world;
	CHECK( world.initialize( ( void * )worldAddress, worldStorage.size() - ( worldAddress - ( uintptr_t )worldStorage.data() ), capacity,
	                         7 ) == SIR_WORLD_MESH_OK );
	CHECK( world.initialize( ( void * )worldAddress, worldStorage.size(), capacity, 7 ) == SIR_WORLD_MESH_BUSY );
	std::vector<sirWorldMeshHandle_t> handles( capacity );
	for ( uint32_t i = 0; i < capacity; ++i ) {
		sirWorldMeshInstance_t instance = {};
		instance.nInstanceId = i + 1;
		instance.nWorldGeneration = 7;
		instance.nLayerMask = ( i % 2 ) == 0 ? 1 : 2;
		instance.pSource = &collision;
		instance.pBVH = &bvh;
		instance.transform[0] = instance.transform[5] = instance.transform[10] = 1;
		instance.transform[3] = ( float )i * 3;
		instance.transform[11] = 2;
		CHECK( world.insert( instance, &handles[i] ) == SIR_WORLD_MESH_OK );
	}
	CHECK( world.getInstanceCount() == 1000 );
	CHECK( bvh.getReaderCount() == 1000 );
	CHECK( cache.getBorrowCount() == 1 && collision.getInstanceCount() == 1 );
	CHECK( cache.retire( meshHandle ) == SIR_MESH_OK );
	CHECK( releases == 0 );
	CHECK( world.shutdown() == SIR_WORLD_MESH_BUSY );
	CHECK( bvh.shutdown() == SIR_MESH_BUSY );
	CHECK( collision.shutdown() == SIR_MESH_BUSY );
	const sirMeshCollisionRay_t ray = { { 6.25f, 0.25f, 10 }, { 0, 0, -5 }, 20 };
	sirWorldMeshHit_t hit = {};
	CHECK( world.raycast( ray, 1, 7, &hit ) == SIR_WORLD_MESH_OK );
	CHECK( hit.nInstanceId == 3 && hit.keySource.nRevision == 7 );
	CHECK( hit.nMaterialId == 77 && std::fabs( hit.fDistance - 8.0f ) < 0.0001f );
	CHECK( hit.position[2] == 2.0f && hit.normal[2] == 1.0f );
	CHECK( world.raycast( ray, 2, 7, &hit ) == SIR_WORLD_MESH_NOT_FOUND );
	CHECK( world.raycast( ray, 1, 8, &hit ) == SIR_WORLD_MESH_STALE_HANDLE );
	CHECK( world.raycast( ray, 0, 7, &hit ) == SIR_WORLD_MESH_INVALID_ARGUMENT );
	CHECK( world.remove( handles[2] ) == SIR_WORLD_MESH_OK );
	CHECK( world.remove( handles[2] ) == SIR_WORLD_MESH_STALE_HANDLE );
	CHECK( world.raycast( ray, 1, 7, &hit ) == SIR_WORLD_MESH_NOT_FOUND );
	sirWorldMeshInstance_t reused = {};
	reused.nInstanceId = 999999;
	reused.nWorldGeneration = 7;
	reused.nLayerMask = 1;
	reused.pSource = &collision;
	reused.pBVH = &bvh;
	reused.transform[0] = reused.transform[5] = reused.transform[10] = 1;
	reused.transform[3] = 6;
	reused.transform[11] = 2;
	sirWorldMeshHandle_t newHandle = {};
	CHECK( world.insert( reused, &newHandle ) == SIR_WORLD_MESH_OK );
	CHECK( newHandle.nSlot == handles[2].nSlot && newHandle.nGeneration != handles[2].nGeneration );
	CHECK( world.raycast( ray, 1, 7, &hit ) == SIR_WORLD_MESH_OK && hit.nInstanceId == 999999 );
	// Non-uniform, rotated instance: local square maps to x[-2,0], y[0,3], z=4.
	const float rotated[12] = { 0, -2, 0, 0, 3, 0, 0, 0, 0, 0, 1.5f, 4 };
	CHECK( world.setTransform( handles[0], rotated ) == SIR_WORLD_MESH_OK );
	const sirMeshCollisionRay_t rotatedRay = { { -0.5f, 0.5f, 10 }, { 0, 0, -1 }, 20 };
	CHECK( world.raycast( rotatedRay, 1, 7, &hit ) == SIR_WORLD_MESH_OK );
	CHECK( hit.nInstanceId == 1 && std::fabs( hit.fDistance - 6 ) < 0.0001f );
	CHECK( hit.normal[2] == 1.0f );
	const float singular[12] = { 0 };
	CHECK( world.setTransform( handles[0], singular ) == SIR_WORLD_MESH_INVALID_ARGUMENT );
	CHECK( world.raycast( rotatedRay, 1, 7, &hit ) == SIR_WORLD_MESH_OK && hit.nInstanceId == 1 );
	const float mirrored[12] = { -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 4 };
	CHECK( world.setTransform( handles[0], mirrored ) == SIR_WORLD_MESH_OK );
	const sirMeshCollisionRay_t mirroredRay = { { -0.25f, 0.25f, 10 }, { 0, 0, -1 }, 20 };
	CHECK( world.raycast( mirroredRay, 1, 7, &hit ) == SIR_WORLD_MESH_OK );
	CHECK( hit.nInstanceId == 1 && std::fabs( hit.fDistance - 6 ) < 0.0001f );
	CHECK( hit.normal[2] == -1.0f );
	CHECK( world.remove( newHandle ) == SIR_WORLD_MESH_OK );
	for ( uint32_t i = 0; i < capacity; ++i ) {
		if ( i != 2 )
			CHECK( world.remove( handles[i] ) == SIR_WORLD_MESH_OK );
	}
	CHECK( bvh.getReaderCount() == 0 );
	CHECK( world.shutdown() == SIR_WORLD_MESH_OK );
	CHECK( world.shutdown() == SIR_WORLD_MESH_NOT_READY );
	CHECK( bvh.shutdown() == SIR_MESH_OK );
	CHECK( collision.shutdown() == SIR_MESH_OK );
	CHECK( releases == 1 );
	CHECK( cache.shutdown() == SIR_MESH_OK );
	puts( "World mesh queries: 1000 transformed instance leases, stale generations, layers, and GRDG-safe source retirement PASS" );
	return 0;
}
