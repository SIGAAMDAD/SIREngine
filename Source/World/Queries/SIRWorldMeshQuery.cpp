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
#include <World/Queries/SIRWorldMeshQuery.h>

#include <Core/Platform/SIRAtomic.h>

#include <assert.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

struct sirWorldMeshEntry_s
{
	SIRMeshCollisionSource *pSource;
	SIRMeshCollisionBVH *pBVH;
	uint64_t nInstanceId;
	uint32_t nGeneration;
	uint32_t nLayerMask;
	float inverse[12];
	float boundsMin[3];
	float boundsMax[3];
	float fWindingSign;
	bool bActive;
};

static SIRAtomic<uint64_t> s_nNextWorldMeshOwnerId( 1ULL );

static uint64_t nextWorldMeshOwnerId( void ) noexcept
{
	uint64_t nExpected = s_nNextWorldMeshOwnerId.load( SIR_MEMORY_ORDER_RELAXED );
	for ( ;; ) {
		if ( nExpected == UINT64_MAX )
			return 0;
		if ( s_nNextWorldMeshOwnerId.compareExchangeWeak( nExpected, nExpected + 1, SIR_MEMORY_ORDER_RELAXED, SIR_MEMORY_ORDER_RELAXED ) )
			return nExpected;
	}
}

static bool finiteFloat( double value ) noexcept
{
	return std::isfinite( value ) && value <= 3.4e38 && value >= -3.4e38;
}

SIRWorldMeshQuery::SIRWorldMeshQuery( void ) noexcept
    : m_pEntries( nullptr ), m_nOwnerId( 0 ), m_nNextGeneration( 0 ), m_nWorldGeneration( 0 ), m_nCapacity( 0 ), m_nCount( 0 ),
      m_bReady( false )
{
}

SIRWorldMeshQuery::~SIRWorldMeshQuery( void ) noexcept
{
	assert( !m_bReady && "SIRWorldMeshQuery requires explicit shutdown" );
}

uint32_t SIRWorldMeshQuery::getInstanceCount( void ) const noexcept
{
	return m_nCount;
}

uint64_t SIRWorldMeshQuery::requiredBytes( uint32_t nCapacity ) noexcept
{
	return ( uint64_t )nCapacity * sizeof( sirWorldMeshEntry_s );
}

uint32_t SIRWorldMeshQuery::requiredAlignment( void ) noexcept
{
	return alignof( sirWorldMeshEntry_s );
}

sirWorldMeshQueryResult_t SIRWorldMeshQuery::initialize( void *pStorage, uint64_t nBytes, uint32_t nCapacity,
                                                         uint32_t nWorldGeneration ) noexcept
{
	if ( m_bReady )
		return SIR_WORLD_MESH_BUSY;
	if ( pStorage == nullptr || nCapacity == 0 || nWorldGeneration == 0 || nBytes < requiredBytes( nCapacity ) ||
	     ( ( uintptr_t )pStorage % requiredAlignment() ) != 0 )
		return SIR_WORLD_MESH_INVALID_ARGUMENT;
	const uint64_t nOwner = nextWorldMeshOwnerId();
	if ( nOwner == 0 )
		return SIR_WORLD_MESH_OUT_OF_CAPACITY;
	m_pEntries = ( sirWorldMeshEntry_s * )pStorage;
	for ( uint32_t i = 0; i < nCapacity; ++i )
		new ( &m_pEntries[i] ) sirWorldMeshEntry_s{};
	m_nOwnerId = nOwner;
	m_nWorldGeneration = nWorldGeneration;
	m_nCapacity = nCapacity;
	m_nCount = 0;
	m_bReady = true;
	return SIR_WORLD_MESH_OK;
}

