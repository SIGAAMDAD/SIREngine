#include <Editor/Host/SIREditorWorkflow.h>
#include <assert.h>
#include <stdio.h>

int main( void )
{
	sirEditorNode_t nodes[8] = {}, staging[8] = {};
	sirEditorMutation_t pending[8] = {};
	sirEditorDelta_t pendingDeltas[8] = {}, historyDeltas[64] = {};
	sirEditorHistoryFrame_t frames[16] = {};
	sirEditorIndexEntry_t index[16] = {};
	sirEditorProperty_t properties[8] = {}, propertyStaging[8] = {};
	sirEditorPropertyMutation_t pendingProperties[8] = {};
	sirEditorPropertyDelta_t pendingPropertyDeltas[16] = {}, historyPropertyDeltas[64] = {};
	sirEditorDocumentStorage_t storage = {};
	storage.pNodes = nodes;
	storage.pStaging = staging;
	storage.pPending = pending;
	storage.pPendingDeltas = pendingDeltas;
	storage.pHistoryDeltas = historyDeltas;
	storage.pHistoryFrames = frames;
	storage.pIndex = index;
	storage.nNodeCapacity = 8;
	storage.nPendingCapacity = 8;
	storage.nDeltaCapacity = 64;
	storage.nFrameCapacity = 16;
	storage.nIndexCapacity = 16;
	storage.pProperties = properties;
	storage.pPropertyStaging = propertyStaging;
	storage.pPendingProperties = pendingProperties;
	storage.pPendingPropertyDeltas = pendingPropertyDeltas;
	storage.pHistoryPropertyDeltas = historyPropertyDeltas;
	storage.nPropertyCapacity = 8;
	storage.nPendingPropertyCapacity = 8;
	storage.nPropertyDeltaCapacity = 64;
	SIREditorSceneDocument document;
	assert( document.initialize( storage ) == SIR_EDITOR_OK );
	sirObjectTypeDescriptor_t types[2] = {};
	sirObjectPropertyDescriptor_t descriptors[2] = {};
	SIRObjectRegistry registry;
	assert( registry.initialize( types, 2, descriptors, 2 ) == SIR_OBJECT_OK );
	assert( registry.registerType( { 1, 0, 11 } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 5, 1, 55, SIR_PROPERTY_UINT64, {} } ) == SIR_OBJECT_OK );
	assert( registry.seal() == SIR_OBJECT_OK );
	assert( document.setPropertyRegistry( &registry ) == SIR_EDITOR_OK );
	SIREditorWorkflow workflow;
	assert( workflow.initialize( &document, nullptr ) == SIR_EDITOR_OK );
	assert( workflow.removeSelected() == SIR_EDITOR_NOT_FOUND );
	assert( workflow.createNode( { 1, 0, 1, 0, 0, 0 } ) == SIR_EDITOR_OK );
	assert( workflow.getSelectedId() == 1 );
	assert( workflow.createNode( { 2, 1, 1, 0, 0, 0 } ) == SIR_EDITOR_OK );
	assert( workflow.removeSelected() == SIR_EDITOR_OK );
	assert( workflow.getSelectedId() == 0 );
	sirEditorNode_t found = {};
	assert( document.findNode( 2, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( workflow.undo() == SIR_EDITOR_OK );
	assert( document.findNode( 2, &found ) == SIR_EDITOR_OK );
	assert( workflow.redo() == SIR_EDITOR_OK );
	assert( document.findNode( 2, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( workflow.select( 7 ) == SIR_EDITOR_NOT_FOUND );
	assert( workflow.select( 1 ) == SIR_EDITOR_OK );
	sirPropertyValue_t value = {};
	value.eKind = SIR_PROPERTY_UINT64;
	value.nPayload = 123;
	assert( workflow.setSelectedProperty( 5, value ) == SIR_EDITOR_OK );
	sirPropertyValue_t actual = {};
	assert( document.findProperty( 1, 5, &actual ) == SIR_EDITOR_OK && actual.nPayload == 123 );
	assert( workflow.undo() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 5, &actual ) == SIR_EDITOR_NOT_FOUND );
	assert( workflow.redo() == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 5, &actual ) == SIR_EDITOR_OK );
	assert( workflow.removeSelectedProperty( 5 ) == SIR_EDITOR_OK );
	assert( document.findProperty( 1, 5, &actual ) == SIR_EDITOR_NOT_FOUND );
	workflow.shutdown();
	document.shutdown();
	puts( "PASS: native standard editor workflow" );
	return 0;
}
