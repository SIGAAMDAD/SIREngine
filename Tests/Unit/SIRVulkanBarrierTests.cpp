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

#include <stdio.h>

int main( void )
{
	sirVulkanImage_t image = {};
	image.eType = VK_IMAGE_TYPE_2D;
	image.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	image.nMips = 2;
	image.nLayers = 2;
	image.nUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	sirGraphicsBarrier_t barrier = {};
	barrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.eBefore = SIR_GRAPHICS_STATE_UNDEFINED;

	barrier.eAfter = SIR_GRAPHICS_STATE_COMMON;
	barrier.nBaseMip = 1;
	barrier.nMipCount = 1;
	barrier.nBaseLayer = 1;
	barrier.nLayerCount = 1;
	sirVulkanImageBarrier_t native = {};
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_SUCCESS || native.eBefore != VK_IMAGE_LAYOUT_UNDEFINED ||
	     native.eAfter != VK_IMAGE_LAYOUT_GENERAL || native.nMip != 1 || native.nLayer != 1 || native.bUav ||
	     !sirVulkanSetSubresourceLayout( &image, 1, 1, VK_IMAGE_LAYOUT_GENERAL ) ) {
		return 1;
	}
	VkImageLayout other = VK_IMAGE_LAYOUT_GENERAL;
	if ( !sirVulkanGetSubresourceLayout( &image, 0, 0, &other ) || other != VK_IMAGE_LAYOUT_UNDEFINED ) {
		return 2;
	}
	barrier.eBefore = SIR_GRAPHICS_STATE_COMMON;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 3;
	}
	image.szUploadedSubresources[0] |= 1ULL << 3;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.eAfter != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || !sirVulkanSetSubresourceLayout( &image, 1, 1, native.eAfter ) ) {
		return 4;
	}
	barrier.eBefore = SIR_GRAPHICS_STATE_SHADER_READ;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.eBefore != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || native.eAfter != VK_IMAGE_LAYOUT_GENERAL ||
	     !sirVulkanSetSubresourceLayout( &image, 1, 1, native.eAfter ) ) {
		return 5;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_UAV;
	barrier.eBefore = SIR_GRAPHICS_STATE_SHADER_WRITE;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_SUCCESS || !native.bUav ||
	     native.eBefore != native.eAfter ) {
		return 6;
	}
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 7;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.eBefore = SIR_GRAPHICS_STATE_SHADER_WRITE;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 8;
	}
	barrier.nMipCount = 2;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 9;
	}
	barrier.nMipCount = 1;
	barrier.eBefore = SIR_GRAPHICS_STATE_UNDEFINED;
	barrier.eAfter = SIR_GRAPHICS_STATE_COMMON;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 10;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_ALIAS;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 11;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.nBaseMip = 2;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 12;
	}
	barrier.nBaseMip = 0;
	barrier.nBaseLayer = 0;
	image.eFormat = VK_FORMAT_D32_SFLOAT;
	barrier.eAfter = SIR_GRAPHICS_STATE_DEPTH_WRITE;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 13;
	}
	image.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	image.nUsage = VK_IMAGE_USAGE_STORAGE_BIT;
	barrier.eBefore = SIR_GRAPHICS_STATE_UNDEFINED;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 14;
	}
	if ( sirVulkanPlanImageBarrier( barrier, nullptr, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirVulkanPlanImageBarrier( barrier, &image, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 15;
	}
	// Range planning must check every mip and layer before accepting a transition.
	sirVulkanImage_t range = {};
	range.eType = VK_IMAGE_TYPE_2D;
	range.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	range.nMips = 2;
	range.nLayers = 2;
	range.nUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	sirGraphicsBarrier_t rangeBarrier = {};
	rangeBarrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	rangeBarrier.eBefore = SIR_GRAPHICS_STATE_UNDEFINED;
	rangeBarrier.eAfter = SIR_GRAPHICS_STATE_COMMON;
	rangeBarrier.nMipCount = 2;
	rangeBarrier.nLayerCount = 2;
	if ( sirVulkanPlanImageBarrier( rangeBarrier, &range, &native ) != SIR_GRAPHICS_SUCCESS || native.nMip != 0 || native.nLayer != 0 ||
	     native.nMipCount != 2 || native.nLayerCount != 2 ) {
		return 16;
	}
	if ( !sirVulkanSetSubresourceLayout( &range, 0, 0, VK_IMAGE_LAYOUT_GENERAL ) ||
	     sirVulkanPlanImageBarrier( rangeBarrier, &range, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 17;
	}
	rangeBarrier.nMipCount = 0;
	if ( sirVulkanPlanImageBarrier( rangeBarrier, &range, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 18;
	}
	rangeBarrier.nMipCount = 2;
	rangeBarrier.nBaseLayer = 1;
	if ( sirVulkanPlanImageBarrier( rangeBarrier, &range, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 19;
	}
	rangeBarrier.nBaseLayer = 0;

	sirVulkanImage_t uploaded = {};
	uploaded.eType = VK_IMAGE_TYPE_2D;
	uploaded.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	uploaded.nMips = 2;
	uploaded.nLayers = 2;
	uploaded.nUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	for ( uint32_t iLayer = 0; iLayer < uploaded.nLayers; ++iLayer ) {
		for ( uint32_t iMip = 0; iMip < uploaded.nMips; ++iMip ) {
			if ( !sirVulkanSetSubresourceLayout( &uploaded, iMip, iLayer, VK_IMAGE_LAYOUT_GENERAL ) ) {
				return 20;
			}
		}
	}
	rangeBarrier.eBefore = SIR_GRAPHICS_STATE_COMMON;
	rangeBarrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	uploaded.szUploadedSubresources[0] = 0x7ULL;
	if ( sirVulkanPlanImageBarrier( rangeBarrier, &uploaded, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 21;
	}
	uploaded.szUploadedSubresources[0] = 0xFULL;
	if ( sirVulkanPlanImageBarrier( rangeBarrier, &uploaded, &native ) != SIR_GRAPHICS_SUCCESS || native.nMipCount != 2 ||
	     native.nLayerCount != 2 ) {
		return 22;
	}
	// Transfer layouts remain valid final states across separate command submissions.
	sirVulkanImage_t copyImage = {};
	copyImage.eType = VK_IMAGE_TYPE_2D;
	copyImage.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	copyImage.nMips = 1;
	copyImage.nLayers = 1;
	copyImage.nUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	sirGraphicsBarrier_t copyBarrier = {};
	copyBarrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	copyBarrier.eBefore = SIR_GRAPHICS_STATE_UNDEFINED;
	copyBarrier.eAfter = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	copyBarrier.nMipCount = 1;
	copyBarrier.nLayerCount = 1;
	if ( sirVulkanPlanImageBarrier( copyBarrier, &copyImage, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.eAfter != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || !sirVulkanSetSubresourceLayout( &copyImage, 0, 0, native.eAfter ) ) {
		return 23;
	}
	copyBarrier.eBefore = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	copyBarrier.eAfter = SIR_GRAPHICS_STATE_COPY_SOURCE;
	if ( sirVulkanPlanImageBarrier( copyBarrier, &copyImage, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 24;
	}
	copyImage.szUploadedSubresources[0] = 1;
	if ( sirVulkanPlanImageBarrier( copyBarrier, &copyImage, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.eBefore != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || native.eAfter != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
	     !sirVulkanSetSubresourceLayout( &copyImage, 0, 0, native.eAfter ) ) {
		return 25;
	}
	copyBarrier.eBefore = SIR_GRAPHICS_STATE_COPY_SOURCE;
	copyBarrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( sirVulkanPlanImageBarrier( copyBarrier, &copyImage, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.eBefore != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || native.eAfter != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		return 26;
	}
	copyImage.nUsage &= ~VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	copyBarrier.eAfter = SIR_GRAPHICS_STATE_COPY_SOURCE;
	if ( sirVulkanPlanImageBarrier( copyBarrier, &copyImage, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 27;
	}
	copyImage.nUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	copyBarrier.eAfter = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	if ( sirVulkanPlanImageBarrier( copyBarrier, &copyImage, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 28;
	}

	// Depth-only and packed depth/stencil transitions must use format-correct aspects.
	const VkFormat szDepthFormats[3] = { VK_FORMAT_D16_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT };
	for ( uint32_t iFormat = 0; iFormat < 3; ++iFormat ) {
		sirVulkanImage_t depth = {};
		depth.eFormat = szDepthFormats[iFormat];
		depth.eType = VK_IMAGE_TYPE_2D;
		depth.nMips = 2;
		depth.nLayers = 2;
		depth.nUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		const bool bStencil = depth.eFormat == VK_FORMAT_D24_UNORM_S8_UINT;
		const VkImageLayout eWrite = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		const VkImageLayout eRead = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		if ( sirVulkanImageAspectMask( depth.eFormat ) !=
		     ( bStencil ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT ) ) {
			return 29;
		}
		sirGraphicsBarrier_t ds = {};
		ds.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
		ds.eBefore = SIR_GRAPHICS_STATE_UNDEFINED;
		ds.eAfter = SIR_GRAPHICS_STATE_DEPTH_WRITE;
		ds.nBaseMip = 1;
		ds.nMipCount = 1;
		ds.nBaseLayer = 1;
		ds.nLayerCount = 1;
		if ( sirVulkanPlanImageBarrier( ds, &depth, &native ) != SIR_GRAPHICS_SUCCESS || native.eAfter != eWrite ||
		     !sirVulkanSetSubresourceLayout( &depth, 1, 1, eWrite ) ) {
			return 30;
		}
		ds.eBefore = SIR_GRAPHICS_STATE_DEPTH_WRITE;
		ds.eAfter = SIR_GRAPHICS_STATE_DEPTH_READ;
		if ( sirVulkanPlanImageBarrier( ds, &depth, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return 31;
		}
		depth.szUploadedSubresources[0] = 1ULL << 3;
		if ( sirVulkanPlanImageBarrier( ds, &depth, &native ) != SIR_GRAPHICS_SUCCESS || native.eBefore != eWrite ||
		     native.eAfter != eRead || !sirVulkanSetSubresourceLayout( &depth, 1, 1, eRead ) ) {
			return 32;
		}
		ds.eBefore = SIR_GRAPHICS_STATE_DEPTH_READ;
		ds.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
		if ( sirVulkanPlanImageBarrier( ds, &depth, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return 33;
		}
		ds.eAfter = SIR_GRAPHICS_STATE_DEPTH_WRITE;
		if ( sirVulkanPlanImageBarrier( ds, &depth, &native ) != SIR_GRAPHICS_SUCCESS || native.eAfter != eWrite ) {
			return 34;
		}
		ds.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
		if ( sirVulkanPlanImageBarrier( ds, &depth, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
		     native.eAfter != VK_IMAGE_LAYOUT_UNDEFINED ) {
			return 35;
		}
		ds.eAfter = SIR_GRAPHICS_STATE_COPY_DESTINATION;
		if ( sirVulkanPlanImageBarrier( ds, &depth, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return 36;
		}
		depth.nUsage = VK_IMAGE_USAGE_SAMPLED_BIT;
		ds.eAfter = SIR_GRAPHICS_STATE_DEPTH_WRITE;
		if ( sirVulkanPlanImageBarrier( ds, &depth, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return 37;
		}
		depth.nUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ds.eAfter = SIR_GRAPHICS_STATE_COMMON;
		if ( sirVulkanPlanImageBarrier( ds, &depth, &native ) != SIR_GRAPHICS_SUCCESS || native.eAfter != VK_IMAGE_LAYOUT_GENERAL ) {
			return 38;
		}
	}
	// An incompatible depth layout cannot be bound to a color image.
	sirVulkanImage_t onlyColor = {};
	onlyColor.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	onlyColor.nMips = 1;
	onlyColor.nLayers = 1;
	onlyColor.nUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	sirVulkanImageBarrier_t impossible = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0, 1, 1, false };
	if ( sirVulkanValidateNativeImageBarrier( &onlyColor, impossible ) || sirVulkanValidateNativeImageBarrier( nullptr, impossible ) ) {
		return 39;
	}
	sirVulkanImage_t onlyDepth = {};
	onlyDepth.eFormat = VK_FORMAT_D32_SFLOAT;
	onlyDepth.nMips = 1;
	onlyDepth.nLayers = 1;
	onlyDepth.nUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	impossible.eAfter = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	if ( sirVulkanValidateNativeImageBarrier( &onlyDepth, impossible ) ) {
		return 40;
	}
	printf( "Vulkan image barrier contracts passed\n" );
	return 0;
}
