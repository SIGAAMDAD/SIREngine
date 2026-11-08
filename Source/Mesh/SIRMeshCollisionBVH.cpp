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
#include <Mesh/SIRMeshCollisionBVH.h>

#include <assert.h>
#include <cmath>
#include <cstdint>
#include <cstring>

SIRMeshCollisionBVH::SIRMeshCollisionBVH( void ) noexcept
    : m_pSource( nullptr ), m_pNodes( nullptr ), m_pTriangleOrder( nullptr ), m_nNodeCount( 0 ), m_nTriangleCount( 0 ), m_bReady( false )
{
}

SIRMeshCollisionBVH::~SIRMeshCollisionBVH( void ) noexcept
{
	assert( !m_bReady && "SIRMeshCollisionBVH requires explicit shutdown" );
}

uint32_t SIRMeshCollisionBVH::getNodeCount( void ) const noexcept
{
	return m_nNodeCount;
}

uint32_t SIRMeshCollisionBVH::getTriangleCount( void ) const noexcept
{
	return m_nTriangleCount;
}

uint64_t SIRMeshCollisionBVH::requiredBytes( uint32_t nTriangles ) noexcept
{
	if ( nTriangles == 0 || nTriangles > UINT32_MAX / 2 )
		return 0;
	return ( uint64_t )nTriangles * ( sizeof( sirMeshBVHNode_t ) * 2 + sizeof( uint32_t ) );
}

uint32_t SIRMeshCollisionBVH::requiredAlignment( void ) noexcept
{
	return alignof( sirMeshBVHNode_t );
}

sirMeshResult_t SIRMeshCollisionBVH::initialize( SIRMeshCollisionSource& source, void *pStorage, uint64_t nStorageBytes ) noexcept
{
	if ( m_bReady )
		return SIR_MESH_BUSY;
	const uint32_t nTriangles = source.getTriangleCount();
	const uint64_t bytes = requiredBytes( nTriangles );
	if ( bytes == 0 || pStorage == nullptr || ( ( uintptr_t )pStorage % requiredAlignment() ) != 0 || nStorageBytes < bytes )
		return SIR_MESH_INVALID_ARGUMENT;
	const sirMeshResult_t borrowed = source.acquireInstance();
	if ( borrowed != SIR_MESH_OK )
		return borrowed;
	m_pSource = &source;
	m_pNodes = ( sirMeshBVHNode_t * )pStorage;
	m_pTriangleOrder = ( uint32_t * )( m_pNodes + 2ULL * nTriangles );
	m_nTriangleCount = nTriangles;
	m_nNodeCount = 0;
	for ( uint32_t i = 0; i < nTriangles; ++i )
		m_pTriangleOrder[i] = i;
	buildNode( 0, nTriangles, 0 );
	m_bReady = true;
	return SIR_MESH_OK;
}

float SIRMeshCollisionBVH::getCentroid( uint32_t nTriangle, uint32_t nAxis ) const noexcept
{
	sirMeshCollisionTriangle_t tri = {};
	const sirMeshResult_t result = m_pSource->getTriangle( nTriangle, &tri );
	assert( result == SIR_MESH_OK );
	( void )result;
	return ( tri.positions[nAxis] + tri.positions[nAxis + 3] + tri.positions[nAxis + 6] ) / 3.0f;
}

uint32_t SIRMeshCollisionBVH::buildNode( uint32_t nFirst, uint32_t nCount, uint32_t nDepth ) noexcept
{
	const uint32_t nNode = m_nNodeCount++;
	sirMeshBVHNode_t& node = m_pNodes[nNode];
	node.nFirst = nFirst;
	node.nCount = nCount;
	node.nLeft = UINT32_MAX;
	node.nRight = UINT32_MAX;
	float minCentroid[3] = { INFINITY, INFINITY, INFINITY };
	float maxCentroid[3] = { -INFINITY, -INFINITY, -INFINITY };
	for ( uint32_t a = 0; a < 3; ++a ) {
		node.boundsMin[a] = INFINITY;
		node.boundsMax[a] = -INFINITY;
	}
	for ( uint32_t i = nFirst; i < nFirst + nCount; ++i ) {
		sirMeshCollisionTriangle_t triangle = {};
		const sirMeshResult_t result = m_pSource->getTriangle( m_pTriangleOrder[i], &triangle );
		assert( result == SIR_MESH_OK );
		( void )result;
		for ( uint32_t axis = 0; axis < 3; ++axis ) {
			for ( uint32_t corner = 0; corner < 3; ++corner ) {
				const float value = triangle.positions[corner * 3 + axis];
				if ( value < node.boundsMin[axis] )
					node.boundsMin[axis] = value;
				if ( value > node.boundsMax[axis] )
					node.boundsMax[axis] = value;
			}
			const float center = ( triangle.positions[axis] + triangle.positions[axis + 3] + triangle.positions[axis + 6] ) / 3.0f;
			if ( center < minCentroid[axis] )
				minCentroid[axis] = center;
			if ( center > maxCentroid[axis] )
				maxCentroid[axis] = center;
		}
	}
	if ( nCount <= 8 )
		return nNode;
	uint32_t axis = 0;
	for ( uint32_t a = 1; a < 3; ++a ) {
		if ( maxCentroid[a] - minCentroid[a] > maxCentroid[axis] - minCentroid[axis] )
			axis = a;
	}
	const float midpoint = minCentroid[axis] + ( maxCentroid[axis] - minCentroid[axis] ) * 0.5f;
	uint32_t left = nFirst;
	uint32_t right = nFirst + nCount;
	if ( nDepth < 32 && maxCentroid[axis] > minCentroid[axis] ) {
		while ( left < right ) {
			if ( getCentroid( m_pTriangleOrder[left], axis ) < midpoint ) {
				++left;
			} else {
				--right;
				const uint32_t swapped = m_pTriangleOrder[left];
				m_pTriangleOrder[left] = m_pTriangleOrder[right];
				m_pTriangleOrder[right] = swapped;
			}
		}
	}
	if ( left == nFirst || left == nFirst + nCount )
		left = nFirst + nCount / 2;
	const uint32_t leftCount = left - nFirst;
	node.nCount = 0;
	node.nLeft = buildNode( nFirst, leftCount, nDepth + 1 );
	node.nRight = buildNode( left, nCount - leftCount, nDepth + 1 );
	return nNode;
}

