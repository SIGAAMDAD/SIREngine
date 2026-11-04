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
#include <stdio.h>

int main( void )
{
	sirVulkanBuffer_t source = {};
	source.nSize = 4096;
	source.nUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	sirVulkanBuffer_t staging = {};
	staging.nSize = 4096;
	staging.nUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	sirVulkanBuffer_t readback = {};
	readback.nSize = 4096;
	readback.nUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	sirVulkanImage_t image = {};
	image.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	image.eType = VK_IMAGE_TYPE_2D;
	image.nWidth = 8;
	image.nHeight = 8;
	image.nMips = 1;
	image.nLayers = 1;
	image.nUsage =
	    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	sirVulkanRecordedCommand_t commands[SIR_VULKAN_COPY_BATCH_CAPACITY + 1] = {};
	commands[0].eKind = SIR_VULKAN_RECORDED_BUFFER_COPY;
	commands[0].bufferCopy = { &source, &staging, 0, 0, 256 };
	commands[1].eKind = SIR_VULKAN_RECORDED_TEXTURE_COPY;
	commands[1].pBuffer = &staging;
	commands[1].pImage = &image;
	commands[1].textureCopy.extent = { 8, 8, 1 };
	commands[1].textureCopy.bBufferToImage = true;
	commands[2].eKind = SIR_VULKAN_RECORDED_IMAGE_BARRIER;
	commands[2].pImage = &image;
	commands[2].imageBarrier = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0, 1, 1, false };
	commands[3].eKind = SIR_VULKAN_RECORDED_IMAGE_BARRIER;
	commands[3].pImage = &image;
	commands[3].imageBarrier = { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, 1, 1, false };
	commands[4].eKind = SIR_VULKAN_RECORDED_TEXTURE_COPY;
	commands[4].pBuffer = &readback;
	commands[4].pImage = &image;
	commands[4].textureCopy.extent = { 8, 8, 1 };
	commands[4].textureCopy.bBufferToImage = false;

	if ( sirVulkanValidateRecordedCommands( commands, 5 ) != SIR_VULKAN_SUCCESS ) {
		return 1;
	}
	VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
	if ( !sirVulkanGetSubresourceLayout( &image, 0, 0, &layout ) || layout != VK_IMAGE_LAYOUT_UNDEFINED ||
	     image.szUploadedSubresources[0] != 0 ) {
		return 2;
	}
	if ( sirVulkanValidateRecordedCommands( nullptr, 5 ) != SIR_VULKAN_INVALID_ARGUMENT ||
	     sirVulkanValidateRecordedCommands( commands, 0 ) != SIR_VULKAN_INVALID_ARGUMENT ||
	     sirVulkanValidateRecordedCommands( commands, SIR_VULKAN_COPY_BATCH_CAPACITY + 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 3;
	}
	if ( sirVulkanValidateRecordedCommands( &commands[4], 1 ) != SIR_VULKAN_INVALID_ARGUMENT ||
	     sirVulkanValidateRecordedCommands( &commands[2], 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 4;
	}
	commands[2].imageBarrier.eBefore = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( sirVulkanValidateRecordedCommands( commands, 5 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 5;
	}
	commands[2].imageBarrier.eBefore = VK_IMAGE_LAYOUT_GENERAL;
	commands[4].textureCopy.extent.width = 9;
	if ( sirVulkanValidateRecordedCommands( commands, 5 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 6;
	}
	commands[4].textureCopy.extent.width = 8;
	commands[1].pBuffer = &readback;
	if ( sirVulkanValidateRecordedCommands( commands, 5 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 7;
	}
	commands[1].pBuffer = &staging;
	commands[2].imageBarrier.bUav = true;
	if ( sirVulkanValidateRecordedCommands( commands, 5 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 8;
	}
	commands[2].imageBarrier.bUav = false;
	commands[2].imageBarrier.eAfter = VK_IMAGE_LAYOUT_GENERAL;
	if ( sirVulkanValidateRecordedCommands( commands, 5 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 9;
	}
	commands[2].imageBarrier.eAfter = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	commands[2].imageBarrier.nMip = 1;
	if ( sirVulkanValidateRecordedCommands( commands, 5 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 10;
	}
	commands[2].imageBarrier.nMip = 0;
	commands[5].eKind = SIR_VULKAN_RECORDED_BUFFER_COPY;
	commands[5].bufferCopy = { &staging, &readback, 0, 256, 256 };
	if ( sirVulkanValidateRecordedCommands( commands, 6 ) != SIR_VULKAN_SUCCESS ) {
		return 11;
	}
	commands[6].eKind = ( sirVulkanRecordedKind_t )255;
	if ( sirVulkanValidateRecordedCommands( commands, 7 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 12;
	}
	sirVulkanComputePipeline_t compute = {};
	sirVulkanRecordedCommand_t dispatch = {};
	dispatch.eKind = SIR_VULKAN_RECORDED_COMPUTE_DISPATCH;
	dispatch.pPipeline = &compute;
	dispatch.nGroupCountX = 1;
	dispatch.nGroupCountY = 2;
	dispatch.nGroupCountZ = 3;
	if ( sirVulkanValidateRecordedCommands( &dispatch, 1 ) != SIR_VULKAN_SUCCESS ) {
		return 13;
	}
	dispatch.nGroupCountY = 0;
	if ( sirVulkanValidateRecordedCommands( &dispatch, 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 14;
	}
	dispatch.nGroupCountY = 1;
	dispatch.pPipeline = nullptr;
	if ( sirVulkanValidateRecordedCommands( &dispatch, 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 15;
	}
	dispatch.pPipeline = &compute;
	commands[6] = dispatch;
	if ( sirVulkanValidateRecordedCommands( commands, 7 ) != SIR_VULKAN_SUCCESS ) {
		return 16;
	}
	// A multi-subresource barrier validates every member and preserves original state.
	sirVulkanImage_t range = {};
	range.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	range.eType = VK_IMAGE_TYPE_2D;
	range.nMips = 2;
	range.nLayers = 2;
	range.nUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	sirVulkanRecordedCommand_t rangeCommand = {};
	rangeCommand.eKind = SIR_VULKAN_RECORDED_IMAGE_BARRIER;
	rangeCommand.pImage = &range;
	rangeCommand.imageBarrier = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, 2, 2, false };
	if ( sirVulkanValidateRecordedCommands( &rangeCommand, 1 ) != SIR_VULKAN_SUCCESS ) {
		return 17;
	}
	VkImageLayout eTracked = VK_IMAGE_LAYOUT_GENERAL;
	if ( !sirVulkanGetSubresourceLayout( &range, 1, 1, &eTracked ) || eTracked != VK_IMAGE_LAYOUT_UNDEFINED ) {
		return 18;
	}
	if ( !sirVulkanSetSubresourceLayout( &range, 1, 0, VK_IMAGE_LAYOUT_GENERAL ) ||
	     sirVulkanValidateRecordedCommands( &rangeCommand, 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 19;
	}
	rangeCommand.imageBarrier.nMipCount = 0;
	if ( sirVulkanValidateRecordedCommands( &rangeCommand, 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 20;
	}
	rangeCommand.imageBarrier.nMipCount = 2;
	rangeCommand.imageBarrier.nLayerCount = 3;
	if ( sirVulkanValidateRecordedCommands( &rangeCommand, 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 21;
	}
	// Chained range barriers use shadow state without changing the original image.
	sirVulkanImage_t orderedRange = {};
	orderedRange.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	orderedRange.eType = VK_IMAGE_TYPE_2D;
	orderedRange.nMips = 2;
	orderedRange.nLayers = 2;
	orderedRange.nUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	orderedRange.szUploadedSubresources[0] = 0xFULL;
	sirVulkanRecordedCommand_t szRangeSequence[2] = {};
	szRangeSequence[0].eKind = SIR_VULKAN_RECORDED_IMAGE_BARRIER;
	szRangeSequence[0].pImage = &orderedRange;
	szRangeSequence[0].imageBarrier = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, 2, 2, false };
	szRangeSequence[1].eKind = SIR_VULKAN_RECORDED_IMAGE_BARRIER;
	szRangeSequence[1].pImage = &orderedRange;
	szRangeSequence[1].imageBarrier = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0, 2, 2, false };
	VkImageLayout eOrdered = VK_IMAGE_LAYOUT_GENERAL;
	if ( sirVulkanValidateRecordedCommands( szRangeSequence, 2 ) != SIR_VULKAN_SUCCESS ||
	     !sirVulkanGetSubresourceLayout( &orderedRange, 1, 1, &eOrdered ) || eOrdered != VK_IMAGE_LAYOUT_UNDEFINED ) {
		return 23;
	}
	szRangeSequence[1].imageBarrier.nLayerCount = 3;
	if ( sirVulkanValidateRecordedCommands( szRangeSequence, 2 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 24;
	}

	// Independent copies need no internal memory barrier; aliased read/write buffers do.
	sirVulkanRecordedCommand_t hazards[7] = {};
	hazards[0].eKind = SIR_VULKAN_RECORDED_BUFFER_COPY;
	hazards[0].bufferCopy = { &source, &staging, 0, 0, 256 };
	hazards[1].eKind = SIR_VULKAN_RECORDED_BUFFER_COPY;
	hazards[1].bufferCopy = { &source, &readback, 0, 0, 256 };
	hazards[2].eKind = SIR_VULKAN_RECORDED_BUFFER_COPY;
	hazards[2].bufferCopy = { &staging, &readback, 0, 256, 256 };
	hazards[3].eKind = SIR_VULKAN_RECORDED_TEXTURE_COPY;
	hazards[3].pBuffer = &readback;
	hazards[3].textureCopy.bBufferToImage = true;
	hazards[4].eKind = SIR_VULKAN_RECORDED_TEXTURE_COPY;
	hazards[4].pBuffer = &source;
	hazards[4].textureCopy.bBufferToImage = true;
	hazards[5].eKind = SIR_VULKAN_RECORDED_TEXTURE_COPY;
	hazards[5].pBuffer = &source;
	hazards[5].textureCopy.bBufferToImage = false;
	hazards[6].eKind = SIR_VULKAN_RECORDED_COMPUTE_DISPATCH;
	if ( sirVulkanRecordedNeedsBufferBarrier( hazards, 0 ) || sirVulkanRecordedNeedsBufferBarrier( hazards, 1 ) ||
	     !sirVulkanRecordedNeedsBufferBarrier( hazards, 2 ) || !sirVulkanRecordedNeedsBufferBarrier( hazards, 3 ) ||
	     sirVulkanRecordedNeedsBufferBarrier( hazards, 4 ) || !sirVulkanRecordedNeedsBufferBarrier( hazards, 5 ) ||
	     sirVulkanRecordedNeedsBufferBarrier( hazards, 6 ) ||
	     sirVulkanRecordedNeedsBufferBarrier( nullptr, 1 ) ||
	     sirVulkanRecordedNeedsBufferBarrier( hazards, SIR_VULKAN_COPY_BATCH_CAPACITY ) ) {
		return 22;
	}
	printf( "Vulkan ordered mixed command validation passed\n" );
	return 0;
}
