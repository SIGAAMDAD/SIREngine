// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.
// Compact IEEE-754 binary16 storage for immutable per-cell coefficients.
#ifndef TERRAIN3D_HALF_BUFFER_H
#define TERRAIN3D_HALF_BUFFER_H

#include "core/config/terrain_3d_compiler.h"
#include "data/formats/terrain_3d_half.h"
#include "memory/arena/terrain_3d_linear_arena.h"

#include <EASTL/algorithm.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class Terrain3DHalfBufferReference
{
public:
	EA_FORCE_INLINE explicit Terrain3DHalfBufferReference( uint16_t *const pStorage ) : m_pStorage( pStorage )
	{
	}

	Terrain3DHalfBufferReference& operator=( const float fValue )
	{
		*m_pStorage = t3dHalfEncode( fValue );
		return *this;
	}

	Terrain3DHalfBufferReference& operator=( const Terrain3DHalfBufferReference& rOther )
	{
		return operator=( ( float )( rOther ) );
	}

	EA_FORCE_INLINE operator float( void ) const
	{
		return t3dHalfDecode( *m_pStorage );
	}

private:
	uint16_t *m_pStorage = NULL;
};

class Terrain3DHalfBuffer
{
public:
	bool assign( Terrain3DLinearArena& rArena, const size_t nCount, const float fValue )
	{
		clear();
		if ( nCount == 0u ) {
			return true;
		}
		m_pData = rArena.allocateArray<uint16_t>( nCount );
		if ( !m_pData ) {
			return false;
		}
		m_nSize = nCount;
		const uint16_t encoded = t3dHalfEncode( fValue );
		eastl::fill_n( m_pData, m_nSize, encoded );
		return true;
	}

	EA_FORCE_INLINE void clear( void )
	{
		m_pData = NULL;
		m_nSize = 0u;
	}

	Terrain3DHalfBufferReference operator[]( const size_t iIndex )
	{
		return Terrain3DHalfBufferReference( m_pData + iIndex );
	}

	float operator[]( const size_t iIndex ) const
	{
		return t3dHalfDecode( m_pData[iIndex] );
	}

	EA_FORCE_INLINE size_t size( void ) const
	{
		return m_nSize;
	}

	EA_FORCE_INLINE bool empty( void ) const
	{
		return m_nSize == 0u;
	}

	EA_FORCE_INLINE uint16_t *data( void )
	{
		return m_pData;
	}

	EA_FORCE_INLINE const uint16_t *data( void ) const
	{
		return m_pData;
	}

private:
	uint16_t *m_pData = NULL;
	size_t m_nSize = 0u;
};

#endif // TERRAIN3D_HALF_BUFFER_H
