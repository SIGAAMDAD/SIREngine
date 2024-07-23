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

#endif