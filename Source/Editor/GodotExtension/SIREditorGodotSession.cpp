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

#include <Editor/GodotExtension/SIREditorGodotSession.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/char_string.hpp>

#include <stdio.h>
#include <string.h>

static constexpr uint64_t SIR_GODOT_EDITOR_MIB = UINT64_C( 1024 ) * 1024;
static constexpr uint32_t SIR_GODOT_EDITOR_MAX_NODES = 2048;
static constexpr uint32_t SIR_GODOT_EDITOR_MAX_PROPERTIES = 8192;
static constexpr size_t SIR_GODOT_EDITOR_MAX_FIELD = 8192;

void SIREditorGodotSession::_bind_methods( void )
{
	godot::ClassDB::bind_method( D_METHOD( "open_scene", "path" ), &SIREditorGodotSession::open_scene );
	godot::ClassDB::bind_method( D_METHOD( "close_scene" ), &SIREditorGodotSession::close_scene );
	godot::ClassDB::bind_method( D_METHOD( "get_snapshot" ), &SIREditorGodotSession::get_snapshot );
	godot::ClassDB::bind_method( D_METHOD( "get_last_error" ), &SIREditorGodotSession::get_last_error );
}

SIREditorGodotSession::~SIREditorGodotSession()
{
	if ( m_pBridge != nullptr ) {
		sirEditorNativeBridgeDestroy( m_pBridge );
		m_pBridge = nullptr;
	}
	if ( m_bMemoryInitialized ) {
		m_Memory.shutdown();
		m_bMemoryInitialized = false;
	}
}

bool SIREditorGodotSession::initializeBridge( void ) noexcept
{
	if ( m_pBridge != nullptr ) {
		return true;
	}
	if ( sirEditorNativeBridgeStorageBytes() > BRIDGE_STORAGE_CAPACITY || sirEditorNativeBridgeStorageAlignment() > 64 ) {
		return false;
	}

	const sirMemoryConfig_t config = { 128 * SIR_GODOT_EDITOR_MIB, 64 * SIR_GODOT_EDITOR_MIB, 2 * SIR_GODOT_EDITOR_MIB, 2 };
	if ( !m_Memory.init( config ) ) {
		return false;
	}
	m_bMemoryInitialized = true;
	m_pEditorSegment =
	    m_Memory.createSegment( "GodotEditorBridge", 96 * SIR_GODOT_EDITOR_MIB, 56 * SIR_GODOT_EDITOR_MIB, SIR_MEMORY_DOMAIN_EDITOR );
	if ( m_pEditorSegment == nullptr ) {
		m_Memory.shutdown();
		m_bMemoryInitialized = false;
		return false;
	}

	if ( sirEditorNativeBridgeInit( m_szBridgeStorage, sizeof( m_szBridgeStorage ), m_pEditorSegment, &m_pBridge ) !=
	     SIR_EDITOR_BRIDGE_OK ) {
		m_Memory.shutdown();
		m_bMemoryInitialized = false;
		return false;
	}
	return true;
}

bool SIREditorGodotSession::open_scene( const godot::String &sPath )
{
	m_sLastError = godot::String();
	close_scene();
	if ( sPath.is_empty() || !initializeBridge() ) {
		m_sLastError = "Editor memory initialization failed or empty scene path";
		return false;
	}

	const godot::CharString szPath = sPath.utf8();
	const sirEditorBridgeResult_t eResult = sirEditorNativeBridgeOpenFile( m_pBridge, szPath.get_data() );
	if ( eResult != SIR_EDITOR_BRIDGE_OK ) {
		m_sLastError = godot::String( "Native scene parse failed (code " ) + formatId( ( uint64_t )eResult ) + ")";
		return false;
	}
	m_sScenePath = sPath;
	return true;
}

void SIREditorGodotSession::close_scene( void ) noexcept
{
	if ( m_pBridge != nullptr ) {
		sirEditorNativeBridgeClose( m_pBridge );
	}
	m_sScenePath = godot::String();
}

godot::String SIREditorGodotSession::formatId( uint64_t nValue )
{
	char szDigits[32] = {};
	snprintf( szDigits, sizeof( szDigits ), "%llu", ( unsigned long long )nValue );
	return godot::String( szDigits );
}

godot::String SIREditorGodotSession::copyField( uint64_t nGeneration, sirEditorBridgeStringField_t eField, uint32_t iRecord,
                                                bool *pValid ) const
{
	*pValid = false;
	char szField[SIR_GODOT_EDITOR_MAX_FIELD];
	size_t nRequired = 0;
	const sirEditorBridgeResult_t eMeasure =
	    sirEditorNativeBridgeCopyField( m_pBridge, nGeneration, eField, iRecord, nullptr, 0, &nRequired );
	if ( eMeasure != SIR_EDITOR_BRIDGE_OUT_OF_CAPACITY || nRequired == 0 || nRequired > sizeof( szField ) ) {
		return godot::String();
	}
	const sirEditorBridgeResult_t eCopy =
	    sirEditorNativeBridgeCopyField( m_pBridge, nGeneration, eField, iRecord, szField, sizeof( szField ), &nRequired );
	if ( eCopy != SIR_EDITOR_BRIDGE_OK ) {
		return godot::String();
	}
	*pValid = true;
	return godot::String::utf8( szField, ( int )( nRequired - 1 ) );
}

