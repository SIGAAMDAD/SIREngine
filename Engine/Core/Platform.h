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
#elif defined(__unix__)
    #if defined(__ANDROID__)
        #define SIRENGINE_PLATFORM_NAME "Android"
        #define SIRENGINE_PLATFORM_ANDROID 1
    #elif defined(__linux__)
        #define SIRENGINE_PLATFORM_NAME "Linux"
        #define SIRENGINE_PLATFORM_LINUX 1
    #endif
#endif

#endif