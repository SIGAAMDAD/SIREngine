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

#ifndef __SIRENGINE_SIREDITORTEXTSCENE_H__
#define __SIRENGINE_SIREDITORTEXTSCENE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Editor/Scene/SIREditorSceneDocument.h>

typedef enum sirEditorTextSceneResult_e : uint8_t
{
	SIR_EDITOR_TEXT_OK = 0,
	SIR_EDITOR_TEXT_INVALID_ARGUMENT = 1,
	SIR_EDITOR_TEXT_INVALID_HEADER = 2,
	SIR_EDITOR_TEXT_INVALID_TAG = 3,
	SIR_EDITOR_TEXT_INVALID_NODE = 4,
	SIR_EDITOR_TEXT_UNSUPPORTED_INSTANCE = 5,
	SIR_EDITOR_TEXT_MISSING_PARENT = 6,
	SIR_EDITOR_TEXT_DUPLICATE_NODE = 7,
	SIR_EDITOR_TEXT_OUT_OF_CAPACITY = 8
} sirEditorTextSceneResult_t;

typedef struct sirEditorTextSceneNode_s
{
	sirEditorNode_t node;
	uint32_t nSourceLine;
	uint32_t nNameOffset;
	uint32_t nNameLength;
	uint32_t nPathOffset;
	uint32_t nPathLength;
	uint32_t nGodotUniqueId;
} sirEditorTextSceneNode_t;

// The source text and all storage remain caller-owned. No Godot runtime types cross
// this boundary. The text scanner is read-only and does not replace Godot's Variant
// parser, inherited-scene loader, editor serializer or property storage.
typedef struct sirEditorTextSceneStorage_s
{
	sirEditorTextSceneNode_t *pNodes;
	char *pszPaths;
	uint32_t nNodeCapacity;
	uint32_t nPathCapacity;
} sirEditorTextSceneStorage_t;

typedef struct sirEditorTextSceneIndex_s
{
	sirEditorTextSceneStorage_t storage;
	uint32_t nNodeCount;
	uint32_t nPathBytes;
	uint32_t nErrorLine;
} sirEditorTextSceneIndex_t;

// Supports direct node hierarchy headers, Godot unique_id and opaque resource /
// property sections. Rejects unsupported inherited/instanced nodes explicitly.
// On parse failure, nNodeCount and nPathBytes are zero (no partial publication).
sirEditorTextSceneResult_t sirEditorScanTextScene( const char *pszSource, size_t nSourceBytes, sirEditorTextSceneIndex_t *pIndex ) noexcept;

// Reconcile one scanned scene into the native document as a single transaction.
// pSnapshot is independent caller-owned scratch with capacity for all current nodes.
// No-op reconciliation does not consume an undo entry or increment revision.
sirEditorResult_t sirEditorReconcileTextScene( SIREditorSceneDocument &document, const sirEditorTextSceneIndex_t &index,
                                               sirEditorNode_t *pSnapshot, uint32_t nSnapshotCapacity ) noexcept;

// Source-backed resource identity/dependency metadata for inspector and cook intake.
// Offsets address the caller-owned .tscn bytes and remain valid only while those
// bytes are retained. This index never owns or decodes Godot Variant payloads.
typedef enum sirEditorTextResourceResult_e : uint8_t
{
	SIR_EDITOR_RESOURCE_OK = 0,
	SIR_EDITOR_RESOURCE_INVALID_ARGUMENT = 1,
	SIR_EDITOR_RESOURCE_INVALID_HEADER = 2,
	SIR_EDITOR_RESOURCE_INVALID_TAG = 3,
	SIR_EDITOR_RESOURCE_INVALID_DECLARATION = 4,
	SIR_EDITOR_RESOURCE_DUPLICATE_ID = 5,
	SIR_EDITOR_RESOURCE_MISSING_REFERENCE = 6,
	SIR_EDITOR_RESOURCE_INVALID_REFERENCE = 7,
	SIR_EDITOR_RESOURCE_OUT_OF_CAPACITY = 8
} sirEditorTextResourceResult_t;

typedef enum sirEditorTextResourceKind_e : uint8_t
{
	SIR_EDITOR_RESOURCE_EXTERNAL = 1,
	SIR_EDITOR_RESOURCE_EMBEDDED = 2
} sirEditorTextResourceKind_t;

typedef struct sirEditorTextResource_s
{
	uint64_t nStableKey;
	uint32_t nSourceLine;
	uint32_t nIdOffset;
	uint32_t nIdLength;
	uint32_t nTypeOffset;
	uint32_t nTypeLength;
	uint32_t nPathOffset;
	uint32_t nPathLength;
	uint32_t nUidOffset;
	uint32_t nUidLength;
	sirEditorTextResourceKind_t eKind;
} sirEditorTextResource_t;

typedef struct sirEditorTextResourceIndex_s
{
	sirEditorTextResource_t *pResources;
	uint32_t nCapacity;
	uint32_t nCount;
	uint32_t nErrorLine;
} sirEditorTextResourceIndex_t;

// Index [ext_resource] and [sub_resource] declarations and verify every
// ExtResource(...) / SubResource(...) expression against declared local IDs.
// Duplicate IDs are rejected within their respective resource kinds.
// An empty resource list is valid with pResources=nullptr and nCapacity=0.
// On failure, nCount is zero and nErrorLine identifies the source line.
sirEditorTextResourceResult_t sirEditorScanTextResources( const char *pszSource, size_t nSourceBytes,
                                                          sirEditorTextResourceIndex_t *pIndex ) noexcept;

// Lookup a validated local resource ID, using the original source bytes.
// Resource stable keys only identify declarations within a particular scene;
// the importing editor must add its canonical source identity.
sirEditorTextResourceResult_t sirEditorFindTextResource( const char *pszSource, const sirEditorTextResourceIndex_t &index,
                                                         sirEditorTextResourceKind_t eKind, const char *pszId, uint32_t nIdLength,
                                                         uint32_t *pOutIndex ) noexcept;

#endif // __SIRENGINE_SIREDITORTEXTSCENE_H__
