#ifndef __VK_COMMON_H__
#define __VK_COMMON_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#if !defined(VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR)
    #define VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR 0x00000001
#endif

#if defined(SIRENGINE_PLATFORM_LINUX)
    #include <X11/X.h>
    #include <X11/Xlib.h>
    #include <X11/Xresource.h>

//    #include <vulkan/vulkan_xlib.h>
//    #include <vulkan/vulkan_xlib_xrandr.h>
//    #include <vulkan/vulkan_xcb.h>
#elif defined(SIRENGINE_PLATFORM_ANDROID)
    #include <vulkan/vulkan_android.h>
#elif defined(SIRENGINE_PLATFORM_WINDOWS)
    #include <vulkan/vulkan_win32.h>
#endif

#define VK_MAX_FRAMES_IN_FLIGHT 2
#define SIRENGINE_USE_VK_DESCRIPTOR_POOLS 1

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *pFormats;
    VkPresentModeKHR *pPresentModes;
    uint32_t nFormats;
    uint32_t nPresentModes;
} SwapChainSupportInfo_t;

typedef struct {
    bool32 bHasGraphicsFamily;
    uint32_t nGraphicsFamily;
    bool32 bHasPresentFamily;
    uint32_t nPresentFamily;

    bool IsComplete( void ) const {
        return bHasGraphicsFamily && bHasPresentFamily;
    }
} QueueFamilyIndices_t;

class VKShaderBuffer;

typedef struct {
    VkPipeline hPipeline;
    VkPipelineLayout hLayout;
    VkDescriptorSet *pDescriptorSets;
    VkDescriptorSetLayout *pDescriptorSetLayouts;
#if defined(SIRENGINE_USE_VK_DESCRIPTOR_POOLS)
    VkDescriptorPool hDescriptorPool;
#else
    VkDescriptorUpdateTemplate hUpdateTemplate;
#endif

    VKShaderBuffer **pShaderBuffers;
    uint64_t nShaderBufferCount;

    VkVertexInputAttributeDescription *pAttributes;
    VkVertexInputBindingDescription *pBindings;

    uint32_t nAttribCount;
    uint32_t nBindingCount;
    uint32_t nUniformCount;
} VKPipelineSet_t;

typedef struct VKDescriptorCache {
    VkDescriptorSet *pSets;
    VkDescriptorSetLayout *pLayouts;
    VkDescriptorPool hPool;

    uint64_t nSetCount;
} VKDescriptorCache_t;

extern void VkError( const char *pCall, VkResult nResult );

SIRENGINE_FORCEINLINE void VkCall( const char *pCall, VkResult nResult )
{
    if ( nResult != VK_SUCCESS ) {
        VkError( pCall, nResult );
    }
}

#define VK_CALL( call ) VkCall( #call, call )

#endif