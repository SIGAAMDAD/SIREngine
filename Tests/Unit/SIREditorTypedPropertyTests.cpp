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

#include <Editor/Scene/SIREditorSceneDocument.h>

#include <assert.h>
#include <string.h>

static sirEditorNode_t sirTypedTestNode( uint64_t nId, uint32_t nTypeId ) noexcept
{
	sirEditorNode_t node = {};
	node.nId = nId;
	node.nTypeId = nTypeId;
	return node;
}

static sirEditorProperty_t sirTypedTestProperty( uint64_t nNodeId, uint32_t nPropertyId, uint64_t nValue ) noexcept
{
	sirEditorProperty_t property = {};
	property.nNodeId = nNodeId;
	property.nPropertyId = nPropertyId;
	property.value.eKind = SIR_PROPERTY_UINT64;
	property.value.nPayload = nValue;
	return property;
}

int main( void )
{
	sirObjectTypeDescriptor_t szTypes[2] = {};
	sirObjectPropertyDescriptor_t szDescriptors[2] = {};
	SIRObjectRegistry registry;
	assert( registry.initialize( szTypes, 2, szDescriptors, 2 ) == SIR_OBJECT_OK );
	assert( registry.registerType( { 1, 0, 11 } ) == SIR_OBJECT_OK );
	assert( registry.registerType( { 2, 0, 22 } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 7, 1, 77, SIR_PROPERTY_UINT64, {} } ) == SIR_OBJECT_OK );
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
	assert( document.setPropertyRegistry( nullptr ) == SIR_EDITOR_INVALID_ARGUMENT );
	assert( document.queueSetProperty( sirTypedTestProperty( 1, 7, 1 ) ) == SIR_EDITOR_NO_TRANSACTION );
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTypedTestNode( 1, 1 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.setPropertyRegistry( &registry ) == SIR_EDITOR_OK );

	sirPropertyValue_t found = {};
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueSetProperty( sirTypedTestProperty( 1, 7, 10 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 10 );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( document.redo() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 10 );

	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueSetProperty( sirTypedTestProperty( 1, 7, 20 ) ) == SIR_EDITOR_OK );
	assert( document.queueSetProperty( sirTypedTestProperty( 1, 7, 30 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 30 );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 10 );
	assert( document.redo() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 30 );

	const uint64_t nBefore = document.getRevision();
	assert( document.beginEdit( nBefore ) == SIR_EDITOR_OK );
	assert( document.queueSetProperty( sirTypedTestProperty( 9, 7, 1 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_NOT_FOUND );
	assert( document.getRevision() == nBefore );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 30 );
	assert( document.cancelEdit() == SIR_EDITOR_OK );

	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	sirEditorProperty_t invalid = sirTypedTestProperty( 1, 7, 44 );
	invalid.value.eKind = SIR_PROPERTY_BOOL;
	assert( document.queueSetProperty( invalid ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_INVALID_PROPERTY );
	assert( document.cancelEdit() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 30 );

	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTypedTestNode( 1, 2 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_INVALID_PROPERTY );
	assert( document.cancelEdit() == SIR_EDITOR_OK );
	sirEditorNode_t foundNode = {};
	assert( document.findNode( 1, &foundNode ) == SIR_EDITOR_OK && foundNode.nTypeId == 1 );

	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueRemove( 1 ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.findNode( 1, &foundNode ) == SIR_EDITOR_NOT_FOUND );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.findNode( 1, &foundNode ) == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 30 );
	assert( document.redo() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 7, &found ) == SIR_EDITOR_NOT_FOUND );

	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTypedTestNode( 2, 1 ) ) == SIR_EDITOR_OK );
	assert( document.queueSetProperty( sirTypedTestProperty( 2, 7, 99 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 99 );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.findNode( 2, &foundNode ) == SIR_EDITOR_NOT_FOUND );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( document.redo() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 99 );

	uint32_t nProperties = 0;
	sirEditorProperty_t szSnapshot[4] = {};
	assert( document.copyPropertySnapshot( nullptr, 0, &nProperties ) == SIR_EDITOR_OUT_OF_CAPACITY );
	assert( nProperties == 1 );
	assert( document.copyPropertySnapshot( szSnapshot, 4, &nProperties ) == SIR_EDITOR_OK );
	assert( nProperties == 1 && szSnapshot[0].value.nPayload == 99 );

	// Reusing an authored ID with a new generation must not retain old
	// properties unless the new incarnation explicitly assigns them.
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueRemove( 2 ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTypedTestNode( 2, 1 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.findNode( 2, &foundNode ) == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 99 );
	assert( document.redo() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_NOT_FOUND );

	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueRemove( 2 ) == SIR_EDITOR_OK );
	assert( document.queueUpsert( sirTypedTestNode( 2, 1 ) ) == SIR_EDITOR_OK );
	assert( document.queueSetProperty( sirTypedTestProperty( 2, 7, 456 ) ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 456 );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( document.redo() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 456 );

	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	assert( document.queueRemoveProperty( 2, 7 ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( document.undo() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_OK && found.nPayload == 456 );
	assert( document.redo() == SIR_EDITOR_OK );
	assert( document.findProperty( 2, 7, &found ) == SIR_EDITOR_NOT_FOUND );

	const uint64_t nRevisionAfterRemove = document.getRevision();
	assert( document.beginEdit( nRevisionAfterRemove - 1 ) == SIR_EDITOR_STALE_REVISION );
	assert( document.beginEdit( nRevisionAfterRemove ) == SIR_EDITOR_OK );
	assert( document.queueRemoveProperty( 2, 7 ) == SIR_EDITOR_OK );
	assert( document.commitEdit() == SIR_EDITOR_NOT_FOUND );
	assert( document.getRevision() == nRevisionAfterRemove );
	assert( document.cancelEdit() == SIR_EDITOR_OK );

	document.shutdown();
	registry.shutdown();
	return 0;
}
