#include <Editor/Scene/SIREditorTextScene.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef SIR_EDITOR_TEST_UI_SCENE
#define SIR_EDITOR_TEST_UI_SCENE "Tests/Fixtures/Editor/SourceGodotUI.tscn"
#endif

static void sirTestExpectedResource( const char *pszSource, const sirEditorTextResource_t& resource, const char *pszId, const char *pszPath,
                                     sirEditorTextResourceKind_t eKind )
{
	assert( resource.eKind == eKind );
	assert( resource.nStableKey != 0 );
	assert( strlen( pszId ) == resource.nIdLength );
	assert( memcmp( pszSource + resource.nIdOffset, pszId, resource.nIdLength ) == 0 );
	if ( pszPath != nullptr ) {
		assert( resource.nPathLength == strlen( pszPath ) );
		assert( memcmp( pszSource + resource.nPathOffset, pszPath, resource.nPathLength ) == 0 );
	}
}

static void sirTestGodotFixtureResources( void )
{
	FILE *pFile = fopen( SIR_EDITOR_TEST_UI_SCENE, "rb" );
	assert( pFile != nullptr );
	char szSource[8192] = {};
	const size_t nBytes = fread( szSource, 1, sizeof( szSource ), pFile );
	assert( ferror( pFile ) == 0 && feof( pFile ) != 0 );
	fclose( pFile );

	sirEditorTextResource_t szResources[12] = {};
	sirEditorTextResourceIndex_t index = { szResources, 12, 0, 0 };
	assert( sirEditorScanTextResources( szSource, nBytes, &index ) == SIR_EDITOR_RESOURCE_OK );
	assert( index.nCount == 1 && index.nErrorLine == 0 );
	sirTestExpectedResource( szSource, szResources[0], "1_j0gfq", "res://main.gd", SIR_EDITOR_RESOURCE_EXTERNAL );
	assert( szResources[0].nUidLength == strlen( "uid://bv6y7in6otgcm" ) );
	uint32_t iResource = 99;
	assert( sirEditorFindTextResource( szSource, index, SIR_EDITOR_RESOURCE_EXTERNAL, "1_j0gfq", 7, &iResource ) ==
	        SIR_EDITOR_RESOURCE_OK );
	assert( iResource == 0 );
	assert( sirEditorFindTextResource( szSource, index, SIR_EDITOR_RESOURCE_EMBEDDED, "1_j0gfq", 7, &iResource ) ==
	        SIR_EDITOR_RESOURCE_MISSING_REFERENCE );
}

static void sirTestExternalEmbeddedAndForwardResourceReferences( void )
{
	const char szSource[] = "[gd_scene load_steps=4 format=3]\n"
	                        "[ext_resource type=\"Script\" path=\"res://main.gd\" uid=\"uid://abcd\" id=\"1_a\"]\n"
	                        "[ext_resource type=\"Texture2D\" path=\"res://icon.png\" id=\"2_b\"]\n"
	                        "[sub_resource type=\"BoxShape3D\" id=\"1_a\"]\n"
	                        "size = Vector3(1, 2, 3)\n"
	                        "[node name=\"Main\" type=\"Node3D\"]\n"
	                        "script = ExtResource(\"1_a\")\n"
	                        "texture = ExtResource( \"2_b\" )\n"
	                        "shape = SubResource(\"1_a\")\n"
	                        "comment = \"ExtResource(\\\"missing\\\")\"\n"
	                        "multiline = \"line 1\n"
	                        "[ext_resource type=\\\"NotResource\\\" path=\\\"bad\\\" id=\\\"no\\\"]\n"
	                        "line 3\"\n"
	                        "# ExtResource(\"missing\")\n";
	sirEditorTextResource_t szResources[4] = {};
	sirEditorTextResourceIndex_t index = { szResources, 4, 0, 0 };
	assert( sirEditorScanTextResources( szSource, sizeof( szSource ) - 1, &index ) == SIR_EDITOR_RESOURCE_OK );
	assert( index.nCount == 3 );
	sirTestExpectedResource( szSource, szResources[0], "1_a", "res://main.gd", SIR_EDITOR_RESOURCE_EXTERNAL );
	sirTestExpectedResource( szSource, szResources[1], "2_b", "res://icon.png", SIR_EDITOR_RESOURCE_EXTERNAL );
	sirTestExpectedResource( szSource, szResources[2], "1_a", nullptr, SIR_EDITOR_RESOURCE_EMBEDDED );
	assert( szResources[0].nStableKey != szResources[2].nStableKey );
	assert( szResources[0].nSourceLine == 2 && szResources[2].nSourceLine == 4 );
	const char szForward[] = "[gd_scene]\n[node name=\"R\" type=\"Node\"]\n"
	                         "shape = SubResource(\"new_shape\")\n"
	                         "[sub_resource type=\"Shape3D\" id=\"new_shape\"]\n";
	assert( sirEditorScanTextResources( szForward, sizeof( szForward ) - 1, &index ) == SIR_EDITOR_RESOURCE_OK );
	assert( index.nCount == 1 );
}

