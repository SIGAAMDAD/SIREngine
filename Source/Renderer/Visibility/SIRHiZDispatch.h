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

#ifndef __SIRENGINE_SIRHIZDISPATCH_H__
#define __SIRENGINE_SIRHIZDISPATCH_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Visibility/SIRVisibilityFoundation.h>

// Layout shared with the HLSL SIRHiZParameters constant buffer (b0).
// A pass consumes a single-sample source view and writes one R32F mip view.
typedef struct sirHiZShaderParameters_s
{
	uint32_t nSourceWidth;
	uint32_t nSourceHeight;
	uint32_t nDestinationWidth;
	uint32_t nDestinationHeight;
	uint32_t nSourceOriginX;
	uint32_t nSourceOriginY;
	uint32_t nReserved0;
	uint32_t nReserved1;
} sirHiZShaderParameters_t;

typedef struct sirHiZDispatch_s
{
	sirHiZShaderParameters_t parameters;
	uint32_t nSourceMip;
	uint32_t nDestinationMip;
	uint32_t nWorkgroupsX;
	uint32_t nWorkgroupsY;
	bool bCopyDepth;
} sirHiZDispatch_t;

typedef struct sirHiZDepthInput_s
{
	sirGraphicsTextureDesc_t texture;
	sirGraphicsRect_t viewport;
	sirVisibilityDepthConvention_t eConvention;
} sirHiZDepthInput_t;

typedef struct sirHiZDispatchPlan_s
{
	sirGraphicsTextureDesc_t pyramid;
	uint32_t nPassCount;
	sirHiZDispatch_t szPasses[32];
} sirHiZDispatchPlan_t;

// Produces the R32F texture descriptor and deterministic per-mip dispatches.
// Does not create GPU resources or submit commands. A caller must bind each
// source/destination mip separately and synchronize writes before later reads.
// Multisampled depth returns UNSUPPORTED until the device supplies an explicit
// reverse-Z conservative resolve (min across samples) into a single-sample
// view. Invalid input leaves the caller's plan untouched.
sirGraphicsResult_t sirHiZPlanDispatches( const sirHiZLayout_t& layout, const sirHiZDepthInput_t& depth,
                                          sirHiZDispatchPlan_t *pPlan ) noexcept;

static_assert( sizeof( sirHiZShaderParameters_t ) == 32, "Hi-Z HLSL b0 layout changed" );
static_assert( offsetof( sirHiZShaderParameters_t, nSourceOriginX ) == 16, "Hi-Z shader origin offset changed" );

#endif // __SIRENGINE_SIRHIZDISPATCH_H__
