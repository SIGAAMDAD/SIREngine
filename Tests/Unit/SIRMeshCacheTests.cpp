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

#include <Mesh/SIRMeshCache.h>

#include <limits>
#include <stdio.h>

// Covers the CPU metadata-only contract; source bytes are externally owned.
typedef struct sirMeshTestOwner_s
{
	uint32_t nReleases;
} sirMeshTestOwner_t;

static void releaseSourcePin( void *pContext ) noexcept
{
	sirMeshTestOwner_t *pOwner = ( sirMeshTestOwner_t * )pContext;
	++pOwner->nReleases;
}

static bool require( bool bSuccess, const char *pszCase ) noexcept
{
	if ( !bSuccess ) {
		fprintf( stderr, "FAILED: %s\n", pszCase );
	}
	return bSuccess;
}

int main( void )
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	const float szVertices[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	sirMeshTestOwner_t ownerA = {};
	sirMeshTestOwner_t ownerB = {};
	sirMeshTestOwner_t ownerC = {};
	sirMeshGeometrySource_t source = { { 42, 7, 0, 0 }, szVertices, szIndices, 3, 3, &ownerA, releaseSourcePin };
	SIRMeshCache cache;
	sirMeshHandle_t hA = {};
	sirMeshHandle_t hB = {};
	sirMeshGeometryRead_t readA = {};
	sirMeshGeometryRead_t readB = {};

	if ( !require( cache.insert( &source, &hA ) == SIR_MESH_NOT_READY, "reject uninitialized insertion" ) ) {
		return 1;
	}
	if ( !require( cache.requiredBytes( 2 ) <= sizeof( szStorage ), "storage size" ) ) {
		return 1;
	}
	if ( !require( cache.initialize( szStorage + 1, sizeof( szStorage ) - 1, 2 ) == SIR_MESH_INVALID_ARGUMENT,
	               "reject unaligned storage" ) ) {
		return 1;
	}
	if ( !require( cache.initialize( szStorage, cache.requiredBytes( 2 ) - 1, 2 ) == SIR_MESH_INVALID_ARGUMENT,
	               "reject undersized storage" ) ) {
		return 1;
	}
	if ( !require( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "initialize" ) ) {
		return 1;
	}
	if ( !require( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_BUSY, "reject double initialization" ) ) {
		return 1;
	}
	if ( !require( cache.getCapacity() == 2 && cache.getResidentCount() == 0, "initial accounting" ) ) {
		return 1;
	}

	if ( !require( cache.insert( &source, nullptr ) == SIR_MESH_INVALID_ARGUMENT, "reject missing output" ) ) {
		return 1;
	}
	source.pfnReleaseSourcePin = nullptr;
	if ( !require( cache.insert( &source, &hA ) == SIR_MESH_INVALID_ARGUMENT, "require source pin" ) ) {
		return 1;
	}
	source.pfnReleaseSourcePin = releaseSourcePin;
	const uint32_t szBadIndices[3] = { 0, 1, 3 };
	source.pTriangleIndices = szBadIndices;
	if ( !require( cache.insert( &source, &hA ) == SIR_MESH_INVALID_ARGUMENT, "invalid triangle index" ) ) {
		return 1;
	}
	source.pTriangleIndices = szIndices;
	if ( !require( cache.insert( &source, &hA ) == SIR_MESH_OK, "insert mesh" ) ) {
		return 1;
	}
	if ( !require( cache.insert( &source, &hB ) == SIR_MESH_DUPLICATE, "deduplicate mesh key" ) ) {
		return 1;
	}
	if ( !require( cache.find( &source.key, &hB ) == SIR_MESH_OK && hB.nSlot == hA.nSlot, "lookup" ) ) {
		return 1;
	}
	if ( !require( cache.acquire( hA, &readA ) == SIR_MESH_OK, "physics borrow" ) ) {
		return 1;
	}
	if ( !require( cache.acquire( hA, &readB ) == SIR_MESH_OK, "renderer borrow" ) ) {
		return 1;
	}
	if ( !require( readA.pPositionsXYZ == readB.pPositionsXYZ && readA.pPositionsXYZ == szVertices, "physically same vertices" ) ) {
		return 1;
	}
	if ( !require( readA.pTriangleIndices == readB.pTriangleIndices && readA.pTriangleIndices == szIndices, "physically same topology" ) ) {
		return 1;
	}
	if ( !require( cache.getBorrowCount() == 2, "borrow accounting" ) ) {
		return 1;
	}

	if ( !require( cache.retire( hA ) == SIR_MESH_OK, "retire leased mesh" ) ) {
		return 1;
	}
	if ( !require( cache.acquire( hA, &readA ) == SIR_MESH_RETIRED, "no new borrow during retirement" ) ) {
		return 1;
	}
	if ( !require( cache.find( &source.key, &hB ) == SIR_MESH_NOT_FOUND, "retired not discoverable" ) ) {
		return 1;
	}
	if ( !require( ownerA.nReleases == 0, "pin alive with readers" ) ) {
		return 1;
	}
	if ( !require( cache.shutdown() == SIR_MESH_BUSY, "no shutdown while borrowed" ) ) {
		return 1;
	}
	if ( !require( cache.release( hA ) == SIR_MESH_OK && ownerA.nReleases == 0, "one reader still alive" ) ) {
		return 1;
	}
	if ( !require( cache.release( hA ) == SIR_MESH_OK && ownerA.nReleases == 1, "final reader releases pin" ) ) {
		return 1;
	}
	if ( !require( cache.release( hA ) == SIR_MESH_STALE_HANDLE, "stale release rejected" ) ) {
		return 1;
	}

	source.pSourcePin = &ownerB;
	if ( !require( cache.insert( &source, &hB ) == SIR_MESH_OK, "slot reuse" ) ) {
		return 1;
	}
	if ( !require( hB.nSlot == hA.nSlot && hB.nGeneration != hA.nGeneration, "generation increment" ) ) {
		return 1;
	}
	if ( !require( cache.acquire( hA, &readA ) == SIR_MESH_STALE_HANDLE, "ABA protection" ) ) {
		return 1;
	}
	source.key = { 43, 8, 1, 0 };
	source.pSourcePin = &ownerC;
	sirMeshHandle_t hC = {};
	if ( !require( cache.insert( &source, &hC ) == SIR_MESH_OK, "second resident mesh" ) ) {
		return 1;
	}
	source.key = { 44, 9, 0, 0 };
	if ( !require( cache.insert( &source, &hC ) == SIR_MESH_OUT_OF_CAPACITY, "capacity bound" ) ) {
		return 1;
	}
	if ( !require( cache.shutdown() == SIR_MESH_OK, "shutdown releases all idle pins" ) ) {
		return 1;
	}
	if ( !require( ownerA.nReleases == 1 && ownerB.nReleases == 1 && ownerC.nReleases == 1, "exactly once release" ) ) {
		return 1;
	}
	if ( !require( cache.getResidentCount() == 0 && cache.getCapacity() == 0, "shutdown accounting" ) ) {
		return 1;
	}
	if ( !require( cache.shutdown() == SIR_MESH_NOT_READY, "shutdown idempotence status" ) ) {
		return 1;
	}
	return 0;
}
