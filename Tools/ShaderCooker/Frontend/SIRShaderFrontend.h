/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRSHADERFRONTEND_H__
#define __SIRENGINE_SIRSHADERFRONTEND_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>
#include <filesystem>
#include <string>
#include <vector>

// ShaderCooker uses the compact graphics vocabulary while the runtime RHI is
// expressed with strongly typed enums. Keep this translation private to the
// tool; it does not reintroduce a runtime compatibility API.
using sirGraphicsBackend_t = SIRRenderBackend;
using sirGraphicsPipelineModel_t = SIRGraphicsPipelineModel;
using sirGraphicsShaderFormat_t = SIRGraphicsShaderFormat;
constexpr SIRRenderBackend SIR_GRAPHICS_BACKEND_AUTO = SIRRenderBackend::SIR_RENDER_BACKEND_AUTO;
constexpr SIRRenderBackend SIR_GRAPHICS_BACKEND_VULKAN = SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN;
constexpr SIRRenderBackend SIR_GRAPHICS_BACKEND_D3D12 = SIRRenderBackend::SIR_RENDER_BACKEND_D3D12;
constexpr SIRRenderBackend SIR_GRAPHICS_BACKEND_D3D11 = SIRRenderBackend::SIR_RENDER_BACKEND_D3D11;
constexpr SIRRenderBackend SIR_GRAPHICS_BACKEND_D3D10 = SIRRenderBackend::SIR_RENDER_BACKEND_D3D10;
constexpr SIRRenderBackend SIR_GRAPHICS_BACKEND_D3D9 = SIRRenderBackend::SIR_RENDER_BACKEND_D3D9;
constexpr SIRRenderBackend SIR_GRAPHICS_BACKEND_D3D8 = SIRRenderBackend::SIR_RENDER_BACKEND_D3D8;
constexpr SIRRenderBackend SIR_GRAPHICS_BACKEND_D3D7 = SIRRenderBackend::SIR_RENDER_BACKEND_D3D7;
constexpr SIRRenderBackend SIR_GRAPHICS_BACKEND_OPENGL = SIRRenderBackend::SIR_RENDER_BACKEND_OPENGL_PROGRAMMABLE;
constexpr SIRGraphicsPipelineModel SIR_GRAPHICS_PIPELINE_MODEL_MODERN = SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
constexpr SIRGraphicsPipelineModel SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE = SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
constexpr SIRGraphicsShaderFormat SIR_GRAPHICS_SHADER_DXIL = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL;
constexpr SIRGraphicsShaderFormat SIR_GRAPHICS_SHADER_SPIRV = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_SPIRV;
constexpr SIRGraphicsShaderFormat SIR_GRAPHICS_SHADER_GLSL = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_GLSL;
constexpr SIRGraphicsShaderFormat SIR_GRAPHICS_SHADER_DXBC = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXBC;
constexpr SIRGraphicsShaderFormat SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS;
constexpr uint32_t SIR_GRAPHICS_STAGE_VERTEX = 1U << 0;
constexpr uint32_t SIR_GRAPHICS_STAGE_FRAGMENT = 1U << 1;
constexpr uint32_t SIR_GRAPHICS_STAGE_COMPUTE = 1U << 2;
constexpr uint32_t SIR_GRAPHICS_STAGE_GEOMETRY = 1U << 3;
constexpr uint32_t SIR_GRAPHICS_STAGE_TESS_CONTROL = 1U << 4;
constexpr uint32_t SIR_GRAPHICS_STAGE_TESS_EVALUATION = 1U << 5;
constexpr uint32_t SIR_GRAPHICS_STAGE_MESH = 1U << 6;
constexpr uint32_t SIR_GRAPHICS_STAGE_TASK = 1U << 7;

typedef enum sirShaderCookResult_e : uint32_t
{
	SIR_SHADER_COOK_OK = 0,
	SIR_SHADER_COOK_INVALID_ARGUMENT = 1,
	SIR_SHADER_COOK_UNSUPPORTED = 2,
	SIR_SHADER_COOK_IO_ERROR = 3,
	SIR_SHADER_COOK_BAD_DIRECTIVE = 4,
	SIR_SHADER_COOK_INCLUDE_CYCLE = 5,
	SIR_SHADER_COOK_PATH_ESCAPE = 6,
	SIR_SHADER_COOK_CAPACITY_EXCEEDED = 7
} sirShaderCookResult_t;

typedef struct sirShaderCookDefine_s
{
	std::string sName;
	std::string sValue;
} sirShaderCookDefine_t;

typedef struct sirShaderCookRequest_s
{
	std::filesystem::path sRoot;
	std::string sEntryFile;
	std::string sEntryPoint;
	sirGraphicsBackend_t eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	uint32_t nStage = 0;
	uint32_t nShaderMajor = 0; // D3D9 SM2/SM3 selection; 0 uses the backend default.
	std::vector<sirShaderCookDefine_t> szDefines;
} sirShaderCookRequest_t;

typedef struct sirShaderCookFile_s
{
	std::string sPath;
	uint64_t nContentHash = 0;
	std::vector<std::string> szIncludes;
} sirShaderCookFile_t;

typedef struct sirShaderCookManifest_s
{
	std::string sExpandedSource;
	std::string sEntryPoint;
	std::string sProfile;
	std::vector<sirShaderCookFile_t> szFiles;
	std::vector<sirShaderCookDefine_t> szDefines;
	uint64_t nSourceHash = 0;
	uint64_t nClosureHash = 0;
	uint64_t nDefinesHash = 0;
	uint32_t nRecipeVersion = 0;
	sirGraphicsPipelineModel_t eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	sirGraphicsShaderFormat_t eFormat = SIR_GRAPHICS_SHADER_DXIL;
	uint32_t nCompilerKind = 0;
	bool bSpirv = false;
} sirShaderCookManifest_t;

// Deterministic, relative-path-only HLSL source expansion. The resulting
// source must still be compiled by a real offline compiler; it is not DXIL/SPIR-V.
sirShaderCookResult_t sirShaderCookPreprocess( const sirShaderCookRequest_t& request, sirShaderCookManifest_t *pManifest,
                                               std::string *pDiagnostic );

#endif // __SIRENGINE_SIRSHADERFRONTEND_H__
