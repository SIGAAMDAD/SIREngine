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
#include <Mesh/SIRMeshCollisionSource.h>

#include <assert.h>
#include <cmath>
#include <cstdint>
#include <cstring>

SIRMeshCollisionSource::SIRMeshCollisionSource( void ) noexcept
    : m_pCache( nullptr ), m_Handle( {} ), m_Geometry( {} ), m_Profile( {} ), m_nInstances( 0 ), m_bReady( false )
{
}

SIRMeshCollisionSource::~SIRMeshCollisionSource( void ) noexcept
{
	assert( !m_bReady && "SIRMeshCollisionSource requires explicit shutdown" );
}

uint32_t SIRMeshCollisionSource::getInstanceCount( void ) const noexcept
{
	return m_nInstances;
}

uint32_t SIRMeshCollisionSource::getTriangleCount( void ) const noexcept
{
	return m_bReady ? m_Geometry.nIndexCount / 3 : 0;
}

const sirMeshKey_t& SIRMeshCollisionSource::getSourceKey( void ) const noexcept
{
	return m_Geometry.key;
}

sirMeshResult_t SIRMeshCollisionSource::initialize( SIRMeshCache& cache, sirMeshHandle_t handle,
                                                    const sirMeshCollisionProfile_t& profile ) noexcept
{
	if ( m_bReady )
		return SIR_MESH_BUSY;
	if ( profile.nProfileId == 0 || profile.nReserved != 0 )
		return SIR_MESH_INVALID_ARGUMENT;
	sirMeshGeometryRead_t read = {};
	const sirMeshResult_t result = cache.acquire( handle, &read );
	if ( result != SIR_MESH_OK )
		return result;
	m_pCache = &cache;
	m_Handle = handle;
	m_Geometry = read;
	m_Profile = profile;
	m_nInstances = 0;
	m_bReady = true;
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCollisionSource::acquireInstance( void ) noexcept
{
	if ( !m_bReady )
		return SIR_MESH_NOT_READY;
	if ( m_nInstances == UINT32_MAX )
		return SIR_MESH_OUT_OF_CAPACITY;
	++m_nInstances;
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCollisionSource::releaseInstance( void ) noexcept
{
	if ( !m_bReady )
		return SIR_MESH_NOT_READY;
	if ( m_nInstances == 0 )
		return SIR_MESH_INVALID_ARGUMENT;
	--m_nInstances;
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCollisionSource::getTriangle( uint32_t nTriangle, sirMeshCollisionTriangle_t *pOut ) const noexcept
{
	if ( !m_bReady )
		return SIR_MESH_NOT_READY;
	if ( pOut == nullptr || nTriangle >= m_Geometry.nIndexCount / 3 )
		return SIR_MESH_INVALID_ARGUMENT;
	sirMeshCollisionTriangle_t output = {};
	for ( uint32_t i = 0; i < 3; ++i ) {
		const uint32_t vertex = m_Geometry.pTriangleIndices[nTriangle * 3 + i];
		memcpy( output.positions + i * 3, m_Geometry.pPositionsXYZ + vertex * 3, 3 * sizeof( float ) );
	}
	output.nMaterialId = m_Profile.nMaterialId;
	*pOut = output;
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCollisionSource::raycast( const sirMeshCollisionRay_t& ray, sirMeshCollisionHit_t *pHit ) const noexcept
{
	return raycastRange( ray, 0, getTriangleCount(), pHit );
}

sirMeshResult_t SIRMeshCollisionSource::raycastTriangle( const sirMeshCollisionRay_t& ray, uint32_t nTriangle,
                                                         sirMeshCollisionHit_t *pHit ) const noexcept
{
	if ( !m_bReady )
		return SIR_MESH_NOT_READY;
	if ( nTriangle >= getTriangleCount() )
		return SIR_MESH_INVALID_ARGUMENT;
	return raycastRange( ray, nTriangle, 1, pHit );
}

sirMeshResult_t SIRMeshCollisionSource::raycastRange( const sirMeshCollisionRay_t& ray, uint32_t nStart, uint32_t nCount,
                                                      sirMeshCollisionHit_t *pHit ) const noexcept
{
	if ( !m_bReady )
		return SIR_MESH_NOT_READY;
	if ( pHit == nullptr || !std::isfinite( ray.fMaxDistance ) || ray.fMaxDistance <= 0.0f )
		return SIR_MESH_INVALID_ARGUMENT;
	float directionLength = 0.0f;
	for ( uint32_t i = 0; i < 3; ++i ) {
		if ( !std::isfinite( ray.origin[i] ) || !std::isfinite( ray.direction[i] ) )
			return SIR_MESH_INVALID_ARGUMENT;
		directionLength += ray.direction[i] * ray.direction[i];
	}
	if ( !std::isfinite( directionLength ) || directionLength <= 0.0f )
		return SIR_MESH_INVALID_ARGUMENT;
	const float reciprocalLength = 1.0f / std::sqrt( directionLength );
	const float direction[3] = { ray.direction[0] * reciprocalLength, ray.direction[1] * reciprocalLength,
	                             ray.direction[2] * reciprocalLength };
	float closest = ray.fMaxDistance;
	bool found = false;
	sirMeshCollisionHit_t best = {};
	for ( uint32_t tri = nStart; tri < nStart + nCount; ++tri ) {
		const float *v[3] = {};
		for ( uint32_t i = 0; i < 3; ++i )
			v[i] = m_Geometry.pPositionsXYZ + 3U * m_Geometry.pTriangleIndices[3U * tri + i];
		const float e1[3] = { v[1][0] - v[0][0], v[1][1] - v[0][1], v[1][2] - v[0][2] };
		const float e2[3] = { v[2][0] - v[0][0], v[2][1] - v[0][1], v[2][2] - v[0][2] };
		const float cross[3] = { direction[1] * e2[2] - direction[2] * e2[1], direction[2] * e2[0] - direction[0] * e2[2],
		                         direction[0] * e2[1] - direction[1] * e2[0] };
		const float determinant = e1[0] * cross[0] + e1[1] * cross[1] + e1[2] * cross[2];
		if ( !std::isfinite( determinant ) || std::fabs( determinant ) < 1e-12f )
			continue;
		const float inv = 1.0f / determinant;
		const float delta[3] = { ray.origin[0] - v[0][0], ray.origin[1] - v[0][1], ray.origin[2] - v[0][2] };
		const float u = ( delta[0] * cross[0] + delta[1] * cross[1] + delta[2] * cross[2] ) * inv;
		if ( u < 0.0f || u > 1.0f )
			continue;
		const float q[3] = { delta[1] * e1[2] - delta[2] * e1[1], delta[2] * e1[0] - delta[0] * e1[2],
		                     delta[0] * e1[1] - delta[1] * e1[0] };
		const float vCoord = ( direction[0] * q[0] + direction[1] * q[1] + direction[2] * q[2] ) * inv;
		if ( vCoord < 0.0f || u + vCoord > 1.0f )
			continue;
		const float time = ( e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2] ) * inv;
		if ( time < 0.0f || time > closest || !std::isfinite( time ) || ( found && time == closest && tri >= best.nTriangle ) )
			continue;
		const float normal[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
		const float normalLength = std::sqrt( normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2] );
		if ( normalLength <= 0.0f || !std::isfinite( normalLength ) )
			continue;
		closest = time;
		best.fDistance = time;
		best.nTriangle = tri;
		best.nMaterialId = m_Profile.nMaterialId;
		for ( uint32_t i = 0; i < 3; ++i ) {
			best.position[i] = ray.origin[i] + time * direction[i];
			best.normal[i] = normal[i] / normalLength;
		}
		found = true;
	}
	if ( !found )
		return SIR_MESH_NOT_FOUND;
	*pHit = best;
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCollisionSource::shutdown( void ) noexcept
{
	if ( !m_bReady )
		return SIR_MESH_NOT_READY;
	if ( m_nInstances != 0 )
		return SIR_MESH_BUSY;
	const sirMeshResult_t result = m_pCache->release( m_Handle );
	if ( result != SIR_MESH_OK )
		return result;
	m_pCache = nullptr;
	m_Handle = {};
	m_Geometry = {};
	m_Profile = {};
	m_bReady = false;
	return SIR_MESH_OK;
}
