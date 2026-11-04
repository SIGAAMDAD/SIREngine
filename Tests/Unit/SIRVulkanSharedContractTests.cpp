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
#include <Renderer/SIRGraphicsRegistry.h>

int main( void )
{
	SIRVulkanDriver driver;
	SIRGraphicsRegistry registry;
	if ( registry.registerDriver( &driver ) != SIR_GRAPHICS_SUCCESS ||
	     registry.findDriver( SIR_GRAPHICS_BACKEND_VULKAN ) != &driver ||
	     registry.registerDriver( &driver ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     driver.getAdapterCount( nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     driver.queryAdapter( 0, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 1;
	}

	SIRVulkanRenderingDevice inert;
	sirGraphicsCapabilities_t capabilities = {};
	sirGraphicsMemoryBudget_t budget = {};
	sirGraphicsHandle_t handle = {};
	sirGraphicsMappedBuffer_t mapped = {};
	sirGraphicsCompletion_t completion = {};
	bool bCompleted = true;
	if ( inert.getBackend() != SIR_GRAPHICS_BACKEND_VULKAN || inert.getDeviceGeneration() != 0 ||
	     inert.queryCapabilities( &capabilities ) != SIR_GRAPHICS_BUSY ||
	     inert.queryMemoryBudget( &budget ) != SIR_GRAPHICS_BUSY ||
	     inert.mapBuffer( handle, 0, 4, &mapped ) != SIR_GRAPHICS_INVALID_HANDLE ||
	     inert.poll( completion, &bCompleted ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     bCompleted || inert.shutdown() != SIR_GRAPHICS_SUCCESS ) {
		return 2;
	}

	sirGraphicsBufferDesc_t invalid = {};
	if ( inert.createBuffer( invalid, &handle ) != SIR_GRAPHICS_BUSY || handle.nGeneration != 0 ) {
		return 3;
	}
	sirGraphicsDeviceCreateDesc_t create = {};
	create.nFramesInFlight = 2;
	SIRRenderingDevice *pDevice = reinterpret_cast<SIRRenderingDevice *>( &inert );
	if ( driver.createDevice( create, &pDevice ) != SIR_GRAPHICS_UNSUPPORTED || pDevice != nullptr ||
	     driver.destroyDevice( &inert ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 4;
	}
	if ( registry.unregisterDriver( SIR_GRAPHICS_BACKEND_VULKAN ) != SIR_GRAPHICS_SUCCESS ||
	     registry.getCount() != 0 ) {
		return 5;
	}
	return 0;
}
