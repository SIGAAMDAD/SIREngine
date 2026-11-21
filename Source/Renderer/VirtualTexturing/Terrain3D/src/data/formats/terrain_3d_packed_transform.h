// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.
// Compact persistent instance transforms: float32 origins and binary16 bases.
#ifndef TERRAIN3D_PACKED_TRANSFORM_H
#define TERRAIN3D_PACKED_TRANSFORM_H

#include "Terrain3DTypes.h"

#include "core/containers/terrain_3d_slab_array.h"
#include "data/formats/terrain_3d_half_buffer.h"
#include "eastl_config.h"

#include <stddef.h>
#include <stdint.h>
#include <godot_cpp/variant/transform3d.hpp>

#if defined( GDEXTENSION ) && !defined( GODOT_MODULE )
using namespace godot;
#endif

#define T3D_INSTANCE_BEHAVIOR_MASK ( ( uint16_t )( FOLLOW_TERRAIN_HEIGHT | FOLLOW_TERRAIN_NORMAL | MANUAL_ORIENTATION ) )
#define T3D_INSTANCE_LEGACY_BEHAVIOR ( ( uint16_t )( FOLLOW_TERRAIN_HEIGHT | MANUAL_ORIENTATION ) )

// Two transforms fit exactly in one 64-byte cacheline. The 32-byte alignment
// prevents an individual transform from straddling cachelines in EASTL storage.
typedef struct alignas( 32 ) t3dPackedTransform_s
{
	float m_szOrigin[3] = { 0.0f, 0.0f, 0.0f };
	uint16_t m_szBasis[9] = {};
	uint16_t m_nReserved = 0u;

	EA_FORCE_INLINE t3dPackedTransform_s( void )
	{
		set( Transform3D() );
	}

	EA_FORCE_INLINE explicit t3dPackedTransform_s( const Transform3D& rTransform )
	{
		set( rTransform );
	}

	void set( const Transform3D& rTransform )
	{
		const uint16_t nReserved = m_nReserved;
		m_szOrigin[0] = ( float )( rTransform.origin.x );
		m_szOrigin[1] = ( float )( rTransform.origin.y );
		m_szOrigin[2] = ( float )( rTransform.origin.z );
		for ( int row = 0; row < 3; ++row ) {
			m_szBasis[row * 3 + 0] = t3dHalfEncode( ( float )( rTransform.basis[row].x ) );
			m_szBasis[row * 3 + 1] = t3dHalfEncode( ( float )( rTransform.basis[row].y ) );
			m_szBasis[row * 3 + 2] = t3dHalfEncode( ( float )( rTransform.basis[row].z ) );
		}
		m_nReserved = nReserved;
	}

	EA_FORCE_INLINE void setWithBehaviorFlags( const Transform3D& rTransform, const uint16_t nBehaviorFlags )
	{
		m_nReserved = nBehaviorFlags & T3D_INSTANCE_BEHAVIOR_MASK;
		set( rTransform );
	}

	EA_FORCE_INLINE uint16_t getBehaviorFlags( void ) const
	{
		return m_nReserved & T3D_INSTANCE_BEHAVIOR_MASK;
	}

	EA_FORCE_INLINE void setBehaviorFlags( const uint16_t nBehaviorFlags )
	{
		m_nReserved = ( m_nReserved & ( uint16_t )( ~T3D_INSTANCE_BEHAVIOR_MASK ) ) | ( nBehaviorFlags & T3D_INSTANCE_BEHAVIOR_MASK );
	}

	EA_FORCE_INLINE bool hasBehaviorFlag( const t3dInstanceFollowBehavior_t eFlag ) const
	{
		return ( getBehaviorFlags() & ( uint16_t )( eFlag ) ) != 0u;
	}

	Transform3D get( void ) const
	{
		Basis unpacked_basis;
		for ( int row = 0; row < 3; ++row ) {
			unpacked_basis[row] = Vector3( t3dHalfDecode( m_szBasis[row * 3 + 0] ), t3dHalfDecode( m_szBasis[row * 3 + 1] ),
			                               t3dHalfDecode( m_szBasis[row * 3 + 2] ) );
		}
		return Transform3D( unpacked_basis, Vector3( m_szOrigin[0], m_szOrigin[1], m_szOrigin[2] ) );
	}
} t3dPackedTransform_t;

static_assert( sizeof( t3dPackedTransform_s ) == 32u, "Packed instance transforms must remain 32 bytes." );
static_assert( alignof( t3dPackedTransform_s ) == 32u, "Packed instance transforms must remain 32-byte aligned." );

class Terrain3DTransformArrayReference
{
public:
	EA_FORCE_INLINE explicit Terrain3DTransformArrayReference( t3dPackedTransform_s *const pTransform ) : m_pTransform( pTransform )
	{
	}

	Terrain3DTransformArrayReference& operator=( const Transform3D& rTransform )
	{
		m_pTransform->set( rTransform );
		return *this;
	}

	Terrain3DTransformArrayReference& operator=( const Terrain3DTransformArrayReference& rOther )
	{
		*m_pTransform = *rOther.m_pTransform;
		return *this;
	}

	EA_FORCE_INLINE operator Transform3D( void ) const
	{
		return m_pTransform->get();
	}

private:
	t3dPackedTransform_s *m_pTransform = NULL;
};

