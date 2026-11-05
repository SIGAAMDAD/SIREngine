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

#ifndef __SIRENGINE_SIRSHADERFRONTEND_H__
#define __SIRENGINE_SIRSHADERFRONTEND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>
#include <filesystem>
#include <string>
#include <vector>

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
	bool bSpirv = false;
} sirShaderCookManifest_t;

// Deterministic, relative-path-only HLSL source expansion. The resulting
// source must still be compiled by a real offline compiler; it is not DXIL/SPIR-V.
sirShaderCookResult_t sirShaderCookPreprocess( const sirShaderCookRequest_t& request, sirShaderCookManifest_t *pManifest,
                                               std::string *pDiagnostic );

#endif // __SIRENGINE_SIRSHADERFRONTEND_H__
