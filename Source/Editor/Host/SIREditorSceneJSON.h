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

#ifndef __SIRENGINE_SIREDITORSCENEJSON_H__
#define __SIRENGINE_SIREDITORSCENEJSON_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdio.h>

class SIREditorSceneWorkspace;

// Version 1 is a read-only editor presentation contract. All 64-bit IDs are
// decimal strings so client runtimes cannot lose identity through JSON numbers.
// Source/path strings remain valid only while the workspace is open.
bool sirEditorWriteSceneJSON( const SIREditorSceneWorkspace &workspace, const char *pszScenePath, FILE *pOutput ) noexcept;

// File-output protocol for OS.create_process consumers: write a sibling staging
// file and publish only after the complete JSON document is flushed and closed.
bool sirEditorWriteSceneJSONFile( const SIREditorSceneWorkspace &workspace, const char *pszScenePath, const char *pszOutputPath ) noexcept;

#endif // __SIRENGINE_SIREDITORSCENEJSON_H__
