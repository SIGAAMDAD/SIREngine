#include <Resources/SIRResourceCatalog.h>
#include <assert.h>

SIRResourceCatalog *sirEditorTestCatalog( void ) noexcept
{
	static sirResourceEntry_t szEntries[4] = {};
	static sirResourceDependency_t szDependencies[4] = {};
	static uint32_t szScratch[4] = {};
	static uint8_t szSeen[4] = {};
	static SIRResourceCatalog catalog;
	assert( catalog.initialize( szEntries, 4, szDependencies, 4, szScratch, szSeen ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addResource( 11, 2, 101, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addResource( 22, 2, 102, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.addDependency( 22, 11 ) == SIR_RESOURCE_CATALOG_OK );
	assert( catalog.seal() == SIR_RESOURCE_CATALOG_OK );
	return &catalog;
}
void sirEditorTestCatalogShutdown( SIRResourceCatalog *pCatalog ) noexcept
{
	pCatalog->shutdown();
}
