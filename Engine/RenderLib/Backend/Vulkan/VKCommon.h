#ifndef __VK_COMMON_H__
#define __VK_COMMON_H__

#pragma once

#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKContext.h"

#if !defined(VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR)
    #define VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR 0x00000001
#endif

#if defined(SIRENGINE_PLATFORM_LINUX)
    #include <vulkan/vulkan_xlib.h>
    #include <vulkan/vulkan_xlib_xrandr.h>
    #include <vulkan/vulkan_xcb.h>
#elif defined(SIRENGINE_PLATFORM_ANDROID)
    #include <vulkan/vulkan_android.h>
#elif defined(SIRENGINE_PLATFORM_WINDOWS)
    #include <vulkan/vulkan_win32.h>
#endif

#endif