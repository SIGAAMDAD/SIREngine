#include <Editor/Widgets/SIREditorHierarchyModel.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sirEditorNode_t sirHierarchyTestNode( uint64_t nId, uint64_t nParentId ) noexcept
{
	sirEditorNode_t node = {};
	node.nId = nId;
	node.nParentId = nParentId;
	node.nTypeId = 1;
	return node;
}

static void sirHierarchyTestSmallTree( void )
{
	constexpr uint32_t N_CAPACITY = 16;
	sirEditorNode_t szNodes[N_CAPACITY] = {};
	sirEditorNode_t szStaging[N_CAPACITY] = {};
	sirEditorMutation_t szPending[N_CAPACITY] = {};
	sirEditorDelta_t szPendingDeltas[N_CAPACITY] = {};
	sirEditorDelta_t szHistoryDeltas[N_CAPACITY * 4] = {};
	sirEditorHistoryFrame_t szHistoryFrames[8] = {};
	sirEditorIndexEntry_t szIndex[N_CAPACITY * 2] = {};
	sirEditorDocumentStorage_t documentStorage = { szNodes,         szStaging,       szPending, szPendingDeltas,
	                                               szHistoryDeltas, szHistoryFrames, szIndex,   N_CAPACITY,
	                                               N_CAPACITY,      N_CAPACITY * 4,  8,         N_CAPACITY * 2 };
	SIREditorSceneDocument document;
	assert( document.initialize( documentStorage ) == SIR_EDITOR_OK );
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	const sirEditorNode_t szInitial[] = { sirHierarchyTestNode( 10, 0 ),  sirHierarchyTestNode( 20, 10 ), sirHierarchyTestNode( 21, 20 ),
	                                      sirHierarchyTestNode( 22, 20 ), sirHierarchyTestNode( 30, 10 ), sirHierarchyTestNode( 31, 30 ),
	                                      sirHierarchyTestNode( 211, 21 ) };
	for ( const sirEditorNode_t &node : szInitial ) {
		assert( document.queueUpsert( node ) == SIR_EDITOR_OK );
	}
	assert( document.commitEdit() == SIR_EDITOR_OK );

	sirEditorNode_t szSnapshot[N_CAPACITY] = {};
	sirEditorHierarchyLink_t szLinks[N_CAPACITY] = {};
	sirEditorHierarchyState_t szStates[N_CAPACITY] = {};
	sirEditorHierarchyState_t szNextStates[N_CAPACITY] = {};
	sirEditorHierarchyCursor_t szStack[N_CAPACITY] = {};
	sirEditorHierarchyRow_t szVisible[N_CAPACITY] = {};
	sirEditorVirtualRow_t szListRows[N_CAPACITY] = {};
	uint64_t szFenwick[N_CAPACITY + 1] = {};
	sirEditorHierarchyStorage_t storage = { szSnapshot, szLinks,    szStates,  szNextStates, szStack,
	                                        szVisible,  szListRows, szFenwick, N_CAPACITY };
	SIREditorHierarchyModel hierarchy;
	assert( hierarchy.getVisibleRange( 0, 100, 0, nullptr ) == SIR_EDITOR_HIERARCHY_NOT_INITIALIZED );
	assert( hierarchy.initialize( storage, 20 ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.initialize( storage, 20 ) == SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT );
	assert( hierarchy.rebuild( document ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.isCurrent( document ) );
	assert( hierarchy.getVisibleCount() == 3 ); // Root defaults expanded; children default collapsed.
	assert( hierarchy.getTotalHeight() == 60 );
	sirEditorHierarchyRow_t row = {};
	assert( hierarchy.getVisibleRow( 0, &row ) == SIR_EDITOR_HIERARCHY_OK );
	assert( row.nId == 10 && row.nDepth == 0 && row.bExpanded && row.bHasChildren );
	assert( hierarchy.getVisibleRow( 1, &row ) == SIR_EDITOR_HIERARCHY_OK && row.nId == 20 && row.nDepth == 1 );
	assert( hierarchy.getVisibleRow( 2, &row ) == SIR_EDITOR_HIERARCHY_OK && row.nId == 30 );
	assert( hierarchy.getVisibleRow( 3, &row ) == SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT );

	uint64_t nLayout = hierarchy.getLayoutEpoch();
	uint64_t nPaint = hierarchy.getPaintEpoch();
	assert( hierarchy.setSelected( document, 21 ) == SIR_EDITOR_HIERARCHY_OK ); // Hidden selection remains stable.
	assert( hierarchy.getLayoutEpoch() == nLayout && hierarchy.getPaintEpoch() == nPaint + 1 );
	assert( hierarchy.setSelected( document, 21 ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getPaintEpoch() == nPaint + 1 );
	assert( hierarchy.setExpanded( document, 20, true ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == 5 );
	assert( hierarchy.getVisibleRow( 2, &row ) == SIR_EDITOR_HIERARCHY_OK );
	assert( row.nId == 21 && row.nDepth == 2 && row.bSelected );
	assert( hierarchy.setExpanded( document, 21, true ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == 6 );
	assert( hierarchy.getVisibleRow( 3, &row ) == SIR_EDITOR_HIERARCHY_OK );
	assert( row.nId == 211 && row.nDepth == 3 );
	assert( hierarchy.setRowHeight( document, 21, 45 ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getTotalHeight() == 145 );
	sirEditorVirtualRange_t range = {};
	assert( hierarchy.getVisibleRange( 30, 21, 0, &range ) == SIR_EDITOR_HIERARCHY_OK );
	assert( range.iBegin == 1 && range.iEnd == 3 );
	assert( hierarchy.setRowHeight( document, 31, 33 ) == SIR_EDITOR_HIERARCHY_OK ); // Hidden height stored.
	assert( hierarchy.getTotalHeight() == 145 );
	assert( hierarchy.setExpanded( document, 30, true ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == 7 && hierarchy.getTotalHeight() == 178 );

	// Rebuild with explicit source order (siblings 30 then 20; children 22 then 21).
	const sirEditorHierarchyOrder_t szOrder[] = { { 10, 0, 0 },   { 20, 200, 0 }, { 21, 250, 0 }, { 22, 100, 0 },
	                                              { 30, 100, 0 }, { 31, 0, 0 },   { 211, 0, 0 } };
	assert( hierarchy.rebuild( document, szOrder, 7 ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == 7 );
	assert( hierarchy.getVisibleRow( 1, &row ) == SIR_EDITOR_HIERARCHY_OK && row.nId == 30 );
	assert( hierarchy.getVisibleRow( 2, &row ) == SIR_EDITOR_HIERARCHY_OK && row.nId == 31 );
	assert( hierarchy.getVisibleRow( 3, &row ) == SIR_EDITOR_HIERARCHY_OK && row.nId == 20 );
	assert( hierarchy.getVisibleRow( 4, &row ) == SIR_EDITOR_HIERARCHY_OK && row.nId == 22 );
	assert( hierarchy.getVisibleRow( 5, &row ) == SIR_EDITOR_HIERARCHY_OK && row.nId == 21 && row.bSelected );
	assert( hierarchy.getTotalHeight() == 178 );
	assert( hierarchy.rebuild( document, szOrder, 6 ) == SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT );
	assert( hierarchy.getVisibleCount() == 7 );
	const sirEditorHierarchyOrder_t szWrongOrder[] = { { 10, 0, 0 }, { 20, 0, 0 }, { 21, 0, 0 }, { 22, 0, 0 },
	                                                   { 31, 0, 0 }, { 31, 0, 0 }, { 211, 0, 0 } };
	assert( hierarchy.rebuild( document, szWrongOrder, 7 ) == SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT );
	assert( hierarchy.getVisibleCount() == 7 );

	// Stale reads cannot mutate selection/expansion; rebuild preserves state and discards removed selection.
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueRemove( 211 ) == SIR_EDITOR_OK );
	assert( document.queueRemove( 21 ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( !hierarchy.isCurrent( document ) );
	const uint64_t nBefore = hierarchy.getLayoutEpoch();
	assert( hierarchy.setExpanded( document, 30, false ) == SIR_EDITOR_HIERARCHY_STALE_SOURCE );
	assert( hierarchy.setSelected( document, 10 ) == SIR_EDITOR_HIERARCHY_STALE_SOURCE );
	assert( hierarchy.getLayoutEpoch() == nBefore );
	assert( hierarchy.rebuild( document ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getSelectedId() == 0 );
	assert( hierarchy.getVisibleCount() == 5 );  // Expansion of 20/30 persisted.
	assert( hierarchy.getTotalHeight() == 113 ); // 4 rows of 20 plus 33 for 31.
	assert( hierarchy.setRowHeight( document, 999, 30 ) == SIR_EDITOR_HIERARCHY_NOT_FOUND );
	assert( hierarchy.setRowHeight( document, 20, 0 ) == SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT );
	assert( hierarchy.setSelected( document, 999 ) == SIR_EDITOR_HIERARCHY_NOT_FOUND );
	assert( hierarchy.setExpanded( document, 30, false ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == 4 );
	assert( hierarchy.setExpanded( document, 30, false ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == 4 );
	assert( hierarchy.setExpanded( document, 30, true ) == SIR_EDITOR_HIERARCHY_OK );

	// Alias arguments to initialize are rejected before the virtual list takes ownership.
	sirEditorHierarchyStorage_t badStorage = storage;
	badStorage.pNextState = storage.pState;
	SIREditorHierarchyModel badHierarchy;
	assert( badHierarchy.initialize( badStorage, 20 ) == SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT );
	assert( badHierarchy.initialize( storage, 0 ) == SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT );
	// Small storage must reject before changing its visible list.
	sirEditorNode_t szSmallSnapshot[2] = {};
	sirEditorHierarchyLink_t szSmallLinks[2] = {};
	sirEditorHierarchyState_t szSmallStates[2] = {};
	sirEditorHierarchyState_t szSmallNext[2] = {};
	sirEditorHierarchyCursor_t szSmallStack[2] = {};
	sirEditorHierarchyRow_t szSmallVisible[2] = {};
	sirEditorVirtualRow_t szSmallRows[2] = {};
	uint64_t szSmallFenwick[3] = {};
	sirEditorHierarchyStorage_t smallStorage = {
	    szSmallSnapshot, szSmallLinks, szSmallStates, szSmallNext, szSmallStack, szSmallVisible, szSmallRows, szSmallFenwick, 2 };
	assert( badHierarchy.initialize( smallStorage, 20 ) == SIR_EDITOR_HIERARCHY_OK );
	assert( badHierarchy.rebuild( document ) == SIR_EDITOR_HIERARCHY_OUT_OF_CAPACITY );
	assert( badHierarchy.getVisibleCount() == 0 );
	badHierarchy.shutdown();
	hierarchy.shutdown();
	assert( hierarchy.getVisibleRow( 0, &row ) == SIR_EDITOR_HIERARCHY_NOT_INITIALIZED );
	document.shutdown();
}

static void sirHierarchyTestLargeFlatTree( void )
{
	constexpr uint32_t N_CAPACITY = 8192;
	// Fixture-only storage; production backing comes from the editor allocator domain.
	sirEditorNode_t *pNodes = ( sirEditorNode_t * )calloc( N_CAPACITY, sizeof( sirEditorNode_t ) );
	sirEditorNode_t *pStaging = ( sirEditorNode_t * )calloc( N_CAPACITY, sizeof( sirEditorNode_t ) );
	sirEditorMutation_t *pPending = ( sirEditorMutation_t * )calloc( N_CAPACITY, sizeof( sirEditorMutation_t ) );
	sirEditorDelta_t *pPendingDeltas = ( sirEditorDelta_t * )calloc( N_CAPACITY, sizeof( sirEditorDelta_t ) );
	sirEditorDelta_t *pHistory = ( sirEditorDelta_t * )calloc( N_CAPACITY, sizeof( sirEditorDelta_t ) );
	sirEditorHistoryFrame_t *pFrames = ( sirEditorHistoryFrame_t * )calloc( 2, sizeof( sirEditorHistoryFrame_t ) );
	sirEditorIndexEntry_t *pIndex = ( sirEditorIndexEntry_t * )calloc( N_CAPACITY * 2, sizeof( sirEditorIndexEntry_t ) );
	sirEditorNode_t *pSnapshot = ( sirEditorNode_t * )calloc( N_CAPACITY, sizeof( sirEditorNode_t ) );
	sirEditorHierarchyLink_t *pLinks = ( sirEditorHierarchyLink_t * )calloc( N_CAPACITY, sizeof( sirEditorHierarchyLink_t ) );
	sirEditorHierarchyState_t *pState = ( sirEditorHierarchyState_t * )calloc( N_CAPACITY, sizeof( sirEditorHierarchyState_t ) );
	sirEditorHierarchyState_t *pNext = ( sirEditorHierarchyState_t * )calloc( N_CAPACITY, sizeof( sirEditorHierarchyState_t ) );
	sirEditorHierarchyCursor_t *pStack = ( sirEditorHierarchyCursor_t * )calloc( N_CAPACITY, sizeof( sirEditorHierarchyCursor_t ) );
	sirEditorHierarchyRow_t *pVisible = ( sirEditorHierarchyRow_t * )calloc( N_CAPACITY, sizeof( sirEditorHierarchyRow_t ) );
	sirEditorVirtualRow_t *pRows = ( sirEditorVirtualRow_t * )calloc( N_CAPACITY, sizeof( sirEditorVirtualRow_t ) );
	uint64_t *pFenwick = ( uint64_t * )calloc( N_CAPACITY + 1, sizeof( uint64_t ) );
	assert( pNodes && pStaging && pPending && pPendingDeltas && pHistory && pFrames && pIndex );
	assert( pSnapshot && pLinks && pState && pNext && pStack && pVisible && pRows && pFenwick );

	sirEditorDocumentStorage_t documentStorage = { pNodes, pStaging,   pPending,   pPendingDeltas, pHistory, pFrames,
	                                               pIndex, N_CAPACITY, N_CAPACITY, N_CAPACITY,     2,        N_CAPACITY * 2 };
	SIREditorSceneDocument document;
	assert( document.initialize( documentStorage ) == SIR_EDITOR_OK );
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	for ( uint32_t iNode = 1; iNode <= N_CAPACITY; ++iNode ) {
		assert( document.queueUpsert( sirHierarchyTestNode( iNode, iNode == 1 ? 0 : 1 ) ) == SIR_EDITOR_OK );
	}
	assert( document.commitEdit() == SIR_EDITOR_OK );
	sirEditorHierarchyStorage_t storage = { pSnapshot, pLinks, pState, pNext, pStack, pVisible, pRows, pFenwick, N_CAPACITY };
	SIREditorHierarchyModel hierarchy;
	assert( hierarchy.initialize( storage, 19 ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.rebuild( document ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == N_CAPACITY );
	assert( hierarchy.getTotalHeight() == ( uint64_t )N_CAPACITY * 19 );
	sirEditorVirtualRange_t range = {};
	assert( hierarchy.getVisibleRange( 90000, 500, 57, &range ) == SIR_EDITOR_HIERARCHY_OK );
	assert( range.iBegin < range.iEnd && range.iEnd - range.iBegin < 40 );
	sirEditorHierarchyRow_t row = {};
	assert( hierarchy.getVisibleRow( range.iBegin, &row ) == SIR_EDITOR_HIERARCHY_OK );
	assert( row.nDepth == 1 && row.nId != 0 );
	assert( hierarchy.setExpanded( document, 1, false ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == 1 );
	assert( hierarchy.setExpanded( document, 1, true ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == N_CAPACITY );
	hierarchy.shutdown();
	document.shutdown();
	free( pNodes );
	free( pStaging );
	free( pPending );
	free( pPendingDeltas );
	free( pHistory );
	free( pFrames );
	free( pIndex );
	free( pSnapshot );
	free( pLinks );
	free( pState );
	free( pNext );
	free( pStack );
	free( pVisible );
	free( pRows );
	free( pFenwick );
}

static void sirHierarchyTestGodotSceneOrder( void )
{
	FILE *pFile = fopen( SIR_EDITOR_TEST_UI_SCENE, "rb" );
	assert( pFile != nullptr );
	char szSource[4096] = {};
	const size_t nBytes = fread( szSource, 1, sizeof( szSource ), pFile );
	assert( ferror( pFile ) == 0 && feof( pFile ) != 0 );
	fclose( pFile );

	sirEditorTextSceneNode_t szScanned[16] = {};
	char szPaths[2048] = {};
	sirEditorTextSceneIndex_t index = { { szScanned, szPaths, 16, 2048 }, 0, 0, 0 };
	assert( sirEditorScanTextScene( szSource, nBytes, &index ) == SIR_EDITOR_TEXT_OK );
	assert( index.nNodeCount == 5 );
	sirEditorHierarchyOrder_t szOrders[16] = {};
	uint32_t nOrders = 0;
	assert( sirEditorHierarchyOrderFromTextScene( index, szOrders, 3, &nOrders ) == SIR_EDITOR_HIERARCHY_OUT_OF_CAPACITY );
	assert( nOrders == 0 );
	sirEditorTextSceneIndex_t invalidIndex = index;
	invalidIndex.nNodeCount = 17;
	assert( sirEditorHierarchyOrderFromTextScene( invalidIndex, szOrders, 16, &nOrders ) == SIR_EDITOR_HIERARCHY_INVALID_SOURCE );
	assert( sirEditorHierarchyOrderFromTextScene( index, ( sirEditorHierarchyOrder_t * )szScanned, 16, &nOrders ) ==
	        SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT );
	assert( sirEditorHierarchyOrderFromTextScene( index, szOrders, 16, &nOrders ) == SIR_EDITOR_HIERARCHY_OK );
	assert( nOrders == 5 );
	for ( uint32_t iNode = 1; iNode < nOrders; ++iNode ) {
		assert( szOrders[iNode - 1].nId < szOrders[iNode].nId );
	}

	sirEditorNode_t szNodes[16] = {};
	sirEditorNode_t szStaging[16] = {};
	sirEditorMutation_t szPending[16] = {};
	sirEditorDelta_t szPendingDeltas[16] = {};
	sirEditorDelta_t szHistory[32] = {};
	sirEditorHistoryFrame_t szFrames[4] = {};
	sirEditorIndexEntry_t szIndex[32] = {};
	sirEditorDocumentStorage_t documentStorage = { szNodes, szStaging, szPending, szPendingDeltas, szHistory, szFrames, szIndex, 16, 16,
	                                               32,      4,         32 };
	SIREditorSceneDocument document;
	assert( document.initialize( documentStorage ) == SIR_EDITOR_OK );
	sirEditorNode_t szReconcileScratch[16] = {};
	assert( sirEditorReconcileTextScene( document, index, szReconcileScratch, 16 ) == SIR_EDITOR_OK );

	sirEditorNode_t szSnapshot[16] = {};
	sirEditorHierarchyLink_t szLinks[16] = {};
	sirEditorHierarchyState_t szState[16] = {};
	sirEditorHierarchyState_t szNext[16] = {};
	sirEditorHierarchyCursor_t szStack[16] = {};
	sirEditorHierarchyRow_t szVisible[16] = {};
	sirEditorVirtualRow_t szRows[16] = {};
	uint64_t szFenwick[17] = {};
	sirEditorHierarchyStorage_t hierarchyStorage = { szSnapshot, szLinks, szState, szNext, szStack, szVisible, szRows, szFenwick, 16 };
	SIREditorHierarchyModel hierarchy;
	assert( hierarchy.initialize( hierarchyStorage, 23 ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.rebuild( document, szOrders, nOrders ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == 2 );
	assert( hierarchy.setExpanded( document, szScanned[1].node.nId, true ) == SIR_EDITOR_HIERARCHY_OK );
	assert( hierarchy.getVisibleCount() == 5 );
	for ( uint32_t iNode = 0; iNode < index.nNodeCount; ++iNode ) {
		sirEditorHierarchyRow_t row = {};
		assert( hierarchy.getVisibleRow( iNode, &row ) == SIR_EDITOR_HIERARCHY_OK );
		assert( row.nId == szScanned[iNode].node.nId );
		assert( row.nDepth == ( iNode == 0 ? 0u : ( iNode == 1 ? 1u : 2u ) ) );
	}
	assert( hierarchy.getSourceRevision() == document.getRevision() );
	hierarchy.shutdown();
	document.shutdown();
}

int main( void )
{
	sirHierarchyTestSmallTree();
	sirHierarchyTestLargeFlatTree();
	sirHierarchyTestGodotSceneOrder();
	puts( "SIREditor hierarchy view model tests passed" );
	return 0;
}
