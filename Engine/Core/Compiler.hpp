#ifndef __COMPILER_CONFIG_HPP__
#define __COMPILER_CONFIG_HPP__

#pragma once

#include "Platform.hpp"

#if defined(__ARMCC_VERSION)
    #define SIRENGINE_COMPILER_RVCT 1
    #define SIRENGINE_COMPILER_ARM 1
    #define SIRENGINE_COMPILER_VERSION __ARMCC_VERSION
    #define SIRENGINE_COMPILER_NAME "RVCT"
#elif defined(__GNUC__)
    #if defined(__MINGW32__) || defined(__MINGW64__)
        #define SIRENGINE_COMPILER_NAME "MingW"
    #elif defined(__GNUG__)
        #define SIRENGINE_COMPILER_NAME "GCC"
    #endif
    #define SIRENGINE_COMPILER_GCC 1
#elif defined(_MSC_VER)
    #define SIRENGINE_COMPILER_NAME "MSVC"
#elif defined(__clang__)
    #define SIRENGINE_COMPILER_NAME "clang"
#endif

#if !defined(SIRENGINE_DLL_EXPORT)
    #if defined(SIRENGINE_PLATFORM_WINDOWS)
        #if defined(SIRENGINE_DLL_COMPILE)
            #define SIRENGNIE_DLL_EXPORT __declspec(dllexport)
        #else
            #define SIRENGINE_DLL_IMPORT __declspec(dllimport)
        #endif
    #elif defined(SIRENGINE_COMPILER_GCC)
        #define SIRENGINE_DLL_EXPORT __attribute__((visibility("default")))
    #endif
#endif

#if !defined(SIRENGINE_NORETURN)
    #if defined(SIRENGINE_COMPILER_MSVC)
        #define SIRENGINE_NORETURN __declspec(noreturn)
    #elif defined(SIRENGINE_COMPILER_GCC)
        #define SIRENGINE_NORETURN __attribute__((noreturn))
    #endif
#endif

#if !defined(SIRENGINE_ATTRIBUTE)
    #if defined(SIRENGINE_COMPILER_GCC)
        #define SIRENGINE_ATTRIBUTE(x) __attribute__((x))
    #else
        #define SIRENGINE_ATTRIBUTE(x)
    #endif
#endif

#if !defined(SIRENGINE_FORCEINLINE)
    #if defined(SIRENGINE_COMPILER_MSVC)
        #define SIRENGINE_FORCEINLINE __forceinline
    #elif defined(SIRENGINE_COMPILER_GCC)
        #define SIRENGINE_FORCEINLINE __attribute__((always_inline))
    #endif
#endif

#if !defined(SIRENGINE_EXPORT)
    #if defined(SIRENGINE_COMPILER_MSVC)
        #if defined(SIRENGINE_DLL_EXPORTS)
            #define SIRENGINE_EXPORT __declspec(dllexport)
        #else
            #define SIRENGINE_EXPORT __declspec(dllimport)
        #endif
    #elif defined(SIRENGINE_COMPILER_GCC)
        #define SIRENGINE_EXPORT __attribute__((visibility("default")))
    #endif
#endif

#if defined(assert)
    #undef assert
#endif
#define assert( x ) SIREngine_Assert( x )
#define SIREngine_Assert( x ) ( ( x ) ? (void)0 : SIREngine_AssertionFailure( #x, __FILE__, __LINE__ ) )

#define SIRENGINE_PAD( base, alignment ) ( ( ( base ) + ( alignment ) - 1 ) & ~( ( alignment ) - 1 ) )

#define SIRENGINE_INT8_MAX INT8_MAX
#define SIRENGINE_INT16_MAX INT16_MAX
#define SIRENGINE_INT32_MAX INT32_MAX
#define SIRENGINE_INT64_MAX INT64_MAX
#define SIRENGINE_UINT8_MAX UINT8_MAX
#define SIRENGINE_UINT16_MAX UINT16_MAX
#define SIRENGINE_UINT32_MAX UINT32_MAX
#define SIRENGINE_UINT64_MAX UINT64_MAX

#endif