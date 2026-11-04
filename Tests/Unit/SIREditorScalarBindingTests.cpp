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

#include <Editor/Scene/SIREditorScalarBinding.h>

#include <assert.h>
#include <string.h>

static bool sirEditorTestAtomMatches( uint64_t nAtom, const char *pszSource, uint32_t nBytes, void *pContext ) noexcept
{
	( void )pContext;
	if ( nAtom == 107 ) {
		return nBytes == 3 && memcmp( pszSource, "foo", 3 ) == 0;
	}
	if ( nAtom == 108 ) {
		return nBytes == 3 && memcmp( pszSource, "bar", 3 ) == 0;
	}
	return false;
}

int main( void )
{
	sirObjectTypeDescriptor_t szTypes[1] = {};
	sirObjectPropertyDescriptor_t szDescriptors[2] = {};
	SIRObjectRegistry registry;
	assert( registry.initialize( szTypes, 1, szDescriptors, 2 ) == SIR_OBJECT_OK );
	assert( registry.registerType( { 1, 0, 101 } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 7, 1, 107, SIR_PROPERTY_UINT64, {} } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 8, 1, 108, SIR_PROPERTY_BOOL, {} } ) == SIR_OBJECT_OK );
	assert( registry.seal() == SIR_OBJECT_OK );

	sirEditorNode_t szNodes[4] = {};
	sirEditorNode_t szStaging[4] = {};
	sirEditorMutation_t szPending[4] = {};
	sirEditorDelta_t szPendingDeltas[4] = {};
	sirEditorDelta_t szHistoryDeltas[16] = {};
	sirEditorHistoryFrame_t szFrames[8] = {};
	sirEditorIndexEntry_t szIndex[8] = {};
	sirEditorProperty_t szProperties[4] = {};
	sirEditorProperty_t szPropertyStaging[4] = {};
	sirEditorPropertyMutation_t szPendingProperties[4] = {};
	sirEditorPropertyDelta_t szPendingPropertyDeltas[8] = {};
	sirEditorPropertyDelta_t szHistoryPropertyDeltas[32] = {};
	sirEditorDocumentStorage_t storage = {};
	storage.pNodes = szNodes;
	storage.pStaging = szStaging;
	storage.pPending = szPending;
	storage.pPendingDeltas = szPendingDeltas;
	storage.pHistoryDeltas = szHistoryDeltas;
	storage.pHistoryFrames = szFrames;
	storage.pIndex = szIndex;
	storage.nNodeCapacity = 4;
	storage.nPendingCapacity = 4;
	storage.nDeltaCapacity = 16;
	storage.nFrameCapacity = 8;
	storage.nIndexCapacity = 8;
	storage.pProperties = szProperties;
	storage.pPropertyStaging = szPropertyStaging;
	storage.pPendingProperties = szPendingProperties;
	storage.pPendingPropertyDeltas = szPendingPropertyDeltas;
	storage.pHistoryPropertyDeltas = szHistoryPropertyDeltas;
	storage.nPropertyCapacity = 4;
	storage.nPendingPropertyCapacity = 4;
	storage.nPropertyDeltaCapacity = 32;

	SIREditorSceneDocument document;
	assert( document.initialize( storage ) == SIR_EDITOR_OK );
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	sirEditorNode_t node = {};
	node.nId = 1;
	node.nTypeId = 1;
	assert( document.queueUpsert( node ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );

	const char *pszSource = "# scene header\n[node name=\"Root\" type=\"Node\"]\nfoo = 12\nbar = false\n";
	const char *pszFoo = strstr( pszSource, "12" );
	const char *pszBar = strstr( pszSource, "false" );
	assert( pszFoo != nullptr && pszBar != nullptr );
	sirEditorTextProperty_t szText[2] = {};
	szText[0].nNodeId = 1;
	szText[0].nNameOffset = ( uint32_t )( strstr( pszSource, "foo =" ) - pszSource );
	szText[0].nNameLength = 3;
	szText[0].nValueOffset = ( uint32_t )( pszFoo - pszSource );
	szText[0].nValueLength = 2;
	szText[1].nNodeId = 1;
	szText[1].nNameOffset = ( uint32_t )( strstr( pszSource, "bar =" ) - pszSource );
	szText[1].nNameLength = 3;
	szText[1].nValueOffset = ( uint32_t )( pszBar - pszSource );
	szText[1].nValueLength = 5;
	sirEditorTextPropertyIndex_t index = { szText, 2, 2, 0 };
	sirEditorScalarBinding_t szBindings[2] = {};
	sirEditorScalarDiscovery_t discovery = {};
	discovery.pDocument = &document;
	discovery.pRegistry = &registry;
	discovery.pIndex = &index;
	discovery.pszSource = pszSource;
	discovery.nSourceBytes = strlen( pszSource );
	discovery.pMatchesAtom = sirEditorTestAtomMatches;
	uint32_t nDiscovered = 0;
	assert( sirEditorDiscoverScalarBindings( discovery, &nDiscovered ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
	assert( nDiscovered == 2 );
	discovery.pOutput = szBindings;
	discovery.nOutputCapacity = 1;
	szBindings[0] = { 999, 999, 999 };
	assert( sirEditorDiscoverScalarBindings( discovery, &nDiscovered ) == SIR_EDITOR_SCALAR_BRIDGE_OUT_OF_CAPACITY );
	assert( nDiscovered == 2 && szBindings[0].nNodeId == 999 );
	discovery.nOutputCapacity = 2;
	assert( sirEditorDiscoverScalarBindings( discovery, &nDiscovered ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
	assert( nDiscovered == 2 );
	assert( szBindings[0].nNodeId == 1 && szBindings[0].nPropertyId == 7 && szBindings[0].iSourceProperty == 0 );
	assert( szBindings[1].nNodeId == 1 && szBindings[1].nPropertyId == 8 && szBindings[1].iSourceProperty == 1 );

	// Duplicate name on the same authored node is invalid and publishes no
	// partial output. Restoring the source span permits discovery again.
	const uint32_t nBarName = szText[1].nNameOffset;
	szText[1].nNameOffset = szText[0].nNameOffset;
	assert( sirEditorDiscoverScalarBindings( discovery, &nDiscovered ) == SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING );
	assert( nDiscovered == 0 );
	szText[1].nNameOffset = nBarName;
	sirEditorProperty_t szStaged[2] = {};
	sirEditorTextEdit_t szEdits[2] = {};
	char szValues[256] = {};
	sirEditorScalarBridge_t context = {};
	context.pDocument = &document;
	context.pRegistry = &registry;
	context.pIndex = &index;
	context.pszSource = pszSource;
	context.nSourceBytes = strlen( pszSource );
	context.pBindings = szBindings;
	context.nBindingCount = 2;
	context.pScratch = szStaged;
	context.pEdits = szEdits;
	context.pszEncodedScalars = szValues;
	context.nEncodedScalarBytes = sizeof( szValues );

	assert( sirEditorImportBoundScalars( context ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
	sirPropertyValue_t found = {};
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 12 );
	assert( document.findProperty( 1, 8, &found ) == SIR_EDITOR_OK && found.nPayload == 0 );

	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	sirEditorProperty_t edit = {};
	edit.nNodeId = 1;
	edit.nPropertyId = 7;
	edit.value.eKind = SIR_PROPERTY_UINT64;
	edit.value.nPayload = 85;
	assert( document.queueSetProperty( edit ) == SIR_EDITOR_OK );
	edit.nPropertyId = 8;
	edit.value.eKind = SIR_PROPERTY_BOOL;
	edit.value.nPayload = 1;
	assert( document.queueSetProperty( edit ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );

	size_t nWritten = 0;
	assert( sirEditorExportBoundScalars( context, nullptr, 0, &nWritten ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
	char szOutput[512] = {};
	assert( nWritten < sizeof( szOutput ) );
	assert( sirEditorExportBoundScalars( context, szOutput, sizeof( szOutput ), &nWritten ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
	assert( strstr( szOutput, "foo = 85\n" ) != nullptr );
	assert( strstr( szOutput, "bar = true\n" ) != nullptr );
	assert( strstr( szOutput, "# scene header\n" ) == szOutput );
	assert( strstr( szOutput, "[node name=\"Root\" type=\"Node\"]\n" ) != nullptr );

	char szGuard[16] = {};
	memset( szGuard, 0x66, sizeof( szGuard ) );
	assert( sirEditorExportBoundScalars( context, szGuard, sizeof( szGuard ), &nWritten ) ==
	        SIR_EDITOR_SCALAR_BRIDGE_WRITE_ERROR );
	assert( szGuard[0] == 0x66 );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 12 );
	assert( document.findProperty( 1, 8, &found ) == SIR_EDITOR_OK && found.nPayload == 0 );
	assert( sirEditorExportBoundScalars( context, szOutput, sizeof( szOutput ), &nWritten ) == SIR_EDITOR_SCALAR_BRIDGE_OK );
	assert( strcmp( pszSource, szOutput ) == 0 );
	assert( document.redo() == SIR_EDITOR_OK );

	const uint64_t nRevision = document.getRevision();
	szBindings[0].iSourceProperty = 99;
	assert( sirEditorImportBoundScalars( context ) == SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING );
	assert( document.getRevision() == nRevision );
	szBindings[0].iSourceProperty = 0;
	assert( sirEditorExportBoundScalars( context, szValues, sizeof( szValues ), &nWritten ) ==
	        SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT );
	document.shutdown();
	registry.shutdown();
	return 0;
}
