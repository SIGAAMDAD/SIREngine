// Copyright (c) 2023-2026 Terrain3D Contributors.
#ifndef TERRAIN3D_OBJECT_BYTES_H
#define TERRAIN3D_OBJECT_BYTES_H
#include <stddef.h>
#include <string.h>
#include <type_traits>

// Explicit wire/state reset for live trivially-copyable, trivially destructible
// objects. This preserves historical all-zero byte images, including padding.
// Not suitable for owning class types or Godot built-in objects.
template <typename T>
inline void t3dClearTrivialObjectBytes( T& rObject )
{
	static_assert( std::is_trivially_copyable<T>::value, "Byte reset requires trivially copyable storage" );
	static_assert( std::is_trivially_destructible<T>::value, "Byte reset requires trivial destruction" );
	memset( static_cast<void *>( &rObject ), 0, sizeof( T ) );
}
template <typename T, size_t N>
inline void t3dClearTrivialArrayBytes( T ( &rObjects )[N] )
{
	static_assert( std::is_trivially_copyable<T>::value, "Array byte reset requires trivially copyable elements" );
	static_assert( std::is_trivially_destructible<T>::value, "Array byte reset requires trivial destruction" );
	memset( static_cast<void *>( rObjects ), 0, sizeof( rObjects ) );
}
template <typename T>
inline void t3dClearTrivialSpanBytes( T *pObjects, size_t nCount )
{
	static_assert( std::is_trivially_copyable<T>::value, "Span byte reset requires trivially copyable elements" );
	static_assert( std::is_trivially_destructible<T>::value, "Span byte reset requires trivial destruction" );
	if ( nCount != 0u ) {
		memset( static_cast<void *>( pObjects ), 0, nCount * sizeof( T ) );
	}
}
#endif // TERRAIN3D_OBJECT_BYTES_H
