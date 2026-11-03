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
