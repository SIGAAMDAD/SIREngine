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

int main( void )
{
	sirVulkanImage_t color = {};
	color.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	color.nUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	color.nWidth = color.nHeight = 16;
	color.nMips = color.nLayers = 1;
	sirVulkanComputePipeline_t pipeline = {};
	pipeline.hPipeline = ( VkPipeline )1;
	pipeline.bGraphics = true;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = color.eFormat;
	sirVulkanRecordedCommand_t ops[7] = {};
	ops[0].eKind = SIR_VULKAN_RECORDED_RENDER_BEGIN;
	ops[0].nColorCount = 1;
	ops[0].szColorImages[0] = &color;
	ops[0].szColorViews[0] = ( VkImageView )1;
	ops[0].szColorClear[0] = true;
	ops[0].szColorStore[0] = true;
	ops[0].extent = { 16, 16 };
	ops[1].eKind = SIR_VULKAN_RECORDED_GRAPHICS_PIPELINE;
	ops[1].pPipeline = &pipeline;
	ops[2].eKind = SIR_VULKAN_RECORDED_VIEWPORT;
	ops[2].viewport = { 0, 0, 16, 16, 0, 1 };
	ops[2].scissor = { { 0, 0 }, { 16, 16 } };
	ops[3].eKind = SIR_VULKAN_RECORDED_DRAW;
	ops[3].nDrawCount = 3;
	ops[3].nInstanceCount = 1;
	ops[4].eKind = SIR_VULKAN_RECORDED_RENDER_END;
	if ( sirVulkanValidateRecordedCommands( ops, 5 ) != SIR_VULKAN_SUCCESS )
		return 1;
	if ( sirVulkanValidateRecordedCommands( ops, 4 ) != SIR_VULKAN_INVALID_ARGUMENT )
		return 2;
	ops[1].pPipeline = nullptr;
	if ( sirVulkanValidateRecordedCommands( ops, 5 ) != SIR_VULKAN_INVALID_ARGUMENT )
		return 3;
	ops[1].pPipeline = &pipeline;
	pipeline.szColorFormats[0] = VK_FORMAT_B8G8R8A8_UNORM;
	if ( sirVulkanValidateRecordedCommands( ops, 5 ) != SIR_VULKAN_INVALID_ARGUMENT )
		return 4;
	pipeline.szColorFormats[0] = color.eFormat;
	ops[3].eKind = SIR_VULKAN_RECORDED_DRAW_INDEXED;
	if ( sirVulkanValidateRecordedCommands( ops, 5 ) != SIR_VULKAN_INVALID_ARGUMENT )
		return 5;
	sirVulkanBuffer_t index = {};
	index.hBuffer = ( VkBuffer )1;
	index.nSize = 128;
	index.nUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	ops[3] = ops[2];
	ops[3].eKind = SIR_VULKAN_RECORDED_INDEX_BUFFER;
	ops[3].pBuffer = &index;
	ops[4].eKind = SIR_VULKAN_RECORDED_DRAW_INDEXED;
	ops[4].nDrawCount = 3;
	ops[4].nInstanceCount = 1;
	ops[5].eKind = SIR_VULKAN_RECORDED_RENDER_END;
	if ( sirVulkanValidateRecordedCommands( ops, 6 ) != SIR_VULKAN_SUCCESS )
		return 6;
	ops[0].nColorCount = 9;
	if ( sirVulkanValidateRecordedCommands( ops, 6 ) != SIR_VULKAN_INVALID_ARGUMENT )
		return 7;
	ops[0].nColorCount = 1;
	ops[4].nFirstElement = 100;
	if ( sirVulkanValidateRecordedCommands( ops, 6 ) != SIR_VULKAN_INVALID_ARGUMENT )
		return 10;
	ops[4].nFirstElement = 0;
	ops[6].eKind = SIR_VULKAN_RECORDED_IMAGE_BARRIER;
	ops[6].pImage = &color;
	ops[6].imageBarrier.eBefore = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	ops[6].imageBarrier.eAfter = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ops[6].imageBarrier.nMipCount = 1;
	ops[6].imageBarrier.nLayerCount = 1;
	if ( sirVulkanValidateRecordedCommands( ops, 7 ) != SIR_VULKAN_SUCCESS )
		return 8;
	ops[0].szColorStore[0] = false;
	if ( sirVulkanValidateRecordedCommands( ops, 7 ) != SIR_VULKAN_INVALID_ARGUMENT )
		return 9;
	return 0;
}