bool SIRWorldMeshQuery::buildTransform( sirWorldMeshEntry_s& entry, const float *pTransform ) const noexcept
{
	if ( pTransform == nullptr || entry.pBVH == nullptr )
		return false;
	for ( uint32_t i = 0; i < 12; ++i ) {
		if ( !std::isfinite( pTransform[i] ) )
			return false;
	}
	const double a = pTransform[0], b = pTransform[1], c = pTransform[2];
	const double d = pTransform[4], e = pTransform[5], f = pTransform[6];
	const double g = pTransform[8], h = pTransform[9], j = pTransform[10];
	const double det = a * ( e * j - f * h ) - b * ( d * j - f * g ) + c * ( d * h - e * g );
	if ( !std::isfinite( det ) || std::fabs( det ) <= 1.0e-12 )
		return false;
	const double inverseDet = 1.0 / det;
	const double r[9] = { ( e * j - f * h ) * inverseDet, ( c * h - b * j ) * inverseDet, ( b * f - c * e ) * inverseDet,
	                      ( f * g - d * j ) * inverseDet, ( a * j - c * g ) * inverseDet, ( c * d - a * f ) * inverseDet,
	                      ( d * h - e * g ) * inverseDet, ( b * g - a * h ) * inverseDet, ( a * e - b * d ) * inverseDet };
	sirWorldMeshEntry_s candidate = entry;
	candidate.fWindingSign = det < 0.0 ? -1.0f : 1.0f;
	for ( uint32_t row = 0; row < 3; ++row ) {
		for ( uint32_t column = 0; column < 3; ++column ) {
			if ( !finiteFloat( r[row * 3 + column] ) )
				return false;
			candidate.inverse[row * 4 + column] = ( float )r[row * 3 + column];
		}
		const double translation = -r[row * 3] * pTransform[3] - r[row * 3 + 1] * pTransform[7] - r[row * 3 + 2] * pTransform[11];
		if ( !finiteFloat( translation ) )
			return false;
		candidate.inverse[row * 4 + 3] = ( float )translation;
	}
	float localMin[3] = {}, localMax[3] = {};
	if ( entry.pBVH->getBounds( localMin, localMax ) != SIR_MESH_OK )
		return false;
	for ( uint32_t row = 0; row < 3; ++row ) {
		double minValue = pTransform[row * 4 + 3];
		double maxValue = minValue;
		for ( uint32_t column = 0; column < 3; ++column ) {
			const double low = ( double )pTransform[row * 4 + column] * localMin[column];
			const double high = ( double )pTransform[row * 4 + column] * localMax[column];
			minValue += low < high ? low : high;
			maxValue += low > high ? low : high;
		}
		if ( !finiteFloat( minValue ) || !finiteFloat( maxValue ) )
			return false;
		candidate.boundsMin[row] = ( float )minValue;
		candidate.boundsMax[row] = ( float )maxValue;
	}
	for ( uint32_t i = 0; i < 12; ++i )
		entry.inverse[i] = candidate.inverse[i];
	for ( uint32_t i = 0; i < 3; ++i ) {
		entry.boundsMin[i] = candidate.boundsMin[i];
		entry.boundsMax[i] = candidate.boundsMax[i];
	}
	entry.fWindingSign = candidate.fWindingSign;
	return true;
}

bool SIRWorldMeshQuery::isValidHandle( sirWorldMeshHandle_t handle ) const noexcept
{
	return handle.nOwnerId == m_nOwnerId && handle.nSlot < m_nCapacity && m_pEntries[handle.nSlot].bActive &&
	       m_pEntries[handle.nSlot].nGeneration == handle.nGeneration;
}

sirWorldMeshQueryResult_t SIRWorldMeshQuery::insert( const sirWorldMeshInstance_t& instance, sirWorldMeshHandle_t *pHandle ) noexcept
{
	if ( !m_bReady )
		return SIR_WORLD_MESH_NOT_READY;
	if ( pHandle == nullptr || instance.nInstanceId == 0 || instance.nWorldGeneration != m_nWorldGeneration || instance.nLayerMask == 0 ||
	     instance.pSource == nullptr || instance.pBVH == nullptr || !instance.pBVH->matchesSource( *instance.pSource ) )
		return SIR_WORLD_MESH_INVALID_ARGUMENT;
	uint32_t slot = m_nCapacity;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) {
		if ( m_pEntries[i].bActive && m_pEntries[i].nInstanceId == instance.nInstanceId )
			return SIR_WORLD_MESH_DUPLICATE;
		if ( slot == m_nCapacity && !m_pEntries[i].bActive )
			slot = i;
	}
	if ( slot == m_nCapacity || m_nNextGeneration == UINT32_MAX )
		return SIR_WORLD_MESH_OUT_OF_CAPACITY;
	sirWorldMeshEntry_s candidate = {};
	candidate.pBVH = instance.pBVH;
	candidate.pSource = instance.pSource;
	candidate.nInstanceId = instance.nInstanceId;
	candidate.nLayerMask = instance.nLayerMask;
	if ( !buildTransform( candidate, instance.transform ) )
		return SIR_WORLD_MESH_INVALID_ARGUMENT;
	if ( instance.pBVH->acquireReader() != SIR_MESH_OK )
		return SIR_WORLD_MESH_NOT_READY;
	candidate.nGeneration = ++m_nNextGeneration;
	candidate.bActive = true;
	m_pEntries[slot] = candidate;
	++m_nCount;
	*pHandle = { slot, candidate.nGeneration, m_nOwnerId };
	return SIR_WORLD_MESH_OK;
}