static void sirTestMalformedAndMissingResources( void )
{
	const char *szInputs[] = { "[gd_resource]\n[ext_resource type=\"X\" path=\"res://a\" id=\"1_a\"]\n",
	                           "[gd_scene]\n[ext_resource type=\"Script\" id=\"1_a\"]\n",
	                           "[gd_scene]\n[ext_resource type=\"Script\" path=\"res://a\" id=\"bad-id\"]\n",
	                           ( "[gd_scene]\n[ext_resource type=\"Script\" path=\"res://a\" id=\"1_a\"]\n"
	                             "[ext_resource type=\"Texture2D\" path=\"res://b\" id=\"1_a\"]\n" ),
	                           "[gd_scene]\n[sub_resource type=\"A\" id=\"a\"]\n[sub_resource type=\"B\" id=\"a\"]\n",
	                           "[gd_scene]\n[node name=\"R\" type=\"Node\"]\nscript = ExtResource(\"missing\")\n",
	                           "[gd_scene]\n[node name=\"R\" type=\"Node\"]\nshape = SubResource(\"missing\")\n",
	                           "[gd_scene]\n[node name=\"R\" type=\"Node\"]\nscript = ExtResource(noquote)\n",
	                           "[gd_scene]\n[ext_resource type=\"A\" path=\"res://a\" id=\"a\" id=\"b\"]\n",
	                           "[gd_scene]\n[ext_resource type=\"A\" path=\"res://a\" id=\"a\"\n" };
	const sirEditorTextResourceResult_t szExpected[] = { SIR_EDITOR_RESOURCE_INVALID_HEADER,      SIR_EDITOR_RESOURCE_INVALID_DECLARATION,
	                                                     SIR_EDITOR_RESOURCE_INVALID_DECLARATION, SIR_EDITOR_RESOURCE_DUPLICATE_ID,
	                                                     SIR_EDITOR_RESOURCE_DUPLICATE_ID,        SIR_EDITOR_RESOURCE_MISSING_REFERENCE,
	                                                     SIR_EDITOR_RESOURCE_MISSING_REFERENCE,   SIR_EDITOR_RESOURCE_INVALID_REFERENCE,
	                                                     SIR_EDITOR_RESOURCE_INVALID_TAG,         SIR_EDITOR_RESOURCE_INVALID_TAG };
	sirEditorTextResource_t szResources[8] = {};
	sirEditorTextResourceIndex_t index = { szResources, 8, 0, 0 };
	for ( uint32_t iCase = 0; iCase < sizeof( szInputs ) / sizeof( szInputs[0] ); ++iCase ) {
		const char *pszSource = szInputs[iCase];
		const sirEditorTextResourceResult_t eResult = sirEditorScanTextResources( pszSource, strlen( pszSource ), &index );
		if ( eResult != szExpected[iCase] ) {
			fprintf( stderr, "resource case %u got %u instead of %u\n", iCase, eResult, szExpected[iCase] );
		}
		assert( eResult == szExpected[iCase] );
		assert( index.nCount == 0 && index.nErrorLine != 0 );
	}
}

