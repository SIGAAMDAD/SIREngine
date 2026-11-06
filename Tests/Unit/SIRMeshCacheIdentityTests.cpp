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

typedef struct sirMeshIdentityPin_s
{
	uint32_t nReleases;
} sirMeshIdentityPin_t;

static void releaseMeshIdentityPin( void *pState ) noexcept
{
	++( ( sirMeshIdentityPin_t * )pState )->nReleases;
}

static bool checkMeshIdentity( bool bCondition, const char *pszCase ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "Mesh identity FAILED: %s\n", pszCase );
	}
	return bCondition;
}

int main( void )
{
	alignas( 64 ) uint8_t szStorage[1024] = {};
	const float szPositions[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	sirMeshIdentityPin_t older = {};
	sirMeshIdentityPin_t newer = {};
	sirMeshGeometrySource_t oldSource = { { 41, 3, 0, 0 }, szPositions, szIndices, 3, 3, &older, releaseMeshIdentityPin };
	sirMeshGeometrySource_t newSource = { { 41, 4, 0, 0 }, szPositions, szIndices, 3, 3, &newer, releaseMeshIdentityPin };
	SIRMeshCache cache;
	sirMeshHandle_t oldHandle = {};
	sirMeshHandle_t newHandle = {};
	sirMeshHandle_t found = {};

	if ( !checkMeshIdentity( cache.requiredBytes( 2 ) <= sizeof( szStorage ), "bounded storage" ) ||
	     !checkMeshIdentity( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "initialize" ) ||
	     !checkMeshIdentity( cache.insert( &oldSource, &oldHandle ) == SIR_MESH_OK, "insert old revision" ) ) {
		return 1;
	}

	sirMeshKey_t key = oldSource.key;
	key.nReserved = 1;
	if ( !checkMeshIdentity( cache.find( &key, &found ) == SIR_MESH_NOT_FOUND, "reserved bits must not alias resident key" ) ||
	     !checkMeshIdentity( found.nOwnerId == 0, "failed lookup clears output" ) ) {
		return 1;
	}
	key = oldSource.key;
	key.nVariant = 1;
	if ( !checkMeshIdentity( cache.find( &key, &found ) == SIR_MESH_NOT_FOUND, "variant cannot alias" ) ) {
		return 1;
	}
	key = newSource.key;
	if ( !checkMeshIdentity( cache.find( &key, &found ) == SIR_MESH_NOT_FOUND, "new revision absent" ) ||
	     !checkMeshIdentity( cache.insert( &newSource, &newHandle ) == SIR_MESH_OK, "publish new revision" ) ||
	     !checkMeshIdentity( cache.find( &key, &found ) == SIR_MESH_OK && found.nGeneration == newHandle.nGeneration,
	                         "new revision resolved" ) ) {
		return 1;
	}

	sirMeshGeometryRead_t oldRead = {};
	sirMeshGeometryRead_t newRead = {};
	if ( !checkMeshIdentity( cache.acquire( oldHandle, &oldRead ) == SIR_MESH_OK, "old revision borrow" ) ||
	     !checkMeshIdentity( cache.acquire( newHandle, &newRead ) == SIR_MESH_OK, "new revision borrow" ) ||
	     !checkMeshIdentity( oldRead.pPositionsXYZ == newRead.pPositionsXYZ && oldRead.pPositionsXYZ == szPositions,
	                         "revisions retain shared immutable vertex address" ) ||
	     !checkMeshIdentity( oldRead.pTriangleIndices == newRead.pTriangleIndices && oldRead.pTriangleIndices == szIndices,
	                         "revisions retain shared immutable topology address" ) ||
	     !checkMeshIdentity( cache.retire( oldHandle ) == SIR_MESH_OK, "retire old while borrowed" ) ||
	     !checkMeshIdentity( older.nReleases == 0 && newer.nReleases == 0, "no early release" ) ||
	     !checkMeshIdentity( cache.acquire( oldHandle, &oldRead ) == SIR_MESH_RETIRED, "old revision refuses new borrows" ) ||
	     !checkMeshIdentity( cache.release( oldHandle ) == SIR_MESH_OK, "old final borrow release" ) ||
	     !checkMeshIdentity( older.nReleases == 1 && newer.nReleases == 0, "old pin released exactly once" ) ||
	     !checkMeshIdentity( cache.acquire( oldHandle, &oldRead ) == SIR_MESH_STALE_HANDLE, "old handle stale" ) ) {
		return 1;
	}

	for ( uint32_t i = 0; i < 1000; ++i ) {
		sirMeshGeometryRead_t read = {};
		if ( !checkMeshIdentity( cache.acquire( newHandle, &read ) == SIR_MESH_OK && read.pPositionsXYZ == szPositions &&
		                             read.pTriangleIndices == szIndices,
		                         "one backing across 1000 logical users" ) ) {
			return 1;
		}
	}
	if ( !checkMeshIdentity( cache.getBorrowCount() == 1001, "1000 shared borrows plus revision lease" ) ||
	     !checkMeshIdentity( cache.retire( newHandle ) == SIR_MESH_OK, "retire shared backing" ) ||
	     !checkMeshIdentity( newer.nReleases == 0, "retain pin with 1001 borrows" ) ) {
		return 1;
	}
	for ( uint32_t i = 0; i < 1000; ++i ) {
		if ( !checkMeshIdentity( cache.release( newHandle ) == SIR_MESH_OK && newer.nReleases == 0,
		                         "release intermediate shared borrower" ) ) {
			return 1;
		}
	}
	if ( !checkMeshIdentity( cache.release( newHandle ) == SIR_MESH_OK && newer.nReleases == 1, "final borrower releases pin" ) ||
	     !checkMeshIdentity( cache.getResidentCount() == 0 && cache.getBorrowCount() == 0, "accounting clears" ) ||
	     !checkMeshIdentity( cache.shutdown() == SIR_MESH_OK, "shutdown" ) ) {
		return 1;
	}
	return 0;
}
