#ifndef __VK_CONTEXT_H__
#define __VK_CONTEXT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"
#include "../RenderContext.h"
#include "VKProgram.h"
#include "VKTexture.h"
#include <Engine/Memory/Backend/TagArenaAllocator.h>
#include <Engine/Memory/Allocators/HunkAllocator.h>

namespace SIREngine::RenderLib::Backend::Vulkan {
    typedef struct VKMemoryUsage : public GPUMemoryUsage {
        uint32_t allocationCount;
        uint32_t blockCount;
    } VKMemoryUsage_t;

    class VKContext : public Backend::IRenderContext
    {
    public:
        VKContext( const Application::ApplicationInfo_t& appInfo );
        virtual ~VKContext() override;

        virtual void Init( void ) override;
        virtual void Shutdown( void ) override;

        virtual void SetupShaderPipeline( void ) override;
        virtual void SwapBuffers( void ) override;
        virtual void BeginFrame( void ) override;
        virtual void CompleteRenderPass( Backend::IRenderShaderPipeline *pShaderPipeline ) override;
        virtual void FinalizeResources( void ) override
        { }

        virtual void *Alloc( size_t nBytes, size_t nAligment = 16 ) override;
        virtual void Free( void *pBuffer ) override;

        SIRENGINE_FORCEINLINE VkDevice GetDevice( void )
        { return m_hDevice; }
        SIRENGINE_FORCEINLINE VkPhysicalDevice GetPhysicalDevice( void )
        { return m_hPhysicalDevice; }
        SIRENGINE_FORCEINLINE VkSurfaceKHR GetSurface( void )
        { return m_hSurface; }
        SIRENGINE_FORCEINLINE VmaAllocator GetAllocator( void )
        { return m_hAllocator; }

        SIRENGINE_FORCEINLINE CVector<VkImage>& GetSwapChainImages( void )
        { return m_SwapChainImages; }
        SIRENGINE_FORCEINLINE CVector<VkImageView>& GetSwapChainImageViews( void )
        { return m_SwapChainImageViews; }

        SIRENGINE_FORCEINLINE const VkAllocationCallbacks *GetAllocationCallbacks( void ) const
        { return &m_AllocationCallbacks; }

        SIRENGINE_FORCEINLINE VkRenderPass GetRenderPass( void )
        { return m_hRenderPass; }

        void AllocateBuffer( VkBuffer *pBuffer, VkDeviceSize nSize, VkBufferUsageFlags nUsage,
            VkMemoryPropertyFlags memProperties );

        VkFormat GetSupportedFormat( const VkFormat *pFormats, uint64_t nFormatCount, VkImageTiling nTiling,
            VkFormatFeatureFlags nFeatures ) const;

        QueueFamilyIndices_t FindQueueFamilies( void );
        SwapChainSupportInfo_t& QuerySwapChainSupport( void );
        VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const VkSurfaceFormatKHR *pAvailableFormats,
            uint32_t nAvailableFormats );
        VkPresentModeKHR ChooseSwapPresentMode( const VkPresentModeKHR *pAvailablePresentModes,
            uint32_t nAvailablePresentModes );
        VkExtent2D ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities, int nWidth, int nHeight );

        virtual Backend::IRenderProgram *AllocateProgram( const RenderProgramInit_t& programInfo ) override;
        virtual Backend::IRenderShader *AllocateShader( const RenderShaderInit_t& shaderInit ) override;
        virtual Backend::IRenderBuffer *AllocateBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize ) override;
        virtual Backend::IRenderTexture *AllocateTexture( const TextureInit_t& textureInfo ) override;

        virtual const GPUMemoryUsage_t GetMemoryUsage( void ) override;
        virtual void PrintMemoryInfo( void ) const override;

        void LockPipeline( void );
    private:
        virtual void GetGPUExtensionList( void ) override;

        void InitLogicalDevice( void );
        void InitWindowInstance( void );
        void InitPhysicalVKDevice( void );
        void InitCommandPool( void );
        void InitRenderPass( void );
        void InitSwapChain( void );

        void RecreateSwapChain( void );
        void ShutdownSwapChain( void );

        void CheckExtensionsSupported( void );

        CTagArenaAllocator *m_pTagAllocator;

        VkDebugUtilsMessengerEXT m_hDebugHandler;

        VkAllocationCallbacks m_AllocationCallbacks;

        VkInstance m_hInstance;
        VkSurfaceKHR m_hSurface;
        VkDevice m_hDevice;
        VkPhysicalDevice m_hPhysicalDevice;
        VkPipeline m_hPipeline;
        VkPipelineLayout m_hPipelineLayout;
        VkRenderPass m_hRenderPass;
        VkCommandPool m_hCommandPool;
        VkCommandBuffer m_hCommandBuffers[ VK_MAX_FRAMES_IN_FLIGHT ];
        VkFence m_hInFlightFences[ VK_MAX_FRAMES_IN_FLIGHT ];
        VkSemaphore m_hRenderFinished[ VK_MAX_FRAMES_IN_FLIGHT ];
        VmaAllocator m_hAllocator;

        VkSemaphore m_hSwapChainImageAvailable[ VK_MAX_FRAMES_IN_FLIGHT ];
        CVector<VkImage> m_SwapChainImages;
        CVector<VkImageView> m_SwapChainImageViews;
        CVector<VkFramebuffer> m_SwapChainFramebuffers;
        CVector<VmaAllocation> m_SwapChainImageAllocations;
        VkSwapchainKHR m_hSwapChain;
        VkFormat m_nSwapChainFormat;
        VkExtent2D m_nSwapChainExtent;

        VkImageView m_UniformDiffuseMap;
        VkImageView m_UniformNormalMap;
        VkImageView m_UniformSpecularMap;
        VKTexture *m_pDiffuseMap;
        VKTexture *m_pNormalMap;
        VKTexture *m_pSpecularMap;

        VkQueue m_hGraphicsQueue;
        VkQueue m_hPresentQueue;

        VKProgram *m_pGenericShader;

        VmaVirtualBlock m_hVirtualMemory;

        CVector<VkExtensionProperties> m_Extensions;

        uint32_t m_nCurrentFrameIndex;
        uint32_t m_nImageFrameIndex;
    };

    extern CTagArenaAllocator *VK_pTagAllocator;

    extern PFN_vkCreateSwapchainKHR fn_vkCreateSwapchainKHR;
    extern PFN_vkCmdPushDescriptorSetKHR fn_vkCmdPushDescriptorSetKHR;
    extern PFN_vkCreateDescriptorUpdateTemplateKHR fn_vkCreateDescriptorUpdateTemplateKHR;

    extern VKContext *g_pVKContext;
};

#endif