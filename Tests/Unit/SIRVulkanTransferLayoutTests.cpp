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
	sirVulkanFormatBlock_t block = {};
	if ( !sirVulkanGetFormatBlock( VK_FORMAT_R8_UNORM, &block ) || block.nBlockWidth != 1 || block.nBlockHeight != 1 ||
	     block.nBytesPerBlock != 1 || !sirVulkanGetFormatBlock( VK_FORMAT_R16G16B16A16_SFLOAT, &block ) || block.nBytesPerBlock != 8 ||
	     !sirVulkanGetFormatBlock( VK_FORMAT_BC1_RGBA_UNORM_BLOCK, &block ) || block.nBlockWidth != 4 || block.nBlockHeight != 4 ||
	     block.nBytesPerBlock != 8 || !sirVulkanGetFormatBlock( VK_FORMAT_BC7_UNORM_BLOCK, &block ) || block.nBytesPerBlock != 16 ||
	     sirVulkanGetFormatBlock( VK_FORMAT_D32_SFLOAT, &block ) || sirVulkanGetFormatBlock( VK_FORMAT_R8_UNORM, nullptr ) ) {
		return 1;
	}
	sirVulkanImage_t image = {};
	image.eType = VK_IMAGE_TYPE_2D;
	image.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	image.nWidth = 16;
	image.nHeight = 16;
	image.nDepth = 1;
	image.nMips = 3;
	image.nLayers = 2;
	image.nUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	sirVulkanBuffer_t buffer = {};
	buffer.nSize = 4096;
	buffer.nUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	sirVulkanTextureTransfer_t copy = {};
	copy.nMip = 2;
	copy.nLayer = 1;
	copy.offset = { 0, 0, 0 };
	copy.extent = { 4, 4, 1 };
	copy.bBufferToImage = true;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_SUCCESS ) {
		return 2;
	}
	copy.bBufferToImage = false;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 3;
	}
	image.szUploadedSubresources[( 1 * 3 + 2 ) / 64] |= 1ULL << ( ( 1 * 3 + 2 ) % 64 );
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_SUCCESS ) {
		return 4;
	}
	copy.nLayer = 0;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 5;
	}
	copy.nLayer = 1;
	copy.bBufferToImage = true;
	copy.nMip = 1;
	copy.offset = { 7, 0, 0 };
	copy.extent = { 2, 4, 1 };
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 6;
	}
	copy.offset = { 0, 0, 0 };
	copy.extent = { 4, 4, 1 };
	copy.nBufferRowLength = 8;
	buffer.nSize = 111;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 7;
	}
	buffer.nSize = 112;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_SUCCESS ) {
		return 8;
	}
	copy.nBufferRowLength = 3;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 9;
	}
	copy.nBufferRowLength = 0;
	copy.nBufferOffset = 2;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 10;
	}

	image.eFormat = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	image.nWidth = 10;
	image.nHeight = 6;
	image.nMips = 1;
	image.nLayers = 1;
	buffer.nSize = 48;
	copy = {};
	copy.extent = { 10, 6, 1 };
	copy.bBufferToImage = true;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_SUCCESS ) {
		return 11;
	}
	copy.offset = { 2, 0, 0 };
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 12;
	}
	copy.offset = { 0, 0, 0 };
	copy.extent.width = 5;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 13;
	}
	copy.extent.width = 10;
	copy.nBufferRowLength = 16;
	copy.nBufferImageHeight = 8;
	buffer.nSize = 55;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 14;
	}
	buffer.nSize = 56;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_SUCCESS ) {
		return 15;
	}
	copy.nBufferImageHeight = 7;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 16;
	}
	copy.nBufferImageHeight = 0;
	copy.nBufferRowLength = 0;
	image.eFormat = VK_FORMAT_BC7_UNORM_BLOCK;
	buffer.nSize = 96;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_SUCCESS ) {
		return 17;
	}
	image.eFormat = VK_FORMAT_D32_SFLOAT;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_API_UNSUPPORTED ) {
		return 18;
	}
	image.eFormat = VK_FORMAT_R8_UNORM;
	image.nLayers = SIR_VULKAN_MAX_TRACKED_SUBRESOURCES + 1;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 19;
	}
	image.nLayers = 1;
	copy.nMip = 32;
	if ( sirVulkanValidateTextureTransfer( &buffer, &image, copy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 20;
	}
	// Layouts are independently tracked for every mip/layer pair.
	// The packed table must not change adjacent two-bit entries.
	sirVulkanImage_t tracked = {};
	tracked.nMips = 3;
	tracked.nLayers = 2;
	VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
	if ( !sirVulkanGetSubresourceLayout( &tracked, 2, 1, &layout ) || layout != VK_IMAGE_LAYOUT_UNDEFINED ||
	     !sirVulkanSetSubresourceLayout( &tracked, 2, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) ||
	     !sirVulkanGetSubresourceLayout( &tracked, 2, 1, &layout ) || layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		return 21;
	}
	if ( !sirVulkanGetSubresourceLayout( &tracked, 0, 0, &layout ) || layout != VK_IMAGE_LAYOUT_UNDEFINED ||
	     !sirVulkanGetSubresourceLayout( &tracked, 1, 1, &layout ) || layout != VK_IMAGE_LAYOUT_UNDEFINED ||
	     !sirVulkanSetSubresourceLayout( &tracked, 1, 1, VK_IMAGE_LAYOUT_GENERAL ) ||
	     !sirVulkanGetSubresourceLayout( &tracked, 2, 1, &layout ) || layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
	     !sirVulkanGetSubresourceLayout( &tracked, 1, 1, &layout ) || layout != VK_IMAGE_LAYOUT_GENERAL ) {
		return 22;
	}
	if ( sirVulkanSetSubresourceLayout( &tracked, 2, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) ||
	     !sirVulkanGetSubresourceLayout( &tracked, 2, 1, &layout ) || layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
	     !sirVulkanSetSubresourceLayout( &tracked, 2, 1, VK_IMAGE_LAYOUT_UNDEFINED ) ||
	     !sirVulkanGetSubresourceLayout( &tracked, 2, 1, &layout ) || layout != VK_IMAGE_LAYOUT_UNDEFINED ) {
		return 23;
	}
	if ( sirVulkanGetSubresourceLayout( nullptr, 0, 0, &layout ) || sirVulkanGetSubresourceLayout( &tracked, 3, 1, &layout ) ||
	     sirVulkanGetSubresourceLayout( &tracked, 0, 2, &layout ) || sirVulkanGetSubresourceLayout( &tracked, 0, 0, nullptr ) ||
	     sirVulkanSetSubresourceLayout( &tracked, 3, 1, VK_IMAGE_LAYOUT_GENERAL ) ||
	     sirVulkanSetSubresourceLayout( &tracked, 0, 2, VK_IMAGE_LAYOUT_GENERAL ) ) {
		return 24;
	}
	// Final byte of the 4096-subresource table must be addressable.
	tracked.nMips = 32;
	tracked.nLayers = 128;
	if ( !sirVulkanSetSubresourceLayout( &tracked, 31, 127, VK_IMAGE_LAYOUT_GENERAL ) ||
	     !sirVulkanGetSubresourceLayout( &tracked, 31, 127, &layout ) || layout != VK_IMAGE_LAYOUT_GENERAL ||
	     !sirVulkanGetSubresourceLayout( &tracked, 30, 127, &layout ) || layout != VK_IMAGE_LAYOUT_UNDEFINED ||
	     !sirVulkanGetSubresourceLayout( &tracked, 31, 126, &layout ) || layout != VK_IMAGE_LAYOUT_UNDEFINED ) {
		return 25;
	}
	tracked.nLayers = 129;
	if ( sirVulkanGetSubresourceLayout( &tracked, 0, 0, &layout ) ||
	     sirVulkanSetSubresourceLayout( &tracked, 0, 0, VK_IMAGE_LAYOUT_GENERAL ) ) {
		return 26;
	}
	// 3D buffer/image copies account for the full depth-slice footprint.
	sirVulkanImage_t volume = {};
	volume.eType = VK_IMAGE_TYPE_3D;
	volume.eFormat = VK_FORMAT_R8G8B8A8_UNORM;
	volume.nWidth = 8;
	volume.nHeight = 8;
	volume.nDepth = 4;
	volume.nMips = 2;
	volume.nLayers = 1;
	volume.nUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	sirVulkanBuffer_t volumeBuffer = {};
	volumeBuffer.nUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	sirVulkanTextureTransfer_t volumeCopy = {};
	volumeCopy.offset = { 0, 0, 1 };
	volumeCopy.extent = { 4, 2, 3 };
	volumeCopy.bBufferToImage = true;
	volumeBuffer.nSize = 95;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 27;
	}
	volumeBuffer.nSize = 96;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_SUCCESS ) {
		return 28;
	}
	volumeCopy.nBufferRowLength = 8;
	volumeCopy.nBufferImageHeight = 4;
	volumeBuffer.nSize = 303;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 29;
	}
	volumeBuffer.nSize = 304;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_SUCCESS ) {
		return 30;
	}
	volumeCopy.offset.z = 2;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 31;
	}
	volumeCopy.offset.z = 0;
	volumeCopy.nMip = 1;
	volumeCopy.extent.depth = 2;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_SUCCESS ) {
		return 32;
	}
	volumeCopy.offset.z = 1;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 33;
	}
	volumeCopy.offset.z = -1;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 34;
	}
	volumeCopy.offset.z = 0;
	volumeCopy.nLayer = 1;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 35;
	}
	volumeCopy.nLayer = 0;
	volumeCopy.extent.depth = 0;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 36;
	}
	volumeCopy.extent.depth = 2;
	volumeCopy.nMip = 0;
	volumeCopy.bBufferToImage = false;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 37;
	}
	volume.szUploadedSubresources[0] |= 1ULL;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_SUCCESS ) {
		return 38;
	}
	volume.eType = VK_IMAGE_TYPE_2D;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 39;
	}
	volumeCopy.extent.depth = 1;
	volumeCopy.offset.z = 1;
	if ( sirVulkanValidateTextureTransfer( &volumeBuffer, &volume, volumeCopy ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 40;
	}
	// Initialization is per entire mip/layer, not per uploaded rectangle or depth slice.
	sirVulkanImage_t coverage = {};
	coverage.eType = VK_IMAGE_TYPE_2D;
	coverage.nWidth = 16;
	coverage.nHeight = 8;
	coverage.nDepth = 1;
	coverage.nMips = 3;
	coverage.nLayers = 2;
	coverage.nUsage = VK_IMAGE_USAGE_SAMPLED_BIT;
	sirVulkanTextureTransfer_t uploaded = {};
	uploaded.bBufferToImage = true;
	uploaded.nMip = 1;
	uploaded.nLayer = 1;
	uploaded.extent = { 8, 4, 1 };
	if ( !sirVulkanTransferCoversSubresource( &coverage, uploaded ) ||
	     sirVulkanUploadFinalLayout( &coverage, uploaded ) != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		return 41;
	}
	uploaded.extent.width = 4;
	if ( sirVulkanTransferCoversSubresource( &coverage, uploaded ) ||
	     sirVulkanUploadFinalLayout( &coverage, uploaded ) != VK_IMAGE_LAYOUT_GENERAL ) {
		return 42;
	}
	coverage.szUploadedSubresources[0] = 1ULL << ( uploaded.nLayer * coverage.nMips + uploaded.nMip );
	if ( sirVulkanUploadFinalLayout( &coverage, uploaded ) != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		return 43;
	}
	coverage.nUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
	if ( sirVulkanUploadFinalLayout( &coverage, uploaded ) != VK_IMAGE_LAYOUT_GENERAL ) {
		return 44;
	}
	uploaded.extent.width = 8;
	uploaded.offset.x = 1;
	if ( sirVulkanTransferCoversSubresource( &coverage, uploaded ) ) {
		return 45;
	}
	uploaded.offset.x = 0;
	uploaded.bBufferToImage = false;
	if ( sirVulkanTransferCoversSubresource( &coverage, uploaded ) ) {
		return 46;
	}
	uploaded.bBufferToImage = true;
	uploaded.nMip = coverage.nMips;
	if ( sirVulkanTransferCoversSubresource( &coverage, uploaded ) ||
	     sirVulkanUploadFinalLayout( &coverage, uploaded ) != VK_IMAGE_LAYOUT_UNDEFINED ||
	     sirVulkanTransferCoversSubresource( nullptr, uploaded ) ) {
		return 47;
	}
	coverage.eType = VK_IMAGE_TYPE_3D;
	coverage.nMips = 2;
	coverage.nLayers = 1;
	coverage.nDepth = 4;
	uploaded.nMip = 1;
	uploaded.nLayer = 0;
	uploaded.extent = { 8, 4, 2 };
	if ( !sirVulkanTransferCoversSubresource( &coverage, uploaded ) ) {
		return 48;
	}
	uploaded.extent.depth = 1;
	if ( sirVulkanTransferCoversSubresource( &coverage, uploaded ) ) {
		return 49;
	}
	printf( "Vulkan transfer format and subresource contracts passed\n" );
	return 0;
}
