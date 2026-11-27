/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include "SIRVulkanShared.h"

bool SIR_Vulkan_LoadGlobalProcs( sirVulkanProcs_t& procs, PFN_vkGetInstanceProcAddr getProc ) noexcept
{
	if ( !getProc ) {
		return false;
	}

	procs = {};
	procs.vkGetInstanceProcAddr = getProc;

#define VULKAN_PROC( name ) procs.name = reinterpret_cast<PFN_##name>( getProc( VK_NULL_HANDLE, #name ) );

#if defined( VK_VERSION_1_0 )
	VULKAN_PROC( vkCreateInstance )
	VULKAN_PROC( vkEnumerateInstanceExtensionProperties )
	VULKAN_PROC( vkEnumerateInstanceLayerProperties )
#endif

#if defined( VK_VERSION_1_1 )
	VULKAN_PROC( vkEnumerateInstanceVersion )
#endif

#undef VULKAN_PROC

	return procs.vkCreateInstance != nullptr && procs.vkEnumerateInstanceExtensionProperties != nullptr && procs.vkEnumerateInstanceLayerProperties != nullptr;
}

bool SIR_Vulkan_LoadInstanceProcs( sirVulkanProcs_t& procs, VkInstance instance ) noexcept
{
	const PFN_vkGetInstanceProcAddr getProc = procs.vkGetInstanceProcAddr;

	if ( !getProc || instance == VK_NULL_HANDLE ) {
		return false;
	}

#define VULKAN_PROC( name ) procs.name = reinterpret_cast<PFN_##name>( getProc( instance, #name ) );

#if defined( VK_VERSION_1_0 )
	VULKAN_PROC( vkDestroyInstance )
	VULKAN_PROC( vkEnumeratePhysicalDevices )
	VULKAN_PROC( vkGetPhysicalDeviceFeatures )
	VULKAN_PROC( vkGetPhysicalDeviceFormatProperties )
	VULKAN_PROC( vkGetPhysicalDeviceImageFormatProperties )
	VULKAN_PROC( vkGetPhysicalDeviceProperties )
	VULKAN_PROC( vkGetPhysicalDeviceQueueFamilyProperties )
	VULKAN_PROC( vkGetPhysicalDeviceMemoryProperties )
	VULKAN_PROC( vkGetDeviceProcAddr )
	VULKAN_PROC( vkCreateDevice )
	VULKAN_PROC( vkEnumerateDeviceExtensionProperties )
	VULKAN_PROC( vkEnumerateDeviceLayerProperties )
	VULKAN_PROC( vkGetPhysicalDeviceSparseImageFormatProperties )
#endif

#if defined( VK_VERSION_1_1 )
	VULKAN_PROC( vkEnumeratePhysicalDeviceGroups )
	VULKAN_PROC( vkGetPhysicalDeviceFeatures2 )
	VULKAN_PROC( vkGetPhysicalDeviceProperties2 )
	VULKAN_PROC( vkGetPhysicalDeviceFormatProperties2 )
	VULKAN_PROC( vkGetPhysicalDeviceImageFormatProperties2 )
	VULKAN_PROC( vkGetPhysicalDeviceQueueFamilyProperties2 )
	VULKAN_PROC( vkGetPhysicalDeviceMemoryProperties2 )
	VULKAN_PROC( vkGetPhysicalDeviceSparseImageFormatProperties2 )
	VULKAN_PROC( vkGetPhysicalDeviceExternalBufferProperties )
	VULKAN_PROC( vkGetPhysicalDeviceExternalFenceProperties )
	VULKAN_PROC( vkGetPhysicalDeviceExternalSemaphoreProperties )
#endif

#if defined( VK_VERSION_1_3 )
	VULKAN_PROC( vkGetPhysicalDeviceToolProperties )
#endif

#if defined( VK_KHR_surface )
	VULKAN_PROC( vkDestroySurfaceKHR )
	VULKAN_PROC( vkGetPhysicalDeviceSurfaceSupportKHR )
	VULKAN_PROC( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
	VULKAN_PROC( vkGetPhysicalDeviceSurfaceFormatsKHR )
	VULKAN_PROC( vkGetPhysicalDeviceSurfacePresentModesKHR )
#endif

#if defined( VK_KHR_swapchain )
	VULKAN_PROC( vkGetPhysicalDevicePresentRectanglesKHR )
#endif

#if defined( VK_KHR_display )
	VULKAN_PROC( vkGetPhysicalDeviceDisplayPropertiesKHR )
	VULKAN_PROC( vkGetPhysicalDeviceDisplayPlanePropertiesKHR )
	VULKAN_PROC( vkGetDisplayPlaneSupportedDisplaysKHR )
	VULKAN_PROC( vkGetDisplayModePropertiesKHR )
	VULKAN_PROC( vkCreateDisplayModeKHR )
	VULKAN_PROC( vkGetDisplayPlaneCapabilitiesKHR )
	VULKAN_PROC( vkCreateDisplayPlaneSurfaceKHR )
#endif

#if defined( VK_KHR_video_queue )
	VULKAN_PROC( vkGetPhysicalDeviceVideoCapabilitiesKHR )
	VULKAN_PROC( vkGetPhysicalDeviceVideoFormatPropertiesKHR )
#endif

#if defined( VK_KHR_get_physical_device_properties2 )
	VULKAN_PROC( vkGetPhysicalDeviceFeatures2KHR )
	VULKAN_PROC( vkGetPhysicalDeviceProperties2KHR )
	VULKAN_PROC( vkGetPhysicalDeviceFormatProperties2KHR )
	VULKAN_PROC( vkGetPhysicalDeviceImageFormatProperties2KHR )
	VULKAN_PROC( vkGetPhysicalDeviceQueueFamilyProperties2KHR )
	VULKAN_PROC( vkGetPhysicalDeviceMemoryProperties2KHR )
	VULKAN_PROC( vkGetPhysicalDeviceSparseImageFormatProperties2KHR )
#endif

#if defined( VK_KHR_device_group_creation )
	VULKAN_PROC( vkEnumeratePhysicalDeviceGroupsKHR )
#endif

#if defined( VK_KHR_external_memory_capabilities )
	VULKAN_PROC( vkGetPhysicalDeviceExternalBufferPropertiesKHR )
#endif

#if defined( VK_KHR_external_semaphore_capabilities )
	VULKAN_PROC( vkGetPhysicalDeviceExternalSemaphorePropertiesKHR )
#endif

#if defined( VK_KHR_external_fence_capabilities )
	VULKAN_PROC( vkGetPhysicalDeviceExternalFencePropertiesKHR )
#endif

#if defined( VK_KHR_performance_query )
	VULKAN_PROC( vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR )
	VULKAN_PROC( vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR )
#endif

#if defined( VK_KHR_get_surface_capabilities2 )
	VULKAN_PROC( vkGetPhysicalDeviceSurfaceCapabilities2KHR )
	VULKAN_PROC( vkGetPhysicalDeviceSurfaceFormats2KHR )
#endif

#if defined( VK_KHR_get_display_properties2 )
	VULKAN_PROC( vkGetPhysicalDeviceDisplayProperties2KHR )
	VULKAN_PROC( vkGetPhysicalDeviceDisplayPlaneProperties2KHR )
	VULKAN_PROC( vkGetDisplayModeProperties2KHR )
	VULKAN_PROC( vkGetDisplayPlaneCapabilities2KHR )
#endif

#if defined( VK_KHR_fragment_shading_rate )
	VULKAN_PROC( vkGetPhysicalDeviceFragmentShadingRatesKHR )
#endif

#if defined( VK_KHR_video_encode_queue )
	VULKAN_PROC( vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR )
#endif

#if defined( VK_KHR_cooperative_matrix )
	VULKAN_PROC( vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR )
#endif

#if defined( VK_KHR_calibrated_timestamps )
	VULKAN_PROC( vkGetPhysicalDeviceCalibrateableTimeDomainsKHR )
#endif

#if defined( VK_EXT_debug_report )
	VULKAN_PROC( vkCreateDebugReportCallbackEXT )
	VULKAN_PROC( vkDestroyDebugReportCallbackEXT )
	VULKAN_PROC( vkDebugReportMessageEXT )
#endif

#if defined( VK_NV_external_memory_capabilities )
	VULKAN_PROC( vkGetPhysicalDeviceExternalImageFormatPropertiesNV )
#endif

#if defined( VK_EXT_direct_mode_display )
	VULKAN_PROC( vkReleaseDisplayEXT )
#endif

#if defined( VK_EXT_display_surface_counter )
	VULKAN_PROC( vkGetPhysicalDeviceSurfaceCapabilities2EXT )
#endif

#if defined( VK_EXT_debug_utils )
	VULKAN_PROC( vkCreateDebugUtilsMessengerEXT )
	VULKAN_PROC( vkDestroyDebugUtilsMessengerEXT )
	VULKAN_PROC( vkSubmitDebugUtilsMessageEXT )
#endif

#if defined( VK_EXT_descriptor_heap )
	VULKAN_PROC( vkGetPhysicalDeviceDescriptorSizeEXT )
#endif

#if defined( VK_EXT_sample_locations )
	VULKAN_PROC( vkGetPhysicalDeviceMultisamplePropertiesEXT )
#endif

#if defined( VK_EXT_calibrated_timestamps )
	VULKAN_PROC( vkGetPhysicalDeviceCalibrateableTimeDomainsEXT )
#endif

#if defined( VK_EXT_tooling_info )
	VULKAN_PROC( vkGetPhysicalDeviceToolPropertiesEXT )
#endif

#if defined( VK_NV_cooperative_matrix )
	VULKAN_PROC( vkGetPhysicalDeviceCooperativeMatrixPropertiesNV )
#endif

#if defined( VK_NV_coverage_reduction_mode )
	VULKAN_PROC( vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV )
#endif

#if defined( VK_EXT_headless_surface )
	VULKAN_PROC( vkCreateHeadlessSurfaceEXT )
#endif

#if defined( VK_EXT_acquire_drm_display )
	VULKAN_PROC( vkAcquireDrmDisplayEXT )
	VULKAN_PROC( vkGetDrmDisplayEXT )
#endif

#if defined( VK_ARM_tensors )
	VULKAN_PROC( vkGetPhysicalDeviceExternalTensorPropertiesARM )
#endif

#if defined( VK_NV_optical_flow )
	VULKAN_PROC( vkGetPhysicalDeviceOpticalFlowImageFormatsNV )
#endif

#if defined( VK_NV_cooperative_vector )
	VULKAN_PROC( vkGetPhysicalDeviceCooperativeVectorPropertiesNV )
#endif

#if defined( VK_ARM_data_graph )
	VULKAN_PROC( vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM )
	VULKAN_PROC( vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM )
#endif

#if defined( VK_ARM_data_graph_instruction_set_tosa )
	VULKAN_PROC( vkGetPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM )
#endif

#if defined( VK_NV_cooperative_matrix2 )
	VULKAN_PROC( vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV )
#endif

#if defined( VK_ARM_performance_counters_by_region )
	VULKAN_PROC( vkEnumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM )
#endif

#if defined( VK_ARM_shader_instrumentation )
	VULKAN_PROC( vkEnumeratePhysicalDeviceShaderInstrumentationMetricsARM )
#endif

#if defined( VK_ARM_data_graph_optical_flow )
	VULKAN_PROC( vkGetPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM )
#endif

#if defined( VK_EXT_cooperative_matrix_maintenance1 )
	VULKAN_PROC( vkGetPhysicalDeviceCooperativeMatrixProperties2EXT )
#endif

#if defined( VK_KHR_android_surface )
	VULKAN_PROC( vkCreateAndroidSurfaceKHR )
#endif

#if defined( VK_FUCHSIA_imagepipe_surface )
	VULKAN_PROC( vkCreateImagePipeSurfaceFUCHSIA )
#endif

#if defined( VK_MVK_ios_surface )
	VULKAN_PROC( vkCreateIOSSurfaceMVK )
#endif

#if defined( VK_MVK_macos_surface )
	VULKAN_PROC( vkCreateMacOSSurfaceMVK )
#endif

#if defined( VK_EXT_metal_surface )
	VULKAN_PROC( vkCreateMetalSurfaceEXT )
#endif

#if defined( VK_NN_vi_surface )
	VULKAN_PROC( vkCreateViSurfaceNN )
#endif

#if defined( VK_KHR_wayland_surface )
	VULKAN_PROC( vkCreateWaylandSurfaceKHR )
	VULKAN_PROC( vkGetPhysicalDeviceWaylandPresentationSupportKHR )
#endif

#if defined( VK_KHR_win32_surface )
	VULKAN_PROC( vkCreateWin32SurfaceKHR )
	VULKAN_PROC( vkGetPhysicalDeviceWin32PresentationSupportKHR )
#endif

#if defined( VK_EXT_full_screen_exclusive )
	VULKAN_PROC( vkGetPhysicalDeviceSurfacePresentModes2EXT )
#endif

#if defined( VK_NV_acquire_winrt_display )
	VULKAN_PROC( vkAcquireWinrtDisplayNV )
	VULKAN_PROC( vkGetWinrtDisplayNV )
#endif

#if defined( VK_KHR_xcb_surface )
	VULKAN_PROC( vkCreateXcbSurfaceKHR )
	VULKAN_PROC( vkGetPhysicalDeviceXcbPresentationSupportKHR )
#endif

#if defined( VK_KHR_xlib_surface )
	VULKAN_PROC( vkCreateXlibSurfaceKHR )
	VULKAN_PROC( vkGetPhysicalDeviceXlibPresentationSupportKHR )
#endif

#if defined( VK_EXT_directfb_surface )
	VULKAN_PROC( vkCreateDirectFBSurfaceEXT )
	VULKAN_PROC( vkGetPhysicalDeviceDirectFBPresentationSupportEXT )
#endif

#if defined( VK_EXT_acquire_xlib_display )
	VULKAN_PROC( vkAcquireXlibDisplayEXT )
	VULKAN_PROC( vkGetRandROutputDisplayEXT )
#endif

#if defined( VK_GGP_stream_descriptor_surface )
	VULKAN_PROC( vkCreateStreamDescriptorSurfaceGGP )
#endif

#if defined( VK_QNX_screen_surface )
	VULKAN_PROC( vkCreateScreenSurfaceQNX )
	VULKAN_PROC( vkGetPhysicalDeviceScreenPresentationSupportQNX )
#endif

#if defined( VK_OHOS_surface )
	VULKAN_PROC( vkCreateSurfaceOHOS )
#endif

#if defined( VK_SEC_ubm_surface )
	VULKAN_PROC( vkCreateUbmSurfaceSEC )
	VULKAN_PROC( vkGetPhysicalDeviceUbmPresentationSupportSEC )
#endif

#undef VULKAN_PROC

	return procs.vkDestroyInstance != nullptr && procs.vkEnumeratePhysicalDevices != nullptr && procs.vkGetPhysicalDeviceFeatures != nullptr &&
	       procs.vkGetPhysicalDeviceFormatProperties != nullptr && procs.vkGetPhysicalDeviceImageFormatProperties != nullptr && procs.vkGetPhysicalDeviceProperties != nullptr &&
	       procs.vkGetPhysicalDeviceQueueFamilyProperties != nullptr && procs.vkGetPhysicalDeviceMemoryProperties != nullptr && procs.vkGetDeviceProcAddr != nullptr &&
	       procs.vkCreateDevice != nullptr && procs.vkEnumerateDeviceExtensionProperties != nullptr && procs.vkEnumerateDeviceLayerProperties != nullptr &&
	       procs.vkGetPhysicalDeviceSparseImageFormatProperties != nullptr;
}

bool SIR_Vulkan_LoadDeviceProcs( sirVulkanProcs_t& procs, VkDevice device ) noexcept
{
	const PFN_vkGetDeviceProcAddr getProc = procs.vkGetDeviceProcAddr;

	if ( !getProc || device == VK_NULL_HANDLE ) {
		return false;
	}

#define VULKAN_PROC( name ) procs.name = reinterpret_cast<PFN_##name>( getProc( device, #name ) );

#if defined( VK_VERSION_1_0 )
	VULKAN_PROC( vkDestroyDevice )
	VULKAN_PROC( vkGetDeviceQueue )
	VULKAN_PROC( vkQueueSubmit )
	VULKAN_PROC( vkQueueWaitIdle )
	VULKAN_PROC( vkDeviceWaitIdle )
	VULKAN_PROC( vkAllocateMemory )
	VULKAN_PROC( vkFreeMemory )
	VULKAN_PROC( vkMapMemory )
	VULKAN_PROC( vkUnmapMemory )
	VULKAN_PROC( vkFlushMappedMemoryRanges )
	VULKAN_PROC( vkInvalidateMappedMemoryRanges )
	VULKAN_PROC( vkGetDeviceMemoryCommitment )
	VULKAN_PROC( vkBindBufferMemory )
	VULKAN_PROC( vkBindImageMemory )
	VULKAN_PROC( vkGetBufferMemoryRequirements )
	VULKAN_PROC( vkGetImageMemoryRequirements )
	VULKAN_PROC( vkGetImageSparseMemoryRequirements )
	VULKAN_PROC( vkQueueBindSparse )
	VULKAN_PROC( vkCreateFence )
	VULKAN_PROC( vkDestroyFence )
	VULKAN_PROC( vkResetFences )
	VULKAN_PROC( vkGetFenceStatus )
	VULKAN_PROC( vkWaitForFences )
	VULKAN_PROC( vkCreateSemaphore )
	VULKAN_PROC( vkDestroySemaphore )
	VULKAN_PROC( vkCreateQueryPool )
	VULKAN_PROC( vkDestroyQueryPool )
	VULKAN_PROC( vkGetQueryPoolResults )
	VULKAN_PROC( vkCreateBuffer )
	VULKAN_PROC( vkDestroyBuffer )
	VULKAN_PROC( vkCreateImage )
	VULKAN_PROC( vkDestroyImage )
	VULKAN_PROC( vkGetImageSubresourceLayout )
	VULKAN_PROC( vkCreateImageView )
	VULKAN_PROC( vkDestroyImageView )
	VULKAN_PROC( vkCreateCommandPool )
	VULKAN_PROC( vkDestroyCommandPool )
	VULKAN_PROC( vkResetCommandPool )
	VULKAN_PROC( vkAllocateCommandBuffers )
	VULKAN_PROC( vkFreeCommandBuffers )
	VULKAN_PROC( vkBeginCommandBuffer )
	VULKAN_PROC( vkEndCommandBuffer )
	VULKAN_PROC( vkResetCommandBuffer )
	VULKAN_PROC( vkCmdCopyBuffer )
	VULKAN_PROC( vkCmdCopyImage )
	VULKAN_PROC( vkCmdCopyBufferToImage )
	VULKAN_PROC( vkCmdCopyImageToBuffer )
	VULKAN_PROC( vkCmdUpdateBuffer )
	VULKAN_PROC( vkCmdFillBuffer )
	VULKAN_PROC( vkCmdPipelineBarrier )
	VULKAN_PROC( vkCmdBeginQuery )
	VULKAN_PROC( vkCmdEndQuery )
	VULKAN_PROC( vkCmdResetQueryPool )
	VULKAN_PROC( vkCmdWriteTimestamp )
	VULKAN_PROC( vkCmdCopyQueryPoolResults )
	VULKAN_PROC( vkCmdExecuteCommands )
	VULKAN_PROC( vkCreateEvent )
	VULKAN_PROC( vkDestroyEvent )
	VULKAN_PROC( vkGetEventStatus )
	VULKAN_PROC( vkSetEvent )
	VULKAN_PROC( vkResetEvent )
	VULKAN_PROC( vkCreateBufferView )
	VULKAN_PROC( vkDestroyBufferView )
	VULKAN_PROC( vkCreateShaderModule )
	VULKAN_PROC( vkDestroyShaderModule )
	VULKAN_PROC( vkCreatePipelineCache )
	VULKAN_PROC( vkDestroyPipelineCache )
	VULKAN_PROC( vkGetPipelineCacheData )
	VULKAN_PROC( vkMergePipelineCaches )
	VULKAN_PROC( vkCreateComputePipelines )
	VULKAN_PROC( vkDestroyPipeline )
	VULKAN_PROC( vkCreatePipelineLayout )
	VULKAN_PROC( vkDestroyPipelineLayout )
	VULKAN_PROC( vkCreateSampler )
	VULKAN_PROC( vkDestroySampler )
	VULKAN_PROC( vkCreateDescriptorSetLayout )
	VULKAN_PROC( vkDestroyDescriptorSetLayout )
	VULKAN_PROC( vkCreateDescriptorPool )
	VULKAN_PROC( vkDestroyDescriptorPool )
	VULKAN_PROC( vkResetDescriptorPool )
	VULKAN_PROC( vkAllocateDescriptorSets )
	VULKAN_PROC( vkFreeDescriptorSets )
	VULKAN_PROC( vkUpdateDescriptorSets )
	VULKAN_PROC( vkCmdBindPipeline )
	VULKAN_PROC( vkCmdBindDescriptorSets )
	VULKAN_PROC( vkCmdClearColorImage )
	VULKAN_PROC( vkCmdDispatch )
	VULKAN_PROC( vkCmdDispatchIndirect )
	VULKAN_PROC( vkCmdSetEvent )
	VULKAN_PROC( vkCmdResetEvent )
	VULKAN_PROC( vkCmdWaitEvents )
	VULKAN_PROC( vkCmdPushConstants )
	VULKAN_PROC( vkCreateGraphicsPipelines )
	VULKAN_PROC( vkCreateFramebuffer )
	VULKAN_PROC( vkDestroyFramebuffer )
	VULKAN_PROC( vkCreateRenderPass )
	VULKAN_PROC( vkDestroyRenderPass )
	VULKAN_PROC( vkGetRenderAreaGranularity )
	VULKAN_PROC( vkCmdSetViewport )
	VULKAN_PROC( vkCmdSetScissor )
	VULKAN_PROC( vkCmdSetLineWidth )
	VULKAN_PROC( vkCmdSetDepthBias )
	VULKAN_PROC( vkCmdSetBlendConstants )
	VULKAN_PROC( vkCmdSetDepthBounds )
	VULKAN_PROC( vkCmdSetStencilCompareMask )
	VULKAN_PROC( vkCmdSetStencilWriteMask )
	VULKAN_PROC( vkCmdSetStencilReference )
	VULKAN_PROC( vkCmdBindIndexBuffer )
	VULKAN_PROC( vkCmdBindVertexBuffers )
	VULKAN_PROC( vkCmdDraw )
	VULKAN_PROC( vkCmdDrawIndexed )
	VULKAN_PROC( vkCmdDrawIndirect )
	VULKAN_PROC( vkCmdDrawIndexedIndirect )
	VULKAN_PROC( vkCmdBlitImage )
	VULKAN_PROC( vkCmdClearDepthStencilImage )
	VULKAN_PROC( vkCmdClearAttachments )
	VULKAN_PROC( vkCmdResolveImage )
	VULKAN_PROC( vkCmdBeginRenderPass )
	VULKAN_PROC( vkCmdNextSubpass )
	VULKAN_PROC( vkCmdEndRenderPass )
#endif

#if defined( VK_VERSION_1_1 )
	VULKAN_PROC( vkBindBufferMemory2 )
	VULKAN_PROC( vkBindImageMemory2 )
	VULKAN_PROC( vkGetDeviceGroupPeerMemoryFeatures )
	VULKAN_PROC( vkCmdSetDeviceMask )
	VULKAN_PROC( vkGetImageMemoryRequirements2 )
	VULKAN_PROC( vkGetBufferMemoryRequirements2 )
	VULKAN_PROC( vkGetImageSparseMemoryRequirements2 )
	VULKAN_PROC( vkTrimCommandPool )
	VULKAN_PROC( vkGetDeviceQueue2 )
	VULKAN_PROC( vkCmdDispatchBase )
	VULKAN_PROC( vkCreateDescriptorUpdateTemplate )
	VULKAN_PROC( vkDestroyDescriptorUpdateTemplate )
	VULKAN_PROC( vkUpdateDescriptorSetWithTemplate )
	VULKAN_PROC( vkGetDescriptorSetLayoutSupport )
	VULKAN_PROC( vkCreateSamplerYcbcrConversion )
	VULKAN_PROC( vkDestroySamplerYcbcrConversion )
#endif

#if defined( VK_VERSION_1_2 )
	VULKAN_PROC( vkResetQueryPool )
	VULKAN_PROC( vkGetSemaphoreCounterValue )
	VULKAN_PROC( vkWaitSemaphores )
	VULKAN_PROC( vkSignalSemaphore )
	VULKAN_PROC( vkGetBufferDeviceAddress )
	VULKAN_PROC( vkGetBufferOpaqueCaptureAddress )
	VULKAN_PROC( vkGetDeviceMemoryOpaqueCaptureAddress )
	VULKAN_PROC( vkCmdDrawIndirectCount )
	VULKAN_PROC( vkCmdDrawIndexedIndirectCount )
	VULKAN_PROC( vkCreateRenderPass2 )
	VULKAN_PROC( vkCmdBeginRenderPass2 )
	VULKAN_PROC( vkCmdNextSubpass2 )
	VULKAN_PROC( vkCmdEndRenderPass2 )
#endif

#if defined( VK_VERSION_1_3 )
	VULKAN_PROC( vkCreatePrivateDataSlot )
	VULKAN_PROC( vkDestroyPrivateDataSlot )
	VULKAN_PROC( vkSetPrivateData )
	VULKAN_PROC( vkGetPrivateData )
	VULKAN_PROC( vkCmdPipelineBarrier2 )
	VULKAN_PROC( vkCmdWriteTimestamp2 )
	VULKAN_PROC( vkQueueSubmit2 )
	VULKAN_PROC( vkCmdCopyBuffer2 )
	VULKAN_PROC( vkCmdCopyImage2 )
	VULKAN_PROC( vkCmdCopyBufferToImage2 )
	VULKAN_PROC( vkCmdCopyImageToBuffer2 )
	VULKAN_PROC( vkGetDeviceBufferMemoryRequirements )
	VULKAN_PROC( vkGetDeviceImageMemoryRequirements )
	VULKAN_PROC( vkGetDeviceImageSparseMemoryRequirements )
	VULKAN_PROC( vkCmdSetEvent2 )
	VULKAN_PROC( vkCmdResetEvent2 )
	VULKAN_PROC( vkCmdWaitEvents2 )
	VULKAN_PROC( vkCmdBlitImage2 )
	VULKAN_PROC( vkCmdResolveImage2 )
	VULKAN_PROC( vkCmdBeginRendering )
	VULKAN_PROC( vkCmdEndRendering )
	VULKAN_PROC( vkCmdSetCullMode )
	VULKAN_PROC( vkCmdSetFrontFace )
	VULKAN_PROC( vkCmdSetPrimitiveTopology )
	VULKAN_PROC( vkCmdSetViewportWithCount )
	VULKAN_PROC( vkCmdSetScissorWithCount )
	VULKAN_PROC( vkCmdBindVertexBuffers2 )
	VULKAN_PROC( vkCmdSetDepthTestEnable )
	VULKAN_PROC( vkCmdSetDepthWriteEnable )
	VULKAN_PROC( vkCmdSetDepthCompareOp )
	VULKAN_PROC( vkCmdSetDepthBoundsTestEnable )
	VULKAN_PROC( vkCmdSetStencilTestEnable )
	VULKAN_PROC( vkCmdSetStencilOp )
	VULKAN_PROC( vkCmdSetRasterizerDiscardEnable )
	VULKAN_PROC( vkCmdSetDepthBiasEnable )
	VULKAN_PROC( vkCmdSetPrimitiveRestartEnable )
#endif

#if defined( VK_VERSION_1_4 )
	VULKAN_PROC( vkMapMemory2 )
	VULKAN_PROC( vkUnmapMemory2 )
	VULKAN_PROC( vkGetDeviceImageSubresourceLayout )
	VULKAN_PROC( vkGetImageSubresourceLayout2 )
	VULKAN_PROC( vkCopyMemoryToImage )
	VULKAN_PROC( vkCopyImageToMemory )
	VULKAN_PROC( vkCopyImageToImage )
	VULKAN_PROC( vkTransitionImageLayout )
	VULKAN_PROC( vkCmdPushDescriptorSet )
	VULKAN_PROC( vkCmdPushDescriptorSetWithTemplate )
	VULKAN_PROC( vkCmdBindDescriptorSets2 )
	VULKAN_PROC( vkCmdPushConstants2 )
	VULKAN_PROC( vkCmdPushDescriptorSet2 )
	VULKAN_PROC( vkCmdPushDescriptorSetWithTemplate2 )
	VULKAN_PROC( vkCmdSetLineStipple )
	VULKAN_PROC( vkCmdBindIndexBuffer2 )
	VULKAN_PROC( vkGetRenderingAreaGranularity )
	VULKAN_PROC( vkCmdSetRenderingAttachmentLocations )
	VULKAN_PROC( vkCmdSetRenderingInputAttachmentIndices )
#endif

#if defined( VK_KHR_swapchain )
	VULKAN_PROC( vkCreateSwapchainKHR )
	VULKAN_PROC( vkDestroySwapchainKHR )
	VULKAN_PROC( vkGetSwapchainImagesKHR )
	VULKAN_PROC( vkAcquireNextImageKHR )
	VULKAN_PROC( vkQueuePresentKHR )
	VULKAN_PROC( vkGetDeviceGroupPresentCapabilitiesKHR )
	VULKAN_PROC( vkGetDeviceGroupSurfacePresentModesKHR )
	VULKAN_PROC( vkAcquireNextImage2KHR )
#endif

#if defined( VK_KHR_display_swapchain )
	VULKAN_PROC( vkCreateSharedSwapchainsKHR )
#endif

#if defined( VK_KHR_video_queue )
	VULKAN_PROC( vkCreateVideoSessionKHR )
	VULKAN_PROC( vkDestroyVideoSessionKHR )
	VULKAN_PROC( vkGetVideoSessionMemoryRequirementsKHR )
	VULKAN_PROC( vkBindVideoSessionMemoryKHR )
	VULKAN_PROC( vkCreateVideoSessionParametersKHR )
	VULKAN_PROC( vkUpdateVideoSessionParametersKHR )
	VULKAN_PROC( vkDestroyVideoSessionParametersKHR )
	VULKAN_PROC( vkCmdBeginVideoCodingKHR )
	VULKAN_PROC( vkCmdEndVideoCodingKHR )
	VULKAN_PROC( vkCmdControlVideoCodingKHR )
#endif

#if defined( VK_KHR_video_decode_queue )
	VULKAN_PROC( vkCmdDecodeVideoKHR )
#endif

#if defined( VK_KHR_dynamic_rendering )
	VULKAN_PROC( vkCmdBeginRenderingKHR )
	VULKAN_PROC( vkCmdEndRenderingKHR )
#endif

#if defined( VK_KHR_device_group )
	VULKAN_PROC( vkGetDeviceGroupPeerMemoryFeaturesKHR )
	VULKAN_PROC( vkCmdSetDeviceMaskKHR )
	VULKAN_PROC( vkCmdDispatchBaseKHR )
#endif

#if defined( VK_KHR_maintenance1 )
	VULKAN_PROC( vkTrimCommandPoolKHR )
#endif

#if defined( VK_KHR_external_memory_fd )
	VULKAN_PROC( vkGetMemoryFdKHR )
	VULKAN_PROC( vkGetMemoryFdPropertiesKHR )
#endif

#if defined( VK_KHR_external_semaphore_fd )
	VULKAN_PROC( vkImportSemaphoreFdKHR )
	VULKAN_PROC( vkGetSemaphoreFdKHR )
#endif

#if defined( VK_KHR_push_descriptor )
	VULKAN_PROC( vkCmdPushDescriptorSetKHR )
	VULKAN_PROC( vkCmdPushDescriptorSetWithTemplateKHR )
#endif

#if defined( VK_KHR_descriptor_update_template )
	VULKAN_PROC( vkCreateDescriptorUpdateTemplateKHR )
	VULKAN_PROC( vkDestroyDescriptorUpdateTemplateKHR )
	VULKAN_PROC( vkUpdateDescriptorSetWithTemplateKHR )
#endif

#if defined( VK_KHR_create_renderpass2 )
	VULKAN_PROC( vkCreateRenderPass2KHR )
	VULKAN_PROC( vkCmdBeginRenderPass2KHR )
	VULKAN_PROC( vkCmdNextSubpass2KHR )
	VULKAN_PROC( vkCmdEndRenderPass2KHR )
#endif

#if defined( VK_KHR_shared_presentable_image )
	VULKAN_PROC( vkGetSwapchainStatusKHR )
#endif

#if defined( VK_KHR_external_fence_fd )
	VULKAN_PROC( vkImportFenceFdKHR )
	VULKAN_PROC( vkGetFenceFdKHR )
#endif

#if defined( VK_KHR_performance_query )
	VULKAN_PROC( vkAcquireProfilingLockKHR )
	VULKAN_PROC( vkReleaseProfilingLockKHR )
#endif

#if defined( VK_KHR_get_memory_requirements2 )
	VULKAN_PROC( vkGetImageMemoryRequirements2KHR )
	VULKAN_PROC( vkGetBufferMemoryRequirements2KHR )
	VULKAN_PROC( vkGetImageSparseMemoryRequirements2KHR )
#endif

#if defined( VK_KHR_sampler_ycbcr_conversion )
	VULKAN_PROC( vkCreateSamplerYcbcrConversionKHR )
	VULKAN_PROC( vkDestroySamplerYcbcrConversionKHR )
#endif

#if defined( VK_KHR_bind_memory2 )
	VULKAN_PROC( vkBindBufferMemory2KHR )
	VULKAN_PROC( vkBindImageMemory2KHR )
#endif

#if defined( VK_KHR_maintenance3 )
	VULKAN_PROC( vkGetDescriptorSetLayoutSupportKHR )
#endif

#if defined( VK_KHR_draw_indirect_count )
	VULKAN_PROC( vkCmdDrawIndirectCountKHR )
	VULKAN_PROC( vkCmdDrawIndexedIndirectCountKHR )
#endif

#if defined( VK_KHR_timeline_semaphore )
	VULKAN_PROC( vkGetSemaphoreCounterValueKHR )
	VULKAN_PROC( vkWaitSemaphoresKHR )
	VULKAN_PROC( vkSignalSemaphoreKHR )
#endif

#if defined( VK_KHR_fragment_shading_rate )
	VULKAN_PROC( vkCmdSetFragmentShadingRateKHR )
#endif

#if defined( VK_KHR_dynamic_rendering_local_read )
	VULKAN_PROC( vkCmdSetRenderingAttachmentLocationsKHR )
	VULKAN_PROC( vkCmdSetRenderingInputAttachmentIndicesKHR )
#endif

#if defined( VK_KHR_present_wait )
	VULKAN_PROC( vkWaitForPresentKHR )
#endif

#if defined( VK_KHR_buffer_device_address )
	VULKAN_PROC( vkGetBufferDeviceAddressKHR )
	VULKAN_PROC( vkGetBufferOpaqueCaptureAddressKHR )
	VULKAN_PROC( vkGetDeviceMemoryOpaqueCaptureAddressKHR )
#endif

#if defined( VK_KHR_deferred_host_operations )
	VULKAN_PROC( vkCreateDeferredOperationKHR )
	VULKAN_PROC( vkDestroyDeferredOperationKHR )
	VULKAN_PROC( vkGetDeferredOperationMaxConcurrencyKHR )
	VULKAN_PROC( vkGetDeferredOperationResultKHR )
	VULKAN_PROC( vkDeferredOperationJoinKHR )
#endif

#if defined( VK_KHR_pipeline_executable_properties )
	VULKAN_PROC( vkGetPipelineExecutablePropertiesKHR )
	VULKAN_PROC( vkGetPipelineExecutableStatisticsKHR )
	VULKAN_PROC( vkGetPipelineExecutableInternalRepresentationsKHR )
#endif

#if defined( VK_KHR_map_memory2 )
	VULKAN_PROC( vkMapMemory2KHR )
	VULKAN_PROC( vkUnmapMemory2KHR )
#endif

#if defined( VK_KHR_video_encode_queue )
	VULKAN_PROC( vkGetEncodedVideoSessionParametersKHR )
	VULKAN_PROC( vkCmdEncodeVideoKHR )
#endif

#if defined( VK_KHR_synchronization2 )
	VULKAN_PROC( vkCmdSetEvent2KHR )
	VULKAN_PROC( vkCmdResetEvent2KHR )
	VULKAN_PROC( vkCmdWaitEvents2KHR )
	VULKAN_PROC( vkCmdPipelineBarrier2KHR )
	VULKAN_PROC( vkCmdWriteTimestamp2KHR )
	VULKAN_PROC( vkQueueSubmit2KHR )
#endif

#if defined( VK_KHR_device_address_commands )
	VULKAN_PROC( vkCmdBindIndexBuffer3KHR )
	VULKAN_PROC( vkCmdBindVertexBuffers3KHR )
	VULKAN_PROC( vkCmdDrawIndirect2KHR )
	VULKAN_PROC( vkCmdDrawIndexedIndirect2KHR )
	VULKAN_PROC( vkCmdDispatchIndirect2KHR )
	VULKAN_PROC( vkCmdCopyMemoryKHR )
	VULKAN_PROC( vkCmdCopyMemoryToImageKHR )
	VULKAN_PROC( vkCmdCopyImageToMemoryKHR )
	VULKAN_PROC( vkCmdUpdateMemoryKHR )
	VULKAN_PROC( vkCmdFillMemoryKHR )
	VULKAN_PROC( vkCmdCopyQueryPoolResultsToMemoryKHR )
	VULKAN_PROC( vkCmdDrawIndirectCount2KHR )
	VULKAN_PROC( vkCmdDrawIndexedIndirectCount2KHR )
	VULKAN_PROC( vkCmdBeginConditionalRendering2EXT )
	VULKAN_PROC( vkCmdBindTransformFeedbackBuffers2EXT )
	VULKAN_PROC( vkCmdBeginTransformFeedback2EXT )
	VULKAN_PROC( vkCmdEndTransformFeedback2EXT )
	VULKAN_PROC( vkCmdDrawIndirectByteCount2EXT )
	VULKAN_PROC( vkCmdDrawMeshTasksIndirect2EXT )
	VULKAN_PROC( vkCmdDrawMeshTasksIndirectCount2EXT )
	VULKAN_PROC( vkCmdWriteMarkerToMemoryAMD )
	VULKAN_PROC( vkCreateAccelerationStructure2KHR )
#endif

#if defined( VK_KHR_copy_commands2 )
	VULKAN_PROC( vkCmdCopyBuffer2KHR )
	VULKAN_PROC( vkCmdCopyImage2KHR )
	VULKAN_PROC( vkCmdCopyBufferToImage2KHR )
	VULKAN_PROC( vkCmdCopyImageToBuffer2KHR )
	VULKAN_PROC( vkCmdBlitImage2KHR )
	VULKAN_PROC( vkCmdResolveImage2KHR )
#endif

#if defined( VK_KHR_ray_tracing_maintenance1 )
	VULKAN_PROC( vkCmdTraceRaysIndirect2KHR )
#endif

#if defined( VK_KHR_maintenance4 )
	VULKAN_PROC( vkGetDeviceBufferMemoryRequirementsKHR )
	VULKAN_PROC( vkGetDeviceImageMemoryRequirementsKHR )
	VULKAN_PROC( vkGetDeviceImageSparseMemoryRequirementsKHR )
#endif

#if defined( VK_KHR_maintenance5 )
	VULKAN_PROC( vkCmdBindIndexBuffer2KHR )
	VULKAN_PROC( vkGetRenderingAreaGranularityKHR )
	VULKAN_PROC( vkGetDeviceImageSubresourceLayoutKHR )
	VULKAN_PROC( vkGetImageSubresourceLayout2KHR )
#endif

#if defined( VK_KHR_present_wait2 )
	VULKAN_PROC( vkWaitForPresent2KHR )
#endif

#if defined( VK_KHR_pipeline_binary )
	VULKAN_PROC( vkCreatePipelineBinariesKHR )
	VULKAN_PROC( vkDestroyPipelineBinaryKHR )
	VULKAN_PROC( vkGetPipelineKeyKHR )
	VULKAN_PROC( vkGetPipelineBinaryDataKHR )
	VULKAN_PROC( vkReleaseCapturedPipelineDataKHR )
#endif

#if defined( VK_KHR_swapchain_maintenance1 )
	VULKAN_PROC( vkReleaseSwapchainImagesKHR )
#endif

#if defined( VK_KHR_line_rasterization )
	VULKAN_PROC( vkCmdSetLineStippleKHR )
#endif

#if defined( VK_KHR_calibrated_timestamps )
	VULKAN_PROC( vkGetCalibratedTimestampsKHR )
#endif

#if defined( VK_KHR_maintenance6 )
	VULKAN_PROC( vkCmdBindDescriptorSets2KHR )
	VULKAN_PROC( vkCmdPushConstants2KHR )
	VULKAN_PROC( vkCmdPushDescriptorSet2KHR )
	VULKAN_PROC( vkCmdPushDescriptorSetWithTemplate2KHR )
	VULKAN_PROC( vkCmdSetDescriptorBufferOffsets2EXT )
	VULKAN_PROC( vkCmdBindDescriptorBufferEmbeddedSamplers2EXT )
#endif

#if defined( VK_KHR_copy_memory_indirect )
	VULKAN_PROC( vkCmdCopyMemoryIndirectKHR )
	VULKAN_PROC( vkCmdCopyMemoryToImageIndirectKHR )
#endif

#if defined( VK_KHR_device_fault )
	VULKAN_PROC( vkGetDeviceFaultReportsKHR )
	VULKAN_PROC( vkGetDeviceFaultDebugInfoKHR )
#endif

#if defined( VK_KHR_maintenance10 )
	VULKAN_PROC( vkCmdEndRendering2KHR )
#endif

#if defined( VK_EXT_debug_marker )
	VULKAN_PROC( vkDebugMarkerSetObjectTagEXT )
	VULKAN_PROC( vkDebugMarkerSetObjectNameEXT )
	VULKAN_PROC( vkCmdDebugMarkerBeginEXT )
	VULKAN_PROC( vkCmdDebugMarkerEndEXT )
	VULKAN_PROC( vkCmdDebugMarkerInsertEXT )
#endif

#if defined( VK_EXT_transform_feedback )
	VULKAN_PROC( vkCmdBindTransformFeedbackBuffersEXT )
	VULKAN_PROC( vkCmdBeginTransformFeedbackEXT )
	VULKAN_PROC( vkCmdEndTransformFeedbackEXT )
	VULKAN_PROC( vkCmdBeginQueryIndexedEXT )
	VULKAN_PROC( vkCmdEndQueryIndexedEXT )
	VULKAN_PROC( vkCmdDrawIndirectByteCountEXT )
#endif

#if defined( VK_NVX_binary_import )
	VULKAN_PROC( vkCreateCuModuleNVX )
	VULKAN_PROC( vkCreateCuFunctionNVX )
	VULKAN_PROC( vkDestroyCuModuleNVX )
	VULKAN_PROC( vkDestroyCuFunctionNVX )
	VULKAN_PROC( vkCmdCuLaunchKernelNVX )
#endif

#if defined( VK_NVX_image_view_handle )
	VULKAN_PROC( vkGetImageViewHandleNVX )
	VULKAN_PROC( vkGetImageViewHandle64NVX )
	VULKAN_PROC( vkGetImageViewAddressNVX )
	VULKAN_PROC( vkGetDeviceCombinedImageSamplerIndexNVX )
#endif

#if defined( VK_AMD_draw_indirect_count )
	VULKAN_PROC( vkCmdDrawIndirectCountAMD )
	VULKAN_PROC( vkCmdDrawIndexedIndirectCountAMD )
#endif

#if defined( VK_AMD_shader_info )
	VULKAN_PROC( vkGetShaderInfoAMD )
#endif

#if defined( VK_EXT_conditional_rendering )
	VULKAN_PROC( vkCmdBeginConditionalRenderingEXT )
	VULKAN_PROC( vkCmdEndConditionalRenderingEXT )
#endif

#if defined( VK_NV_clip_space_w_scaling )
	VULKAN_PROC( vkCmdSetViewportWScalingNV )
#endif

#if defined( VK_EXT_display_control )
	VULKAN_PROC( vkDisplayPowerControlEXT )
	VULKAN_PROC( vkRegisterDeviceEventEXT )
	VULKAN_PROC( vkRegisterDisplayEventEXT )
	VULKAN_PROC( vkGetSwapchainCounterEXT )
#endif

#if defined( VK_GOOGLE_display_timing )
	VULKAN_PROC( vkGetRefreshCycleDurationGOOGLE )
	VULKAN_PROC( vkGetPastPresentationTimingGOOGLE )
#endif

#if defined( VK_EXT_discard_rectangles )
	VULKAN_PROC( vkCmdSetDiscardRectangleEXT )
	VULKAN_PROC( vkCmdSetDiscardRectangleEnableEXT )
	VULKAN_PROC( vkCmdSetDiscardRectangleModeEXT )
#endif

#if defined( VK_EXT_hdr_metadata )
	VULKAN_PROC( vkSetHdrMetadataEXT )
#endif

#if defined( VK_EXT_debug_utils )
	VULKAN_PROC( vkSetDebugUtilsObjectNameEXT )
	VULKAN_PROC( vkSetDebugUtilsObjectTagEXT )
	VULKAN_PROC( vkQueueBeginDebugUtilsLabelEXT )
	VULKAN_PROC( vkQueueEndDebugUtilsLabelEXT )
	VULKAN_PROC( vkQueueInsertDebugUtilsLabelEXT )
	VULKAN_PROC( vkCmdBeginDebugUtilsLabelEXT )
	VULKAN_PROC( vkCmdEndDebugUtilsLabelEXT )
	VULKAN_PROC( vkCmdInsertDebugUtilsLabelEXT )
#endif

#if defined( VK_AMD_gpa_interface )
	VULKAN_PROC( vkCreateGpaSessionAMD )
	VULKAN_PROC( vkDestroyGpaSessionAMD )
	VULKAN_PROC( vkSetGpaDeviceClockModeAMD )
	VULKAN_PROC( vkGetGpaDeviceClockInfoAMD )
	VULKAN_PROC( vkCmdBeginGpaSessionAMD )
	VULKAN_PROC( vkCmdEndGpaSessionAMD )
	VULKAN_PROC( vkCmdBeginGpaSampleAMD )
	VULKAN_PROC( vkCmdEndGpaSampleAMD )
	VULKAN_PROC( vkGetGpaSessionStatusAMD )
	VULKAN_PROC( vkGetGpaSessionResultsAMD )
	VULKAN_PROC( vkResetGpaSessionAMD )
	VULKAN_PROC( vkCmdCopyGpaSessionResultsAMD )
#endif

#if defined( VK_EXT_descriptor_heap )
	VULKAN_PROC( vkWriteSamplerDescriptorsEXT )
	VULKAN_PROC( vkWriteResourceDescriptorsEXT )
	VULKAN_PROC( vkCmdBindSamplerHeapEXT )
	VULKAN_PROC( vkCmdBindResourceHeapEXT )
	VULKAN_PROC( vkCmdPushDataEXT )
	VULKAN_PROC( vkGetImageOpaqueCaptureDataEXT )
	VULKAN_PROC( vkRegisterCustomBorderColorEXT )
	VULKAN_PROC( vkUnregisterCustomBorderColorEXT )
	VULKAN_PROC( vkGetTensorOpaqueCaptureDataARM )
#endif

#if defined( VK_EXT_sample_locations )
	VULKAN_PROC( vkCmdSetSampleLocationsEXT )
#endif

#if defined( VK_EXT_image_drm_format_modifier )
	VULKAN_PROC( vkGetImageDrmFormatModifierPropertiesEXT )
#endif

#if defined( VK_EXT_validation_cache )
	VULKAN_PROC( vkCreateValidationCacheEXT )
	VULKAN_PROC( vkDestroyValidationCacheEXT )
	VULKAN_PROC( vkMergeValidationCachesEXT )
	VULKAN_PROC( vkGetValidationCacheDataEXT )
#endif

#if defined( VK_NV_shading_rate_image )
	VULKAN_PROC( vkCmdBindShadingRateImageNV )
	VULKAN_PROC( vkCmdSetViewportShadingRatePaletteNV )
	VULKAN_PROC( vkCmdSetCoarseSampleOrderNV )
#endif

#if defined( VK_NV_ray_tracing )
	VULKAN_PROC( vkCreateAccelerationStructureNV )
	VULKAN_PROC( vkDestroyAccelerationStructureNV )
	VULKAN_PROC( vkGetAccelerationStructureMemoryRequirementsNV )
	VULKAN_PROC( vkBindAccelerationStructureMemoryNV )
	VULKAN_PROC( vkCmdBuildAccelerationStructureNV )
	VULKAN_PROC( vkCmdCopyAccelerationStructureNV )
	VULKAN_PROC( vkCmdTraceRaysNV )
	VULKAN_PROC( vkCreateRayTracingPipelinesNV )
	VULKAN_PROC( vkGetRayTracingShaderGroupHandlesKHR )
	VULKAN_PROC( vkGetRayTracingShaderGroupHandlesNV )
	VULKAN_PROC( vkGetAccelerationStructureHandleNV )
	VULKAN_PROC( vkCmdWriteAccelerationStructuresPropertiesNV )
	VULKAN_PROC( vkCompileDeferredNV )
#endif

#if defined( VK_EXT_external_memory_host )
	VULKAN_PROC( vkGetMemoryHostPointerPropertiesEXT )
#endif

#if defined( VK_AMD_buffer_marker )
	VULKAN_PROC( vkCmdWriteBufferMarkerAMD )
	VULKAN_PROC( vkCmdWriteBufferMarker2AMD )
#endif

#if defined( VK_EXT_calibrated_timestamps )
	VULKAN_PROC( vkGetCalibratedTimestampsEXT )
#endif

#if defined( VK_NV_mesh_shader )
	VULKAN_PROC( vkCmdDrawMeshTasksNV )
	VULKAN_PROC( vkCmdDrawMeshTasksIndirectNV )
	VULKAN_PROC( vkCmdDrawMeshTasksIndirectCountNV )
#endif

#if defined( VK_NV_scissor_exclusive )
	VULKAN_PROC( vkCmdSetExclusiveScissorEnableNV )
	VULKAN_PROC( vkCmdSetExclusiveScissorNV )
#endif

#if defined( VK_NV_device_diagnostic_checkpoints )
	VULKAN_PROC( vkCmdSetCheckpointNV )
	VULKAN_PROC( vkGetQueueCheckpointDataNV )
	VULKAN_PROC( vkGetQueueCheckpointData2NV )
#endif

#if defined( VK_EXT_present_timing )
	VULKAN_PROC( vkSetSwapchainPresentTimingQueueSizeEXT )
	VULKAN_PROC( vkGetSwapchainTimingPropertiesEXT )
	VULKAN_PROC( vkGetSwapchainTimeDomainPropertiesEXT )
	VULKAN_PROC( vkGetPastPresentationTimingEXT )
#endif

#if defined( VK_INTEL_performance_query )
	VULKAN_PROC( vkInitializePerformanceApiINTEL )
	VULKAN_PROC( vkUninitializePerformanceApiINTEL )
	VULKAN_PROC( vkCmdSetPerformanceMarkerINTEL )
	VULKAN_PROC( vkCmdSetPerformanceStreamMarkerINTEL )
	VULKAN_PROC( vkCmdSetPerformanceOverrideINTEL )
	VULKAN_PROC( vkAcquirePerformanceConfigurationINTEL )
	VULKAN_PROC( vkReleasePerformanceConfigurationINTEL )
	VULKAN_PROC( vkQueueSetPerformanceConfigurationINTEL )
	VULKAN_PROC( vkGetPerformanceParameterINTEL )
#endif

#if defined( VK_AMD_display_native_hdr )
	VULKAN_PROC( vkSetLocalDimmingAMD )
#endif

#if defined( VK_EXT_buffer_device_address )
	VULKAN_PROC( vkGetBufferDeviceAddressEXT )
#endif

#if defined( VK_EXT_line_rasterization )
	VULKAN_PROC( vkCmdSetLineStippleEXT )
#endif

#if defined( VK_EXT_host_query_reset )
	VULKAN_PROC( vkResetQueryPoolEXT )
#endif

#if defined( VK_EXT_extended_dynamic_state )
	VULKAN_PROC( vkCmdSetCullModeEXT )
	VULKAN_PROC( vkCmdSetFrontFaceEXT )
	VULKAN_PROC( vkCmdSetPrimitiveTopologyEXT )
	VULKAN_PROC( vkCmdSetViewportWithCountEXT )
	VULKAN_PROC( vkCmdSetScissorWithCountEXT )
	VULKAN_PROC( vkCmdBindVertexBuffers2EXT )
	VULKAN_PROC( vkCmdSetDepthTestEnableEXT )
	VULKAN_PROC( vkCmdSetDepthWriteEnableEXT )
	VULKAN_PROC( vkCmdSetDepthCompareOpEXT )
	VULKAN_PROC( vkCmdSetDepthBoundsTestEnableEXT )
	VULKAN_PROC( vkCmdSetStencilTestEnableEXT )
	VULKAN_PROC( vkCmdSetStencilOpEXT )
#endif

#if defined( VK_EXT_host_image_copy )
	VULKAN_PROC( vkCopyMemoryToImageEXT )
	VULKAN_PROC( vkCopyImageToMemoryEXT )
	VULKAN_PROC( vkCopyImageToImageEXT )
	VULKAN_PROC( vkTransitionImageLayoutEXT )
	VULKAN_PROC( vkGetImageSubresourceLayout2EXT )
#endif

#if defined( VK_EXT_swapchain_maintenance1 )
	VULKAN_PROC( vkReleaseSwapchainImagesEXT )
#endif

#if defined( VK_NV_device_generated_commands )
	VULKAN_PROC( vkGetGeneratedCommandsMemoryRequirementsNV )
	VULKAN_PROC( vkCmdPreprocessGeneratedCommandsNV )
	VULKAN_PROC( vkCmdExecuteGeneratedCommandsNV )
	VULKAN_PROC( vkCmdBindPipelineShaderGroupNV )
	VULKAN_PROC( vkCreateIndirectCommandsLayoutNV )
	VULKAN_PROC( vkDestroyIndirectCommandsLayoutNV )
#endif

#if defined( VK_EXT_depth_bias_control )
	VULKAN_PROC( vkCmdSetDepthBias2EXT )
#endif

#if defined( VK_EXT_private_data )
	VULKAN_PROC( vkCreatePrivateDataSlotEXT )
	VULKAN_PROC( vkDestroyPrivateDataSlotEXT )
	VULKAN_PROC( vkSetPrivateDataEXT )
	VULKAN_PROC( vkGetPrivateDataEXT )
#endif

#if defined( VK_QCOM_queue_perf_hint )
	VULKAN_PROC( vkQueueSetPerfHintQCOM )
#endif

#if defined( VK_QCOM_tile_shading )
	VULKAN_PROC( vkCmdDispatchTileQCOM )
	VULKAN_PROC( vkCmdBeginPerTileExecutionQCOM )
	VULKAN_PROC( vkCmdEndPerTileExecutionQCOM )
#endif

#if defined( VK_NV_low_latency )
	VULKAN_PROC( vkSetLatencySleepModeLegacyNV )
	VULKAN_PROC( vkLatencySleepLegacyNV )
	VULKAN_PROC( vkSetLatencyMarkerLegacyNV )
	VULKAN_PROC( vkGetLatencyTimingsLegacyNV )
	VULKAN_PROC( vkQueueNotifyOutOfBandLegacyNV )
	VULKAN_PROC( vkGetSleepStatusLegacyNV )
	VULKAN_PROC( vkShutdownLatencyDeviceLegacyNV )
#endif

#if defined( VK_EXT_descriptor_buffer )
	VULKAN_PROC( vkGetDescriptorSetLayoutSizeEXT )
	VULKAN_PROC( vkGetDescriptorSetLayoutBindingOffsetEXT )
	VULKAN_PROC( vkGetDescriptorEXT )
	VULKAN_PROC( vkCmdBindDescriptorBuffersEXT )
	VULKAN_PROC( vkCmdSetDescriptorBufferOffsetsEXT )
	VULKAN_PROC( vkCmdBindDescriptorBufferEmbeddedSamplersEXT )
	VULKAN_PROC( vkGetBufferOpaqueCaptureDescriptorDataEXT )
	VULKAN_PROC( vkGetImageOpaqueCaptureDescriptorDataEXT )
	VULKAN_PROC( vkGetImageViewOpaqueCaptureDescriptorDataEXT )
	VULKAN_PROC( vkGetSamplerOpaqueCaptureDescriptorDataEXT )
	VULKAN_PROC( vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT )
#endif

#if defined( VK_NV_fragment_shading_rate_enums )
	VULKAN_PROC( vkCmdSetFragmentShadingRateEnumNV )
#endif

#if defined( VK_EXT_device_fault )
	VULKAN_PROC( vkGetDeviceFaultInfoEXT )
#endif

#if defined( VK_EXT_vertex_input_dynamic_state )
	VULKAN_PROC( vkCmdSetVertexInputEXT )
#endif

#if defined( VK_HUAWEI_subpass_shading )
	VULKAN_PROC( vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI )
	VULKAN_PROC( vkCmdSubpassShadingHUAWEI )
#endif

#if defined( VK_HUAWEI_invocation_mask )
	VULKAN_PROC( vkCmdBindInvocationMaskHUAWEI )
#endif

#if defined( VK_NV_external_memory_rdma )
	VULKAN_PROC( vkGetMemoryRemoteAddressNV )
#endif

#if defined( VK_EXT_pipeline_properties )
	VULKAN_PROC( vkGetPipelinePropertiesEXT )
#endif

#if defined( VK_EXT_extended_dynamic_state2 )
	VULKAN_PROC( vkCmdSetPatchControlPointsEXT )
	VULKAN_PROC( vkCmdSetRasterizerDiscardEnableEXT )
	VULKAN_PROC( vkCmdSetDepthBiasEnableEXT )
	VULKAN_PROC( vkCmdSetLogicOpEXT )
	VULKAN_PROC( vkCmdSetPrimitiveRestartEnableEXT )
#endif

#if defined( VK_EXT_color_write_enable )
	VULKAN_PROC( vkCmdSetColorWriteEnableEXT )
#endif

#if defined( VK_EXT_multi_draw )
	VULKAN_PROC( vkCmdDrawMultiEXT )
	VULKAN_PROC( vkCmdDrawMultiIndexedEXT )
#endif

#if defined( VK_EXT_opacity_micromap )
	VULKAN_PROC( vkCreateMicromapEXT )
	VULKAN_PROC( vkDestroyMicromapEXT )
	VULKAN_PROC( vkCmdBuildMicromapsEXT )
	VULKAN_PROC( vkBuildMicromapsEXT )
	VULKAN_PROC( vkCopyMicromapEXT )
	VULKAN_PROC( vkCopyMicromapToMemoryEXT )
	VULKAN_PROC( vkCopyMemoryToMicromapEXT )
	VULKAN_PROC( vkWriteMicromapsPropertiesEXT )
	VULKAN_PROC( vkCmdCopyMicromapEXT )
	VULKAN_PROC( vkCmdCopyMicromapToMemoryEXT )
	VULKAN_PROC( vkCmdCopyMemoryToMicromapEXT )
	VULKAN_PROC( vkCmdWriteMicromapsPropertiesEXT )
	VULKAN_PROC( vkGetDeviceMicromapCompatibilityEXT )
	VULKAN_PROC( vkGetMicromapBuildSizesEXT )
#endif

#if defined( VK_HUAWEI_cluster_culling_shader )
	VULKAN_PROC( vkCmdDrawClusterHUAWEI )
	VULKAN_PROC( vkCmdDrawClusterIndirectHUAWEI )
#endif

#if defined( VK_EXT_pageable_device_local_memory )
	VULKAN_PROC( vkSetDeviceMemoryPriorityEXT )
#endif

#if defined( VK_ARM_scheduling_controls )
	VULKAN_PROC( vkCmdSetDispatchParametersARM )
#endif

#if defined( VK_VALVE_descriptor_set_host_mapping )
	VULKAN_PROC( vkGetDescriptorSetLayoutHostMappingInfoVALVE )
	VULKAN_PROC( vkGetDescriptorSetHostMappingVALVE )
#endif

#if defined( VK_NV_copy_memory_indirect )
	VULKAN_PROC( vkCmdCopyMemoryIndirectNV )
	VULKAN_PROC( vkCmdCopyMemoryToImageIndirectNV )
#endif

#if defined( VK_NV_memory_decompression )
	VULKAN_PROC( vkCmdDecompressMemoryNV )
	VULKAN_PROC( vkCmdDecompressMemoryIndirectCountNV )
#endif

#if defined( VK_NV_device_generated_commands_compute )
	VULKAN_PROC( vkGetPipelineIndirectMemoryRequirementsNV )
	VULKAN_PROC( vkCmdUpdatePipelineIndirectBufferNV )
	VULKAN_PROC( vkGetPipelineIndirectDeviceAddressNV )
#endif

#if defined( VK_EXT_extended_dynamic_state3 )
	VULKAN_PROC( vkCmdSetDepthClampEnableEXT )
	VULKAN_PROC( vkCmdSetPolygonModeEXT )
	VULKAN_PROC( vkCmdSetRasterizationSamplesEXT )
	VULKAN_PROC( vkCmdSetSampleMaskEXT )
	VULKAN_PROC( vkCmdSetAlphaToCoverageEnableEXT )
	VULKAN_PROC( vkCmdSetAlphaToOneEnableEXT )
	VULKAN_PROC( vkCmdSetLogicOpEnableEXT )
	VULKAN_PROC( vkCmdSetColorBlendEnableEXT )
	VULKAN_PROC( vkCmdSetColorBlendEquationEXT )
	VULKAN_PROC( vkCmdSetColorWriteMaskEXT )
	VULKAN_PROC( vkCmdSetTessellationDomainOriginEXT )
	VULKAN_PROC( vkCmdSetRasterizationStreamEXT )
	VULKAN_PROC( vkCmdSetConservativeRasterizationModeEXT )
	VULKAN_PROC( vkCmdSetExtraPrimitiveOverestimationSizeEXT )
	VULKAN_PROC( vkCmdSetDepthClipEnableEXT )
	VULKAN_PROC( vkCmdSetSampleLocationsEnableEXT )
	VULKAN_PROC( vkCmdSetColorBlendAdvancedEXT )
	VULKAN_PROC( vkCmdSetProvokingVertexModeEXT )
	VULKAN_PROC( vkCmdSetLineRasterizationModeEXT )
	VULKAN_PROC( vkCmdSetLineStippleEnableEXT )
	VULKAN_PROC( vkCmdSetDepthClipNegativeOneToOneEXT )
	VULKAN_PROC( vkCmdSetViewportWScalingEnableNV )
	VULKAN_PROC( vkCmdSetViewportSwizzleNV )
	VULKAN_PROC( vkCmdSetCoverageToColorEnableNV )
	VULKAN_PROC( vkCmdSetCoverageToColorLocationNV )
	VULKAN_PROC( vkCmdSetCoverageModulationModeNV )
	VULKAN_PROC( vkCmdSetCoverageModulationTableEnableNV )
	VULKAN_PROC( vkCmdSetCoverageModulationTableNV )
	VULKAN_PROC( vkCmdSetShadingRateImageEnableNV )
	VULKAN_PROC( vkCmdSetRepresentativeFragmentTestEnableNV )
	VULKAN_PROC( vkCmdSetCoverageReductionModeNV )
#endif

#if defined( VK_ARM_tensors )
	VULKAN_PROC( vkCreateTensorARM )
	VULKAN_PROC( vkDestroyTensorARM )
	VULKAN_PROC( vkCreateTensorViewARM )
	VULKAN_PROC( vkDestroyTensorViewARM )
	VULKAN_PROC( vkGetTensorMemoryRequirementsARM )
	VULKAN_PROC( vkBindTensorMemoryARM )
	VULKAN_PROC( vkGetDeviceTensorMemoryRequirementsARM )
	VULKAN_PROC( vkCmdCopyTensorARM )
	VULKAN_PROC( vkGetTensorOpaqueCaptureDescriptorDataARM )
	VULKAN_PROC( vkGetTensorViewOpaqueCaptureDescriptorDataARM )
#endif

#if defined( VK_EXT_shader_module_identifier )
	VULKAN_PROC( vkGetShaderModuleIdentifierEXT )
	VULKAN_PROC( vkGetShaderModuleCreateInfoIdentifierEXT )
#endif

#if defined( VK_NV_optical_flow )
	VULKAN_PROC( vkCreateOpticalFlowSessionNV )
	VULKAN_PROC( vkDestroyOpticalFlowSessionNV )
	VULKAN_PROC( vkBindOpticalFlowSessionImageNV )
	VULKAN_PROC( vkCmdOpticalFlowExecuteNV )
#endif

#if defined( VK_AMD_anti_lag )
	VULKAN_PROC( vkAntiLagUpdateAMD )
#endif

#if defined( VK_EXT_shader_object )
	VULKAN_PROC( vkCreateShadersEXT )
	VULKAN_PROC( vkDestroyShaderEXT )
	VULKAN_PROC( vkGetShaderBinaryDataEXT )
	VULKAN_PROC( vkCmdBindShadersEXT )
	VULKAN_PROC( vkCmdSetDepthClampRangeEXT )
#endif

#if defined( VK_QCOM_tile_properties )
	VULKAN_PROC( vkGetFramebufferTilePropertiesQCOM )
	VULKAN_PROC( vkGetDynamicRenderingTilePropertiesQCOM )
#endif

#if defined( VK_NV_cooperative_vector )
	VULKAN_PROC( vkConvertCooperativeVectorMatrixNV )
	VULKAN_PROC( vkCmdConvertCooperativeVectorMatrixNV )
#endif

#if defined( VK_NV_low_latency2 )
	VULKAN_PROC( vkSetLatencySleepModeNV )
	VULKAN_PROC( vkLatencySleepNV )
	VULKAN_PROC( vkSetLatencyMarkerNV )
	VULKAN_PROC( vkGetLatencyTimingsNV )
	VULKAN_PROC( vkQueueNotifyOutOfBandNV )
#endif

#if defined( VK_ARM_data_graph )
	VULKAN_PROC( vkCreateDataGraphPipelinesARM )
	VULKAN_PROC( vkCreateDataGraphPipelineSessionARM )
	VULKAN_PROC( vkGetDataGraphPipelineSessionBindPointRequirementsARM )
	VULKAN_PROC( vkGetDataGraphPipelineSessionMemoryRequirementsARM )
	VULKAN_PROC( vkBindDataGraphPipelineSessionMemoryARM )
	VULKAN_PROC( vkDestroyDataGraphPipelineSessionARM )
	VULKAN_PROC( vkCmdDispatchDataGraphARM )
	VULKAN_PROC( vkGetDataGraphPipelineAvailablePropertiesARM )
	VULKAN_PROC( vkGetDataGraphPipelinePropertiesARM )
#endif

#if defined( VK_EXT_attachment_feedback_loop_dynamic_state )
	VULKAN_PROC( vkCmdSetAttachmentFeedbackLoopEnableEXT )
#endif

#if defined( VK_QCOM_tile_memory_heap )
	VULKAN_PROC( vkCmdBindTileMemoryQCOM )
#endif

#if defined( VK_EXT_memory_decompression )
	VULKAN_PROC( vkCmdDecompressMemoryEXT )
	VULKAN_PROC( vkCmdDecompressMemoryIndirectCountEXT )
#endif

#if defined( VK_NV_external_compute_queue )
	VULKAN_PROC( vkCreateExternalComputeQueueNV )
	VULKAN_PROC( vkDestroyExternalComputeQueueNV )
	VULKAN_PROC( vkGetExternalComputeQueueDataNV )
#endif

#if defined( VK_NV_cluster_acceleration_structure )
	VULKAN_PROC( vkGetClusterAccelerationStructureBuildSizesNV )
	VULKAN_PROC( vkCmdBuildClusterAccelerationStructureIndirectNV )
#endif

#if defined( VK_NV_partitioned_acceleration_structure )
	VULKAN_PROC( vkGetPartitionedAccelerationStructuresBuildSizesNV )
	VULKAN_PROC( vkCmdBuildPartitionedAccelerationStructuresNV )
#endif

#if defined( VK_EXT_device_generated_commands )
	VULKAN_PROC( vkGetGeneratedCommandsMemoryRequirementsEXT )
	VULKAN_PROC( vkCmdPreprocessGeneratedCommandsEXT )
	VULKAN_PROC( vkCmdExecuteGeneratedCommandsEXT )
	VULKAN_PROC( vkCreateIndirectCommandsLayoutEXT )
	VULKAN_PROC( vkDestroyIndirectCommandsLayoutEXT )
	VULKAN_PROC( vkCreateIndirectExecutionSetEXT )
	VULKAN_PROC( vkDestroyIndirectExecutionSetEXT )
	VULKAN_PROC( vkUpdateIndirectExecutionSetPipelineEXT )
	VULKAN_PROC( vkUpdateIndirectExecutionSetShaderEXT )
#endif

#if defined( VK_ARM_shader_instrumentation )
	VULKAN_PROC( vkCreateShaderInstrumentationARM )
	VULKAN_PROC( vkDestroyShaderInstrumentationARM )
	VULKAN_PROC( vkCmdBeginShaderInstrumentationARM )
	VULKAN_PROC( vkCmdEndShaderInstrumentationARM )
	VULKAN_PROC( vkGetShaderInstrumentationValuesARM )
	VULKAN_PROC( vkClearShaderInstrumentationMetricsARM )
#endif

#if defined( VK_EXT_fragment_density_map_offset )
	VULKAN_PROC( vkCmdEndRendering2EXT )
#endif

#if defined( VK_EXT_custom_resolve )
	VULKAN_PROC( vkCmdBeginCustomResolveEXT )
#endif

#if defined( VK_NV_compute_occupancy_priority )
	VULKAN_PROC( vkCmdSetComputeOccupancyPriorityNV )
#endif

#if defined( VK_EXT_primitive_restart_index )
	VULKAN_PROC( vkCmdSetPrimitiveRestartIndexEXT )
#endif

#if defined( VK_KHR_acceleration_structure )
	VULKAN_PROC( vkCreateAccelerationStructureKHR )
	VULKAN_PROC( vkDestroyAccelerationStructureKHR )
	VULKAN_PROC( vkCmdBuildAccelerationStructuresKHR )
	VULKAN_PROC( vkCmdBuildAccelerationStructuresIndirectKHR )
	VULKAN_PROC( vkBuildAccelerationStructuresKHR )
	VULKAN_PROC( vkCopyAccelerationStructureKHR )
	VULKAN_PROC( vkCopyAccelerationStructureToMemoryKHR )
	VULKAN_PROC( vkCopyMemoryToAccelerationStructureKHR )
	VULKAN_PROC( vkWriteAccelerationStructuresPropertiesKHR )
	VULKAN_PROC( vkCmdCopyAccelerationStructureKHR )
	VULKAN_PROC( vkCmdCopyAccelerationStructureToMemoryKHR )
	VULKAN_PROC( vkCmdCopyMemoryToAccelerationStructureKHR )
	VULKAN_PROC( vkGetAccelerationStructureDeviceAddressKHR )
	VULKAN_PROC( vkCmdWriteAccelerationStructuresPropertiesKHR )
	VULKAN_PROC( vkGetDeviceAccelerationStructureCompatibilityKHR )
	VULKAN_PROC( vkGetAccelerationStructureBuildSizesKHR )
#endif

#if defined( VK_KHR_ray_tracing_pipeline )
	VULKAN_PROC( vkCmdTraceRaysKHR )
	VULKAN_PROC( vkCreateRayTracingPipelinesKHR )
	VULKAN_PROC( vkGetRayTracingCaptureReplayShaderGroupHandlesKHR )
	VULKAN_PROC( vkCmdTraceRaysIndirectKHR )
	VULKAN_PROC( vkGetRayTracingShaderGroupStackSizeKHR )
	VULKAN_PROC( vkCmdSetRayTracingPipelineStackSizeKHR )
#endif

#if defined( VK_EXT_mesh_shader )
	VULKAN_PROC( vkCmdDrawMeshTasksEXT )
	VULKAN_PROC( vkCmdDrawMeshTasksIndirectEXT )
	VULKAN_PROC( vkCmdDrawMeshTasksIndirectCountEXT )
#endif

#if defined( VK_ANDROID_external_memory_android_hardware_buffer )
	VULKAN_PROC( vkGetAndroidHardwareBufferPropertiesANDROID )
	VULKAN_PROC( vkGetMemoryAndroidHardwareBufferANDROID )
#endif

#if defined( VK_FUCHSIA_external_memory )
	VULKAN_PROC( vkGetMemoryZirconHandleFUCHSIA )
	VULKAN_PROC( vkGetMemoryZirconHandlePropertiesFUCHSIA )
#endif

#if defined( VK_FUCHSIA_external_semaphore )
	VULKAN_PROC( vkImportSemaphoreZirconHandleFUCHSIA )
	VULKAN_PROC( vkGetSemaphoreZirconHandleFUCHSIA )
#endif

#if defined( VK_FUCHSIA_buffer_collection )
	VULKAN_PROC( vkCreateBufferCollectionFUCHSIA )
	VULKAN_PROC( vkSetBufferCollectionImageConstraintsFUCHSIA )
	VULKAN_PROC( vkSetBufferCollectionBufferConstraintsFUCHSIA )
	VULKAN_PROC( vkDestroyBufferCollectionFUCHSIA )
	VULKAN_PROC( vkGetBufferCollectionPropertiesFUCHSIA )
#endif

#if defined( VK_EXT_metal_objects )
	VULKAN_PROC( vkExportMetalObjectsEXT )
#endif

#if defined( VK_EXT_external_memory_metal )
	VULKAN_PROC( vkGetMemoryMetalHandleEXT )
	VULKAN_PROC( vkGetMemoryMetalHandlePropertiesEXT )
#endif

#if defined( VK_KHR_external_memory_win32 )
	VULKAN_PROC( vkGetMemoryWin32HandleKHR )
	VULKAN_PROC( vkGetMemoryWin32HandlePropertiesKHR )
#endif

#if defined( VK_KHR_external_semaphore_win32 )
	VULKAN_PROC( vkImportSemaphoreWin32HandleKHR )
	VULKAN_PROC( vkGetSemaphoreWin32HandleKHR )
#endif

#if defined( VK_KHR_external_fence_win32 )
	VULKAN_PROC( vkImportFenceWin32HandleKHR )
	VULKAN_PROC( vkGetFenceWin32HandleKHR )
#endif

#if defined( VK_NV_external_memory_win32 )
	VULKAN_PROC( vkGetMemoryWin32HandleNV )
#endif

#if defined( VK_EXT_full_screen_exclusive )
	VULKAN_PROC( vkAcquireFullScreenExclusiveModeEXT )
	VULKAN_PROC( vkReleaseFullScreenExclusiveModeEXT )
	VULKAN_PROC( vkGetDeviceGroupSurfacePresentModes2EXT )
#endif

#if defined( VK_QNX_external_memory_screen_buffer )
	VULKAN_PROC( vkGetScreenBufferPropertiesQNX )
#endif

#if defined( VK_AMDX_shader_enqueue )
	VULKAN_PROC( vkCreateExecutionGraphPipelinesAMDX )
	VULKAN_PROC( vkGetExecutionGraphPipelineScratchSizeAMDX )
	VULKAN_PROC( vkGetExecutionGraphPipelineNodeIndexAMDX )
	VULKAN_PROC( vkCmdInitializeGraphScratchMemoryAMDX )
	VULKAN_PROC( vkCmdDispatchGraphAMDX )
	VULKAN_PROC( vkCmdDispatchGraphIndirectAMDX )
	VULKAN_PROC( vkCmdDispatchGraphIndirectCountAMDX )
#endif

#if defined( VK_NV_cuda_kernel_launch )
	VULKAN_PROC( vkCreateCudaModuleNV )
	VULKAN_PROC( vkGetCudaModuleCacheNV )
	VULKAN_PROC( vkCreateCudaFunctionNV )
	VULKAN_PROC( vkDestroyCudaModuleNV )
	VULKAN_PROC( vkDestroyCudaFunctionNV )
	VULKAN_PROC( vkCmdCudaLaunchKernelNV )
#endif

#if defined( VK_OHOS_external_memory )
	VULKAN_PROC( vkGetNativeBufferPropertiesOHOS )
	VULKAN_PROC( vkGetMemoryNativeBufferOHOS )
#endif

#undef VULKAN_PROC

	return procs.vkDestroyDevice != nullptr && procs.vkGetDeviceQueue != nullptr && procs.vkQueueSubmit != nullptr && procs.vkQueueWaitIdle != nullptr &&
	       procs.vkDeviceWaitIdle != nullptr && procs.vkAllocateMemory != nullptr && procs.vkFreeMemory != nullptr && procs.vkMapMemory != nullptr &&
	       procs.vkUnmapMemory != nullptr && procs.vkFlushMappedMemoryRanges != nullptr && procs.vkInvalidateMappedMemoryRanges != nullptr &&
	       procs.vkGetDeviceMemoryCommitment != nullptr && procs.vkBindBufferMemory != nullptr && procs.vkBindImageMemory != nullptr &&
	       procs.vkGetBufferMemoryRequirements != nullptr && procs.vkGetImageMemoryRequirements != nullptr && procs.vkGetImageSparseMemoryRequirements != nullptr &&
	       procs.vkQueueBindSparse != nullptr && procs.vkCreateFence != nullptr && procs.vkDestroyFence != nullptr && procs.vkResetFences != nullptr &&
	       procs.vkGetFenceStatus != nullptr && procs.vkWaitForFences != nullptr && procs.vkCreateSemaphore != nullptr && procs.vkDestroySemaphore != nullptr &&
	       procs.vkCreateQueryPool != nullptr && procs.vkDestroyQueryPool != nullptr && procs.vkGetQueryPoolResults != nullptr && procs.vkCreateBuffer != nullptr &&
	       procs.vkDestroyBuffer != nullptr && procs.vkCreateImage != nullptr && procs.vkDestroyImage != nullptr && procs.vkGetImageSubresourceLayout != nullptr &&
	       procs.vkCreateImageView != nullptr && procs.vkDestroyImageView != nullptr && procs.vkCreateCommandPool != nullptr && procs.vkDestroyCommandPool != nullptr &&
	       procs.vkResetCommandPool != nullptr && procs.vkAllocateCommandBuffers != nullptr && procs.vkFreeCommandBuffers != nullptr && procs.vkBeginCommandBuffer != nullptr &&
	       procs.vkEndCommandBuffer != nullptr && procs.vkResetCommandBuffer != nullptr && procs.vkCmdCopyBuffer != nullptr && procs.vkCmdCopyImage != nullptr &&
	       procs.vkCmdCopyBufferToImage != nullptr && procs.vkCmdCopyImageToBuffer != nullptr && procs.vkCmdUpdateBuffer != nullptr && procs.vkCmdFillBuffer != nullptr &&
	       procs.vkCmdPipelineBarrier != nullptr && procs.vkCmdBeginQuery != nullptr && procs.vkCmdEndQuery != nullptr && procs.vkCmdResetQueryPool != nullptr &&
	       procs.vkCmdWriteTimestamp != nullptr && procs.vkCmdCopyQueryPoolResults != nullptr && procs.vkCmdExecuteCommands != nullptr && procs.vkCreateEvent != nullptr &&
	       procs.vkDestroyEvent != nullptr && procs.vkGetEventStatus != nullptr && procs.vkSetEvent != nullptr && procs.vkResetEvent != nullptr &&
	       procs.vkCreateBufferView != nullptr && procs.vkDestroyBufferView != nullptr && procs.vkCreateShaderModule != nullptr && procs.vkDestroyShaderModule != nullptr &&
	       procs.vkCreatePipelineCache != nullptr && procs.vkDestroyPipelineCache != nullptr && procs.vkGetPipelineCacheData != nullptr && procs.vkMergePipelineCaches != nullptr &&
	       procs.vkCreateComputePipelines != nullptr && procs.vkDestroyPipeline != nullptr && procs.vkCreatePipelineLayout != nullptr && procs.vkDestroyPipelineLayout != nullptr &&
	       procs.vkCreateSampler != nullptr && procs.vkDestroySampler != nullptr && procs.vkCreateDescriptorSetLayout != nullptr && procs.vkDestroyDescriptorSetLayout != nullptr &&
	       procs.vkCreateDescriptorPool != nullptr && procs.vkDestroyDescriptorPool != nullptr && procs.vkResetDescriptorPool != nullptr &&
	       procs.vkAllocateDescriptorSets != nullptr && procs.vkFreeDescriptorSets != nullptr && procs.vkUpdateDescriptorSets != nullptr && procs.vkCmdBindPipeline != nullptr &&
	       procs.vkCmdBindDescriptorSets != nullptr && procs.vkCmdClearColorImage != nullptr && procs.vkCmdDispatch != nullptr && procs.vkCmdDispatchIndirect != nullptr &&
	       procs.vkCmdSetEvent != nullptr && procs.vkCmdResetEvent != nullptr && procs.vkCmdWaitEvents != nullptr && procs.vkCmdPushConstants != nullptr &&
	       procs.vkCreateGraphicsPipelines != nullptr && procs.vkCreateFramebuffer != nullptr && procs.vkDestroyFramebuffer != nullptr && procs.vkCreateRenderPass != nullptr &&
	       procs.vkDestroyRenderPass != nullptr && procs.vkGetRenderAreaGranularity != nullptr && procs.vkCmdSetViewport != nullptr && procs.vkCmdSetScissor != nullptr &&
	       procs.vkCmdSetLineWidth != nullptr && procs.vkCmdSetDepthBias != nullptr && procs.vkCmdSetBlendConstants != nullptr && procs.vkCmdSetDepthBounds != nullptr &&
	       procs.vkCmdSetStencilCompareMask != nullptr && procs.vkCmdSetStencilWriteMask != nullptr && procs.vkCmdSetStencilReference != nullptr &&
	       procs.vkCmdBindIndexBuffer != nullptr && procs.vkCmdBindVertexBuffers != nullptr && procs.vkCmdDraw != nullptr && procs.vkCmdDrawIndexed != nullptr &&
	       procs.vkCmdDrawIndirect != nullptr && procs.vkCmdDrawIndexedIndirect != nullptr && procs.vkCmdBlitImage != nullptr && procs.vkCmdClearDepthStencilImage != nullptr &&
	       procs.vkCmdClearAttachments != nullptr && procs.vkCmdResolveImage != nullptr && procs.vkCmdBeginRenderPass != nullptr && procs.vkCmdNextSubpass != nullptr &&
	       procs.vkCmdEndRenderPass != nullptr;
}
