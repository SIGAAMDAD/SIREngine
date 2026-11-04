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
	if ( !sirVulkanGetFormatBlock( VK_FORMAT_R8_UNORM, &block ) || block.nBlockWidth != 1 ||
	     block.nBlockHeight != 1 || block.nBytesPerBlock != 1 ||
	     !sirVulkanGetFormatBlock( VK_FORMAT_R16G16B16A16_SFLOAT, &block ) || block.nBytesPerBlock != 8 ||
	     !sirVulkanGetFormatBlock( VK_FORMAT_BC1_RGBA_UNORM_BLOCK, &block ) || block.nBlockWidth != 4 ||
	     block.nBlockHeight != 4 || block.nBytesPerBlock != 8 ||
	     !sirVulkanGetFormatBlock( VK_FORMAT_BC7_UNORM_BLOCK, &block ) || block.nBytesPerBlock != 16 ||
	     sirVulkanGetFormatBlock( VK_FORMAT_D32_SFLOAT, &block ) ||
	     sirVulkanGetFormatBlock( VK_FORMAT_R8_UNORM, nullptr ) ) {
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
	printf( "Vulkan transfer format and subresource contracts passed\n" );
	return 0;
}
