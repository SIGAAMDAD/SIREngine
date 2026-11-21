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
#ifndef __SIRENGINE_SIRMESHPACKSCHEMA_H__
#define __SIRENGINE_SIRMESHPACKSCHEMA_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshCache.h>
#include <Resources/SIRPackFormat.h>

// SIRG V1 is one immutable SIRPack GEOMETRY resource: chunk 0 is this
// manifest; chunks 1..N hold page payloads. Chunk IDs are stable within a
// revision. The runtime never stores a native shape, device handle or pointer.
static constexpr uint16_t SIR_MESH_PACK_VERSION = 1;
static constexpr uint32_t SIR_MESH_PACK_FORMAT_ID = 0x47524953U; // 'SIRG' as LE
static constexpr uint32_t SIR_MESH_PACK_MAX_RECORDS = 1048576;
static constexpr uint32_t SIR_MESH_PACK_NO_PARENT = 0xffffffffU;
static constexpr uint32_t SIR_MESH_PACK_COLLISION_METERS = 1U;
static constexpr uint32_t SIR_MESH_PACK_COLLISION_CCW = 2U;
static constexpr uint32_t SIR_MESH_PACK_COLLISION_MATERIAL_IDS = 4U;
static constexpr uint32_t SIR_MESH_PACK_COLLISION_SOURCE_FLAGS =
    SIR_MESH_PACK_COLLISION_METERS | SIR_MESH_PACK_COLLISION_CCW | SIR_MESH_PACK_COLLISION_MATERIAL_IDS;
static constexpr uint64_t SIR_MESH_PACK_HEADER_BYTES = 128;
static constexpr uint64_t SIR_MESH_PACK_GROUP_BYTES = 48;
static constexpr uint64_t SIR_MESH_PACK_CLUSTER_BYTES = 48;
static constexpr uint64_t SIR_MESH_PACK_PAGE_BYTES = 48;
static constexpr uint64_t SIR_MESH_PACK_COLLISION_BYTES = 48;

typedef enum sirMeshPackResult_e : uint8_t
{
	SIR_MESH_PACK_OK = 0,
	SIR_MESH_PACK_INVALID_ARGUMENT,
	SIR_MESH_PACK_UNSUPPORTED_VERSION,
	SIR_MESH_PACK_TRUNCATED,
	SIR_MESH_PACK_INVALID_FORMAT,
	SIR_MESH_PACK_BAD_DIGEST,
	SIR_MESH_PACK_OUT_OF_CAPACITY
} sirMeshPackResult_t;

typedef struct sirMeshPackGroup_s
{
	uint32_t nParent;
	uint32_t nFirstCluster;
	uint32_t nClusterCount;
	uint32_t nFirstChild;
	uint32_t nChildCount;
	uint32_t nMaterialId;
	uint32_t nLevel;
	uint32_t nPage;
	float fError;
} sirMeshPackGroup_t;

typedef struct sirMeshPackCluster_s
{
	uint32_t nGroup;
	uint32_t nPage;
	uint32_t nVertexCount;
	uint32_t nTriangleCount;
	uint64_t nByteOffset;
	uint64_t nByteCount;
	float fError;
} sirMeshPackCluster_t;

typedef struct sirMeshPackPage_s
{
	uint32_t nParent;
	uint32_t nFirstCluster;
	uint32_t nClusterCount;
	uint32_t nChunkId;
	uint64_t nByteCount;
	uint8_t szDigest[16];
} sirMeshPackPage_t;

typedef struct sirMeshPackCollision_s
{
	uint32_t nProfile;
	uint32_t nFlags;
	uint32_t nVertexCount;
	uint32_t nIndexCount;
	uint32_t nChunkId;
	uint64_t nRevision;
} sirMeshPackCollision_t;

typedef struct sirMeshPackVertex_s
{
	float szPosition[3];
	float szNormal[3];
	float szTangent[4];
	float szUV[2];
} sirMeshPackVertex_t;

typedef struct sirMeshPackVertexQuery_s
{
	const void *pPageBytes;
	uint64_t nPageBytes;
	uint32_t nPage;
	uint32_t nCluster;
	uint32_t nVertex;
} sirMeshPackVertexQuery_t;

typedef struct sirMeshPackDesc_s
{
	sirMeshKey_t key;
	const sirMeshPackGroup_t *pGroups;
	const sirMeshPackCluster_t *pClusters;
	const sirMeshPackPage_t *pPages;
	const sirMeshPackCollision_t *pCollisions;
	uint32_t nGroups;
	uint32_t nClusters;
	uint32_t nPages;
	uint32_t nCollisions;
	uint32_t nVertexCount;
	uint32_t nTriangleCount;
} sirMeshPackDesc_t;

// Immutable, borrowed, validated view. Wire metadata is field-decoded; no
// packed structs or host-endian direct reads. Caller retains source pin.
class SIRMeshPackSchema
{
public:
	SIRMeshPackSchema( void ) noexcept;

	bool isOpen( void ) const noexcept;
	sirMeshKey_t getKey( void ) const noexcept;
	uint32_t getGroupCount( void ) const noexcept;
	uint32_t getClusterCount( void ) const noexcept;
	uint32_t getPageCount( void ) const noexcept;

	static uint64_t requiredBytes( const sirMeshPackDesc_t& desc ) noexcept;
	static sirMeshPackResult_t write( const sirMeshPackDesc_t& desc, uint8_t *pBytes, uint64_t nCapacity ) noexcept;
	sirMeshPackResult_t open( const void *pBytes, uint64_t nBytes ) noexcept;
	void reset( void ) noexcept;
	sirMeshPackResult_t groupAt( uint32_t iGroup, sirMeshPackGroup_t *pGroup ) const noexcept;
	sirMeshPackResult_t clusterAt( uint32_t iCluster, sirMeshPackCluster_t *pCluster ) const noexcept;
	sirMeshPackResult_t pageAt( uint32_t iPage, sirMeshPackPage_t *pPage ) const noexcept;
	sirMeshPackResult_t collisionAt( uint32_t iCollision, sirMeshPackCollision_t *pCollision ) const noexcept;
	// Bind an opened immutable manifest to its canonical SIRPack resource
	// and check each logical chunk without allocating or publishing state.
	sirMeshPackResult_t validateResource( const sirPackResource_t& resource ) const noexcept;
	sirMeshPackResult_t validateChunk( uint32_t iLogicalChunk, const sirPackChunk_t& chunk ) const noexcept;
	sirMeshPackResult_t verifyPage( uint32_t iPage, const void *pBytes, uint64_t nBytes ) const noexcept;
	sirMeshPackResult_t verifyCollision( uint32_t iCollision, const void *pBytes, uint64_t nBytes ) const noexcept;
	sirMeshPackResult_t decodeVertex( const sirMeshPackVertexQuery_t& query, sirMeshPackVertex_t *pVertex ) const noexcept;

protected:
private:
	const uint8_t *m_pBytes;
	uint64_t m_nBytes;
	sirMeshKey_t m_Key;
	uint32_t m_nGroups;
	uint32_t m_nClusters;
	uint32_t m_nPages;
	uint32_t m_nCollisions;
};

#endif // __SIRENGINE_SIRMESHPACKSCHEMA_H__
