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

#include <Editor/Bridge/SIREditorNativeBridge.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sirEditorTestNativeBridge( void )
{
	const size_t nRequired = sirEditorNativeBridgeStorageBytes();
	const size_t nAlignment = sirEditorNativeBridgeStorageAlignment();
	assert( nRequired > 0 && nAlignment > 0 );
	assert( ( nAlignment & ( nAlignment - 1 ) ) == 0 );
	unsigned char *pRaw = ( unsigned char * )malloc( nRequired + nAlignment );
	assert( pRaw != nullptr );
	const uintptr_t nAligned = ( ( uintptr_t )pRaw + nAlignment - 1 ) & ~( ( uintptr_t )nAlignment - 1 );
	void *pStorage = ( void * )nAligned;
	constexpr uint64_t MIB = 1024ULL * 1024ULL;
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 128 * MIB, 64 * MIB, 2 * MIB, 2 };
	assert( memory.init( config ) );
	SIRMemorySegment *pEditorSegment = memory.createSegment( "EditorABITest", 96 * MIB, 56 * MIB, SIR_MEMORY_DOMAIN_EDITOR );
	assert( pEditorSegment != nullptr );
	SIRMemoryManager wrongMemory;
	const sirMemoryConfig_t wrongConfig = { 32 * MIB, 16 * MIB, 2 * MIB, 2 };
	assert( wrongMemory.init( wrongConfig ) );
	SIRMemorySegment *pWrongDomain = wrongMemory.createSegment( "NotEditor", 4 * MIB, 2 * MIB, SIR_MEMORY_DOMAIN_CORE );
	assert( pWrongDomain != nullptr );
	sirEditorNativeBridgeHandle_t *pBridge = nullptr;
	assert( sirEditorNativeBridgeInit( pStorage, nRequired - 1, pEditorSegment, &pBridge ) == SIR_EDITOR_BRIDGE_INVALID_ARGUMENT );
	assert( pBridge == nullptr );
	assert( sirEditorNativeBridgeInit( pStorage, nRequired, nullptr, &pBridge ) == SIR_EDITOR_BRIDGE_INVALID_ARGUMENT );
	assert( pBridge == nullptr );
	assert( sirEditorNativeBridgeInit( pStorage, nRequired, pWrongDomain, &pBridge ) == SIR_EDITOR_BRIDGE_INVALID_ARGUMENT );
	assert( pBridge == nullptr );
	if ( nAlignment > 1 ) {
		assert( sirEditorNativeBridgeInit( ( void * )( nAligned + 1 ), nRequired, pEditorSegment, &pBridge ) ==
		        SIR_EDITOR_BRIDGE_INVALID_ARGUMENT );
		assert( pBridge == nullptr );
	}
	assert( sirEditorNativeBridgeInit( pStorage, nRequired, pEditorSegment, &pBridge ) == SIR_EDITOR_BRIDGE_OK );
	assert( pBridge != nullptr );

	sirEditorBridgeSnapshot_t snapshot = {};
	assert( sirEditorNativeBridgeSnapshot( pBridge, &snapshot ) == SIR_EDITOR_BRIDGE_NOT_OPEN );
	assert( sirEditorNativeBridgeOpenText( pBridge, "invalid scene", 13 ) == SIR_EDITOR_BRIDGE_SCENE_FAILURE );
	assert( sirEditorNativeBridgeOpenFile( pBridge, "/does/not/exist.tscn" ) == SIR_EDITOR_BRIDGE_SCENE_FAILURE );
	assert( sirEditorNativeBridgeOpenFile( pBridge, SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_BRIDGE_OK );
	assert( sirEditorNativeBridgeSnapshot( pBridge, &snapshot ) == SIR_EDITOR_BRIDGE_OK );
	assert( snapshot.nAbiVersion == SIR_EDITOR_BRIDGE_ABI_VERSION );
	assert( snapshot.nNodeCount == 5 );
	assert( snapshot.nResourceCount == 1 );
	assert( snapshot.nPropertyCount == 15 );
	assert( snapshot.nDocumentRevision > 0 );
	assert( snapshot.nGeneration > 1 );
	assert( sirEditorNativeBridgeOpenFile( pBridge, SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_BRIDGE_ALREADY_OPEN );

	bool bFoundPath = false;
	for ( uint32_t i = 0; i < snapshot.nNodeCount; ++i ) {
		sirEditorBridgeNode_t node = {};
		assert( sirEditorNativeBridgeNodeAt( pBridge, snapshot.nGeneration, i, &node ) == SIR_EDITOR_BRIDGE_OK );
		assert( node.nId != 0 && node.nSourceLine != 0 && node.nNameBytes != 0 );
		char szPath[256];
		size_t nPathRequired = 0;
		assert( sirEditorNativeBridgeCopyField( pBridge, snapshot.nGeneration, SIR_EDITOR_BRIDGE_NODE_PATH, i, nullptr, 0,
		                                        &nPathRequired ) == SIR_EDITOR_BRIDGE_OUT_OF_CAPACITY );
		assert( nPathRequired == ( size_t )node.nPathBytes + 1 );
		assert( nPathRequired <= sizeof( szPath ) );
		assert( sirEditorNativeBridgeCopyField( pBridge, snapshot.nGeneration, SIR_EDITOR_BRIDGE_NODE_PATH, i, szPath, sizeof( szPath ),
		                                        &nPathRequired ) == SIR_EDITOR_BRIDGE_OK );
		if ( strcmp( szPath, "VBoxContainer/PluginToastButton" ) == 0 ) {
			bFoundPath = true;
		}
	}
	assert( bFoundPath );
	sirEditorBridgeNode_t absentNode = {};
	assert( sirEditorNativeBridgeNodeAt( pBridge, snapshot.nGeneration, 99, &absentNode ) == SIR_EDITOR_BRIDGE_NOT_FOUND );
	sirEditorBridgeResource_t resource = {};
	assert( sirEditorNativeBridgeResourceAt( pBridge, snapshot.nGeneration, 0, &resource ) == SIR_EDITOR_BRIDGE_OK );
	assert( resource.nKind != 0 && resource.nStableKey != 0 );
	assert( resource.nTypeBytes > 0 );

	bool bFoundProperty = false;
	for ( uint32_t i = 0; i < snapshot.nPropertyCount; ++i ) {
		sirEditorBridgeProperty_t property = {};
		assert( sirEditorNativeBridgePropertyAt( pBridge, snapshot.nGeneration, i, &property ) == SIR_EDITOR_BRIDGE_OK );
		assert( property.nNodeId != 0 && property.nSourceLine > 0 && property.nNameBytes > 0 );
		char szName[128] = {};
		size_t nNeeded = 0;
		if ( property.nNameBytes >= sizeof( szName ) ) {
			continue;
		}
		assert( sirEditorNativeBridgeCopyField( pBridge, snapshot.nGeneration, SIR_EDITOR_BRIDGE_PROPERTY_NAME, i, szName, sizeof( szName ),
		                                        &nNeeded ) == SIR_EDITOR_BRIDGE_OK );
		if ( strcmp( szName, "text" ) == 0 ) {
			bFoundProperty = true;
		}
	}
	assert( bFoundProperty );

	char szSentinel[8] = "untouch";
	size_t nNeeded = 0;
	assert( sirEditorNativeBridgeCopyField( pBridge, snapshot.nGeneration, SIR_EDITOR_BRIDGE_NODE_NAME, 0, szSentinel, 1, &nNeeded ) ==
	        SIR_EDITOR_BRIDGE_OUT_OF_CAPACITY );
	assert( strcmp( szSentinel, "untouch" ) == 0 && nNeeded > 1 );
	assert( sirEditorNativeBridgeCopyField( pBridge, snapshot.nGeneration, ( sirEditorBridgeStringField_t )250, 0, szSentinel,
	                                        sizeof( szSentinel ), &nNeeded ) == SIR_EDITOR_BRIDGE_INVALID_ARGUMENT );

	const uint64_t nOldGeneration = snapshot.nGeneration;
	sirEditorNativeBridgeClose( pBridge );
	assert( sirEditorNativeBridgeNodeAt( pBridge, nOldGeneration, 0, &absentNode ) == SIR_EDITOR_BRIDGE_STALE );
	assert( sirEditorNativeBridgeSnapshot( pBridge, &snapshot ) == SIR_EDITOR_BRIDGE_NOT_OPEN );
	assert( sirEditorNativeBridgeOpenFile( pBridge, SIR_EDITOR_TEST_UI_SCENE ) == SIR_EDITOR_BRIDGE_OK );
	assert( sirEditorNativeBridgeSnapshot( pBridge, &snapshot ) == SIR_EDITOR_BRIDGE_OK );
	assert( snapshot.nGeneration != nOldGeneration && snapshot.nNodeCount == 5 );
	assert( sirEditorNativeBridgeNodeAt( pBridge, nOldGeneration, 0, &absentNode ) == SIR_EDITOR_BRIDGE_STALE );
	assert( sirEditorNativeBridgeNodeAt( pBridge, snapshot.nGeneration, 0, &absentNode ) == SIR_EDITOR_BRIDGE_OK );

	sirEditorNativeBridgeDestroy( pBridge );
	wrongMemory.shutdown();
	memory.shutdown();
	free( pRaw );
}

int main( void )
{
	sirEditorTestNativeBridge();
	puts( "PASS: in-process native editor bridge ABI, snapshot, strings and generation" );
	return 0;
}
