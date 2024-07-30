#include "VKSwapChain.h"

VKSwapChain::VKSwapChain( void )
{
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkExtent2D extent;
    uint32_t nImageCount, i;
    QueueFamilyIndices_t queueIndices;

    const SwapChainSupportInfo_t& swapChainSupport = g_pVKContext->QuerySwapChainSupport();
    surfaceFormat = g_pVKContext->ChooseSwapSurfaceFormat( swapChainSupport.pFormats, swapChainSupport.nFormats );
    presentMode = g_pVKContext->ChooseSwapPresentMode( swapChainSupport.pPresentModes, swapChainSupport.nPresentModes );
    extent = g_pVKContext->ChooseSwapExtent( swapChainSupport.capabilities,
        g_pApplication->GetAppInfo().nWindowWidth, g_pApplication->GetAppInfo().nWindowHeight );

    nImageCount = swapChainSupport.capabilities.minImageCount + 1;
    if ( swapChainSupport.capabilities.maxImageCount > 0 && nImageCount > swapChainSupport.capabilities.maxImageCount ) {
        nImageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo;
    memset( &createInfo, 0, sizeof( createInfo ) );
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = g_pVKContext->GetSurface();
    createInfo.minImageCount = nImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    queueIndices = g_pVKContext->FindQueueFamilies();
    const bool32 queueFamilyIndices[] = {
        queueIndices.bHasGraphicsFamily, queueIndices.bHasPresentFamily
    };

    if ( queueIndices.nGraphicsFamily != queueIndices.nPresentFamily ) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if ( vkCreateSwapchainKHR( g_pVKContext->GetDevice(), &createInfo, NULL, &m_hSwapChain ) != VK_SUCCESS ) {
        g_pApplication->Error( "vkCreateSwapchainKHR: failed" );
    }

    vkGetSwapchainImagesKHR( g_pVKContext->GetDevice(), m_hSwapChain, &nImageCount, NULL );
    m_SwapChainImages.Resize( nImageCount );
    vkGetSwapchainImagesKHR( g_pVKContext->GetDevice(), m_hSwapChain, &nImageCount, m_SwapChainImages.GetBuffer() );

    m_nSwapChainFormat = surfaceFormat.format;
    m_nSwapChainExtent = extent;

    m_SwapChainImageViews.Resize( m_SwapChainImages.Size() );

    for ( i = 0; i < m_SwapChainImages.Size(); i++ ) {
        VkImageViewCreateInfo imageViewInfo;
        memset( &imageViewInfo, 0, sizeof( imageViewInfo ) );
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = m_SwapChainImages[i];
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format = m_nSwapChainFormat;
        imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;

        if ( vkCreateImageView( g_pVKContext->GetDevice(), &imageViewInfo, NULL, &m_SwapChainImageViews[i] ) != VK_SUCCESS ) {

        }
    }

    m_SwapChainFramebuffers.Resize( m_SwapChainImageViews.Size() );

    for ( i = 0; i < m_SwapChainFramebuffers.Size(); i++ ) {
        VkImageView szAttachments[] = {
            m_SwapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo;
        memset( &framebufferInfo, 0, sizeof( framebufferInfo ) );
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = szAttachments;
        framebufferInfo.width = g_pApplication->GetAppInfo().nWindowWidth;
        framebufferInfo.height = g_pApplication->GetAppInfo().nWindowHeight;
        framebufferInfo.layers = 1;

        if ( vkCreateFramebuffer( g_pVKContext->GetDevice(), &framebufferInfo, NULL, &m_SwapChainFramebuffers[i] ) != VK_SUCCESS )
        {

        }
    }
}

VKSwapChain::~VKSwapChain()
{
    for ( auto it : m_SwapChainImageViews ) {
        if ( it ) {
            vkDestroyImageView( g_pVKContext->GetDevice(), it, NULL );
        }
    }
    m_SwapChainImageViews.Clear();

    for ( size_t i = 0; i < m_SwapChainImageAllocations.Size(); i++ ) {
        if ( m_SwapChainImages[i] ) {
            vmaDestroyImage( g_pVKContext->GetAllocator(), m_SwapChainImages[i], m_SwapChainImageAllocations[i] );
        }
    }
    m_SwapChainImages.Clear();
    m_SwapChainImageAllocations.Clear();

    for ( auto it : m_SwapChainFramebuffers ) {
        if ( it ) {
            vkDestroyFramebuffer( g_pVKContext->GetDevice(), it, NULL );
        }
    }

    if ( m_hSwapChain ) {
        vkDestroySwapchainKHR( g_pVKContext->GetDevice(), m_hSwapChain, NULL );
    }
}