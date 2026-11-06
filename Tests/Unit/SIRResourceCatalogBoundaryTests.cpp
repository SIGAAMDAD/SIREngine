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

#include <Resources/SIRResourceCatalog.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void sirCatalogRequire( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "catalog boundary test failed at line %u\n", nLine );
		abort();
	}
}

#define REQUIRE( condition ) sirCatalogRequire( ( condition ), __LINE__ )

int main( void )
{
	sirResourceEntry_t entries[3] = {};
	sirResourceDependency_t edges[3] = {};
	uint32_t scratch[3] = {};
	uint8_t seen[3] = {};
	SIRResourceCatalog catalog;
	sirResourceEntry_t found = {};
	sirResourceDependency_t foundEdge = {};
	uint32_t nCount = 0;
	uint64_t order[3] = { 99, 99, 99 };

	REQUIRE( catalog.addResource( 1, 1, 1, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_UNINITIALIZED );
	REQUIRE( catalog.addDependency( 1, 2 ) == SIR_RESOURCE_CATALOG_UNINITIALIZED );
	REQUIRE( catalog.findResource( 1, &found ) == SIR_RESOURCE_CATALOG_UNINITIALIZED );
	REQUIRE( catalog.dependencyAt( 0, &foundEdge ) == SIR_RESOURCE_CATALOG_UNINITIALIZED );
	REQUIRE( catalog.buildLoadOrder( order, 3, &nCount ) == SIR_RESOURCE_CATALOG_UNINITIALIZED );
	REQUIRE( catalog.seal() == SIR_RESOURCE_CATALOG_UNINITIALIZED );
	REQUIRE( catalog.initialize( nullptr, 3, edges, 3, scratch, seen ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.initialize( entries, 0, edges, 3, scratch, seen ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.initialize( entries, 3, nullptr, 3, scratch, seen ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.initialize( entries, 3, edges, 0, scratch, seen ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.initialize( entries, 3, edges, 3, nullptr, seen ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.initialize( entries, 3, edges, 3, scratch, nullptr ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.initialize( entries, UINT32_MAX, edges, 3, scratch, seen ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.initialize( entries, 3, edges, UINT32_MAX, scratch, seen ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.initialize( entries, 3, edges, 3, scratch, seen ) == SIR_RESOURCE_CATALOG_OK );
	REQUIRE( catalog.initialize( entries, 3, edges, 3, scratch, seen ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.getResourceCount() == 0 && catalog.getDependencyCount() == 0 );
	REQUIRE( catalog.buildLoadOrder( nullptr, 0, &nCount ) == SIR_RESOURCE_CATALOG_OK && nCount == 0 );
	REQUIRE( catalog.buildLoadOrder( nullptr, 0, nullptr ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.buildLoadOrder( nullptr, 1, &nCount ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.findResource( 0, &found ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.findResource( 1, nullptr ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.findResource( 1, &found ) == SIR_RESOURCE_CATALOG_NOT_FOUND );
	REQUIRE( catalog.dependencyAt( 0, nullptr ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.dependencyAt( 0, &foundEdge ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.addResource( 0, 1, 1, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.addResource( 1, 0, 1, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.addResource( 1, 1, 0, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.addResource( 1, 1, 1, ( sirResourceSource_t )0 ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.addResource( 1, 1, 0, SIR_RESOURCE_SOURCE_EMBEDDED ) == SIR_RESOURCE_CATALOG_OK );
	REQUIRE( catalog.addResource( 1, 1, 1, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_DUPLICATE );
	REQUIRE( catalog.addResource( 2, 1, 20, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_OK );
	REQUIRE( catalog.addResource( 3, 1, 30, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_OK );
	REQUIRE( catalog.addResource( 4, 1, 40, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_OUT_OF_CAPACITY );
	REQUIRE( catalog.findResource( 2, &found ) == SIR_RESOURCE_CATALOG_OK && found.nId == 2 );
	REQUIRE( catalog.addDependency( 0, 1 ) == SIR_RESOURCE_CATALOG_NOT_FOUND );
	REQUIRE( catalog.addDependency( 1, 0 ) == SIR_RESOURCE_CATALOG_NOT_FOUND );
	REQUIRE( catalog.addDependency( 10, 1 ) == SIR_RESOURCE_CATALOG_NOT_FOUND );
	REQUIRE( catalog.addDependency( 1, 10 ) == SIR_RESOURCE_CATALOG_NOT_FOUND );
	REQUIRE( catalog.addDependency( 1, 1 ) == SIR_RESOURCE_CATALOG_CYCLIC );
	REQUIRE( catalog.addDependency( 1, 2 ) == SIR_RESOURCE_CATALOG_OK );
	REQUIRE( catalog.addDependency( 1, 2 ) == SIR_RESOURCE_CATALOG_DUPLICATE );
	REQUIRE( catalog.addDependency( 2, 1 ) == SIR_RESOURCE_CATALOG_CYCLIC );
	REQUIRE( catalog.addDependency( 2, 3 ) == SIR_RESOURCE_CATALOG_OK );
	REQUIRE( catalog.addDependency( 3, 1 ) == SIR_RESOURCE_CATALOG_CYCLIC );
	REQUIRE( catalog.addDependency( 1, 3 ) == SIR_RESOURCE_CATALOG_OK );
	REQUIRE( catalog.addDependency( 3, 2 ) == SIR_RESOURCE_CATALOG_OUT_OF_CAPACITY );
	REQUIRE( catalog.getDependencyCount() == 3 );
	REQUIRE( catalog.dependencyAt( 2, &foundEdge ) == SIR_RESOURCE_CATALOG_OK && foundEdge.nTargetId == 3 );
	REQUIRE( catalog.dependencyAt( 3, &foundEdge ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	REQUIRE( catalog.buildLoadOrder( nullptr, 0, &nCount ) == SIR_RESOURCE_CATALOG_OK && nCount == 3 );
	REQUIRE( catalog.buildLoadOrder( order, 2, &nCount ) == SIR_RESOURCE_CATALOG_OUT_OF_CAPACITY );
	REQUIRE( order[0] == 99 && order[1] == 99 );
	REQUIRE( catalog.buildLoadOrder( order, 3, &nCount ) == SIR_RESOURCE_CATALOG_OK );
	REQUIRE( order[0] == 3 && order[1] == 2 && order[2] == 1 );

	const uint64_t nTarget = edges[0].nTargetId;
	edges[0].nTargetId = 400;
	order[0] = 77;
	REQUIRE( catalog.buildLoadOrder( order, 3, &nCount ) == SIR_RESOURCE_CATALOG_NOT_FOUND );
	REQUIRE( order[0] == 77 );
	edges[0].nTargetId = nTarget;
	// Manually corrupt a caller-owned edge array to create an impossible cycle.
	edges[2].nOwnerId = 3;
	edges[2].nTargetId = 1;
	REQUIRE( catalog.buildLoadOrder( order, 3, &nCount ) == SIR_RESOURCE_CATALOG_CYCLIC );
	REQUIRE( order[0] == 77 );
	REQUIRE( catalog.seal() == SIR_RESOURCE_CATALOG_OK );
	REQUIRE( catalog.addResource( 4, 1, 40, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_SEALED );
	REQUIRE( catalog.addDependency( 2, 1 ) == SIR_RESOURCE_CATALOG_SEALED );
	catalog.shutdown();
	REQUIRE( catalog.getResourceCount() == 0 && catalog.getDependencyCount() == 0 );
	REQUIRE( catalog.seal() == SIR_RESOURCE_CATALOG_UNINITIALIZED );
	return 0;
}
