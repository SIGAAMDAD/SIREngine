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

#ifndef __SIRENGINE_SIRSHADERARCHIVE_H__
#define __SIRENGINE_SIRSHADERARCHIVE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Shaders/SIRShaderContracts.h>
#include <Tools/ShaderCooker/Frontend/SIRShaderFrontend.h>

constexpr uint32_t SIR_SHADER_ARCHIVE_MAGIC = 0x31414853U;
constexpr uint32_t SIR_SHADER_ARCHIVE_SCHEMA = 1;
constexpr uint32_t SIR_SHADER_ARCHIVE_TARGET_ABI_V1 = 1;
constexpr uint32_t SIR_SHADER_ARCHIVE_HEADER_BYTES = 64;
constexpr uint32_t SIR_SHADER_ARCHIVE_ENTRY_BYTES = 32;
constexpr uint32_t SIR_SHADER_ARCHIVE_UNIQUE_BYTES = 24;
constexpr uint32_t SIR_SHADER_ARCHIVE_MAX_ENTRIES = 128;
constexpr uint64_t SIR_SHADER_ARCHIVE_MAX_BYTES = 64ULL * 1024ULL * 1024ULL;

// Offline inputs are borrowed. An archive physically stores each distinct
// (format, exact-byte-string) stage blob once and keeps references separately.
typedef struct sirShaderArchiveStageInput_s
{
	sirGraphicsShaderFormat_t eFormat;
	uint32_t nStageMask;
	uint64_t nEntryPointHash;
	const uint8_t *pBytes;
	uint32_t nByteCount;
} sirShaderArchiveStageInput_t;

typedef struct sirShaderArchiveBuildDesc_s
{
	uint64_t nArchiveId;
	uint32_t nTargetAbiVersion;
	const sirShaderArchiveStageInput_t *pEntries;
	uint32_t nEntryCount;
} sirShaderArchiveBuildDesc_t;

typedef struct sirShaderArchiveBuildResult_s
{
	uint64_t nRequiredBytes;
	uint32_t nUniqueCount;
	uint64_t nManifestHash;
} sirShaderArchiveBuildResult_t;

// Source pointer is borrowed from the caller's validated packed archive,
// or from an active GRDG read scope when that archive is GRDG-owned.
typedef struct sirShaderArchiveStageView_s
{
	sirGraphicsShaderFormat_t eFormat;
	uint32_t nStageMask;
	uint64_t nEntryPointHash;
	uint64_t nBinaryHash;
	const uint8_t *pBytes;
	uint32_t nByteCount;
} sirShaderArchiveStageView_t;

sirShaderCookResult_t sirShaderArchiveBuild( const sirShaderArchiveBuildDesc_t& desc, void *pDestination, uint64_t nCapacity,
                                             sirShaderArchiveBuildResult_t *pOut ) noexcept;
sirShaderCookResult_t sirShaderArchiveValidate( const void *pArchive, uint64_t nBytes ) noexcept;
sirShaderCookResult_t sirShaderArchiveResolve( const void *pArchive, uint64_t nBytes, uint32_t iEntry,
                                               sirShaderArchiveStageView_t *pOut ) noexcept;

#endif // __SIRENGINE_SIRSHADERARCHIVE_H__
