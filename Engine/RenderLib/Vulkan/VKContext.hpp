#ifndef __VK_CONTEXT_HPP__
#define __VK_CONTEXT_HPP__

#pragma once

#include "VKCommon.hpp"

class VKContext : public IRenderContext
{
public:
    VKContext( const ApplicationInfo_t& appInfo );
    virtual ~VKContext() override;

    VkDevice GetDevice( void );
    VmaAllocator GetAllocator( void );
private:
    void InitWindowInstance( void );
    void InitPhysicalVKDevice( void );

    VkInstance m_hInstance;
    VkSurfaceKHR m_hSurface;
    VkDevice m_hDevice;
    VkPhysicalDevice m_hPhysicalDevice;
    VmaAllocator m_hAllocator;
};

extern VKContext *g_pVKContext;

SIRENGINE_FORCEINLINE VkDevice VKContext::GetDevice( void ) {
    return m_hDevice;
}

SIRENGINE_FORCEINLINE VmaAllocator VKContext::GetAllocator( void ) {
    return m_hAllocator;
}

#endif