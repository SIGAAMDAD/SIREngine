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

#ifndef __SIRENGINE_SIRD3D11TEXTUREVALIDATION_H__
#define __SIRENGINE_SIRD3D11TEXTUREVALIDATION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/D3D11/SIRD3D11Formats.h>

typedef struct sirD3D11TexturePlan_s
{
	uint64_t nLogicalBytes;
	uint32_t nNativeBindUsage;
	uint32_t nArrayLayers;
	uint32_t nMipLevels;
	uint32_t nSampleCount;
} sirD3D11TexturePlan_t;

// D3D11-owned admission and logical capacity accounting. The driver checks
// hardware/format support separately before creating a native COM resource.
sirGraphicsResult_t sirD3D11PlanTexture( const sirGraphicsTextureDesc_t *pDesc, const sirD3D11FormatSupport_t *pSupport,
                                         uint64_t nCommittedBytes, uint64_t nBudgetBytes, sirD3D11TexturePlan_t *pPlan ) noexcept;
sirGraphicsResult_t sirD3D11ValidateTextureView( const sirGraphicsViewDesc_t *pView, const sirGraphicsTextureDesc_t *pTexture ) noexcept;

#endif // __SIRENGINE_SIRD3D11TEXTUREVALIDATION_H__
