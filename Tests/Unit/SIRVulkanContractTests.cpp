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

// Negative API-state tests do not need a Vulkan loader or GPU.
int main( void )
{
	SIRVulkanDevice device;
	sirVulkanBuffer_t buffer = {};
	sirVulkanCompletion_t completion = {};
	uint8_t szBytes[8] = {};
	bool bComplete = true;

	if ( device.isReady() || device.queryAdapter( nullptr ) || device.queryMemoryStats( nullptr ) ) {
		return 1;
	}
	if ( device.createBuffer( sizeof( szBytes ), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, &buffer ) != SIR_VULKAN_INVALID_STATE ) {
		return 2;
	}
	if ( device.writeBuffer( &buffer, 0, szBytes, sizeof( szBytes ) ) != SIR_VULKAN_INVALID_STATE ||
	     device.readBuffer( &buffer, 0, szBytes, sizeof( szBytes ) ) != SIR_VULKAN_INVALID_STATE ) {
		return 3;
	}
	if ( device.submitBufferCopy( &buffer, 0, &buffer, 0, sizeof( szBytes ), &completion ) != SIR_VULKAN_INVALID_STATE ) {
		return 4;
	}
	if ( device.queryCompletion( &completion, &bComplete ) != SIR_VULKAN_INVALID_STATE || bComplete ) {
		return 5;
	}
	if ( device.waitCompletion( &completion, 0 ) != SIR_VULKAN_INVALID_STATE ||
	     device.destroyIdleBuffer( &buffer ) != SIR_VULKAN_INVALID_STATE ) {
		return 6;
	}
	if ( !device.shutdown() || device.isReady() ) {
		return 7;
	}
	return 0;
}
