#include <Editor/Scene/SIREditorTextWriter.h>
#include <Editor/Host/SIREditorSceneWorkspace.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <assert.h>
#include <string.h>

static void sirEditorWriterRealSceneRoundtrip( void )
{
	constexpr uint64_t N_MIB = UINT64_C( 1024 ) * 1024;
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 128 * N_MIB, 64 * N_MIB, 2 * N_MIB, 2 };
	assert( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "EditorWriter", 96 * N_MIB, 56 * N_MIB, SIR_MEMORY_DOMAIN_EDITOR );
	assert( pSegment != nullptr );
	SIREditorSceneWorkspace workspace;
	assert( workspace.initialize( pSegment ) );
	assert( workspace.openTextFile( SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_WORKSPACE_OK );

	const sirEditorTextPropertyIndex_t &properties = workspace.getPropertyIndex();
	assert( properties.nCount == 15 );
	uint32_t iLayout = UINT32_MAX;
	for ( uint32_t iProperty = 0; iProperty < properties.nCount; ++iProperty ) {
		const sirEditorTextProperty_t &property = properties.pProperties[iProperty];
		if ( property.nNameLength == 11 && memcmp( workspace.getSource() + property.nNameOffset, "layout_mode", 11 ) == 0 ) {
			iLayout = iProperty;
			break;
		}
	}
	assert( iLayout != UINT32_MAX );

	const sirEditorTextEdit_t edit = { iLayout, "4", 1 };
	size_t nOutputBytes = 0;
	assert( sirEditorWriteScalarPatches( workspace.getSource(), workspace.getSourceBytes(), properties, &edit, 1, nullptr, 0,
	                                     &nOutputBytes ) == SIR_EDITOR_WRITE_OK );
	char szOutput[8192] = {};
	assert( nOutputBytes < sizeof( szOutput ) );
	assert( sirEditorWriteScalarPatches( workspace.getSource(), workspace.getSourceBytes(), properties, &edit, 1, szOutput,
	                                     sizeof( szOutput ), &nOutputBytes ) == SIR_EDITOR_WRITE_OK );
	workspace.close();

	assert( workspace.openTextSource( szOutput, nOutputBytes ) == SIR_EDITOR_WORKSPACE_OK );
	assert( workspace.getSceneIndex().nNodeCount == 5 );
	assert( workspace.getResourceIndex().nCount == 1 );
	assert( workspace.getPropertyIndex().nCount == 15 );
	const sirEditorTextProperty_t &written = workspace.getPropertyIndex().pProperties[iLayout];
	assert( written.nValueLength == 1 );
	assert( workspace.getSource()[written.nValueOffset] == '4' );
	workspace.close();
	assert( workspace.shutdown() );
	memory.shutdown();
}

int main( void )
{
	const char *pszSource = "[gd_scene load_steps=1 format=3]\n[node name=\"Root\" type=\"Node\"]\nvisible = false\ncaption = \"hello\"\n";
	const char *pszBoolean = strstr( pszSource, "false" );
	const char *pszCaption = strstr( pszSource, "\"hello\"" );
	assert( pszBoolean != nullptr && pszCaption != nullptr );
	sirEditorTextProperty_t szProperties[2] = {};
	szProperties[0].nValueOffset = ( uint32_t )( pszBoolean - pszSource );
	szProperties[0].nValueLength = 5;
	szProperties[1].nValueOffset = ( uint32_t )( pszCaption - pszSource );
	szProperties[1].nValueLength = 7;
	sirEditorTextPropertyIndex_t index = { szProperties, 2, 2, 0 };
	sirEditorTextEdit_t szEdits[] = { { 1, "\"edited\"", 8 }, { 0, "true", 4 } };
	size_t nWritten = 0;
	assert( sirEditorWriteScalarPatches( pszSource, strlen( pszSource ), index, szEdits, 2, nullptr, 0, &nWritten ) ==
	        SIR_EDITOR_WRITE_OK );
	char szOutput[256] = {};
	assert( sirEditorWriteScalarPatches( pszSource, strlen( pszSource ), index, szEdits, 2, szOutput, nWritten + 1, &nWritten ) ==
	        SIR_EDITOR_WRITE_OK );
	assert( strstr( szOutput, "visible = true\n" ) != nullptr );
	assert( strstr( szOutput, "caption = \"edited\"\n" ) != nullptr );
	assert( strlen( szOutput ) == nWritten );
	char szGuard[256] = {};
	memset( szGuard, 0x6e, sizeof( szGuard ) );
	assert( sirEditorWriteScalarPatches( pszSource, strlen( pszSource ), index, szEdits, 2, szGuard, nWritten, &nWritten ) ==
	        SIR_EDITOR_WRITE_OUT_OF_CAPACITY );
	assert( szGuard[0] == 0x6e );
	sirEditorTextEdit_t malformed = { 0, "true\n[node]", 11 };
	assert( sirEditorWriteScalarPatches( pszSource, strlen( pszSource ), index, &malformed, 1, szGuard, sizeof( szGuard ), &nWritten ) ==
	        SIR_EDITOR_WRITE_INVALID_SCALAR );
	sirEditorTextEdit_t duplicate[] = { { 0, "true", 4 }, { 0, "false", 5 } };
	assert( sirEditorWriteScalarPatches( pszSource, strlen( pszSource ), index, duplicate, 2, szGuard, sizeof( szGuard ), &nWritten ) ==
	        SIR_EDITOR_WRITE_DUPLICATE_PATCH );
	assert( strcmp( pszSource,
	                "[gd_scene load_steps=1 format=3]\n[node name=\"Root\" type=\"Node\"]\nvisible = false\ncaption = \"hello\"\n" ) == 0 );
	sirEditorWriterRealSceneRoundtrip();
	return 0;
}
