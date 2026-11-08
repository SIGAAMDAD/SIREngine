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

#ifndef __SIRENGINE_SIRPACKCOOKER_H__
#define __SIRENGINE_SIRPACKCOOKER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRPackFormat.h>

typedef struct sirPackCookResource_s
{
	SIRResourceKey key;
	uint64_t nRevision;
	uint32_t nFormatId;
	uint32_t nFirstChunk;
	uint32_t nChunkCount;
	uint32_t nFirstDependency;
	uint32_t nDependencyCount;
	uint16_t nSchemaVersion;
	uint8_t nKind;
} sirPackCookResource_t;

typedef struct sirPackCookChunk_s
{
	uint32_t nChunkId;
	uint32_t nShardIndex;
	const uint8_t *pBytes;
	uint64_t nBytes;
	uint32_t nAlignment;
} sirPackCookChunk_t;

typedef struct sirPackCookShard_s
{
	uint8_t *pBytes;
	uint64_t nCapacity;
} sirPackCookShard_t;

typedef struct sirPackCookDesc_s
{
	uint64_t nPackageId;
	uint64_t nGeneration;
	const sirPackCookResource_t *pResources;
	uint32_t nResourceCount;
	const sirPackCookChunk_t *pChunks;
	uint32_t nChunkCount;
	const sirPackDependency_t *pDependencies;
	uint32_t nDependencyCount;
	const sirPackCookShard_t *pShards;
	uint32_t nShardCount;
	void *pScratch;
	uint64_t nScratchBytes;
} sirPackCookDesc_t;

// Offline-only deterministic writer. Does not open files or allocate memory;
// caller atomically installs completed .sirm and .sirp outputs after verification.
class SIRPackCooker
{
public:
	static uint64_t requiredCatalogBytes( const sirPackCookDesc_t& desc ) noexcept;
	static uint64_t requiredScratch( const sirPackCookDesc_t& desc ) noexcept;
	static sirPackResult_t build( const sirPackCookDesc_t& desc, uint8_t *pCatalog, uint64_t nCatalogCapacity ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRPACKCOOKER_H__