static void sirTestEmptyCapacityAndAliases( void )
{
	const char szEmpty[] = "[gd_scene]\n[node name=\"R\" type=\"Node\"]\n";
	sirEditorTextResourceIndex_t index = { nullptr, 0, 0, 0 };
	assert( sirEditorScanTextResources( szEmpty, sizeof( szEmpty ) - 1, &index ) == SIR_EDITOR_RESOURCE_OK );
	assert( index.nCount == 0 );
	const char szWithResource[] = "[gd_scene]\n[ext_resource type=\"A\" path=\"res://a\" id=\"a\"]\n";
	assert( sirEditorScanTextResources( szWithResource, sizeof( szWithResource ) - 1, &index ) == SIR_EDITOR_RESOURCE_OUT_OF_CAPACITY );
	assert( index.nCount == 0 );
	index.pResources = ( sirEditorTextResource_t * )szWithResource;
	index.nCapacity = 1;
	assert( sirEditorScanTextResources( szWithResource, sizeof( szWithResource ) - 1, &index ) == SIR_EDITOR_RESOURCE_INVALID_ARGUMENT );
	assert( index.nCount == 0 );
	index.pResources = ( sirEditorTextResource_t * )&index;
	assert( sirEditorScanTextResources( szWithResource, sizeof( szWithResource ) - 1, &index ) == SIR_EDITOR_RESOURCE_INVALID_ARGUMENT );
	index.pResources = nullptr;
	index.nCapacity = 1;
	assert( sirEditorScanTextResources( szWithResource, sizeof( szWithResource ) - 1, &index ) == SIR_EDITOR_RESOURCE_INVALID_ARGUMENT );
}

static void sirTestMutatedResourceInputs( void )
{
	const char szTemplate[] = "[gd_scene format=3]\n[ext_resource type=\"Script\" path=\"res://main.gd\" id=\"1_a\"]\n"
	                          "[node name=\"R\" type=\"Node\"]\nscript = ExtResource(\"1_a\")\n";
	const uint32_t nBytes = sizeof( szTemplate ) - 1;
	char szMutable[sizeof( szTemplate )] = {};
	sirEditorTextResource_t szResources[4] = {};
	sirEditorTextResourceIndex_t index = { szResources, 4, 0, 0 };
	uint32_t nSeed = 13971;
	for ( uint32_t iTrial = 0; iTrial < 1500; ++iTrial ) {
		memcpy( szMutable, szTemplate, sizeof( szMutable ) );
		for ( uint32_t iEdit = 0; iEdit < 4; ++iEdit ) {
			nSeed = nSeed * 1664525 + 1013904223;
			const uint32_t iOffset = nSeed % nBytes;
			nSeed = nSeed * 1664525 + 1013904223;
			szMutable[iOffset] = ( char )( 32u + ( nSeed >> 18 ) % 94u );
		}
		const sirEditorTextResourceResult_t eResult = sirEditorScanTextResources( szMutable, nBytes, &index );
		if ( eResult != SIR_EDITOR_RESOURCE_OK ) {
			assert( index.nCount == 0 );
			continue;
		}
		assert( index.nCount <= 4 );
		for ( uint32_t i = 0; i < index.nCount; ++i ) {
			assert( index.pResources[i].nIdOffset + index.pResources[i].nIdLength <= nBytes );
			assert( index.pResources[i].nStableKey != 0 );
		}
	}
}

int main( void )
{
	sirTestGodotFixtureResources();
	sirTestExternalEmbeddedAndForwardResourceReferences();
	sirTestMalformedAndMissingResources();
	sirTestEmptyCapacityAndAliases();
	sirTestMutatedResourceInputs();
	puts( "SIREditor Godot resource dependency tests passed" );
	return 0;
}
