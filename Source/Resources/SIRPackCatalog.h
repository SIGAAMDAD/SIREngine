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

#ifndef __SIR_PACKCATALOG_H__
#define __SIR_PACKCATALOG_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRPackFormat.h>

// Non-owning immutable view into one validated SIRM generation. The source
// bytes must be pinned by the future VFS/IO source owner while this view lives.
// Scratch is temporary caller-owned space, never retained after open().
class SIRPackCatalog
{
public:
	SIRPackCatalog( void ) noexcept;

	bool isOpen( void ) const noexcept;
	uint32_t getResourceCount( void ) const noexcept;
	uint32_t getChunkCount( void ) const noexcept;
	uint32_t getShardCount( void ) const noexcept;
	uint32_t getDependencyCount( void ) const noexcept;
	uint64_t getGeneration( void ) const noexcept;
	uint64_t getPackageId( void ) const noexcept;

	static uint64_t requiredScratch( uint32_t nResources, uint32_t nShards ) noexcept;
	SIRPackResult open( const void *pBytes, uint64_t nBytes, void *pScratch, uint64_t nScratchBytes ) noexcept;
	void reset( void ) noexcept;
	SIRPackResult findResource( const SIRResourceKey& key, sirPackResource_t *pResource ) const noexcept;
	SIRPackResult resourceAt( uint32_t iResource, sirPackResource_t *pResource ) const noexcept;
	SIRPackResult chunkAt( uint32_t iChunk, sirPackChunk_t *pChunk ) const noexcept;
	SIRPackResult shardAt( uint32_t iShard, sirPackShard_t *pShard ) const noexcept;
	SIRPackResult dependencyAt( uint32_t iEdge, sirPackDependency_t *pEdge ) const noexcept;
	SIRPackResult validateShard( uint32_t iShard, const void *pBytes, uint64_t nBytes ) const noexcept;
	SIRPackResult verifyDecodedChunk( uint32_t iChunk, const void *pBytes, uint64_t nBytes ) const noexcept;

protected:
private:
	const uint8_t *m_pBytes;
	uint64_t m_nBytes;
	uint64_t m_nPackageId;
	uint64_t m_nGeneration;
	uint64_t m_nShardOffset;
	uint64_t m_nResourceOffset;
	uint64_t m_nChunkOffset;
	uint64_t m_nDependencyOffset;
	uint32_t m_nShards;
	uint32_t m_nResources;
	uint32_t m_nChunks;
	uint32_t m_nDependencies;

	uint32_t findIndex( const SIRResourceKey& key ) const noexcept;
	SIRPackResult validateTables( uint8_t *pScratch, uint64_t nScratchBytes ) const noexcept;
};

#endif // __SIR_PACKCATALOG_H__
