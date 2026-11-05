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

#ifndef __SIRENGINE_SIRLANGDIAGNOSTICS_H__
#define __SIRENGINE_SIRLANGDIAGNOSTICS_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Scripting/Language/SIRLangSourceCompiler.h>

// Native editor/tooling projection of a compiler failure. All positions are
// source-byte offsets. No UI toolkit types or editor-owned memory is stored.
typedef struct sirLangEditorDiagnostic_s
{
	sirLangCompilePhase_t ePhase;
	uint32_t nCode;
	uint32_t nOffset;
	uint32_t nLength;
	uint32_t nLine;
	uint32_t nColumn;
	bool bHasSourceLocation;
} sirLangEditorDiagnostic_t;

// Writes up to nCapacity errors from the failed compilation in source order.
// The return value counts written entries. If the failing stage has no
// diagnostic array, one stage-level fallback is emitted. No heap allocation.
uint32_t SIRLangCollectCompileDiagnostics( const sirLangCompileWorkspace_t *pWorkspace, sirLangEditorDiagnostic_t *pDiagnostics,
                                           uint32_t nCapacity ) noexcept;

// Returns false only when the source has no failure to report or the caller
// supplied an invalid destination. Never invokes a compiler pass.
bool SIRLangGetCompileDiagnostic( const sirLangCompileWorkspace_t *pWorkspace, sirLangEditorDiagnostic_t *pDiagnostic ) noexcept;

#endif // __SIRENGINE_SIRLANGDIAGNOSTICS_H__
