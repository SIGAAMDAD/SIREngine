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

#include <stdio.h>
#include <stdlib.h>

typedef struct sirMeshBorrowPin_s
{
	uint32_t nReleased;
} sirMeshBorrowPin_t;

static void requireBorrow( bool bCondition, const char *pszMessage ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "mesh borrow-limit FAILED: %s\n", pszMessage );
		abort();
	}
}

static void releaseBorrowPin( void *pContext ) noexcept
{
	++( ( sirMeshBorrowPin_t * )pContext )->nReleased;
}

int main( void )
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	const float szPositions[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	sirMeshBorrowPin_t szPins[2] = {};
	sirMeshGeometrySource_t first = { { 101, 1, 0, 0 }, szPositions, szIndices, 3, 3, &szPins[0], releaseBorrowPin };
	sirMeshGeometrySource_t second = { { 102, 1, 0, 0 }, szPositions, szIndices, 3, 3, &szPins[1], releaseBorrowPin };
	SIRMeshCache cache;
	sirMeshHandle_t firstHandle = {};
	sirMeshHandle_t secondHandle = {};
	sirMeshGeometryRead_t read = {};
	requireBorrow( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "initialize" );
	requireBorrow( cache.insert( &first, &firstHandle ) == SIR_MESH_OK, "first insert" );
	requireBorrow( cache.insert( &second, &secondHandle ) == SIR_MESH_OK, "second insert" );
	requireBorrow( cache.acquire( firstHandle, &read ) == SIR_MESH_OK, "borrow first once" );
	requireBorrow( cache.acquire( firstHandle, &read ) == SIR_MESH_OK, "borrow first twice" );
	requireBorrow( cache.acquire( firstHandle, &read ) == SIR_MESH_OUT_OF_CAPACITY, "per-slot borrow limit" );
	requireBorrow( read.pPositionsXYZ == nullptr, "failed per-slot acquire clears read" );
	requireBorrow( cache.acquire( secondHandle, &read ) == SIR_MESH_OUT_OF_CAPACITY, "global borrow limit" );
	requireBorrow( read.pTriangleIndices == nullptr && cache.getBorrowCount() == 2, "failed global acquire preserves count" );
	requireBorrow( cache.retire( firstHandle ) == SIR_MESH_OK, "deferred retirement" );
	requireBorrow( cache.release( firstHandle ) == SIR_MESH_OK, "first release" );
	requireBorrow( szPins[0].nReleased == 0, "pin still borrowed" );
	requireBorrow( cache.acquire( secondHandle, &read ) == SIR_MESH_OK, "slot can borrow after releasing global capacity" );
	requireBorrow( cache.release( firstHandle ) == SIR_MESH_OK, "final release of retired first" );
	requireBorrow( szPins[0].nReleased == 1, "first pin released exactly once" );
	requireBorrow( cache.release( secondHandle ) == SIR_MESH_OK, "release second" );
	requireBorrow( cache.shutdown() == SIR_MESH_OK, "shutdown" );
	requireBorrow( szPins[1].nReleased == 1 && cache.getBorrowCount() == 0, "second pin released once" );
	return 0;
}