class Terrain3DTransformArray
{
public:
	Terrain3DTransformArray( void ) = default;
	Terrain3DTransformArray( const Terrain3DTransformArray& ) = delete;
	Terrain3DTransformArray& operator=( const Terrain3DTransformArray& ) = delete;
	Terrain3DTransformArray( Terrain3DTransformArray&& ) noexcept = default;
	Terrain3DTransformArray& operator=( Terrain3DTransformArray&& ) noexcept = default;

	EA_FORCE_INLINE void bind( Terrain3DHighWaterSlab<t3dPackedTransform_s> *const pSlab )
	{
		m_storage.bind( pSlab );
	}

	EA_FORCE_INLINE void rebind( Terrain3DHighWaterSlab<t3dPackedTransform_s> *const pSlab )
	{
		m_storage.rebind( pSlab );
	}

	EA_FORCE_INLINE bool relocateExact( Terrain3DHighWaterSlab<t3dPackedTransform_s> *const pSlab )
	{
		return m_storage.relocateExact( pSlab );
	}

	EA_FORCE_INLINE bool reserve( const size_t nCount )
	{
		return m_storage.reserve( nCount );
	}

	EA_FORCE_INLINE bool reserveExact( const size_t nCount )
	{
		return m_storage.reserveExact( nCount );
	}

	EA_FORCE_INLINE bool resize( const size_t nCount )
	{
		return m_storage.resizeUninitialized( nCount );
	}

	EA_FORCE_INLINE void clear( void )
	{
		m_storage.clear();
	}

	EA_FORCE_INLINE void shrinkToFit( void )
	{
	}

	EA_FORCE_INLINE bool pushBack( const Transform3D& rTransform )
	{
		t3dPackedTransform_s packed( rTransform );
		packed.setBehaviorFlags( T3D_INSTANCE_LEGACY_BEHAVIOR );
		return m_storage.pushBack( packed );
	}

	EA_FORCE_INLINE bool pushBack( const Transform3D& rTransform, const uint16_t nBehaviorFlags )
	{
		t3dPackedTransform_s packed( rTransform );
		packed.setBehaviorFlags( nBehaviorFlags );
		return m_storage.pushBack( packed );
	}

	EA_FORCE_INLINE size_t size( void ) const
	{
		return m_storage.size();
	}

	EA_FORCE_INLINE size_t capacity( void ) const
	{
		return m_storage.capacity();
	}

	EA_FORCE_INLINE bool empty( void ) const
	{
		return m_storage.empty();
	}

	Terrain3DTransformArrayReference operator[]( const size_t iIndex )
	{
		return Terrain3DTransformArrayReference( &m_storage[iIndex] );
	}

	Transform3D operator[]( const size_t iIndex ) const
	{
		return m_storage[iIndex].get();
	}

	EA_FORCE_INLINE const t3dPackedTransform_s *data( void ) const
	{
		return m_storage.data();
	}

	EA_FORCE_INLINE t3dPackedTransform_s *data( void )
	{
		return m_storage.data();
	}

	EA_FORCE_INLINE Vector3 getOrigin( const size_t iIndex ) const
	{
		const t3dPackedTransform_s& packed = m_storage[iIndex];
		return Vector3( packed.m_szOrigin[0], packed.m_szOrigin[1], packed.m_szOrigin[2] );
	}

	EA_FORCE_INLINE Vector3 getUpAxis( const size_t iIndex ) const
	{
		const t3dPackedTransform_s& packed = m_storage[iIndex];
		return Vector3( t3dHalfDecode( packed.m_szBasis[1] ), t3dHalfDecode( packed.m_szBasis[4] ), t3dHalfDecode( packed.m_szBasis[7] ) );
	}

	EA_FORCE_INLINE uint16_t getBehaviorFlags( const size_t iIndex ) const
	{
		return m_storage[iIndex].getBehaviorFlags();
	}

	EA_FORCE_INLINE void setBehaviorFlags( const size_t iIndex, const uint16_t nBehaviorFlags )
	{
		m_storage[iIndex].setBehaviorFlags( nBehaviorFlags );
	}

	EA_FORCE_INLINE void setTransformAndBehaviorFlags( const size_t iIndex, const Transform3D& rTransform, const uint16_t nBehaviorFlags )
	{
		m_storage[iIndex].setWithBehaviorFlags( rTransform, nBehaviorFlags );
	}

	EA_FORCE_INLINE void setOriginY( const size_t iIndex, const real_t fY )
	{
		m_storage[iIndex].m_szOrigin[1] = ( float )( fY );
	}

	EA_FORCE_INLINE void rescaleOriginXz( const size_t iIndex, const real_t fOldSpacing, const real_t fNewSpacing )
	{
		t3dPackedTransform_s& packed = m_storage[iIndex];
		packed.m_szOrigin[0] = ( float )( ( real_t )( packed.m_szOrigin[0] ) / fOldSpacing * fNewSpacing );
		packed.m_szOrigin[2] = ( float )( ( real_t )( packed.m_szOrigin[2] ) / fOldSpacing * fNewSpacing );
	}

private:
	Terrain3DSlabArray<t3dPackedTransform_s> m_storage;
};

#endif // TERRAIN3D_PACKED_TRANSFORM_H
