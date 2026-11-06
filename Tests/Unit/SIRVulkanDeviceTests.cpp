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
#include <string.h>

int main( void )
{
	SIRVulkanDevice device;
	sirVulkanAdapterInfo_t adapter = {};
	if ( device.queryAdapter( &adapter ) ) {
		return 1;
	}
	const sirVulkanResult_t eInit = device.initialize();
	if ( eInit == SIR_VULKAN_LOADER_UNAVAILABLE || eInit == SIR_VULKAN_API_UNSUPPORTED || eInit == SIR_VULKAN_DEVICE_UNAVAILABLE ) {
		printf( "SKIP: Vulkan 1.4 loader/device unavailable (result %u)\n", ( unsigned )eInit );
		return 77;
	}
	if ( eInit != SIR_VULKAN_SUCCESS || !device.isReady() ) {
		fprintf( stderr, "Vulkan initialization failed: %u\n", ( unsigned )eInit );
		return 2;
	}
	if ( device.initialize() != SIR_VULKAN_INVALID_STATE || !device.queryAdapter( &adapter ) || adapter.nApiVersion < VK_API_VERSION_1_4 ||
	     adapter.nMemoryHeapCount == 0 ) {
		return 3;
	}

	// Reject image-view aspects that the image format cannot contain, before the native call.
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent = { 4, 4, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	sirVulkanImage_t image = {};
	if ( device.createImage( imageInfo, &image ) != SIR_VULKAN_SUCCESS ) {
		return 20;
	}
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image.hImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = image.eFormat;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;
	sirVulkanImageView_t view = {};
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( device.createImageView( &image, viewInfo, &view ) != SIR_VULKAN_INVALID_ARGUMENT || view.hView != VK_NULL_HANDLE ) {
		return 21;
	}
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_COLOR_BIT;
	if ( device.createImageView( &image, viewInfo, &view ) != SIR_VULKAN_INVALID_ARGUMENT || view.hView != VK_NULL_HANDLE ) {
		return 22;
	}
	viewInfo.subresourceRange.aspectMask = 0;
	if ( device.createImageView( &image, viewInfo, &view ) != SIR_VULKAN_INVALID_ARGUMENT || view.hView != VK_NULL_HANDLE ) {
		return 23;
	}
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	if ( device.createImageView( &image, viewInfo, &view ) != SIR_VULKAN_SUCCESS ) {
		return 24;
	}
	if ( device.destroyImageView( &view ) != SIR_VULKAN_SUCCESS || device.destroyImage( &image ) != SIR_VULKAN_SUCCESS ) {
		return 25;
	}

	sirVulkanMemoryStats_t before = {};
	if ( !device.queryMemoryStats( &before ) || before.nLiveBuffers != 0 ) {
		return 4;
	}

	sirVulkanBuffer_t buffer = {};
	if ( device.createBuffer( 4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, &buffer ) != SIR_VULKAN_SUCCESS ) {
		return 5;
	}
	if ( device.shutdown() ) {
		return 6;
	}
	uint8_t szBytes[128];
	memset( szBytes, 0xa5, sizeof( szBytes ) );
	if ( device.writeBuffer( &buffer, 0, szBytes, sizeof( szBytes ) ) != SIR_VULKAN_SUCCESS ||
	     device.writeBuffer( &buffer, 4090, szBytes, sizeof( szBytes ) ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 7;
	}
	sirVulkanBuffer_t foreign = buffer;
	foreign.pOwnerAllocator = nullptr;
	if ( device.writeBuffer( &foreign, 0, szBytes, sizeof( szBytes ) ) != SIR_VULKAN_INVALID_ARGUMENT ||
	     device.destroyIdleBuffer( &foreign ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 11;
	}
	sirVulkanMemoryStats_t active = {};
	if ( !device.queryMemoryStats( &active ) || active.nLiveBuffers != 1 || active.nAllocationBytes == 0 ) {
		return 8;
	}
	// Copy a CPU-written upload through device-local memory and back. Use
	// identical buffer contents to check queue completion and visibility.
	sirVulkanBuffer_t gpu = {};
	sirVulkanBuffer_t readback = {};
	if ( device.createBuffer( 4096, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, false, &gpu ) !=
	     SIR_VULKAN_SUCCESS ) {
		return 12;
	}
	if ( device.createBuffer( 4096, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, &readback ) != SIR_VULKAN_SUCCESS ) {
		return 13;
	}
	sirVulkanCompletion_t first = {};
	sirVulkanCompletion_t second = {};
	if ( device.submitBufferCopy( &buffer, 0, &gpu, 0, 128, &first ) != SIR_VULKAN_SUCCESS || first.nValue == 0 ) {
		return 14;
	}
	if ( device.submitBufferCopy( &gpu, 0, &readback, 0, 128, &second ) != SIR_VULKAN_SUCCESS || second.nValue <= first.nValue ) {
		return 15;
	}
	bool bCompleted = false;
	if ( device.queryCompletion( &second, &bCompleted ) != SIR_VULKAN_SUCCESS ||
	     device.waitCompletion( &second, 5000000000ull ) != SIR_VULKAN_SUCCESS ) {
		return 16;
	}
	uint8_t szReadback[128] = {};
	if ( device.readBuffer( &readback, 0, szReadback, sizeof( szReadback ) ) != SIR_VULKAN_SUCCESS ||
	     memcmp( szBytes, szReadback, sizeof( szReadback ) ) != 0 ) {
		return 17;
	}
	sirVulkanCompletion_t invalid = second;
	invalid.nDeviceSerial++;
	if ( device.waitCompletion( &invalid, 0 ) != SIR_VULKAN_INVALID_ARGUMENT ||
	     device.submitBufferCopy( &readback, 0, &gpu, 0, 128, &invalid ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 18;
	}
	if ( device.destroyIdleBuffer( &gpu ) != SIR_VULKAN_SUCCESS || device.destroyIdleBuffer( &readback ) != SIR_VULKAN_SUCCESS ) {
		return 19;
	}
	if ( device.destroyIdleBuffer( &buffer ) != SIR_VULKAN_SUCCESS || buffer.pAllocation != nullptr ||
	     device.destroyIdleBuffer( &buffer ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 9;
	}
	sirVulkanMemoryStats_t after = {};
	if ( !device.queryMemoryStats( &after ) || after.nLiveBuffers != 0 || !device.shutdown() || device.isReady() ) {
		return 10;
	}
	printf( "Vulkan headless + VMA smoke OK: %s\n", adapter.szDeviceName );
	return 0;
}
