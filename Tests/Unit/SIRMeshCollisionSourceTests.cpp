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
#include <Mesh/SIRMeshCollisionSource.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Collision source test failed at %s:%u: %s\n", __FILE__, __LINE__, #expression );                             \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static void released( void *pPin ) noexcept
{
	++*( uint32_t * )pPin;
}

int main( void )
{
	const float vertices[] = { 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 4, 2, 0, 4, 0, 2, 4 };
	const uint32_t indices[] = { 0, 1, 2, 3, 4, 5 };
	uint32_t releases = 0;
	std::vector<uint8_t> storage( SIRMeshCache::requiredBytes( 2 ) + SIRMeshCache::requiredAlignment() );
	const uintptr_t p =
	    ( ( uintptr_t )storage.data() + SIRMeshCache::requiredAlignment() - 1 ) & ~( ( uintptr_t )SIRMeshCache::requiredAlignment() - 1 );
	SIRMeshCache cache;
	CHECK( cache.initialize( ( void * )p, storage.size() - ( p - ( uintptr_t )storage.data() ), 2 ) == SIR_MESH_OK );
	const sirMeshGeometrySource_t mesh = { { 19, 7, 0, 0 }, vertices, indices, 6, 6, &releases, released };
	sirMeshHandle_t handle = {};
	CHECK( cache.insert( &mesh, &handle ) == SIR_MESH_OK );
	SIRMeshCollisionSource collision;
	const sirMeshCollisionProfile_t profile = { 4, 17, 0, 0 };
	CHECK( collision.initialize( cache, handle, profile ) == SIR_MESH_OK );
	CHECK( cache.getBorrowCount() == 1 );
	CHECK( collision.initialize( cache, handle, profile ) == SIR_MESH_BUSY );
	CHECK( collision.getSourceKey().nRevision == 7 );
	for ( uint32_t i = 0; i < 1000; ++i )
		CHECK( collision.acquireInstance() == SIR_MESH_OK );
	CHECK( collision.getInstanceCount() == 1000 );
	CHECK( cache.getBorrowCount() == 1 );
	CHECK( cache.retire( handle ) == SIR_MESH_OK );
	CHECK( releases == 0 );
	CHECK( collision.shutdown() == SIR_MESH_BUSY );
	sirMeshCollisionTriangle_t triangle = {};
	CHECK( collision.getTriangle( 1, &triangle ) == SIR_MESH_OK );
	CHECK( triangle.positions[2] == 4.0f && triangle.positions[3] == 2.0f && triangle.nMaterialId == 17 );
	CHECK( collision.getTriangle( 2, &triangle ) == SIR_MESH_INVALID_ARGUMENT );
	CHECK( collision.getTriangle( 0, nullptr ) == SIR_MESH_INVALID_ARGUMENT );
	const sirMeshCollisionRay_t ray = { { 0.5f, 0.5f, 9.0f }, { 0, 0, -1 }, 20.0f };
	sirMeshCollisionHit_t hit = {};
	CHECK( collision.raycast( ray, &hit ) == SIR_MESH_OK );
	CHECK( hit.nTriangle == 1 && hit.nMaterialId == 17 && std::fabs( hit.fDistance - 5.0f ) < 0.0001f );
	CHECK( std::fabs( hit.normal[2] - 1.0f ) < 0.0001f );
	CHECK( hit.position[2] == 4.0f );
	const sirMeshCollisionRay_t miss = { { 5, 5, 9 }, { 0, 0, -1 }, 20 };
	CHECK( collision.raycast( miss, &hit ) == SIR_MESH_NOT_FOUND );
	const sirMeshCollisionRay_t shortRay = { { 0.5f, 0.5f, 9 }, { 0, 0, -1 }, 4 };
	CHECK( collision.raycast( shortRay, &hit ) == SIR_MESH_NOT_FOUND );
	const sirMeshCollisionRay_t zeroRay = { { 0.5f, 0.5f, 9 }, { 0, 0, 0 }, 10 };
	CHECK( collision.raycast( zeroRay, &hit ) == SIR_MESH_INVALID_ARGUMENT );
	CHECK( collision.raycast( ray, nullptr ) == SIR_MESH_INVALID_ARGUMENT );
	for ( uint32_t i = 0; i < 1000; ++i )
		CHECK( collision.releaseInstance() == SIR_MESH_OK );
	CHECK( collision.releaseInstance() == SIR_MESH_INVALID_ARGUMENT );
	CHECK( collision.shutdown() == SIR_MESH_OK );
	CHECK( releases == 1 );
	CHECK( collision.shutdown() == SIR_MESH_NOT_READY );
	CHECK( collision.getTriangle( 0, &triangle ) == SIR_MESH_NOT_READY );
	CHECK( collision.acquireInstance() == SIR_MESH_NOT_READY );
	CHECK( collision.raycast( ray, &hit ) == SIR_MESH_NOT_READY );
	CHECK( cache.shutdown() == SIR_MESH_OK );
	puts( "shared collision source: 1000 instances, pin lifetime, ray parity and validation passed" );
	return 0;
}
