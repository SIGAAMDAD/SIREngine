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
	const sirVulkanPoolBudget_t budget = { 8ULL << 20, 2ULL << 20, 2ULL << 20 };
	const sirVulkanResult_t eInit = device.initialize( &budget );
	if ( eInit == SIR_VULKAN_LOADER_UNAVAILABLE || eInit == SIR_VULKAN_API_UNSUPPORTED || eInit == SIR_VULKAN_DEVICE_UNAVAILABLE ) {
		printf( "SKIP: Vulkan 1.4 device unavailable\n" );
		return 77;
	}
	if ( eInit != SIR_VULKAN_SUCCESS ) {
		return 1;
	}
	sirVulkanMemoryStats_t before = {};
	if ( !device.queryMemoryStats( &before ) || before.nBlockBytes < budget.nResidentBytes + budget.nUploadBytes + budget.nReadbackBytes ) {
		return 2;
	}

	sirVulkanBuffer_t upload = {};
	sirVulkanBuffer_t readback = {};
	sirVulkanBuffer_t gpu = {};
	const VkBufferUsageFlags nTransfer = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if ( device.createBuffer( 65536, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, &upload ) != SIR_VULKAN_SUCCESS ||
	     device.createBuffer( 65536, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, &readback ) != SIR_VULKAN_SUCCESS ||
	     device.createBuffer( 65536, nTransfer, false, &gpu ) != SIR_VULKAN_SUCCESS || upload.pMappedData == nullptr ||
	     readback.pMappedData == nullptr || gpu.pMappedData != nullptr ) {
		return 3;
	}
	void *pOriginalUpload = upload.pMappedData;
	uint8_t szBytes[64] = {};
	memset( szBytes, 0x5a, sizeof( szBytes ) );
	if ( device.writeBuffer( &upload, 0, szBytes, sizeof( szBytes ) ) != SIR_VULKAN_SUCCESS || upload.pMappedData != pOriginalUpload ) {
		return 4;
	}

	sirVulkanImage_t image = {};
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent = { 64, 64, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( device.createImage( imageInfo, &image ) != SIR_VULKAN_SUCCESS ) {
		return 5;
	}
	sirVulkanMemoryStats_t afterCreate = {};
	if ( !device.queryMemoryStats( &afterCreate ) || afterCreate.nBlockBytes != before.nBlockBytes ) {
		return 6;
	}
	sirVulkanBuffer_t oversized = {};
	if ( device.createBuffer( 16ULL << 20, nTransfer, false, &oversized ) != SIR_VULKAN_ALLOCATION_FAILED ||
	     oversized.hBuffer != VK_NULL_HANDLE || oversized.pAllocation != nullptr ) {
		return 7;
	}
	sirVulkanMemoryStats_t afterReject = {};
	if ( !device.queryMemoryStats( &afterReject ) || afterReject.nBlockBytes != before.nBlockBytes ) {
		return 8;
	}
	if ( device.destroyImage( &image ) != SIR_VULKAN_SUCCESS || device.destroyIdleBuffer( &gpu ) != SIR_VULKAN_SUCCESS ||
	     device.destroyIdleBuffer( &upload ) != SIR_VULKAN_SUCCESS || device.destroyIdleBuffer( &readback ) != SIR_VULKAN_SUCCESS ) {
		return 9;
	}
	sirVulkanMemoryStats_t afterRetire = {};
	if ( !device.queryMemoryStats( &afterRetire ) || afterRetire.nBlockBytes != before.nBlockBytes || !device.shutdown() ) {
		return 10;
	}
	printf( "Sealed Vulkan pools and persistent mappings passed\n" );
	return 0;
}
