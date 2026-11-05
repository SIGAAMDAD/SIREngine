#include <Editor/Host/SIREditorSceneWorkspace.h>

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void sirTestSceneWorkspace( void )
{
	constexpr uint64_t N_MIB = UINT64_C( 1024 ) * 1024;
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 128 * N_MIB, 64 * N_MIB, 2 * N_MIB, 2 };
	assert( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "EditorTest", 96 * N_MIB, 56 * N_MIB, SIR_MEMORY_DOMAIN_EDITOR );
	assert( pSegment != nullptr );

	SIREditorSceneWorkspace workspace;
	assert( workspace.openTextSource( nullptr, 4 ) == SIR_EDITOR_WORKSPACE_NOT_INITIALIZED );
	assert( workspace.initialize( pSegment ) );
	assert( !workspace.initialize( pSegment ) );
	assert( workspace.openTextSource( nullptr, 3 ) == SIR_EDITOR_WORKSPACE_INVALID_ARGUMENT );
	assert( workspace.openTextSource( "", 0 ) == SIR_EDITOR_WORKSPACE_INVALID_ARGUMENT );

	const char szInvalidScene[] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n"
	                              "[node name=\"Orphan\" type=\"Node\" parent=\"Missing\"]\n";
	assert( workspace.openTextSource( szInvalidScene, sizeof( szInvalidScene ) - 1 ) == SIR_EDITOR_WORKSPACE_INVALID_SCENE );
	assert( !workspace.isOpen() );
	assert( workspace.getSource() == nullptr );
	assert( workspace.getErrorLine() > 0 );

	const char szInvalidResource[] = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n"
	                                 "script = ExtResource(\"absent\")\n";
	assert( workspace.openTextSource( szInvalidResource, sizeof( szInvalidResource ) - 1 ) == SIR_EDITOR_WORKSPACE_INVALID_RESOURCES );
	assert( !workspace.isOpen() );
	assert( workspace.getErrorLine() > 0 );

	const char szValid[] = "[gd_scene format=3]\n"
	                       "[ext_resource type=\"Script\" path=\"res://script.gd\" id=\"1_a\"]\n"
	                       "[node name=\"Root\" type=\"Node\" unique_id=1]\n"
	                       "script = ExtResource(\"1_a\")\n"
	                       "[node name=\"Child\" type=\"Node2D\" parent=\".\" unique_id=2]\n";
	assert( workspace.openTextSource( szValid, sizeof( szValid ) - 1 ) == SIR_EDITOR_WORKSPACE_OK );
	assert( workspace.isOpen() );
	assert( workspace.getSourceBytes() == sizeof( szValid ) - 1 );
	assert( workspace.getSceneIndex().nNodeCount == 2 );
	assert( workspace.getResourceIndex().nCount == 1 );
	assert( workspace.getHierarchy().getVisibleCount() == 2 );
	assert( workspace.getDocument().getRevision() == 2 );
	assert( workspace.getSceneIndex().storage.pNodes[1].node.nParentId == workspace.getSceneIndex().storage.pNodes[0].node.nId );
	assert( workspace.openTextSource( szValid, sizeof( szValid ) - 1 ) == SIR_EDITOR_WORKSPACE_ALREADY_OPEN );

	const uint64_t nRoot = workspace.getSceneIndex().storage.pNodes[0].node.nId;
	const uint64_t nChild = workspace.getSceneIndex().storage.pNodes[1].node.nId;
	assert( workspace.getHierarchy().setSelected( workspace.getDocument(), nChild ) == SIR_EDITOR_HIERARCHY_OK );
	assert( workspace.getHierarchy().getSelectedId() == nChild );
	assert( workspace.getHierarchy().setExpanded( workspace.getDocument(), nRoot, false ) == SIR_EDITOR_HIERARCHY_OK );
	assert( workspace.getHierarchy().getVisibleCount() == 1 );

	// Source-backed editor nodes now have native typed property journal storage.
	sirObjectTypeDescriptor_t szTypes[1] = {};
	sirObjectPropertyDescriptor_t szDescriptors[1] = {};
	SIRObjectRegistry registry;
	assert( registry.initialize( szTypes, 1, szDescriptors, 1 ) == SIR_OBJECT_OK );
	sirEditorNode_t root = {};
	assert( workspace.getDocument().findNode( nRoot, &root ) == SIR_EDITOR_OK );
	assert( registry.registerType( { root.nTypeId, 0, 11 } ) == SIR_OBJECT_OK );
	assert( registry.registerProperty( { 17, root.nTypeId, 17, SIR_PROPERTY_UINT64, {} } ) == SIR_OBJECT_OK );
	assert( registry.seal() == SIR_OBJECT_OK );
	assert( workspace.getDocument().setPropertyRegistry( &registry ) == SIR_EDITOR_OK );
	sirEditorProperty_t property = {};
	property.nNodeId = nRoot;
	property.nPropertyId = 17;
	property.value.eKind = SIR_PROPERTY_UINT64;
	property.value.nPayload = 123;
	assert( workspace.getDocument().beginEdit( workspace.getDocument().getRevision() ) == SIR_EDITOR_OK );
	assert( workspace.getDocument().queueSetProperty( property ) == SIR_EDITOR_OK );
	assert( workspace.getDocument().commitEdit() == SIR_EDITOR_OK );
	sirPropertyValue_t found = {};
	assert( workspace.getDocument().findProperty( nRoot, 17, &found ) == SIR_EDITOR_OK );
	assert( found.nPayload == 123 );
	assert( workspace.getDocument().undo() == SIR_EDITOR_OK );
	assert( workspace.getDocument().findProperty( nRoot, 17, &found ) == SIR_EDITOR_NOT_FOUND );
	assert( workspace.getDocument().redo() == SIR_EDITOR_OK );
	assert( workspace.getDocument().findProperty( nRoot, 17, &found ) == SIR_EDITOR_OK );
	workspace.close();
	registry.shutdown();
	assert( !workspace.isOpen() );
	assert( workspace.getSource() == nullptr );
	assert( workspace.getResourceIndex().nCount == 0 );
	assert( workspace.getHierarchy().getVisibleCount() == 0 );

	assert( workspace.openTextFile( SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_WORKSPACE_OK );
	assert( workspace.getSceneIndex().nNodeCount == 5 );
	assert( workspace.getResourceIndex().nCount == 1 );
	assert( workspace.getHierarchy().getVisibleCount() == 2 );
	assert( workspace.getErrorLine() == 0 );
	assert( workspace.getSourceBytes() > 0 );
	assert( workspace.getSource()[workspace.getSourceBytes()] == '\0' );
	assert( workspace.openTextFile( SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_WORKSPACE_ALREADY_OPEN );
	workspace.close();

	// Platform reader rejects missing, malformed and nonregular scene sources.
	assert( workspace.openTextFile( nullptr ) == SIR_EDITOR_WORKSPACE_INVALID_ARGUMENT );
	assert( workspace.openTextFile( "" ) == SIR_EDITOR_WORKSPACE_INVALID_ARGUMENT );
	assert( workspace.openTextFile( "." ) == SIR_EDITOR_WORKSPACE_FILE_IO );
	assert( !workspace.isOpen() );
	assert( workspace.getSource() == nullptr );
	assert( workspace.openTextFile( "SIREditor/nonexistent_scene_9802.tscn" ) == SIR_EDITOR_WORKSPACE_FILE_IO );
	assert( !workspace.isOpen() );
	assert( workspace.getSourceBytes() == 0 );
	assert( workspace.getDocument().getRevision() == 0 );
	assert( workspace.openTextFile( SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_WORKSPACE_OK );
	workspace.close();

	assert( workspace.shutdown() );
	assert( workspace.shutdown() );
	memory.shutdown();
}

int main( void )
{
	sirTestSceneWorkspace();
	puts( "PASS: editor scene workspace integration" );
	return 0;
}
