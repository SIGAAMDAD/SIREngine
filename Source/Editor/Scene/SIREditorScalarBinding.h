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

#ifndef __SIRENGINE_SIREDITORSCALARBINDING_H__
#define __SIRENGINE_SIREDITORSCALARBINDING_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Editor/Scene/SIREditorScalarCodec.h>
#include <Editor/Scene/SIREditorTextWriter.h>

typedef enum sirEditorScalarBridgeResult_e : uint8_t
{
	SIR_EDITOR_SCALAR_BRIDGE_OK = 0,
	SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT = 1,
	SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING = 2,
	SIR_EDITOR_SCALAR_BRIDGE_UNSUPPORTED_VALUE = 3,
	SIR_EDITOR_SCALAR_BRIDGE_DOCUMENT_ERROR = 4,
	SIR_EDITOR_SCALAR_BRIDGE_WRITE_ERROR = 5,
	SIR_EDITOR_SCALAR_BRIDGE_OUT_OF_CAPACITY = 6
} sirEditorScalarBridgeResult_t;

typedef struct sirEditorScalarBinding_s
{
	uint64_t nNodeId;
	uint32_t nPropertyId;
	uint32_t iSourceProperty;
} sirEditorScalarBinding_t;

// The engine's string-atom pool implements this comparison. This API does
// not create a parallel string table or require Godot Variant objects.
typedef bool (*sirEditorScalarAtomMatches_t)( uint64_t nAtom, const char *pszSource, uint32_t nBytes,
                                               void *pContext ) noexcept;

typedef struct sirEditorScalarDiscovery_s
{
	const SIREditorSceneDocument *pDocument;
	const SIRObjectRegistry *pRegistry;
	const sirEditorTextPropertyIndex_t *pIndex;
	const char *pszSource;
	size_t nSourceBytes;
	sirEditorScalarAtomMatches_t pMatchesAtom;
	void *pContext;
	sirEditorScalarBinding_t *pOutput;
	uint32_t nOutputCapacity;
} sirEditorScalarDiscovery_t;

// Discover supported scalar properties from source names and native type
// metadata. Null output/capacity zero measures required storage. A failure
// does not publish a partial binding table.
sirEditorScalarBridgeResult_t sirEditorDiscoverScalarBindings( const sirEditorScalarDiscovery_t& context,
                                                               uint32_t *pOutCount ) noexcept;

// Caller-supplied scratch; all borrowed pointers remain valid through the
// entire operation. The editor document is the only mutable authoring graph.
typedef struct sirEditorScalarBridge_s
{
	SIREditorSceneDocument *pDocument;
	const SIRObjectRegistry *pRegistry;
	const sirEditorTextPropertyIndex_t *pIndex;
	const char *pszSource;
	size_t nSourceBytes;
	const sirEditorScalarBinding_t *pBindings;
	uint32_t nBindingCount;
	sirEditorProperty_t *pScratch;
	sirEditorTextEdit_t *pEdits;
	char *pszEncodedScalars;
	size_t nEncodedScalarBytes;
} sirEditorScalarBridge_t;

// Maps only the explicitly bound scalar subset into one document transaction.
// A failed parse, type check, or commit never publishes partial property edits.
sirEditorScalarBridgeResult_t sirEditorImportBoundScalars( const sirEditorScalarBridge_t& context ) noexcept;

// Writes the current document values into the preserved original source bytes.
// Supports a measure-only call (null output, zero capacity).
sirEditorScalarBridgeResult_t sirEditorExportBoundScalars( const sirEditorScalarBridge_t& context, char *pszOutput,
                                                           size_t nCapacity, size_t *pOutBytes ) noexcept;

#endif // __SIRENGINE_SIREDITORSCALARBINDING_H__