bool SIRMeshCollisionBVH::intersectBounds( const sirMeshBVHNode_t& node, const sirMeshCollisionRay_t& ray,
                                           float fMaxDistance ) const noexcept
{
	float tMin = 0.0f;
	float tMax = fMaxDistance;
	for ( uint32_t a = 0; a < 3; ++a ) {
		const float d = ray.direction[a];
		if ( std::fabs( d ) < 1.0e-20f ) {
			if ( ray.origin[a] < node.boundsMin[a] || ray.origin[a] > node.boundsMax[a] )
				return false;
			continue;
		}
		const float inv = 1.0f / d;
		float nearHit = ( node.boundsMin[a] - ray.origin[a] ) * inv;
		float farHit = ( node.boundsMax[a] - ray.origin[a] ) * inv;
		if ( nearHit > farHit ) {
			const float swapped = nearHit;
			nearHit = farHit;
			farHit = swapped;
		}
		if ( nearHit > tMin )
			tMin = nearHit;
		if ( farHit < tMax )
			tMax = farHit;
		if ( tMin > tMax )
			return false;
	}
	return true;
}

bool SIRMeshCollisionBVH::raycastNode( uint32_t nNode, const sirMeshCollisionRay_t& ray, sirMeshCollisionHit_t *pHit ) const noexcept
{
	const sirMeshBVHNode_t& node = m_pNodes[nNode];
	if ( !intersectBounds( node, ray, pHit->fDistance ) )
		return false;
	bool found = false;
	if ( node.nCount == 0 ) {
		const bool leftHit = raycastNode( node.nLeft, ray, pHit );
		const bool rightHit = raycastNode( node.nRight, ray, pHit );
		return leftHit || rightHit;
	}
	for ( uint32_t i = node.nFirst; i < node.nFirst + node.nCount; ++i ) {
		sirMeshCollisionRay_t candidate = ray;
		candidate.fMaxDistance = pHit->fDistance;
		sirMeshCollisionHit_t result = {};
		const sirMeshResult_t rc = m_pSource->raycastTriangle( candidate, m_pTriangleOrder[i], &result );
		if ( rc == SIR_MESH_OK ) {
			if ( result.fDistance < pHit->fDistance || ( result.fDistance == pHit->fDistance && result.nTriangle < pHit->nTriangle ) )
				*pHit = result;
			found = true;
		}
	}
	return found;
}

sirMeshResult_t SIRMeshCollisionBVH::raycast( const sirMeshCollisionRay_t& ray, sirMeshCollisionHit_t *pHit ) const noexcept
{
	if ( !m_bReady )
		return SIR_MESH_NOT_READY;
	if ( pHit == nullptr || !std::isfinite( ray.fMaxDistance ) || ray.fMaxDistance <= 0.0f )
		return SIR_MESH_INVALID_ARGUMENT;
	float length = 0.0f;
	for ( uint32_t i = 0; i < 3; ++i ) {
		if ( !std::isfinite( ray.origin[i] ) || !std::isfinite( ray.direction[i] ) )
			return SIR_MESH_INVALID_ARGUMENT;
		length += ray.direction[i] * ray.direction[i];
	}
	if ( !std::isfinite( length ) || length <= 0.0f )
		return SIR_MESH_INVALID_ARGUMENT;
	sirMeshCollisionRay_t normalized = ray;
	const float inverseLength = 1.0f / std::sqrt( length );
	for ( uint32_t i = 0; i < 3; ++i )
		normalized.direction[i] *= inverseLength;
	sirMeshCollisionHit_t best = {};
	best.fDistance = ray.fMaxDistance;
	best.nTriangle = UINT32_MAX;
	if ( !raycastNode( 0, normalized, &best ) )
		return SIR_MESH_NOT_FOUND;
	*pHit = best;
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCollisionBVH::shutdown( void ) noexcept
{
	if ( !m_bReady )
		return SIR_MESH_NOT_READY;
	const sirMeshResult_t result = m_pSource->releaseInstance();
	if ( result != SIR_MESH_OK )
		return result;
	m_pSource = nullptr;
	m_pNodes = nullptr;
	m_pTriangleOrder = nullptr;
	m_nNodeCount = 0;
	m_nTriangleCount = 0;
	m_bReady = false;
	return SIR_MESH_OK;
}
