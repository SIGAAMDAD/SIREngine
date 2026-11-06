#include <Editor/Scene/SIREditorTextProperties.h>
#include <Editor/Host/SIREditorSceneWorkspace.h>

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void sirEditorTestPropertiesFixture( void )
{
	constexpr uint64_t N_MIB = 1024 * 1024;
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 128 * N_MIB, 64 * N_MIB, 2 * N_MIB, 2 };
	assert( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "EditorProperties", 96 * N_MIB, 56 * N_MIB, SIR_MEMORY_DOMAIN_EDITOR );
	assert( pSegment != nullptr );
	SIREditorSceneWorkspace workspace;
	assert( workspace.initialize( pSegment ) );
	assert( workspace.openTextFile( SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_WORKSPACE_OK );
	const sirEditorTextPropertyIndex_t &props = workspace.getPropertyIndex();
	assert( props.nCount == 15 );
	assert( props.pProperties != nullptr );
	assert( props.pProperties[0].nNodeId == workspace.getSceneIndex().storage.pNodes[0].node.nId );
	assert( props.pProperties[0].nNameLength == 6 );
	assert( memcmp( workspace.getSource() + props.pProperties[0].nNameOffset, "script", 6 ) == 0 );
	assert( props.pProperties[0].nValueLength == strlen( "ExtResource(\"1_j0gfq\")" ) );
	assert( memcmp( workspace.getSource() + props.pProperties[0].nValueOffset, "ExtResource(\"1_j0gfq\")",
	                props.pProperties[0].nValueLength ) == 0 );
	bool bMultiline = false;
	for ( uint32_t i = 0; i < props.nCount; ++i ) {
		if ( props.pProperties[i].nValueLength == strlen( "\"Plugin Toast\n\"" ) &&
		     memcmp( workspace.getSource() + props.pProperties[i].nValueOffset, "\"Plugin Toast\n\"", props.pProperties[i].nValueLength ) ==
		         0 ) {
			bMultiline = true;
		}
	}
	assert( bMultiline );
	workspace.close();

	// Large UTF-8 serialized values must keep their exact native source span.
	char szLongScene[2048] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\ntext = \"";
	size_t nUsed = strlen( szLongScene );
	memset( szLongScene + nUsed, 'x', 511 );
	nUsed += 511;
	szLongScene[nUsed++] = ( char )0xc3;
	szLongScene[nUsed++] = ( char )0xa9;
	szLongScene[nUsed++] = '"';
	szLongScene[nUsed++] = '\n';
	szLongScene[nUsed] = '\0';
	assert( workspace.openTextSource( szLongScene, nUsed ) == SIR_EDITOR_WORKSPACE_OK );
	assert( workspace.getPropertyIndex().nCount == 1 );
	assert( workspace.getPropertyIndex().pProperties[0].nValueLength == 515 );
	const sirEditorTextProperty_t &longValue = workspace.getPropertyIndex().pProperties[0];
	assert( memcmp( workspace.getSource() + longValue.nValueOffset + 512, "\xc3\xa9", 2 ) == 0 );
	workspace.close();
	assert( workspace.getPropertyIndex().nCount == 0 );
	assert( workspace.shutdown() );
	memory.shutdown();
}

static void sirEditorTestPropertiesSynthetic( void )
{
	const char szScene[] = "[gd_scene format=3]\n"
	                       "[node name=\"Root\" type=\"Node\"]\n"
	                       "items = [1,\n"
	                       "  { \"nested\": [2, 3]}]\n"
	                       "text = \"hello\\\"world\"\n"
	                       "[node name=\"Child\" type=\"Node\" parent=\".\"]\n"
	                       "metadata/serialized = { \"arr\": [1, 2], \"obj\": (1, 2) }\n";
	sirEditorTextSceneNode_t nodes[4] = {};
	char szPaths[4096] = {};
	sirEditorTextSceneIndex_t scene = {};
	scene.storage.pNodes = nodes;
	scene.storage.pszPaths = szPaths;
	scene.storage.nNodeCapacity = 4;
	scene.storage.nPathCapacity = 4096;
	assert( sirEditorScanTextScene( szScene, strlen( szScene ), &scene ) == SIR_EDITOR_TEXT_OK );
	assert( scene.nNodeCount == 2 );
	sirEditorTextPropertyIndex_t measure = {};
	assert( sirEditorScanNodeTextProperties( szScene, strlen( szScene ), scene, &measure ) == SIR_EDITOR_PROPERTY_OK );
	assert( measure.nCount == 3 );
	sirEditorTextProperty_t props[4] = {};
	sirEditorTextPropertyIndex_t index = { props, 4, 0, 0 };
	assert( sirEditorScanNodeTextProperties( szScene, strlen( szScene ), scene, &index ) == SIR_EDITOR_PROPERTY_OK );
	assert( index.nCount == 3 );
	assert( props[0].nNodeId == nodes[0].node.nId );
	assert( props[1].nSourceLine == 5 );
	assert( props[2].nNodeId == nodes[1].node.nId );
	assert( memcmp( szScene + props[2].nNameOffset, "metadata/serialized", props[2].nNameLength ) == 0 );
	assert( memchr( szScene + props[0].nValueOffset, '\n', props[0].nValueLength ) != nullptr );
	index.nCapacity = 2;
	assert( sirEditorScanNodeTextProperties( szScene, strlen( szScene ), scene, &index ) == SIR_EDITOR_PROPERTY_OUT_OF_CAPACITY );
	assert( index.nCount == 0 );
	index.nCapacity = 4;
	index.pProperties = ( sirEditorTextProperty_t * )szScene;
	assert( sirEditorScanNodeTextProperties( szScene, strlen( szScene ), scene, &index ) == SIR_EDITOR_PROPERTY_INVALID_ARGUMENT );
	index.pProperties = props;
	const char szBad[] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n"
	                     "items = [1, 2)\n";
	assert( sirEditorScanNodeTextProperties( szBad, strlen( szBad ), scene, &index ) == SIR_EDITOR_PROPERTY_MALFORMED_VALUE );
	const char szBadQuote[] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n"
	                          "name = \"unclosed\n";
	assert( sirEditorScanNodeTextProperties( szBadQuote, strlen( szBadQuote ), scene, &index ) == SIR_EDITOR_PROPERTY_MALFORMED_VALUE );
	const char szMissingEquals[] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n"
	                               "this_is_invalid\n";
	assert( sirEditorScanNodeTextProperties( szMissingEquals, strlen( szMissingEquals ), scene, &index ) ==
	        SIR_EDITOR_PROPERTY_INVALID_SOURCE );
}

int main( void )
{
	sirEditorTestPropertiesFixture();
	sirEditorTestPropertiesSynthetic();
	puts( "PASS: native editor source-backed property inspector index" );
	return 0;
}
