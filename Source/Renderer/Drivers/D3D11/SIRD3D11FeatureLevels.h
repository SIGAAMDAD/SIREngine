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

#ifndef __SIRENGINE_SIRD3D11FEATURELEVELS_H__
#define __SIRENGINE_SIRD3D11FEATURELEVELS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>
#include <Renderer/SIRGraphicsTypes.h>

// Values match D3D_FEATURE_LEVEL; the public header never imports Windows SDK
// types. The runtime's returned feature level is the authoritative input.
typedef enum sirD3D11FeatureLevel_e : uint32_t
{
	SIR_D3D11_FEATURE_LEVEL_9_1 = 0x9100,
	SIR_D3D11_FEATURE_LEVEL_9_2 = 0x9200,
	SIR_D3D11_FEATURE_LEVEL_9_3 = 0x9300,
	SIR_D3D11_FEATURE_LEVEL_10_0 = 0xA000,
	SIR_D3D11_FEATURE_LEVEL_10_1 = 0xA100,
	SIR_D3D11_FEATURE_LEVEL_11_0 = 0xB000,
	SIR_D3D11_FEATURE_LEVEL_11_1 = 0xB100
} sirD3D11FeatureLevel_t;

typedef enum sirD3D11ShaderProfile_e : uint32_t
{
	SIR_D3D11_SHADER_PROFILE_4_0_LEVEL_9_1 = 1,
	SIR_D3D11_SHADER_PROFILE_4_0_LEVEL_9_3 = 2,
	SIR_D3D11_SHADER_PROFILE_4_0 = 3,
	SIR_D3D11_SHADER_PROFILE_4_1 = 4,
	SIR_D3D11_SHADER_PROFILE_5_0 = 5
} sirD3D11ShaderProfile_t;

// Booleans are 0/1 uint32_t to make validation deterministic at the ABI seam.
// Only populate these fields from native runtime/device probes, not the
// requested feature level, a display adapter name, or a shader manifest.
typedef struct sirD3D11FeatureProbe_s
{
	uint32_t nSize;
	uint32_t nFeatureLevel;
	uint32_t nD3D11_1InterfaceAvailable;
	uint32_t nComputeShader4xAvailable;
	uint32_t nIndirectDrawAvailable;
	uint32_t nMultisampleAvailable;
	uint32_t nPresentationAvailable;
	uint32_t nReserved;
} sirD3D11FeatureProbe_t;

// Conservative guaranteed limits. Format-specific support and actual sample
// counts require CheckFormatSupport/CheckMultisampleQualityLevels later.
typedef struct sirD3D11FeatureProfile_s
{
	uint32_t nSize;
	uint32_t nFeatureLevel;
	uint32_t nShaderProfile;
	uint32_t nMaximumTexture2D;
	uint32_t nMaximumColorTargets;
	uint32_t nMaximumUavSlots;
	uint32_t nMaximumShaderResourceSlots;
	uint32_t nDirectQueueMask;
	uint32_t nPipelineModelMask;
	uint32_t nSupportsIndexedInstancing;
	uint32_t nD3D11_1InterfaceAvailable;
	uint32_t nReserved;
	uint64_t nEnabledFeatures;
} sirD3D11FeatureProfile_t;

sirGraphicsResult_t sirD3D11BuildFeatureProfile( const sirD3D11FeatureProbe_t *pProbe, sirD3D11FeatureProfile_t *pProfile ) noexcept;

#endif // __SIRENGINE_SIRD3D11FEATURELEVELS_H__
