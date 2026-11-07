#include <Editor/Scene/SIREditorTextScene.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static sirEditorTextSceneIndex_t sirTestSceneIndex( sirEditorTextSceneNode_t *pNodes, uint32_t nNodes, char *pszPaths,
                                                    uint32_t nPaths ) noexcept
{
	sirEditorTextSceneIndex_t index = {};
	index.storage = { pNodes, pszPaths, nNodes, nPaths };
	return index;
}

static sirEditorDocumentStorage_t sirTestSceneStorage( sirEditorNode_t *pNodes, sirEditorNode_t *pStaging, sirEditorMutation_t *pPending,
                                                       sirEditorDelta_t *pPendingDeltas, sirEditorDelta_t *pHistoryDeltas,
                                                       sirEditorHistoryFrame_t *pHistoryFrames, sirEditorIndexEntry_t *pIndex ) noexcept
{
	return { pNodes, pStaging, pPending, pPendingDeltas, pHistoryDeltas, pHistoryFrames, pIndex, 16, 16, 64, 16, 32 };
}

static void sirTestRealForkScene( void )
{
	FILE *pFile = fopen( SIR_EDITOR_TEST_SCENE, "rb" );
	assert( pFile != nullptr );
	char szSource[8192] = {};
	size_t nSize = fread( szSource, 1, sizeof( szSource ), pFile );
	assert( ferror( pFile ) == 0 );
	assert( feof( pFile ) != 0 );
	fclose( pFile );

	sirEditorTextSceneNode_t szNodes[16] = {};
	char szPaths[2048] = {};
	sirEditorTextSceneIndex_t index = sirTestSceneIndex( szNodes, 16, szPaths, 2048 );
	assert( sirEditorScanTextScene( szSource, nSize, &index ) == SIR_EDITOR_TEXT_OK );
	assert( index.nNodeCount == 6 );
	assert( index.nErrorLine == 0 );
	assert( index.storage.pNodes[0].node.nParentId == 0 );
	assert( index.storage.pNodes[1].node.nParentId == index.storage.pNodes[0].node.nId );
	assert( index.storage.pNodes[0].nPathLength == 0 );
	assert( index.storage.pNodes[3].node.nParentId == index.storage.pNodes[2].node.nId );
	assert( strcmp( szPaths + szNodes[3].nPathOffset, "UniqueNames/UniqueAnimationPlayer" ) == 0 );
	assert( strncmp( szSource + szNodes[3].nNameOffset, "UniqueAnimationPlayer", szNodes[3].nNameLength ) == 0 );

	sirEditorNode_t szLive[16] = {};
	sirEditorNode_t szStaging[16] = {};
	sirEditorMutation_t szPending[16] = {};
	sirEditorDelta_t szPendingDeltas[16] = {};
	sirEditorDelta_t szHistoryDeltas[64] = {};
	sirEditorHistoryFrame_t szFrames[16] = {};
	sirEditorIndexEntry_t szLookup[32] = {};
	sirEditorNode_t szSnapshot[16] = {};
	SIREditorSceneDocument document;
	const sirEditorDocumentStorage_t storage =
	    sirTestSceneStorage( szLive, szStaging, szPending, szPendingDeltas, szHistoryDeltas, szFrames, szLookup );
	assert( document.initialize( storage ) == SIR_EDITOR_OK );
	assert( sirEditorReconcileTextScene( document, index, szSnapshot, 16 ) == SIR_EDITOR_OK );
	assert( document.getRevision() == 2 );
	assert( sirEditorReconcileTextScene( document, index, szSnapshot, 16 ) == SIR_EDITOR_OK );
	assert( document.getRevision() == 2 );
	uint32_t nCount = 0;
	uint64_t nRevision = 0;
	assert( document.copySnapshot( szSnapshot, 16, &nCount, &nRevision ) == SIR_EDITOR_OK );
	assert( nCount == 6 && nRevision == 2 );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.copySnapshot( szSnapshot, 16, &nCount, &nRevision ) == SIR_EDITOR_OK );
	assert( nCount == 0 );
	assert( document.redo() == SIR_EDITOR_OK );
	assert( document.copySnapshot( szSnapshot, 16, &nCount, &nRevision ) == SIR_EDITOR_OK );
	assert( nCount == 6 );

	const char szChange[] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n"
	                        "[node name=\"Camera\" type=\"Camera3D\" parent=\".\" unique_id=765]\n";
	assert( sirEditorScanTextScene( szChange, sizeof( szChange ) - 1, &index ) == SIR_EDITOR_TEXT_OK );
	assert( sirEditorReconcileTextScene( document, index, szSnapshot, 16 ) == SIR_EDITOR_OK );
	assert( document.copySnapshot( szSnapshot, 16, &nCount, &nRevision ) == SIR_EDITOR_OK );
	assert( nCount == 2 );
	assert( document.findNode( UINT64_C( 0x80000000000002fd ), szSnapshot ) == SIR_EDITOR_OK );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.copySnapshot( szSnapshot, 16, &nCount, &nRevision ) == SIR_EDITOR_OK && nCount == 6 );
}

