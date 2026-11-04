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

#include <string.h>

int main( void )
{
	SIRVulkanDriver driver;
	SIRGraphicsRegistry registry;
	if ( registry.registerDriver( &driver ) != SIR_GRAPHICS_SUCCESS || registry.findDriver( SIR_GRAPHICS_BACKEND_VULKAN ) != &driver ||
	     registry.registerDriver( &driver ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     driver.getAdapterCount( nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     driver.queryAdapter( 0, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 1;
	}

	sirVulkanAdapterInfo_t nativeAdapter = {};
	nativeAdapter.nApiVersion = VK_API_VERSION_1_4;
	nativeAdapter.nVendorId = 0x1234;
	nativeAdapter.bComputeCapable = true;
	nativeAdapter.bSoftwareAdapter = true;
	memcpy( nativeAdapter.szDeviceName, "Synthetic Vulkan", sizeof( "Synthetic Vulkan" ) );
	sirGraphicsAdapterInfo_t reported = {};
	if ( sirVulkanDescribeAdapter( 3, nativeAdapter, &reported ) != SIR_GRAPHICS_SUCCESS || reported.nAdapterIndex != 3 ||
	     reported.nVendorId != 0x1234 || reported.nFeatureMask != SIR_GRAPHICS_FEATURE_COMPUTE || !reported.bSoftwareAdapter ||
	     sirVulkanDescribeAdapter( 0, nativeAdapter, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 8;
	}
	nativeAdapter.bComputeCapable = false;
	nativeAdapter.bSoftwareAdapter = false;
	if ( sirVulkanDescribeAdapter( 0, nativeAdapter, &reported ) != SIR_GRAPHICS_SUCCESS || reported.nFeatureMask != 0 ||
	     reported.bSoftwareAdapter ) {
		return 9;
	}

	uint32_t szBatchCounts[SIR_VULKAN_TRANSFER_SLOT_COUNT + 1] = { 1, 7 };
	uint32_t nBatchTotal = 13;
	if ( sirVulkanValidateBatchCounts( szBatchCounts, 2, &nBatchTotal ) != SIR_GRAPHICS_SUCCESS || nBatchTotal != 8 ) {
		return 10;
	}
	szBatchCounts[1] = 8;
	if ( sirVulkanValidateBatchCounts( szBatchCounts, 2, &nBatchTotal ) != SIR_GRAPHICS_CAPACITY_EXCEEDED || nBatchTotal != 0 ) {
		return 11;
	}
	szBatchCounts[1] = 0;
	if ( sirVulkanValidateBatchCounts( szBatchCounts, 2, &nBatchTotal ) != SIR_GRAPHICS_INVALID_ARGUMENT || nBatchTotal != 0 ) {
		return 12;
	}
	if ( sirVulkanValidateBatchCounts( szBatchCounts, SIR_VULKAN_TRANSFER_SLOT_COUNT + 1, &nBatchTotal ) !=
	         SIR_GRAPHICS_CAPACITY_EXCEEDED ||
	     sirVulkanValidateBatchCounts( nullptr, 2, &nBatchTotal ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirVulkanValidateBatchCounts( szBatchCounts, 0, &nBatchTotal ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirVulkanValidateBatchCounts( szBatchCounts, 1, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 13;
	}

	// Physical-device priority is stable and independent of the Vulkan loader.
	if ( sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) != 4 ||
	     sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) != 3 ||
	     sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ) != 2 ||
	     sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_CPU ) != 1 ||
	     sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_OTHER ) != 1 ) {
		return 14;
	}

	SIRVulkanRenderingDevice inert;
	sirGraphicsCapabilities_t capabilities = {};
	sirGraphicsMemoryBudget_t budget = {};
	sirGraphicsHandle_t handle = {};
	sirGraphicsMappedBuffer_t mapped = {};
	sirGraphicsCompletion_t completion = {};
	bool bCompleted = true;
	if ( inert.getBackend() != SIR_GRAPHICS_BACKEND_VULKAN || inert.getDeviceGeneration() != 0 ||
	     inert.queryCapabilities( &capabilities ) != SIR_GRAPHICS_BUSY || inert.queryMemoryBudget( &budget ) != SIR_GRAPHICS_BUSY ||
	     inert.mapBuffer( handle, 0, 4, &mapped ) != SIR_GRAPHICS_INVALID_HANDLE ||
	     inert.poll( completion, &bCompleted ) != SIR_GRAPHICS_INVALID_ARGUMENT || bCompleted ||
	     inert.shutdown() != SIR_GRAPHICS_SUCCESS ) {
		return 2;
	}

	sirGraphicsBufferDesc_t invalid = {};
	sirGraphicsTextureDesc_t texture = {};
	sirGraphicsViewDesc_t view = {};
	sirGraphicsSamplerDesc_t sampler = {};
	if ( inert.createTexture( texture, &handle ) != SIR_GRAPHICS_BUSY || inert.createView( view, &handle ) != SIR_GRAPHICS_BUSY ||
	     inert.createSampler( sampler, &handle ) != SIR_GRAPHICS_BUSY || inert.createBuffer( invalid, &handle ) != SIR_GRAPHICS_BUSY ||
	     handle.nGeneration != 0 ) {
		return 3;
	}
	sirGraphicsDeviceCreateDesc_t create = {};
	create.nFramesInFlight = 2;
	SIRRenderingDevice *pDevice = reinterpret_cast<SIRRenderingDevice *>( &inert );
	if ( driver.createDevice( create, &pDevice ) != SIR_GRAPHICS_UNSUPPORTED || pDevice != nullptr ||
	     driver.destroyDevice( &inert ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 4;
	}
	create.bHeadless = true;
	create.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	pDevice = reinterpret_cast<SIRRenderingDevice *>( &inert );
	if ( driver.createDevice( create, &pDevice ) != SIR_GRAPHICS_UNSUPPORTED || pDevice != nullptr ) {
		return 6;
	}
	create.nRequiredFeatures = 0;
	create.nAdapterIndex = 32;
	pDevice = reinterpret_cast<SIRRenderingDevice *>( &inert );
	if ( driver.createDevice( create, &pDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT || pDevice != nullptr ||
	     driver.queryAdapter( 32, &reported ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 15;
	}
	create.nAdapterIndex = 0;
	create.nFramesInFlight = SIR_VULKAN_TRANSFER_SLOT_COUNT + 1;
	pDevice = reinterpret_cast<SIRRenderingDevice *>( &inert );
	if ( driver.createDevice( create, &pDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT || pDevice != nullptr ) {
		return 7;
	}
	if ( registry.unregisterDriver( SIR_GRAPHICS_BACKEND_VULKAN ) != SIR_GRAPHICS_SUCCESS || registry.getCount() != 0 ) {
		return 5;
	}
	return 0;
}
