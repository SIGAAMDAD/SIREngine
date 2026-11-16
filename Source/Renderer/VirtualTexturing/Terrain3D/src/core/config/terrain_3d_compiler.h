#ifndef TERRAIN3D_COMPILER_H
#define TERRAIN3D_COMPILER_H

// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef EA_FORCE_INLINE
#if defined( _MSC_VER )
#define EA_FORCE_INLINE __forceinline
#elif defined( __GNUC__ ) || defined( __clang__ )
#define EA_FORCE_INLINE inline __attribute__( ( always_inline ) )
#else
#define EA_FORCE_INLINE inline
#endif
#endif

#ifndef T3D_ARRAY_COUNT
#define T3D_ARRAY_COUNT( a ) ( sizeof( a ) / sizeof( ( a )[0] ) )
#endif

#endif // TERRAIN3D_COMPILER_H
