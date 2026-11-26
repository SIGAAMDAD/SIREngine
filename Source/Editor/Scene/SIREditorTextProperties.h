/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_EDITORTEXTPROPERTIES_H__
#define __SIR_EDITORTEXTPROPERTIES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Editor/Scene/SIREditorTextScene.h>

enum class SIREditorTextPropertyResult : uint8_t {
	SIR_EDITOR_PROPERTY_OK = 0,
	SIR_EDITOR_PROPERTY_INVALID_ARGUMENT = 1,
	SIR_EDITOR_PROPERTY_INVALID_SOURCE = 2,
	SIR_EDITOR_PROPERTY_MALFORMED_VALUE = 3,
	SIR_EDITOR_PROPERTY_OUT_OF_CAPACITY = 4,
	SIR_EDITOR_PROPERTY_TOO_MANY = 5
};

typedef struct sirEditorTextProperty_s
{
	uint64_t nNodeId;
	uint32_t nNameOffset;
	uint32_t nNameLength;
	uint32_t nValueOffset;
	uint32_t nValueLength;
	uint32_t nSourceLine;
} sirEditorTextProperty_t;

typedef struct sirEditorTextPropertyIndex_s
{
	sirEditorTextProperty_t *pProperties;
	uint32_t nCapacity;
	uint32_t nCount;
	uint32_t nErrorLine;
} sirEditorTextPropertyIndex_t;

// Index serialized property spans of authored [node] sections without Variant
// parsing, mutation, or heap allocations. Offsets borrow the retained .tscn
// source. A null output/capacity zero first pass measures the exact count.
// A failed scan publishes zero entries and identifies the offending line.
SIREditorTextPropertyResult sirEditorScanNodeTextProperties( const char *pszSource, size_t nSourceBytes,
                                                               const sirEditorTextSceneIndex_t& scene,
                                                               sirEditorTextPropertyIndex_t *pIndex ) noexcept;

#endif // __SIR_EDITORTEXTPROPERTIES_H__
