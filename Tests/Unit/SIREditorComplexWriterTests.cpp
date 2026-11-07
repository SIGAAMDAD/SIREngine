#include <Editor/Scene/SIREditorComplexWriter.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main( void )
{
	const char *values[] = { "Vector3(1, 2, 3)", "Transform3D(Basis(1, 0, 0), Vector3(4, 5, 6))", "[1, 2, {\"key\": Vector2(3, 4)}]",
	                         "ExtResource(\"1_a\")", "{\"a\": [1, 2]}" };
	for ( const char *v : values ) {
		assert( sirEditorValidateComplexValue( v, ( uint32_t )strlen( v ) ) );
	}
	const char *invalid[] = { "Vector3(1, 2", "[1, (2]", "[1, 2]]", "\"unterminated", "[]#bad", "[1,\x01]" };
	for ( const char *v : invalid ) {
		assert( !sirEditorValidateComplexValue( v, ( uint32_t )strlen( v ) ) );
	}
	const char source[] = "[node name=\"Root\" type=\"Node3D\"]\ntransform = Vector3(0, 0, 0)\n";
	const char *pValue = strstr( source, "Vector3(0, 0, 0)" );
	assert( pValue != nullptr );
	sirEditorTextProperty_t property = {};
	property.nValueOffset = ( uint32_t )( pValue - source );
	property.nValueLength = ( uint32_t )strlen( "Vector3(0, 0, 0)" );
	sirEditorTextPropertyIndex_t index = { &property, 1, 1, 0 };
	const char *replacement = "Transform3D(Basis(1, 0, 0), Vector3(4, 5, 6))";
	sirEditorTextEdit_t edit = { 0, replacement, ( uint32_t )strlen( replacement ) };
	size_t nBytes = 0;
	assert( sirEditorWriteComplexPatches( source, sizeof( source ) - 1, index, &edit, 1, nullptr, 0, &nBytes ) == SIR_EDITOR_WRITE_OK );
	char output[256] = {};
	assert( sirEditorWriteComplexPatches( source, sizeof( source ) - 1, index, &edit, 1, output, sizeof( output ), &nBytes ) ==
	        SIR_EDITOR_WRITE_OK );
	assert( strstr( output, replacement ) != nullptr );
	assert( strstr( output, "[node name" ) == output );
	char guard[256] = {};
	memset( guard, 0x45, sizeof( guard ) );
	edit.pszReplacement = "Vector3(1, 2";
	edit.nReplacementBytes = ( uint32_t )strlen( edit.pszReplacement );
	assert( sirEditorWriteComplexPatches( source, sizeof( source ) - 1, index, &edit, 1, guard, sizeof( guard ), &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_SCALAR );
	assert( guard[0] == 0x45 );
	puts( "PASS: complex text-scene property source serialization" );
	return 0;
}
