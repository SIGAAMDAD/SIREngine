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

#include <Resources/SIRPackCatalog.h>
#include <Tools/SIRPackCooker/SIRPackCooker.h>

#include <limits.h>
#include <string.h>

static int32_t sirPackCookCompareKey( const SIRResourceKey& a, const SIRResourceKey& b ) noexcept
{
	if ( a.m_nAssetId != b.m_nAssetId ) {
		return a.m_nAssetId < b.m_nAssetId ? -1 : 1;
	}
	if ( a.m_nTypeId != b.m_nTypeId ) {
		return a.m_nTypeId < b.m_nTypeId ? -1 : 1;
	}
	return a.m_nVariantId == b.m_nVariantId ? 0 : ( a.m_nVariantId < b.m_nVariantId ? -1 : 1 );
}

static uint32_t sirPackCookFindResource( const sirPackCookDesc_t& desc, const SIRResourceKey& key ) noexcept
{
	uint32_t nLo = 0;
	uint32_t nHi = desc.nResourceCount;
	while ( nLo < nHi ) {
		const uint32_t nMiddle = nLo + ( nHi - nLo ) / 2;
		if ( sirPackCookCompareKey( desc.pResources[nMiddle].key, key ) < 0 ) {
			nLo = nMiddle + 1;
		} else {
			nHi = nMiddle;
		}
	}
	return nLo < desc.nResourceCount && sirPackCookCompareKey( desc.pResources[nLo].key, key ) == 0 ? nLo : desc.nResourceCount;
}

// Reject aliasing before any output is written. Even a completely valid cooked
// input must not be overwritten by a sibling output or caller scratch buffer.
static bool sirPackCookOverlap( const void *pA, uint64_t nA, const void *pB, uint64_t nB ) noexcept
{
	if ( pA == nullptr || pB == nullptr || nA == 0 || nB == 0 ) {
		return false;
	}
	const uintptr_t nLeft = ( uintptr_t )pA;
	const uintptr_t nRight = ( uintptr_t )pB;
	if ( nA > UINTPTR_MAX - nLeft || nB > UINTPTR_MAX - nRight ) {
		return true;
	}
	return nLeft < nRight + nB && nRight < nLeft + nA;
}

uint64_t SIRPackCooker::requiredCatalogBytes( const sirPackCookDesc_t& desc ) noexcept
{
	if ( desc.nShardCount > SIR_PACK_MAX_SHARDS || desc.nResourceCount > SIR_PACK_MAX_RESOURCES || desc.nChunkCount > SIR_PACK_MAX_CHUNKS ||
	     desc.nDependencyCount > SIR_PACK_MAX_CHUNKS ) {
		return 0;
	}
	return SIR_PACK_HEADER_BYTES + ( uint64_t )desc.nShardCount * SIR_PACK_SHARD_RECORD_BYTES +
	       ( uint64_t )desc.nResourceCount * SIR_PACK_RESOURCE_RECORD_BYTES + ( uint64_t )desc.nChunkCount * SIR_PACK_CHUNK_RECORD_BYTES +
	       ( uint64_t )desc.nDependencyCount * SIR_PACK_DEPENDENCY_RECORD_BYTES;
}

uint64_t SIRPackCooker::requiredScratch( const sirPackCookDesc_t& desc ) noexcept
{
	return SIRPackCatalog::requiredScratch( desc.nResourceCount, desc.nShardCount );
}

typedef struct sirPackCookScratch_s
{
	uint64_t *pCursors;
	uint32_t *pStack;
	uint32_t *pNext;
} sirPackCookScratch_t;

