#include "Pch.h"
#include "VulkanUtils.h"
#include "VulkanSwapChain.h"

using namespace SIREngine::Rendering::Vulkan;

struct SSwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR Capabilities;
	VkSurfaceFormatKHR *pFormats;
	VkPresentModeKHR *pPresentModes;

	uint32_t nPresentModeCount;
	uint32_t nSurfaceFormatCount;
};

VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const VkSurfaceFormatKHR *pFormats, uint32_t nFormatCount )
{
	for ( uint32_t i = 0; i < nFormatCount; i++ ) {
		if ( pFormats[ i ].format == VK_FORMAT_B8G8R8A8_SRGB && pFormats[ i ].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
			return pFormats[ i ];
		}
	}
	return pFormats[ 0 ];
}

VkPresentModeKHR ChooseSwapPresentMode( const VkPresentModeKHR *pPresentModes, uint32_t nPresentModes )
{
	for ( uint32_t i = 0; i < nPresentModes; i++ ) {
		if ( pPresentModes[ i ] == VK_PRESENT_MODE_MAILBOX_KHR ) {
			return pPresentModes[ i ];
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapExtent( SDL_Window *pWindow, const VkSurfaceCapabilitiesKHR& capabilities )
{
	if ( capabilities.currentExtent.width != SIRENGINE_UINT32_MAX ) {
		return capabilities.currentExtent;
	}

	int width, height;
	SDL_GetWindowSize( pWindow, &width, &height );

	return VkExtent2D {
		eastl::clamp( (uint32_t)width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width ),
		eastl::clamp( (uint32_t)height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height )
	};
}

SSwapChainSupportDetails *QuerySwapChainSupport( VkPhysicalDevice hDevice, VkSurfaceKHR hSurface )
{
	SSwapChainSupportDetails *pSupportDetails;
	size_t nSize = sizeof( *pSupportDetails );

	uint32_t nPresentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR( hDevice, hSurface, &nPresentModeCount, NULL );
	nSize += SIRENGINE_PAD( sizeof( *pSupportDetails->pPresentModes ) * nPresentModeCount, 16 );

	uint32_t nSurfaceFormatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR( hDevice, hSurface, &nSurfaceFormatCount, NULL );
	nSize += SIRENGINE_PAD( sizeof( *pSupportDetails->pFormats ) * nSurfaceFormatCount, 16 );

	pSupportDetails = (SSwapChainSupportDetails *)calloc( nSize, 1 );
	if ( !pSupportDetails ) {
		return NULL;
	}
	pSupportDetails->pFormats = (VkSurfaceFormatKHR *)SIRENGINE_PAD( (uintptr_t)( (char *)pSupportDetails + sizeof( *pSupportDetails ) ), 16 );
	pSupportDetails->pPresentModes = (VkPresentModeKHR *)SIRENGINE_PAD( (uintptr_t)( (char *)pSupportDetails->pFormats + ( sizeof( *pSupportDetails->pFormats ) * nSurfaceFormatCount ) ), 16 );

	vkGetPhysicalDeviceSurfacePresentModesKHR( hDevice, hSurface, &pSupportDetails->nPresentModeCount, pSupportDetails->pPresentModes );
	vkGetPhysicalDeviceSurfaceFormatsKHR( hDevice, hSurface, &pSupportDetails->nSurfaceFormatCount, pSupportDetails->pFormats );
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR( hDevice, hSurface, &pSupportDetails->Capabilities );

	return pSupportDetails;
}

VulkanSwapChain::VulkanSwapChain( SDL_Window *pWindow, const SVulkanInstance& instance )
	: m_Instance( instance ), m_parrImages( NULL )
{
	SSwapChainSupportDetails *pSwapChainSupport = QuerySwapChainSupport( instance.hPhysicalDevice, instance.hSurface );

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat( pSwapChainSupport->pFormats, pSwapChainSupport->nSurfaceFormatCount );
	VkPresentModeKHR presentMode = ChooseSwapPresentMode( pSwapChainSupport->pPresentModes, pSwapChainSupport->nPresentModeCount );
	VkExtent2D extent = ChooseSwapExtent( pWindow, pSwapChainSupport->Capabilities );

	uint32_t imageCount = pSwapChainSupport->Capabilities.minImageCount + 1;
	
	// make sure we aren't exceeding the actual limit
	if ( pSwapChainSupport->Capabilities.maxImageCount > 0 && imageCount > pSwapChainSupport->Capabilities.maxImageCount ) {
		imageCount = pSwapChainSupport->Capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = instance.hSurface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const uint32_t szQueueFamilyIndices[] = { instance.QueueFamilyIndices.nGraphicsFamily.value(), instance.QueueFamilyIndices.nPresentSupport.value() };
	if ( instance.QueueFamilyIndices.nGraphicsFamily != instance.QueueFamilyIndices.nPresentSupport ) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = szQueueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = NULL;
	}
	createInfo.preTransform = pSwapChainSupport->Capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	m_SwapChainImageFormat = surfaceFormat.format;
	m_SwapChainExtent = extent;

	VkResult result = vkCreateSwapchainKHR( instance.hDevice, &createInfo, NULL, &m_hSwapChain );

	CreateImageViews();
	CreateFramebuffers();
}

VulkanSwapChain::~VulkanSwapChain()
{
	if ( m_hSwapChain != VK_NULL_HANDLE ) {
		vkDestroySwapchainKHR( m_Instance.hDevice, m_hSwapChain, NULL );
	}
	if ( m_parrImageViews != NULL ) {
		for ( uint32_t i = 0; i < m_nImageCount; i++ ) {
			if ( m_parrImageViews[ i ] != VK_NULL_HANDLE ) {
				vkDestroyImageView( m_Instance.hDevice, m_parrImageViews[ i ], NULL );
			}
		}
		delete[] m_parrImageViews;
	}
	if ( m_parrFramebuffers != NULL ) {
		for ( uint32_t i = 0; i < m_nImageCount; i++ ) {
			if ( m_parrFramebuffers[ i ] != VK_NULL_HANDLE ) {
				vkDestroyFramebuffer( m_Instance.hDevice, m_parrFramebuffers[ i ], NULL );
			}
		}
		delete[] m_parrFramebuffers;
	}
	if ( m_parrImages != NULL ) {
		delete[] m_parrImages;
	}
}

void VulkanSwapChain::CreateImageViews( void )
{
	vkGetSwapchainImagesKHR( m_Instance.hDevice, m_hSwapChain, &m_nImageCount, NULL );
	m_parrImages = new VkImage[ m_nImageCount ];
	vkGetSwapchainImagesKHR( m_Instance.hDevice, m_hSwapChain, &m_nImageCount, m_parrImages );

	m_parrImageViews = new VkImageView[ m_nImageCount ];
	for ( uint32_t i = 0; i < m_nImageCount; i++ ) {
		VkImageViewCreateInfo imageInfo;
		memset( &imageInfo, 0, sizeof( imageInfo ) );
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageInfo.image = m_parrImages[ i ];
		imageInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageInfo.format = m_SwapChainImageFormat;
		imageInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageInfo.subresourceRange.baseMipLevel = 0;
		imageInfo.subresourceRange.levelCount = 1;
		imageInfo.subresourceRange.baseArrayLayer = 0;
		imageInfo.subresourceRange.layerCount = 1;

		VkResult result = vkCreateImageView( m_Instance.hDevice, &imageInfo, NULL, &m_parrImageViews[ i ] );
	}
}

void VulkanSwapChain::CreateFramebuffers( void )
{
	m_parrFramebuffers = new VkFramebuffer[ m_nImageCount ];
	for ( uint32_t i = 0; i < m_nImageCount; i++ ) {
		const VkImageView arrAttachments[] = {
			m_parrImageViews[ i ]
		};
		
		VkFramebufferCreateInfo createInfo;
		memset( &createInfo, 0, sizeof( createInfo ) );
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.attachmentCount = SIREngine_ArrayLength( arrAttachments );
		createInfo.pAttachments = arrAttachments;
		createInfo.width = m_SwapChainExtent.width;
		createInfo.height = m_SwapChainExtent.height;
		createInfo.layers = 1;

		VkResult result = vkCreateFramebuffer( m_Instance.hDevice, &createInfo, NULL, &m_parrFramebuffers[ i ] );
	}
}