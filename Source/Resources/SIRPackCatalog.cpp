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

#include <limits.h>
#include <string.h>

static int32_t sirPackCompareKey( const SIRResourceKey& a, const SIRResourceKey& b ) noexcept
{
	if ( a.m_nAssetId != b.m_nAssetId ) {
		return a.m_nAssetId < b.m_nAssetId ? -1 : 1;
	}
	if ( a.m_nTypeId != b.m_nTypeId ) {
		return a.m_nTypeId < b.m_nTypeId ? -1 : 1;
	}
	if ( a.m_nVariantId != b.m_nVariantId ) {
		return a.m_nVariantId < b.m_nVariantId ? -1 : 1;
	}
	return 0;
}

static bool sirPackZeroBytes( const uint8_t *pBytes, uint32_t nBytes ) noexcept
{
	for ( uint32_t i = 0; i < nBytes; ++i ) {
		if ( pBytes[i] != 0 ) {
			return false;
		}
	}
	return true;
}

SIRPackCatalog::SIRPackCatalog( void ) noexcept
    : m_pBytes( nullptr ), m_nBytes( 0 ), m_nPackageId( 0 ), m_nGeneration( 0 ), m_nShardOffset( 0 ), m_nResourceOffset( 0 ),
      m_nChunkOffset( 0 ), m_nDependencyOffset( 0 ), m_nShards( 0 ), m_nResources( 0 ), m_nChunks( 0 ), m_nDependencies( 0 )
{
}

bool SIRPackCatalog::isOpen( void ) const noexcept
{
	return m_pBytes != nullptr;
}
uint32_t SIRPackCatalog::getResourceCount( void ) const noexcept
{
	return m_nResources;
}
uint32_t SIRPackCatalog::getChunkCount( void ) const noexcept
{
	return m_nChunks;
}
uint32_t SIRPackCatalog::getShardCount( void ) const noexcept
{
	return m_nShards;
}
uint32_t SIRPackCatalog::getDependencyCount( void ) const noexcept
{
	return m_nDependencies;
}
uint64_t SIRPackCatalog::getGeneration( void ) const noexcept
{
	return m_nGeneration;
}
uint64_t SIRPackCatalog::getPackageId( void ) const noexcept
{
	return m_nPackageId;
}

uint64_t SIRPackCatalog::requiredScratch( uint32_t nResources, uint32_t nShards ) noexcept
{
	return ( uint64_t )nResources * 9 + ( uint64_t )nShards * 8 + 7;
}

void SIRPackCatalog::reset( void ) noexcept
{
	*this = SIRPackCatalog();
}

