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

	sirVulkanBufferCopy_t copies[SIR_VULKAN_COPY_BATCH_CAPACITY + 1] = {};
	for ( uint32_t i = 0; i < SIR_VULKAN_COPY_BATCH_CAPACITY; ++i ) {
		copies[i] = { ( i & 1 ) == 0 ? &source : &staging,
		             ( i & 1 ) == 0 ? &staging : &readback,
		             ( i / 2 ) * 128, ( i / 2 ) * 128, 128 };
	}
	if ( sirVulkanValidateBufferCopies( copies, SIR_VULKAN_COPY_BATCH_CAPACITY ) != SIR_VULKAN_SUCCESS ||
	     sirVulkanValidateBufferCopies( copies, 1 ) != SIR_VULKAN_SUCCESS ) {
		return 1;
	}
	if ( sirVulkanValidateBufferCopies( copies, 0 ) != SIR_VULKAN_INVALID_ARGUMENT ||
	     sirVulkanValidateBufferCopies( nullptr, 1 ) != SIR_VULKAN_INVALID_ARGUMENT ||
	     sirVulkanValidateBufferCopies( copies, SIR_VULKAN_COPY_BATCH_CAPACITY + 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 2;
	}
	copies[1].pDestination = &staging;
	if ( sirVulkanValidateBufferCopies( copies, 2 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 3;
	}
	copies[1].pDestination = &readback;
	copies[1].pSource = nullptr;
	if ( sirVulkanValidateBufferCopies( copies, 2 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 4;
	}
	copies[1].pSource = &staging;
	copies[1].nBytes = 0;
	if ( sirVulkanValidateBufferCopies( copies, 2 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 5;
	}
	copies[1].nBytes = 128;
	copies[1].nSourceOffset = staging.nSize - 127;
	if ( sirVulkanValidateBufferCopies( copies, 2 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 6;
	}
	copies[1].nSourceOffset = 0;
	copies[1].nDestinationOffset = readback.nSize;
	if ( sirVulkanValidateBufferCopies( copies, 2 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 7;
	}
	copies[1].nDestinationOffset = 0;
	staging.nUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if ( sirVulkanValidateBufferCopies( copies, 2 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 8;
	}
	staging.nUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if ( sirVulkanValidateBufferCopies( copies, SIR_VULKAN_COPY_BATCH_CAPACITY ) != SIR_VULKAN_SUCCESS ) {
		return 9;
	}
	printf( "Vulkan copy batch validation passed\n" );
	return 0;
}
