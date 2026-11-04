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

#ifndef __SIRENGINE_SIREDITORFILESAVE_H__
#define __SIRENGINE_SIREDITORFILESAVE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Editor/Scene/SIREditorScalarBinding.h>

typedef enum sirEditorFileSaveResult_e : uint8_t
{
	SIR_EDITOR_FILE_SAVE_OK = 0,
	SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT = 1,
	SIR_EDITOR_FILE_SAVE_INVALID_PATH = 2,
	SIR_EDITOR_FILE_SAVE_TEMP_EXISTS = 3,
	SIR_EDITOR_FILE_SAVE_IO_ERROR = 4,
	SIR_EDITOR_FILE_SAVE_REPLACE_FAILED = 5,
	SIR_EDITOR_FILE_SAVE_COMMITTED_NOT_DURABLE = 6,
	SIR_EDITOR_FILE_SAVE_OUT_OF_CAPACITY = 7,
	SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED = 8
} sirEditorFileSaveResult_t;

typedef struct sirEditorFileWriteRequest_s
{
	const char *pszDestination;
	const char *pszTemporary;
	const char *pBytes;
	size_t nBytes;
} sirEditorFileWriteRequest_t;

typedef struct sirEditorSceneSaveRequest_s
{
	const sirEditorScalarBridge_t *pBridge;
	const char *pszDestination;
	const char *pszTemporary;
	char *pszScratch;
	size_t nScratchCapacity;
} sirEditorSceneSaveRequest_t;

// Creates an exclusively owned sibling temporary file; writes, flushes and
// syncs its contents before atomic replacement. On pre-replace failures the
// destination remains untouched and the temporary file is removed. A
// durability failure after replacement is reported separately because the
// destination has already changed.
sirEditorFileSaveResult_t sirEditorWriteFileAtomically( const sirEditorFileWriteRequest_t &request ) noexcept;

// Exports the canonical scene document to caller-owned scratch and then saves
// it via the same atomic path; no second authored graph is created. The
// workspace source remains the original snapshot until caller closes/reopens.
sirEditorFileSaveResult_t sirEditorSaveBoundScene( const sirEditorSceneSaveRequest_t &request ) noexcept;

#endif // __SIRENGINE_SIREDITORFILESAVE_H__
