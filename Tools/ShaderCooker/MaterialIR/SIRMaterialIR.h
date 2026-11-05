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

#ifndef __SIRENGINE_SIRMATERIALIR_H__
#define __SIRENGINE_SIRMATERIALIR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Shaders/SIRShaderContracts.h>
#include <Tools/ShaderCooker/Frontend/SIRShaderFrontend.h>

constexpr uint32_t SIR_MATERIAL_IR_MAX_NODES = 64;
constexpr uint32_t SIR_MATERIAL_IR_MAX_SWITCHES = 8;
constexpr uint32_t SIR_MATERIAL_IR_MAX_PASSES = 16;
constexpr uint32_t SIR_MATERIAL_IR_MAX_BINDINGS = 32;
constexpr uint16_t SIR_MATERIAL_IR_NO_INPUT = UINT16_MAX;

typedef enum sirMaterialIRType_e : uint8_t
{
	SIR_MATERIAL_IR_FLOAT = 1,
	SIR_MATERIAL_IR_FLOAT2 = 2,
	SIR_MATERIAL_IR_FLOAT3 = 3,
	SIR_MATERIAL_IR_FLOAT4 = 4,
	SIR_MATERIAL_IR_BOOL = 5,
	SIR_MATERIAL_IR_TEXTURE2D = 6,
	SIR_MATERIAL_IR_SAMPLER = 7
} sirMaterialIRType_t;

typedef enum sirMaterialIROp_e : uint8_t
{
	SIR_MATERIAL_IR_CONSTANT = 1,
	SIR_MATERIAL_IR_PARAMETER = 2,
	SIR_MATERIAL_IR_ADD = 3,
	SIR_MATERIAL_IR_MULTIPLY = 4,
	SIR_MATERIAL_IR_SAMPLE_2D = 5,
	SIR_MATERIAL_IR_SELECT = 6,
	SIR_MATERIAL_IR_STATIC_SWITCH = 7,
	SIR_MATERIAL_IR_OUTPUT = 8
} sirMaterialIROp_t;

// Topologically ordered operands; indices always refer to earlier nodes.
// Dynamic parameters never expand the static permutation domain.
typedef struct sirMaterialIRNode_s
{
	sirMaterialIROp_t eOp;
	sirMaterialIRType_t eType;
	uint16_t iA;
	uint16_t iB;
	uint16_t iC;
	uint16_t nParameter;
	uint32_t nSwitchBit;
	uint64_t nValueHash;
} sirMaterialIRNode_t;

typedef struct sirMaterialIRSwitch_s
{
	uint32_t nBit;
	uint32_t nRequires;
	uint32_t nExcludes;
	uint64_t nRequiredFeatures;
	bool bDynamic;
} sirMaterialIRSwitch_t;

typedef struct sirMaterialIRPass_s
{
	sirShaderPass_t ePass;
	uint16_t iOutput;
	uint16_t nStageMask;
	uint64_t nRequiredFeatures;
} sirMaterialIRPass_t;

typedef struct sirMaterialIRGraph_s
{
	uint64_t nAssetId;
	uint64_t nSourceHash;
	const sirMaterialIRNode_t *pNodes;
	uint32_t nNodeCount;
	const sirMaterialIRSwitch_t *pSwitches;
	uint32_t nSwitchCount;
	const sirMaterialIRPass_t *pPasses;
	uint32_t nPassCount;
	const uint8_t *pEncodedBindings;
	uint32_t nBindingCount;
} sirMaterialIRGraph_t;

typedef struct sirMaterialIRCompileOptions_s
{
	uint64_t nAvailableFeatures;
	bool bGenerateUberFallback;
} sirMaterialIRCompileOptions_t;

typedef struct sirMaterialShaderMapEntry_s
{
	sirShaderPass_t ePass;
	uint32_t nStageMask;
	uint32_t nPermutationMask;
	uint64_t nRequiredFeatures;
	uint64_t nCodeHash;
	uint64_t nReflectionSignature;
	bool bUberFallback;
} sirMaterialShaderMapEntry_t;

// Caller-supplied bounded output. A capacity failure reports the full count
// without modifying the output entry array.
typedef struct sirMaterialIRCompileOutput_s
{
	sirMaterialShaderMapEntry_t *pEntries;
	uint32_t nCapacity;
	uint32_t nRequiredCount;
	uint32_t nProducedCount;
} sirMaterialIRCompileOutput_t;

// Offline, allocation-free compiler planning; DXC and platform-specific
// emission are separate owners. Shader-map entries are sorted by pass and
// permutation. Static switches unused by a stage share its code hash.
sirShaderCookResult_t sirMaterialIRCompile( const sirMaterialIRGraph_t& graph, const sirMaterialIRCompileOptions_t& options,
                                            sirMaterialIRCompileOutput_t *pOutput ) noexcept;

// Offline bridge from a validated shader-map entry to the portable pipeline
// recipe consumed by renderer shader caching. No physical device state.
typedef struct sirMaterialIRPipelineSettings_s
{
	uint64_t nPackageManifestHash;
	uint64_t nRenderStateHash;
	uint64_t nColorFormatsHash;
	sirGraphicsFormat_t eDepthFormat;
	uint32_t nSampleCount;
	bool bAlphaMasked;
	bool bDoubleSided;
	bool bWritesVelocity;
} sirMaterialIRPipelineSettings_t;

sirShaderCookResult_t sirMaterialIRBuildPipelineRecipe( const sirMaterialIRGraph_t& graph, const sirMaterialShaderMapEntry_t& entry,
                                                        const sirMaterialIRPipelineSettings_t& settings,
                                                        sirShaderPipelineRecipe_t *pRecipe ) noexcept;

#endif // __SIRENGINE_SIRMATERIALIR_H__
