/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIREDITORCOMPLEXWRITER_H__
#define __SIRENGINE_SIREDITORCOMPLEXWRITER_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on
#include <Editor/Scene/SIREditorTextWriter.h>

// Bounded syntactic validation for text-scene constructors, nested arrays,
// dictionaries, string literals and plain scalars. This does not resolve
// native Variant types or resource identities; consumers must reparse the
// output with the authoritative scene reader before persisting it.
bool sirEditorValidateComplexValue( const char *pszValue, uint32_t nLength ) noexcept;

// Lossless source-span replacement, supporting complex Godot text syntax.
// On failure the output remains unchanged; caller-owned buffers are required.
sirEditorTextWriteResult_t sirEditorWriteComplexPatches( const char *pszSource, size_t nSourceBytes,
                                                         const sirEditorTextPropertyIndex_t& index, const sirEditorTextEdit_t *pEdits,
                                                         uint32_t nEdits, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept;
#endif // __SIRENGINE_SIREDITORCOMPLEXWRITER_H__
