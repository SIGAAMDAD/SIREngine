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

static VkFormat sirVulkanTextureFormat( SIRGraphicsFormat eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R8_UNORM:
		return VK_FORMAT_R8_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG8_UNORM:
		return VK_FORMAT_R8G8_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return VK_FORMAT_R16_SFLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_FLOAT:
		return VK_FORMAT_R32_SFLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_UINT:
		return VK_FORMAT_R32_UINT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG32_FLOAT:
		return VK_FORMAT_R32G32_SFLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D16_UNORM:
		return VK_FORMAT_D16_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		return VK_FORMAT_D24_UNORM_S8_UINT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D32_FLOAT:
		return VK_FORMAT_D32_SFLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC1_UNORM:
		return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC3_UNORM:
		return VK_FORMAT_BC3_UNORM_BLOCK;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC5_UNORM:
		return VK_FORMAT_BC5_UNORM_BLOCK;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC7_UNORM:
		return VK_FORMAT_BC7_UNORM_BLOCK;
	default:
		return VK_FORMAT_UNDEFINED;
	}
}

static VkImageUsageFlags sirVulkanTextureUsage( uint32_t nUsage ) noexcept
{
	VkImageUsageFlags nNative = 0;
	if ( ( nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_SAMPLED ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if ( ( nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	if ( ( nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if ( ( nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	if ( ( nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_SOURCE ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	if ( ( nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_DESTINATION ) != 0 ) {
		nNative |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	return nNative;
}

static VkSamplerAddressMode sirVulkanSamplerAddress( SIRGraphicsAddressMode eMode ) noexcept
{
	switch ( eMode ) {
	case SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_REPEAT:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_MIRROR:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_CLAMP:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_BORDER:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	default:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

SIRGraphicsResult SIRVulkanCommandList::copyBufferTexture( const sirGraphicsBufferTextureCopy_t& copy ) noexcept
{
	if ( m_eState != SIRVulkanCommandState::SIR_VULKAN_COMMAND_RECORDING ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( m_pOwner == nullptr || copy.nWidth == 0 || copy.nHeight == 0 || copy.nDepth == 0 || copy.nX > INT32_MAX || copy.nY > INT32_MAX ||
	     copy.nZ > INT32_MAX ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirVulkanGraphicsBufferSlot_t *pBuffer = m_pOwner->resolveBuffer( copy.hBuffer );
	if ( pBuffer == nullptr || copy.hTexture.nSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_TEXTURE_CAPACITY ||
	     copy.hTexture.nGeneration == 0 || copy.hTexture.nDeviceGeneration != m_pOwner->m_nDeviceGeneration ||
	     copy.hTexture.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirVulkanGraphicsImageSlot_t *pImage = &m_pOwner->m_szImages[copy.hTexture.nSlot];
	if ( !pImage->bActive || pImage->nGeneration != copy.hTexture.nGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pBuffer->pMapped != nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}

	sirVulkanFormatBlock_t block {};
	if ( !sirVulkanGetFormatBlock( pImage->native.eFormat, &block ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	sirVulkanTextureTransfer_t transfer {};
	transfer.nBufferOffset = copy.nBufferOffset;
	if ( copy.nBytesPerRow != 0 ) {
		if ( copy.nBytesPerRow % block.nBytesPerBlock != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const uint64_t nTexels = uint64_t( copy.nBytesPerRow / block.nBytesPerBlock ) * block.nBlockWidth;
		if ( nTexels > UINT32_MAX ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		transfer.nBufferRowLength = static_cast<uint32_t>(nTexels);
	}
	transfer.nBufferImageHeight = copy.nRowsPerImage;
	transfer.nMip = copy.nMip;
	transfer.nLayer = copy.nLayer;
	transfer.offset = { static_cast<int32_t>(copy.nX), static_cast<int32_t>(copy.nY), static_cast<int32_t>(copy.nZ) };
	transfer.extent = { copy.nWidth, copy.nHeight, copy.nDepth };
	transfer.bBufferToImage = copy.bBufferToTexture;
	// A readback may follow an upload to the same subresource in this
	// command list; validate it against a local initialization snapshot.
	sirVulkanImage_t imageState = pImage->native;
	if ( !copy.bBufferToTexture && transfer.nMip < imageState.nMips && transfer.nLayer < imageState.nLayers ) {
		for ( uint32_t iOp = 0; iOp < m_nOperationCount; ++iOp ) {
			const sirVulkanGraphicsOperation_t& prior = m_szOperations[iOp];
			if ( prior.eKind == SIRVulkanGraphicsOpKind::SIR_VULKAN_GRAPHICS_TEXTURE_COPY && prior.textureCopy.bBufferToTexture &&
			     prior.textureCopy.hTexture.nSlot == copy.hTexture.nSlot &&
			     prior.textureCopy.hTexture.nGeneration == copy.hTexture.nGeneration && prior.textureCopy.nMip == copy.nMip &&
			     prior.textureCopy.nLayer == copy.nLayer ) {
				sirVulkanTextureTransfer_t earlier {};
				earlier.nMip = prior.textureCopy.nMip;
				earlier.nLayer = prior.textureCopy.nLayer;
				earlier.offset = { static_cast<int32_t>(prior.textureCopy.nX), static_cast<int32_t>(prior.textureCopy.nY), static_cast<int32_t>(prior.textureCopy.nZ) };
				earlier.extent = { prior.textureCopy.nWidth, prior.textureCopy.nHeight, prior.textureCopy.nDepth };
				earlier.bBufferToImage = true;
				if ( sirVulkanTransferCoversSubresource( &imageState, earlier ) ) {
					const uint32_t nIndex = copy.nLayer * imageState.nMips + copy.nMip;
					imageState.szUploadedSubresources[nIndex / 64] |= 1ULL << ( nIndex % 64 );
					break;
				}
			}
		}
	}
	const SIRVulkanResult eValidation = sirVulkanValidateTextureTransfer( &pBuffer->native, &imageState, transfer );
	if ( eValidation == SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( eValidation != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanGraphicsOperation_t& operation = m_szOperations[m_nOperationCount++];
	operation = {};
	operation.eKind = SIRVulkanGraphicsOpKind::SIR_VULKAN_GRAPHICS_TEXTURE_COPY;
	operation.textureCopy = copy;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRVulkanRenderingDevice::createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bReady ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	const SIRGraphicsResult eValidation = sirGraphicsValidateTexture( desc );
	if ( eValidation != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	// Optimal-tiled device-local images are the native foundation; staging and
	// transient aliasing require the following GPU memory and copy packages.
	if ( desc.eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_GPU_LOCAL || desc.nSampleCount != 1 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	constexpr uint32_t IMAGE_MASK = SIRGraphicsUsage::SIR_GRAPHICS_USAGE_SAMPLED | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT |
	                                SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_SOURCE | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	if ( ( desc.nUsage & ~IMAGE_MASK ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bDepth = desc.eFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D16_UNORM || desc.eFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT ||
	                    desc.eFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D32_FLOAT;
	if ( ( bDepth && ( desc.nUsage & ( SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) ) != 0 ) ||
	     ( !bDepth && ( desc.nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const VkFormat eFormat = sirVulkanTextureFormat( desc.eFormat );
	if ( eFormat == VK_FORMAT_UNDEFINED ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}

	VkImageCreateInfo info {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = desc.eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_1D   ? VK_IMAGE_TYPE_1D
	                 : desc.eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_3D ? VK_IMAGE_TYPE_3D
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
	if ( desc.eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_CUBE ) {
		info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}
	collectRetired();
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TEXTURE_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsImageSlot_t *pSlot = &m_szImages[iSlot];
		if ( pSlot->bActive || pSlot->bRetiring || pSlot->nGeneration == 0 ) {
			continue;
		}
		const SIRVulkanResult eNative = m_Native.createImage( info, &pSlot->native );
		if ( eNative != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			return eNative == SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED ? SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
		}
		m_nNativeSerial = pSlot->native.nDeviceSerial;
		pSlot->nViewCount = 0;
		pSlot->nWidth = desc.nWidth;
		pSlot->nHeight = desc.nHeight;
		pSlot->nRetireValue = 0;
		pSlot->bActive = true;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE };
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

SIRGraphicsResult SIRVulkanRenderingDevice::createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bReady ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( !sirGraphicsIsHandleValid( desc.hResource, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE, m_nDeviceGeneration ) ||
	     desc.hResource.nSlot >= SIR_VULKAN_TEXTURE_CAPACITY || desc.nMipLevels == 0 || desc.nLayers == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanGraphicsImageSlot_t *pImage = &m_szImages[desc.hResource.nSlot];
	if ( !pImage->bActive || pImage->nGeneration != desc.hResource.nGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const VkFormat eViewFormat =
	    desc.eFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN ? pImage->native.eFormat : sirVulkanTextureFormat( desc.eFormat );
	if ( eViewFormat != pImage->native.eFormat || desc.nBaseMip >= pImage->native.nMips ||
	     desc.nMipLevels > pImage->native.nMips - desc.nBaseMip || desc.nBaseLayer >= pImage->native.nLayers ||
	     desc.nLayers > pImage->native.nLayers - desc.nBaseLayer ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	VkImageViewType eViewType = VK_IMAGE_VIEW_TYPE_2D;
	if ( pImage->native.eType == VK_IMAGE_TYPE_1D ) {
		eViewType = desc.nLayers == 1 ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
	} else if ( pImage->native.eType == VK_IMAGE_TYPE_3D ) {
		if ( desc.nLayers != 1 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		eViewType = VK_IMAGE_VIEW_TYPE_3D;
	} else if ( pImage->native.bCube ) {
		if ( desc.nBaseLayer % 6 != 0 || desc.nLayers % 6 != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.nLayers != 6 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		eViewType = VK_IMAGE_VIEW_TYPE_CUBE;
	} else {
		eViewType = desc.nLayers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	}
	constexpr uint32_t VIEW_MASK =
	    SIRGraphicsUsage::SIR_GRAPHICS_USAGE_SAMPLED | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	if ( ( desc.nUsage & ~VIEW_MASK ) != 0 ||
	     ( desc.nUsage != 0 && ( sirVulkanTextureUsage( desc.nUsage ) & ~pImage->native.nUsage ) != 0 ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	VkImageViewCreateInfo info {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = pImage->native.hImage;
	info.viewType = eViewType;
	info.format = eViewFormat;
	info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
	                    VK_COMPONENT_SWIZZLE_IDENTITY };
	info.subresourceRange.aspectMask =
	    eViewFormat == VK_FORMAT_D16_UNORM || eViewFormat == VK_FORMAT_D24_UNORM_S8_UINT || eViewFormat == VK_FORMAT_D32_SFLOAT
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
		const SIRVulkanResult eNative = m_Native.createImageView( &pImage->native, info, &pSlot->native );
		if ( eNative != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			return eNative == SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED ? SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
		}
		pSlot->nImageSlot = desc.hResource.nSlot;
		pSlot->nLastGpuUse = 0;
		pSlot->nBaseMip = desc.nBaseMip;
		pSlot->nBaseLayer = desc.nBaseLayer;
		pSlot->nMipLevels = desc.nMipLevels;
		pSlot->nLayers = desc.nLayers;
		pSlot->bActive = true;
		pSlot->nRetireValue = 0;
		++pImage->nViewCount;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW };
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

SIRGraphicsResult SIRVulkanRenderingDevice::createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bReady ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	const SIRGraphicsResult eValidation = sirGraphicsValidateSampler( desc );
	if ( eValidation != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	if ( desc.eFilter == SIRGraphicsSamplerFilter::SIR_GRAPHICS_FILTER_ANISOTROPIC ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bLinear = desc.eFilter != SIRGraphicsSamplerFilter::SIR_GRAPHICS_FILTER_NEAREST;
	VkSamplerCreateInfo info {};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.magFilter = bLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.minFilter = bLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.mipmapMode = desc.eFilter == SIRGraphicsSamplerFilter::SIR_GRAPHICS_FILTER_TRILINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
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
		const SIRVulkanResult eNative = m_Native.createSampler( info, &pSlot->native );
		if ( eNative != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			return eNative == SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED ? SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
		}
		pSlot->bActive = true;
		pSlot->nLastGpuUse = 0;
		pSlot->nRetireValue = 0;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_SAMPLER };
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
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
			if ( m_Native.queryCompletion( &completion, &bDone ) != SIRVulkanResult::SIR_VULKAN_SUCCESS || !bDone ) {
				continue;
			}
		}
		if ( m_Native.destroyImageView( &pSlot->native ) == SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
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
			if ( m_Native.queryCompletion( &completion, &bDone ) != SIRVulkanResult::SIR_VULKAN_SUCCESS || !bDone ) {
				continue;
			}
		}
		if ( m_Native.destroySampler( &pSlot->native ) == SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
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
			if ( m_Native.queryCompletion( &completion, &bDone ) != SIRVulkanResult::SIR_VULKAN_SUCCESS || !bDone ) {
				continue;
			}
		}
		if ( m_Native.destroyImage( &pSlot->native ) == SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			pSlot->bRetiring = false;
			pSlot->nRetireValue = 0;
		}
	}
}

SIRGraphicsResult SIRVulkanRenderingDevice::retireImageResource( sirGraphicsHandle_t hResource,
                                                                   const sirGraphicsCompletionSet_t& completion ) noexcept
{
	if ( !m_bReady || !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ||
	     ( completion.nQueueMask & ~( 1U << SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) ) != 0 ||
	     completion.szQueueValues[SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT] > m_nLastCompletion ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( hResource.nDeviceGeneration != m_nDeviceGeneration || hResource.nGeneration == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const uint64_t nFence = completion.szQueueValues[SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT];
	if ( hResource.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW || hResource.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_SAMPLER ) {
		for ( uint32_t iSet = 0; iSet < SIR_VULKAN_BINDING_SET_CAPACITY; ++iSet ) {
			const sirVulkanGraphicsBindingSlot_t& set = m_szBindingSets[iSet];
			if ( !set.bActive && !set.bRetiring )
				continue;
			for ( uint32_t iRef = 0; iRef < set.nReferenceCount; ++iRef ) {
				const sirGraphicsHandle_t& hReference = set.szReferences[iRef];
				if ( hReference.eType == hResource.eType && hReference.nSlot == hResource.nSlot &&
				     hReference.nGeneration == hResource.nGeneration )
					return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
			}
		}
	}
	switch ( hResource.eType ) {
	case SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE: {
		if ( hResource.nSlot >= SIR_VULKAN_TEXTURE_CAPACITY ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		sirVulkanGraphicsImageSlot_t *pSlot = &m_szImages[hResource.nSlot];
		if ( !pSlot->bActive || pSlot->nGeneration != hResource.nGeneration ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( pSlot->nViewCount != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
		}
		if ( nFence < pSlot->native.nLastGpuUse ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		pSlot->bActive = false;
		pSlot->bRetiring = true;
		pSlot->nRetireValue = nFence;
		++pSlot->nGeneration;
		break;
	}
	case SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW: {
		if ( hResource.nSlot >= SIR_VULKAN_VIEW_CAPACITY ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		sirVulkanGraphicsViewSlot_t *pSlot = &m_szViews[hResource.nSlot];
		if ( !pSlot->bActive || pSlot->nGeneration != hResource.nGeneration ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( nFence < pSlot->nLastGpuUse )
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		pSlot->bActive = false;
		pSlot->bRetiring = true;
		pSlot->nRetireValue = nFence;
		++pSlot->nGeneration;
		break;
	}
	case SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_SAMPLER: {
		if ( hResource.nSlot >= SIR_VULKAN_SAMPLER_CAPACITY ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		sirVulkanGraphicsSamplerSlot_t *pSlot = &m_szSamplers[hResource.nSlot];
		if ( !pSlot->bActive || pSlot->nGeneration != hResource.nGeneration ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( nFence < pSlot->nLastGpuUse )
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		pSlot->bActive = false;
		pSlot->bRetiring = true;
		pSlot->nRetireValue = nFence;
		++pSlot->nGeneration;
		break;
	}
	default:
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	collectRetired();
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
