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

#ifndef __SIRENGINE_SIREDITORSCALARCODEC_H__
#define __SIRENGINE_SIREDITORSCALARCODEC_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Objects/SIRObjectRegistry.h>

typedef enum sirEditorScalarResult_e : uint8_t
{
	SIR_EDITOR_SCALAR_OK = 0,
	SIR_EDITOR_SCALAR_INVALID_ARGUMENT = 1,
	SIR_EDITOR_SCALAR_UNSUPPORTED_KIND = 2,
	SIR_EDITOR_SCALAR_INVALID_TEXT = 3,
	SIR_EDITOR_SCALAR_OUT_OF_CAPACITY = 4
} sirEditorScalarResult_t;

// The codecs handle the scalar subset of Godot text-scene values. String atoms
// and resource IDs require the owning pool/catalog to resolve their source IDs.
// There are no heap allocations, Variant objects, or alternate resource owners.
sirEditorScalarResult_t sirEditorDecodeScalar( const char *pszText, size_t nBytes, sirPropertyKind_t eKind,
                                               sirPropertyValue_t *pOutValue ) noexcept;
sirEditorScalarResult_t sirEditorEncodeScalar( const sirPropertyValue_t& value, char *pszOutput, size_t nCapacity,
                                               size_t *pOutBytes ) noexcept;

#endif // __SIRENGINE_SIREDITORSCALARCODEC_H__
