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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct sirMeshGenerationPin_s
{
	uint32_t nReleaseCount;
} sirMeshGenerationPin_t;

static void onGenerationPinRetired( void *pPin ) noexcept
{
	++( ( sirMeshGenerationPin_t * )pPin )->nReleaseCount;
}

static void checkGeneration( bool bOk, const char *pszMessage ) noexcept
{
	if ( !bOk ) {
		fprintf( stderr, "generation-exhaustion FAILED: %s\n", pszMessage );
		abort();
	}
}

int main( void )
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	const float szVertices[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	sirMeshGenerationPin_t szPins[3] = {};
	sirMeshHandle_t szHandles[3] = {};
	SIRMeshCache cache;
	checkGeneration( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "initialize" );

	sirMeshGeometrySource_t first = { { 10, 1, 0, 0 }, szVertices, szIndices, 3, 3, &szPins[0], onGenerationPinRetired };
	sirMeshGeometrySource_t second = { { 11, 1, 0, 0 }, szVertices, szIndices, 3, 3, &szPins[1], onGenerationPinRetired };
	sirMeshGeometrySource_t third = { { 12, 1, 0, 0 }, szVertices, szIndices, 3, 3, &szPins[2], onGenerationPinRetired };

	checkGeneration( cache.insert( &first, &szHandles[0] ) == SIR_MESH_OK, "penultimate generation" );
	checkGeneration( szHandles[0].nGeneration == UINT32_MAX - 1U, "penultimate handle value" );
	checkGeneration( cache.insert( &second, &szHandles[1] ) == SIR_MESH_OK, "last generation" );
	checkGeneration( szHandles[1].nGeneration == UINT32_MAX, "last handle value" );
	checkGeneration( cache.retire( szHandles[0] ) == SIR_MESH_OK, "retire first slot" );
	checkGeneration( szPins[0].nReleaseCount == 1, "release first source pin" );
	checkGeneration( cache.insert( &third, &szHandles[2] ) == SIR_MESH_OUT_OF_CAPACITY, "generation must not wrap" );
	checkGeneration( cache.getResidentCount() == 1 && szPins[2].nReleaseCount == 0, "failed insert must preserve pin ownership" );

	sirMeshGeometryRead_t read = {};
	checkGeneration( cache.acquire( szHandles[0], &read ) == SIR_MESH_STALE_HANDLE, "retired generation not reused" );
	checkGeneration( cache.acquire( szHandles[1], &read ) == SIR_MESH_OK, "last generation remains valid" );
	checkGeneration( cache.release( szHandles[1] ) == SIR_MESH_OK, "last generation release" );
	checkGeneration( cache.shutdown() == SIR_MESH_OK, "shutdown" );
	checkGeneration( szPins[1].nReleaseCount == 1, "last pin retired once" );
	checkGeneration( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "reinitialize" );
	checkGeneration( cache.insert( &third, &szHandles[2] ) == SIR_MESH_OUT_OF_CAPACITY, "reinitialize cannot recycle generation" );
	checkGeneration( cache.acquire( szHandles[1], &read ) == SIR_MESH_STALE_HANDLE, "old generation remains stale" );
	checkGeneration( cache.shutdown() == SIR_MESH_OK, "final shutdown" );
	checkGeneration( szPins[0].nReleaseCount == 1 && szPins[1].nReleaseCount == 1 && szPins[2].nReleaseCount == 0, "callback counts" );
	return 0;
}
