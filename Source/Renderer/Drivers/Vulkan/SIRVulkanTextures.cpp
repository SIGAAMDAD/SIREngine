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

#include <Renderer/Drivers/Vulkan/SIRVulkanGraphics.h>
#include <Renderer/SIRGraphicsValidation.h>

static VkFormat sirVulkanTextureFormat( sirGraphicsFormat_t eFormat ) noexcept
{
	switch ( eFormat ) {
		case SIR_GRAPHICS_FORMAT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
		case SIR_GRAPHICS_FORMAT_BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
		case SIR_GRAPHICS_FORMAT_RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
		case SIR_GRAPHICS_FORMAT_RGBA16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case SIR_GRAPHICS_FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM;
		case SIR_GRAPHICS_FORMAT_RG8_UNORM: return VK_FORMAT_R8G8_UNORM;
		case SIR_GRAPHICS_FORMAT_R16_FLOAT: return VK_FORMAT_R16_SFLOAT;
		case SIR_GRAPHICS_FORMAT_R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
		case SIR_GRAPHICS_FORMAT_R32_UINT: return VK_FORMAT_R32_UINT;
		case SIR_GRAPHICS_FORMAT_RG32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
		case SIR_GRAPHICS_FORMAT_RGBA32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case SIR_GRAPHICS_FORMAT_D16_UNORM: return VK_FORMAT_D16_UNORM;
		case SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
		case SIR_GRAPHICS_FORMAT_D32_FLOAT: return VK_FORMAT_D32_SFLOAT;
		case SIR_GRAPHICS_FORMAT_BC1_UNORM: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case SIR_GRAPHICS_FORMAT_BC3_UNORM: return VK_FORMAT_BC3_UNORM_BLOCK;
		case SIR_GRAPHICS_FORMAT_BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
		case SIR_GRAPHICS_FORMAT_BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
		default: return VK_FORMAT_UNDEFINED;
	}
}

