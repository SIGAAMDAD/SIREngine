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

#ifndef __SIRENGINE_SIRLANGSOURCECOMPILER_H__
#define __SIRENGINE_SIRLANGSOURCECOMPILER_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Scripting/Language/SIRLangEffects.h>

typedef enum sirLangCompilePhase_e : uint8_t
{
	SIR_LANG_COMPILE_INPUT = 0,
	SIR_LANG_COMPILE_PARSE,
	SIR_LANG_COMPILE_SEMANTIC,
	SIR_LANG_COMPILE_SIGNATURES,
	SIR_LANG_COMPILE_HIR,
	SIR_LANG_COMPILE_CIR,
	SIR_LANG_COMPILE_EFFECTS,
	SIR_LANG_COMPILE_COMPLETE
} sirLangCompilePhase_t;

typedef struct sirLangCompileReport_s
{
	sirLangCompilePhase_t ePhase;
	uint32_t nStageResult;
	uint32_t iErrorNode;
} sirLangCompileReport_t;

// All scratch and compiled output remain owned by the caller. An unsuccessful
// compilation is never eligible for serialization, execution, or publication.
// The input source must outlive the populated syntax tree and HIR.
typedef struct sirLangCompileWorkspace_s
{
	sirLangSyntaxTree_t syntax;
	sirLangSemanticOutput_t semantic;
	sirLangSignatureOutput_t signatures;
	sirLangHIRProgram_t hir;
	sirLangCIRStorage_t cir;
	sirLangEffectOutput_t effects;
	sirLangCompileReport_t report;
} sirLangCompileWorkspace_t;

// Compiles one source module through the real parser and every verification
// pass. The caller supplies output arrays and nonzero module/layout/binding IDs.
// An optional native signature/effect manifest must come from one immutable
// host binding snapshot and may not change during this operation.
bool SIRLangCompileSource( const sirLangSourceView_t& source, sirLangCompileWorkspace_t *pWorkspace ) noexcept;

#endif // __SIRENGINE_SIRLANGSOURCECOMPILER_H__
