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

	const sirEditorTextPropertyIndex_t& properties = workspace.getPropertyIndex();
	assert( properties.nCount == 15 );
	uint32_t iLayout = UINT32_MAX;
	for ( uint32_t iProperty = 0; iProperty < properties.nCount; ++iProperty ) {
		const sirEditorTextProperty_t& property = properties.pProperties[iProperty];
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
	const sirEditorTextProperty_t& written = workspace.getPropertyIndex().pProperties[iLayout];
	assert( written.nValueLength == 1 );
	assert( workspace.getSource()[written.nValueOffset] == '4' );
	workspace.close();
	assert( workspace.shutdown() );
	memory.shutdown();
}

static void sirEditorWriterAdversarialTests( void ) noexcept
{
	const char szSource[] = "[node name=\"Root\" type=\"Node\"]\nvalue = 0\n";
	const char *pszValue = strstr( szSource, "value = 0\n" );
	assert( pszValue != nullptr );
	sirEditorTextProperty_t property = {};
	property.nValueOffset = ( uint32_t )( pszValue - szSource ) + 8;
	property.nValueLength = 1;
	sirEditorTextPropertyIndex_t index = { &property, 1, 1, 0 };
	char szOutput[512] = {};
	size_t nBytes = 0;

	typedef struct sirEditorWriterCase_s
	{
		const char *pszValue;
		bool bValid;
	} sirEditorWriterCase_t;
	const sirEditorWriterCase_t szCases[] = { { "true", true },
	                                          { "false", true },
	                                          { "\"hello\"", true },
	                                          { "\"escaped\\\"quote\"", true },
	                                          { "\"slash\\\\test\"", true },
	                                          { "\"line\\ntext\"", true },
	                                          { "\"unicode\\u12Af\"", true },
	                                          { "\"\"", true },
	                                          { "+42", true },
	                                          { "-7.125", true },
	                                          { "8e+2", true },
	                                          { "-9e-2", true },
	                                          { "3E9", true },
	                                          { "0", true },
	                                          { "+", false },
	                                          { "-", false },
	                                          { "e10", false },
	                                          { "1e", false },
	                                          { "1e+", false },
	                                          { "1e-", false },
	                                          { "1.2.3", false },
	                                          { "1e2bad", false },
	                                          { "\"unicode\\u12Z4\"", false },
	                                          { "\"unicode\\u12\"", false },
	                                          { "\"bad\\q\"", false },
	                                          { "\"raw\nline\"", false },
	                                          { "\"raw\rline\"", false },
	                                          { "\"raw\tline\"", false },
	                                          { "\"a\"b\"", false },
	                                          { "\"unterminated", false },
	                                          { "\"a\\", false } };
	for ( const sirEditorWriterCase_t& test : szCases ) {
		const sirEditorTextEdit_t edit = { 0, test.pszValue, ( uint32_t )strlen( test.pszValue ) };
		memset( szOutput, 0x39, sizeof( szOutput ) );
		const sirEditorTextWriteResult_t eExpected = test.bValid ? SIR_EDITOR_WRITE_OK : SIR_EDITOR_WRITE_INVALID_SCALAR;
		assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &edit, 1, nullptr, 0, &nBytes ) == eExpected );
		if ( !test.bValid ) {
			assert( szOutput[0] == 0x39 );
			continue;
		}
		assert( nBytes < sizeof( szOutput ) );
		assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &edit, 1, szOutput, sizeof( szOutput ), &nBytes ) ==
		        SIR_EDITOR_WRITE_OK );
		assert( memcmp( szOutput + property.nValueOffset, test.pszValue, edit.nReplacementBytes ) == 0 );
	}

	const char szOneQuote[] = "\"";
	const char szTrailingEscape[] = { '"', 'a', '\\', '"', '\0' };
	const sirEditorTextEdit_t malformedQuotes[] = { { 0, szOneQuote, 1 }, { 0, szTrailingEscape, 4 }, { 0, nullptr, 3 }, { 0, "", 0 } };
	for ( const sirEditorTextEdit_t& edit : malformedQuotes ) {
		assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &edit, 1, nullptr, 0, &nBytes ) ==
		        SIR_EDITOR_WRITE_INVALID_SCALAR );
	}
	const sirEditorTextEdit_t escaped[] = { { 0, "\"return\\r\"", 10 }, { 0, "\"tab\\t\"", 7 } };
	for ( const sirEditorTextEdit_t& edit : escaped ) {
		assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &edit, 1, szOutput, sizeof( szOutput ), &nBytes ) ==
		        SIR_EDITOR_WRITE_OK );
	}
	property.nValueLength = 0;
	const sirEditorTextEdit_t anyEdit = { 0, "1", 1 };
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &anyEdit, 1, nullptr, 0, &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_SOURCE );
	property.nValueLength = 1;
	property.nValueOffset = ( uint32_t )strlen( szSource ) + 1;
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &anyEdit, 1, nullptr, 0, &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_SOURCE );
	property.nValueOffset = ( uint32_t )( pszValue - szSource ) + 8;
	sirEditorTextPropertyIndex_t twoIndexes = index;
	twoIndexes.nCount = 2;
	const sirEditorTextEdit_t twoEdits[] = { { 0, "true", 4 }, { 1, "false", 5 } };
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), twoIndexes, twoEdits, 2, nullptr, 0, &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_ARGUMENT );
	char szMutableSource[sizeof( szSource )] = {};
	memcpy( szMutableSource, szSource, sizeof( szSource ) );
	assert( sirEditorWriteScalarPatches( szMutableSource, strlen( szMutableSource ), index, &anyEdit, 1, szMutableSource,
	                                     sizeof( szMutableSource ), &nBytes ) == SIR_EDITOR_WRITE_INVALID_ARGUMENT );

	// Reject output overlap with both the edit descriptors and the retained
	// property metadata; a copying writer must not corrupt unread input.
	sirEditorTextEdit_t szAliasedEdits[4] = {};
	szAliasedEdits[0] = { 0, "42", 2 };
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, szAliasedEdits, 1, ( char * )szAliasedEdits,
	                                     sizeof( szAliasedEdits ), &nBytes ) == SIR_EDITOR_WRITE_INVALID_ARGUMENT );
	assert( szAliasedEdits[0].pszReplacement != nullptr );
	sirEditorTextProperty_t szAliasedProperties[4] = {};
	szAliasedProperties[0] = property;
	sirEditorTextPropertyIndex_t aliasedIndex = { szAliasedProperties, 4, 1, 0 };
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), aliasedIndex, &anyEdit, 1, ( char * )szAliasedProperties,
	                                     sizeof( szAliasedProperties ), &nBytes ) == SIR_EDITOR_WRITE_INVALID_ARGUMENT );
	aliasedIndex.pProperties = nullptr;
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), aliasedIndex, nullptr, 0, nullptr, 0, &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_ARGUMENT );

	const sirEditorTextEdit_t valid = { 0, "42", 2 };
	assert( sirEditorWriteScalarPatches( nullptr, 0, index, &valid, 1, szOutput, sizeof( szOutput ), &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_ARGUMENT );
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &valid, 1, szOutput, sizeof( szOutput ), nullptr ) ==
	        SIR_EDITOR_WRITE_INVALID_ARGUMENT );
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, nullptr, 1, szOutput, sizeof( szOutput ), &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_ARGUMENT );
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &valid, 1, nullptr, 1, &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_ARGUMENT );
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &valid, 1, szOutput, 1, &nBytes ) ==
	        SIR_EDITOR_WRITE_OUT_OF_CAPACITY );
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, nullptr, 0, szOutput, sizeof( szOutput ), &nBytes ) ==
	        SIR_EDITOR_WRITE_OK );
	assert( strcmp( szOutput, szSource ) == 0 );
	const sirEditorTextEdit_t wrongIndex = { 1, "42", 2 };
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &wrongIndex, 1, nullptr, 0, &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_SCALAR );
	index.nCapacity = 0;
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &valid, 1, nullptr, 0, &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_ARGUMENT );
	index.nCapacity = 1;
	property.nValueOffset = ( uint32_t )strlen( szSource );
	assert( sirEditorWriteScalarPatches( szSource, strlen( szSource ), index, &valid, 1, nullptr, 0, &nBytes ) ==
	        SIR_EDITOR_WRITE_INVALID_SOURCE );
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
	// Reject replacement text stored inside the output buffer before writing
	// any bytes; a prior copy must not overwrite a later edit's input.
	memcpy( szGuard + 64, "true", 4 );
	const sirEditorTextEdit_t overlappingReplacement = { 0, szGuard + 64, 4 };
	assert( sirEditorWriteScalarPatches( pszSource, strlen( pszSource ), index, &overlappingReplacement, 1, szGuard, sizeof( szGuard ),
	                                     &nWritten ) == SIR_EDITOR_WRITE_INVALID_ARGUMENT );
	assert( szGuard[0] == 0x6e && memcmp( szGuard + 64, "true", 4 ) == 0 );
	// Replacement slices borrowed from the immutable input remain supported.
	const sirEditorTextEdit_t sourceReplacement = { 0, pszBoolean, 5 };
	assert( sirEditorWriteScalarPatches( pszSource, strlen( pszSource ), index, &sourceReplacement, 1, szOutput, sizeof( szOutput ),
	                                     &nWritten ) == SIR_EDITOR_WRITE_OK );
	assert( strcmp( szOutput, pszSource ) == 0 );
	sirEditorTextEdit_t malformed = { 0, "true\n[node]", 11 };
	assert( sirEditorWriteScalarPatches( pszSource, strlen( pszSource ), index, &malformed, 1, szGuard, sizeof( szGuard ), &nWritten ) ==
	        SIR_EDITOR_WRITE_INVALID_SCALAR );
	sirEditorTextEdit_t duplicate[] = { { 0, "true", 4 }, { 0, "false", 5 } };
	assert( sirEditorWriteScalarPatches( pszSource, strlen( pszSource ), index, duplicate, 2, szGuard, sizeof( szGuard ), &nWritten ) ==
	        SIR_EDITOR_WRITE_DUPLICATE_PATCH );
	assert( strcmp( pszSource,
	                "[gd_scene load_steps=1 format=3]\n[node name=\"Root\" type=\"Node\"]\nvisible = false\ncaption = \"hello\"\n" ) == 0 );
	sirEditorWriterAdversarialTests();
	sirEditorWriterRealSceneRoundtrip();
	return 0;
}
