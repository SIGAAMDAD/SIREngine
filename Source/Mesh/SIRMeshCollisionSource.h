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
#ifndef __SIRENGINE_SIRMESHCOLLISIONSOURCE_H__
#define __SIRENGINE_SIRMESHCOLLISIONSOURCE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshCache.h>

// One immutable source borrow per shared collision geometry. Provider-native
// shapes, contact manifolds, and per-instance transforms belong to physics.
typedef struct sirMeshCollisionProfile_s
{
	uint32_t nProfileId;
	uint32_t nMaterialId;
	uint32_t nFlags;
	uint32_t nReserved;
} sirMeshCollisionProfile_t;

typedef struct sirMeshCollisionTriangle_s
{
	float positions[9];
	uint32_t nMaterialId;
} sirMeshCollisionTriangle_t;

typedef struct sirMeshCollisionRay_s
{
	float origin[3];
	float direction[3];
	float fMaxDistance;
} sirMeshCollisionRay_t;

typedef struct sirMeshCollisionHit_s
{
	float fDistance;
	float position[3];
	float normal[3];
	uint32_t nTriangle;
	uint32_t nMaterialId;
} sirMeshCollisionHit_t;

// The caller retains both the SIRMeshCache and this owner object until the
// final reader leaves; shutdown fails while instances still hold the source.
class SIRMeshCollisionSource
{
public:
	SIRMeshCollisionSource( void ) noexcept;
	SIRMeshCollisionSource( const SIRMeshCollisionSource& other ) = delete;
	SIRMeshCollisionSource( SIRMeshCollisionSource&& other ) = delete;
	~SIRMeshCollisionSource( void ) noexcept;

	SIRMeshCollisionSource& operator=( const SIRMeshCollisionSource& other ) = delete;
	SIRMeshCollisionSource& operator=( SIRMeshCollisionSource&& other ) = delete;

	uint32_t getInstanceCount( void ) const noexcept;
	const sirMeshKey_t& getSourceKey( void ) const noexcept;

	sirMeshResult_t initialize( SIRMeshCache& cache, sirMeshHandle_t handle, const sirMeshCollisionProfile_t& profile ) noexcept;
	sirMeshResult_t acquireInstance( void ) noexcept;
	sirMeshResult_t releaseInstance( void ) noexcept;
	sirMeshResult_t getTriangle( uint32_t nTriangle, sirMeshCollisionTriangle_t *pOut ) const noexcept;
	sirMeshResult_t raycast( const sirMeshCollisionRay_t& ray, sirMeshCollisionHit_t *pHit ) const noexcept;
	sirMeshResult_t shutdown( void ) noexcept;

protected:
private:
	SIRMeshCache *m_pCache;
	sirMeshHandle_t m_Handle;
	sirMeshGeometryRead_t m_Geometry;
	sirMeshCollisionProfile_t m_Profile;
	uint32_t m_nInstances;
	bool m_bReady;
};

#endif // __SIRENGINE_SIRMESHCOLLISIONSOURCE_H__