sirWorldMeshQueryResult_t SIRWorldMeshQuery::setTransform( sirWorldMeshHandle_t handle, const float *pTransform3x4 ) noexcept
{
	if ( !m_bReady )
		return SIR_WORLD_MESH_NOT_READY;
	if ( !isValidHandle( handle ) )
		return SIR_WORLD_MESH_STALE_HANDLE;
	if ( !buildTransform( m_pEntries[handle.nSlot], pTransform3x4 ) )
		return SIR_WORLD_MESH_INVALID_ARGUMENT;
	return SIR_WORLD_MESH_OK;
}

sirWorldMeshQueryResult_t SIRWorldMeshQuery::remove( sirWorldMeshHandle_t handle ) noexcept
{
	if ( !m_bReady )
		return SIR_WORLD_MESH_NOT_READY;
	if ( !isValidHandle( handle ) )
		return SIR_WORLD_MESH_STALE_HANDLE;
	sirWorldMeshEntry_s& entry = m_pEntries[handle.nSlot];
	if ( entry.pBVH->releaseReader() != SIR_MESH_OK )
		return SIR_WORLD_MESH_BUSY;
	entry = {};
	--m_nCount;
	return SIR_WORLD_MESH_OK;
}

static bool rayIntersectsWorldBounds( const sirWorldMeshEntry_s& entry, const float *pOrigin, const float *pDirection,
                                      float fMaxDistance ) noexcept
{
	float entryTime = 0.0f;
	float exitTime = fMaxDistance;
	for ( uint32_t axis = 0; axis < 3; ++axis ) {
		const float d = pDirection[axis];
		if ( std::fabs( d ) < 1.0e-20f ) {
			if ( pOrigin[axis] < entry.boundsMin[axis] || pOrigin[axis] > entry.boundsMax[axis] )
				return false;
			continue;
		}
		const float inverse = 1.0f / d;
		float nearTime = ( entry.boundsMin[axis] - pOrigin[axis] ) * inverse;
		float farTime = ( entry.boundsMax[axis] - pOrigin[axis] ) * inverse;
		if ( nearTime > farTime ) {
			const float swap = nearTime;
			nearTime = farTime;
			farTime = swap;
		}
		if ( nearTime > entryTime )
			entryTime = nearTime;
		if ( farTime < exitTime )
			exitTime = farTime;
		if ( entryTime > exitTime )
			return false;
	}
	return true;
}

