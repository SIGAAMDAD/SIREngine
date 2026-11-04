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

#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

#include <string.h>

bool SIRVulkanDevice::loadImageDispatch( void ) noexcept
{
	m_pfnCreateImageView = ( PFN_vkCreateImageView )m_pfnGetDeviceProcAddr( m_Device, "vkCreateImageView" );
	m_pfnDestroyImageView = ( PFN_vkDestroyImageView )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyImageView" );
	m_pfnCreateSampler = ( PFN_vkCreateSampler )m_pfnGetDeviceProcAddr( m_Device, "vkCreateSampler" );
	m_pfnDestroySampler = ( PFN_vkDestroySampler )m_pfnGetDeviceProcAddr( m_Device, "vkDestroySampler" );
	return m_pfnCreateImageView != nullptr && m_pfnDestroyImageView != nullptr && m_pfnCreateSampler != nullptr &&
	       m_pfnDestroySampler != nullptr;
}

sirVulkanResult_t SIRVulkanDevice::createImage( const VkImageCreateInfo &info, sirVulkanImage_t *pImage ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pImage == nullptr || pImage->hImage != VK_NULL_HANDLE || pImage->pAllocation != nullptr ||
	     info.sType != VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO || info.extent.width == 0 || info.extent.height == 0 ||
	     info.extent.depth == 0 || info.usage == 0 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	VmaAllocationCreateInfo allocation = {};
	allocation.usage = VMA_MEMORY_USAGE_AUTO;
	allocation.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
	const VkResult eCreate = vmaCreateImage( m_Allocator, &info, &allocation, &pImage->hImage, &pImage->pAllocation, nullptr );
	if ( eCreate != VK_SUCCESS ) {
		pImage->hImage = VK_NULL_HANDLE;
		pImage->pAllocation = nullptr;
		return eCreate == VK_ERROR_OUT_OF_DEVICE_MEMORY || eCreate == VK_ERROR_OUT_OF_HOST_MEMORY
		           ? SIR_VULKAN_ALLOCATION_FAILED
		           : SIR_VULKAN_NATIVE_FAILURE;
	}
	pImage->pOwnerAllocator = m_Allocator;
	pImage->nDeviceSerial = m_nDeviceSerial;
	pImage->eFormat = info.format;
	pImage->eType = info.imageType;
	pImage->nLayers = info.arrayLayers;
	pImage->nMips = info.mipLevels;
	pImage->nUsage = info.usage;
	pImage->bCube = ( info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT ) != 0;
	++m_nLiveImages;
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::destroyImage( sirVulkanImage_t *pImage ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pImage == nullptr || pImage->hImage == VK_NULL_HANDLE || pImage->pOwnerAllocator != m_Allocator ||
	     pImage->pAllocation == nullptr || pImage->nDeviceSerial != m_nDeviceSerial || m_nLiveImages == 0 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	vmaDestroyImage( m_Allocator, pImage->hImage, pImage->pAllocation );
	memset( pImage, 0, sizeof( *pImage ) );
	--m_nLiveImages;
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::createImageView( const sirVulkanImage_t *pImage, const VkImageViewCreateInfo &info,
                                                     sirVulkanImageView_t *pView ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pImage == nullptr || pView == nullptr || pImage->hImage == VK_NULL_HANDLE || pImage->pOwnerAllocator != m_Allocator ||
	     pImage->nDeviceSerial != m_nDeviceSerial || pView->hView != VK_NULL_HANDLE ||
	     info.sType != VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO || info.image != pImage->hImage ||
	     info.format != pImage->eFormat || info.subresourceRange.levelCount == 0 || info.subresourceRange.layerCount == 0 ||
	     info.subresourceRange.baseMipLevel >= pImage->nMips ||
	     info.subresourceRange.levelCount > pImage->nMips - info.subresourceRange.baseMipLevel ||
	     info.subresourceRange.baseArrayLayer >= pImage->nLayers ||
	     info.subresourceRange.layerCount > pImage->nLayers - info.subresourceRange.baseArrayLayer ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const VkResult eCreate = m_pfnCreateImageView( m_Device, &info, nullptr, &pView->hView );
	if ( eCreate != VK_SUCCESS ) {
		return eCreate == VK_ERROR_OUT_OF_DEVICE_MEMORY || eCreate == VK_ERROR_OUT_OF_HOST_MEMORY
		           ? SIR_VULKAN_ALLOCATION_FAILED
		           : SIR_VULKAN_NATIVE_FAILURE;
	}
	pView->nDeviceSerial = m_nDeviceSerial;
	pView->hImage = pImage->hImage;
	++m_nLiveImageViews;
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::destroyImageView( sirVulkanImageView_t *pView ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pView == nullptr || pView->hView == VK_NULL_HANDLE || pView->nDeviceSerial != m_nDeviceSerial ||
	     m_nLiveImageViews == 0 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	m_pfnDestroyImageView( m_Device, pView->hView, nullptr );
	memset( pView, 0, sizeof( *pView ) );
	--m_nLiveImageViews;
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::createSampler( const VkSamplerCreateInfo &info, sirVulkanSampler_t *pSampler ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pSampler == nullptr || pSampler->hSampler != VK_NULL_HANDLE || info.sType != VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO ||
	     info.anisotropyEnable != VK_FALSE ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const VkResult eCreate = m_pfnCreateSampler( m_Device, &info, nullptr, &pSampler->hSampler );
	if ( eCreate != VK_SUCCESS ) {
		return eCreate == VK_ERROR_OUT_OF_DEVICE_MEMORY || eCreate == VK_ERROR_OUT_OF_HOST_MEMORY
		           ? SIR_VULKAN_ALLOCATION_FAILED
		           : SIR_VULKAN_NATIVE_FAILURE;
	}
	pSampler->nDeviceSerial = m_nDeviceSerial;
	++m_nLiveSamplers;
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::destroySampler( sirVulkanSampler_t *pSampler ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pSampler == nullptr || pSampler->hSampler == VK_NULL_HANDLE || pSampler->nDeviceSerial != m_nDeviceSerial ||
	     m_nLiveSamplers == 0 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	m_pfnDestroySampler( m_Device, pSampler->hSampler, nullptr );
	memset( pSampler, 0, sizeof( *pSampler ) );
	--m_nLiveSamplers;
	return SIR_VULKAN_SUCCESS;
}