static void sirTestRejectMalformedScenes( void )
{
	sirEditorTextSceneNode_t szNodes[16] = {};
	char szPaths[512] = {};
	sirEditorTextSceneIndex_t index = sirTestSceneIndex( szNodes, 16, szPaths, 512 );
	const char *szReject[] = { "[gd_resource format=3]\n[node name=\"Root\" type=\"Node\"]\n",
	                           "[gd_scene format=3]\n[node name=\"Root\"]\n",
	                           "[gd_scene]\n[node name=\"Root\" type=\"Node\" parent=\".\"]\n",
	                           "[gd_scene]\n[node name=\"Root\" type=\"Node\"]\n[node name=\"Missing\" type=\"Node\" parent=\"Later\"]\n",
	                           "[gd_scene]\n[node name=\"Root\" type=\"Node\"]\n[node name=\"A\" type=\"Node\" parent=\".\"]\n[node "
	                           "name=\"A\" type=\"Node\" parent=\".\"]\n",
	                           "[gd_scene]\n[node name=\"Root\" type=\"Node\"]\n[node name=\"A\" type=\"Node\" parent=\".\" "
	                           "unique_id=8]\n[node name=\"B\" type=\"Node\" parent=\".\" unique_id=8]\n",
	                           "[gd_scene]\n[node name=\"Root\" type=\"Node\" instance=ExtResource(\"1_a\")]\n",
	                           "[gd_scene]\n[node name=\"Root\" type=\"Node\"\n",
	                           "[gd_scene]\n[node name=\"Root\" type=\"Node\" name=\"Duplicate\"]\n" };
	const sirEditorTextSceneResult_t szExpected[] = {
	    SIR_EDITOR_TEXT_INVALID_HEADER,       SIR_EDITOR_TEXT_UNSUPPORTED_INSTANCE, SIR_EDITOR_TEXT_INVALID_NODE,
	    SIR_EDITOR_TEXT_MISSING_PARENT,       SIR_EDITOR_TEXT_DUPLICATE_NODE,       SIR_EDITOR_TEXT_DUPLICATE_NODE,
	    SIR_EDITOR_TEXT_UNSUPPORTED_INSTANCE, SIR_EDITOR_TEXT_INVALID_TAG,          SIR_EDITOR_TEXT_INVALID_TAG };
	for ( uint32_t iCase = 0; iCase < sizeof( szReject ) / sizeof( szReject[0] ); ++iCase ) {
		sirEditorTextSceneResult_t eResult = sirEditorScanTextScene( szReject[iCase], strlen( szReject[iCase] ), &index );
		if ( eResult != szExpected[iCase] ) {
			fprintf( stderr, "Scene case %u returned %u; expected %u\n", iCase, eResult, szExpected[iCase] );
		}
		assert( eResult == szExpected[iCase] );
		assert( index.nNodeCount == 0 && index.nPathBytes == 0 && index.nErrorLine != 0 );
	}
	const char szQuote[] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n"
	                       "text = \"some content\n[node name=\\\"Fake\\\" type=\\\"Node\\\"]\nend\"\n"
	                       "[node name=\"Real\" type=\"Node\" parent=\".\"]\n";
	assert( sirEditorScanTextScene( szQuote, sizeof( szQuote ) - 1, &index ) == SIR_EDITOR_TEXT_OK );
	assert( index.nNodeCount == 2 );

	// An unfinished multiline property must not publish a partial scene index.
	const char szUnclosed[] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n"
	                          "text = \"never closed\n[node name=\\\"Fake\\\" type=\\\"Node\\\"]\n";
	assert( sirEditorScanTextScene( szUnclosed, sizeof( szUnclosed ) - 1, &index ) == SIR_EDITOR_TEXT_INVALID_TAG );
	assert( index.nNodeCount == 0 && index.nPathBytes == 0 && index.nErrorLine != 0 );
	// The same index remains reusable after a malformed import.
	assert( sirEditorScanTextScene( szQuote, sizeof( szQuote ) - 1, &index ) == SIR_EDITOR_TEXT_OK );
	assert( index.nNodeCount == 2 );
}

