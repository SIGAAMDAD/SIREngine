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

#ifndef __SIRENGINE_SIRSHADERMATERIAL_H__
#define __SIRENGINE_SIRSHADERMATERIAL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Shaders/SIRShaderContracts.h>

// The material wire format is independent of graphics ABI v1. All multibyte
// wire fields are explicitly little-endian, not in-memory C++ structs.
constexpr uint32_t SIR_SHADER_MATERIAL_MAGIC = 0x31544d53U;
constexpr uint16_t SIR_SHADER_MATERIAL_SCHEMA = 1;
constexpr uint32_t SIR_SHADER_MATERIAL_HEADER_BYTES = 48;
constexpr uint32_t SIR_SHADER_MATERIAL_PASS_BYTES = 32;
constexpr uint32_t SIR_SHADER_MATERIAL_MAX_PASSES = 16;
constexpr uint32_t SIR_SHADER_MATERIAL_MAX_DEPENDENCIES = 64;
constexpr uint32_t SIR_SHADER_MATERIAL_NO_SHADER = UINT32_MAX;

typedef struct sirShaderMaterialHeader_s
{
	uint32_t nMagic;
	uint16_t nSchema;
	uint16_t nHeaderBytes;
	uint32_t nTotalBytes;
	uint32_t nDependencyCount;
	uint32_t nPassCount;
	uint64_t nAssetId;
	uint64_t nSourceRevision;
	uint64_t nManifestHash;
	uint32_t nReserved;
} sirShaderMaterialHeader_t;

typedef struct sirShaderMaterialPassRecord_s
{
	uint32_t nPass;
	uint32_t iShaderDependency;
	uint64_t nRequiredFeatures;
	uint64_t nPermutationKey;
	uint64_t nRenderStateHash;
} sirShaderMaterialPassRecord_t;

static_assert( sizeof( sirShaderMaterialHeader_t ) == 56, "Material header memory layout changed; wire header remains 48 bytes" );
static_assert( sizeof( sirShaderMaterialPassRecord_t ) == 32, "Material pass memory layout changed" );

sirGraphicsResult_t sirShaderEncodeMaterialHeader( const sirShaderMaterialHeader_t& header, void *pBytes, uint64_t nCapacity ) noexcept;
sirGraphicsResult_t sirShaderDecodeMaterialHeader( const void *pBytes, uint64_t nLength, sirShaderMaterialHeader_t *pHeader ) noexcept;
sirGraphicsResult_t sirShaderEncodeMaterialPass( const sirShaderMaterialPassRecord_t& pass, void *pBytes, uint64_t nCapacity ) noexcept;
sirGraphicsResult_t sirShaderDecodeMaterialPass( const void *pBytes, uint64_t nLength, sirShaderMaterialPassRecord_t *pPass ) noexcept;
sirGraphicsResult_t sirShaderBuildMaterialHash( const sirShaderMaterialHeader_t& header, const void *pPassBytes, uint64_t nLength,
                                                uint64_t *pHash ) noexcept;
sirGraphicsResult_t sirShaderValidateMaterial( const void *pBytes, uint64_t nLength ) noexcept;

#endif // __SIRENGINE_SIRSHADERMATERIAL_H__
