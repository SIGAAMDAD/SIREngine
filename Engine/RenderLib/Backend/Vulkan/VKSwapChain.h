#ifndef __VK_SWAPCHAIN_H__
#define __VK_SWAPCHAIN_H__

#pragma once

#include "VKContext.h"
#include "VKCommon.h"

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *pFormats;
    VkPresentModeKHR *pPresentModes;
    uint32_t nFormats;
    uint32_t nPresentModes;
} SwapChainSupportInfo_t;

class VKSwapChain
{
public:
    VKSwapChain( void );
    ~VKSwapChain();

    SIRENGINE_FORCEINLINE CVector<VkImage>& GetSwapChainImages( void )
    { return m_SwapChainImages; }
    SIRENGINE_FORCEINLINE CVector<VkImageView>& GetSwapChainImageViews( void )
    { return m_SwapChainImageViews; }
    SIRENGINE_FORCEINLINE CVector<VkFramebuffer>& GetSwapChainFramebuffers( void )
    { return m_SwapChainFramebuffers; }
    
    SIRENGINE_FORCEINLINE VkFormat GetFormat( void ) const
    { return m_nSwapChainFormat; }
    SIRENGINE_FORCEINLINE VkExtent2D GetExtent( void ) const
    { return m_nSwapChainExtent; }
private:
    CVector<VkImage> m_SwapChainImages;
    CVector<VkImageView> m_SwapChainImageViews;
    CVector<VkFramebuffer> m_SwapChainFramebuffers;
    CVector<VmaAllocation> m_SwapChainImageAllocations;
    VkSwapchainKHR m_hSwapChain;
    VkFormat m_nSwapChainFormat;
    VkExtent2D m_nSwapChainExtent;
};

#endif