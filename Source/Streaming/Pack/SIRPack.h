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

#ifndef __SIR_PACK_H__
#define __SIR_PACK_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/File/SIRPlatformFileMapping.h>
#include <Core/Platform/Shared/File/SIRPlatformRootDirectory.h>
#include <Streaming/Pack/SIRPackCatalog.h>
#include <Streaming/Pack/SIRPackFormat.h>
#include <Streaming/Pack/SIRPackRuntimeTypes.h>
#include <Streaming/Pack/SIRPackShard.h>

typedef struct sirPackOpenDesc_s
{
	SIRPlatformRootDirectory *pRoot;
	const char *pszCatalogPath;
	const char *const *ppszShardPaths;
	uint32_t nShardCount;
	SIRPackShard *pShardStorage;
	uint32_t nShardCapacity;
	void *pCatalogStorage;
	uint64_t nCatalogStorageBytes;
	void *pValidationScratch;
	uint64_t nValidationScratchBytes;
} sirPackOpenDesc_t;

class SIRPack final
{
public:
	SIRPack( void ) noexcept;
	SIRPack( const SIRPack& ) = delete;
	SIRPack& operator=( const SIRPack& ) = delete;
	~SIRPack() noexcept;

	bool isOpen( void ) const noexcept;
	uint64_t getPackageId( void ) const noexcept;
	uint64_t getGeneration( void ) const noexcept;
	const SIRPackCatalog& getCatalog( void ) const noexcept;

	SIRPackResult open( const sirPackOpenDesc_t& desc ) noexcept;
	SIRPackResult close( void ) noexcept;
	SIRPackResult findResource( const SIRResourceKey& key, sirPackResource_t *pResource ) const noexcept;
	SIRPackResult resolveChunk( const SIRResourceKey& key, uint32_t nChunkId, sirPackChunkSource_t *pSource ) const noexcept;
	SIRPackResult readAt( uint32_t iShard, uint64_t nOffset, SIRResourceMutableBytes bytes, uint64_t *pTransferred ) const noexcept;

private:
	SIRPlatformFile m_CatalogFile;
	SIRPlatformFileMapping m_CatalogMapping;
	SIRPackCatalog m_Catalog;
	SIRPackShard *m_pShards;
	uint32_t m_nShardCount;
	uint64_t m_nPackageId;
	uint64_t m_nGeneration;
	bool m_bOpen;
};

#endif // __SIR_PACK_H__