static VkImageUsageFlags sirVulkanTextureUsage( uint32_t nUsage ) noexcept
{
	VkImageUsageFlags nNative = 0;
	if ( ( nUsage & SIR_GRAPHICS_USAGE_SAMPLED ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if ( ( nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	if ( ( nUsage & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if ( ( nUsage & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	if ( ( nUsage & SIR_GRAPHICS_USAGE_COPY_SOURCE ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	if ( ( nUsage & SIR_GRAPHICS_USAGE_COPY_DESTINATION ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	return nNative;
}

static VkSamplerAddressMode sirVulkanSamplerAddress( sirGraphicsAddressMode_t eMode ) noexcept
{
	switch ( eMode ) {
		case SIR_GRAPHICS_ADDRESS_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case SIR_GRAPHICS_ADDRESS_MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case SIR_GRAPHICS_ADDRESS_CLAMP: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case SIR_GRAPHICS_ADDRESS_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

sirGraphicsResult_t SIRVulkanRenderingDevice::createTexture( const sirGraphicsTextureDesc_t &desc,
                                                              sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirGraphicsResult_t eValidation = sirGraphicsValidateTexture( desc );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	// Optimal-tiled device-local images are the native foundation; staging and
	// transient aliasing require the following GPU memory and copy packages.
	if ( desc.eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL || desc.nSampleCount != 1 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	constexpr uint32_t IMAGE_MASK = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE |
	                                SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_DEPTH_STENCIL |
	                                SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	if ( ( desc.nUsage & ~IMAGE_MASK ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bDepth = desc.eFormat == SIR_GRAPHICS_FORMAT_D16_UNORM ||
	                    desc.eFormat == SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT ||
	                    desc.eFormat == SIR_GRAPHICS_FORMAT_D32_FLOAT;
	if ( ( bDepth && ( desc.nUsage & ( SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) ) != 0 ) ||
	     ( !bDepth && ( desc.nUsage & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const VkFormat eFormat = sirVulkanTextureFormat( desc.eFormat );
	if ( eFormat == VK_FORMAT_UNDEFINED ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = desc.eDimension == SIR_GRAPHICS_TEXTURE_1D ? VK_IMAGE_TYPE_1D
	                 : desc.eDimension == SIR_GRAPHICS_TEXTURE_3D ? VK_IMAGE_TYPE_3D
	                                                               : VK_IMAGE_TYPE_2D;
	info.format = eFormat;
	info.extent = { desc.nWidth, desc.nHeight, desc.nDepth };
	info.mipLevels = desc.nMipLevels;
	info.arrayLayers = desc.nArrayLayers;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = sirVulkanTextureUsage( desc.nUsage );
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( desc.eDimension == SIR_GRAPHICS_TEXTURE_CUBE ) {
		info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}
	collectRetired();
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TEXTURE_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsImageSlot_t *pSlot = &m_szImages[iSlot];
		if ( pSlot->bActive || pSlot->bRetiring || pSlot->nGeneration == 0 ) {
			continue;
		}
		const sirVulkanResult_t eNative = m_Native.createImage( info, &pSlot->native );
		if ( eNative != SIR_VULKAN_SUCCESS ) {
			return eNative == SIR_VULKAN_ALLOCATION_FAILED ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
		}
		m_nNativeSerial = pSlot->native.nDeviceSerial;
		pSlot->nViewCount = 0;
		pSlot->nRetireValue = 0;
		pSlot->bActive = true;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, SIR_GRAPHICS_HANDLE_TEXTURE };
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::createView( const sirGraphicsViewDesc_t &desc,
                                                           sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( !sirGraphicsIsHandleValid( desc.hResource, SIR_GRAPHICS_HANDLE_TEXTURE, m_nDeviceGeneration ) ||
	     desc.hResource.nSlot >= SIR_VULKAN_TEXTURE_CAPACITY || desc.nMipLevels == 0 || desc.nLayers == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanGraphicsImageSlot_t *pImage = &m_szImages[desc.hResource.nSlot];
	if ( !pImage->bActive || pImage->nGeneration != desc.hResource.nGeneration ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const VkFormat eViewFormat = desc.eFormat == SIR_GRAPHICS_FORMAT_UNKNOWN ? pImage->native.eFormat
	                                                                      : sirVulkanTextureFormat( desc.eFormat );
	if ( eViewFormat != pImage->native.eFormat || desc.nBaseMip >= pImage->native.nMips ||
	     desc.nMipLevels > pImage->native.nMips - desc.nBaseMip || desc.nBaseLayer >= pImage->native.nLayers ||
	     desc.nLayers > pImage->native.nLayers - desc.nBaseLayer ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	VkImageViewType eViewType = VK_IMAGE_VIEW_TYPE_2D;
	if ( pImage->native.eType == VK_IMAGE_TYPE_1D ) {
		eViewType = desc.nLayers == 1 ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
	} else if ( pImage->native.eType == VK_IMAGE_TYPE_3D ) {
		if ( desc.nLayers != 1 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		eViewType = VK_IMAGE_VIEW_TYPE_3D;
	} else if ( pImage->native.bCube ) {
		if ( desc.nBaseLayer % 6 != 0 || desc.nLayers % 6 != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.nLayers != 6 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		eViewType = VK_IMAGE_VIEW_TYPE_CUBE;
	} else {
		eViewType = desc.nLayers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	}
	constexpr uint32_t VIEW_MASK = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE |
	                               SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	if ( ( desc.nUsage & ~VIEW_MASK ) != 0 ||
	     ( desc.nUsage != 0 && ( sirVulkanTextureUsage( desc.nUsage ) & ~pImage->native.nUsage ) != 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = pImage->native.hImage;
	info.viewType = eViewType;
	info.format = eViewFormat;
	info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
	                    VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	info.subresourceRange.aspectMask = eViewFormat == VK_FORMAT_D16_UNORM || eViewFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
	                                           eViewFormat == VK_FORMAT_D32_SFLOAT
	                                       ? VK_IMAGE_ASPECT_DEPTH_BIT
	                                       : VK_IMAGE_ASPECT_COLOR_BIT;
	info.subresourceRange.baseMipLevel = desc.nBaseMip;
	info.subresourceRange.levelCount = desc.nMipLevels;
	info.subresourceRange.baseArrayLayer = desc.nBaseLayer;
	info.subresourceRange.layerCount = desc.nLayers;
	collectRetired();
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_VIEW_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsViewSlot_t *pSlot = &m_szViews[iSlot];
		if ( pSlot->bActive || pSlot->bRetiring || pSlot->nGeneration == 0 ) {
			continue;
		}
		const sirVulkanResult_t eNative = m_Native.createImageView( &pImage->native, info, &pSlot->native );
		if ( eNative != SIR_VULKAN_SUCCESS ) {
			return eNative == SIR_VULKAN_ALLOCATION_FAILED ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
		}
		pSlot->nImageSlot = desc.hResource.nSlot;
		pSlot->bActive = true;
		pSlot->nRetireValue = 0;
		++pImage->nViewCount;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, SIR_GRAPHICS_HANDLE_VIEW };
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::createSampler( const sirGraphicsSamplerDesc_t &desc,
                                                              sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirGraphicsResult_t eValidation = sirGraphicsValidateSampler( desc );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	if ( desc.eFilter == SIR_GRAPHICS_FILTER_ANISOTROPIC ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bLinear = desc.eFilter != SIR_GRAPHICS_FILTER_NEAREST;
	VkSamplerCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.magFilter = bLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.minFilter = bLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.mipmapMode = desc.eFilter == SIR_GRAPHICS_FILTER_TRILINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR
	                                                              : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	info.addressModeU = sirVulkanSamplerAddress( desc.eAddressU );
	info.addressModeV = sirVulkanSamplerAddress( desc.eAddressV );
	info.addressModeW = sirVulkanSamplerAddress( desc.eAddressW );
	info.mipLodBias = 0.0f;
	info.anisotropyEnable = VK_FALSE;
	info.maxAnisotropy = 1.0f;
	info.compareEnable = VK_FALSE;
	info.minLod = desc.fMinimumLod;
	info.maxLod = desc.fMaximumLod;
	info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	collectRetired();
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_SAMPLER_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsSamplerSlot_t *pSlot = &m_szSamplers[iSlot];
		if ( pSlot->bActive || pSlot->bRetiring || pSlot->nGeneration == 0 ) {
			continue;
		}
		const sirVulkanResult_t eNative = m_Native.createSampler( info, &pSlot->native );
		if ( eNative != SIR_VULKAN_SUCCESS ) {
			return eNative == SIR_VULKAN_ALLOCATION_FAILED ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
		}
		pSlot->bActive = true;
		pSlot->nRetireValue = 0;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, SIR_GRAPHICS_HANDLE_SAMPLER };
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

void SIRVulkanRenderingDevice::collectImageResources( void ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_VIEW_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsViewSlot_t *pSlot = &m_szViews[iSlot];
		if ( !pSlot->bRetiring ) {
			continue;
		}
		if ( pSlot->nRetireValue != 0 ) {
			const sirVulkanCompletion_t completion = { m_nNativeSerial, pSlot->nRetireValue };
			bool bDone = false;
			if ( m_Native.queryCompletion( &completion, &bDone ) != SIR_VULKAN_SUCCESS || !bDone ) {
				continue;
			}
		}
		if ( m_Native.destroyImageView( &pSlot->native ) == SIR_VULKAN_SUCCESS ) {
			--m_szImages[pSlot->nImageSlot].nViewCount;
			pSlot->bRetiring = false;
			pSlot->nRetireValue = 0;
		}
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_SAMPLER_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsSamplerSlot_t *pSlot = &m_szSamplers[iSlot];
		if ( !pSlot->bRetiring ) {
			continue;
		}
		if ( pSlot->nRetireValue != 0 ) {
			const sirVulkanCompletion_t completion = { m_nNativeSerial, pSlot->nRetireValue };
			bool bDone = false;
			if ( m_Native.queryCompletion( &completion, &bDone ) != SIR_VULKAN_SUCCESS || !bDone ) {
				continue;
			}
		}
		if ( m_Native.destroySampler( &pSlot->native ) == SIR_VULKAN_SUCCESS ) {
			pSlot->bRetiring = false;
			pSlot->nRetireValue = 0;
		}
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TEXTURE_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsImageSlot_t *pSlot = &m_szImages[iSlot];
		if ( !pSlot->bRetiring || pSlot->nViewCount != 0 ) {
			continue;
		}
		if ( pSlot->nRetireValue != 0 ) {
			const sirVulkanCompletion_t completion = { m_nNativeSerial, pSlot->nRetireValue };
			bool bDone = false;
			if ( m_Native.queryCompletion( &completion, &bDone ) != SIR_VULKAN_SUCCESS || !bDone ) {
				continue;
			}
		}
		if ( m_Native.destroyImage( &pSlot->native ) == SIR_VULKAN_SUCCESS ) {
			pSlot->bRetiring = false;
			pSlot->nRetireValue = 0;
		}
	}
}

sirGraphicsResult_t SIRVulkanRenderingDevice::retireImageResource( sirGraphicsHandle_t hResource,
                                                                    const sirGraphicsCompletionSet_t &completion ) noexcept
{
	if ( !m_bReady || !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ||
	     ( completion.nQueueMask & ~( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) != 0 ||
	     completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] > m_nLastCompletion ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( hResource.nDeviceGeneration != m_nDeviceGeneration || hResource.nGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const uint64_t nFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT];
	switch ( hResource.eType ) {
		case SIR_GRAPHICS_HANDLE_TEXTURE: {
			if ( hResource.nSlot >= SIR_VULKAN_TEXTURE_CAPACITY ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			sirVulkanGraphicsImageSlot_t *pSlot = &m_szImages[hResource.nSlot];
			if ( !pSlot->bActive || pSlot->nGeneration != hResource.nGeneration ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			if ( pSlot->nViewCount != 0 ) {
				return SIR_GRAPHICS_BUSY;
			}
			pSlot->bActive = false;
			pSlot->bRetiring = true;
			pSlot->nRetireValue = nFence;
			++pSlot->nGeneration;
			break;
		}
		case SIR_GRAPHICS_HANDLE_VIEW: {
			if ( hResource.nSlot >= SIR_VULKAN_VIEW_CAPACITY ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			sirVulkanGraphicsViewSlot_t *pSlot = &m_szViews[hResource.nSlot];
			if ( !pSlot->bActive || pSlot->nGeneration != hResource.nGeneration ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			pSlot->bActive = false;
			pSlot->bRetiring = true;
			pSlot->nRetireValue = nFence;
			++pSlot->nGeneration;
			break;
		}
		case SIR_GRAPHICS_HANDLE_SAMPLER: {
			if ( hResource.nSlot >= SIR_VULKAN_SAMPLER_CAPACITY ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			sirVulkanGraphicsSamplerSlot_t *pSlot = &m_szSamplers[hResource.nSlot];
			if ( !pSlot->bActive || pSlot->nGeneration != hResource.nGeneration ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			pSlot->bActive = false;
			pSlot->bRetiring = true;
			pSlot->nRetireValue = nFence;
			++pSlot->nGeneration;
			break;
		}
		default: return SIR_GRAPHICS_INVALID_HANDLE;
	}
	collectRetired();
	return SIR_GRAPHICS_SUCCESS;
}