static void sirTestCapacityAtomicity( void )
{
	sirEditorTextSceneNode_t szNodes[2] = {};
	char szPaths[4] = {};
	sirEditorTextSceneIndex_t index = sirTestSceneIndex( szNodes, 2, szPaths, 4 );
	const char szSource[] = "[gd_scene]\n[node name=\"Root\" type=\"Node\"]\n"
	                        "[node name=\"Child\" type=\"Node\" parent=\".\"]\n";
	assert( sirEditorScanTextScene( szSource, sizeof( szSource ) - 1, &index ) == SIR_EDITOR_TEXT_OUT_OF_CAPACITY );
	assert( index.nNodeCount == 0 && index.nPathBytes == 0 );
	// A parent path can fit exactly while leaving zero bytes for the child's name.
	const char szNested[] = "[gd_scene]\n[node name=\"Root\" type=\"Node\"]\n"
	                        "[node name=\"A\" type=\"Node\" parent=\".\"]\n"
	                        "[node name=\"B\" type=\"Node\" parent=\"A\"]\n";
	char szExactParent[5] = {};
	index.storage.pszPaths = szExactParent;
	index.storage.nPathCapacity = 5;
	assert( sirEditorScanTextScene( szNested, sizeof( szNested ) - 1, &index ) == SIR_EDITOR_TEXT_OUT_OF_CAPACITY );
	assert( index.nNodeCount == 0 && index.nPathBytes == 0 );
	index.storage.pszPaths = szPaths;
	index.storage.nPathCapacity = 64;
	char szLargerPaths[64] = {};
	index.storage.pszPaths = szLargerPaths;
	assert( sirEditorScanTextScene( szSource, sizeof( szSource ) - 1, &index ) == SIR_EDITOR_TEXT_OK );
}

static void sirTestGodotUIFixture( void )
{
	FILE *pFile = fopen( SIR_EDITOR_TEST_UI_SCENE, "rb" );
	assert( pFile != nullptr );
	char szSource[8192] = {};
	size_t nSize = fread( szSource, 1, sizeof( szSource ), pFile );
	assert( ferror( pFile ) == 0 && feof( pFile ) != 0 );
	fclose( pFile );
	sirEditorTextSceneNode_t szNodes[16] = {};
	char szPaths[1024] = {};
	sirEditorTextSceneIndex_t index = sirTestSceneIndex( szNodes, 16, szPaths, 1024 );
	assert( sirEditorScanTextScene( szSource, nSize, &index ) == SIR_EDITOR_TEXT_OK );
	assert( index.nNodeCount == 5 );
	assert( index.storage.pNodes[0].nGodotUniqueId == 852911723 );
	assert( index.storage.pNodes[2].nGodotUniqueId == 1670434164 );
	assert( index.storage.pNodes[2].node.nParentId == index.storage.pNodes[1].node.nId );
	assert( strcmp( szPaths + szNodes[2].nPathOffset, "VBoxContainer/PluginToastButton" ) == 0 );
	assert( szNodes[3].nSourceLine > szNodes[2].nSourceLine );
}

