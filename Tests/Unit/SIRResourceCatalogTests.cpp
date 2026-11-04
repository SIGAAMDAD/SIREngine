#include <Resources/SIRResourceCatalog.h>
#include <assert.h>

int main( void )
{
	sirResourceEntry_t szEntries[5] = {};
	sirResourceDependency_t szEdges[5] = {};
	uint32_t szStack[5] = {};
	uint8_t szVisited[5] = {};
	SIRResourceCatalog catalog;
	assert( catalog.initialize( szEntries, 5, szEdges, 5, szStack, szVisited ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addResource( 10, 1, 100, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addResource( 11, 1, 0, SIR_RESOURCE_SOURCE_EMBEDDED ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addResource( 12, 2, 120, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addResource( 10, 1, 100, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_DUPLICATE );
	assert( catalog.addResource( 14, 1, 0, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_INVALID_ARGUMENT );
	assert( catalog.addDependency( 10, 11 ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addDependency( 11, 12 ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addDependency( 12, 10 ) == SIR_RESOURCE_CATALOG_CYCLIC );
	assert( catalog.addDependency( 10, 11 ) == SIR_RESOURCE_CATALOG_DUPLICATE );
	assert( catalog.addDependency( 10, 77 ) == SIR_RESOURCE_CATALOG_NOT_FOUND );
	assert( catalog.addDependency( 12, 12 ) == SIR_RESOURCE_CATALOG_CYCLIC );
	assert( catalog.getDependencyCount() == 2 );
	sirResourceDependency_t edge = {};
	assert( catalog.dependencyAt( 1, &edge ) == SIR_RESOURCE_CATALOG_OK );
	assert( edge.nOwnerId == 11 && edge.nTargetId == 12 );
	assert( catalog.seal() == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addResource( 17, 1, 170, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_SEALED );
	catalog.shutdown();
	return 0;
}
