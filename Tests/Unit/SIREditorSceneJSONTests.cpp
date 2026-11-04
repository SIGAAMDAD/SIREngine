#include <Editor/Host/SIREditorSceneJSON.h>
#include <Editor/Host/SIREditorSceneWorkspace.h>

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main( void )
{
	constexpr uint64_t N_MIB = UINT64_C( 1024 ) * 1024;
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 128 * N_MIB, 64 * N_MIB, 2 * N_MIB, 2 };
	assert( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "EditorJSON", 96 * N_MIB, 56 * N_MIB, SIR_MEMORY_DOMAIN_EDITOR );
	assert( pSegment != nullptr );
	SIREditorSceneWorkspace workspace;
	assert( workspace.initialize( pSegment ) );
	FILE *pOutput = tmpfile();
	assert( pOutput != nullptr );
	assert( !sirEditorWriteSceneJSON( workspace, "test.tscn", pOutput ) );
	assert( ftell( pOutput ) == 0 );
	assert( workspace.openTextFile( SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_WORKSPACE_OK );
	const char szPath[] = "C:\\scene\\\"test\\n.tscn";
	assert( sirEditorWriteSceneJSON( workspace, szPath, pOutput ) );
	assert( fflush( pOutput ) == 0 );
	assert( fseek( pOutput, 0, SEEK_SET ) == 0 );
	char szBuffer[8192];
	const size_t nRead = fread( szBuffer, 1, sizeof( szBuffer ) - 1, pOutput );
	assert( nRead > 0 && nRead < sizeof( szBuffer ) - 1 );
	szBuffer[nRead] = 0;
	assert( strstr( szBuffer, "\"schema\":1" ) != nullptr );
	assert( strstr( szBuffer, "\"nodes\":[" ) != nullptr );
	assert( strstr( szBuffer, "\"resources\":[" ) != nullptr );
	assert( strstr( szBuffer, "\"parent\":\"0\"" ) != nullptr );
	assert( strstr( szBuffer, "\"path\":\"VBoxContainer/PluginToastButton\"" ) != nullptr );
	assert( strstr( szBuffer, "\"id\":\"9223372037707687531\"" ) != nullptr );
	assert( strstr( szBuffer, "\\\\scene\\\\\\\"test" ) != nullptr );
	fclose( pOutput );
	workspace.close();
	assert( workspace.shutdown() );
	memory.shutdown();
	puts( "PASS: native Godot editor JSON scene contract" );
	return 0;
}
