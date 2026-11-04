#include <Editor/Scene/SIREditorTextWriter.h>
#include <assert.h>
#include <string.h>

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
	return 0;
}