sirWorldMeshQueryResult_t SIRWorldMeshQuery::raycast( const sirMeshCollisionRay_t& ray, uint32_t nLayerMask, uint32_t nWorldGeneration,
                                                      sirWorldMeshHit_t *pHit ) const noexcept
{
	if ( !m_bReady )
		return SIR_WORLD_MESH_NOT_READY;
	if ( nWorldGeneration != m_nWorldGeneration )
		return SIR_WORLD_MESH_STALE_HANDLE;
	if ( pHit == nullptr || nLayerMask == 0 || !std::isfinite( ray.fMaxDistance ) || ray.fMaxDistance <= 0.0f )
		return SIR_WORLD_MESH_INVALID_ARGUMENT;
	double worldLength = 0.0;
	for ( uint32_t axis = 0; axis < 3; ++axis ) {
		if ( !std::isfinite( ray.origin[axis] ) || !std::isfinite( ray.direction[axis] ) )
			return SIR_WORLD_MESH_INVALID_ARGUMENT;
		worldLength += ( double )ray.direction[axis] * ray.direction[axis];
	}
	if ( !std::isfinite( worldLength ) || worldLength <= 0.0 )
		return SIR_WORLD_MESH_INVALID_ARGUMENT;
	const double worldInverseLength = 1.0 / std::sqrt( worldLength );
	const float worldDirection[3] = { ( float )( ray.direction[0] * worldInverseLength ),
	                                  ( float )( ray.direction[1] * worldInverseLength ),
	                                  ( float )( ray.direction[2] * worldInverseLength ) };
	bool found = false;
	sirWorldMeshHit_t best = {};
	best.fDistance = ray.fMaxDistance;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) {
		const sirWorldMeshEntry_s& entry = m_pEntries[i];
		if ( !entry.bActive || ( entry.nLayerMask & nLayerMask ) == 0 ||
		     !rayIntersectsWorldBounds( entry, ray.origin, worldDirection, best.fDistance ) )
			continue;
		sirMeshCollisionRay_t local = {};
		double magnitude = 0.0;
		for ( uint32_t row = 0; row < 3; ++row ) {
			const float *matrix = entry.inverse + row * 4;
			const double origin =
			    ( double )matrix[0] * ray.origin[0] + ( double )matrix[1] * ray.origin[1] + ( double )matrix[2] * ray.origin[2] + matrix[3];
			const double direction =
			    ( double )matrix[0] * worldDirection[0] + ( double )matrix[1] * worldDirection[1] + ( double )matrix[2] * worldDirection[2];
			if ( !finiteFloat( origin ) || !finiteFloat( direction ) )
				return SIR_WORLD_MESH_INVALID_ARGUMENT;
			local.origin[row] = ( float )origin;
			local.direction[row] = ( float )direction;
			magnitude += direction * direction;
		}
		if ( !std::isfinite( magnitude ) || magnitude <= 0.0 )
			return SIR_WORLD_MESH_INVALID_ARGUMENT;
		const double scale = std::sqrt( magnitude );
		if ( !finiteFloat( scale * best.fDistance ) )
			return SIR_WORLD_MESH_INVALID_ARGUMENT;
		local.fMaxDistance = ( float )( scale * best.fDistance );
		sirMeshCollisionHit_t localHit = {};
		if ( entry.pBVH->raycast( local, &localHit ) != SIR_MESH_OK )
			continue;
		const float distance = ( float )( localHit.fDistance / scale );
		if ( distance > best.fDistance || ( found && distance == best.fDistance && entry.nInstanceId >= best.nInstanceId ) )
			continue;
		float normal[3] = {};
		double lengthSquared = 0.0;
		for ( uint32_t axis = 0; axis < 3; ++axis ) {
			normal[axis] = entry.inverse[axis] * localHit.normal[0] + entry.inverse[4 + axis] * localHit.normal[1] +
			               entry.inverse[8 + axis] * localHit.normal[2];
			lengthSquared += ( double )normal[axis] * normal[axis];
		}
		if ( !std::isfinite( lengthSquared ) || lengthSquared <= 0.0 )
			continue;
		const float invNormalLength = ( float )( 1.0 / std::sqrt( lengthSquared ) );
		best.nInstanceId = entry.nInstanceId;
		best.keySource = entry.pSource->getSourceKey();
		best.nTriangle = localHit.nTriangle;
		best.nMaterialId = localHit.nMaterialId;
		best.fDistance = distance;
		for ( uint32_t axis = 0; axis < 3; ++axis ) {
			best.position[axis] = ray.origin[axis] + distance * worldDirection[axis];
			best.normal[axis] = normal[axis] * invNormalLength * entry.fWindingSign;
		}
		found = true;
	}
	if ( !found )
		return SIR_WORLD_MESH_NOT_FOUND;
	*pHit = best;
	return SIR_WORLD_MESH_OK;
}

sirWorldMeshQueryResult_t SIRWorldMeshQuery::shutdown( void ) noexcept
{
	if ( !m_bReady )
		return SIR_WORLD_MESH_NOT_READY;
	if ( m_nCount != 0 )
		return SIR_WORLD_MESH_BUSY;
	m_pEntries = nullptr;
	m_nOwnerId = 0;
	m_nNextGeneration = 0;
	m_nCapacity = 0;
	m_nCount = 0;
	m_nWorldGeneration = 0;
	m_bReady = false;
	return SIR_WORLD_MESH_OK;
}
