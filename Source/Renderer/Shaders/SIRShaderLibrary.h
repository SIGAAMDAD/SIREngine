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

#ifndef __SIRENGINE_SIRSHADERLIBRARY_H__
#define __SIRENGINE_SIRSHADERLIBRARY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Shaders/SIRShaderGRDG.h>

// Stateless, scope-bound renderer projection. The library does not own GRDG
// records, bytes, PSOs or a native driver. The graph and scope must agree.
typedef struct sirShaderLibraryContext_s
{
	const SIRResourceGraph *pGraph;
	const SIRResourceReadScope *pScope;
	sirShaderGRDGFormats_t formats;
} sirShaderLibraryContext_t;

typedef struct sirShaderResolveStageRequest_s
{
	sirShaderPass_t ePass;
	uint32_t nStageMask;
	sirShaderPackageValidation_t validation;
} sirShaderResolveStageRequest_t;

typedef struct sirShaderResolvedStage_s
{
	sirShaderMaterialPassRecord_t material;
	sirShaderStageRecord_t stage;
	SIRResourceHandle hShader;
	SIRResourceConstBytes binary;
	uint64_t nMaterialGraphRevision;
	uint64_t nShaderGraphRevision;
	uint64_t nShaderSourceRevision;
	bool bFixedFunction;
} sirShaderResolvedStage_t;

// On success the binary pointer remains borrowed from the active scope.
// Consumers must pin the exact revision before scheduling native GPU work.
SIRResourceResult sirShaderResolveMaterialStage( const sirShaderLibraryContext_t& context, const sirShaderMaterialProjection_t& material,
                                                 const sirShaderResolveStageRequest_t& request, sirShaderResolvedStage_t *pOut ) noexcept;

#endif // __SIRENGINE_SIRSHADERLIBRARY_H__
