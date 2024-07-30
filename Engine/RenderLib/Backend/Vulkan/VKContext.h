#ifndef __VK_CONTEXT_H__
#define __VK_CONTEXT_H__

#pragma once

#include "VKCommon.h"
#include <Engine/Util/CVector.h>
#include <Engine/Util/CStaticArray.h>

#include "VKSwapChain.h"

typedef struct {
    bool32 bHasGraphicsFamily;
    uint32_t nGraphicsFamily;
    bool32 bHasPresentFamily;
    uint32_t nPresentFamily;

    bool IsComplete( void ) const {
        return bHasGraphicsFamily && bHasPresentFamily;
    }
} QueueFamilyIndices_t;

class VKContext : public IRenderContext
{
public:
    VKContext( const ApplicationInfo_t& appInfo );
    virtual ~VKContext() override;

    virtual void SetupShaderPipeline( void ) override;
    virtual void SwapBuffers( void ) override;
    virtual void CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline ) override;

    virtual void *Alloc( size_t nBytes, size_t nAligment = 16 ) override;
    virtual void Free( void *pBuffer ) override;

    SIRENGINE_FORCEINLINE VkDevice GetDevice( void )
    { return m_hDevice; }
    SIRENGINE_FORCEINLINE VkSurfaceKHR GetSurface( void )
    { return m_hSurface; }
    SIRENGINE_FORCEINLINE VmaAllocator GetAllocator( void )
    { return m_hAllocator; }

    SIRENGINE_FORCEINLINE CVector<VkImage>& GetSwapChainImages( void )
    { return m_pSwapChain->GetSwapChainImages(); }
    SIRENGINE_FORCEINLINE CVector<VkImageView>& GetSwapChainImageViews( void )
    { return m_pSwapChain->GetSwapChainImageViews(); }

    SIRENGINE_FORCEINLINE VkRenderPass GetRenderPass( void )
    { return m_hRenderPass; }

    VkFormat GetSupportedFormat( const VkFormat *pFormats, uint64_t nFormatCount, VkImageTiling nTiling,
        VkFormatFeatureFlags nFeatures ) const;
    
    QueueFamilyIndices_t FindQueueFamilies( void );
    SwapChainSupportInfo_t& QuerySwapChainSupport( void );
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const VkSurfaceFormatKHR *pAvailableFormats,
        uint32_t nAvailableFormats );
    VkPresentModeKHR ChooseSwapPresentMode( const VkPresentModeKHR *pAvailablePresentModes,
        uint32_t nAvailablePresentModes );
    VkExtent2D ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities, int nWidth, int nHeight );
private:
    virtual void GetGPUExtensionList( void ) override;

    void InitLogicalDevice( void );
    void InitWindowInstance( void );
    void InitPhysicalVKDevice( void );
    void InitCommandPool( void );
    void InitRenderPass( void );

    void CheckExtensionsSupported( void );

    VkInstance m_hInstance;
    VkSurfaceKHR m_hSurface;
    VkDevice m_hDevice;
    VkPhysicalDevice m_hPhysicalDevice;
    VkPipeline m_hPipeline;
    VkPipelineLayout m_hPipelineLayout;
    VkRenderPass m_hRenderPass;
    VkCommandPool m_hCommandPool;
    VkCommandBuffer m_hCommandBuffer;
    VkFence m_hInFlightFence;
    VmaAllocator m_hAllocator;

    VKSwapChain *m_pSwapChain;

    VkQueue m_hGraphicsQueue;
    VkQueue m_hPresentQueue;

    CVector<VkExtensionProperties> m_Extensions;
};

extern VKContext *g_pVKContext;

#endif