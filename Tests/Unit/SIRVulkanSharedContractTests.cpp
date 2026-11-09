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
	nativeAdapter.nMaximumTextureDimension = 8192;
	nativeAdapter.bComputeCapable = true;
	nativeAdapter.bSoftwareAdapter = true;
	memcpy( nativeAdapter.szDeviceName, "Synthetic Vulkan", sizeof( "Synthetic Vulkan" ) );
	sirGraphicsAdapterInfo_t reported = {};
	if ( sirVulkanDescribeAdapter( 3, nativeAdapter, &reported ) != SIR_GRAPHICS_SUCCESS || reported.nAdapterIndex != 3 ||
	     reported.nVendorId != 0x1234 || reported.nFeatureMask != ( SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_GRAPHICS ) ||
	     !reported.bSoftwareAdapter || sirVulkanDescribeAdapter( 0, nativeAdapter, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 8;
	}
	sirGraphicsCapabilities_t synthetic = {};
	if ( sirVulkanDescribeCapabilities( nativeAdapter, 41, &synthetic ) != SIR_GRAPHICS_SUCCESS ||
	     synthetic.eBackend != SIR_GRAPHICS_BACKEND_VULKAN || synthetic.nDeviceGeneration != 41 ||
	     synthetic.nMaximumTextureDimension != 8192 || synthetic.nMaximumSampleCount != 1 ||
	     synthetic.nFeatureMask != ( SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE |
	                                 SIR_GRAPHICS_FEATURE_MRT ) ||
	     !synthetic.bSoftwareAdapter || synthetic.nMaximumColorAttachments != 8 || synthetic.nMaximumBindingSlots != 32 ) {
		return 17;
	}
	sirGraphicsDeviceCreateDesc_t admission = {};
	admission.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	if ( sirVulkanValidateDeviceAdmission( admission, synthetic ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 18;
	}
	admission.bAllowSoftware = true;
	if ( sirVulkanValidateDeviceAdmission( admission, synthetic ) != SIR_GRAPHICS_SUCCESS ) {
		return 19;
	}
	admission.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( sirVulkanValidateDeviceAdmission( admission, synthetic ) != SIR_GRAPHICS_SUCCESS ) {
		return 20;
	}
	memset( nativeAdapter.szDeviceName, 'x', sizeof( nativeAdapter.szDeviceName ) );
	if ( sirVulkanDescribeAdapter( 0, nativeAdapter, &reported ) != SIR_GRAPHICS_SUCCESS ||
	     reported.szName[sizeof( reported.szName ) - 1] != '\0' ) {
		return 21;
	}
	if ( sirVulkanDescribeCapabilities( nativeAdapter, 0, &synthetic ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirVulkanDescribeCapabilities( nativeAdapter, 1, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 22;
	}
	nativeAdapter.bComputeCapable = false;
	nativeAdapter.bSoftwareAdapter = false;
	if ( sirVulkanDescribeAdapter( 0, nativeAdapter, &reported ) != SIR_GRAPHICS_SUCCESS ||
	     reported.nFeatureMask != SIR_GRAPHICS_FEATURE_GRAPHICS || reported.bSoftwareAdapter ) {
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

	// CPU-only queue discovery: the active native queue remains the graphics family.
	VkQueueFamilyProperties szFamilies[4] = {};
	szFamilies[0].queueCount = 1;
	szFamilies[0].queueFlags = VK_QUEUE_GRAPHICS_BIT;
	szFamilies[1].queueCount = 1;
	szFamilies[1].queueFlags = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
	szFamilies[2].queueCount = 1;
	szFamilies[2].queueFlags = VK_QUEUE_TRANSFER_BIT;
	szFamilies[3].queueCount = 1;
	szFamilies[3].queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
	sirVulkanQueueFamilySelection_t families = {};
	if ( !sirVulkanSelectQueueFamilies( szFamilies, 4, &families ) || families.nGraphicsFamilyIndex != 3 ||
	     families.nComputeFamilyIndex != 1 || families.nTransferFamilyIndex != 2 || !families.bGraphicsSupportsCompute ) {
		return 80;
	}
	if ( !sirVulkanSelectQueueFamilies( szFamilies, 1, &families ) || families.nGraphicsFamilyIndex != 0 ||
	     families.nComputeFamilyIndex != UINT32_MAX || families.nTransferFamilyIndex != 0 || families.bGraphicsSupportsCompute ) {
		return 81;
	}
	if ( sirVulkanSelectQueueFamilies( nullptr, 4, &families ) || sirVulkanSelectQueueFamilies( szFamilies, 65, &families ) ||
	     families.nGraphicsFamilyIndex != UINT32_MAX ) {
		return 82;
	}
	if ( sirVulkanSelectQueueFamilies( szFamilies, 4, nullptr ) ) {
		return 83;
	}
	szFamilies[0].queueCount = 0;
	szFamilies[3].queueCount = 0;
	if ( sirVulkanSelectQueueFamilies( szFamilies, 4, &families ) || families.nComputeFamilyIndex != UINT32_MAX ||
	     families.nTransferFamilyIndex != UINT32_MAX ) {
		return 84;
	}
	szFamilies[0].queueCount = 1;
	if ( !sirVulkanSelectQueueFamilies( szFamilies, 4, &families ) || families.nGraphicsFamilyIndex != 0 ||
	     families.nComputeFamilyIndex != 1 || families.nTransferFamilyIndex != 2 ) {
		return 85;
	}
	szFamilies[2].queueCount = 0;
	if ( !sirVulkanSelectQueueFamilies( szFamilies, 4, &families ) || families.nTransferFamilyIndex != 1 ) {
		return 86;
	}
	szFamilies[1].queueCount = 0;
	if ( !sirVulkanSelectQueueFamilies( szFamilies, 4, &families ) || families.nTransferFamilyIndex != 0 ) {
		return 87;
	}

	// Native queue plan requests each distinct family once, with graphics first.
	const float fQueuePriority = 1.0f;
	VkDeviceQueueCreateInfo szQueueInfos[3] = {};
	uint32_t nQueueCount = 99;
	const sirVulkanQueueFamilySelection_t distinct = { 3, 1, 2, true };
	if ( !sirVulkanBuildQueueCreateInfos( distinct, &fQueuePriority, szQueueInfos, &nQueueCount ) || nQueueCount != 3 ||
	     szQueueInfos[0].queueFamilyIndex != 3 || szQueueInfos[1].queueFamilyIndex != 1 || szQueueInfos[2].queueFamilyIndex != 2 ) {
		return 88;
	}
	for ( uint32_t iQueue = 0; iQueue < nQueueCount; ++iQueue ) {
		if ( szQueueInfos[iQueue].queueCount != 1 || szQueueInfos[iQueue].pQueuePriorities != &fQueuePriority ||
		     szQueueInfos[iQueue].sType != VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO ) {
			return 89;
		}
	}
	const sirVulkanQueueFamilySelection_t shared = { 3, 3, 3, true };
	if ( !sirVulkanBuildQueueCreateInfos( shared, &fQueuePriority, szQueueInfos, &nQueueCount ) || nQueueCount != 1 ||
	     szQueueInfos[0].queueFamilyIndex != 3 ) {
		return 90;
	}
	const sirVulkanQueueFamilySelection_t partial = { 3, UINT32_MAX, 1, false };
	if ( !sirVulkanBuildQueueCreateInfos( partial, &fQueuePriority, szQueueInfos, &nQueueCount ) || nQueueCount != 2 ||
	     szQueueInfos[1].queueFamilyIndex != 1 ) {
		return 91;
	}
	const sirVulkanQueueFamilySelection_t missingGraphics = { UINT32_MAX, 1, 2, false };
	if ( sirVulkanBuildQueueCreateInfos( missingGraphics, &fQueuePriority, szQueueInfos, &nQueueCount ) || nQueueCount != 0 ||
	     sirVulkanBuildQueueCreateInfos( distinct, nullptr, szQueueInfos, &nQueueCount ) || nQueueCount != 0 ||
	     sirVulkanBuildQueueCreateInfos( distinct, &fQueuePriority, szQueueInfos, nullptr ) ) {
		return 92;
	}

	// Physical-device priority is stable and independent of the Vulkan loader.
	if ( sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) != 4 ||
	     sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) != 3 ||
	     sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ) != 2 ||
	     sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_CPU ) != 1 || sirVulkanAdapterPriority( VK_PHYSICAL_DEVICE_TYPE_OTHER ) != 1 ) {
		return 14;
	}

	SIRVulkanDevice invalidProbe;
	uint32_t nCount = 19;
	if ( invalidProbe.probeAdapters( 0, nullptr, nullptr ) != SIR_VULKAN_INVALID_ARGUMENT ||
	     invalidProbe.probeAdapters( 32, &nCount, nullptr ) != SIR_VULKAN_INVALID_ARGUMENT || invalidProbe.isReady() ||
	     invalidProbe.getAdapterCount() != 0 ) {
		return 16;
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
	pDevice = nullptr;
	const sirGraphicsResult_t eGraphicsDevice = driver.createDevice( create, &pDevice );
	if ( eGraphicsDevice == SIR_GRAPHICS_SUCCESS ) {
		if ( pDevice == nullptr || driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS )
			return 6;
	} else if ( eGraphicsDevice != SIR_GRAPHICS_UNSUPPORTED || pDevice != nullptr ) {
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
