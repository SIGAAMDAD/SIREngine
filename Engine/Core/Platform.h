#ifndef __PLATFORM_CONFIG_H__
#define __PLATFORM_CONFIG_H__

#pragma once

#if defined(__CYGWIN__)
    #define SIRENGINE_PLATFORM_CYGWIN 1
    #define SIRENGINE_PLATFORM_DESKTOP 1
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
    #define SIRENGINE_PLATFORM_MINGW 1
    #define SIRENGINE_PLATFORM_DESKTOP 1
#endif

#if defined(_WIN32)
    #define SIRENGINE_PLATFORM_NAME "Windows"
    #define SIRENGINE_PLATFORM_WINDOWS 1

    #define SIRENGINE_STDOUT_HANDLE (void *)STDOUT_HANDLE
    #define SIRENGINE_STDERR_HANDLE (void *)STDERR_HANDLE
#elif defined(__unix__)
    #if defined(__ANDROID__)
        #define SIRENGINE_PLATFORM_NAME "Android"
        #define SIRENGINE_PLATFORM_ANDROID 1
    #elif defined(__linux__)
        #define SIRENGINE_PLATFORM_NAME "Linux"
        #define SIRENGINE_PLATFORM_LINUX 1
    #endif

    #include <unistd.h>

    #if !defined(O_BINARY)
        #define O_BINARY 0
    #endif

    #define SIRENGINE_STDOUT_HANDLE (void *)STDOUT_FILENO
    #define SIRENGINE_STDERR_HANDLE (void *)STDERR_FILENO
#endif

#endif