sirPackResult_t SIRPackCatalog::resourceAt( uint32_t iResource, sirPackResource_t *pResource ) const noexcept
{
	if ( !isOpen() || pResource == nullptr || iResource >= m_nResources ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	const uint8_t *p = m_pBytes + m_nResourceOffset + ( uint64_t )iResource * SIR_PACK_RESOURCE_RECORD_BYTES;
	sirPackResource_t resource = {};
	resource.key.m_nAssetId = sirPackRead64( p );
	resource.key.m_nTypeId = sirPackRead32( p + 8 );
	resource.key.m_nVariantId = sirPackRead32( p + 12 );
	resource.nRevision = sirPackRead64( p + 16 );
	resource.nLogicalBytes = sirPackRead64( p + 24 );
	resource.nFormatId = sirPackRead32( p + 32 );
	resource.nFirstChunk = sirPackRead32( p + 36 );
	resource.nChunkCount = sirPackRead32( p + 40 );
	resource.nFirstDependency = sirPackRead32( p + 44 );
	resource.nDependencyCount = sirPackRead32( p + 48 );
	resource.nSchemaVersion = sirPackRead16( p + 52 );
	resource.nKind = p[54];
	*pResource = resource;
	return SIR_PACK_OK;
}

sirPackResult_t SIRPackCatalog::chunkAt( uint32_t iChunk, sirPackChunk_t *pChunk ) const noexcept
{
	if ( !isOpen() || pChunk == nullptr || iChunk >= m_nChunks ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	const uint8_t *p = m_pBytes + m_nChunkOffset + ( uint64_t )iChunk * SIR_PACK_CHUNK_RECORD_BYTES;
	sirPackChunk_t chunk = {};
	chunk.nChunkId = sirPackRead32( p );
	chunk.nShardIndex = sirPackRead32( p + 4 );
	chunk.nOffset = sirPackRead64( p + 8 );
	chunk.nEncodedBytes = sirPackRead64( p + 16 );
	chunk.nDecodedBytes = sirPackRead64( p + 24 );
	memcpy( chunk.szDigest, p + 32, 32 );
	chunk.eCodec = ( sirPackCodec_t )sirPackRead16( p + 64 );
	chunk.nCodecVersion = sirPackRead16( p + 66 );
	chunk.nAlignment = sirPackRead32( p + 68 );
	chunk.nDictionaryId = sirPackRead32( p + 72 );
	*pChunk = chunk;
	return SIR_PACK_OK;
}

sirPackResult_t SIRPackCatalog::shardAt( uint32_t iShard, sirPackShard_t *pShard ) const noexcept
{
	if ( !isOpen() || pShard == nullptr || iShard >= m_nShards ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	const uint8_t *p = m_pBytes + m_nShardOffset + ( uint64_t )iShard * SIR_PACK_SHARD_RECORD_BYTES;
	sirPackShard_t shard = {};
	shard.nIndex = sirPackRead32( p );
	shard.nBytes = sirPackRead64( p + 8 );
	memcpy( shard.szDigest, p + 16, 32 );
	*pShard = shard;
	return SIR_PACK_OK;
}

sirPackResult_t SIRPackCatalog::dependencyAt( uint32_t iEdge, sirPackDependency_t *pEdge ) const noexcept
{
	if ( !isOpen() || pEdge == nullptr || iEdge >= m_nDependencies ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	const uint8_t *p = m_pBytes + m_nDependencyOffset + ( uint64_t )iEdge * SIR_PACK_DEPENDENCY_RECORD_BYTES;
	sirPackDependency_t edge = {};
	edge.key.m_nAssetId = sirPackRead64( p );
	edge.key.m_nTypeId = sirPackRead32( p + 8 );
	edge.key.m_nVariantId = sirPackRead32( p + 12 );
	edge.eKind = ( sirResourceEdgeKind_t )p[16];
	*pEdge = edge;
	return SIR_PACK_OK;
}

uint32_t SIRPackCatalog::findIndex( const SIRResourceKey& key ) const noexcept
{
	uint32_t nFirst = 0;
	uint32_t nLast = m_nResources;
	while ( nFirst < nLast ) {
		const uint32_t nMiddle = nFirst + ( nLast - nFirst ) / 2;
		sirPackResource_t r = {};
		resourceAt( nMiddle, &r );
		const int32_t nOrder = sirPackCompareKey( r.key, key );
		if ( nOrder < 0 ) {
			nFirst = nMiddle + 1;
		} else {
			nLast = nMiddle;
		}
	}
	if ( nFirst < m_nResources ) {
		sirPackResource_t r = {};
		resourceAt( nFirst, &r );
		if ( sirPackCompareKey( r.key, key ) == 0 ) {
			return nFirst;
		}
	}
	return m_nResources;
}

sirPackResult_t SIRPackCatalog::findResource( const SIRResourceKey& key, sirPackResource_t *pResource ) const noexcept
{
	if ( !isOpen() || pResource == nullptr ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	const uint32_t iResource = findIndex( key );
	return iResource == m_nResources ? SIR_PACK_NOT_FOUND : resourceAt( iResource, pResource );
}

static bool sirPackSpan( uint64_t nOffset, uint64_t nCount, uint64_t nStride, uint64_t nLimit ) noexcept
{
	return nStride != 0 && nOffset <= nLimit && nCount <= ( nLimit - nOffset ) / nStride;
}

sirPackResult_t SIRPackCatalog::open( const void *pBytes, uint64_t nBytes, void *pScratch, uint64_t nScratchBytes ) noexcept
{
	if ( pBytes == nullptr || nBytes < SIR_PACK_HEADER_BYTES ) {
		return pBytes == nullptr ? SIR_PACK_INVALID_ARGUMENT : SIR_PACK_TRUNCATED;
	}
	if ( nBytes > SIZE_MAX ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	const uint8_t *p = ( const uint8_t * )pBytes;
	if ( memcmp( p, "SIRM", 4 ) != 0 || sirPackRead16( p + 6 ) != SIR_PACK_HEADER_BYTES ||
	     sirPackRead32( p + 8 ) != SIR_PACK_ENDIAN_MARKER || sirPackRead32( p + 12 ) != 0 || !sirPackZeroBytes( p + 120, 8 ) ) {
		return SIR_PACK_INVALID_FORMAT;
	}
	if ( sirPackRead16( p + 4 ) != SIR_PACK_VERSION ) {
		return SIR_PACK_UNSUPPORTED_VERSION;
	}
	const uint64_t nPackageId = sirPackRead64( p + 16 );
	const uint64_t nGeneration = sirPackRead64( p + 24 );
	const uint64_t nDeclaredBytes = sirPackRead64( p + 32 );
	const uint32_t nShards = sirPackRead32( p + 40 );
	const uint32_t nResources = sirPackRead32( p + 44 );
	const uint32_t nChunks = sirPackRead32( p + 48 );
	const uint32_t nDependencies = sirPackRead32( p + 52 );
	if ( nPackageId == 0 || nGeneration == 0 || nDeclaredBytes != nBytes || nShards > SIR_PACK_MAX_SHARDS ||
	     nResources > SIR_PACK_MAX_RESOURCES || nChunks > SIR_PACK_MAX_CHUNKS || nDependencies > SIR_PACK_MAX_CHUNKS ) {
		return SIR_PACK_INVALID_FORMAT;
	}
	const uint64_t nOffsets[4] = { sirPackRead64( p + 56 ), sirPackRead64( p + 64 ), sirPackRead64( p + 72 ), sirPackRead64( p + 80 ) };
	const uint64_t nCounts[4] = { nShards, nResources, nChunks, nDependencies };
	const uint64_t nStrides[4] = { SIR_PACK_SHARD_RECORD_BYTES, SIR_PACK_RESOURCE_RECORD_BYTES, SIR_PACK_CHUNK_RECORD_BYTES,
	                               SIR_PACK_DEPENDENCY_RECORD_BYTES };
	uint64_t nCursor = SIR_PACK_HEADER_BYTES;
	for ( uint32_t i = 0; i < 4; ++i ) {
		if ( nOffsets[i] != nCursor || !sirPackSpan( nCursor, nCounts[i], nStrides[i], nBytes ) ) {
			return SIR_PACK_INVALID_FORMAT;
		}
		nCursor += nCounts[i] * nStrides[i];
	}
	if ( nCursor != nBytes ) {
		return SIR_PACK_INVALID_FORMAT;
	}
	uint8_t szDigest[32] = {};
	sirPackCatalogDigest( p, nBytes, szDigest );
	if ( memcmp( p + 88, szDigest, 32 ) != 0 ) {
		return SIR_PACK_BAD_DIGEST;
	}
	if ( requiredScratch( nResources, nShards ) > nScratchBytes || pScratch == nullptr ) {
		return SIR_PACK_OUT_OF_CAPACITY;
	}
	const uintptr_t nCatalogStart = ( uintptr_t )pBytes;
	const uintptr_t nScratchStart = ( uintptr_t )pScratch;
	if ( nBytes > UINTPTR_MAX - nCatalogStart || nScratchBytes > UINTPTR_MAX - nScratchStart ||
	     ( nCatalogStart < nScratchStart + nScratchBytes && nScratchStart < nCatalogStart + nBytes ) ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	SIRPackCatalog candidate;
	candidate.m_pBytes = p;
	candidate.m_nBytes = nBytes;
	candidate.m_nPackageId = nPackageId;
	candidate.m_nGeneration = nGeneration;
	candidate.m_nShardOffset = nOffsets[0];
	candidate.m_nResourceOffset = nOffsets[1];
	candidate.m_nChunkOffset = nOffsets[2];
	candidate.m_nDependencyOffset = nOffsets[3];
	candidate.m_nShards = nShards;
	candidate.m_nResources = nResources;
	candidate.m_nChunks = nChunks;
	candidate.m_nDependencies = nDependencies;
	const sirPackResult_t eValidated = candidate.validateTables( ( uint8_t * )pScratch, nScratchBytes );
	if ( eValidated != SIR_PACK_OK ) {
		return eValidated;
	}
	*this = candidate;
	return SIR_PACK_OK;
}

sirPackResult_t SIRPackCatalog::validateTables( uint8_t *pScratch, uint64_t nScratchBytes ) const noexcept
{
	// Align the caller-owned scratch arrays, then reserve one monotonic physical
	// extent cursor per shard and DFS state per resource.
	const uintptr_t nScratch = ( ( uintptr_t )pScratch + 7 ) & ~( uintptr_t )7;
	if ( nScratch < ( uintptr_t )pScratch || nScratch - ( uintptr_t )pScratch > nScratchBytes ) {
		return SIR_PACK_OUT_OF_CAPACITY;
	}
	uint8_t *pCursor = ( uint8_t * )nScratch;
	uint64_t *pLastExtent = ( uint64_t * )pCursor;
	pCursor += ( uint64_t )m_nShards * 8;
	uint32_t *pStack = ( uint32_t * )pCursor;
	pCursor += ( uint64_t )m_nResources * 4;
	uint32_t *pNext = ( uint32_t * )pCursor;
	pCursor += ( uint64_t )m_nResources * 4;
	uint8_t *pColors = pCursor;
	for ( uint32_t i = 0; i < m_nShards; ++i ) {
		const uint8_t *pShard = m_pBytes + m_nShardOffset + ( uint64_t )i * SIR_PACK_SHARD_RECORD_BYTES;
		if ( sirPackRead32( pShard ) != i || sirPackRead32( pShard + 4 ) != 0 ||
		     sirPackRead64( pShard + 8 ) < SIR_PACK_SHARD_HEADER_BYTES || !sirPackZeroBytes( pShard + 48, 16 ) ) {
			return SIR_PACK_INVALID_FORMAT;
		}
		pLastExtent[i] = SIR_PACK_SHARD_HEADER_BYTES;
	}
	uint32_t nNextChunk = 0;
	uint32_t nNextDep = 0;
	sirPackResource_t previous = {};
	for ( uint32_t i = 0; i < m_nResources; ++i ) {
		sirPackResource_t resource = {};
		resourceAt( i, &resource );
		const uint8_t *pResource = m_pBytes + m_nResourceOffset + ( uint64_t )i * SIR_PACK_RESOURCE_RECORD_BYTES;
		if ( resource.key.m_nAssetId == 0 || resource.nRevision == 0 || resource.nSchemaVersion == 0 ||
		     resource.nKind > SIR_RESOURCE_KIND_COMPOSITION || pResource[55] != 0 ||
		     ( i != 0 && sirPackCompareKey( previous.key, resource.key ) >= 0 ) || resource.nChunkCount == 0 ||
		     resource.nFirstChunk != nNextChunk || resource.nFirstDependency != nNextDep || resource.nChunkCount > m_nChunks - nNextChunk ||
		     resource.nDependencyCount > m_nDependencies - nNextDep ) {
			return SIR_PACK_INVALID_FORMAT;
		}
		uint64_t nDecodedSum = 0;
		uint32_t nPreviousId = 0;
		for ( uint32_t j = 0; j < resource.nChunkCount; ++j ) {
			const uint32_t iChunk = resource.nFirstChunk + j;
			sirPackChunk_t chunk = {};
			chunkAt( iChunk, &chunk );
			const uint8_t *pChunk = m_pBytes + m_nChunkOffset + ( uint64_t )iChunk * SIR_PACK_CHUNK_RECORD_BYTES;
			if ( chunk.nShardIndex >= m_nShards || chunk.nAlignment == 0 || chunk.nAlignment > 4096 ||
			     ( chunk.nAlignment & ( chunk.nAlignment - 1 ) ) != 0 || chunk.nOffset % chunk.nAlignment != 0 ||
			     chunk.nDecodedBytes == 0 || chunk.nDecodedBytes > SIR_PACK_MAX_DECODED_CHUNK_BYTES || chunk.nEncodedBytes == 0 ||
			     chunk.nEncodedBytes != chunk.nDecodedBytes || chunk.eCodec != SIR_PACK_CODEC_NONE || chunk.nCodecVersion != 1 ||
			     chunk.nDictionaryId != 0 || !sirPackZeroBytes( pChunk + 76, 4 ) || ( j > 0 && chunk.nChunkId <= nPreviousId ) ) {
				return chunk.eCodec != SIR_PACK_CODEC_NONE ? SIR_PACK_UNSUPPORTED_CODEC : SIR_PACK_INVALID_FORMAT;
			}
			sirPackShard_t shard = {};
			shardAt( chunk.nShardIndex, &shard );
			if ( chunk.nOffset < pLastExtent[chunk.nShardIndex] || chunk.nOffset > shard.nBytes ||
			     chunk.nEncodedBytes > shard.nBytes - chunk.nOffset || nDecodedSum > UINT64_MAX - chunk.nDecodedBytes ) {
				return SIR_PACK_INVALID_FORMAT;
			}
			pLastExtent[chunk.nShardIndex] = chunk.nOffset + chunk.nEncodedBytes;
			nDecodedSum += chunk.nDecodedBytes;
			nPreviousId = chunk.nChunkId;
		}
		if ( nDecodedSum != resource.nLogicalBytes ) {
			return SIR_PACK_INVALID_FORMAT;
		}
		nNextChunk += resource.nChunkCount;
		nNextDep += resource.nDependencyCount;
		previous = resource;
	}
	if ( nNextChunk != m_nChunks || nNextDep != m_nDependencies ) {
		return SIR_PACK_INVALID_FORMAT;
	}
	for ( uint32_t i = 0; i < m_nResources; ++i ) {
		sirPackResource_t resource = {};
		resourceAt( i, &resource );
		sirPackDependency_t previousDep = {};
		for ( uint32_t j = 0; j < resource.nDependencyCount; ++j ) {
			const uint32_t iEdge = resource.nFirstDependency + j;
			sirPackDependency_t dep = {};
			dependencyAt( iEdge, &dep );
			const uint8_t *pDep = m_pBytes + m_nDependencyOffset + ( uint64_t )iEdge * SIR_PACK_DEPENDENCY_RECORD_BYTES;
			if ( dep.key.m_nAssetId == 0 || dep.eKind > SIR_RESOURCE_EDGE_KIND_STREAMED || !sirPackZeroBytes( pDep + 17, 7 ) ||
			     ( j > 0 && sirPackCompareKey( previousDep.key, dep.key ) >= 0 ) ||
			     ( dep.eKind == SIR_RESOURCE_EDGE_KIND_REQUIRED && findIndex( dep.key ) == m_nResources ) ) {
				return SIR_PACK_INVALID_FORMAT;
			}
			previousDep = dep;
		}
	}
	memset( pColors, 0, m_nResources );
	for ( uint32_t i = 0; i < m_nResources; ++i ) {
		if ( pColors[i] != 0 ) {
			continue;
		}
		uint32_t nDepth = 1;
		pStack[0] = i;
		pNext[0] = 0;
		pColors[i] = 1;
		while ( nDepth != 0 ) {
			const uint32_t iCurrent = pStack[nDepth - 1];
			sirPackResource_t resource = {};
			resourceAt( iCurrent, &resource );
			if ( pNext[nDepth - 1] == resource.nDependencyCount ) {
				pColors[iCurrent] = 2;
				--nDepth;
				continue;
			}
			sirPackDependency_t dep = {};
			dependencyAt( resource.nFirstDependency + pNext[nDepth - 1], &dep );
			++pNext[nDepth - 1];
			const uint32_t iTarget = findIndex( dep.key );
			if ( iTarget == m_nResources || pColors[iTarget] == 2 ) {
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

sirPackResult_t SIRPackCatalog::validateShard( uint32_t iShard, const void *pBytes, uint64_t nBytes ) const noexcept
{
	if ( !isOpen() || pBytes == nullptr || iShard >= m_nShards || nBytes > SIZE_MAX ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	sirPackShard_t shard = {};
	shardAt( iShard, &shard );
	if ( nBytes != shard.nBytes || nBytes < SIR_PACK_SHARD_HEADER_BYTES ) {
		return SIR_PACK_TRUNCATED;
	}
	const uint8_t *p = ( const uint8_t * )pBytes;
	if ( memcmp( p, "SIRP", 4 ) != 0 || sirPackRead16( p + 6 ) != SIR_PACK_SHARD_HEADER_BYTES ||
	     sirPackRead32( p + 8 ) != SIR_PACK_ENDIAN_MARKER || !sirPackZeroBytes( p + 40, 24 ) ) {
		return SIR_PACK_INVALID_FORMAT;
	}
	if ( sirPackRead16( p + 4 ) != SIR_PACK_VERSION ) {
		return SIR_PACK_UNSUPPORTED_VERSION;
	}
	if ( sirPackRead32( p + 12 ) != iShard || sirPackRead64( p + 16 ) != m_nPackageId || sirPackRead64( p + 24 ) != m_nGeneration ||
	     sirPackRead64( p + 32 ) != nBytes ) {
		return SIR_PACK_STALE_SOURCE;
	}
	uint8_t szDigest[32] = {};
	sirPackSha256( p, nBytes, szDigest );
	return memcmp( szDigest, shard.szDigest, 32 ) == 0 ? SIR_PACK_OK : SIR_PACK_BAD_DIGEST;
}

sirPackResult_t SIRPackCatalog::verifyDecodedChunk( uint32_t iChunk, const void *pBytes, uint64_t nBytes ) const noexcept
{
	if ( pBytes == nullptr || !isOpen() || iChunk >= m_nChunks ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	sirPackChunk_t chunk = {};
	chunkAt( iChunk, &chunk );
	if ( chunk.eCodec != SIR_PACK_CODEC_NONE ) {
		return SIR_PACK_UNSUPPORTED_CODEC;
	}
	if ( chunk.nDecodedBytes != nBytes ) {
		return SIR_PACK_INVALID_FORMAT;
	}
	uint8_t szDigest[32] = {};
	sirPackSha256( ( const uint8_t * )pBytes, nBytes, szDigest );
	return memcmp( szDigest, chunk.szDigest, 32 ) == 0 ? SIR_PACK_OK : SIR_PACK_BAD_DIGEST;
}