static sirPackResult_t sirPackCookValidate( const sirPackCookDesc_t& desc, uint64_t nCatalogCapacity,
                                            const sirPackCookScratch_t& scratch ) noexcept
{
	uint64_t *pCursors = scratch.pCursors;
	uint32_t *pStack = scratch.pStack;
	uint32_t *pNext = scratch.pNext;
	if ( desc.nPackageId == 0 || desc.nGeneration == 0 || desc.nShardCount == 0 || desc.nResourceCount == 0 || desc.nChunkCount == 0 ||
	     desc.pResources == nullptr || desc.pChunks == nullptr || desc.pShards == nullptr ||
	     ( desc.nDependencyCount != 0 && desc.pDependencies == nullptr ) || SIRPackCooker::requiredCatalogBytes( desc ) == 0 ||
	     SIRPackCooker::requiredCatalogBytes( desc ) > nCatalogCapacity ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < desc.nShardCount; ++i ) {
		if ( desc.pShards[i].pBytes == nullptr || desc.pShards[i].nCapacity < SIR_PACK_SHARD_HEADER_BYTES ) {
			return SIR_PACK_OUT_OF_CAPACITY;
		}
		pCursors[i] = SIR_PACK_SHARD_HEADER_BYTES;
	}
	uint32_t nNextChunk = 0;
	uint32_t nNextDep = 0;
	for ( uint32_t i = 0; i < desc.nResourceCount; ++i ) {
		const sirPackCookResource_t& resource = desc.pResources[i];
		if ( resource.key.m_nAssetId == 0 || resource.nRevision == 0 || resource.nSchemaVersion == 0 ||
		     resource.nKind > SIR_RESOURCE_KIND_COMPOSITION ||
		     ( i != 0 && sirPackCookCompareKey( desc.pResources[i - 1].key, resource.key ) >= 0 ) || resource.nChunkCount == 0 ||
		     resource.nFirstChunk != nNextChunk || resource.nFirstDependency != nNextDep ||
		     resource.nChunkCount > desc.nChunkCount - nNextChunk || resource.nDependencyCount > desc.nDependencyCount - nNextDep ) {
			return SIR_PACK_INVALID_FORMAT;
		}
		uint32_t nPriorId = 0;
		for ( uint32_t j = 0; j < resource.nChunkCount; ++j ) {
			const sirPackCookChunk_t& chunk = desc.pChunks[resource.nFirstChunk + j];
			if ( chunk.nShardIndex >= desc.nShardCount || chunk.pBytes == nullptr || chunk.nBytes == 0 ||
			     chunk.nBytes > SIR_PACK_MAX_DECODED_CHUNK_BYTES || chunk.nAlignment == 0 || chunk.nAlignment > 4096 ||
			     ( chunk.nAlignment & ( chunk.nAlignment - 1 ) ) != 0 || ( j != 0 && chunk.nChunkId <= nPriorId ) ) {
				return SIR_PACK_INVALID_FORMAT;
			}
			const uint64_t nCurrent = pCursors[chunk.nShardIndex];
			const uint64_t nMask = ( uint64_t )chunk.nAlignment - 1;
			if ( nCurrent > UINT64_MAX - nMask ) {
				return SIR_PACK_INVALID_FORMAT;
			}
			const uint64_t nAligned = ( nCurrent + nMask ) & ~nMask;
			if ( nAligned > desc.pShards[chunk.nShardIndex].nCapacity ||
			     chunk.nBytes > desc.pShards[chunk.nShardIndex].nCapacity - nAligned ) {
				return SIR_PACK_OUT_OF_CAPACITY;
			}
			pCursors[chunk.nShardIndex] = nAligned + chunk.nBytes;
			nPriorId = chunk.nChunkId;
		}
		for ( uint32_t j = 0; j < resource.nDependencyCount; ++j ) {
			const sirPackDependency_t& dep = desc.pDependencies[resource.nFirstDependency + j];
			if ( dep.key.m_nAssetId == 0 || dep.eKind > SIR_RESOURCE_EDGE_KIND_STREAMED ||
			     ( j != 0 && sirPackCookCompareKey( desc.pDependencies[resource.nFirstDependency + j - 1].key, dep.key ) >= 0 ) ||
			     ( dep.eKind == SIR_RESOURCE_EDGE_KIND_REQUIRED && sirPackCookFindResource( desc, dep.key ) == desc.nResourceCount ) ) {
				return SIR_PACK_INVALID_FORMAT;
			}
		}
		nNextChunk += resource.nChunkCount;
		nNextDep += resource.nDependencyCount;
	}
	if ( nNextChunk != desc.nChunkCount || nNextDep != desc.nDependencyCount ) {
		return SIR_PACK_INVALID_FORMAT;
	}
	uint8_t *pColors = ( uint8_t * )( pNext + desc.nResourceCount );
	memset( pColors, 0, desc.nResourceCount );
	for ( uint32_t i = 0; i < desc.nResourceCount; ++i ) {
		if ( pColors[i] != 0 ) {
			continue;
		}
		uint32_t nDepth = 1;
		pStack[0] = i;
		pNext[0] = 0;
		pColors[i] = 1;
		while ( nDepth != 0 ) {
			const uint32_t iCurrent = pStack[nDepth - 1];
			const sirPackCookResource_t& resource = desc.pResources[iCurrent];
			if ( pNext[nDepth - 1] == resource.nDependencyCount ) {
				pColors[iCurrent] = 2;
				--nDepth;
				continue;
			}
			const sirPackDependency_t& dep = desc.pDependencies[resource.nFirstDependency + pNext[nDepth - 1]];
			++pNext[nDepth - 1];
			const uint32_t iTarget = sirPackCookFindResource( desc, dep.key );
			if ( iTarget == desc.nResourceCount || pColors[iTarget] == 2 ) {
				continue;
			}
			if ( pColors[iTarget] == 1 ) {
				return SIR_PACK_CYCLIC_DEPENDENCY;
			}
			pStack[nDepth] = iTarget;
			pNext[nDepth] = 0;
			pColors[iTarget] = 1;
			++nDepth;
		}
	}
	return SIR_PACK_OK;
}

