#include <Editor/Scene/SIREditorResourceGraphBridge.h>
#include <Resources/SIRResourceGraph.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static void *sirEditorTestAlloc( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nDomain;
	if ( nBytes == 0 || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pData = nullptr;
	const size_t nAlign = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	return posix_memalign( &pData, nAlign, ( size_t )nBytes ) == 0 ? pData : nullptr;
}
static void sirEditorTestFree( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nBytes;
	( void )nAlignment;
	( void )nDomain;
	free( pData );
}
SIRResourceCatalog *sirEditorTestCatalog( void ) noexcept;
void sirEditorTestCatalogShutdown( SIRResourceCatalog *pCatalog ) noexcept;
int main( void )
{
	SIRResourceCatalog *pCatalog = sirEditorTestCatalog();
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { nullptr, sirEditorTestAlloc, sirEditorTestFree };
	config.m_nMaxResources = 8;
	config.m_nMaxStaging = 8;
	config.m_nMaxProviderPins = 8;
	config.m_nMaxCommittedBytes = 256 * 1024;
	SIRResourceGraph graph( config );
	assert( graph.isReady() );
	uint64_t order[4] = {};
	sirEditorResourceRequestKey_t keys[4] = {};
	SIRResourceHandle handles[4] = {};
	sirEditorResourceGraphRequest_t request = {};
	request.pCatalog = pCatalog;
	request.pGraph = &graph;
	request.keyBuffer = { order, keys, 4, 0 };
	request.pOutHandles = handles;
	request.nHandleCapacity = 4;
	assert( sirEditorRequestCatalogResources( &request ) == SIR_EDITOR_GRAPH_OK );
	assert( request.nRequested == 2 );
	assert( keys[0].nCanonicalId == 11 && keys[1].nCanonicalId == 22 );
	assert( handles[0].m_nTypeId == 2 && handles[1].m_nTypeId == 2 );
	SIRResourceReadScope read( graph );
	assert( read.isActive() );
	SIRResourceHandle lookup = {};
	assert( graph.find( read, { 11, 2, 0 }, &lookup ) == SIR_RESOURCE_RESULT_SUCCESS );
	assert( lookup.m_nSlot == handles[0].m_nSlot && lookup.m_nGeneration == handles[0].m_nGeneration );
	request.nHandleCapacity = 1;
	assert( sirEditorRequestCatalogResources( &request ) == SIR_EDITOR_GRAPH_OUT_OF_CAPACITY );
	assert( request.nRequested == 0 );
	sirEditorTestCatalogShutdown( pCatalog );
	puts( "PASS: editor resource catalog requests GRDG" );
	return 0;
}
