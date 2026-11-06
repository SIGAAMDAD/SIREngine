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

#include <Terrain/Data/SIRTerrainRegionCatalog.h>

#include <stdio.h>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Terrain catalog assertion failed: %s at line %d\n", #expression, __LINE__ );                                 \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

int main( void )
{
	SIRTerrainRegionCatalog catalog;
	sirTerrainRegionHandle_t handle = {};
	sirTerrainRegionDescriptor_t descriptor = { -4, 9, 2, -10.0f, 100.0f, 1 };
	CHECK( catalog.getCapacity() == 0 );
	CHECK( catalog.upsert( descriptor, handle ) == SIR_TERRAIN_CATALOG_NOT_INITIALIZED );
	CHECK( !catalog.initialize( nullptr, 4 ) );
	sirTerrainRegionCatalogEntry_t badStorage[3] = {};
	CHECK( !catalog.initialize( badStorage, 3 ) );
	sirTerrainRegionCatalogEntry_t storage[4] = {};
	CHECK( catalog.initialize( storage, 4 ) );
	CHECK( !catalog.initialize( storage, 4 ) );
	CHECK( catalog.getCount() == 0 && catalog.getCapacity() == 4 );
	CHECK( catalog.upsert( descriptor, handle ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( catalog.getCount() == 1 );
	sirTerrainRegionDescriptor_t found = {};
	sirTerrainRegionHandle_t foundHandle = {};
	CHECK( catalog.find( { -4, 9 }, found, foundHandle ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( found.nRevision == 1 && foundHandle.nSlot == handle.nSlot && foundHandle.nGeneration == handle.nGeneration );
	CHECK( catalog.find( handle, found ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( catalog.upsert( descriptor, foundHandle ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( foundHandle.nGeneration == handle.nGeneration );
	const sirTerrainRegionHandle_t oldHandle = handle;
	descriptor.nRevision = 2;
	CHECK( catalog.upsert( descriptor, handle ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( catalog.getCount() == 1 && handle.nGeneration != oldHandle.nGeneration );
	CHECK( catalog.find( oldHandle, found ) == SIR_TERRAIN_CATALOG_STALE_HANDLE );
	CHECK( catalog.erase( oldHandle ) == SIR_TERRAIN_CATALOG_STALE_HANDLE );
	descriptor.nRevision = 1;
	CHECK( catalog.upsert( descriptor, foundHandle ) == SIR_TERRAIN_CATALOG_STALE_REVISION );
	descriptor.nRevision = 2;
	descriptor.fMaxHeight = 101.0f;
	CHECK( catalog.upsert( descriptor, foundHandle ) == SIR_TERRAIN_CATALOG_REVISION_CONFLICT );
	descriptor.fMaxHeight = 100.0f;

	sirTerrainRegionHandle_t handles[4] = { handle };
	for ( int32_t iIndex = 1; iIndex < 4; ++iIndex ) {
		const sirTerrainRegionDescriptor_t next = { iIndex, -iIndex, 2, 0.0f, 1.0f, 1 };
		CHECK( catalog.upsert( next, handles[iIndex] ) == SIR_TERRAIN_CATALOG_SUCCESS );
	}
	CHECK( catalog.getCount() == 4 );
	const sirTerrainRegionDescriptor_t extra = { 100, -100, 2, 0.0f, 1.0f, 1 };
	CHECK( catalog.upsert( extra, foundHandle ) == SIR_TERRAIN_CATALOG_FULL );
	CHECK( catalog.find( { 100, -100 }, found, foundHandle ) == SIR_TERRAIN_CATALOG_NOT_FOUND );
	CHECK( catalog.erase( handles[1] ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( catalog.getCount() == 3 );
	CHECK( catalog.find( handles[1], found ) == SIR_TERRAIN_CATALOG_STALE_HANDLE );
	CHECK( catalog.upsert( extra, foundHandle ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( catalog.getCount() == 4 );
	CHECK( catalog.find( { 100, -100 }, found, foundHandle ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( catalog.find( { -4, 9 }, found, foundHandle ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( catalog.find( { 2, -2 }, found, foundHandle ) == SIR_TERRAIN_CATALOG_SUCCESS );
	CHECK( catalog.find( { 3, -3 }, found, foundHandle ) == SIR_TERRAIN_CATALOG_SUCCESS );
	const sirTerrainRegionHandle_t beforeReset = handle;
	catalog.reset();
	CHECK( catalog.getCount() == 0 );
	CHECK( catalog.find( beforeReset, found ) == SIR_TERRAIN_CATALOG_STALE_HANDLE );
	CHECK( catalog.find( { -4, 9 }, found, foundHandle ) == SIR_TERRAIN_CATALOG_NOT_FOUND );
	descriptor.nRevision = 0;
	CHECK( catalog.upsert( descriptor, foundHandle ) == SIR_TERRAIN_CATALOG_INVALID_ARGUMENT );
	return 0;
}