godot::Dictionary SIREditorGodotSession::get_snapshot( void ) const
{
	godot::Dictionary result;
	if ( m_pBridge == nullptr || m_sScenePath.is_empty() ) {
		return result;
	}
	sirEditorBridgeSnapshot_t snapshot = {};
	if ( sirEditorNativeBridgeSnapshot( m_pBridge, &snapshot ) != SIR_EDITOR_BRIDGE_OK ||
	     snapshot.nAbiVersion != SIR_EDITOR_BRIDGE_ABI_VERSION || snapshot.nNodeCount > SIR_GODOT_EDITOR_MAX_NODES ||
	     snapshot.nPropertyCount > SIR_GODOT_EDITOR_MAX_PROPERTIES ) {
		return result;
	}

	godot::Array nodes;
	for ( uint32_t iNode = 0; iNode < snapshot.nNodeCount; ++iNode ) {
		sirEditorBridgeNode_t node = {};
		if ( sirEditorNativeBridgeNodeAt( m_pBridge, snapshot.nGeneration, iNode, &node ) != SIR_EDITOR_BRIDGE_OK ) {
			return godot::Dictionary();
		}
		bool bNameValid = false;
		bool bPathValid = false;
		godot::String sName = copyField( snapshot.nGeneration, SIR_EDITOR_BRIDGE_NODE_NAME, iNode, &bNameValid );
		godot::String sPath = copyField( snapshot.nGeneration, SIR_EDITOR_BRIDGE_NODE_PATH, iNode, &bPathValid );
		if ( !bNameValid || !bPathValid ) {
			return godot::Dictionary();
		}
		godot::Dictionary entry;
		entry["id"] = formatId( node.nId );
		entry["parent"] = formatId( node.nParentId );
		entry["name"] = sName;
		entry["path"] = sPath;
		entry["line"] = ( int64_t )node.nSourceLine;
		entry["unique_id"] = ( int64_t )node.nGodotUniqueId;
		nodes.push_back( entry );
	}

	godot::Array resources;
	for ( uint32_t iResource = 0; iResource < snapshot.nResourceCount; ++iResource ) {
		sirEditorBridgeResource_t source = {};
		if ( sirEditorNativeBridgeResourceAt( m_pBridge, snapshot.nGeneration, iResource, &source ) != SIR_EDITOR_BRIDGE_OK ) {
			return godot::Dictionary();
		}
		bool bValid = false;
		godot::Dictionary entry;
		entry["kind"] = source.nKind == 1 ? "external" : "embedded";
		entry["id"] = copyField( snapshot.nGeneration, SIR_EDITOR_BRIDGE_RESOURCE_ID, iResource, &bValid );
		if ( !bValid ) {
			return godot::Dictionary();
		}
		entry["type"] = copyField( snapshot.nGeneration, SIR_EDITOR_BRIDGE_RESOURCE_TYPE, iResource, &bValid );
		if ( !bValid ) {
			return godot::Dictionary();
		}
		entry["path"] = copyField( snapshot.nGeneration, SIR_EDITOR_BRIDGE_RESOURCE_PATH, iResource, &bValid );
		if ( !bValid ) {
			return godot::Dictionary();
		}
		entry["uid"] = copyField( snapshot.nGeneration, SIR_EDITOR_BRIDGE_RESOURCE_UID, iResource, &bValid );
		if ( !bValid ) {
			return godot::Dictionary();
		}
		entry["line"] = ( int64_t )source.nSourceLine;
		resources.push_back( entry );
	}

	godot::Array properties;
	for ( uint32_t iProperty = 0; iProperty < snapshot.nPropertyCount; ++iProperty ) {
		sirEditorBridgeProperty_t source = {};
		if ( sirEditorNativeBridgePropertyAt( m_pBridge, snapshot.nGeneration, iProperty, &source ) != SIR_EDITOR_BRIDGE_OK ) {
			return godot::Dictionary();
		}
		bool bValid = false;
		godot::String sName = copyField( snapshot.nGeneration, SIR_EDITOR_BRIDGE_PROPERTY_NAME, iProperty, &bValid );
		if ( !bValid ) {
			return godot::Dictionary();
		}
		godot::Dictionary entry;
		entry["node"] = formatId( source.nNodeId );
		entry["name"] = sName;
		entry["line"] = ( int64_t )source.nSourceLine;
		entry["bytes"] = ( int64_t )source.nValueBytes;
		if ( source.nValueBytes < SIR_GODOT_EDITOR_MAX_FIELD ) {
			entry["value"] = copyField( snapshot.nGeneration, SIR_EDITOR_BRIDGE_PROPERTY_VALUE, iProperty, &bValid );
			if ( !bValid ) {
				return godot::Dictionary();
			}
			entry["truncated"] = false;
		} else {
			entry["value"] = "[Serialized value exceeds native preview cap; use Godot Inspector]";
			entry["truncated"] = true;
		}
		properties.push_back( entry );
	}

	result["schema"] = 1;
	result["scene"] = m_sScenePath;
	result["revision"] = formatId( snapshot.nDocumentRevision );
	result["nodes"] = nodes;
	result["resources"] = resources;
	result["properties"] = properties;
	return result;
}

godot::String SIREditorGodotSession::get_last_error( void ) const
{
	return m_sLastError;
}
