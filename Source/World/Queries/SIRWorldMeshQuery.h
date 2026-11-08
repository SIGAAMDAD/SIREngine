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
#ifndef __SIRENGINE_SIRWORLDMESHQUERY_H__
#define __SIRENGINE_SIRWORLDMESHQUERY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshCollisionBVH.h>

// Object-to-world affine matrix, row-major 3x4. The World query owns no
// canonical mesh bytes or physics bodies; a source lease protects each instance.
typedef struct sirWorldMeshInstance_s
{
	uint64_t nInstanceId;
	uint32_t nWorldGeneration;
	uint32_t nLayerMask;
	SIRMeshCollisionSource *pSource;
	SIRMeshCollisionBVH *pBVH;
	float transform[12];
} sirWorldMeshInstance_t;

typedef struct sirWorldMeshHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nOwnerId;
} sirWorldMeshHandle_t;

typedef struct sirWorldMeshHit_s
{
	uint64_t nInstanceId;
	sirMeshKey_t keySource;
	uint32_t nTriangle;
	uint32_t nMaterialId;
	float fDistance;
	float position[3];
	float normal[3];
} sirWorldMeshHit_t;

typedef enum sirWorldMeshQueryResult_e : uint8_t
{
	SIR_WORLD_MESH_OK = 0,
	SIR_WORLD_MESH_INVALID_ARGUMENT = 1,
	SIR_WORLD_MESH_NOT_READY = 2,
	SIR_WORLD_MESH_OUT_OF_CAPACITY = 3,
	SIR_WORLD_MESH_DUPLICATE = 4,
	SIR_WORLD_MESH_NOT_FOUND = 5,
	SIR_WORLD_MESH_STALE_HANDLE = 6,
	SIR_WORLD_MESH_BUSY = 7
} sirWorldMeshQueryResult_t;

struct sirWorldMeshEntry_s;

// Single-owner world query index, keyed to one world-generation epoch.
// No heap allocation or geometry copy occurs on insert/query/update/removal.
class SIRWorldMeshQuery
{
public:
	SIRWorldMeshQuery( void ) noexcept;
	SIRWorldMeshQuery( const SIRWorldMeshQuery& other ) = delete;
	SIRWorldMeshQuery( SIRWorldMeshQuery&& other ) = delete;
	~SIRWorldMeshQuery( void ) noexcept;

	SIRWorldMeshQuery& operator=( const SIRWorldMeshQuery& other ) = delete;
	SIRWorldMeshQuery& operator=( SIRWorldMeshQuery&& other ) = delete;

	uint32_t getInstanceCount( void ) const noexcept;

	static uint64_t requiredBytes( uint32_t nCapacity ) noexcept;
	static uint32_t requiredAlignment( void ) noexcept;

	sirWorldMeshQueryResult_t initialize( void *pStorage, uint64_t nBytes, uint32_t nCapacity, uint32_t nWorldGeneration ) noexcept;
	sirWorldMeshQueryResult_t insert( const sirWorldMeshInstance_t& instance, sirWorldMeshHandle_t *pHandle ) noexcept;
	sirWorldMeshQueryResult_t setTransform( sirWorldMeshHandle_t handle, const float *pTransform3x4 ) noexcept;
	sirWorldMeshQueryResult_t remove( sirWorldMeshHandle_t handle ) noexcept;
	sirWorldMeshQueryResult_t raycast( const sirMeshCollisionRay_t& ray, uint32_t nLayerMask, uint32_t nWorldGeneration,
	                                   sirWorldMeshHit_t *pHit ) const noexcept;
	sirWorldMeshQueryResult_t shutdown( void ) noexcept;

protected:
private:
	bool isValidHandle( sirWorldMeshHandle_t handle ) const noexcept;
	bool buildTransform( sirWorldMeshEntry_s& entry, const float *pTransform3x4 ) const noexcept;

	sirWorldMeshEntry_s *m_pEntries;
	uint64_t m_nOwnerId;
	uint32_t m_nNextGeneration;
	uint32_t m_nWorldGeneration;
	uint32_t m_nCapacity;
	uint32_t m_nCount;
	bool m_bReady;
};

#endif // __SIRENGINE_SIRWORLDMESHQUERY_H__
