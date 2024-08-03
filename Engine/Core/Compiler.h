#ifndef __COMPILER_CONFIG_HPP__
#define __COMPILER_CONFIG_HPP__

#pragma once

#include "Platform.h"

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

#if defined(__cplusplus)
    #if __cplusplus == 1
        #error Please use a modern version of C++
    #endif
    #if __cplusplus >= 199711L
        #define SIRENGINE_CPP98
    #endif
    #if __cplusplus >= 201103L
        #define SIRENGINE_CPP11
    #endif
    #if __cplusplus >= 201402L
        #define SIRENGINE_CPP14
    #endif
    #if __cplusplus >= 201703L
        #define SIRENGINE_CPP17
    #endif
    #if __cplusplus >= 202002L
        #define SIRENGINE_CPP20
    #endif
    #if __cplusplus >= 202302L
        #define SIRENGINE_CPP23
    #endif
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
    #elif defined(SIRENGINE_CPP11)
        #define SIRENGINE_NORETURN [[noreturn]]
    #endif
#endif

#if !defined(SIRENGINE_DEPRECATED)
    #if defined(SIRENGINE_COMPILER_GCC)
        #if defined(SIRENGINEC_CPP14)
            #define SIRENGINE_DEPRECATED( reason ) [[deprecated( reason )]]
        #else
            #define SIRENGINE_DEPRECATED( reason ) __attribute__((deprecated))
        #endif
    #elif defined(SIRENGINE_CPP14)
        #define SIRENGINE_DEPRECATED( reason ) [[deprecated( reason )]]
    #else
        #define SIRENGINE_DEPRECATED( reason )
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
        #define SIRENGINE_FORCEINLINE __attribute__((always_inline)) inline
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

#if !defined(SIRENGINE_CONSTEXPR)
    #if defined(SIRENGINE_COMPILER_CLANG)
        #if __has_feature(__cpp_constexpr) || defined(SIRENGINE_CPP14)
            #define SIRENGINE_CONSTEXPR constexpr
        #endif
    #else
        #if defined(SIRENGINE_CPP14)
            #define SIRENGINE_CONSTEXPR constexpr
        #else
            #define SIRENGINE_CONSTEXPR
        #endif
    #endif
#endif

#if !defined(SIRENGINE_STATIC_ASSERT)
    #if defined(SIRENGINE_CPP11)
        #define SIRENGINE_STATIC_ASSERT( x, str ) static_assert( x, str )
    #else
        #define SIRENGINE_STATIC_ASSERT( x, str ) if ( !( x ) ) assert( ( 0 ) && str )
    #endif
#endif

#if !defined(SIRENGINE_DISABLE_ALL_VC_WARNINGS)
    #if defined(SIRENGINE_COMPILER_MSVC)
        #define SIRENGINE_DISABLE_ALL_VC_WARNINGS() \
            __pragma( warning( push, 0 ) ) \
            __pragma( warning( disable :  ) )
    #else
        #define SIRENGINE_DISABLE_ALL_VC_WARNINGS()
    #endif
#endif

#if !defined(SIRENGINE_RESTORE_ALL_VC_WARNINGS)
    #if defined(SIRENGINE_COMPILER_MSVC)
        #define SIRENGINE_RESTORE_ALL_VC_WARNINGS() \
            __pragma( warning( pop ) )
    #else
        #define SIRENGINE_RESTORE_ALL_VC_WARNINGS()
    #endif
#endif

#if !defined(SIRENGINE_MAX_OSPATH)
    #if defined(SIRENGINE_PLATFORM_WINDOWS)
        #define SIRENGINE_MAX_OSPATH MAX_PATH
    #else
        #define SIRENGINE_MAX_OSPATH PATH_MAX
    #endif
#endif

#if defined(SIRENGINE_COMPILER_MSVC) || defined(SIRENGINE_COMPILER_GCC) || defined(SIRENGINE_PLATFORM_APPLE)
    #define SIRENGINE_PRAGMA_ONCE_SUPPORTED 1
#endif

#if !defined(SIRENGINE_VA_COPY_ENABLED)
	#if ((defined(__GNUC__) && (__GNUC__ >= 3)) || defined(__clang__)) && (!defined(__i386__) || defined(__x86_64__)) && !defined(__ppc__) && !defined(__PPC__) && !defined(__PPC64__)
		#define SIRENGINE_VA_COPY_ENABLED 1
	#else
		#define SIRENGINE_VA_COPY_ENABLED 0
	#endif
#endif

#if !defined(SIRENGINE_VSPRINTF_OVERRIDE)
    #define SIRENGINE_VSNPRINTF_OVERRIDE stbsp_vsnprintf
#endif

#define SIRENGINE_BIT( x ) ( 1 << ( x ) )

#if defined(SIRENGINE_COMPILER_GCC)
    // We can't use GCC 4's __builtin_offsetof because it mistakenly complains about non-PODs that are really PODs.
//    #define SIREngine_offsetof( type, member ) ((size_t)(((uintptr_t)&reinterpret_cast<const volatile char&>((((type*)65536)->member))) - 65536))
    #define SIREngine_offsetof( type, member ) ((size_t)&(((type*)0)->member))
#else
    #define SIREngine_offsetof( type, member ) offsetof( type, member )
#endif

#ifdef offsetof
    #undef offsetof
    #define offsetof( type, member ) SIREngine_offsetof( type, member )
#endif

#define SIRENGINE_EXPORT_DEMANGLE extern "C"

SIRENGINE_EXPORT_DEMANGLE void SIREngine_AssertionFailure( const char *pAssertion, const char *pFileName, uint64_t nLineNumber );

#if defined(assert)
    #undef assert
#endif
#define assert( x ) SIREngine_Assert( x )
#define SIREngine_Assert( x ) ( ( x ) ? (void)0 : SIREngine_AssertionFailure( #x, __FILE__, __LINE__ ) )


#define SIRENGINE_PAD( nBase, nAlignment ) ( ( ( nBase ) + ( nAlignment ) - 1 ) & ~( ( nAlignment ) - 1 ) )
#define SIREngine_Vsnprintf SIRENGINE_VSNPRINTF_OVERRIDE

#define SIRENGINE_INT8_MIN INT8_MIN
#define SIRENGINE_INT16_MIN INT16_MIN
#define SIRENGINE_INT32_MIN INT32_MIN
#define SIRENGINE_INT64_MIN INT64_MIN
#define SIRENGINE_UINT8_MIN UINT8_MIN
#define SIRENGINE_UINT16_MIN UINT16_MIN
#define SIRENGINE_UINT32_MIN UINT32_MIN
#define SIRENGINE_UINT64_MIN UINT64_MIN
#define SIRENGINE_INT8_MAX INT8_MAX
#define SIRENGINE_INT16_MAX INT16_MAX
#define SIRENGINE_INT32_MAX INT32_MAX
#define SIRENGINE_INT64_MAX INT64_MAX
#define SIRENGINE_UINT8_MAX UINT8_MAX
#define SIRENGINE_UINT16_MAX UINT16_MAX
#define SIRENGINE_UINT32_MAX UINT32_MAX
#define SIRENGINE_UINT64_MAX UINT64_MAX

#endif