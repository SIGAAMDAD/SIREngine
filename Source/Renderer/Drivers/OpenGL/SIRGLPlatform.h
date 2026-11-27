/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_GL_PLATFORM_H__
#define __SIR_GL_PLATFORM_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include "SIRGLShared.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// The window system creates and makes the context current; this only loads
// procedure addresses. EGL and GLX need different platform resolvers on Linux.
enum class sirGLNativeAPI_t
{
    GLX,
    EGL
};

struct sirGLPlatform_t
{
    sirGLPlatform_t() noexcept = default;
    sirGLPlatform_t(const sirGLPlatform_t&) = delete;
    sirGLPlatform_t& operator=(const sirGLPlatform_t&) = delete;
    ~sirGLPlatform_t() noexcept { close(); }

    inline bool open(sirGLNativeAPI_t api = sirGLNativeAPI_t::GLX) noexcept
    {
        close();
#if defined(_WIN32)
        (void)api;
        m_Library = LoadLibraryA("opengl32.dll");
        if (m_Library == nullptr)
        {
            return false;
        }
        m_GetProc = reinterpret_cast<void*>(GetProcAddress(m_Library, "wglGetProcAddress"));
        if (m_GetProc == nullptr)
        {
            close();
            return false;
        }
        return true;
#elif defined(__APPLE__)
        (void)api;
        m_Library = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_LOCAL);
        return m_Library != nullptr;
#else
        const char* library = api == sirGLNativeAPI_t::EGL ? "libEGL.so.1" : "libGL.so.1";
        const char* symbol = api == sirGLNativeAPI_t::EGL ? "eglGetProcAddress" : "glXGetProcAddressARB";
        m_Library = dlopen(library, RTLD_LAZY | RTLD_LOCAL);
        if (m_Library == nullptr)
        {
            return false;
        }
        m_Api = api;
        m_GetProc = dlsym(m_Library, symbol);
        if (m_GetProc == nullptr)
        {
            close();
            return false;
        }
        return true;
#endif
    }

    inline void close() noexcept
    {
        m_GetProc = nullptr;
#if defined(_WIN32)
        if (m_Library != nullptr)
        {
            FreeLibrary(m_Library);
        }
#else
        if (m_Library != nullptr)
        {
            dlclose(m_Library);
        }
#endif
        m_Library = nullptr;
    }

    inline sirGLProcResolver_t resolver() noexcept
    {
        sirGLProcResolver_t result;
        result.user = this;
        result.resolve = &sirGLPlatform_t::resolveStatic;
        return result;
    }

private:
    static inline void* resolveStatic(void* user, const char* name) noexcept
    {
        return static_cast<sirGLPlatform_t*>(user)->getProc(name);
    }

    inline void* getProc(const char* name) const noexcept
    {
        if (m_Library == nullptr || name == nullptr)
        {
            return nullptr;
        }
#if defined(_WIN32)
        using WGLGetProc = void* (WINAPI*)(const char*);
        void* result = reinterpret_cast<WGLGetProc>(m_GetProc)(name);
        const auto address = reinterpret_cast<std::uintptr_t>(result);
        if (result == nullptr || address == 1 || address == 2 || address == 3 ||
            address == static_cast<std::uintptr_t>(-1))
        {
            result = reinterpret_cast<void*>(GetProcAddress(m_Library, name));
        }
        return result;
#elif defined(__APPLE__)
        return dlsym(m_Library, name);
#else
        using GLXGetProc = void* (*)(const unsigned char*);
        using EGLGetProc = void* (*)(const char*);
        void* result = m_Api == sirGLNativeAPI_t::EGL ?
            reinterpret_cast<EGLGetProc>(m_GetProc)(name) :
            reinterpret_cast<GLXGetProc>(m_GetProc)(reinterpret_cast<const unsigned char*>(name));
        if (result == nullptr)
        {
            result = dlsym(m_Library, name);
        }
        return result;
#endif
    }

#if defined(_WIN32)
    HMODULE m_Library = nullptr;
#else
    void* m_Library = nullptr;
#endif
    void* m_GetProc = nullptr;
    sirGLNativeAPI_t m_Api = sirGLNativeAPI_t::GLX;
};

#endif // __SIR_GL_PLATFORM_H__
