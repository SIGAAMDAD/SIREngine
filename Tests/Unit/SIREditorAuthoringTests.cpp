#include <Editor/Scene/SIREditorSceneDocument.h>
#include <Editor/Widgets/SIREditorVirtualList.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static sirEditorNode_t sirTestNode( uint64_t nId, uint64_t nParentId, uint32_t nTypeId ) noexcept
{
	sirEditorNode_t node = {};
	node.nId = nId;
	node.nParentId = nParentId;
	node.nTypeId = nTypeId;
	return node;
}

static void sirTestEditorSceneTransactions( void )
{
	sirEditorNode_t szNodes[16] = {};
	sirEditorNode_t szStaging[16] = {};
	sirEditorMutation_t szPending[8] = {};
	sirEditorDelta_t szPendingDeltas[8] = {};
	sirEditorDelta_t szHistoryDeltas[32] = {};
	sirEditorHistoryFrame_t szHistoryFrames[8] = {};
	sirEditorIndexEntry_t szIndex[32] = {};
	sirEditorDocumentStorage_t storage = { szNodes, szStaging, szPending, szPendingDeltas, szHistoryDeltas, szHistoryFrames, szIndex, 16, 8,
	                                       32,      8,         32 };
	SIREditorSceneDocument document;
	sirEditorNode_t node = {};
	assert( document.initialize( storage ) == SIR_EDITOR_OK );
	assert( document.getRevision() == 1 );
	assert( document.queueUpsert( sirTestNode( 1, 0, 1 ) ) == SIR_EDITOR_NO_TRANSACTION );
	assert( document.beginEdit( 0 ) == SIR_EDITOR_STALE_REVISION );
	assert( document.beginEdit( 1 ) == SIR_EDITOR_OK );
	assert( document.beginEdit( 1 ) == SIR_EDITOR_TRANSACTION_ACTIVE );
	assert( document.queueUpsert( sirTestNode( 2, 1, 2 ) ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTestNode( 1, 0, 1 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.getRevision() == 2 );
	assert( document.findNode( 2, &node ) == SIR_EDITOR_OK && node.nParentId == 1 );
	assert( node.nGeneration == 1 );
	assert( document.canUndo() );

	uint32_t nCount = 0;
	uint64_t nRevision = 0;
	sirEditorNode_t szSnapshot[16] = {};
	assert( document.copySnapshot( szSnapshot, 1, &nCount, &nRevision ) == SIR_EDITOR_OUT_OF_CAPACITY );
	assert( nCount == 2 && nRevision == 2 );
	assert( document.copySnapshot( szSnapshot, 16, &nCount, &nRevision ) == SIR_EDITOR_OK );
	assert( szSnapshot[0].nId == 1 && szSnapshot[1].nId == 2 );

	assert( document.beginEdit( 2 ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTestNode( 1, 2, 1 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_INVALID_HIERARCHY );
	assert( document.getRevision() == 2 );
	assert( document.findNode( 1, &node ) == SIR_EDITOR_OK && node.nParentId == 0 );
	assert( document.cancelEdit() == SIR_EDITOR_OK );
	assert( document.beginEdit( 2 ) == SIR_EDITOR_OK );
	assert( document.queueRemove( 1 ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_INVALID_HIERARCHY );
	assert( document.cancelEdit() == SIR_EDITOR_OK );

	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.findNode( 1, &node ) == SIR_EDITOR_NOT_FOUND );
	assert( document.canRedo() );
	assert( document.redo() == SIR_EDITOR_OK );
	assert( document.findNode( 1, &node ) == SIR_EDITOR_OK );
	assert( document.getRevision() == 4 );

	assert( document.beginEdit( 4 ) == SIR_EDITOR_OK );
	assert( document.queueRemove( 2 ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.beginEdit( 5 ) == SIR_EDITOR_OK );
	assert( document.queueRemove( 1 ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.findNode( 1, &node ) == SIR_EDITOR_NOT_FOUND );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTestNode( 3, 0, 3 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( !document.canRedo() );

	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTestNode( 4, 999, 1 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_INVALID_HIERARCHY );
	assert( document.cancelEdit() == SIR_EDITOR_OK );
	assert( document.findNode( 4, &node ) == SIR_EDITOR_NOT_FOUND );
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTestNode( 0, 0, 1 ) ) == SIR_EDITOR_INVALID_ID );
	assert( document.cancelEdit() == SIR_EDITOR_OK );
	document.shutdown();
	assert( document.findNode( 1, &node ) == SIR_EDITOR_NOT_INITIALIZED );
}

static void sirTestEditorVirtualList( void )
{
	sirEditorVirtualRow_t szRows[16] = {};
	uint64_t szTree[17] = {};
	SIREditorVirtualList list;
	assert( list.initialize( szRows, szTree, 16 ) == SIR_EDITOR_LIST_OK );
	const sirEditorVirtualRow_t szInput[] = { { 1, 10, 0 }, { 2, 20, 0 }, { 3, 30, 0 }, { 4, 40, 0 } };
	assert( list.setRows( szInput, 4 ) == SIR_EDITOR_LIST_OK );
	assert( list.getTotalHeight() == 100 );
	uint64_t nOffset = 0;
	assert( list.getRowOffset( 2, &nOffset ) == SIR_EDITOR_LIST_OK && nOffset == 30 );
	sirEditorVirtualRange_t range = {};
	assert( list.getVisibleRange( 10, 20, 0, &range ) == SIR_EDITOR_LIST_OK );
	assert( range.iBegin == 1 && range.iEnd == 2 );
	assert( list.getVisibleRange( 10, 21, 0, &range ) == SIR_EDITOR_LIST_OK );
	assert( range.iBegin == 1 && range.iEnd == 3 );
	assert( list.setHeight( 1, 50 ) == SIR_EDITOR_LIST_OK );
	assert( list.getTotalHeight() == 130 );
	assert( list.getRowOffset( 2, &nOffset ) == SIR_EDITOR_LIST_OK && nOffset == 60 );
	assert( list.getVisibleRange( 11, 40, 0, &range ) == SIR_EDITOR_LIST_OK );
	assert( range.iBegin == 1 && range.iEnd == 2 );
	assert( list.setHeight( 0, 0 ) == SIR_EDITOR_LIST_INVALID_ARGUMENT );
	assert( list.getTotalHeight() == 130 );
	assert( list.getVisibleRange( UINT64_MAX, UINT64_MAX, UINT64_MAX, &range ) == SIR_EDITOR_LIST_OK );
	assert( range.iBegin <= range.iEnd && range.iEnd <= 4 );
	assert( list.setRows( nullptr, 0 ) == SIR_EDITOR_LIST_OK );
	assert( list.getTotalHeight() == 0 && list.getSize() == 0 );
	list.shutdown();
	assert( list.getRowOffset( 0, &nOffset ) == SIR_EDITOR_LIST_NOT_INITIALIZED );
}

static void sirTestVirtualListDifferential( void )
{
	constexpr uint32_t N_ROWS = 101;
	sirEditorVirtualRow_t szRows[N_ROWS] = {};
	sirEditorVirtualRow_t szStorage[N_ROWS] = {};
	uint64_t szTree[N_ROWS + 1] = {};
	SIREditorVirtualList list;
	assert( list.initialize( szStorage, szTree, N_ROWS ) == SIR_EDITOR_LIST_OK );
	uint32_t nSeed = 0x539123u;
	for ( uint32_t iRow = 0; iRow < N_ROWS; ++iRow ) {
		nSeed = nSeed * 1664525u + 1013904223u;
		szRows[iRow].nId = iRow + 1;
		szRows[iRow].nHeight = nSeed % 37u + 1u;
	}
	assert( list.setRows( szRows, N_ROWS ) == SIR_EDITOR_LIST_OK );
	for ( uint32_t iTrial = 0; iTrial < 400; ++iTrial ) {
		nSeed = nSeed * 1664525u + 1013904223u;
		uint32_t iRow = nSeed % N_ROWS;
		uint32_t nHeight = ( nSeed >> 16 ) % 49u + 1u;
		assert( list.setHeight( iRow, nHeight ) == SIR_EDITOR_LIST_OK );
		szRows[iRow].nHeight = nHeight;
		uint64_t nExpectedTotal = 0;
		for ( uint32_t iNode = 0; iNode < N_ROWS; ++iNode ) {
			uint64_t nOffset = 0;
			assert( list.getRowOffset( iNode, &nOffset ) == SIR_EDITOR_LIST_OK );
			assert( nOffset == nExpectedTotal );
			nExpectedTotal += szRows[iNode].nHeight;
		}
		assert( nExpectedTotal == list.getTotalHeight() );
		nSeed = nSeed * 1664525u + 1013904223u;
		uint64_t nScroll = nSeed % ( nExpectedTotal + 80 );
		uint64_t nViewport = 5 + ( nSeed >> 14 ) % 200;
		uint64_t nEnd = nScroll + nViewport;
		uint64_t nPrefix = 0;
		uint32_t iExpectedBegin = N_ROWS;
		uint32_t iExpectedEnd = N_ROWS;
		for ( uint32_t iNode = 0; iNode < N_ROWS; ++iNode ) {
			uint64_t nNext = nPrefix + szRows[iNode].nHeight;
			if ( iExpectedBegin == N_ROWS && nNext > nScroll ) {
				iExpectedBegin = iNode;
			}
			if ( iExpectedEnd == N_ROWS && nPrefix >= nEnd ) {
				iExpectedEnd = iNode;
			}
			nPrefix = nNext;
		}
		sirEditorVirtualRange_t range = {};
		assert( list.getVisibleRange( nScroll, nViewport, 0, &range ) == SIR_EDITOR_LIST_OK );
		assert( range.iBegin == iExpectedBegin && range.iEnd == iExpectedEnd );
	}
}

int main( void )
{
	sirTestEditorSceneTransactions();
	sirTestEditorVirtualList();
	sirTestVirtualListDifferential();
	puts( "SIREditor authoring tests passed" );
	return 0;
}
