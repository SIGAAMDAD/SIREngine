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
#ifndef __SIRENGINE_SIRMESHCOLLISIONBVH_H__
#define __SIRENGINE_SIRMESHCOLLISIONBVH_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshCollisionSource.h>

// Independent CPU acceleration derivative. Nodes and triangle-order indices
// are caller-owned; no vertex or triangle source bytes are duplicated.
typedef struct sirMeshBVHNode_s
{
	float boundsMin[3];
	float boundsMax[3];
	uint32_t nLeft;
	uint32_t nRight;
	uint32_t nFirst;
	uint32_t nCount;
} sirMeshBVHNode_t;

class SIRMeshCollisionBVH
{
public:
	SIRMeshCollisionBVH( void ) noexcept;
	SIRMeshCollisionBVH( const SIRMeshCollisionBVH& other ) = delete;
	SIRMeshCollisionBVH( SIRMeshCollisionBVH&& other ) = delete;
	~SIRMeshCollisionBVH( void ) noexcept;

	SIRMeshCollisionBVH& operator=( const SIRMeshCollisionBVH& other ) = delete;
	SIRMeshCollisionBVH& operator=( SIRMeshCollisionBVH&& other ) = delete;

	uint32_t getNodeCount( void ) const noexcept;
	uint32_t getTriangleCount( void ) const noexcept;
	uint32_t getReaderCount( void ) const noexcept;
	bool matchesSource( const SIRMeshCollisionSource& source ) const noexcept;
	sirMeshResult_t getBounds( float *pMinXYZ, float *pMaxXYZ ) const noexcept;
	sirMeshResult_t acquireReader( void ) noexcept;
	sirMeshResult_t releaseReader( void ) noexcept;

	static uint64_t requiredBytes( uint32_t nTriangles ) noexcept;
	static uint32_t requiredAlignment( void ) noexcept;

	sirMeshResult_t initialize( SIRMeshCollisionSource& source, void *pStorage, uint64_t nStorageBytes ) noexcept;
	sirMeshResult_t raycast( const sirMeshCollisionRay_t& ray, sirMeshCollisionHit_t *pHit ) const noexcept;
	sirMeshResult_t shutdown( void ) noexcept;

protected:
private:
	uint32_t buildNode( uint32_t nFirst, uint32_t nCount, uint32_t nDepth ) noexcept;
	bool raycastNode( uint32_t nNode, const sirMeshCollisionRay_t& ray, sirMeshCollisionHit_t *pHit ) const noexcept;
	bool intersectBounds( const sirMeshBVHNode_t& node, const sirMeshCollisionRay_t& ray, float fMaxDistance ) const noexcept;
	float getCentroid( uint32_t nTriangle, uint32_t nAxis ) const noexcept;

	SIRMeshCollisionSource *m_pSource;
	sirMeshBVHNode_t *m_pNodes;
	uint32_t *m_pTriangleOrder;
	uint32_t m_nNodeCount;
	uint32_t m_nTriangleCount;
	uint32_t m_nReaders;
	bool m_bReady;
};

#endif // __SIRENGINE_SIRMESHCOLLISIONBVH_H__