static void sirTestTextSceneAllocationAndAliasFailures( void )
{
	const char szSource[] = "[gd_scene]\n[node name=\"Root\" type=\"Node\"]\n"
	                        "[node name=\"Child\" type=\"Node\" parent=\".\"]\n";
	sirEditorTextSceneNode_t szNodes[4] = {};
	char szPaths[128] = {};
	sirEditorTextSceneIndex_t index = sirTestSceneIndex( szNodes, 4, szPaths, 128 );
	assert( sirEditorScanTextScene( szSource, sizeof( szSource ) - 1, &index ) == SIR_EDITOR_TEXT_OK );
	index.storage.pszPaths = ( char * )szNodes;
	index.storage.nPathCapacity = sizeof( szNodes );
	assert( sirEditorScanTextScene( szSource, sizeof( szSource ) - 1, &index ) == SIR_EDITOR_TEXT_INVALID_ARGUMENT );
	index.storage.pszPaths = ( char * )szSource;
	index.storage.nPathCapacity = sizeof( szSource );
	assert( sirEditorScanTextScene( szSource, sizeof( szSource ) - 1, &index ) == SIR_EDITOR_TEXT_INVALID_ARGUMENT );
	index.storage.pszPaths = szPaths;
	index.storage.nPathCapacity = 128;
	assert( sirEditorScanTextScene( szSource, sizeof( szSource ) - 1, &index ) == SIR_EDITOR_TEXT_OK );

	sirEditorNode_t szLive[16] = {};
	sirEditorNode_t szStaging[16] = {};
	sirEditorMutation_t szPending[1] = {};
	sirEditorDelta_t szPendingDeltas[1] = {};
	sirEditorDelta_t szHistoryDeltas[1] = {};
	sirEditorHistoryFrame_t szFrames[1] = {};
	sirEditorIndexEntry_t szLookup[32] = {};
	sirEditorNode_t szSnapshot[16] = {};
	sirEditorDocumentStorage_t storage = { szLive, szStaging, szPending, szPendingDeltas, szHistoryDeltas, szFrames, szLookup, 16, 1,
	                                       1,      1,         32 };
	SIREditorSceneDocument document;
	assert( document.initialize( storage ) == SIR_EDITOR_OK );
	assert( sirEditorReconcileTextScene( document, index, szSnapshot, 16 ) == SIR_EDITOR_OUT_OF_CAPACITY );
	assert( document.getRevision() == 1 && !document.canUndo() );
	uint64_t nRevision = 0;
	uint32_t nCount = 99;
	assert( document.copySnapshot( szSnapshot, 16, &nCount, &nRevision ) == SIR_EDITOR_OK );
	assert( nCount == 0 );
	assert( sirEditorReconcileTextScene( document, index, ( sirEditorNode_t * )szNodes, 4 ) == SIR_EDITOR_INVALID_ARGUMENT );
}

static void sirTestMutationFuzz( void )
{
	const char szTemplate[] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n"
	                          "[node name=\"Child\" type=\"Node\" parent=\".\" unique_id=1234567]\n";
	const uint32_t nLength = sizeof( szTemplate ) - 1;
	sirEditorTextSceneNode_t szNodes[8] = {};
	char szPaths[256] = {};
	char szMutable[sizeof( szTemplate )] = {};
	sirEditorTextSceneIndex_t index = sirTestSceneIndex( szNodes, 8, szPaths, 256 );
	uint32_t nSeed = 771237u;
	for ( uint32_t iTrial = 0; iTrial < 1200; ++iTrial ) {
		memcpy( szMutable, szTemplate, nLength + 1 );
		for ( uint32_t iEdit = 0; iEdit < 4; ++iEdit ) {
			nSeed = nSeed * 1664525u + 1013904223u;
			uint32_t iOffset = nSeed % nLength;
			nSeed = nSeed * 1664525u + 1013904223u;
			szMutable[iOffset] = ( char )( 32u + ( nSeed >> 18 ) % 94u );
		}
		sirEditorTextSceneResult_t eResult = sirEditorScanTextScene( szMutable, nLength, &index );
		if ( eResult != SIR_EDITOR_TEXT_OK ) {
			assert( index.nNodeCount == 0 && index.nPathBytes == 0 );
			continue;
		}
		assert( index.nNodeCount <= 8 && index.nPathBytes <= 256 );
		for ( uint32_t iNode = 0; iNode < index.nNodeCount; ++iNode ) {
			assert( index.storage.pNodes[iNode].node.nId != 0 );
			assert( index.storage.pNodes[iNode].nPathOffset + index.storage.pNodes[iNode].nPathLength < index.nPathBytes );
		}
	}
}

int main( void )
{
	sirTestRealForkScene();
	sirTestRejectMalformedScenes();
	sirTestCapacityAtomicity();
	sirTestGodotUIFixture();
	sirTestTextSceneAllocationAndAliasFailures();
	sirTestMutationFuzz();
	puts( "SIREditor Godot text scene bridge tests passed" );
	return 0;
}
