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
#include <Mesh/SIRMeshCollisionBVH.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Mesh collision BVH failed %s:%u: %s\n", __FILE__, __LINE__, #expression );                                   \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static void releaseSource( void *pValue ) noexcept
{
	++*( uint32_t * )pValue;
}

int main( void )
{
	const uint32_t nGrid = 18;
	std::vector<float> positions;
	std::vector<uint32_t> triangles;
	for ( uint32_t y = 0; y <= nGrid; ++y ) {
		for ( uint32_t x = 0; x <= nGrid; ++x ) {
			positions.push_back( ( float )x );
			positions.push_back( ( float )y );
			positions.push_back( ( float )( ( x + y ) % 3 ) * 0.25f );
		}
	}
	for ( uint32_t y = 0; y < nGrid; ++y ) {
		for ( uint32_t x = 0; x < nGrid; ++x ) {
			const uint32_t a = y * ( nGrid + 1 ) + x;
			triangles.insert( triangles.end(), { a, a + 1, a + nGrid + 1, a + 1, a + nGrid + 2, a + nGrid + 1 } );
		}
	}
	uint32_t releases = 0;
	std::vector<uint8_t> sourceStorage( SIRMeshCache::requiredBytes( 1 ) + SIRMeshCache::requiredAlignment() );
	const uintptr_t aligned = ( ( uintptr_t )sourceStorage.data() + SIRMeshCache::requiredAlignment() - 1 ) &
	                          ~( ( uintptr_t )SIRMeshCache::requiredAlignment() - 1 );
	SIRMeshCache cache;
	CHECK( cache.initialize( ( void * )aligned, sourceStorage.size() - ( aligned - ( uintptr_t )sourceStorage.data() ), 1 ) ==
	       SIR_MESH_OK );
	const sirMeshGeometrySource_t source = {
	    { 77, 22, 0, 0 }, positions.data(), triangles.data(), ( uint32_t )( positions.size() / 3 ), ( uint32_t )triangles.size(),
	    &releases,        releaseSource };
	sirMeshHandle_t handle = {};
	CHECK( cache.insert( &source, &handle ) == SIR_MESH_OK );
	SIRMeshCollisionSource collision;
	const sirMeshCollisionProfile_t profile = { 4, 13, 0, 0 };
	CHECK( collision.initialize( cache, handle, profile ) == SIR_MESH_OK );
	const uint32_t nTriangleCount = ( uint32_t )triangles.size() / 3;
	CHECK( collision.getTriangleCount() == nTriangleCount );
	CHECK( SIRMeshCollisionBVH::requiredBytes( 0 ) == 0 );
	CHECK( SIRMeshCollisionBVH::requiredBytes( UINT32_MAX ) == 0 );
	const uint64_t bytes = SIRMeshCollisionBVH::requiredBytes( nTriangleCount );
	std::vector<uint8_t> storage( bytes + SIRMeshCollisionBVH::requiredAlignment() );
	const uintptr_t address = ( ( uintptr_t )storage.data() + SIRMeshCollisionBVH::requiredAlignment() - 1 ) &
	                          ~( ( uintptr_t )SIRMeshCollisionBVH::requiredAlignment() - 1 );
	SIRMeshCollisionBVH bvh;
	CHECK( bvh.initialize( collision, ( void * )address, bytes - 1 ) == SIR_MESH_INVALID_ARGUMENT );
	CHECK( bvh.initialize( collision, ( void * )( address + 1 ), bytes ) == SIR_MESH_INVALID_ARGUMENT );
	CHECK( bvh.initialize( collision, ( void * )address, bytes ) == SIR_MESH_OK );
	CHECK( bvh.initialize( collision, ( void * )address, bytes ) == SIR_MESH_BUSY );
	CHECK( bvh.getTriangleCount() == nTriangleCount );
	CHECK( bvh.getNodeCount() > 1 && bvh.getNodeCount() < 2 * nTriangleCount );
	CHECK( collision.getInstanceCount() == 1 );
	CHECK( cache.getBorrowCount() == 1 );
	CHECK( cache.retire( handle ) == SIR_MESH_OK );
	CHECK( collision.shutdown() == SIR_MESH_BUSY );
	CHECK( releases == 0 );
	for ( uint32_t i = 0; i < 1000; ++i ) {
		const float x = ( float )( ( i * 37 ) % 180 ) * 0.1f + 0.03125f;
		const float y = ( float )( ( i * 71 ) % 180 ) * 0.1f + 0.0625f;
		const sirMeshCollisionRay_t ray = { { x, y, 12.0f }, { 0, 0, -( float )( i % 4 + 1 ) }, 40.0f };
		sirMeshCollisionHit_t brute = {}, fast = {};
		const sirMeshResult_t expected = collision.raycast( ray, &brute );
		CHECK( bvh.raycast( ray, &fast ) == expected );
		if ( expected == SIR_MESH_OK ) {
			CHECK( std::fabs( fast.fDistance - brute.fDistance ) < 0.0001f );
			CHECK( std::fabs( fast.position[2] - brute.position[2] ) < 0.0001f );
			CHECK( fast.nMaterialId == brute.nMaterialId );
			CHECK( fast.nTriangle == brute.nTriangle );
		}
	}
	const sirMeshCollisionRay_t miss = { { -50, -50, 12 }, { 0, 0, -1 }, 100 };
	sirMeshCollisionHit_t hit = {};
	CHECK( bvh.raycast( miss, &hit ) == SIR_MESH_NOT_FOUND );
	CHECK( bvh.raycast( miss, nullptr ) == SIR_MESH_INVALID_ARGUMENT );
	const sirMeshCollisionRay_t badDirection = { { 0, 0, 12 }, { 0, 0, 0 }, 100 };
	CHECK( bvh.raycast( badDirection, &hit ) == SIR_MESH_INVALID_ARGUMENT );
	CHECK( bvh.shutdown() == SIR_MESH_OK );
	CHECK( bvh.shutdown() == SIR_MESH_NOT_READY );
	CHECK( bvh.raycast( miss, &hit ) == SIR_MESH_NOT_READY );
	CHECK( collision.getInstanceCount() == 0 );
	CHECK( collision.shutdown() == SIR_MESH_OK );
	CHECK( releases == 1 );
	CHECK( cache.shutdown() == SIR_MESH_OK );
	puts( "BVH raycasts match brute-force queries on 1000 deterministic rays without copying mesh buffers" );
	return 0;
}