sirPackResult_t SIRPackCooker::build( const sirPackCookDesc_t& desc, uint8_t *pCatalog, uint64_t nCatalogCapacity ) noexcept
{
	if ( pCatalog == nullptr || desc.pScratch == nullptr || nCatalogCapacity > SIZE_MAX || desc.nScratchBytes < requiredScratch( desc ) ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	const uint64_t nExpectedCatalog = requiredCatalogBytes( desc );
	if ( nExpectedCatalog == 0 || ( uintptr_t )desc.pScratch > UINTPTR_MAX - 7 ||
	     sirPackCookOverlap( pCatalog, nExpectedCatalog, desc.pScratch, desc.nScratchBytes ) ||
	     sirPackCookOverlap( desc.pScratch, desc.nScratchBytes, desc.pResources,
	                         ( uint64_t )desc.nResourceCount * sizeof( sirPackCookResource_t ) ) ||
	     sirPackCookOverlap( desc.pScratch, desc.nScratchBytes, desc.pChunks,
	                         ( uint64_t )desc.nChunkCount * sizeof( sirPackCookChunk_t ) ) ||
	     sirPackCookOverlap( desc.pScratch, desc.nScratchBytes, desc.pDependencies,
	                         ( uint64_t )desc.nDependencyCount * sizeof( sirPackDependency_t ) ) ||
	     sirPackCookOverlap( desc.pScratch, desc.nScratchBytes, desc.pShards,
	                         ( uint64_t )desc.nShardCount * sizeof( sirPackCookShard_t ) ) ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	if ( desc.pShards != nullptr ) {
		for ( uint32_t i = 0; i < desc.nShardCount && i < SIR_PACK_MAX_SHARDS; ++i ) {
			if ( sirPackCookOverlap( desc.pScratch, desc.nScratchBytes, desc.pShards[i].pBytes, desc.pShards[i].nCapacity ) ) {
				return SIR_PACK_INVALID_ARGUMENT;
			}
		}
	}
	if ( desc.pChunks != nullptr ) {
		for ( uint32_t i = 0; i < desc.nChunkCount && i < SIR_PACK_MAX_CHUNKS; ++i ) {
			if ( sirPackCookOverlap( desc.pScratch, desc.nScratchBytes, desc.pChunks[i].pBytes, desc.pChunks[i].nBytes ) ) {
				return SIR_PACK_INVALID_ARGUMENT;
			}
		}
	}
	uint8_t *pScratch = ( uint8_t * )( ( ( uintptr_t )desc.pScratch + 7 ) & ~( uintptr_t )7 );
	uint64_t *pCursors = ( uint64_t * )pScratch;
	uint32_t *pStack = ( uint32_t * )( pScratch + ( uint64_t )desc.nShardCount * 8 );
	uint32_t *pNext = pStack + desc.nResourceCount;
	const sirPackResult_t eValid = sirPackCookValidate( desc, nCatalogCapacity, { pCursors, pStack, pNext } );
	if ( eValid != SIR_PACK_OK ) {
		return eValid;
	}
	const uint64_t nCatalogBytes = requiredCatalogBytes( desc );
	if ( sirPackCookOverlap( pCatalog, nCatalogBytes, desc.pScratch, desc.nScratchBytes ) ||
	     sirPackCookOverlap( pCatalog, nCatalogBytes, desc.pResources, ( uint64_t )desc.nResourceCount * sizeof( *desc.pResources ) ) ||
	     sirPackCookOverlap( pCatalog, nCatalogBytes, desc.pChunks, ( uint64_t )desc.nChunkCount * sizeof( *desc.pChunks ) ) ||
	     sirPackCookOverlap( pCatalog, nCatalogBytes, desc.pDependencies,
	                         ( uint64_t )desc.nDependencyCount * sizeof( *desc.pDependencies ) ) ||
	     sirPackCookOverlap( pCatalog, nCatalogBytes, desc.pShards, ( uint64_t )desc.nShardCount * sizeof( *desc.pShards ) ) ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < desc.nShardCount; ++i ) {
		const uint8_t *pShard = desc.pShards[i].pBytes;
		const uint64_t nShardBytes = pCursors[i];
		if ( sirPackCookOverlap( pCatalog, nCatalogBytes, pShard, nShardBytes ) ||
		     sirPackCookOverlap( pShard, nShardBytes, desc.pScratch, desc.nScratchBytes ) ||
		     sirPackCookOverlap( pShard, nShardBytes, desc.pResources, ( uint64_t )desc.nResourceCount * sizeof( *desc.pResources ) ) ||
		     sirPackCookOverlap( pShard, nShardBytes, desc.pChunks, ( uint64_t )desc.nChunkCount * sizeof( *desc.pChunks ) ) ||
		     sirPackCookOverlap( pShard, nShardBytes, desc.pDependencies,
		                         ( uint64_t )desc.nDependencyCount * sizeof( *desc.pDependencies ) ) ||
		     sirPackCookOverlap( pShard, nShardBytes, desc.pShards, ( uint64_t )desc.nShardCount * sizeof( *desc.pShards ) ) ) {
			return SIR_PACK_INVALID_ARGUMENT;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( sirPackCookOverlap( pShard, nShardBytes, desc.pShards[j].pBytes, pCursors[j] ) ) {
				return SIR_PACK_INVALID_ARGUMENT;
			}
		}
		for ( uint32_t j = 0; j < desc.nChunkCount; ++j ) {
			if ( sirPackCookOverlap( pShard, nShardBytes, desc.pChunks[j].pBytes, desc.pChunks[j].nBytes ) ) {
				return SIR_PACK_INVALID_ARGUMENT;
			}
		}
	}
	for ( uint32_t i = 0; i < desc.nChunkCount; ++i ) {
		if ( sirPackCookOverlap( pCatalog, nCatalogBytes, desc.pChunks[i].pBytes, desc.pChunks[i].nBytes ) ) {
			return SIR_PACK_INVALID_ARGUMENT;
		}
	}
	memset( pCatalog, 0, ( size_t )nCatalogBytes );
	memcpy( pCatalog, "SIRM", 4 );
	sirPackWrite16( pCatalog + 4, SIR_PACK_VERSION );
	sirPackWrite16( pCatalog + 6, ( uint16_t )SIR_PACK_HEADER_BYTES );
	sirPackWrite32( pCatalog + 8, SIR_PACK_ENDIAN_MARKER );
	sirPackWrite64( pCatalog + 16, desc.nPackageId );
	sirPackWrite64( pCatalog + 24, desc.nGeneration );
	sirPackWrite64( pCatalog + 32, nCatalogBytes );
	sirPackWrite32( pCatalog + 40, desc.nShardCount );
	sirPackWrite32( pCatalog + 44, desc.nResourceCount );
	sirPackWrite32( pCatalog + 48, desc.nChunkCount );
	sirPackWrite32( pCatalog + 52, desc.nDependencyCount );
	const uint64_t nShardOffset = SIR_PACK_HEADER_BYTES;
	const uint64_t nResourceOffset = nShardOffset + ( uint64_t )desc.nShardCount * SIR_PACK_SHARD_RECORD_BYTES;
	const uint64_t nChunkOffset = nResourceOffset + ( uint64_t )desc.nResourceCount * SIR_PACK_RESOURCE_RECORD_BYTES;
	const uint64_t nDepOffset = nChunkOffset + ( uint64_t )desc.nChunkCount * SIR_PACK_CHUNK_RECORD_BYTES;
	sirPackWrite64( pCatalog + 56, nShardOffset );
	sirPackWrite64( pCatalog + 64, nResourceOffset );
	sirPackWrite64( pCatalog + 72, nChunkOffset );
	sirPackWrite64( pCatalog + 80, nDepOffset );
	for ( uint32_t i = 0; i < desc.nShardCount; ++i ) {
		uint8_t *p = desc.pShards[i].pBytes;
		memset( p, 0, ( size_t )pCursors[i] );
		memcpy( p, "SIRP", 4 );
		sirPackWrite16( p + 4, SIR_PACK_VERSION );
		sirPackWrite16( p + 6, ( uint16_t )SIR_PACK_SHARD_HEADER_BYTES );
		sirPackWrite32( p + 8, SIR_PACK_ENDIAN_MARKER );
		sirPackWrite32( p + 12, i );
		sirPackWrite64( p + 16, desc.nPackageId );
		sirPackWrite64( p + 24, desc.nGeneration );
		sirPackWrite64( p + 32, pCursors[i] );
		uint8_t *pRecord = pCatalog + nShardOffset + ( uint64_t )i * SIR_PACK_SHARD_RECORD_BYTES;
		sirPackWrite32( pRecord, i );
		sirPackWrite64( pRecord + 8, pCursors[i] );
		pCursors[i] = SIR_PACK_SHARD_HEADER_BYTES;
	}
	for ( uint32_t i = 0; i < desc.nResourceCount; ++i ) {
		const sirPackCookResource_t& r = desc.pResources[i];
		uint8_t *p = pCatalog + nResourceOffset + ( uint64_t )i * SIR_PACK_RESOURCE_RECORD_BYTES;
		sirPackWrite64( p, r.key.m_nAssetId );
		sirPackWrite32( p + 8, r.key.m_nTypeId );
		sirPackWrite32( p + 12, r.key.m_nVariantId );
		sirPackWrite64( p + 16, r.nRevision );
		uint64_t nLogicalBytes = 0;
		for ( uint32_t j = 0; j < r.nChunkCount; ++j ) {
			nLogicalBytes += desc.pChunks[r.nFirstChunk + j].nBytes;
		}
		sirPackWrite64( p + 24, nLogicalBytes );
		sirPackWrite32( p + 32, r.nFormatId );
		sirPackWrite32( p + 36, r.nFirstChunk );
		sirPackWrite32( p + 40, r.nChunkCount );
		sirPackWrite32( p + 44, r.nFirstDependency );
		sirPackWrite32( p + 48, r.nDependencyCount );
		sirPackWrite16( p + 52, r.nSchemaVersion );
		p[54] = r.nKind;
	}
	for ( uint32_t i = 0; i < desc.nChunkCount; ++i ) {
		const sirPackCookChunk_t& chunk = desc.pChunks[i];
		const uint64_t nMask = ( uint64_t )chunk.nAlignment - 1;
		const uint64_t nOffset = ( pCursors[chunk.nShardIndex] + nMask ) & ~nMask;
		uint8_t *pShard = desc.pShards[chunk.nShardIndex].pBytes;
		memcpy( pShard + nOffset, chunk.pBytes, ( size_t )chunk.nBytes );
		pCursors[chunk.nShardIndex] = nOffset + chunk.nBytes;
		uint8_t *p = pCatalog + nChunkOffset + ( uint64_t )i * SIR_PACK_CHUNK_RECORD_BYTES;
		sirPackWrite32( p, chunk.nChunkId );
		sirPackWrite32( p + 4, chunk.nShardIndex );
		sirPackWrite64( p + 8, nOffset );
		sirPackWrite64( p + 16, chunk.nBytes );
		sirPackWrite64( p + 24, chunk.nBytes );
		sirPackSha256( chunk.pBytes, chunk.nBytes, p + 32 );
		sirPackWrite16( p + 64, SIR_PACK_CODEC_NONE );
		sirPackWrite16( p + 66, 1 );
		sirPackWrite32( p + 68, chunk.nAlignment );
	}
	for ( uint32_t i = 0; i < desc.nDependencyCount; ++i ) {
		const sirPackDependency_t& dep = desc.pDependencies[i];
		uint8_t *p = pCatalog + nDepOffset + ( uint64_t )i * SIR_PACK_DEPENDENCY_RECORD_BYTES;
		sirPackWrite64( p, dep.key.m_nAssetId );
		sirPackWrite32( p + 8, dep.key.m_nTypeId );
		sirPackWrite32( p + 12, dep.key.m_nVariantId );
		p[16] = ( uint8_t )dep.eKind;
	}
	for ( uint32_t i = 0; i < desc.nShardCount; ++i ) {
		uint8_t *p = pCatalog + nShardOffset + ( uint64_t )i * SIR_PACK_SHARD_RECORD_BYTES;
		sirPackSha256( desc.pShards[i].pBytes, pCursors[i], p + 16 );
	}
	sirPackCatalogDigest( pCatalog, nCatalogBytes, pCatalog + 88 );
	SIRPackCatalog catalog;
	return catalog.open( pCatalog, nCatalogBytes, desc.pScratch, desc.nScratchBytes );
}
