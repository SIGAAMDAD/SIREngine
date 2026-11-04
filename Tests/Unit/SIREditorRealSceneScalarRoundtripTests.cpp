/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Editor/Host/SIREditorSceneWorkspace.h>
#include <Editor/Scene/SIREditorScalarBinding.h>
#include <Editor/Scene/SIREditorFileSave.h>

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined( _WIN32 )
	#include <process.h>
#else
	#include <unistd.h>
#endif

static bool sirTestRoundtripAtomName( uint64_t nAtom, const char *pszText, uint32_t nBytes, void *pContext ) noexcept
{
	( void )pContext;
	const char *pszName = nullptr;
	switch ( nAtom ) {
		case 101: pszName = "offset_left"; break;
		case 102: pszName = "offset_top"; break;
		case 103: pszName = "offset_right"; break;
		case 104: pszName = "offset_bottom"; break;
		case 105: pszName = "layout_mode"; break;
		case 106: pszName = "theme_override_constants/separation"; break;
		default: return false;
	}
	return strlen( pszName ) == nBytes && memcmp( pszText, pszName, nBytes ) == 0;
}

static void sirTestRoundtripImport( SIREditorSceneWorkspace& workspace, const SIRObjectRegistry& registry,
                                     sirEditorScalarBinding_t *pBindings, uint32_t *pOutCount ) noexcept
{
	sirEditorScalarDiscovery_t discovery = {};
	discovery.pDocument = &workspace.getDocument();
	discovery.pRegistry = &registry;
	discovery.pIndex = &workspace.getPropertyIndex();
	discovery.pszSource = workspace.getSource();
	discovery.nSourceBytes = workspace.getSourceBytes();
	discovery.pMatchesAtom = sirTestRoundtripAtomName;
	assert( sirEditorDiscoverScalarBindings( discovery, pOutCount ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
	assert( *pOutCount == 8 );
	discovery.pOutput = pBindings;
	discovery.nOutputCapacity = 16;
	assert( sirEditorDiscoverScalarBindings( discovery, pOutCount ) == SIR_EDITOR_SCALAR_BRIDGE_OK );

	sirEditorProperty_t szScratch[16] = {};
	sirEditorScalarBridge_t bridge = {};
	bridge.pDocument = &workspace.getDocument();
	bridge.pRegistry = &registry;
	bridge.pIndex = &workspace.getPropertyIndex();
	bridge.pszSource = workspace.getSource();
	bridge.nSourceBytes = workspace.getSourceBytes();
	bridge.pBindings = pBindings;
	bridge.nBindingCount = *pOutCount;
	bridge.pScratch = szScratch;
	assert( sirEditorImportBoundScalars( bridge ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
}

int main( void )
{
	constexpr uint64_t N_MIB = UINT64_C( 1024 ) * 1024;
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 128 * N_MIB, 64 * N_MIB, 2 * N_MIB, 2 };
	assert( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "EditorRealRoundtrip", 96 * N_MIB, 56 * N_MIB, SIR_MEMORY_DOMAIN_EDITOR );
	assert( pSegment != nullptr );
	SIREditorSceneWorkspace workspace;
	assert( workspace.initialize( pSegment ) );
	assert( workspace.openTextFile( SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_WORKSPACE_OK );
	assert( workspace.getSceneIndex().nNodeCount == 5 );
	assert( workspace.getPropertyIndex().nCount == 15 );

	const uint64_t nVBox = workspace.getSceneIndex().storage.pNodes[1].node.nId;
	const uint64_t nPluginButton = workspace.getSceneIndex().storage.pNodes[2].node.nId;
	const uint32_t nVBoxType = workspace.getSceneIndex().storage.pNodes[1].node.nTypeId;
	const uint32_t nButtonType = workspace.getSceneIndex().storage.pNodes[2].node.nTypeId;
	assert( nVBoxType != nButtonType && nVBox != nPluginButton );

	sirObjectTypeDescriptor_t szTypes[2] = {};
	sirObjectPropertyDescriptor_t szDescriptors[6] = {};
	SIRObjectRegistry registry;
	assert( registry.initialize( szTypes, 2, szDescriptors, 6 ) == SIR_OBJECT_OK );
	assert( registry.registerType( { nVBoxType, 0, 201 } ) == SIR_OBJECT_OK );
	assert( registry.registerType( { nButtonType, 0, 202 } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 1, nVBoxType, 101, SIR_PROPERTY_REAL64, {} } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 2, nVBoxType, 102, SIR_PROPERTY_REAL64, {} } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 3, nVBoxType, 103, SIR_PROPERTY_REAL64, {} } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 4, nVBoxType, 104, SIR_PROPERTY_REAL64, {} } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 5, nButtonType, 105, SIR_PROPERTY_INT64, {} } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 6, nVBoxType, 106, SIR_PROPERTY_INT64, {} } ) == SIR_OBJECT_OK );
	assert( registry.seal() == SIR_OBJECT_OK );

	sirEditorScalarBinding_t szBindings[16] = {};
	uint32_t nBindings = 0;
	sirTestRoundtripImport( workspace, registry, szBindings, &nBindings );

	sirPropertyValue_t value = {};
	assert( workspace.getDocument().findProperty( nVBox, 1, &value ) == SIR_EDITOR_OK );
	double fOffset = 0;
	memcpy( &fOffset, &value.nPayload, sizeof( fOffset ) );
	assert( fOffset == 68.0 );
	assert( workspace.getDocument().findProperty( nPluginButton, 5, &value ) == SIR_EDITOR_OK && value.nPayload == 2 );

	assert( workspace.getDocument().beginEdit( workspace.getDocument().getRevision() ) == SIR_EDITOR_OK );
	sirEditorProperty_t offset = {};
	offset.nNodeId = nVBox;
	offset.nPropertyId = 1;
	offset.value.eKind = SIR_PROPERTY_REAL64;
	const double fUpdated = 77.5;
	memcpy( &offset.value.nPayload, &fUpdated, sizeof( fUpdated ) );
	assert( workspace.getDocument().queueSetProperty( offset ) == SIR_EDITOR_OK );

	sirEditorProperty_t layout = {};
	layout.nNodeId = nPluginButton;
	layout.nPropertyId = 5;
	layout.value.eKind = SIR_PROPERTY_INT64;
	layout.value.nPayload = 3;
	assert( workspace.getDocument().queueSetProperty( layout ) == SIR_EDITOR_OK );
	assert( workspace.getDocument().commitEdit() == SIR_EDITOR_OK );

	sirEditorTextEdit_t szEdits[16] = {};
	char szEncoded[16 * 128] = {};
	sirEditorScalarBridge_t bridge = {};
	bridge.pDocument = &workspace.getDocument();
	bridge.pRegistry = &registry;
	bridge.pIndex = &workspace.getPropertyIndex();
	bridge.pszSource = workspace.getSource();
	bridge.nSourceBytes = workspace.getSourceBytes();
	bridge.pBindings = szBindings;
	bridge.nBindingCount = nBindings;
	bridge.pEdits = szEdits;
	bridge.pszEncodedScalars = szEncoded;
	bridge.nEncodedScalarBytes = sizeof( szEncoded );
	size_t nOutputBytes = 0;
	assert( sirEditorExportBoundScalars( bridge, nullptr, 0, &nOutputBytes ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
	char szOutput[8192] = {};
	assert( nOutputBytes < sizeof( szOutput ) );
	assert( sirEditorExportBoundScalars( bridge, szOutput, sizeof( szOutput ), &nOutputBytes ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
	assert( strstr( szOutput, "offset_left = 77.5" ) != nullptr );
	assert( strstr( szOutput, "text = \"Plugin Toast\n\"" ) != nullptr );
	assert( strstr( szOutput, "custom_minimum_size = Vector2(0, 50)" ) != nullptr );
	assert( strstr( szOutput, "[connection signal=\"pressed\"" ) != nullptr );
	assert( strstr( szOutput, "ExtResource(\"1_j0gfq\")" ) != nullptr );

	// Persist the edited native document to an actual sibling-staged file,
	// validate the exact bytes, then reopen from disk instead of a memory span.
	char szSavedPath[128] = {};
	char szTemporaryPath[128] = {};
#if defined( _WIN32 )
	const unsigned long nProcessId = ( unsigned long )_getpid();
#else
	const unsigned long nProcessId = ( unsigned long )getpid();
#endif
	assert( snprintf( szSavedPath, sizeof( szSavedPath ), "sir-editor-roundtrip-%lu.tscn", nProcessId ) > 0 );
	assert( snprintf( szTemporaryPath, sizeof( szTemporaryPath ), "sir-editor-roundtrip-%lu.tscn.stage", nProcessId ) > 0 );
	sirEditorSceneSaveRequest_t save = {};
	save.pBridge = &bridge;
	save.pszDestination = szSavedPath;
	save.pszTemporary = szTemporaryPath;
	save.pszScratch = szOutput;
	save.nScratchCapacity = sizeof( szOutput );
	assert( sirEditorSaveBoundScene( save ) == SIR_EDITOR_FILE_SAVE_OK );
	FILE *pSaved = fopen( szSavedPath, "rb" );
	assert( pSaved != nullptr );
	char szReadBack[8192] = {};
	assert( fread( szReadBack, 1, nOutputBytes, pSaved ) == nOutputBytes );
	assert( fgetc( pSaved ) == EOF );
	assert( fclose( pSaved ) == 0 );
	assert( memcmp( szReadBack, szOutput, nOutputBytes ) == 0 );

	// A failed export cannot replace the committed scene file.
	char szTooSmall[2] = {};
	save.pszScratch = szTooSmall;
	save.nScratchCapacity = sizeof( szTooSmall );
	assert( sirEditorSaveBoundScene( save ) == SIR_EDITOR_FILE_SAVE_OUT_OF_CAPACITY );
	save.pszScratch = szOutput;
	save.nScratchCapacity = sizeof( szOutput );
	assert( szReadBack[0] != '\0' );
	pSaved = fopen( szSavedPath, "rb" );
	assert( pSaved != nullptr );
	char szPreserved[8192] = {};
	assert( fread( szPreserved, 1, nOutputBytes, pSaved ) == nOutputBytes );
	assert( fclose( pSaved ) == 0 );
	assert( memcmp( szPreserved, szReadBack, nOutputBytes ) == 0 );

	const uint32_t iOriginalProperty = szBindings[0].iSourceProperty;
	szBindings[0].iSourceProperty = UINT32_MAX;
	assert( sirEditorSaveBoundScene( save ) == SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED );
	szBindings[0].iSourceProperty = iOriginalProperty;
	pSaved = fopen( szSavedPath, "rb" );
	assert( pSaved != nullptr );
	assert( fread( szPreserved, 1, nOutputBytes, pSaved ) == nOutputBytes );
	assert( fclose( pSaved ) == 0 );
	assert( memcmp( szPreserved, szReadBack, nOutputBytes ) == 0 );

	assert( workspace.getDocument().undo() == SIR_EDITOR_OK );
	assert( workspace.getDocument().findProperty( nVBox, 1, &value ) == SIR_EDITOR_OK );
	memcpy( &fOffset, &value.nPayload, sizeof( fOffset ) );
	assert( fOffset == 68.0 );
	assert( workspace.getDocument().redo() == SIR_EDITOR_OK );

	workspace.close();
	assert( workspace.openTextFile( szSavedPath ) == SIR_EDITOR_WORKSPACE_OK );
	assert( workspace.getSceneIndex().nNodeCount == 5 );
	assert( workspace.getResourceIndex().nCount == 1 );
	assert( workspace.getPropertyIndex().nCount == 15 );
	sirEditorScalarBinding_t szReopenedBindings[16] = {};
	sirTestRoundtripImport( workspace, registry, szReopenedBindings, &nBindings );
	assert( workspace.getDocument().findProperty( nVBox, 1, &value ) == SIR_EDITOR_OK );
	memcpy( &fOffset, &value.nPayload, sizeof( fOffset ) );
	assert( fOffset == 77.5 );
	assert( workspace.getDocument().findProperty( nPluginButton, 5, &value ) == SIR_EDITOR_OK && value.nPayload == 3 );

	workspace.close();
	assert( remove( szSavedPath ) == 0 );
	assert( fopen( szTemporaryPath, "rb" ) == nullptr );
	assert( workspace.shutdown() );
	registry.shutdown();
	memory.shutdown();
	return 0;
}
