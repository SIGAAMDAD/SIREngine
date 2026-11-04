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
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_UNSUPPORTED ) {
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
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 12;
	}
	barrier.nBaseMip = 0;
	barrier.nBaseLayer = 0;
	image.eFormat = VK_FORMAT_D32_SFLOAT;
	if ( sirVulkanPlanImageBarrier( barrier, &image, &native ) != SIR_GRAPHICS_UNSUPPORTED ) {
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
	if ( sirVulkanPlanImageBarrier( rangeBarrier, &range, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.nMip != 0 || native.nLayer != 0 || native.nMipCount != 2 || native.nLayerCount != 2 ) {
		return 16;
	}
	if ( !sirVulkanSetSubresourceLayout( &range, 0, 0, VK_IMAGE_LAYOUT_GENERAL ) ||
	     sirVulkanPlanImageBarrier( rangeBarrier, &range, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 17;
	}
	rangeBarrier.nMipCount = 0;
	if ( sirVulkanPlanImageBarrier( rangeBarrier, &range, &native ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 18;
	}
	rangeBarrier.nMipCount = 2;
	rangeBarrier.nBaseLayer = 1;
	if ( sirVulkanPlanImageBarrier( rangeBarrier, &range, &native ) != SIR_GRAPHICS_UNSUPPORTED ) {
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
	if ( sirVulkanPlanImageBarrier( rangeBarrier, &uploaded, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.nMipCount != 2 || native.nLayerCount != 2 ) {
		return 22;
	}
	printf( "Vulkan image barrier contracts passed\n" );
	return 0;
}
