#include "VKContext.h"
#include "VKShaderPipeline.h"
#include <EASTL/algorithm.h>

QueueFamilyIndices_t VKContext::FindQueueFamilies( void )
{
    QueueFamilyIndices_t indices;
    uint32_t nQueueFamilyCount, i;
    VkQueueFamilyProperties *pQueueProperties;

    memset( &indices, 0, sizeof( indices ) );

    nQueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( m_hPhysicalDevice, &nQueueFamilyCount, NULL );

    pQueueProperties = (VkQueueFamilyProperties *)alloca( sizeof( *pQueueProperties ) * nQueueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( m_hPhysicalDevice, &nQueueFamilyCount, pQueueProperties );

    for ( i = 0; i < nQueueFamilyCount; i++ ) {
        if ( pQueueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
            indices.bHasGraphicsFamily = true;
            indices.nGraphicsFamily = i;

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR( m_hPhysicalDevice, i, m_hSurface, &presentSupport );
            if ( presentSupport ) {
                indices.bHasPresentFamily = true;
                indices.nPresentFamily = i;
            }

            if ( indices.IsComplete() ) {
                break;
            }

            i++;
        }

        return indices;
    }
}

SwapChainSupportInfo_t& VKContext::QuerySwapChainSupport( void )
{
    static SwapChainSupportInfo_t swapChainSupport;
    uint32_t nFormatCount, nPresentModeCount;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( m_hPhysicalDevice, m_hSurface, &swapChainSupport.capabilities );

    vkGetPhysicalDeviceSurfaceFormatsKHR( m_hPhysicalDevice, m_hSurface, &nFormatCount, NULL );
    if ( nFormatCount != 0 ) {
        static VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR *)alloca( sizeof( *formats ) * nFormatCount );
        swapChainSupport.pFormats = formats;
        swapChainSupport.nFormats = nFormatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR( m_hPhysicalDevice, m_hSurface, &nFormatCount, formats );
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR( m_hPhysicalDevice, m_hSurface, &nPresentModeCount, NULL );
    if ( nPresentModeCount != 0 ) {
        static VkPresentModeKHR *presentModes = (VkPresentModeKHR *)alloca(
            sizeof( *presentModes ) * nPresentModeCount );
        
        swapChainSupport.pPresentModes = presentModes;
        swapChainSupport.nPresentModes = nPresentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR( m_hPhysicalDevice, m_hSurface, &nPresentModeCount, presentModes );
    }

    return swapChainSupport;
}

VkSurfaceFormatKHR VKContext::ChooseSwapSurfaceFormat( const VkSurfaceFormatKHR *pAvailableFormats,
    uint32_t nAvailableFormats )
{
    uint32_t i;
    
    for ( i = 0; i < nAvailableFormats; i++ ) {
        if ( pAvailableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB
            && pAvailableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
        {
            return pAvailableFormats[i];
        }
    }

    return pAvailableFormats[0];
}

VkPresentModeKHR VKContext::ChooseSwapPresentMode( const VkPresentModeKHR *pAvailablePresentModes,
    uint32_t nAvailablePresentModes )
{
    uint32_t i;

    for ( i = 0; i < nAvailablePresentModes; i++ ) {
        if ( pAvailablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR ) {
            return pAvailablePresentModes[i];
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VKContext::ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities, int nWidth, int nHeight )
{
    if ( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = { nWidth, nHeight };

        actualExtent.width = eastl::clamp( actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width );
        actualExtent.height = eastl::clamp( actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height );
        
        return actualExtent;
    }
}

static int RateVKDevice( VkPhysicalDevice hDevice )
{
    int score;

    score = 0;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties( hDevice, &deviceProperties );

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures( hDevice, &deviceFeatures );
    
    switch ( deviceProperties.deviceType ) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        // dedicated GPUs have the greatest performance
        score += 1000;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        // most likely an intel GPU
        score += 700;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        // software rendering, the slowest
        score += 200;
        break;
    };

    // this engine is more optimized on specific vendors
    switch ( deviceProperties.vendorID ) {
    case VK_VENDOR_ID_MESA:
        score += 1000;
        break;
    };

    score += deviceProperties.limits.maxImageDimension2D;

    // add some bonus points for framebuffer features
    score += deviceProperties.limits.maxFramebufferWidth;
    score += deviceProperties.limits.maxFramebufferHeight;

    // add some bonues points for extra functionality with vertex array attributes
    score += deviceProperties.limits.maxVertexInputAttributeOffset;
    score += deviceProperties.limits.maxVertexInputAttributes;
    score += deviceProperties.limits.maxVertexInputBindings;
    score += deviceProperties.limits.maxVertexInputBindingStride;

    score += deviceProperties.limits.maxStorageBufferRange;

    // 16-bit shader input required for colors, TODO: make it so that this is
    // not a required device feature
    if ( !deviceFeatures.shaderInt16 ) {
        score = 0;
    }
    if ( !deviceFeatures.fullDrawIndexUint32 ) {
        score = 0;
    }

    return score;
}

VKContext::VKContext( const ApplicationInfo_t& appInfo )
    : IRenderContext( appInfo )
{
    InitWindowInstance();
    InitPhysicalVKDevice();
    InitLogicalDevice();
    m_pSwapChain = new VKSwapChain();
    InitRenderPass();
}

VKContext::~VKContext()
{
    delete m_pSwapChain;

    if ( m_hInFlightFence ) {
        vkDestroyFence( m_hDevice, m_hInFlightFence, NULL );
    }

    if ( m_hRenderPass ) {
        vkDestroyRenderPass( m_hDevice, m_hRenderPass, NULL );
    }
    if ( m_hPipelineLayout ) {
        vkDestroyPipelineLayout( m_hDevice, m_hPipelineLayout, NULL );
    }

    if ( m_hCommandPool ) {
        vkDestroyCommandPool( m_hDevice, m_hCommandPool, NULL );
    }

    if ( m_hSurface != NULL ) {
        vkDestroySurfaceKHR( m_hInstance, m_hSurface, NULL );
    }
    if ( m_hDevice ) {
        vkDestroyDevice( m_hDevice, NULL );
    }
    if ( m_hInstance != NULL ) {
        vkDestroyInstance( m_hInstance, NULL );
    }
}

void VKContext::InitWindowInstance( void )
{
    uint32_t extensionCount;
    VkExtensionProperties *extensionList;

    VkApplicationInfo appInfo;
    memset( &appInfo, 0, sizeof( appInfo ) );
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_AppInfo.pszAppName; // user defined window name
    appInfo.applicationVersion = m_AppInfo.nAppVersion;
    appInfo.pEngineName = SIRENGINE_VERSION_STRING;
    appInfo.engineVersion = SIRENGINE_VERSION;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo;
    memset( &createInfo, 0, sizeof( createInfo ) );
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if ( !SDL_Vulkan_GetInstanceExtensions( m_pWindow, &m_nGPUExtensionCount, m_pGPUExtensionList ) ) {

    }

    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    createInfo.enabledExtensionCount = m_nGPUExtensionCount;
    createInfo.ppEnabledExtensionNames = m_pGPUExtensionList;

    if ( vkCreateInstance( &createInfo, NULL, &m_hInstance ) == VK_SUCCESS ) {

    }

    if ( !SDL_Vulkan_CreateSurface( m_pWindow, m_hInstance, &m_hSurface ) ) {

    }
}

void VKContext::InitCommandPool( void )
{
    const QueueFamilyIndices_t indices = FindQueueFamilies();

    VkCommandPoolCreateInfo poolInfo;
    memset( &poolInfo, 0, sizeof( poolInfo ) );
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.nGraphicsFamily;

    if ( vkCreateCommandPool( m_hDevice, &poolInfo, NULL, &m_hCommandPool ) != VK_SUCCESS ) {

    }

    VkCommandBufferAllocateInfo allocInfo;
    memset( &allocInfo, 0, sizeof( allocInfo ) );
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_hCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if ( vkAllocateCommandBuffers( m_hDevice, &allocInfo, &m_hCommandBuffer ) != VK_SUCCESS ) {

    }

    VkFenceCreateInfo fenceInfo;
    memset( &fenceInfo, 0, sizeof( fenceInfo ) );
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if ( vkCreateFence( m_hDevice, &fenceInfo, NULL, &m_hInFlightFence ) != VK_SUCCESS ) {

    }
}

void VKContext::InitLogicalDevice( void )
{
    VkDeviceQueueCreateInfo szQueueCreateInfos[2];
    uint32_t nQueueCreateInfoCount, i;
    float nQueuePriority;
    const char *szValidationLayers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    const QueueFamilyIndices_t indices = FindQueueFamilies();
    const uint32_t szUniqueQueueFamilies[] = { indices.nGraphicsFamily, indices.nPresentFamily };

    nQueuePriority = 1.0f;
    for ( i = 0; i < SIREngine_StaticArrayLength( szUniqueQueueFamilies ); i++ ) {
        szQueueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        szQueueCreateInfos[i].queueFamilyIndex = i;
        szQueueCreateInfos[i].queueCount = 1;
        szQueueCreateInfos[i].pQueuePriorities = &nQueuePriority;
    }

    VkPhysicalDeviceFeatures deviceFeatures;
    memset( &deviceFeatures, 0, sizeof( deviceFeatures ) );

    VkDeviceCreateInfo createInfo;
    memset( &createInfo, 0, sizeof( createInfo ) );
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = SIREngine_StaticArrayLength( szQueueCreateInfos );
    createInfo.pQueueCreateInfos = szQueueCreateInfos;

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = m_nGPUExtensionCount;
    createInfo.ppEnabledExtensionNames = m_pGPUExtensionList;

#if defined(SIRENGINE_RENDERER_DEBUG)
    createInfo.enabledLayerCount = SIREngine_StaticArrayLength( szValidationLayers );
    createInfo.ppEnabledLayerNames = szValidationLayers;
#else
    createInfo.enabledLayerCount = 0;
#endif

    if ( vkCreateDevice( m_hPhysicalDevice, &createInfo, NULL, &m_hDevice ) != VK_SUCCESS ) {

    }

    vkGetDeviceQueue( m_hDevice, indices.nGraphicsFamily, 0, &m_hGraphicsQueue );
    vkGetDeviceQueue( m_hDevice, indices.nPresentFamily, 0, &m_hPresentQueue );
}

void VKContext::InitPhysicalVKDevice( void )
{
    uint32_t deviceCount, i;
    VkPhysicalDevice *deviceList;
    int *deviceScoreList, prevScore;

    deviceCount = 0;
    m_hPhysicalDevice = VK_NULL_HANDLE;

    vkEnumeratePhysicalDevices( m_hInstance, &deviceCount, NULL );

    if ( deviceCount == 0 ) {

    }

    prevScore = 0;
    deviceScoreList = (int *)alloca( sizeof( *deviceScoreList ) * deviceCount );
    deviceList = (VkPhysicalDevice *)alloca( sizeof( *deviceList ) * deviceCount );
    vkEnumeratePhysicalDevices( m_hInstance, &deviceCount, deviceList );
    for ( i = 0; i < deviceCount; i++ ) {
        deviceScoreList[i] = RateVKDevice( deviceList[i] );
        if ( deviceScoreList[i] > prevScore ) {
            m_hPhysicalDevice = deviceList[i];
        }
        prevScore = deviceScoreList[i];
    }

    if ( m_hPhysicalDevice == VK_NULL_HANDLE ) {

    }
}

void VKContext::InitRenderPass( void )
{
    VkAttachmentDescription colorAttachment;
    memset( &colorAttachment, 0, sizeof( colorAttachment ) );
    colorAttachment.format = VK_FORMAT_R8G8B8A8_SRGB;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef;
    memset( &colorAttachmentRef, 0, sizeof( colorAttachmentRef ) );
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subPass;
    memset( &subPass, 0, sizeof( subPass ) );
    subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPass.colorAttachmentCount = 1;
    subPass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo;
    memset( &renderPassInfo, 0, sizeof( renderPassInfo ) );
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subPass;

    if ( vkCreateRenderPass( m_hDevice, &renderPassInfo, NULL, &m_hRenderPass ) != VK_SUCCESS ) {
    }

}

void VKContext::CheckExtensionsSupported( void )
{
    uint32_t nExtensionCount;
    bool32 bValid;
    const char *szExtensionsList[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    vkEnumerateDeviceExtensionProperties( m_hPhysicalDevice, NULL, &nExtensionCount, NULL );
    
    m_Extensions.Resize( nExtensionCount );
    vkEnumerateDeviceExtensionProperties( m_hPhysicalDevice, NULL, &nExtensionCount, m_Extensions.GetBuffer() );

    bValid = true;
    for ( const auto& it : szExtensionsList ) {
        for ( const auto& ext : m_Extensions ) {
            bool32 bFound = false;
            if ( SIREngine_stricmp( it, ext.extensionName ) == 0 ) {
                bFound = true;
                break;
            }
            if ( !bFound ) {
                bValid = false;
                break;
            }
        }
        if ( !bValid ) {
            break;
        }
    }

    if ( !bValid ) {

    }
}

void VKContext::CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline )
{

    const VkClearValue clearColor = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };

    VkRenderPassBeginInfo passInfo;
    memset( &passInfo, 0, sizeof( passInfo ) );
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = m_hRenderPass;
    passInfo.framebuffer = m_pSwapChain->GetSwapChainFramebuffers()[ 0 ];
    passInfo.renderArea.offset = { 0, 0 };
    passInfo.renderArea.extent = m_pSwapChain->GetExtent();
    vkCmdBeginRenderPass( m_hCommandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

    vkCmdBindPipeline( m_hCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_hPipeline );

    VkViewport viewport;
    memset( &viewport, 0, sizeof( viewport ) );
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_pSwapChain->GetExtent().width;
    viewport.height = (float)m_pSwapChain->GetExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport( m_hCommandBuffer, 0, 1, &viewport );

    VkRect2D scissor;
    memset( &scissor, 0, sizeof( scissor ) );
    scissor.offset = { 0, 0 };
    scissor.extent = m_pSwapChain->GetExtent();
    vkCmdSetScissor( m_hCommandBuffer, 0, 1, &scissor );

    vkCmdDrawIndexed( m_hCommandBuffer, pShaderPipeline->GetIndexCount(), 1, 0, 0, 0 );

    vkCmdEndRenderPass( m_hCommandBuffer );
}

void VKContext::SwapBuffers( void )
{
    vkWaitForFences( m_hDevice, 1, &m_hInFlightFence, VK_TRUE, SIRENGINE_UINT64_MAX );
    vkResetFences( m_hDevice, 1, &m_hInFlightFence );

    if ( vkEndCommandBuffer( m_hCommandBuffer ) != VK_SUCCESS ) {
        
    }

    VkCommandBufferBeginInfo beginInfo;
    memset( &beginInfo, 0, sizeof( beginInfo ) );
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // optional
    beginInfo.pInheritanceInfo = NULL;

    if ( vkBeginCommandBuffer( m_hCommandBuffer, &beginInfo ) != VK_SUCCESS ) {

    }
}

void *VKContext::Alloc( size_t nBytes, size_t nAlignment )
{

}

void VKContext::Free( void *pBuffer )
{

}

VkFormat VKContext::GetSupportedFormat( const VkFormat *pFormats, uint64_t nFormatCount,
    VkImageTiling nTiling, VkFormatFeatureFlags nFeatures ) const
{
    uint64_t i;

    for ( i = 0; i < nFormatCount; i++ ) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties( m_hPhysicalDevice, pFormats[i], &props );

        if ( nTiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & nFeatures ) == nFeatures ) {
            return pFormats[i];
        } else if ( nTiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & nFeatures ) == nFeatures ) {
            return pFormats[i];
        }
    }
    
    g_pApplication->Warning( "" );
    
    return VK_FORMAT_UNDEFINED;
}