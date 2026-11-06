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

#include <assert.h>
#include <stdint.h>

static uint32_t sirCatalogFuzzNext( uint32_t *pSeed ) noexcept
{
	uint32_t nValue = *pSeed;
	nValue ^= nValue << 13;
	nValue ^= nValue >> 17;
	nValue ^= nValue << 5;
	*pSeed = nValue;
	return nValue;
}

// Independent bounded reference graph. The production SIRResourceCatalog
// uses its own caller-supplied traversal scratch and iterative cycle checks.
static bool sirCatalogReferenceCanReach( const bool ( &szEdges )[32][32], uint32_t iSource, uint32_t iTarget ) noexcept
{
	uint32_t szStack[32] = {};
	bool szSeen[32] = {};
	uint32_t nTop = 0;
	szStack[nTop++] = iSource;
	szSeen[iSource] = true;
	while ( nTop != 0 ) {
		const uint32_t iNode = szStack[--nTop];
		if ( iNode == iTarget ) {
			return true;
		}
		for ( uint32_t iNext = 0; iNext < 32; ++iNext ) {
			if ( szEdges[iNode][iNext] && !szSeen[iNext] ) {
				szSeen[iNext] = true;
				szStack[nTop++] = iNext;
			}
		}
	}
	return false;
}

int main( void )
{
	sirResourceEntry_t szEntries[32] = {};
	sirResourceDependency_t szDependencies[600] = {};
	uint32_t szScratch[32] = {};
	uint8_t szSeen[32] = {};
	bool szReference[32][32] = {};
	SIRResourceCatalog catalog;
	assert( catalog.initialize( szEntries, 32, szDependencies, 600, szScratch, szSeen ) == SIR_RESOURCE_CATALOG_OK );
	for ( uint32_t iResource = 0; iResource < 32; ++iResource ) {
		const uint64_t nResource = iResource + 1;
		assert( catalog.addResource( nResource, 1, nResource + 100, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_OK );
	}
	assert( catalog.getResourceCount() == 32 );

	uint32_t nAccepted = 0;
	uint32_t nSeed = 0xf154b367;
	for ( uint32_t iTrial = 0; iTrial < 1800; ++iTrial ) {
		const uint32_t iOwner = sirCatalogFuzzNext( &nSeed ) % 32;
		const uint32_t iTarget = sirCatalogFuzzNext( &nSeed ) % 32;
		sirResourceCatalogResult_t eExpected = SIR_RESOURCE_CATALOG_OK;
		if ( iOwner == iTarget || sirCatalogReferenceCanReach( szReference, iTarget, iOwner ) ) {
			eExpected = SIR_RESOURCE_CATALOG_CYCLIC;
		} else if ( szReference[iOwner][iTarget] ) {
			eExpected = SIR_RESOURCE_CATALOG_DUPLICATE;
		}
		const sirResourceCatalogResult_t eActual = catalog.addDependency( iOwner + 1, iTarget + 1 );
		assert( eActual == eExpected );
		if ( eActual == SIR_RESOURCE_CATALOG_OK ) {
			szReference[iOwner][iTarget] = true;
			++nAccepted;
		}
		assert( catalog.getDependencyCount() == nAccepted );
		if ( iTrial % 37 == 0 ) {
			for ( uint32_t iDependency = 0; iDependency < nAccepted; ++iDependency ) {
				sirResourceDependency_t edge = {};
				assert( catalog.dependencyAt( iDependency, &edge ) == SIR_RESOURCE_CATALOG_OK );
				assert( edge.nOwnerId >= 1 && edge.nOwnerId <= 32 );
				assert( edge.nTargetId >= 1 && edge.nTargetId <= 32 );
				assert( szReference[edge.nOwnerId - 1][edge.nTargetId - 1] );
			}
		}
	}
	// Verify every dependent follows its dependencies, and that the plan
	// remains deterministic when requested repeatedly.
	uint64_t szOrder[32] = {};
	uint64_t szSecondOrder[32] = {};
	uint32_t nOrderCount = 0;
	assert( catalog.buildLoadOrder( nullptr, 0, &nOrderCount ) == SIR_RESOURCE_CATALOG_OK );
	assert( nOrderCount == 32 );
	assert( catalog.buildLoadOrder( szOrder, 32, &nOrderCount ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.buildLoadOrder( szSecondOrder, 32, &nOrderCount ) == SIR_RESOURCE_CATALOG_OK );
	uint32_t szPositions[32] = {};
	for ( uint32_t iOrder = 0; iOrder < nOrderCount; ++iOrder ) {
		assert( szOrder[iOrder] >= 1 && szOrder[iOrder] <= 32 );
		assert( szOrder[iOrder] == szSecondOrder[iOrder] );
		szPositions[szOrder[iOrder] - 1] = iOrder;
	}
	for ( uint32_t iOwner = 0; iOwner < 32; ++iOwner ) {
		for ( uint32_t iTarget = 0; iTarget < 32; ++iTarget ) {
			if ( szReference[iOwner][iTarget] ) {
				assert( szPositions[iTarget] < szPositions[iOwner] );
			}
		}
	}
	assert( catalog.dependencyAt( nAccepted, &szDependencies[0] ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	assert( catalog.seal() == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addDependency( 1, 2 ) == SIR_RESOURCE_CATALOG_SEALED );
	assert( catalog.addResource( 33, 1, 133, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_SEALED );
	catalog.shutdown();
	return 0;
}
