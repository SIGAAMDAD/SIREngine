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

#ifndef __SIRENGINE_SIREDITORTEXTWRITER_H__
#define __SIRENGINE_SIREDITORTEXTWRITER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Editor/Scene/SIREditorTextProperties.h>

typedef enum sirEditorTextWriteResult_e : uint8_t
{
	SIR_EDITOR_WRITE_OK = 0,
	SIR_EDITOR_WRITE_INVALID_ARGUMENT = 1,
	SIR_EDITOR_WRITE_INVALID_SOURCE = 2,
	SIR_EDITOR_WRITE_INVALID_SCALAR = 3,
	SIR_EDITOR_WRITE_DUPLICATE_PATCH = 4,
	SIR_EDITOR_WRITE_OUT_OF_CAPACITY = 5
} sirEditorTextWriteResult_t;

typedef struct sirEditorTextEdit_s
{
	uint32_t iProperty;
	const char *pszReplacement;
	uint32_t nReplacementBytes;
} sirEditorTextEdit_t;

// Lossless text-scene scalar patch writer. Retains untouched source bytes,
// including formatting, comments and unrecognized sections. It is a format
// codec helper, never a second mutable scene or an independent serializer.
// The caller reparses the result into the canonical editor document before
// committing or saving it. No bytes are published on a failed validation.
// pOutBytes excludes the trailing NUL; output capacity must include it.
sirEditorTextWriteResult_t sirEditorWriteScalarPatches( const char *pszSource, size_t nSourceBytes,
                                                        const sirEditorTextPropertyIndex_t &index, const sirEditorTextEdit_t *pEdits,
                                                        uint32_t nEdits, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept;

#endif // __SIRENGINE_SIREDITORTEXTWRITER_H__
