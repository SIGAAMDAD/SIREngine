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

#ifndef __SIRENGINE_SIREDITORNATIVEBRIDGE_H__
#define __SIRENGINE_SIREDITORNATIVEBRIDGE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stddef.h>
#include <stdint.h>

#define SIR_EDITOR_BRIDGE_ABI_VERSION 1u

#if defined( __cplusplus )
extern "C" {
#endif

// This binary interface is intentionally free of Godot Object/Variant and C++
// library types. Its implementation is single-authoring-thread owned.
typedef struct sirEditorNativeBridgeHandle_s sirEditorNativeBridgeHandle_t;

typedef enum sirEditorBridgeResult_e : uint8_t
{
	SIR_EDITOR_BRIDGE_OK = 0,
	SIR_EDITOR_BRIDGE_INVALID_ARGUMENT = 1,
	SIR_EDITOR_BRIDGE_UNINITIALIZED = 2,
	SIR_EDITOR_BRIDGE_NOT_OPEN = 3,
	SIR_EDITOR_BRIDGE_ALREADY_OPEN = 4,
	SIR_EDITOR_BRIDGE_STALE = 5,
	SIR_EDITOR_BRIDGE_NOT_FOUND = 6,
	SIR_EDITOR_BRIDGE_OUT_OF_CAPACITY = 7,
	SIR_EDITOR_BRIDGE_MEMORY_FAILURE = 8,
	SIR_EDITOR_BRIDGE_SCENE_FAILURE = 9
} sirEditorBridgeResult_t;

typedef enum sirEditorBridgeStringField_e : uint8_t
{
	SIR_EDITOR_BRIDGE_NODE_NAME = 1,
	SIR_EDITOR_BRIDGE_NODE_PATH = 2,
	SIR_EDITOR_BRIDGE_RESOURCE_ID = 3,
	SIR_EDITOR_BRIDGE_RESOURCE_TYPE = 4,
	SIR_EDITOR_BRIDGE_RESOURCE_PATH = 5,
	SIR_EDITOR_BRIDGE_RESOURCE_UID = 6,
	SIR_EDITOR_BRIDGE_PROPERTY_NAME = 7,
	SIR_EDITOR_BRIDGE_PROPERTY_VALUE = 8
} sirEditorBridgeStringField_t;

typedef struct sirEditorBridgeSnapshot_s
{
	uint32_t nAbiVersion;
	uint32_t nNodeCount;
	uint32_t nResourceCount;
	uint32_t nPropertyCount;
	uint64_t nGeneration;
	uint64_t nDocumentRevision;
} sirEditorBridgeSnapshot_t;

typedef struct sirEditorBridgeNode_s
{
	uint64_t nId;
	uint64_t nParentId;
	uint32_t nSourceLine;
	uint32_t nGodotUniqueId;
	uint32_t nNameBytes;
	uint32_t nPathBytes;
} sirEditorBridgeNode_t;

typedef struct sirEditorBridgeResource_s
{
	uint64_t nStableKey;
	uint32_t nSourceLine;
	uint32_t nKind;
	uint32_t nIdBytes;
	uint32_t nTypeBytes;
	uint32_t nPathBytes;
	uint32_t nUidBytes;
} sirEditorBridgeResource_t;

typedef struct sirEditorBridgeProperty_s
{
	uint64_t nNodeId;
	uint32_t nSourceLine;
	uint32_t nNameBytes;
	uint32_t nValueBytes;
} sirEditorBridgeProperty_t;

// The editor host owns the storage AND the supplied SIRMemorySegment. The
// segment must belong to its existing SIRMemoryManager and have domain EDITOR.
// This bridge never creates a second OS reservation or manager; it only
// borrows the segment for its bounded, single-thread authoring workspace.
size_t sirEditorNativeBridgeStorageBytes( void ) noexcept;
size_t sirEditorNativeBridgeStorageAlignment( void ) noexcept;
sirEditorBridgeResult_t sirEditorNativeBridgeInit( void *pStorage, size_t nStorageBytes, void *pEditorSegment,
                                                   sirEditorNativeBridgeHandle_t **ppOutHandle ) noexcept;
void sirEditorNativeBridgeDestroy( sirEditorNativeBridgeHandle_t *pHandle ) noexcept;

sirEditorBridgeResult_t sirEditorNativeBridgeOpenText( sirEditorNativeBridgeHandle_t *pHandle, const char *pszSource,
                                                       size_t nBytes ) noexcept;
sirEditorBridgeResult_t sirEditorNativeBridgeOpenFile( sirEditorNativeBridgeHandle_t *pHandle, const char *pszPath ) noexcept;
void sirEditorNativeBridgeClose( sirEditorNativeBridgeHandle_t *pHandle ) noexcept;

sirEditorBridgeResult_t sirEditorNativeBridgeSnapshot( const sirEditorNativeBridgeHandle_t *pHandle,
                                                       sirEditorBridgeSnapshot_t *pOutSnapshot ) noexcept;
sirEditorBridgeResult_t sirEditorNativeBridgeNodeAt( const sirEditorNativeBridgeHandle_t *pHandle, uint64_t nExpectedGeneration,
                                                     uint32_t iNode, sirEditorBridgeNode_t *pOutNode ) noexcept;
sirEditorBridgeResult_t sirEditorNativeBridgeResourceAt( const sirEditorNativeBridgeHandle_t *pHandle, uint64_t nExpectedGeneration,
                                                         uint32_t iResource, sirEditorBridgeResource_t *pOutResource ) noexcept;
sirEditorBridgeResult_t sirEditorNativeBridgePropertyAt( const sirEditorNativeBridgeHandle_t *pHandle, uint64_t nExpectedGeneration,
                                                         uint32_t iProperty, sirEditorBridgeProperty_t *pOutProperty ) noexcept;

// Copies the FULL raw field bytes + NUL. Insufficient capacity never changes
// the destination. Required capacity includes the terminator. This operation
// never exposes borrowed source or a pointer surviving a scene reload.
sirEditorBridgeResult_t sirEditorNativeBridgeCopyField( const sirEditorNativeBridgeHandle_t *pHandle, uint64_t nExpectedGeneration,
                                                        sirEditorBridgeStringField_t eField, uint32_t iRecord, char *pszOutput,
                                                        size_t nCapacity, size_t *pRequiredBytes ) noexcept;

#if defined( __cplusplus )
}
#endif

#endif // __SIRENGINE_SIREDITORNATIVEBRIDGE_H__
