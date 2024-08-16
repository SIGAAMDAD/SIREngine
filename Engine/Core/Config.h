#ifndef __SIRENGINE_CONFIG_HPP__
#define __SIRENGINE_CONFIG_HPP__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#if !defined(SIRENGINE_BUILD_TYPE)
    #error "SIRENGINE_BUILD_TYPE must be defined!"
#endif

#define SIRENGINE_BUILD_TYPE_ENGINE 0
#define SIRENGINE_BUILD_TYPE_EDITOR 1

#if SIRENGINE_BUILD_TYPE == SIRENGINE_BUILD_TYPE_ENGINE
    #define SIRENGINE_BUILD_ENGINE 1
#elif SIRENGINE_BUILD_TYPE == SIRENGINE_BUILD_TYPE_EDITOR
    #define SIRENGINE_BUILD_EDITOR 1
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

#if !defined(MAX_RESOURCE_PATH)
    #define MAX_RESOURCE_PATH 128
#endif

#define SIRENGINE_MAX_COOP_PLAYERS 4

#define USE_ARENA_ALLOC 1

#endif