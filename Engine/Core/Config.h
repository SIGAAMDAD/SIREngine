#ifndef __SIRENGINE_CONFIG_HPP__
#define __SIRENGINE_CONFIG_HPP__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#if !defined(SIRENGINE_MALLOC_OVERRIDE)
    #define SIRENGINE_MALLOC( size ) Mem_Alloc( size )
#endif

#if !defined(SIRENGINE_FREE_OVERRIDE)
    #define SIRENGINE_FREE( ptr ) Mem_Free( ptr )
#endif

#if !defined(SIRENGINE_MATH_OVERRIDE)
    #define SIRENGINE_USE_GLM_MATH
#endif

#if !defined(SIRENGINE_USE_GLM)
    #define SIRENGINE_USE_MATHLIB 1
#endif

#define USE_ARENA_ALLOC 1

#endif