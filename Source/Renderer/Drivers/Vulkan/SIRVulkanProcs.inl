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

// Generated from the supplied sirVulkanProcs_t inventory (SDK 1.4.363.1).
// No include guard: define SIR_VK_PROC before each inclusion.
// Arguments: method name, Vulkan symbol, return type, declaration, call arguments.
// Only VkDevice-first commands receive m_Device implicitly.

    SIR_VK_PROC( getInstanceProcAddr, vkGetInstanceProcAddr, PFN_vkVoidFunction,
        (VkInstance instance, const char* pName),
        (instance, pName) )

    // Global procedures.
#if defined(VK_VERSION_1_0)
    SIR_VK_PROC( createInstance, vkCreateInstance, VkResult,
        (const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance),
        (pCreateInfo, pAllocator, pInstance) )
    SIR_VK_PROC( enumerateInstanceExtensionProperties, vkEnumerateInstanceExtensionProperties, VkResult,
        (const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties),
        (pLayerName, pPropertyCount, pProperties) )
    SIR_VK_PROC( enumerateInstanceLayerProperties, vkEnumerateInstanceLayerProperties, VkResult,
        (uint32_t* pPropertyCount, VkLayerProperties* pProperties),
        (pPropertyCount, pProperties) )
#endif

#if defined(VK_VERSION_1_1)
    SIR_VK_PROC( enumerateInstanceVersion, vkEnumerateInstanceVersion, VkResult,
        (uint32_t* pApiVersion),
        (pApiVersion) )
#endif

    // Instance procedures.
#if defined(VK_VERSION_1_0)
    SIR_VK_PROC( destroyInstance, vkDestroyInstance, void,
        (VkInstance instance, const VkAllocationCallbacks* pAllocator),
        (instance, pAllocator) )
    SIR_VK_PROC( enumeratePhysicalDevices, vkEnumeratePhysicalDevices, VkResult,
        (VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices),
        (instance, pPhysicalDeviceCount, pPhysicalDevices) )
    SIR_VK_PROC( getPhysicalDeviceFeatures, vkGetPhysicalDeviceFeatures, void,
        (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures),
        (physicalDevice, pFeatures) )
    SIR_VK_PROC( getPhysicalDeviceFormatProperties, vkGetPhysicalDeviceFormatProperties, void,
        (VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties),
        (physicalDevice, format, pFormatProperties) )
    SIR_VK_PROC( getPhysicalDeviceImageFormatProperties, vkGetPhysicalDeviceImageFormatProperties, VkResult,
        (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties),
        (physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties) )
    SIR_VK_PROC( getPhysicalDeviceProperties, vkGetPhysicalDeviceProperties, void,
        (VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties),
        (physicalDevice, pProperties) )
    SIR_VK_PROC( getPhysicalDeviceQueueFamilyProperties, vkGetPhysicalDeviceQueueFamilyProperties, void,
        (VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties),
        (physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties) )
    SIR_VK_PROC( getPhysicalDeviceMemoryProperties, vkGetPhysicalDeviceMemoryProperties, void,
        (VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties),
        (physicalDevice, pMemoryProperties) )
    SIR_VK_PROC( getDeviceProcAddr, vkGetDeviceProcAddr, PFN_vkVoidFunction,
        (const char* pName),
        (m_Device, pName) )
    SIR_VK_PROC( createDevice, vkCreateDevice, VkResult,
        (VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice),
        (physicalDevice, pCreateInfo, pAllocator, pDevice) )
    SIR_VK_PROC( enumerateDeviceExtensionProperties, vkEnumerateDeviceExtensionProperties, VkResult,
        (VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties),
        (physicalDevice, pLayerName, pPropertyCount, pProperties) )
    SIR_VK_PROC( enumerateDeviceLayerProperties, vkEnumerateDeviceLayerProperties, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties),
        (physicalDevice, pPropertyCount, pProperties) )
    SIR_VK_PROC( getPhysicalDeviceSparseImageFormatProperties, vkGetPhysicalDeviceSparseImageFormatProperties, void,
        (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties),
        (physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties) )
#endif

#if defined(VK_VERSION_1_1)
    SIR_VK_PROC( enumeratePhysicalDeviceGroups, vkEnumeratePhysicalDeviceGroups, VkResult,
        (VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties),
        (instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties) )
    SIR_VK_PROC( getPhysicalDeviceFeatures2, vkGetPhysicalDeviceFeatures2, void,
        (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures),
        (physicalDevice, pFeatures) )
    SIR_VK_PROC( getPhysicalDeviceProperties2, vkGetPhysicalDeviceProperties2, void,
        (VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties),
        (physicalDevice, pProperties) )
    SIR_VK_PROC( getPhysicalDeviceFormatProperties2, vkGetPhysicalDeviceFormatProperties2, void,
        (VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties),
        (physicalDevice, format, pFormatProperties) )
    SIR_VK_PROC( getPhysicalDeviceImageFormatProperties2, vkGetPhysicalDeviceImageFormatProperties2, VkResult,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties),
        (physicalDevice, pImageFormatInfo, pImageFormatProperties) )
    SIR_VK_PROC( getPhysicalDeviceQueueFamilyProperties2, vkGetPhysicalDeviceQueueFamilyProperties2, void,
        (VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties),
        (physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties) )
    SIR_VK_PROC( getPhysicalDeviceMemoryProperties2, vkGetPhysicalDeviceMemoryProperties2, void,
        (VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties),
        (physicalDevice, pMemoryProperties) )
    SIR_VK_PROC( getPhysicalDeviceSparseImageFormatProperties2, vkGetPhysicalDeviceSparseImageFormatProperties2, void,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties),
        (physicalDevice, pFormatInfo, pPropertyCount, pProperties) )
    SIR_VK_PROC( getPhysicalDeviceExternalBufferProperties, vkGetPhysicalDeviceExternalBufferProperties, void,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties),
        (physicalDevice, pExternalBufferInfo, pExternalBufferProperties) )
    SIR_VK_PROC( getPhysicalDeviceExternalFenceProperties, vkGetPhysicalDeviceExternalFenceProperties, void,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties),
        (physicalDevice, pExternalFenceInfo, pExternalFenceProperties) )
    SIR_VK_PROC( getPhysicalDeviceExternalSemaphoreProperties, vkGetPhysicalDeviceExternalSemaphoreProperties, void,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties),
        (physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties) )
#endif

#if defined(VK_VERSION_1_3)
    SIR_VK_PROC( getPhysicalDeviceToolProperties, vkGetPhysicalDeviceToolProperties, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties),
        (physicalDevice, pToolCount, pToolProperties) )
#endif

#if defined(VK_KHR_surface)
    SIR_VK_PROC( destroySurfaceKHR, vkDestroySurfaceKHR, void,
        (VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator),
        (instance, surface, pAllocator) )
    SIR_VK_PROC( getPhysicalDeviceSurfaceSupportKHR, vkGetPhysicalDeviceSurfaceSupportKHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported),
        (physicalDevice, queueFamilyIndex, surface, pSupported) )
    SIR_VK_PROC( getPhysicalDeviceSurfaceCapabilitiesKHR, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities),
        (physicalDevice, surface, pSurfaceCapabilities) )
    SIR_VK_PROC( getPhysicalDeviceSurfaceFormatsKHR, vkGetPhysicalDeviceSurfaceFormatsKHR, VkResult,
        (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats),
        (physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats) )
    SIR_VK_PROC( getPhysicalDeviceSurfacePresentModesKHR, vkGetPhysicalDeviceSurfacePresentModesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes),
        (physicalDevice, surface, pPresentModeCount, pPresentModes) )
#endif

#if defined(VK_KHR_swapchain)
    SIR_VK_PROC( getPhysicalDevicePresentRectanglesKHR, vkGetPhysicalDevicePresentRectanglesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects),
        (physicalDevice, surface, pRectCount, pRects) )
#endif

#if defined(VK_KHR_display)
    SIR_VK_PROC( getPhysicalDeviceDisplayPropertiesKHR, vkGetPhysicalDeviceDisplayPropertiesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayPropertiesKHR* pProperties),
        (physicalDevice, pPropertyCount, pProperties) )
    SIR_VK_PROC( getPhysicalDeviceDisplayPlanePropertiesKHR, vkGetPhysicalDeviceDisplayPlanePropertiesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayPlanePropertiesKHR* pProperties),
        (physicalDevice, pPropertyCount, pProperties) )
    SIR_VK_PROC( getDisplayPlaneSupportedDisplaysKHR, vkGetDisplayPlaneSupportedDisplaysKHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t planeIndex, uint32_t* pDisplayCount, VkDisplayKHR* pDisplays),
        (physicalDevice, planeIndex, pDisplayCount, pDisplays) )
    SIR_VK_PROC( getDisplayModePropertiesKHR, vkGetDisplayModePropertiesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, VkDisplayKHR display, uint32_t* pPropertyCount, VkDisplayModePropertiesKHR* pProperties),
        (physicalDevice, display, pPropertyCount, pProperties) )
    SIR_VK_PROC( createDisplayModeKHR, vkCreateDisplayModeKHR, VkResult,
        (VkPhysicalDevice physicalDevice, VkDisplayKHR display, const VkDisplayModeCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDisplayModeKHR* pMode),
        (physicalDevice, display, pCreateInfo, pAllocator, pMode) )
    SIR_VK_PROC( getDisplayPlaneCapabilitiesKHR, vkGetDisplayPlaneCapabilitiesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, VkDisplayModeKHR mode, uint32_t planeIndex, VkDisplayPlaneCapabilitiesKHR* pCapabilities),
        (physicalDevice, mode, planeIndex, pCapabilities) )
    SIR_VK_PROC( createDisplayPlaneSurfaceKHR, vkCreateDisplayPlaneSurfaceKHR, VkResult,
        (VkInstance instance, const VkDisplaySurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_KHR_video_queue)
    SIR_VK_PROC( getPhysicalDeviceVideoCapabilitiesKHR, vkGetPhysicalDeviceVideoCapabilitiesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, const VkVideoProfileInfoKHR* pVideoProfile, VkVideoCapabilitiesKHR* pCapabilities),
        (physicalDevice, pVideoProfile, pCapabilities) )
    SIR_VK_PROC( getPhysicalDeviceVideoFormatPropertiesKHR, vkGetPhysicalDeviceVideoFormatPropertiesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceVideoFormatInfoKHR* pVideoFormatInfo, uint32_t* pVideoFormatPropertyCount, VkVideoFormatPropertiesKHR* pVideoFormatProperties),
        (physicalDevice, pVideoFormatInfo, pVideoFormatPropertyCount, pVideoFormatProperties) )
#endif

#if defined(VK_KHR_get_physical_device_properties2)
    SIR_VK_PROC( getPhysicalDeviceFeatures2KHR, vkGetPhysicalDeviceFeatures2KHR, void,
        (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures),
        (physicalDevice, pFeatures) )
    SIR_VK_PROC( getPhysicalDeviceProperties2KHR, vkGetPhysicalDeviceProperties2KHR, void,
        (VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties),
        (physicalDevice, pProperties) )
    SIR_VK_PROC( getPhysicalDeviceFormatProperties2KHR, vkGetPhysicalDeviceFormatProperties2KHR, void,
        (VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties),
        (physicalDevice, format, pFormatProperties) )
    SIR_VK_PROC( getPhysicalDeviceImageFormatProperties2KHR, vkGetPhysicalDeviceImageFormatProperties2KHR, VkResult,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties),
        (physicalDevice, pImageFormatInfo, pImageFormatProperties) )
    SIR_VK_PROC( getPhysicalDeviceQueueFamilyProperties2KHR, vkGetPhysicalDeviceQueueFamilyProperties2KHR, void,
        (VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties),
        (physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties) )
    SIR_VK_PROC( getPhysicalDeviceMemoryProperties2KHR, vkGetPhysicalDeviceMemoryProperties2KHR, void,
        (VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties),
        (physicalDevice, pMemoryProperties) )
    SIR_VK_PROC( getPhysicalDeviceSparseImageFormatProperties2KHR, vkGetPhysicalDeviceSparseImageFormatProperties2KHR, void,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties),
        (physicalDevice, pFormatInfo, pPropertyCount, pProperties) )
#endif

#if defined(VK_KHR_device_group_creation)
    SIR_VK_PROC( enumeratePhysicalDeviceGroupsKHR, vkEnumeratePhysicalDeviceGroupsKHR, VkResult,
        (VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties),
        (instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties) )
#endif

#if defined(VK_KHR_external_memory_capabilities)
    SIR_VK_PROC( getPhysicalDeviceExternalBufferPropertiesKHR, vkGetPhysicalDeviceExternalBufferPropertiesKHR, void,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties),
        (physicalDevice, pExternalBufferInfo, pExternalBufferProperties) )
#endif

#if defined(VK_KHR_external_semaphore_capabilities)
    SIR_VK_PROC( getPhysicalDeviceExternalSemaphorePropertiesKHR, vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, void,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties),
        (physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties) )
#endif

#if defined(VK_KHR_external_fence_capabilities)
    SIR_VK_PROC( getPhysicalDeviceExternalFencePropertiesKHR, vkGetPhysicalDeviceExternalFencePropertiesKHR, void,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties),
        (physicalDevice, pExternalFenceInfo, pExternalFenceProperties) )
#endif

#if defined(VK_KHR_performance_query)
    SIR_VK_PROC( enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR, vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t* pCounterCount, VkPerformanceCounterKHR* pCounters, VkPerformanceCounterDescriptionKHR* pCounterDescriptions),
        (physicalDevice, queueFamilyIndex, pCounterCount, pCounters, pCounterDescriptions) )
    SIR_VK_PROC( getPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR, vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR, void,
        (VkPhysicalDevice physicalDevice, const VkQueryPoolPerformanceCreateInfoKHR* pPerformanceQueryCreateInfo, uint32_t* pNumPasses),
        (physicalDevice, pPerformanceQueryCreateInfo, pNumPasses) )
#endif

#if defined(VK_KHR_get_surface_capabilities2)
    SIR_VK_PROC( getPhysicalDeviceSurfaceCapabilities2KHR, vkGetPhysicalDeviceSurfaceCapabilities2KHR, VkResult,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, VkSurfaceCapabilities2KHR* pSurfaceCapabilities),
        (physicalDevice, pSurfaceInfo, pSurfaceCapabilities) )
    SIR_VK_PROC( getPhysicalDeviceSurfaceFormats2KHR, vkGetPhysicalDeviceSurfaceFormats2KHR, VkResult,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, uint32_t* pSurfaceFormatCount, VkSurfaceFormat2KHR* pSurfaceFormats),
        (physicalDevice, pSurfaceInfo, pSurfaceFormatCount, pSurfaceFormats) )
#endif

#if defined(VK_KHR_get_display_properties2)
    SIR_VK_PROC( getPhysicalDeviceDisplayProperties2KHR, vkGetPhysicalDeviceDisplayProperties2KHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayProperties2KHR* pProperties),
        (physicalDevice, pPropertyCount, pProperties) )
    SIR_VK_PROC( getPhysicalDeviceDisplayPlaneProperties2KHR, vkGetPhysicalDeviceDisplayPlaneProperties2KHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayPlaneProperties2KHR* pProperties),
        (physicalDevice, pPropertyCount, pProperties) )
    SIR_VK_PROC( getDisplayModeProperties2KHR, vkGetDisplayModeProperties2KHR, VkResult,
        (VkPhysicalDevice physicalDevice, VkDisplayKHR display, uint32_t* pPropertyCount, VkDisplayModeProperties2KHR* pProperties),
        (physicalDevice, display, pPropertyCount, pProperties) )
    SIR_VK_PROC( getDisplayPlaneCapabilities2KHR, vkGetDisplayPlaneCapabilities2KHR, VkResult,
        (VkPhysicalDevice physicalDevice, const VkDisplayPlaneInfo2KHR* pDisplayPlaneInfo, VkDisplayPlaneCapabilities2KHR* pCapabilities),
        (physicalDevice, pDisplayPlaneInfo, pCapabilities) )
#endif

#if defined(VK_KHR_fragment_shading_rate)
    SIR_VK_PROC( getPhysicalDeviceFragmentShadingRatesKHR, vkGetPhysicalDeviceFragmentShadingRatesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates),
        (physicalDevice, pFragmentShadingRateCount, pFragmentShadingRates) )
#endif

#if defined(VK_KHR_video_encode_queue)
    SIR_VK_PROC( getPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR, vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR* pQualityLevelInfo, VkVideoEncodeQualityLevelPropertiesKHR* pQualityLevelProperties),
        (physicalDevice, pQualityLevelInfo, pQualityLevelProperties) )
#endif

#if defined(VK_KHR_cooperative_matrix)
    SIR_VK_PROC( getPhysicalDeviceCooperativeMatrixPropertiesKHR, vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesKHR* pProperties),
        (physicalDevice, pPropertyCount, pProperties) )
#endif

#if defined(VK_KHR_calibrated_timestamps)
    SIR_VK_PROC( getPhysicalDeviceCalibrateableTimeDomainsKHR, vkGetPhysicalDeviceCalibrateableTimeDomainsKHR, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains),
        (physicalDevice, pTimeDomainCount, pTimeDomains) )
#endif

#if defined(VK_EXT_debug_report)
    SIR_VK_PROC( createDebugReportCallbackEXT, vkCreateDebugReportCallbackEXT, VkResult,
        (VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback),
        (instance, pCreateInfo, pAllocator, pCallback) )
    SIR_VK_PROC( destroyDebugReportCallbackEXT, vkDestroyDebugReportCallbackEXT, void,
        (VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator),
        (instance, callback, pAllocator) )
    SIR_VK_PROC( debugReportMessageEXT, vkDebugReportMessageEXT, void,
        (VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage),
        (instance, flags, objectType, object, location, messageCode, pLayerPrefix, pMessage) )
#endif

#if defined(VK_NV_external_memory_capabilities)
    SIR_VK_PROC( getPhysicalDeviceExternalImageFormatPropertiesNV, vkGetPhysicalDeviceExternalImageFormatPropertiesNV, VkResult,
        (VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkExternalMemoryHandleTypeFlagsNV externalHandleType, VkExternalImageFormatPropertiesNV* pExternalImageFormatProperties),
        (physicalDevice, format, type, tiling, usage, flags, externalHandleType, pExternalImageFormatProperties) )
#endif

#if defined(VK_EXT_direct_mode_display)
    SIR_VK_PROC( releaseDisplayEXT, vkReleaseDisplayEXT, VkResult,
        (VkPhysicalDevice physicalDevice, VkDisplayKHR display),
        (physicalDevice, display) )
#endif

#if defined(VK_EXT_display_surface_counter)
    SIR_VK_PROC( getPhysicalDeviceSurfaceCapabilities2EXT, vkGetPhysicalDeviceSurfaceCapabilities2EXT, VkResult,
        (VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilities2EXT* pSurfaceCapabilities),
        (physicalDevice, surface, pSurfaceCapabilities) )
#endif

#if defined(VK_EXT_debug_utils)
    SIR_VK_PROC( createDebugUtilsMessengerEXT, vkCreateDebugUtilsMessengerEXT, VkResult,
        (VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger),
        (instance, pCreateInfo, pAllocator, pMessenger) )
    SIR_VK_PROC( destroyDebugUtilsMessengerEXT, vkDestroyDebugUtilsMessengerEXT, void,
        (VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator),
        (instance, messenger, pAllocator) )
    SIR_VK_PROC( submitDebugUtilsMessageEXT, vkSubmitDebugUtilsMessageEXT, void,
        (VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData),
        (instance, messageSeverity, messageTypes, pCallbackData) )
#endif

#if defined(VK_EXT_descriptor_heap)
    SIR_VK_PROC( getPhysicalDeviceDescriptorSizeEXT, vkGetPhysicalDeviceDescriptorSizeEXT, VkDeviceSize,
        (VkPhysicalDevice                                    physicalDevice, VkDescriptorType                                    descriptorType),
        (physicalDevice, descriptorType) )
#endif

#if defined(VK_EXT_sample_locations)
    SIR_VK_PROC( getPhysicalDeviceMultisamplePropertiesEXT, vkGetPhysicalDeviceMultisamplePropertiesEXT, void,
        (VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties),
        (physicalDevice, samples, pMultisampleProperties) )
#endif

#if defined(VK_EXT_calibrated_timestamps)
    SIR_VK_PROC( getPhysicalDeviceCalibrateableTimeDomainsEXT, vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains),
        (physicalDevice, pTimeDomainCount, pTimeDomains) )
#endif

#if defined(VK_EXT_tooling_info)
    SIR_VK_PROC( getPhysicalDeviceToolPropertiesEXT, vkGetPhysicalDeviceToolPropertiesEXT, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties),
        (physicalDevice, pToolCount, pToolProperties) )
#endif

#if defined(VK_NV_cooperative_matrix)
    SIR_VK_PROC( getPhysicalDeviceCooperativeMatrixPropertiesNV, vkGetPhysicalDeviceCooperativeMatrixPropertiesNV, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesNV* pProperties),
        (physicalDevice, pPropertyCount, pProperties) )
#endif

#if defined(VK_NV_coverage_reduction_mode)
    SIR_VK_PROC( getPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV, vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pCombinationCount, VkFramebufferMixedSamplesCombinationNV* pCombinations),
        (physicalDevice, pCombinationCount, pCombinations) )
#endif

#if defined(VK_EXT_headless_surface)
    SIR_VK_PROC( createHeadlessSurfaceEXT, vkCreateHeadlessSurfaceEXT, VkResult,
        (VkInstance instance, const VkHeadlessSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_EXT_acquire_drm_display)
    SIR_VK_PROC( acquireDrmDisplayEXT, vkAcquireDrmDisplayEXT, VkResult,
        (VkPhysicalDevice physicalDevice, int32_t drmFd, VkDisplayKHR display),
        (physicalDevice, drmFd, display) )
    SIR_VK_PROC( getDrmDisplayEXT, vkGetDrmDisplayEXT, VkResult,
        (VkPhysicalDevice physicalDevice, int32_t drmFd, uint32_t connectorId, VkDisplayKHR* display),
        (physicalDevice, drmFd, connectorId, display) )
#endif

#if defined(VK_ARM_tensors)
    SIR_VK_PROC( getPhysicalDeviceExternalTensorPropertiesARM, vkGetPhysicalDeviceExternalTensorPropertiesARM, void,
        (VkPhysicalDevice                             physicalDevice, const VkPhysicalDeviceExternalTensorInfoARM* pExternalTensorInfo, VkExternalTensorPropertiesARM*               pExternalTensorProperties),
        (physicalDevice, pExternalTensorInfo, pExternalTensorProperties) )
#endif

#if defined(VK_NV_optical_flow)
    SIR_VK_PROC( getPhysicalDeviceOpticalFlowImageFormatsNV, vkGetPhysicalDeviceOpticalFlowImageFormatsNV, VkResult,
        (VkPhysicalDevice physicalDevice, const VkOpticalFlowImageFormatInfoNV* pOpticalFlowImageFormatInfo, uint32_t* pFormatCount, VkOpticalFlowImageFormatPropertiesNV* pImageFormatProperties),
        (physicalDevice, pOpticalFlowImageFormatInfo, pFormatCount, pImageFormatProperties) )
#endif

#if defined(VK_NV_cooperative_vector)
    SIR_VK_PROC( getPhysicalDeviceCooperativeVectorPropertiesNV, vkGetPhysicalDeviceCooperativeVectorPropertiesNV, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeVectorPropertiesNV* pProperties),
        (physicalDevice, pPropertyCount, pProperties) )
#endif

#if defined(VK_ARM_data_graph)
    SIR_VK_PROC( getPhysicalDeviceQueueFamilyDataGraphPropertiesARM, vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM, VkResult,
        (VkPhysicalDevice                     physicalDevice, uint32_t                             queueFamilyIndex, uint32_t*                            pQueueFamilyDataGraphPropertyCount, VkQueueFamilyDataGraphPropertiesARM* pQueueFamilyDataGraphProperties),
        (physicalDevice, queueFamilyIndex, pQueueFamilyDataGraphPropertyCount, pQueueFamilyDataGraphProperties) )
    SIR_VK_PROC( getPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM, vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM, void,
        (VkPhysicalDevice                                 physicalDevice, const VkPhysicalDeviceQueueFamilyDataGraphProcessingEngineInfoARM* pQueueFamilyDataGraphProcessingEngineInfo, VkQueueFamilyDataGraphProcessingEnginePropertiesARM*               pQueueFamilyDataGraphProcessingEngineProperties),
        (physicalDevice, pQueueFamilyDataGraphProcessingEngineInfo, pQueueFamilyDataGraphProcessingEngineProperties) )
#endif

#if defined(VK_ARM_data_graph_instruction_set_tosa)
    SIR_VK_PROC( getPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM, vkGetPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM, VkResult,
        (VkPhysicalDevice                                 physicalDevice, uint32_t                                         queueFamilyIndex, const VkQueueFamilyDataGraphPropertiesARM*       pQueueFamilyDataGraphProperties, VkBaseOutStructure*        pProperties),
        (physicalDevice, queueFamilyIndex, pQueueFamilyDataGraphProperties, pProperties) )
#endif

#if defined(VK_NV_cooperative_matrix2)
    SIR_VK_PROC( getPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV, vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixFlexibleDimensionsPropertiesNV* pProperties),
        (physicalDevice, pPropertyCount, pProperties) )
#endif

#if defined(VK_ARM_performance_counters_by_region)
    SIR_VK_PROC( enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM, vkEnumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t* pCounterCount, VkPerformanceCounterARM* pCounters, VkPerformanceCounterDescriptionARM* pCounterDescriptions),
        (physicalDevice, queueFamilyIndex, pCounterCount, pCounters, pCounterDescriptions) )
#endif

#if defined(VK_ARM_shader_instrumentation)
    SIR_VK_PROC( enumeratePhysicalDeviceShaderInstrumentationMetricsARM, vkEnumeratePhysicalDeviceShaderInstrumentationMetricsARM, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t* pDescriptionCount, VkShaderInstrumentationMetricDescriptionARM* pDescriptions),
        (physicalDevice, pDescriptionCount, pDescriptions) )
#endif

#if defined(VK_ARM_data_graph_optical_flow)
    SIR_VK_PROC( getPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM, vkGetPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM, VkResult,
        (VkPhysicalDevice                                physicalDevice, uint32_t                                        queueFamilyIndex, const VkQueueFamilyDataGraphPropertiesARM*      pQueueFamilyDataGraphProperties, const VkDataGraphOpticalFlowImageFormatInfoARM* pOpticalFlowImageFormatInfo, uint32_t*                 pFormatCount, VkDataGraphOpticalFlowImageFormatPropertiesARM* pImageFormatProperties),
        (physicalDevice, queueFamilyIndex, pQueueFamilyDataGraphProperties, pOpticalFlowImageFormatInfo, pFormatCount, pImageFormatProperties) )
#endif

#if defined(VK_EXT_cooperative_matrix_maintenance1)
    SIR_VK_PROC( getPhysicalDeviceCooperativeMatrixProperties2EXT, vkGetPhysicalDeviceCooperativeMatrixProperties2EXT, VkResult,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceCooperativeMatrixInfo2EXT* pCooperativeMatrixInfo, uint32_t* pPropertyCount, VkCooperativeMatrixProperties2EXT* pProperties),
        (physicalDevice, pCooperativeMatrixInfo, pPropertyCount, pProperties) )
#endif

#if defined(VK_KHR_android_surface)
    SIR_VK_PROC( createAndroidSurfaceKHR, vkCreateAndroidSurfaceKHR, VkResult,
        (VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_FUCHSIA_imagepipe_surface)
    SIR_VK_PROC( createImagePipeSurfaceFUCHSIA, vkCreateImagePipeSurfaceFUCHSIA, VkResult,
        (VkInstance instance, const VkImagePipeSurfaceCreateInfoFUCHSIA* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_MVK_ios_surface)
    SIR_VK_PROC( createIOSSurfaceMVK, vkCreateIOSSurfaceMVK, VkResult,
        (VkInstance instance, const VkIOSSurfaceCreateInfoMVK* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_MVK_macos_surface)
    SIR_VK_PROC( createMacOSSurfaceMVK, vkCreateMacOSSurfaceMVK, VkResult,
        (VkInstance instance, const VkMacOSSurfaceCreateInfoMVK* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_EXT_metal_surface)
    SIR_VK_PROC( createMetalSurfaceEXT, vkCreateMetalSurfaceEXT, VkResult,
        (VkInstance instance, const VkMetalSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_NN_vi_surface)
    SIR_VK_PROC( createViSurfaceNN, vkCreateViSurfaceNN, VkResult,
        (VkInstance instance, const VkViSurfaceCreateInfoNN* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_KHR_wayland_surface)
    SIR_VK_PROC( createWaylandSurfaceKHR, vkCreateWaylandSurfaceKHR, VkResult,
        (VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
    SIR_VK_PROC( getPhysicalDeviceWaylandPresentationSupportKHR, vkGetPhysicalDeviceWaylandPresentationSupportKHR, VkBool32,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, struct wl_display* display),
        (physicalDevice, queueFamilyIndex, display) )
#endif

#if defined(VK_KHR_win32_surface)
    SIR_VK_PROC( createWin32SurfaceKHR, vkCreateWin32SurfaceKHR, VkResult,
        (VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
    SIR_VK_PROC( getPhysicalDeviceWin32PresentationSupportKHR, vkGetPhysicalDeviceWin32PresentationSupportKHR, VkBool32,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex),
        (physicalDevice, queueFamilyIndex) )
#endif

#if defined(VK_EXT_full_screen_exclusive)
    SIR_VK_PROC( getPhysicalDeviceSurfacePresentModes2EXT, vkGetPhysicalDeviceSurfacePresentModes2EXT, VkResult,
        (VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes),
        (physicalDevice, pSurfaceInfo, pPresentModeCount, pPresentModes) )
#endif

#if defined(VK_NV_acquire_winrt_display)
    SIR_VK_PROC( acquireWinrtDisplayNV, vkAcquireWinrtDisplayNV, VkResult,
        (VkPhysicalDevice physicalDevice, VkDisplayKHR display),
        (physicalDevice, display) )
    SIR_VK_PROC( getWinrtDisplayNV, vkGetWinrtDisplayNV, VkResult,
        (VkPhysicalDevice physicalDevice, uint32_t deviceRelativeId, VkDisplayKHR* pDisplay),
        (physicalDevice, deviceRelativeId, pDisplay) )
#endif

#if defined(VK_KHR_xcb_surface)
    SIR_VK_PROC( createXcbSurfaceKHR, vkCreateXcbSurfaceKHR, VkResult,
        (VkInstance instance, const VkXcbSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
    SIR_VK_PROC( getPhysicalDeviceXcbPresentationSupportKHR, vkGetPhysicalDeviceXcbPresentationSupportKHR, VkBool32,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, xcb_connection_t* connection, xcb_visualid_t visual_id),
        (physicalDevice, queueFamilyIndex, connection, visual_id) )
#endif

#if defined(VK_KHR_xlib_surface)
    SIR_VK_PROC( createXlibSurfaceKHR, vkCreateXlibSurfaceKHR, VkResult,
        (VkInstance instance, const VkXlibSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
    SIR_VK_PROC( getPhysicalDeviceXlibPresentationSupportKHR, vkGetPhysicalDeviceXlibPresentationSupportKHR, VkBool32,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, Display* dpy, VisualID visualID),
        (physicalDevice, queueFamilyIndex, dpy, visualID) )
#endif

#if defined(VK_EXT_directfb_surface)
    SIR_VK_PROC( createDirectFBSurfaceEXT, vkCreateDirectFBSurfaceEXT, VkResult,
        (VkInstance instance, const VkDirectFBSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
    SIR_VK_PROC( getPhysicalDeviceDirectFBPresentationSupportEXT, vkGetPhysicalDeviceDirectFBPresentationSupportEXT, VkBool32,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, IDirectFB* dfb),
        (physicalDevice, queueFamilyIndex, dfb) )
#endif

#if defined(VK_EXT_acquire_xlib_display)
    SIR_VK_PROC( acquireXlibDisplayEXT, vkAcquireXlibDisplayEXT, VkResult,
        (VkPhysicalDevice physicalDevice, Display* dpy, VkDisplayKHR display),
        (physicalDevice, dpy, display) )
    SIR_VK_PROC( getRandROutputDisplayEXT, vkGetRandROutputDisplayEXT, VkResult,
        (VkPhysicalDevice physicalDevice, Display* dpy, RROutput rrOutput, VkDisplayKHR* pDisplay),
        (physicalDevice, dpy, rrOutput, pDisplay) )
#endif

#if defined(VK_GGP_stream_descriptor_surface)
    SIR_VK_PROC( createStreamDescriptorSurfaceGGP, vkCreateStreamDescriptorSurfaceGGP, VkResult,
        (VkInstance instance, const VkStreamDescriptorSurfaceCreateInfoGGP* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_QNX_screen_surface)
    SIR_VK_PROC( createScreenSurfaceQNX, vkCreateScreenSurfaceQNX, VkResult,
        (VkInstance instance, const VkScreenSurfaceCreateInfoQNX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
    SIR_VK_PROC( getPhysicalDeviceScreenPresentationSupportQNX, vkGetPhysicalDeviceScreenPresentationSupportQNX, VkBool32,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, struct _screen_window* window),
        (physicalDevice, queueFamilyIndex, window) )
#endif

#if defined(VK_OHOS_surface)
    SIR_VK_PROC( createSurfaceOHOS, vkCreateSurfaceOHOS, VkResult,
        (VkInstance instance, const VkSurfaceCreateInfoOHOS* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
#endif

#if defined(VK_SEC_ubm_surface)
    SIR_VK_PROC( createUbmSurfaceSEC, vkCreateUbmSurfaceSEC, VkResult,
        (VkInstance instance, const VkUbmSurfaceCreateInfoSEC* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface),
        (instance, pCreateInfo, pAllocator, pSurface) )
    SIR_VK_PROC( getPhysicalDeviceUbmPresentationSupportSEC, vkGetPhysicalDeviceUbmPresentationSupportSEC, VkBool32,
        (VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, struct ubm_device* device),
        (physicalDevice, queueFamilyIndex, device) )
#endif

    // Device procedures.
#if defined(VK_VERSION_1_0)
    SIR_VK_PROC( destroyDevice, vkDestroyDevice, void,
        (const VkAllocationCallbacks* pAllocator),
        (m_Device, pAllocator) )
    SIR_VK_PROC( getDeviceQueue, vkGetDeviceQueue, void,
        (uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue),
        (m_Device, queueFamilyIndex, queueIndex, pQueue) )
    SIR_VK_PROC( queueSubmit, vkQueueSubmit, VkResult,
        (VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence),
        (queue, submitCount, pSubmits, fence) )
    SIR_VK_PROC( queueWaitIdle, vkQueueWaitIdle, VkResult,
        (VkQueue queue),
        (queue) )
    SIR_VK_PROC( deviceWaitIdle, vkDeviceWaitIdle, VkResult,
        (),
        (m_Device) )
    SIR_VK_PROC( allocateMemory, vkAllocateMemory, VkResult,
        (const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory),
        (m_Device, pAllocateInfo, pAllocator, pMemory) )
    SIR_VK_PROC( freeMemory, vkFreeMemory, void,
        (VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator),
        (m_Device, memory, pAllocator) )
    SIR_VK_PROC( mapMemory, vkMapMemory, VkResult,
        (VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData),
        (m_Device, memory, offset, size, flags, ppData) )
    SIR_VK_PROC( unmapMemory, vkUnmapMemory, void,
        (VkDeviceMemory memory),
        (m_Device, memory) )
    SIR_VK_PROC( flushMappedMemoryRanges, vkFlushMappedMemoryRanges, VkResult,
        (uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges),
        (m_Device, memoryRangeCount, pMemoryRanges) )
    SIR_VK_PROC( invalidateMappedMemoryRanges, vkInvalidateMappedMemoryRanges, VkResult,
        (uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges),
        (m_Device, memoryRangeCount, pMemoryRanges) )
    SIR_VK_PROC( getDeviceMemoryCommitment, vkGetDeviceMemoryCommitment, void,
        (VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes),
        (m_Device, memory, pCommittedMemoryInBytes) )
    SIR_VK_PROC( bindBufferMemory, vkBindBufferMemory, VkResult,
        (VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset),
        (m_Device, buffer, memory, memoryOffset) )
    SIR_VK_PROC( bindImageMemory, vkBindImageMemory, VkResult,
        (VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset),
        (m_Device, image, memory, memoryOffset) )
    SIR_VK_PROC( getBufferMemoryRequirements, vkGetBufferMemoryRequirements, void,
        (VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements),
        (m_Device, buffer, pMemoryRequirements) )
    SIR_VK_PROC( getImageMemoryRequirements, vkGetImageMemoryRequirements, void,
        (VkImage image, VkMemoryRequirements* pMemoryRequirements),
        (m_Device, image, pMemoryRequirements) )
    SIR_VK_PROC( getImageSparseMemoryRequirements, vkGetImageSparseMemoryRequirements, void,
        (VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements),
        (m_Device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements) )
    SIR_VK_PROC( queueBindSparse, vkQueueBindSparse, VkResult,
        (VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence),
        (queue, bindInfoCount, pBindInfo, fence) )
    SIR_VK_PROC( createFence, vkCreateFence, VkResult,
        (const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence),
        (m_Device, pCreateInfo, pAllocator, pFence) )
    SIR_VK_PROC( destroyFence, vkDestroyFence, void,
        (VkFence fence, const VkAllocationCallbacks* pAllocator),
        (m_Device, fence, pAllocator) )
    SIR_VK_PROC( resetFences, vkResetFences, VkResult,
        (uint32_t fenceCount, const VkFence* pFences),
        (m_Device, fenceCount, pFences) )
    SIR_VK_PROC( getFenceStatus, vkGetFenceStatus, VkResult,
        (VkFence fence),
        (m_Device, fence) )
    SIR_VK_PROC( waitForFences, vkWaitForFences, VkResult,
        (uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout),
        (m_Device, fenceCount, pFences, waitAll, timeout) )
    SIR_VK_PROC( createSemaphore, vkCreateSemaphore, VkResult,
        (const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore),
        (m_Device, pCreateInfo, pAllocator, pSemaphore) )
    SIR_VK_PROC( destroySemaphore, vkDestroySemaphore, void,
        (VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator),
        (m_Device, semaphore, pAllocator) )
    SIR_VK_PROC( createQueryPool, vkCreateQueryPool, VkResult,
        (const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool),
        (m_Device, pCreateInfo, pAllocator, pQueryPool) )
    SIR_VK_PROC( destroyQueryPool, vkDestroyQueryPool, void,
        (VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator),
        (m_Device, queryPool, pAllocator) )
    SIR_VK_PROC( getQueryPoolResults, vkGetQueryPoolResults, VkResult,
        (VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags),
        (m_Device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags) )
    SIR_VK_PROC( createBuffer, vkCreateBuffer, VkResult,
        (const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer),
        (m_Device, pCreateInfo, pAllocator, pBuffer) )
    SIR_VK_PROC( destroyBuffer, vkDestroyBuffer, void,
        (VkBuffer buffer, const VkAllocationCallbacks* pAllocator),
        (m_Device, buffer, pAllocator) )
    SIR_VK_PROC( createImage, vkCreateImage, VkResult,
        (const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage),
        (m_Device, pCreateInfo, pAllocator, pImage) )
    SIR_VK_PROC( destroyImage, vkDestroyImage, void,
        (VkImage image, const VkAllocationCallbacks* pAllocator),
        (m_Device, image, pAllocator) )
    SIR_VK_PROC( getImageSubresourceLayout, vkGetImageSubresourceLayout, void,
        (VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout),
        (m_Device, image, pSubresource, pLayout) )
    SIR_VK_PROC( createImageView, vkCreateImageView, VkResult,
        (const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView),
        (m_Device, pCreateInfo, pAllocator, pView) )
    SIR_VK_PROC( destroyImageView, vkDestroyImageView, void,
        (VkImageView imageView, const VkAllocationCallbacks* pAllocator),
        (m_Device, imageView, pAllocator) )
    SIR_VK_PROC( createCommandPool, vkCreateCommandPool, VkResult,
        (const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool),
        (m_Device, pCreateInfo, pAllocator, pCommandPool) )
    SIR_VK_PROC( destroyCommandPool, vkDestroyCommandPool, void,
        (VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator),
        (m_Device, commandPool, pAllocator) )
    SIR_VK_PROC( resetCommandPool, vkResetCommandPool, VkResult,
        (VkCommandPool commandPool, VkCommandPoolResetFlags flags),
        (m_Device, commandPool, flags) )
    SIR_VK_PROC( allocateCommandBuffers, vkAllocateCommandBuffers, VkResult,
        (const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers),
        (m_Device, pAllocateInfo, pCommandBuffers) )
    SIR_VK_PROC( freeCommandBuffers, vkFreeCommandBuffers, void,
        (VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers),
        (m_Device, commandPool, commandBufferCount, pCommandBuffers) )
    SIR_VK_PROC( beginCommandBuffer, vkBeginCommandBuffer, VkResult,
        (VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo),
        (commandBuffer, pBeginInfo) )
    SIR_VK_PROC( endCommandBuffer, vkEndCommandBuffer, VkResult,
        (VkCommandBuffer commandBuffer),
        (commandBuffer) )
    SIR_VK_PROC( resetCommandBuffer, vkResetCommandBuffer, VkResult,
        (VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags),
        (commandBuffer, flags) )
    SIR_VK_PROC( cmdCopyBuffer, vkCmdCopyBuffer, void,
        (VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions),
        (commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions) )
    SIR_VK_PROC( cmdCopyImage, vkCmdCopyImage, void,
        (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions),
        (commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions) )
    SIR_VK_PROC( cmdCopyBufferToImage, vkCmdCopyBufferToImage, void,
        (VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions),
        (commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions) )
    SIR_VK_PROC( cmdCopyImageToBuffer, vkCmdCopyImageToBuffer, void,
        (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions),
        (commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions) )
    SIR_VK_PROC( cmdUpdateBuffer, vkCmdUpdateBuffer, void,
        (VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData),
        (commandBuffer, dstBuffer, dstOffset, dataSize, pData) )
    SIR_VK_PROC( cmdFillBuffer, vkCmdFillBuffer, void,
        (VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data),
        (commandBuffer, dstBuffer, dstOffset, size, data) )
    SIR_VK_PROC( cmdPipelineBarrier, vkCmdPipelineBarrier, void,
        (VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers),
        (commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers) )
    SIR_VK_PROC( cmdBeginQuery, vkCmdBeginQuery, void,
        (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags),
        (commandBuffer, queryPool, query, flags) )
    SIR_VK_PROC( cmdEndQuery, vkCmdEndQuery, void,
        (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query),
        (commandBuffer, queryPool, query) )
    SIR_VK_PROC( cmdResetQueryPool, vkCmdResetQueryPool, void,
        (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount),
        (commandBuffer, queryPool, firstQuery, queryCount) )
    SIR_VK_PROC( cmdWriteTimestamp, vkCmdWriteTimestamp, void,
        (VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query),
        (commandBuffer, pipelineStage, queryPool, query) )
    SIR_VK_PROC( cmdCopyQueryPoolResults, vkCmdCopyQueryPoolResults, void,
        (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags),
        (commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags) )
    SIR_VK_PROC( cmdExecuteCommands, vkCmdExecuteCommands, void,
        (VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers),
        (commandBuffer, commandBufferCount, pCommandBuffers) )
    SIR_VK_PROC( createEvent, vkCreateEvent, VkResult,
        (const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent),
        (m_Device, pCreateInfo, pAllocator, pEvent) )
    SIR_VK_PROC( destroyEvent, vkDestroyEvent, void,
        (VkEvent event, const VkAllocationCallbacks* pAllocator),
        (m_Device, event, pAllocator) )
    SIR_VK_PROC( getEventStatus, vkGetEventStatus, VkResult,
        (VkEvent event),
        (m_Device, event) )
    SIR_VK_PROC( setEvent, vkSetEvent, VkResult,
        (VkEvent event),
        (m_Device, event) )
    SIR_VK_PROC( resetEvent, vkResetEvent, VkResult,
        (VkEvent event),
        (m_Device, event) )
    SIR_VK_PROC( createBufferView, vkCreateBufferView, VkResult,
        (const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView),
        (m_Device, pCreateInfo, pAllocator, pView) )
    SIR_VK_PROC( destroyBufferView, vkDestroyBufferView, void,
        (VkBufferView bufferView, const VkAllocationCallbacks* pAllocator),
        (m_Device, bufferView, pAllocator) )
    SIR_VK_PROC( createShaderModule, vkCreateShaderModule, VkResult,
        (const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule),
        (m_Device, pCreateInfo, pAllocator, pShaderModule) )
    SIR_VK_PROC( destroyShaderModule, vkDestroyShaderModule, void,
        (VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator),
        (m_Device, shaderModule, pAllocator) )
    SIR_VK_PROC( createPipelineCache, vkCreatePipelineCache, VkResult,
        (const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache),
        (m_Device, pCreateInfo, pAllocator, pPipelineCache) )
    SIR_VK_PROC( destroyPipelineCache, vkDestroyPipelineCache, void,
        (VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator),
        (m_Device, pipelineCache, pAllocator) )
    SIR_VK_PROC( getPipelineCacheData, vkGetPipelineCacheData, VkResult,
        (VkPipelineCache pipelineCache, size_t* pDataSize, void* pData),
        (m_Device, pipelineCache, pDataSize, pData) )
    SIR_VK_PROC( mergePipelineCaches, vkMergePipelineCaches, VkResult,
        (VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches),
        (m_Device, dstCache, srcCacheCount, pSrcCaches) )
    SIR_VK_PROC( createComputePipelines, vkCreateComputePipelines, VkResult,
        (VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines),
        (m_Device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines) )
    SIR_VK_PROC( destroyPipeline, vkDestroyPipeline, void,
        (VkPipeline pipeline, const VkAllocationCallbacks* pAllocator),
        (m_Device, pipeline, pAllocator) )
    SIR_VK_PROC( createPipelineLayout, vkCreatePipelineLayout, VkResult,
        (const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout),
        (m_Device, pCreateInfo, pAllocator, pPipelineLayout) )
    SIR_VK_PROC( destroyPipelineLayout, vkDestroyPipelineLayout, void,
        (VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator),
        (m_Device, pipelineLayout, pAllocator) )
    SIR_VK_PROC( createSampler, vkCreateSampler, VkResult,
        (const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler),
        (m_Device, pCreateInfo, pAllocator, pSampler) )
    SIR_VK_PROC( destroySampler, vkDestroySampler, void,
        (VkSampler sampler, const VkAllocationCallbacks* pAllocator),
        (m_Device, sampler, pAllocator) )
    SIR_VK_PROC( createDescriptorSetLayout, vkCreateDescriptorSetLayout, VkResult,
        (const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout),
        (m_Device, pCreateInfo, pAllocator, pSetLayout) )
    SIR_VK_PROC( destroyDescriptorSetLayout, vkDestroyDescriptorSetLayout, void,
        (VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator),
        (m_Device, descriptorSetLayout, pAllocator) )
    SIR_VK_PROC( createDescriptorPool, vkCreateDescriptorPool, VkResult,
        (const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool),
        (m_Device, pCreateInfo, pAllocator, pDescriptorPool) )
    SIR_VK_PROC( destroyDescriptorPool, vkDestroyDescriptorPool, void,
        (VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator),
        (m_Device, descriptorPool, pAllocator) )
    SIR_VK_PROC( resetDescriptorPool, vkResetDescriptorPool, VkResult,
        (VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags),
        (m_Device, descriptorPool, flags) )
    SIR_VK_PROC( allocateDescriptorSets, vkAllocateDescriptorSets, VkResult,
        (const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets),
        (m_Device, pAllocateInfo, pDescriptorSets) )
    SIR_VK_PROC( freeDescriptorSets, vkFreeDescriptorSets, VkResult,
        (VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets),
        (m_Device, descriptorPool, descriptorSetCount, pDescriptorSets) )
    SIR_VK_PROC( updateDescriptorSets, vkUpdateDescriptorSets, void,
        (uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies),
        (m_Device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies) )
    SIR_VK_PROC( cmdBindPipeline, vkCmdBindPipeline, void,
        (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline),
        (commandBuffer, pipelineBindPoint, pipeline) )
    SIR_VK_PROC( cmdBindDescriptorSets, vkCmdBindDescriptorSets, void,
        (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets),
        (commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets) )
    SIR_VK_PROC( cmdClearColorImage, vkCmdClearColorImage, void,
        (VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges),
        (commandBuffer, image, imageLayout, pColor, rangeCount, pRanges) )
    SIR_VK_PROC( cmdDispatch, vkCmdDispatch, void,
        (VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ),
        (commandBuffer, groupCountX, groupCountY, groupCountZ) )
    SIR_VK_PROC( cmdDispatchIndirect, vkCmdDispatchIndirect, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset),
        (commandBuffer, buffer, offset) )
    SIR_VK_PROC( cmdSetEvent, vkCmdSetEvent, void,
        (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask),
        (commandBuffer, event, stageMask) )
    SIR_VK_PROC( cmdResetEvent, vkCmdResetEvent, void,
        (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask),
        (commandBuffer, event, stageMask) )
    SIR_VK_PROC( cmdWaitEvents, vkCmdWaitEvents, void,
        (VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers),
        (commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers) )
    SIR_VK_PROC( cmdPushConstants, vkCmdPushConstants, void,
        (VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues),
        (commandBuffer, layout, stageFlags, offset, size, pValues) )
    SIR_VK_PROC( createGraphicsPipelines, vkCreateGraphicsPipelines, VkResult,
        (VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines),
        (m_Device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines) )
    SIR_VK_PROC( createFramebuffer, vkCreateFramebuffer, VkResult,
        (const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer),
        (m_Device, pCreateInfo, pAllocator, pFramebuffer) )
    SIR_VK_PROC( destroyFramebuffer, vkDestroyFramebuffer, void,
        (VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator),
        (m_Device, framebuffer, pAllocator) )
    SIR_VK_PROC( createRenderPass, vkCreateRenderPass, VkResult,
        (const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass),
        (m_Device, pCreateInfo, pAllocator, pRenderPass) )
    SIR_VK_PROC( destroyRenderPass, vkDestroyRenderPass, void,
        (VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator),
        (m_Device, renderPass, pAllocator) )
    SIR_VK_PROC( getRenderAreaGranularity, vkGetRenderAreaGranularity, void,
        (VkRenderPass renderPass, VkExtent2D* pGranularity),
        (m_Device, renderPass, pGranularity) )
    SIR_VK_PROC( cmdSetViewport, vkCmdSetViewport, void,
        (VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports),
        (commandBuffer, firstViewport, viewportCount, pViewports) )
    SIR_VK_PROC( cmdSetScissor, vkCmdSetScissor, void,
        (VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors),
        (commandBuffer, firstScissor, scissorCount, pScissors) )
    SIR_VK_PROC( cmdSetLineWidth, vkCmdSetLineWidth, void,
        (VkCommandBuffer commandBuffer, float lineWidth),
        (commandBuffer, lineWidth) )
    SIR_VK_PROC( cmdSetDepthBias, vkCmdSetDepthBias, void,
        (VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor),
        (commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor) )
    SIR_VK_PROC( cmdSetBlendConstants, vkCmdSetBlendConstants, void,
        (VkCommandBuffer commandBuffer, const float blendConstants[4]),
        (commandBuffer, blendConstants) )
    SIR_VK_PROC( cmdSetDepthBounds, vkCmdSetDepthBounds, void,
        (VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds),
        (commandBuffer, minDepthBounds, maxDepthBounds) )
    SIR_VK_PROC( cmdSetStencilCompareMask, vkCmdSetStencilCompareMask, void,
        (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask),
        (commandBuffer, faceMask, compareMask) )
    SIR_VK_PROC( cmdSetStencilWriteMask, vkCmdSetStencilWriteMask, void,
        (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask),
        (commandBuffer, faceMask, writeMask) )
    SIR_VK_PROC( cmdSetStencilReference, vkCmdSetStencilReference, void,
        (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference),
        (commandBuffer, faceMask, reference) )
    SIR_VK_PROC( cmdBindIndexBuffer, vkCmdBindIndexBuffer, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType),
        (commandBuffer, buffer, offset, indexType) )
    SIR_VK_PROC( cmdBindVertexBuffers, vkCmdBindVertexBuffers, void,
        (VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets),
        (commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets) )
    SIR_VK_PROC( cmdDraw, vkCmdDraw, void,
        (VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance),
        (commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance) )
    SIR_VK_PROC( cmdDrawIndexed, vkCmdDrawIndexed, void,
        (VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance),
        (commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance) )
    SIR_VK_PROC( cmdDrawIndirect, vkCmdDrawIndirect, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride),
        (commandBuffer, buffer, offset, drawCount, stride) )
    SIR_VK_PROC( cmdDrawIndexedIndirect, vkCmdDrawIndexedIndirect, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride),
        (commandBuffer, buffer, offset, drawCount, stride) )
    SIR_VK_PROC( cmdBlitImage, vkCmdBlitImage, void,
        (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter),
        (commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter) )
    SIR_VK_PROC( cmdClearDepthStencilImage, vkCmdClearDepthStencilImage, void,
        (VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges),
        (commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges) )
    SIR_VK_PROC( cmdClearAttachments, vkCmdClearAttachments, void,
        (VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects),
        (commandBuffer, attachmentCount, pAttachments, rectCount, pRects) )
    SIR_VK_PROC( cmdResolveImage, vkCmdResolveImage, void,
        (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions),
        (commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions) )
    SIR_VK_PROC( cmdBeginRenderPass, vkCmdBeginRenderPass, void,
        (VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents),
        (commandBuffer, pRenderPassBegin, contents) )
    SIR_VK_PROC( cmdNextSubpass, vkCmdNextSubpass, void,
        (VkCommandBuffer commandBuffer, VkSubpassContents contents),
        (commandBuffer, contents) )
    SIR_VK_PROC( cmdEndRenderPass, vkCmdEndRenderPass, void,
        (VkCommandBuffer commandBuffer),
        (commandBuffer) )
#endif

#if defined(VK_VERSION_1_1)
    SIR_VK_PROC( bindBufferMemory2, vkBindBufferMemory2, VkResult,
        (uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos),
        (m_Device, bindInfoCount, pBindInfos) )
    SIR_VK_PROC( bindImageMemory2, vkBindImageMemory2, VkResult,
        (uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos),
        (m_Device, bindInfoCount, pBindInfos) )
    SIR_VK_PROC( getDeviceGroupPeerMemoryFeatures, vkGetDeviceGroupPeerMemoryFeatures, void,
        (uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures),
        (m_Device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures) )
    SIR_VK_PROC( cmdSetDeviceMask, vkCmdSetDeviceMask, void,
        (VkCommandBuffer commandBuffer, uint32_t deviceMask),
        (commandBuffer, deviceMask) )
    SIR_VK_PROC( getImageMemoryRequirements2, vkGetImageMemoryRequirements2, void,
        (const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( getBufferMemoryRequirements2, vkGetBufferMemoryRequirements2, void,
        (const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( getImageSparseMemoryRequirements2, vkGetImageSparseMemoryRequirements2, void,
        (const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements),
        (m_Device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements) )
    SIR_VK_PROC( trimCommandPool, vkTrimCommandPool, void,
        (VkCommandPool commandPool, VkCommandPoolTrimFlags flags),
        (m_Device, commandPool, flags) )
    SIR_VK_PROC( getDeviceQueue2, vkGetDeviceQueue2, void,
        (const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue),
        (m_Device, pQueueInfo, pQueue) )
    SIR_VK_PROC( cmdDispatchBase, vkCmdDispatchBase, void,
        (VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ),
        (commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ) )
    SIR_VK_PROC( createDescriptorUpdateTemplate, vkCreateDescriptorUpdateTemplate, VkResult,
        (const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate),
        (m_Device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate) )
    SIR_VK_PROC( destroyDescriptorUpdateTemplate, vkDestroyDescriptorUpdateTemplate, void,
        (VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator),
        (m_Device, descriptorUpdateTemplate, pAllocator) )
    SIR_VK_PROC( updateDescriptorSetWithTemplate, vkUpdateDescriptorSetWithTemplate, void,
        (VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData),
        (m_Device, descriptorSet, descriptorUpdateTemplate, pData) )
    SIR_VK_PROC( getDescriptorSetLayoutSupport, vkGetDescriptorSetLayoutSupport, void,
        (const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport),
        (m_Device, pCreateInfo, pSupport) )
    SIR_VK_PROC( createSamplerYcbcrConversion, vkCreateSamplerYcbcrConversion, VkResult,
        (const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion),
        (m_Device, pCreateInfo, pAllocator, pYcbcrConversion) )
    SIR_VK_PROC( destroySamplerYcbcrConversion, vkDestroySamplerYcbcrConversion, void,
        (VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator),
        (m_Device, ycbcrConversion, pAllocator) )
#endif

#if defined(VK_VERSION_1_2)
    SIR_VK_PROC( resetQueryPool, vkResetQueryPool, void,
        (VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount),
        (m_Device, queryPool, firstQuery, queryCount) )
    SIR_VK_PROC( getSemaphoreCounterValue, vkGetSemaphoreCounterValue, VkResult,
        (VkSemaphore semaphore, uint64_t* pValue),
        (m_Device, semaphore, pValue) )
    SIR_VK_PROC( waitSemaphores, vkWaitSemaphores, VkResult,
        (const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout),
        (m_Device, pWaitInfo, timeout) )
    SIR_VK_PROC( signalSemaphore, vkSignalSemaphore, VkResult,
        (const VkSemaphoreSignalInfo* pSignalInfo),
        (m_Device, pSignalInfo) )
    SIR_VK_PROC( getBufferDeviceAddress, vkGetBufferDeviceAddress, VkDeviceAddress,
        (const VkBufferDeviceAddressInfo* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( getBufferOpaqueCaptureAddress, vkGetBufferOpaqueCaptureAddress, uint64_t,
        (const VkBufferDeviceAddressInfo* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( getDeviceMemoryOpaqueCaptureAddress, vkGetDeviceMemoryOpaqueCaptureAddress, uint64_t,
        (const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( cmdDrawIndirectCount, vkCmdDrawIndirectCount, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride),
        (commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride) )
    SIR_VK_PROC( cmdDrawIndexedIndirectCount, vkCmdDrawIndexedIndirectCount, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride),
        (commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride) )
    SIR_VK_PROC( createRenderPass2, vkCreateRenderPass2, VkResult,
        (const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass),
        (m_Device, pCreateInfo, pAllocator, pRenderPass) )
    SIR_VK_PROC( cmdBeginRenderPass2, vkCmdBeginRenderPass2, void,
        (VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo*      pRenderPassBegin, const VkSubpassBeginInfo*      pSubpassBeginInfo),
        (commandBuffer, pRenderPassBegin, pSubpassBeginInfo) )
    SIR_VK_PROC( cmdNextSubpass2, vkCmdNextSubpass2, void,
        (VkCommandBuffer commandBuffer, const VkSubpassBeginInfo*      pSubpassBeginInfo, const VkSubpassEndInfo*        pSubpassEndInfo),
        (commandBuffer, pSubpassBeginInfo, pSubpassEndInfo) )
    SIR_VK_PROC( cmdEndRenderPass2, vkCmdEndRenderPass2, void,
        (VkCommandBuffer commandBuffer, const VkSubpassEndInfo*        pSubpassEndInfo),
        (commandBuffer, pSubpassEndInfo) )
#endif

#if defined(VK_VERSION_1_3)
    SIR_VK_PROC( createPrivateDataSlot, vkCreatePrivateDataSlot, VkResult,
        (const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot),
        (m_Device, pCreateInfo, pAllocator, pPrivateDataSlot) )
    SIR_VK_PROC( destroyPrivateDataSlot, vkDestroyPrivateDataSlot, void,
        (VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator),
        (m_Device, privateDataSlot, pAllocator) )
    SIR_VK_PROC( setPrivateData, vkSetPrivateData, VkResult,
        (VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data),
        (m_Device, objectType, objectHandle, privateDataSlot, data) )
    SIR_VK_PROC( getPrivateData, vkGetPrivateData, void,
        (VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData),
        (m_Device, objectType, objectHandle, privateDataSlot, pData) )
    SIR_VK_PROC( cmdPipelineBarrier2, vkCmdPipelineBarrier2, void,
        (VkCommandBuffer                   commandBuffer, const VkDependencyInfo*                             pDependencyInfo),
        (commandBuffer, pDependencyInfo) )
    SIR_VK_PROC( cmdWriteTimestamp2, vkCmdWriteTimestamp2, void,
        (VkCommandBuffer                   commandBuffer, VkPipelineStageFlags2               stage, VkQueryPool                                         queryPool, uint32_t                                            query),
        (commandBuffer, stage, queryPool, query) )
    SIR_VK_PROC( queueSubmit2, vkQueueSubmit2, VkResult,
        (VkQueue                          queue, uint32_t                            submitCount, const VkSubmitInfo2*              pSubmits, VkFence           fence),
        (queue, submitCount, pSubmits, fence) )
    SIR_VK_PROC( cmdCopyBuffer2, vkCmdCopyBuffer2, void,
        (VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo),
        (commandBuffer, pCopyBufferInfo) )
    SIR_VK_PROC( cmdCopyImage2, vkCmdCopyImage2, void,
        (VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo),
        (commandBuffer, pCopyImageInfo) )
    SIR_VK_PROC( cmdCopyBufferToImage2, vkCmdCopyBufferToImage2, void,
        (VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo),
        (commandBuffer, pCopyBufferToImageInfo) )
    SIR_VK_PROC( cmdCopyImageToBuffer2, vkCmdCopyImageToBuffer2, void,
        (VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo),
        (commandBuffer, pCopyImageToBufferInfo) )
    SIR_VK_PROC( getDeviceBufferMemoryRequirements, vkGetDeviceBufferMemoryRequirements, void,
        (const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( getDeviceImageMemoryRequirements, vkGetDeviceImageMemoryRequirements, void,
        (const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( getDeviceImageSparseMemoryRequirements, vkGetDeviceImageSparseMemoryRequirements, void,
        (const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements),
        (m_Device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements) )
    SIR_VK_PROC( cmdSetEvent2, vkCmdSetEvent2, void,
        (VkCommandBuffer                   commandBuffer, VkEvent                                             event, const VkDependencyInfo*                             pDependencyInfo),
        (commandBuffer, event, pDependencyInfo) )
    SIR_VK_PROC( cmdResetEvent2, vkCmdResetEvent2, void,
        (VkCommandBuffer                   commandBuffer, VkEvent                                             event, VkPipelineStageFlags2               stageMask),
        (commandBuffer, event, stageMask) )
    SIR_VK_PROC( cmdWaitEvents2, vkCmdWaitEvents2, void,
        (VkCommandBuffer                   commandBuffer, uint32_t                                            eventCount, const VkEvent*                     pEvents, const VkDependencyInfo*            pDependencyInfos),
        (commandBuffer, eventCount, pEvents, pDependencyInfos) )
    SIR_VK_PROC( cmdBlitImage2, vkCmdBlitImage2, void,
        (VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo),
        (commandBuffer, pBlitImageInfo) )
    SIR_VK_PROC( cmdResolveImage2, vkCmdResolveImage2, void,
        (VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo),
        (commandBuffer, pResolveImageInfo) )
    SIR_VK_PROC( cmdBeginRendering, vkCmdBeginRendering, void,
        (VkCommandBuffer                   commandBuffer, const VkRenderingInfo*                              pRenderingInfo),
        (commandBuffer, pRenderingInfo) )
    SIR_VK_PROC( cmdEndRendering, vkCmdEndRendering, void,
        (VkCommandBuffer                   commandBuffer),
        (commandBuffer) )
    SIR_VK_PROC( cmdSetCullMode, vkCmdSetCullMode, void,
        (VkCommandBuffer commandBuffer, VkCullModeFlags cullMode),
        (commandBuffer, cullMode) )
    SIR_VK_PROC( cmdSetFrontFace, vkCmdSetFrontFace, void,
        (VkCommandBuffer commandBuffer, VkFrontFace frontFace),
        (commandBuffer, frontFace) )
    SIR_VK_PROC( cmdSetPrimitiveTopology, vkCmdSetPrimitiveTopology, void,
        (VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology),
        (commandBuffer, primitiveTopology) )
    SIR_VK_PROC( cmdSetViewportWithCount, vkCmdSetViewportWithCount, void,
        (VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports),
        (commandBuffer, viewportCount, pViewports) )
    SIR_VK_PROC( cmdSetScissorWithCount, vkCmdSetScissorWithCount, void,
        (VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors),
        (commandBuffer, scissorCount, pScissors) )
    SIR_VK_PROC( cmdBindVertexBuffers2, vkCmdBindVertexBuffers2, void,
        (VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides),
        (commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides) )
    SIR_VK_PROC( cmdSetDepthTestEnable, vkCmdSetDepthTestEnable, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthTestEnable),
        (commandBuffer, depthTestEnable) )
    SIR_VK_PROC( cmdSetDepthWriteEnable, vkCmdSetDepthWriteEnable, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable),
        (commandBuffer, depthWriteEnable) )
    SIR_VK_PROC( cmdSetDepthCompareOp, vkCmdSetDepthCompareOp, void,
        (VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp),
        (commandBuffer, depthCompareOp) )
    SIR_VK_PROC( cmdSetDepthBoundsTestEnable, vkCmdSetDepthBoundsTestEnable, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable),
        (commandBuffer, depthBoundsTestEnable) )
    SIR_VK_PROC( cmdSetStencilTestEnable, vkCmdSetStencilTestEnable, void,
        (VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable),
        (commandBuffer, stencilTestEnable) )
    SIR_VK_PROC( cmdSetStencilOp, vkCmdSetStencilOp, void,
        (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp),
        (commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp) )
    SIR_VK_PROC( cmdSetRasterizerDiscardEnable, vkCmdSetRasterizerDiscardEnable, void,
        (VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable),
        (commandBuffer, rasterizerDiscardEnable) )
    SIR_VK_PROC( cmdSetDepthBiasEnable, vkCmdSetDepthBiasEnable, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable),
        (commandBuffer, depthBiasEnable) )
    SIR_VK_PROC( cmdSetPrimitiveRestartEnable, vkCmdSetPrimitiveRestartEnable, void,
        (VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable),
        (commandBuffer, primitiveRestartEnable) )
#endif

#if defined(VK_VERSION_1_4)
    SIR_VK_PROC( mapMemory2, vkMapMemory2, VkResult,
        (const VkMemoryMapInfo* pMemoryMapInfo, void** ppData),
        (m_Device, pMemoryMapInfo, ppData) )
    SIR_VK_PROC( unmapMemory2, vkUnmapMemory2, VkResult,
        (const VkMemoryUnmapInfo* pMemoryUnmapInfo),
        (m_Device, pMemoryUnmapInfo) )
    SIR_VK_PROC( getDeviceImageSubresourceLayout, vkGetDeviceImageSubresourceLayout, void,
        (const VkDeviceImageSubresourceInfo* pInfo, VkSubresourceLayout2* pLayout),
        (m_Device, pInfo, pLayout) )
    SIR_VK_PROC( getImageSubresourceLayout2, vkGetImageSubresourceLayout2, void,
        (VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout),
        (m_Device, image, pSubresource, pLayout) )
    SIR_VK_PROC( copyMemoryToImage, vkCopyMemoryToImage, VkResult,
        (const VkCopyMemoryToImageInfo*    pCopyMemoryToImageInfo),
        (m_Device, pCopyMemoryToImageInfo) )
    SIR_VK_PROC( copyImageToMemory, vkCopyImageToMemory, VkResult,
        (const VkCopyImageToMemoryInfo*    pCopyImageToMemoryInfo),
        (m_Device, pCopyImageToMemoryInfo) )
    SIR_VK_PROC( copyImageToImage, vkCopyImageToImage, VkResult,
        (const VkCopyImageToImageInfo*    pCopyImageToImageInfo),
        (m_Device, pCopyImageToImageInfo) )
    SIR_VK_PROC( transitionImageLayout, vkTransitionImageLayout, VkResult,
        (uint32_t transitionCount, const VkHostImageLayoutTransitionInfo*    pTransitions),
        (m_Device, transitionCount, pTransitions) )
    SIR_VK_PROC( cmdPushDescriptorSet, vkCmdPushDescriptorSet, void,
        (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites),
        (commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites) )
    SIR_VK_PROC( cmdPushDescriptorSetWithTemplate, vkCmdPushDescriptorSetWithTemplate, void,
        (VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData),
        (commandBuffer, descriptorUpdateTemplate, layout, set, pData) )
    SIR_VK_PROC( cmdBindDescriptorSets2, vkCmdBindDescriptorSets2, void,
        (VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfo*   pBindDescriptorSetsInfo),
        (commandBuffer, pBindDescriptorSetsInfo) )
    SIR_VK_PROC( cmdPushConstants2, vkCmdPushConstants2, void,
        (VkCommandBuffer commandBuffer, const VkPushConstantsInfo*        pPushConstantsInfo),
        (commandBuffer, pPushConstantsInfo) )
    SIR_VK_PROC( cmdPushDescriptorSet2, vkCmdPushDescriptorSet2, void,
        (VkCommandBuffer commandBuffer, const VkPushDescriptorSetInfo*    pPushDescriptorSetInfo),
        (commandBuffer, pPushDescriptorSetInfo) )
    SIR_VK_PROC( cmdPushDescriptorSetWithTemplate2, vkCmdPushDescriptorSetWithTemplate2, void,
        (VkCommandBuffer commandBuffer, const VkPushDescriptorSetWithTemplateInfo* pPushDescriptorSetWithTemplateInfo),
        (commandBuffer, pPushDescriptorSetWithTemplateInfo) )
    SIR_VK_PROC( cmdSetLineStipple, vkCmdSetLineStipple, void,
        (VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern),
        (commandBuffer, lineStippleFactor, lineStipplePattern) )
    SIR_VK_PROC( cmdBindIndexBuffer2, vkCmdBindIndexBuffer2, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType),
        (commandBuffer, buffer, offset, size, indexType) )
    SIR_VK_PROC( getRenderingAreaGranularity, vkGetRenderingAreaGranularity, void,
        (const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity),
        (m_Device, pRenderingAreaInfo, pGranularity) )
    SIR_VK_PROC( cmdSetRenderingAttachmentLocations, vkCmdSetRenderingAttachmentLocations, void,
        (VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo),
        (commandBuffer, pLocationInfo) )
    SIR_VK_PROC( cmdSetRenderingInputAttachmentIndices, vkCmdSetRenderingInputAttachmentIndices, void,
        (VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo),
        (commandBuffer, pInputAttachmentIndexInfo) )
#endif

#if defined(VK_KHR_swapchain)
    SIR_VK_PROC( createSwapchainKHR, vkCreateSwapchainKHR, VkResult,
        (const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain),
        (m_Device, pCreateInfo, pAllocator, pSwapchain) )
    SIR_VK_PROC( destroySwapchainKHR, vkDestroySwapchainKHR, void,
        (VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator),
        (m_Device, swapchain, pAllocator) )
    SIR_VK_PROC( getSwapchainImagesKHR, vkGetSwapchainImagesKHR, VkResult,
        (VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages),
        (m_Device, swapchain, pSwapchainImageCount, pSwapchainImages) )
    SIR_VK_PROC( acquireNextImageKHR, vkAcquireNextImageKHR, VkResult,
        (VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex),
        (m_Device, swapchain, timeout, semaphore, fence, pImageIndex) )
    SIR_VK_PROC( queuePresentKHR, vkQueuePresentKHR, VkResult,
        (VkQueue queue, const VkPresentInfoKHR* pPresentInfo),
        (queue, pPresentInfo) )
    SIR_VK_PROC( getDeviceGroupPresentCapabilitiesKHR, vkGetDeviceGroupPresentCapabilitiesKHR, VkResult,
        (VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities),
        (m_Device, pDeviceGroupPresentCapabilities) )
    SIR_VK_PROC( getDeviceGroupSurfacePresentModesKHR, vkGetDeviceGroupSurfacePresentModesKHR, VkResult,
        (VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes),
        (m_Device, surface, pModes) )
    SIR_VK_PROC( acquireNextImage2KHR, vkAcquireNextImage2KHR, VkResult,
        (const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex),
        (m_Device, pAcquireInfo, pImageIndex) )
#endif

#if defined(VK_KHR_display_swapchain)
    SIR_VK_PROC( createSharedSwapchainsKHR, vkCreateSharedSwapchainsKHR, VkResult,
        (uint32_t swapchainCount, const VkSwapchainCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchains),
        (m_Device, swapchainCount, pCreateInfos, pAllocator, pSwapchains) )
#endif

#if defined(VK_KHR_video_queue)
    SIR_VK_PROC( createVideoSessionKHR, vkCreateVideoSessionKHR, VkResult,
        (const VkVideoSessionCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkVideoSessionKHR* pVideoSession),
        (m_Device, pCreateInfo, pAllocator, pVideoSession) )
    SIR_VK_PROC( destroyVideoSessionKHR, vkDestroyVideoSessionKHR, void,
        (VkVideoSessionKHR videoSession, const VkAllocationCallbacks* pAllocator),
        (m_Device, videoSession, pAllocator) )
    SIR_VK_PROC( getVideoSessionMemoryRequirementsKHR, vkGetVideoSessionMemoryRequirementsKHR, VkResult,
        (VkVideoSessionKHR videoSession, uint32_t* pMemoryRequirementsCount, VkVideoSessionMemoryRequirementsKHR* pMemoryRequirements),
        (m_Device, videoSession, pMemoryRequirementsCount, pMemoryRequirements) )
    SIR_VK_PROC( bindVideoSessionMemoryKHR, vkBindVideoSessionMemoryKHR, VkResult,
        (VkVideoSessionKHR videoSession, uint32_t bindSessionMemoryInfoCount, const VkBindVideoSessionMemoryInfoKHR* pBindSessionMemoryInfos),
        (m_Device, videoSession, bindSessionMemoryInfoCount, pBindSessionMemoryInfos) )
    SIR_VK_PROC( createVideoSessionParametersKHR, vkCreateVideoSessionParametersKHR, VkResult,
        (const VkVideoSessionParametersCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkVideoSessionParametersKHR* pVideoSessionParameters),
        (m_Device, pCreateInfo, pAllocator, pVideoSessionParameters) )
    SIR_VK_PROC( updateVideoSessionParametersKHR, vkUpdateVideoSessionParametersKHR, VkResult,
        (VkVideoSessionParametersKHR videoSessionParameters, const VkVideoSessionParametersUpdateInfoKHR* pUpdateInfo),
        (m_Device, videoSessionParameters, pUpdateInfo) )
    SIR_VK_PROC( destroyVideoSessionParametersKHR, vkDestroyVideoSessionParametersKHR, void,
        (VkVideoSessionParametersKHR videoSessionParameters, const VkAllocationCallbacks* pAllocator),
        (m_Device, videoSessionParameters, pAllocator) )
    SIR_VK_PROC( cmdBeginVideoCodingKHR, vkCmdBeginVideoCodingKHR, void,
        (VkCommandBuffer commandBuffer, const VkVideoBeginCodingInfoKHR* pBeginInfo),
        (commandBuffer, pBeginInfo) )
    SIR_VK_PROC( cmdEndVideoCodingKHR, vkCmdEndVideoCodingKHR, void,
        (VkCommandBuffer commandBuffer, const VkVideoEndCodingInfoKHR* pEndCodingInfo),
        (commandBuffer, pEndCodingInfo) )
    SIR_VK_PROC( cmdControlVideoCodingKHR, vkCmdControlVideoCodingKHR, void,
        (VkCommandBuffer commandBuffer, const VkVideoCodingControlInfoKHR* pCodingControlInfo),
        (commandBuffer, pCodingControlInfo) )
#endif

#if defined(VK_KHR_video_decode_queue)
    SIR_VK_PROC( cmdDecodeVideoKHR, vkCmdDecodeVideoKHR, void,
        (VkCommandBuffer commandBuffer, const VkVideoDecodeInfoKHR* pDecodeInfo),
        (commandBuffer, pDecodeInfo) )
#endif

#if defined(VK_KHR_dynamic_rendering)
    SIR_VK_PROC( cmdBeginRenderingKHR, vkCmdBeginRenderingKHR, void,
        (VkCommandBuffer                   commandBuffer, const VkRenderingInfo*                              pRenderingInfo),
        (commandBuffer, pRenderingInfo) )
    SIR_VK_PROC( cmdEndRenderingKHR, vkCmdEndRenderingKHR, void,
        (VkCommandBuffer                   commandBuffer),
        (commandBuffer) )
#endif

#if defined(VK_KHR_device_group)
    SIR_VK_PROC( getDeviceGroupPeerMemoryFeaturesKHR, vkGetDeviceGroupPeerMemoryFeaturesKHR, void,
        (uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures),
        (m_Device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures) )
    SIR_VK_PROC( cmdSetDeviceMaskKHR, vkCmdSetDeviceMaskKHR, void,
        (VkCommandBuffer commandBuffer, uint32_t deviceMask),
        (commandBuffer, deviceMask) )
    SIR_VK_PROC( cmdDispatchBaseKHR, vkCmdDispatchBaseKHR, void,
        (VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ),
        (commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ) )
#endif

#if defined(VK_KHR_maintenance1)
    SIR_VK_PROC( trimCommandPoolKHR, vkTrimCommandPoolKHR, void,
        (VkCommandPool commandPool, VkCommandPoolTrimFlags flags),
        (m_Device, commandPool, flags) )
#endif

#if defined(VK_KHR_external_memory_fd)
    SIR_VK_PROC( getMemoryFdKHR, vkGetMemoryFdKHR, VkResult,
        (const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd),
        (m_Device, pGetFdInfo, pFd) )
    SIR_VK_PROC( getMemoryFdPropertiesKHR, vkGetMemoryFdPropertiesKHR, VkResult,
        (VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties),
        (m_Device, handleType, fd, pMemoryFdProperties) )
#endif

#if defined(VK_KHR_external_semaphore_fd)
    SIR_VK_PROC( importSemaphoreFdKHR, vkImportSemaphoreFdKHR, VkResult,
        (const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo),
        (m_Device, pImportSemaphoreFdInfo) )
    SIR_VK_PROC( getSemaphoreFdKHR, vkGetSemaphoreFdKHR, VkResult,
        (const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd),
        (m_Device, pGetFdInfo, pFd) )
#endif

#if defined(VK_KHR_push_descriptor)
    SIR_VK_PROC( cmdPushDescriptorSetKHR, vkCmdPushDescriptorSetKHR, void,
        (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites),
        (commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites) )
    SIR_VK_PROC( cmdPushDescriptorSetWithTemplateKHR, vkCmdPushDescriptorSetWithTemplateKHR, void,
        (VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData),
        (commandBuffer, descriptorUpdateTemplate, layout, set, pData) )
#endif

#if defined(VK_KHR_descriptor_update_template)
    SIR_VK_PROC( createDescriptorUpdateTemplateKHR, vkCreateDescriptorUpdateTemplateKHR, VkResult,
        (const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate),
        (m_Device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate) )
    SIR_VK_PROC( destroyDescriptorUpdateTemplateKHR, vkDestroyDescriptorUpdateTemplateKHR, void,
        (VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator),
        (m_Device, descriptorUpdateTemplate, pAllocator) )
    SIR_VK_PROC( updateDescriptorSetWithTemplateKHR, vkUpdateDescriptorSetWithTemplateKHR, void,
        (VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData),
        (m_Device, descriptorSet, descriptorUpdateTemplate, pData) )
#endif

#if defined(VK_KHR_create_renderpass2)
    SIR_VK_PROC( createRenderPass2KHR, vkCreateRenderPass2KHR, VkResult,
        (const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass),
        (m_Device, pCreateInfo, pAllocator, pRenderPass) )
    SIR_VK_PROC( cmdBeginRenderPass2KHR, vkCmdBeginRenderPass2KHR, void,
        (VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo*      pRenderPassBegin, const VkSubpassBeginInfo*      pSubpassBeginInfo),
        (commandBuffer, pRenderPassBegin, pSubpassBeginInfo) )
    SIR_VK_PROC( cmdNextSubpass2KHR, vkCmdNextSubpass2KHR, void,
        (VkCommandBuffer commandBuffer, const VkSubpassBeginInfo*      pSubpassBeginInfo, const VkSubpassEndInfo*        pSubpassEndInfo),
        (commandBuffer, pSubpassBeginInfo, pSubpassEndInfo) )
    SIR_VK_PROC( cmdEndRenderPass2KHR, vkCmdEndRenderPass2KHR, void,
        (VkCommandBuffer commandBuffer, const VkSubpassEndInfo*        pSubpassEndInfo),
        (commandBuffer, pSubpassEndInfo) )
#endif

#if defined(VK_KHR_shared_presentable_image)
    SIR_VK_PROC( getSwapchainStatusKHR, vkGetSwapchainStatusKHR, VkResult,
        (VkSwapchainKHR swapchain),
        (m_Device, swapchain) )
#endif

#if defined(VK_KHR_external_fence_fd)
    SIR_VK_PROC( importFenceFdKHR, vkImportFenceFdKHR, VkResult,
        (const VkImportFenceFdInfoKHR* pImportFenceFdInfo),
        (m_Device, pImportFenceFdInfo) )
    SIR_VK_PROC( getFenceFdKHR, vkGetFenceFdKHR, VkResult,
        (const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd),
        (m_Device, pGetFdInfo, pFd) )
#endif

#if defined(VK_KHR_performance_query)
    SIR_VK_PROC( acquireProfilingLockKHR, vkAcquireProfilingLockKHR, VkResult,
        (const VkAcquireProfilingLockInfoKHR* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( releaseProfilingLockKHR, vkReleaseProfilingLockKHR, void,
        (),
        (m_Device) )
#endif

#if defined(VK_KHR_get_memory_requirements2)
    SIR_VK_PROC( getImageMemoryRequirements2KHR, vkGetImageMemoryRequirements2KHR, void,
        (const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( getBufferMemoryRequirements2KHR, vkGetBufferMemoryRequirements2KHR, void,
        (const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( getImageSparseMemoryRequirements2KHR, vkGetImageSparseMemoryRequirements2KHR, void,
        (const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements),
        (m_Device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements) )
#endif

#if defined(VK_KHR_sampler_ycbcr_conversion)
    SIR_VK_PROC( createSamplerYcbcrConversionKHR, vkCreateSamplerYcbcrConversionKHR, VkResult,
        (const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion),
        (m_Device, pCreateInfo, pAllocator, pYcbcrConversion) )
    SIR_VK_PROC( destroySamplerYcbcrConversionKHR, vkDestroySamplerYcbcrConversionKHR, void,
        (VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator),
        (m_Device, ycbcrConversion, pAllocator) )
#endif

#if defined(VK_KHR_bind_memory2)
    SIR_VK_PROC( bindBufferMemory2KHR, vkBindBufferMemory2KHR, VkResult,
        (uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos),
        (m_Device, bindInfoCount, pBindInfos) )
    SIR_VK_PROC( bindImageMemory2KHR, vkBindImageMemory2KHR, VkResult,
        (uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos),
        (m_Device, bindInfoCount, pBindInfos) )
#endif

#if defined(VK_KHR_maintenance3)
    SIR_VK_PROC( getDescriptorSetLayoutSupportKHR, vkGetDescriptorSetLayoutSupportKHR, void,
        (const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport),
        (m_Device, pCreateInfo, pSupport) )
#endif

#if defined(VK_KHR_draw_indirect_count)
    SIR_VK_PROC( cmdDrawIndirectCountKHR, vkCmdDrawIndirectCountKHR, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride),
        (commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride) )
    SIR_VK_PROC( cmdDrawIndexedIndirectCountKHR, vkCmdDrawIndexedIndirectCountKHR, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride),
        (commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride) )
#endif

#if defined(VK_KHR_timeline_semaphore)
    SIR_VK_PROC( getSemaphoreCounterValueKHR, vkGetSemaphoreCounterValueKHR, VkResult,
        (VkSemaphore semaphore, uint64_t* pValue),
        (m_Device, semaphore, pValue) )
    SIR_VK_PROC( waitSemaphoresKHR, vkWaitSemaphoresKHR, VkResult,
        (const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout),
        (m_Device, pWaitInfo, timeout) )
    SIR_VK_PROC( signalSemaphoreKHR, vkSignalSemaphoreKHR, VkResult,
        (const VkSemaphoreSignalInfo* pSignalInfo),
        (m_Device, pSignalInfo) )
#endif

#if defined(VK_KHR_fragment_shading_rate)
    SIR_VK_PROC( cmdSetFragmentShadingRateKHR, vkCmdSetFragmentShadingRateKHR, void,
        (VkCommandBuffer           commandBuffer, const VkExtent2D*                           pFragmentSize, const VkFragmentShadingRateCombinerOpKHR    combinerOps[2]),
        (commandBuffer, pFragmentSize, combinerOps) )
#endif

#if defined(VK_KHR_dynamic_rendering_local_read)
    SIR_VK_PROC( cmdSetRenderingAttachmentLocationsKHR, vkCmdSetRenderingAttachmentLocationsKHR, void,
        (VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo),
        (commandBuffer, pLocationInfo) )
    SIR_VK_PROC( cmdSetRenderingInputAttachmentIndicesKHR, vkCmdSetRenderingInputAttachmentIndicesKHR, void,
        (VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo),
        (commandBuffer, pInputAttachmentIndexInfo) )
#endif

#if defined(VK_KHR_present_wait)
    SIR_VK_PROC( waitForPresentKHR, vkWaitForPresentKHR, VkResult,
        (VkSwapchainKHR swapchain, uint64_t presentId, uint64_t timeout),
        (m_Device, swapchain, presentId, timeout) )
#endif

#if defined(VK_KHR_buffer_device_address)
    SIR_VK_PROC( getBufferDeviceAddressKHR, vkGetBufferDeviceAddressKHR, VkDeviceAddress,
        (const VkBufferDeviceAddressInfo* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( getBufferOpaqueCaptureAddressKHR, vkGetBufferOpaqueCaptureAddressKHR, uint64_t,
        (const VkBufferDeviceAddressInfo* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( getDeviceMemoryOpaqueCaptureAddressKHR, vkGetDeviceMemoryOpaqueCaptureAddressKHR, uint64_t,
        (const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo),
        (m_Device, pInfo) )
#endif

#if defined(VK_KHR_deferred_host_operations)
    SIR_VK_PROC( createDeferredOperationKHR, vkCreateDeferredOperationKHR, VkResult,
        (const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation),
        (m_Device, pAllocator, pDeferredOperation) )
    SIR_VK_PROC( destroyDeferredOperationKHR, vkDestroyDeferredOperationKHR, void,
        (VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator),
        (m_Device, operation, pAllocator) )
    SIR_VK_PROC( getDeferredOperationMaxConcurrencyKHR, vkGetDeferredOperationMaxConcurrencyKHR, uint32_t,
        (VkDeferredOperationKHR operation),
        (m_Device, operation) )
    SIR_VK_PROC( getDeferredOperationResultKHR, vkGetDeferredOperationResultKHR, VkResult,
        (VkDeferredOperationKHR operation),
        (m_Device, operation) )
    SIR_VK_PROC( deferredOperationJoinKHR, vkDeferredOperationJoinKHR, VkResult,
        (VkDeferredOperationKHR operation),
        (m_Device, operation) )
#endif

#if defined(VK_KHR_pipeline_executable_properties)
    SIR_VK_PROC( getPipelineExecutablePropertiesKHR, vkGetPipelineExecutablePropertiesKHR, VkResult,
        (const VkPipelineInfoKHR*        pPipelineInfo, uint32_t* pExecutableCount, VkPipelineExecutablePropertiesKHR* pProperties),
        (m_Device, pPipelineInfo, pExecutableCount, pProperties) )
    SIR_VK_PROC( getPipelineExecutableStatisticsKHR, vkGetPipelineExecutableStatisticsKHR, VkResult,
        (const VkPipelineExecutableInfoKHR*  pExecutableInfo, uint32_t* pStatisticCount, VkPipelineExecutableStatisticKHR* pStatistics),
        (m_Device, pExecutableInfo, pStatisticCount, pStatistics) )
    SIR_VK_PROC( getPipelineExecutableInternalRepresentationsKHR, vkGetPipelineExecutableInternalRepresentationsKHR, VkResult,
        (const VkPipelineExecutableInfoKHR*  pExecutableInfo, uint32_t* pInternalRepresentationCount, VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations),
        (m_Device, pExecutableInfo, pInternalRepresentationCount, pInternalRepresentations) )
#endif

#if defined(VK_KHR_map_memory2)
    SIR_VK_PROC( mapMemory2KHR, vkMapMemory2KHR, VkResult,
        (const VkMemoryMapInfo* pMemoryMapInfo, void** ppData),
        (m_Device, pMemoryMapInfo, ppData) )
    SIR_VK_PROC( unmapMemory2KHR, vkUnmapMemory2KHR, VkResult,
        (const VkMemoryUnmapInfo* pMemoryUnmapInfo),
        (m_Device, pMemoryUnmapInfo) )
#endif

#if defined(VK_KHR_video_encode_queue)
    SIR_VK_PROC( getEncodedVideoSessionParametersKHR, vkGetEncodedVideoSessionParametersKHR, VkResult,
        (const VkVideoEncodeSessionParametersGetInfoKHR* pVideoSessionParametersInfo, VkVideoEncodeSessionParametersFeedbackInfoKHR* pFeedbackInfo, size_t* pDataSize, void* pData),
        (m_Device, pVideoSessionParametersInfo, pFeedbackInfo, pDataSize, pData) )
    SIR_VK_PROC( cmdEncodeVideoKHR, vkCmdEncodeVideoKHR, void,
        (VkCommandBuffer commandBuffer, const VkVideoEncodeInfoKHR* pEncodeInfo),
        (commandBuffer, pEncodeInfo) )
#endif

#if defined(VK_KHR_synchronization2)
    SIR_VK_PROC( cmdSetEvent2KHR, vkCmdSetEvent2KHR, void,
        (VkCommandBuffer                   commandBuffer, VkEvent                                             event, const VkDependencyInfo*                             pDependencyInfo),
        (commandBuffer, event, pDependencyInfo) )
    SIR_VK_PROC( cmdResetEvent2KHR, vkCmdResetEvent2KHR, void,
        (VkCommandBuffer                   commandBuffer, VkEvent                                             event, VkPipelineStageFlags2               stageMask),
        (commandBuffer, event, stageMask) )
    SIR_VK_PROC( cmdWaitEvents2KHR, vkCmdWaitEvents2KHR, void,
        (VkCommandBuffer                   commandBuffer, uint32_t                                            eventCount, const VkEvent*                     pEvents, const VkDependencyInfo*            pDependencyInfos),
        (commandBuffer, eventCount, pEvents, pDependencyInfos) )
    SIR_VK_PROC( cmdPipelineBarrier2KHR, vkCmdPipelineBarrier2KHR, void,
        (VkCommandBuffer                   commandBuffer, const VkDependencyInfo*                             pDependencyInfo),
        (commandBuffer, pDependencyInfo) )
    SIR_VK_PROC( cmdWriteTimestamp2KHR, vkCmdWriteTimestamp2KHR, void,
        (VkCommandBuffer                   commandBuffer, VkPipelineStageFlags2               stage, VkQueryPool                                         queryPool, uint32_t                                            query),
        (commandBuffer, stage, queryPool, query) )
    SIR_VK_PROC( queueSubmit2KHR, vkQueueSubmit2KHR, VkResult,
        (VkQueue                          queue, uint32_t                            submitCount, const VkSubmitInfo2*              pSubmits, VkFence           fence),
        (queue, submitCount, pSubmits, fence) )
#endif

#if defined(VK_KHR_device_address_commands)
    SIR_VK_PROC( cmdBindIndexBuffer3KHR, vkCmdBindIndexBuffer3KHR, void,
        (VkCommandBuffer   commandBuffer, const VkBindIndexBuffer3InfoKHR*    pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdBindVertexBuffers3KHR, vkCmdBindVertexBuffers3KHR, void,
        (VkCommandBuffer commandBuffer, uint32_t                            firstBinding, uint32_t                            bindingCount, const VkBindVertexBuffer3InfoKHR* pBindingInfos),
        (commandBuffer, firstBinding, bindingCount, pBindingInfos) )
    SIR_VK_PROC( cmdDrawIndirect2KHR, vkCmdDrawIndirect2KHR, void,
        (VkCommandBuffer   commandBuffer, const VkDrawIndirect2InfoKHR*       pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdDrawIndexedIndirect2KHR, vkCmdDrawIndexedIndirect2KHR, void,
        (VkCommandBuffer   commandBuffer, const VkDrawIndirect2InfoKHR*       pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdDispatchIndirect2KHR, vkCmdDispatchIndirect2KHR, void,
        (VkCommandBuffer   commandBuffer, const VkDispatchIndirect2InfoKHR*   pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdCopyMemoryKHR, vkCmdCopyMemoryKHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyDeviceMemoryInfoKHR* pCopyMemoryInfo),
        (commandBuffer, pCopyMemoryInfo) )
    SIR_VK_PROC( cmdCopyMemoryToImageKHR, vkCmdCopyMemoryToImageKHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyDeviceMemoryImageInfoKHR* pCopyMemoryInfo),
        (commandBuffer, pCopyMemoryInfo) )
    SIR_VK_PROC( cmdCopyImageToMemoryKHR, vkCmdCopyImageToMemoryKHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyDeviceMemoryImageInfoKHR* pCopyMemoryInfo),
        (commandBuffer, pCopyMemoryInfo) )
    SIR_VK_PROC( cmdUpdateMemoryKHR, vkCmdUpdateMemoryKHR, void,
        (VkCommandBuffer   commandBuffer, const VkDeviceAddressRangeKHR*      pDstRange, VkAddressCommandFlagsKHR dstFlags, VkDeviceSize                        dataSize, const void*          pData),
        (commandBuffer, pDstRange, dstFlags, dataSize, pData) )
    SIR_VK_PROC( cmdFillMemoryKHR, vkCmdFillMemoryKHR, void,
        (VkCommandBuffer   commandBuffer, const VkDeviceAddressRangeKHR*      pDstRange, VkAddressCommandFlagsKHR dstFlags, uint32_t                            data),
        (commandBuffer, pDstRange, dstFlags, data) )
    SIR_VK_PROC( cmdCopyQueryPoolResultsToMemoryKHR, vkCmdCopyQueryPoolResultsToMemoryKHR, void,
        (VkCommandBuffer   commandBuffer, VkQueryPool                         queryPool, uint32_t                            firstQuery, uint32_t                            queryCount, const VkStridedDeviceAddressRangeKHR* pDstRange, VkAddressCommandFlagsKHR    dstFlags, VkQueryResultFlags  queryResultFlags),
        (commandBuffer, queryPool, firstQuery, queryCount, pDstRange, dstFlags, queryResultFlags) )
    SIR_VK_PROC( cmdDrawIndirectCount2KHR, vkCmdDrawIndirectCount2KHR, void,
        (VkCommandBuffer   commandBuffer, const VkDrawIndirectCount2InfoKHR*  pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdDrawIndexedIndirectCount2KHR, vkCmdDrawIndexedIndirectCount2KHR, void,
        (VkCommandBuffer   commandBuffer, const VkDrawIndirectCount2InfoKHR*  pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdBeginConditionalRendering2EXT, vkCmdBeginConditionalRendering2EXT, void,
        (VkCommandBuffer   commandBuffer, const VkConditionalRenderingBeginInfo2EXT* pConditionalRenderingBegin),
        (commandBuffer, pConditionalRenderingBegin) )
    SIR_VK_PROC( cmdBindTransformFeedbackBuffers2EXT, vkCmdBindTransformFeedbackBuffers2EXT, void,
        (VkCommandBuffer   commandBuffer, uint32_t                            firstBinding, uint32_t                            bindingCount, const VkBindTransformFeedbackBuffer2InfoEXT* pBindingInfos),
        (commandBuffer, firstBinding, bindingCount, pBindingInfos) )
    SIR_VK_PROC( cmdBeginTransformFeedback2EXT, vkCmdBeginTransformFeedback2EXT, void,
        (VkCommandBuffer   commandBuffer, uint32_t                            firstCounterRange, uint32_t            counterRangeCount, const VkBindTransformFeedbackBuffer2InfoEXT* pCounterInfos),
        (commandBuffer, firstCounterRange, counterRangeCount, pCounterInfos) )
    SIR_VK_PROC( cmdEndTransformFeedback2EXT, vkCmdEndTransformFeedback2EXT, void,
        (VkCommandBuffer   commandBuffer, uint32_t                            firstCounterRange, uint32_t            counterRangeCount, const VkBindTransformFeedbackBuffer2InfoEXT* pCounterInfos),
        (commandBuffer, firstCounterRange, counterRangeCount, pCounterInfos) )
    SIR_VK_PROC( cmdDrawIndirectByteCount2EXT, vkCmdDrawIndirectByteCount2EXT, void,
        (VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, const VkBindTransformFeedbackBuffer2InfoEXT* pCounterInfo, uint32_t counterOffset, uint32_t vertexStride),
        (commandBuffer, instanceCount, firstInstance, pCounterInfo, counterOffset, vertexStride) )
    SIR_VK_PROC( cmdDrawMeshTasksIndirect2EXT, vkCmdDrawMeshTasksIndirect2EXT, void,
        (VkCommandBuffer   commandBuffer, const VkDrawIndirect2InfoKHR*       pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdDrawMeshTasksIndirectCount2EXT, vkCmdDrawMeshTasksIndirectCount2EXT, void,
        (VkCommandBuffer   commandBuffer, const VkDrawIndirectCount2InfoKHR*  pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdWriteMarkerToMemoryAMD, vkCmdWriteMarkerToMemoryAMD, void,
        (VkCommandBuffer   commandBuffer, const VkMemoryMarkerInfoAMD*         pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( createAccelerationStructure2KHR, vkCreateAccelerationStructure2KHR, VkResult,
        (const VkAccelerationStructureCreateInfo2KHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR*                  pAccelerationStructure),
        (m_Device, pCreateInfo, pAllocator, pAccelerationStructure) )
#endif

#if defined(VK_KHR_copy_commands2)
    SIR_VK_PROC( cmdCopyBuffer2KHR, vkCmdCopyBuffer2KHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo),
        (commandBuffer, pCopyBufferInfo) )
    SIR_VK_PROC( cmdCopyImage2KHR, vkCmdCopyImage2KHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo),
        (commandBuffer, pCopyImageInfo) )
    SIR_VK_PROC( cmdCopyBufferToImage2KHR, vkCmdCopyBufferToImage2KHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo),
        (commandBuffer, pCopyBufferToImageInfo) )
    SIR_VK_PROC( cmdCopyImageToBuffer2KHR, vkCmdCopyImageToBuffer2KHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo),
        (commandBuffer, pCopyImageToBufferInfo) )
    SIR_VK_PROC( cmdBlitImage2KHR, vkCmdBlitImage2KHR, void,
        (VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo),
        (commandBuffer, pBlitImageInfo) )
    SIR_VK_PROC( cmdResolveImage2KHR, vkCmdResolveImage2KHR, void,
        (VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo),
        (commandBuffer, pResolveImageInfo) )
#endif

#if defined(VK_KHR_ray_tracing_maintenance1)
    SIR_VK_PROC( cmdTraceRaysIndirect2KHR, vkCmdTraceRaysIndirect2KHR, void,
        (VkCommandBuffer commandBuffer, VkDeviceAddress indirectDeviceAddress),
        (commandBuffer, indirectDeviceAddress) )
#endif

#if defined(VK_KHR_maintenance4)
    SIR_VK_PROC( getDeviceBufferMemoryRequirementsKHR, vkGetDeviceBufferMemoryRequirementsKHR, void,
        (const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( getDeviceImageMemoryRequirementsKHR, vkGetDeviceImageMemoryRequirementsKHR, void,
        (const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( getDeviceImageSparseMemoryRequirementsKHR, vkGetDeviceImageSparseMemoryRequirementsKHR, void,
        (const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements),
        (m_Device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements) )
#endif

#if defined(VK_KHR_maintenance5)
    SIR_VK_PROC( cmdBindIndexBuffer2KHR, vkCmdBindIndexBuffer2KHR, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType),
        (commandBuffer, buffer, offset, size, indexType) )
    SIR_VK_PROC( getRenderingAreaGranularityKHR, vkGetRenderingAreaGranularityKHR, void,
        (const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity),
        (m_Device, pRenderingAreaInfo, pGranularity) )
    SIR_VK_PROC( getDeviceImageSubresourceLayoutKHR, vkGetDeviceImageSubresourceLayoutKHR, void,
        (const VkDeviceImageSubresourceInfo* pInfo, VkSubresourceLayout2* pLayout),
        (m_Device, pInfo, pLayout) )
    SIR_VK_PROC( getImageSubresourceLayout2KHR, vkGetImageSubresourceLayout2KHR, void,
        (VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout),
        (m_Device, image, pSubresource, pLayout) )
#endif

#if defined(VK_KHR_present_wait2)
    SIR_VK_PROC( waitForPresent2KHR, vkWaitForPresent2KHR, VkResult,
        (VkSwapchainKHR swapchain, const VkPresentWait2InfoKHR* pPresentWait2Info),
        (m_Device, swapchain, pPresentWait2Info) )
#endif

#if defined(VK_KHR_pipeline_binary)
    SIR_VK_PROC( createPipelineBinariesKHR, vkCreatePipelineBinariesKHR, VkResult,
        (const VkPipelineBinaryCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineBinaryHandlesInfoKHR* pBinaries),
        (m_Device, pCreateInfo, pAllocator, pBinaries) )
    SIR_VK_PROC( destroyPipelineBinaryKHR, vkDestroyPipelineBinaryKHR, void,
        (VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks* pAllocator),
        (m_Device, pipelineBinary, pAllocator) )
    SIR_VK_PROC( getPipelineKeyKHR, vkGetPipelineKeyKHR, VkResult,
        (const VkPipelineCreateInfoKHR* pPipelineCreateInfo, VkPipelineBinaryKeyKHR* pPipelineKey),
        (m_Device, pPipelineCreateInfo, pPipelineKey) )
    SIR_VK_PROC( getPipelineBinaryDataKHR, vkGetPipelineBinaryDataKHR, VkResult,
        (const VkPipelineBinaryDataInfoKHR* pInfo, VkPipelineBinaryKeyKHR* pPipelineBinaryKey, size_t* pPipelineBinaryDataSize, void* pPipelineBinaryData),
        (m_Device, pInfo, pPipelineBinaryKey, pPipelineBinaryDataSize, pPipelineBinaryData) )
    SIR_VK_PROC( releaseCapturedPipelineDataKHR, vkReleaseCapturedPipelineDataKHR, VkResult,
        (const VkReleaseCapturedPipelineDataInfoKHR* pInfo, const VkAllocationCallbacks* pAllocator),
        (m_Device, pInfo, pAllocator) )
#endif

#if defined(VK_KHR_swapchain_maintenance1)
    SIR_VK_PROC( releaseSwapchainImagesKHR, vkReleaseSwapchainImagesKHR, VkResult,
        (const VkReleaseSwapchainImagesInfoKHR* pReleaseInfo),
        (m_Device, pReleaseInfo) )
#endif

#if defined(VK_KHR_line_rasterization)
    SIR_VK_PROC( cmdSetLineStippleKHR, vkCmdSetLineStippleKHR, void,
        (VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern),
        (commandBuffer, lineStippleFactor, lineStipplePattern) )
#endif

#if defined(VK_KHR_calibrated_timestamps)
    SIR_VK_PROC( getCalibratedTimestampsKHR, vkGetCalibratedTimestampsKHR, VkResult,
        (uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation),
        (m_Device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation) )
#endif

#if defined(VK_KHR_maintenance6)
    SIR_VK_PROC( cmdBindDescriptorSets2KHR, vkCmdBindDescriptorSets2KHR, void,
        (VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfo*   pBindDescriptorSetsInfo),
        (commandBuffer, pBindDescriptorSetsInfo) )
    SIR_VK_PROC( cmdPushConstants2KHR, vkCmdPushConstants2KHR, void,
        (VkCommandBuffer commandBuffer, const VkPushConstantsInfo*        pPushConstantsInfo),
        (commandBuffer, pPushConstantsInfo) )
    SIR_VK_PROC( cmdPushDescriptorSet2KHR, vkCmdPushDescriptorSet2KHR, void,
        (VkCommandBuffer commandBuffer, const VkPushDescriptorSetInfo*    pPushDescriptorSetInfo),
        (commandBuffer, pPushDescriptorSetInfo) )
    SIR_VK_PROC( cmdPushDescriptorSetWithTemplate2KHR, vkCmdPushDescriptorSetWithTemplate2KHR, void,
        (VkCommandBuffer commandBuffer, const VkPushDescriptorSetWithTemplateInfo* pPushDescriptorSetWithTemplateInfo),
        (commandBuffer, pPushDescriptorSetWithTemplateInfo) )
    SIR_VK_PROC( cmdSetDescriptorBufferOffsets2EXT, vkCmdSetDescriptorBufferOffsets2EXT, void,
        (VkCommandBuffer commandBuffer, const VkSetDescriptorBufferOffsetsInfoEXT* pSetDescriptorBufferOffsetsInfo),
        (commandBuffer, pSetDescriptorBufferOffsetsInfo) )
    SIR_VK_PROC( cmdBindDescriptorBufferEmbeddedSamplers2EXT, vkCmdBindDescriptorBufferEmbeddedSamplers2EXT, void,
        (VkCommandBuffer commandBuffer, const VkBindDescriptorBufferEmbeddedSamplersInfoEXT* pBindDescriptorBufferEmbeddedSamplersInfo),
        (commandBuffer, pBindDescriptorBufferEmbeddedSamplersInfo) )
#endif

#if defined(VK_KHR_copy_memory_indirect)
    SIR_VK_PROC( cmdCopyMemoryIndirectKHR, vkCmdCopyMemoryIndirectKHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyMemoryIndirectInfoKHR* pCopyMemoryIndirectInfo),
        (commandBuffer, pCopyMemoryIndirectInfo) )
    SIR_VK_PROC( cmdCopyMemoryToImageIndirectKHR, vkCmdCopyMemoryToImageIndirectKHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyMemoryToImageIndirectInfoKHR* pCopyMemoryToImageIndirectInfo),
        (commandBuffer, pCopyMemoryToImageIndirectInfo) )
#endif

#if defined(VK_KHR_device_fault)
    SIR_VK_PROC( getDeviceFaultReportsKHR, vkGetDeviceFaultReportsKHR, VkResult,
        (uint64_t timeout, uint32_t* pFaultCounts, VkDeviceFaultInfoKHR* pFaultInfo),
        (m_Device, timeout, pFaultCounts, pFaultInfo) )
    SIR_VK_PROC( getDeviceFaultDebugInfoKHR, vkGetDeviceFaultDebugInfoKHR, VkResult,
        (VkDeviceFaultDebugInfoKHR* pDebugInfo),
        (m_Device, pDebugInfo) )
#endif

#if defined(VK_KHR_maintenance10)
    SIR_VK_PROC( cmdEndRendering2KHR, vkCmdEndRendering2KHR, void,
        (VkCommandBuffer                   commandBuffer, const VkRenderingEndInfoKHR*        pRenderingEndInfo),
        (commandBuffer, pRenderingEndInfo) )
#endif

#if defined(VK_EXT_debug_marker)
    SIR_VK_PROC( debugMarkerSetObjectTagEXT, vkDebugMarkerSetObjectTagEXT, VkResult,
        (const VkDebugMarkerObjectTagInfoEXT* pTagInfo),
        (m_Device, pTagInfo) )
    SIR_VK_PROC( debugMarkerSetObjectNameEXT, vkDebugMarkerSetObjectNameEXT, VkResult,
        (const VkDebugMarkerObjectNameInfoEXT* pNameInfo),
        (m_Device, pNameInfo) )
    SIR_VK_PROC( cmdDebugMarkerBeginEXT, vkCmdDebugMarkerBeginEXT, void,
        (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo),
        (commandBuffer, pMarkerInfo) )
    SIR_VK_PROC( cmdDebugMarkerEndEXT, vkCmdDebugMarkerEndEXT, void,
        (VkCommandBuffer commandBuffer),
        (commandBuffer) )
    SIR_VK_PROC( cmdDebugMarkerInsertEXT, vkCmdDebugMarkerInsertEXT, void,
        (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo),
        (commandBuffer, pMarkerInfo) )
#endif

#if defined(VK_EXT_transform_feedback)
    SIR_VK_PROC( cmdBindTransformFeedbackBuffersEXT, vkCmdBindTransformFeedbackBuffersEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes),
        (commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes) )
    SIR_VK_PROC( cmdBeginTransformFeedbackEXT, vkCmdBeginTransformFeedbackEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets),
        (commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets) )
    SIR_VK_PROC( cmdEndTransformFeedbackEXT, vkCmdEndTransformFeedbackEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets),
        (commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets) )
    SIR_VK_PROC( cmdBeginQueryIndexedEXT, vkCmdBeginQueryIndexedEXT, void,
        (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index),
        (commandBuffer, queryPool, query, flags, index) )
    SIR_VK_PROC( cmdEndQueryIndexedEXT, vkCmdEndQueryIndexedEXT, void,
        (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index),
        (commandBuffer, queryPool, query, index) )
    SIR_VK_PROC( cmdDrawIndirectByteCountEXT, vkCmdDrawIndirectByteCountEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride),
        (commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride) )
#endif

#if defined(VK_NVX_binary_import)
    SIR_VK_PROC( createCuModuleNVX, vkCreateCuModuleNVX, VkResult,
        (const VkCuModuleCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuModuleNVX* pModule),
        (m_Device, pCreateInfo, pAllocator, pModule) )
    SIR_VK_PROC( createCuFunctionNVX, vkCreateCuFunctionNVX, VkResult,
        (const VkCuFunctionCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuFunctionNVX* pFunction),
        (m_Device, pCreateInfo, pAllocator, pFunction) )
    SIR_VK_PROC( destroyCuModuleNVX, vkDestroyCuModuleNVX, void,
        (VkCuModuleNVX module, const VkAllocationCallbacks* pAllocator),
        (m_Device, module, pAllocator) )
    SIR_VK_PROC( destroyCuFunctionNVX, vkDestroyCuFunctionNVX, void,
        (VkCuFunctionNVX function, const VkAllocationCallbacks* pAllocator),
        (m_Device, function, pAllocator) )
    SIR_VK_PROC( cmdCuLaunchKernelNVX, vkCmdCuLaunchKernelNVX, void,
        (VkCommandBuffer commandBuffer, const VkCuLaunchInfoNVX* pLaunchInfo),
        (commandBuffer, pLaunchInfo) )
#endif

#if defined(VK_NVX_image_view_handle)
    SIR_VK_PROC( getImageViewHandleNVX, vkGetImageViewHandleNVX, uint32_t,
        (const VkImageViewHandleInfoNVX* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( getImageViewHandle64NVX, vkGetImageViewHandle64NVX, uint64_t,
        (const VkImageViewHandleInfoNVX* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( getImageViewAddressNVX, vkGetImageViewAddressNVX, VkResult,
        (VkImageView imageView, VkImageViewAddressPropertiesNVX* pProperties),
        (m_Device, imageView, pProperties) )
    SIR_VK_PROC( getDeviceCombinedImageSamplerIndexNVX, vkGetDeviceCombinedImageSamplerIndexNVX, uint64_t,
        (uint64_t imageViewIndex, uint64_t samplerIndex),
        (m_Device, imageViewIndex, samplerIndex) )
#endif

#if defined(VK_AMD_draw_indirect_count)
    SIR_VK_PROC( cmdDrawIndirectCountAMD, vkCmdDrawIndirectCountAMD, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride),
        (commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride) )
    SIR_VK_PROC( cmdDrawIndexedIndirectCountAMD, vkCmdDrawIndexedIndirectCountAMD, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride),
        (commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride) )
#endif

#if defined(VK_AMD_shader_info)
    SIR_VK_PROC( getShaderInfoAMD, vkGetShaderInfoAMD, VkResult,
        (VkPipeline pipeline, VkShaderStageFlagBits shaderStage, VkShaderInfoTypeAMD infoType, size_t* pInfoSize, void* pInfo),
        (m_Device, pipeline, shaderStage, infoType, pInfoSize, pInfo) )
#endif

#if defined(VK_EXT_conditional_rendering)
    SIR_VK_PROC( cmdBeginConditionalRenderingEXT, vkCmdBeginConditionalRenderingEXT, void,
        (VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin),
        (commandBuffer, pConditionalRenderingBegin) )
    SIR_VK_PROC( cmdEndConditionalRenderingEXT, vkCmdEndConditionalRenderingEXT, void,
        (VkCommandBuffer commandBuffer),
        (commandBuffer) )
#endif

#if defined(VK_NV_clip_space_w_scaling)
    SIR_VK_PROC( cmdSetViewportWScalingNV, vkCmdSetViewportWScalingNV, void,
        (VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportWScalingNV* pViewportWScalings),
        (commandBuffer, firstViewport, viewportCount, pViewportWScalings) )
#endif

#if defined(VK_EXT_display_control)
    SIR_VK_PROC( displayPowerControlEXT, vkDisplayPowerControlEXT, VkResult,
        (VkDisplayKHR display, const VkDisplayPowerInfoEXT* pDisplayPowerInfo),
        (m_Device, display, pDisplayPowerInfo) )
    SIR_VK_PROC( registerDeviceEventEXT, vkRegisterDeviceEventEXT, VkResult,
        (const VkDeviceEventInfoEXT* pDeviceEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence),
        (m_Device, pDeviceEventInfo, pAllocator, pFence) )
    SIR_VK_PROC( registerDisplayEventEXT, vkRegisterDisplayEventEXT, VkResult,
        (VkDisplayKHR display, const VkDisplayEventInfoEXT* pDisplayEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence),
        (m_Device, display, pDisplayEventInfo, pAllocator, pFence) )
    SIR_VK_PROC( getSwapchainCounterEXT, vkGetSwapchainCounterEXT, VkResult,
        (VkSwapchainKHR swapchain, VkSurfaceCounterFlagBitsEXT counter, uint64_t* pCounterValue),
        (m_Device, swapchain, counter, pCounterValue) )
#endif

#if defined(VK_GOOGLE_display_timing)
    SIR_VK_PROC( getRefreshCycleDurationGOOGLE, vkGetRefreshCycleDurationGOOGLE, VkResult,
        (VkSwapchainKHR swapchain, VkRefreshCycleDurationGOOGLE* pDisplayTimingProperties),
        (m_Device, swapchain, pDisplayTimingProperties) )
    SIR_VK_PROC( getPastPresentationTimingGOOGLE, vkGetPastPresentationTimingGOOGLE, VkResult,
        (VkSwapchainKHR swapchain, uint32_t* pPresentationTimingCount, VkPastPresentationTimingGOOGLE* pPresentationTimings),
        (m_Device, swapchain, pPresentationTimingCount, pPresentationTimings) )
#endif

#if defined(VK_EXT_discard_rectangles)
    SIR_VK_PROC( cmdSetDiscardRectangleEXT, vkCmdSetDiscardRectangleEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t firstDiscardRectangle, uint32_t discardRectangleCount, const VkRect2D* pDiscardRectangles),
        (commandBuffer, firstDiscardRectangle, discardRectangleCount, pDiscardRectangles) )
    SIR_VK_PROC( cmdSetDiscardRectangleEnableEXT, vkCmdSetDiscardRectangleEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 discardRectangleEnable),
        (commandBuffer, discardRectangleEnable) )
    SIR_VK_PROC( cmdSetDiscardRectangleModeEXT, vkCmdSetDiscardRectangleModeEXT, void,
        (VkCommandBuffer commandBuffer, VkDiscardRectangleModeEXT discardRectangleMode),
        (commandBuffer, discardRectangleMode) )
#endif

#if defined(VK_EXT_hdr_metadata)
    SIR_VK_PROC( setHdrMetadataEXT, vkSetHdrMetadataEXT, void,
        (uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata),
        (m_Device, swapchainCount, pSwapchains, pMetadata) )
#endif

#if defined(VK_EXT_debug_utils)
    SIR_VK_PROC( setDebugUtilsObjectNameEXT, vkSetDebugUtilsObjectNameEXT, VkResult,
        (const VkDebugUtilsObjectNameInfoEXT* pNameInfo),
        (m_Device, pNameInfo) )
    SIR_VK_PROC( setDebugUtilsObjectTagEXT, vkSetDebugUtilsObjectTagEXT, VkResult,
        (const VkDebugUtilsObjectTagInfoEXT* pTagInfo),
        (m_Device, pTagInfo) )
    SIR_VK_PROC( queueBeginDebugUtilsLabelEXT, vkQueueBeginDebugUtilsLabelEXT, void,
        (VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo),
        (queue, pLabelInfo) )
    SIR_VK_PROC( queueEndDebugUtilsLabelEXT, vkQueueEndDebugUtilsLabelEXT, void,
        (VkQueue queue),
        (queue) )
    SIR_VK_PROC( queueInsertDebugUtilsLabelEXT, vkQueueInsertDebugUtilsLabelEXT, void,
        (VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo),
        (queue, pLabelInfo) )
    SIR_VK_PROC( cmdBeginDebugUtilsLabelEXT, vkCmdBeginDebugUtilsLabelEXT, void,
        (VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo),
        (commandBuffer, pLabelInfo) )
    SIR_VK_PROC( cmdEndDebugUtilsLabelEXT, vkCmdEndDebugUtilsLabelEXT, void,
        (VkCommandBuffer commandBuffer),
        (commandBuffer) )
    SIR_VK_PROC( cmdInsertDebugUtilsLabelEXT, vkCmdInsertDebugUtilsLabelEXT, void,
        (VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo),
        (commandBuffer, pLabelInfo) )
#endif

#if defined(VK_AMD_gpa_interface)
    SIR_VK_PROC( createGpaSessionAMD, vkCreateGpaSessionAMD, VkResult,
        (const VkGpaSessionCreateInfoAMD*             pCreateInfo, const VkAllocationCallbacks* pAllocator, VkGpaSessionAMD*                             pGpaSession),
        (m_Device, pCreateInfo, pAllocator, pGpaSession) )
    SIR_VK_PROC( destroyGpaSessionAMD, vkDestroyGpaSessionAMD, void,
        (VkGpaSessionAMD gpaSession, const VkAllocationCallbacks*      pAllocator),
        (m_Device, gpaSession, pAllocator) )
    SIR_VK_PROC( setGpaDeviceClockModeAMD, vkSetGpaDeviceClockModeAMD, VkResult,
        (VkGpaDeviceClockModeInfoAMD* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( getGpaDeviceClockInfoAMD, vkGetGpaDeviceClockInfoAMD, VkResult,
        (VkGpaDeviceGetClockInfoAMD* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( cmdBeginGpaSessionAMD, vkCmdBeginGpaSessionAMD, VkResult,
        (VkCommandBuffer commandBuffer, VkGpaSessionAMD                   gpaSession),
        (commandBuffer, gpaSession) )
    SIR_VK_PROC( cmdEndGpaSessionAMD, vkCmdEndGpaSessionAMD, VkResult,
        (VkCommandBuffer commandBuffer, VkGpaSessionAMD                   gpaSession),
        (commandBuffer, gpaSession) )
    SIR_VK_PROC( cmdBeginGpaSampleAMD, vkCmdBeginGpaSampleAMD, VkResult,
        (VkCommandBuffer commandBuffer, VkGpaSessionAMD                   gpaSession, const VkGpaSampleBeginInfoAMD*    pGpaSampleBeginInfo, uint32_t*                         pSampleID),
        (commandBuffer, gpaSession, pGpaSampleBeginInfo, pSampleID) )
    SIR_VK_PROC( cmdEndGpaSampleAMD, vkCmdEndGpaSampleAMD, void,
        (VkCommandBuffer commandBuffer, VkGpaSessionAMD                   gpaSession, uint32_t                          sampleID),
        (commandBuffer, gpaSession, sampleID) )
    SIR_VK_PROC( getGpaSessionStatusAMD, vkGetGpaSessionStatusAMD, VkResult,
        (VkGpaSessionAMD gpaSession),
        (m_Device, gpaSession) )
    SIR_VK_PROC( getGpaSessionResultsAMD, vkGetGpaSessionResultsAMD, VkResult,
        (VkGpaSessionAMD                          gpaSession, uint32_t                                 sampleID, size_t*            pSizeInBytes, void* pData),
        (m_Device, gpaSession, sampleID, pSizeInBytes, pData) )
    SIR_VK_PROC( resetGpaSessionAMD, vkResetGpaSessionAMD, VkResult,
        (VkGpaSessionAMD gpaSession),
        (m_Device, gpaSession) )
    SIR_VK_PROC( cmdCopyGpaSessionResultsAMD, vkCmdCopyGpaSessionResultsAMD, void,
        (VkCommandBuffer commandBuffer, VkGpaSessionAMD                   gpaSession),
        (commandBuffer, gpaSession) )
#endif

#if defined(VK_EXT_descriptor_heap)
    SIR_VK_PROC( writeSamplerDescriptorsEXT, vkWriteSamplerDescriptorsEXT, VkResult,
        (uint32_t                                            samplerCount, const VkSamplerCreateInfo*       pSamplers, const VkHostAddressRangeEXT*     pDescriptors),
        (m_Device, samplerCount, pSamplers, pDescriptors) )
    SIR_VK_PROC( writeResourceDescriptorsEXT, vkWriteResourceDescriptorsEXT, VkResult,
        (uint32_t                                                resourceCount, const VkResourceDescriptorInfoEXT*  pResources, const VkHostAddressRangeEXT*        pDescriptors),
        (m_Device, resourceCount, pResources, pDescriptors) )
    SIR_VK_PROC( cmdBindSamplerHeapEXT, vkCmdBindSamplerHeapEXT, void,
        (VkCommandBuffer                   commandBuffer, const VkBindHeapInfoEXT*                            pBindInfo),
        (commandBuffer, pBindInfo) )
    SIR_VK_PROC( cmdBindResourceHeapEXT, vkCmdBindResourceHeapEXT, void,
        (VkCommandBuffer                   commandBuffer, const VkBindHeapInfoEXT*                            pBindInfo),
        (commandBuffer, pBindInfo) )
    SIR_VK_PROC( cmdPushDataEXT, vkCmdPushDataEXT, void,
        (VkCommandBuffer                   commandBuffer, const VkPushDataInfoEXT*                            pPushDataInfo),
        (commandBuffer, pPushDataInfo) )
    SIR_VK_PROC( getImageOpaqueCaptureDataEXT, vkGetImageOpaqueCaptureDataEXT, VkResult,
        (uint32_t                                            imageCount, const VkImage*                     pImages, VkHostAddressRangeEXT*             pDatas),
        (m_Device, imageCount, pImages, pDatas) )
    SIR_VK_PROC( registerCustomBorderColorEXT, vkRegisterCustomBorderColorEXT, VkResult,
        (const VkSamplerCustomBorderColorCreateInfoEXT*      pBorderColor, VkBool32                                            requestIndex, uint32_t*                                           pIndex),
        (m_Device, pBorderColor, requestIndex, pIndex) )
    SIR_VK_PROC( unregisterCustomBorderColorEXT, vkUnregisterCustomBorderColorEXT, void,
        (uint32_t                                            index),
        (m_Device, index) )
    SIR_VK_PROC( getTensorOpaqueCaptureDataARM, vkGetTensorOpaqueCaptureDataARM, VkResult,
        (uint32_t                                            tensorCount, const VkTensorARM*                pTensors, VkHostAddressRangeEXT*            pDatas),
        (m_Device, tensorCount, pTensors, pDatas) )
#endif

#if defined(VK_EXT_sample_locations)
    SIR_VK_PROC( cmdSetSampleLocationsEXT, vkCmdSetSampleLocationsEXT, void,
        (VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo),
        (commandBuffer, pSampleLocationsInfo) )
#endif

#if defined(VK_EXT_image_drm_format_modifier)
    SIR_VK_PROC( getImageDrmFormatModifierPropertiesEXT, vkGetImageDrmFormatModifierPropertiesEXT, VkResult,
        (VkImage image, VkImageDrmFormatModifierPropertiesEXT* pProperties),
        (m_Device, image, pProperties) )
#endif

#if defined(VK_EXT_validation_cache)
    SIR_VK_PROC( createValidationCacheEXT, vkCreateValidationCacheEXT, VkResult,
        (const VkValidationCacheCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkValidationCacheEXT* pValidationCache),
        (m_Device, pCreateInfo, pAllocator, pValidationCache) )
    SIR_VK_PROC( destroyValidationCacheEXT, vkDestroyValidationCacheEXT, void,
        (VkValidationCacheEXT validationCache, const VkAllocationCallbacks* pAllocator),
        (m_Device, validationCache, pAllocator) )
    SIR_VK_PROC( mergeValidationCachesEXT, vkMergeValidationCachesEXT, VkResult,
        (VkValidationCacheEXT dstCache, uint32_t srcCacheCount, const VkValidationCacheEXT* pSrcCaches),
        (m_Device, dstCache, srcCacheCount, pSrcCaches) )
    SIR_VK_PROC( getValidationCacheDataEXT, vkGetValidationCacheDataEXT, VkResult,
        (VkValidationCacheEXT validationCache, size_t* pDataSize, void* pData),
        (m_Device, validationCache, pDataSize, pData) )
#endif

#if defined(VK_NV_shading_rate_image)
    SIR_VK_PROC( cmdBindShadingRateImageNV, vkCmdBindShadingRateImageNV, void,
        (VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout),
        (commandBuffer, imageView, imageLayout) )
    SIR_VK_PROC( cmdSetViewportShadingRatePaletteNV, vkCmdSetViewportShadingRatePaletteNV, void,
        (VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkShadingRatePaletteNV* pShadingRatePalettes),
        (commandBuffer, firstViewport, viewportCount, pShadingRatePalettes) )
    SIR_VK_PROC( cmdSetCoarseSampleOrderNV, vkCmdSetCoarseSampleOrderNV, void,
        (VkCommandBuffer commandBuffer, VkCoarseSampleOrderTypeNV sampleOrderType, uint32_t customSampleOrderCount, const VkCoarseSampleOrderCustomNV* pCustomSampleOrders),
        (commandBuffer, sampleOrderType, customSampleOrderCount, pCustomSampleOrders) )
#endif

#if defined(VK_NV_ray_tracing)
    SIR_VK_PROC( createAccelerationStructureNV, vkCreateAccelerationStructureNV, VkResult,
        (const VkAccelerationStructureCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureNV* pAccelerationStructure),
        (m_Device, pCreateInfo, pAllocator, pAccelerationStructure) )
    SIR_VK_PROC( destroyAccelerationStructureNV, vkDestroyAccelerationStructureNV, void,
        (VkAccelerationStructureNV accelerationStructure, const VkAllocationCallbacks* pAllocator),
        (m_Device, accelerationStructure, pAllocator) )
    SIR_VK_PROC( getAccelerationStructureMemoryRequirementsNV, vkGetAccelerationStructureMemoryRequirementsNV, void,
        (const VkAccelerationStructureMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( bindAccelerationStructureMemoryNV, vkBindAccelerationStructureMemoryNV, VkResult,
        (uint32_t bindInfoCount, const VkBindAccelerationStructureMemoryInfoNV* pBindInfos),
        (m_Device, bindInfoCount, pBindInfos) )
    SIR_VK_PROC( cmdBuildAccelerationStructureNV, vkCmdBuildAccelerationStructureNV, void,
        (VkCommandBuffer commandBuffer, const VkAccelerationStructureInfoNV* pInfo, VkBuffer instanceData, VkDeviceSize instanceOffset, VkBool32 update, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkBuffer scratch, VkDeviceSize scratchOffset),
        (commandBuffer, pInfo, instanceData, instanceOffset, update, dst, src, scratch, scratchOffset) )
    SIR_VK_PROC( cmdCopyAccelerationStructureNV, vkCmdCopyAccelerationStructureNV, void,
        (VkCommandBuffer commandBuffer, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkCopyAccelerationStructureModeKHR mode),
        (commandBuffer, dst, src, mode) )
    SIR_VK_PROC( cmdTraceRaysNV, vkCmdTraceRaysNV, void,
        (VkCommandBuffer commandBuffer, VkBuffer raygenShaderBindingTableBuffer, VkDeviceSize raygenShaderBindingOffset, VkBuffer missShaderBindingTableBuffer, VkDeviceSize missShaderBindingOffset, VkDeviceSize missShaderBindingStride, VkBuffer hitShaderBindingTableBuffer, VkDeviceSize hitShaderBindingOffset, VkDeviceSize hitShaderBindingStride, VkBuffer callableShaderBindingTableBuffer, VkDeviceSize callableShaderBindingOffset, VkDeviceSize callableShaderBindingStride, uint32_t width, uint32_t height, uint32_t depth),
        (commandBuffer, raygenShaderBindingTableBuffer, raygenShaderBindingOffset, missShaderBindingTableBuffer, missShaderBindingOffset, missShaderBindingStride, hitShaderBindingTableBuffer, hitShaderBindingOffset, hitShaderBindingStride, callableShaderBindingTableBuffer, callableShaderBindingOffset, callableShaderBindingStride, width, height, depth) )
    SIR_VK_PROC( createRayTracingPipelinesNV, vkCreateRayTracingPipelinesNV, VkResult,
        (VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoNV* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines),
        (m_Device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines) )
    SIR_VK_PROC( getRayTracingShaderGroupHandlesKHR, vkGetRayTracingShaderGroupHandlesKHR, VkResult,
        (VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData),
        (m_Device, pipeline, firstGroup, groupCount, dataSize, pData) )
    SIR_VK_PROC( getRayTracingShaderGroupHandlesNV, vkGetRayTracingShaderGroupHandlesNV, VkResult,
        (VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData),
        (m_Device, pipeline, firstGroup, groupCount, dataSize, pData) )
    SIR_VK_PROC( getAccelerationStructureHandleNV, vkGetAccelerationStructureHandleNV, VkResult,
        (VkAccelerationStructureNV accelerationStructure, size_t dataSize, void* pData),
        (m_Device, accelerationStructure, dataSize, pData) )
    SIR_VK_PROC( cmdWriteAccelerationStructuresPropertiesNV, vkCmdWriteAccelerationStructuresPropertiesNV, void,
        (VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureNV* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery),
        (commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery) )
    SIR_VK_PROC( compileDeferredNV, vkCompileDeferredNV, VkResult,
        (VkPipeline pipeline, uint32_t shader),
        (m_Device, pipeline, shader) )
#endif

#if defined(VK_EXT_external_memory_host)
    SIR_VK_PROC( getMemoryHostPointerPropertiesEXT, vkGetMemoryHostPointerPropertiesEXT, VkResult,
        (VkExternalMemoryHandleTypeFlagBits handleType, const void* pHostPointer, VkMemoryHostPointerPropertiesEXT* pMemoryHostPointerProperties),
        (m_Device, handleType, pHostPointer, pMemoryHostPointerProperties) )
#endif

#if defined(VK_AMD_buffer_marker)
    SIR_VK_PROC( cmdWriteBufferMarkerAMD, vkCmdWriteBufferMarkerAMD, void,
        (VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker),
        (commandBuffer, pipelineStage, dstBuffer, dstOffset, marker) )
    SIR_VK_PROC( cmdWriteBufferMarker2AMD, vkCmdWriteBufferMarker2AMD, void,
        (VkCommandBuffer                   commandBuffer, VkPipelineStageFlags2               stage, VkBuffer                                            dstBuffer, VkDeviceSize                                        dstOffset, uint32_t                                            marker),
        (commandBuffer, stage, dstBuffer, dstOffset, marker) )
#endif

#if defined(VK_EXT_calibrated_timestamps)
    SIR_VK_PROC( getCalibratedTimestampsEXT, vkGetCalibratedTimestampsEXT, VkResult,
        (uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation),
        (m_Device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation) )
#endif

#if defined(VK_NV_mesh_shader)
    SIR_VK_PROC( cmdDrawMeshTasksNV, vkCmdDrawMeshTasksNV, void,
        (VkCommandBuffer commandBuffer, uint32_t taskCount, uint32_t firstTask),
        (commandBuffer, taskCount, firstTask) )
    SIR_VK_PROC( cmdDrawMeshTasksIndirectNV, vkCmdDrawMeshTasksIndirectNV, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride),
        (commandBuffer, buffer, offset, drawCount, stride) )
    SIR_VK_PROC( cmdDrawMeshTasksIndirectCountNV, vkCmdDrawMeshTasksIndirectCountNV, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride),
        (commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride) )
#endif

#if defined(VK_NV_scissor_exclusive)
    SIR_VK_PROC( cmdSetExclusiveScissorEnableNV, vkCmdSetExclusiveScissorEnableNV, void,
        (VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkBool32* pExclusiveScissorEnables),
        (commandBuffer, firstExclusiveScissor, exclusiveScissorCount, pExclusiveScissorEnables) )
    SIR_VK_PROC( cmdSetExclusiveScissorNV, vkCmdSetExclusiveScissorNV, void,
        (VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkRect2D* pExclusiveScissors),
        (commandBuffer, firstExclusiveScissor, exclusiveScissorCount, pExclusiveScissors) )
#endif

#if defined(VK_NV_device_diagnostic_checkpoints)
    SIR_VK_PROC( cmdSetCheckpointNV, vkCmdSetCheckpointNV, void,
        (VkCommandBuffer commandBuffer, const void* pCheckpointMarker),
        (commandBuffer, pCheckpointMarker) )
    SIR_VK_PROC( getQueueCheckpointDataNV, vkGetQueueCheckpointDataNV, void,
        (VkQueue queue, uint32_t* pCheckpointDataCount, VkCheckpointDataNV* pCheckpointData),
        (queue, pCheckpointDataCount, pCheckpointData) )
    SIR_VK_PROC( getQueueCheckpointData2NV, vkGetQueueCheckpointData2NV, void,
        (VkQueue queue, uint32_t* pCheckpointDataCount, VkCheckpointData2NV* pCheckpointData),
        (queue, pCheckpointDataCount, pCheckpointData) )
#endif

#if defined(VK_EXT_present_timing)
    SIR_VK_PROC( setSwapchainPresentTimingQueueSizeEXT, vkSetSwapchainPresentTimingQueueSizeEXT, VkResult,
        (VkSwapchainKHR swapchain, uint32_t size),
        (m_Device, swapchain, size) )
    SIR_VK_PROC( getSwapchainTimingPropertiesEXT, vkGetSwapchainTimingPropertiesEXT, VkResult,
        (VkSwapchainKHR swapchain, VkSwapchainTimingPropertiesEXT* pSwapchainTimingProperties, uint64_t* pSwapchainTimingPropertiesCounter),
        (m_Device, swapchain, pSwapchainTimingProperties, pSwapchainTimingPropertiesCounter) )
    SIR_VK_PROC( getSwapchainTimeDomainPropertiesEXT, vkGetSwapchainTimeDomainPropertiesEXT, VkResult,
        (VkSwapchainKHR swapchain, VkSwapchainTimeDomainPropertiesEXT* pSwapchainTimeDomainProperties, uint64_t* pTimeDomainsCounter),
        (m_Device, swapchain, pSwapchainTimeDomainProperties, pTimeDomainsCounter) )
    SIR_VK_PROC( getPastPresentationTimingEXT, vkGetPastPresentationTimingEXT, VkResult,
        (const VkPastPresentationTimingInfoEXT* pPastPresentationTimingInfo, VkPastPresentationTimingPropertiesEXT* pPastPresentationTimingProperties),
        (m_Device, pPastPresentationTimingInfo, pPastPresentationTimingProperties) )
#endif

#if defined(VK_INTEL_performance_query)
    SIR_VK_PROC( initializePerformanceApiINTEL, vkInitializePerformanceApiINTEL, VkResult,
        (const VkInitializePerformanceApiInfoINTEL* pInitializeInfo),
        (m_Device, pInitializeInfo) )
    SIR_VK_PROC( uninitializePerformanceApiINTEL, vkUninitializePerformanceApiINTEL, void,
        (),
        (m_Device) )
    SIR_VK_PROC( cmdSetPerformanceMarkerINTEL, vkCmdSetPerformanceMarkerINTEL, VkResult,
        (VkCommandBuffer commandBuffer, const VkPerformanceMarkerInfoINTEL* pMarkerInfo),
        (commandBuffer, pMarkerInfo) )
    SIR_VK_PROC( cmdSetPerformanceStreamMarkerINTEL, vkCmdSetPerformanceStreamMarkerINTEL, VkResult,
        (VkCommandBuffer commandBuffer, const VkPerformanceStreamMarkerInfoINTEL* pMarkerInfo),
        (commandBuffer, pMarkerInfo) )
    SIR_VK_PROC( cmdSetPerformanceOverrideINTEL, vkCmdSetPerformanceOverrideINTEL, VkResult,
        (VkCommandBuffer commandBuffer, const VkPerformanceOverrideInfoINTEL* pOverrideInfo),
        (commandBuffer, pOverrideInfo) )
    SIR_VK_PROC( acquirePerformanceConfigurationINTEL, vkAcquirePerformanceConfigurationINTEL, VkResult,
        (const VkPerformanceConfigurationAcquireInfoINTEL* pAcquireInfo, VkPerformanceConfigurationINTEL* pConfiguration),
        (m_Device, pAcquireInfo, pConfiguration) )
    SIR_VK_PROC( releasePerformanceConfigurationINTEL, vkReleasePerformanceConfigurationINTEL, VkResult,
        (VkPerformanceConfigurationINTEL configuration),
        (m_Device, configuration) )
    SIR_VK_PROC( queueSetPerformanceConfigurationINTEL, vkQueueSetPerformanceConfigurationINTEL, VkResult,
        (VkQueue queue, VkPerformanceConfigurationINTEL configuration),
        (queue, configuration) )
    SIR_VK_PROC( getPerformanceParameterINTEL, vkGetPerformanceParameterINTEL, VkResult,
        (VkPerformanceParameterTypeINTEL parameter, VkPerformanceValueINTEL* pValue),
        (m_Device, parameter, pValue) )
#endif

#if defined(VK_AMD_display_native_hdr)
    SIR_VK_PROC( setLocalDimmingAMD, vkSetLocalDimmingAMD, void,
        (VkSwapchainKHR swapChain, VkBool32 localDimmingEnable),
        (m_Device, swapChain, localDimmingEnable) )
#endif

#if defined(VK_EXT_buffer_device_address)
    SIR_VK_PROC( getBufferDeviceAddressEXT, vkGetBufferDeviceAddressEXT, VkDeviceAddress,
        (const VkBufferDeviceAddressInfo* pInfo),
        (m_Device, pInfo) )
#endif

#if defined(VK_EXT_line_rasterization)
    SIR_VK_PROC( cmdSetLineStippleEXT, vkCmdSetLineStippleEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern),
        (commandBuffer, lineStippleFactor, lineStipplePattern) )
#endif

#if defined(VK_EXT_host_query_reset)
    SIR_VK_PROC( resetQueryPoolEXT, vkResetQueryPoolEXT, void,
        (VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount),
        (m_Device, queryPool, firstQuery, queryCount) )
#endif

#if defined(VK_EXT_extended_dynamic_state)
    SIR_VK_PROC( cmdSetCullModeEXT, vkCmdSetCullModeEXT, void,
        (VkCommandBuffer commandBuffer, VkCullModeFlags cullMode),
        (commandBuffer, cullMode) )
    SIR_VK_PROC( cmdSetFrontFaceEXT, vkCmdSetFrontFaceEXT, void,
        (VkCommandBuffer commandBuffer, VkFrontFace frontFace),
        (commandBuffer, frontFace) )
    SIR_VK_PROC( cmdSetPrimitiveTopologyEXT, vkCmdSetPrimitiveTopologyEXT, void,
        (VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology),
        (commandBuffer, primitiveTopology) )
    SIR_VK_PROC( cmdSetViewportWithCountEXT, vkCmdSetViewportWithCountEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports),
        (commandBuffer, viewportCount, pViewports) )
    SIR_VK_PROC( cmdSetScissorWithCountEXT, vkCmdSetScissorWithCountEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors),
        (commandBuffer, scissorCount, pScissors) )
    SIR_VK_PROC( cmdBindVertexBuffers2EXT, vkCmdBindVertexBuffers2EXT, void,
        (VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides),
        (commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides) )
    SIR_VK_PROC( cmdSetDepthTestEnableEXT, vkCmdSetDepthTestEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthTestEnable),
        (commandBuffer, depthTestEnable) )
    SIR_VK_PROC( cmdSetDepthWriteEnableEXT, vkCmdSetDepthWriteEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable),
        (commandBuffer, depthWriteEnable) )
    SIR_VK_PROC( cmdSetDepthCompareOpEXT, vkCmdSetDepthCompareOpEXT, void,
        (VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp),
        (commandBuffer, depthCompareOp) )
    SIR_VK_PROC( cmdSetDepthBoundsTestEnableEXT, vkCmdSetDepthBoundsTestEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable),
        (commandBuffer, depthBoundsTestEnable) )
    SIR_VK_PROC( cmdSetStencilTestEnableEXT, vkCmdSetStencilTestEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable),
        (commandBuffer, stencilTestEnable) )
    SIR_VK_PROC( cmdSetStencilOpEXT, vkCmdSetStencilOpEXT, void,
        (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp),
        (commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp) )
#endif

#if defined(VK_EXT_host_image_copy)
    SIR_VK_PROC( copyMemoryToImageEXT, vkCopyMemoryToImageEXT, VkResult,
        (const VkCopyMemoryToImageInfo*    pCopyMemoryToImageInfo),
        (m_Device, pCopyMemoryToImageInfo) )
    SIR_VK_PROC( copyImageToMemoryEXT, vkCopyImageToMemoryEXT, VkResult,
        (const VkCopyImageToMemoryInfo*    pCopyImageToMemoryInfo),
        (m_Device, pCopyImageToMemoryInfo) )
    SIR_VK_PROC( copyImageToImageEXT, vkCopyImageToImageEXT, VkResult,
        (const VkCopyImageToImageInfo*    pCopyImageToImageInfo),
        (m_Device, pCopyImageToImageInfo) )
    SIR_VK_PROC( transitionImageLayoutEXT, vkTransitionImageLayoutEXT, VkResult,
        (uint32_t transitionCount, const VkHostImageLayoutTransitionInfo*    pTransitions),
        (m_Device, transitionCount, pTransitions) )
    SIR_VK_PROC( getImageSubresourceLayout2EXT, vkGetImageSubresourceLayout2EXT, void,
        (VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout),
        (m_Device, image, pSubresource, pLayout) )
#endif

#if defined(VK_EXT_swapchain_maintenance1)
    SIR_VK_PROC( releaseSwapchainImagesEXT, vkReleaseSwapchainImagesEXT, VkResult,
        (const VkReleaseSwapchainImagesInfoKHR* pReleaseInfo),
        (m_Device, pReleaseInfo) )
#endif

#if defined(VK_NV_device_generated_commands)
    SIR_VK_PROC( getGeneratedCommandsMemoryRequirementsNV, vkGetGeneratedCommandsMemoryRequirementsNV, void,
        (const VkGeneratedCommandsMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( cmdPreprocessGeneratedCommandsNV, vkCmdPreprocessGeneratedCommandsNV, void,
        (VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo),
        (commandBuffer, pGeneratedCommandsInfo) )
    SIR_VK_PROC( cmdExecuteGeneratedCommandsNV, vkCmdExecuteGeneratedCommandsNV, void,
        (VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo),
        (commandBuffer, isPreprocessed, pGeneratedCommandsInfo) )
    SIR_VK_PROC( cmdBindPipelineShaderGroupNV, vkCmdBindPipelineShaderGroupNV, void,
        (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline, uint32_t groupIndex),
        (commandBuffer, pipelineBindPoint, pipeline, groupIndex) )
    SIR_VK_PROC( createIndirectCommandsLayoutNV, vkCreateIndirectCommandsLayoutNV, VkResult,
        (const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkIndirectCommandsLayoutNV* pIndirectCommandsLayout),
        (m_Device, pCreateInfo, pAllocator, pIndirectCommandsLayout) )
    SIR_VK_PROC( destroyIndirectCommandsLayoutNV, vkDestroyIndirectCommandsLayoutNV, void,
        (VkIndirectCommandsLayoutNV indirectCommandsLayout, const VkAllocationCallbacks* pAllocator),
        (m_Device, indirectCommandsLayout, pAllocator) )
#endif

#if defined(VK_EXT_depth_bias_control)
    SIR_VK_PROC( cmdSetDepthBias2EXT, vkCmdSetDepthBias2EXT, void,
        (VkCommandBuffer commandBuffer, const VkDepthBiasInfoEXT*         pDepthBiasInfo),
        (commandBuffer, pDepthBiasInfo) )
#endif

#if defined(VK_EXT_private_data)
    SIR_VK_PROC( createPrivateDataSlotEXT, vkCreatePrivateDataSlotEXT, VkResult,
        (const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot),
        (m_Device, pCreateInfo, pAllocator, pPrivateDataSlot) )
    SIR_VK_PROC( destroyPrivateDataSlotEXT, vkDestroyPrivateDataSlotEXT, void,
        (VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator),
        (m_Device, privateDataSlot, pAllocator) )
    SIR_VK_PROC( setPrivateDataEXT, vkSetPrivateDataEXT, VkResult,
        (VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data),
        (m_Device, objectType, objectHandle, privateDataSlot, data) )
    SIR_VK_PROC( getPrivateDataEXT, vkGetPrivateDataEXT, void,
        (VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData),
        (m_Device, objectType, objectHandle, privateDataSlot, pData) )
#endif

#if defined(VK_QCOM_queue_perf_hint)
    SIR_VK_PROC( queueSetPerfHintQCOM, vkQueueSetPerfHintQCOM, VkResult,
        (VkQueue queue, const VkPerfHintInfoQCOM*  pPerfHintInfo),
        (queue, pPerfHintInfo) )
#endif

#if defined(VK_QCOM_tile_shading)
    SIR_VK_PROC( cmdDispatchTileQCOM, vkCmdDispatchTileQCOM, void,
        (VkCommandBuffer commandBuffer, const VkDispatchTileInfoQCOM* pDispatchTileInfo),
        (commandBuffer, pDispatchTileInfo) )
    SIR_VK_PROC( cmdBeginPerTileExecutionQCOM, vkCmdBeginPerTileExecutionQCOM, void,
        (VkCommandBuffer commandBuffer, const VkPerTileBeginInfoQCOM* pPerTileBeginInfo),
        (commandBuffer, pPerTileBeginInfo) )
    SIR_VK_PROC( cmdEndPerTileExecutionQCOM, vkCmdEndPerTileExecutionQCOM, void,
        (VkCommandBuffer commandBuffer, const VkPerTileEndInfoQCOM* pPerTileEndInfo),
        (commandBuffer, pPerTileEndInfo) )
#endif

#if defined(VK_NV_low_latency)
    SIR_VK_PROC( setLatencySleepModeLegacyNV, vkSetLatencySleepModeLegacyNV, void,
        (VkBool32 lowLatencyMode, VkBool32 lowLatencyBoost, uint32_t minimumIntervalUs),
        (m_Device, lowLatencyMode, lowLatencyBoost, minimumIntervalUs) )
    SIR_VK_PROC( latencySleepLegacyNV, vkLatencySleepLegacyNV, void,
        (VkSemaphore signalSemaphore, uint64_t value),
        (m_Device, signalSemaphore, value) )
    SIR_VK_PROC( setLatencyMarkerLegacyNV, vkSetLatencyMarkerLegacyNV, void,
        (uint64_t frameID, uint32_t marker),
        (m_Device, frameID, marker) )
    SIR_VK_PROC( getLatencyTimingsLegacyNV, vkGetLatencyTimingsLegacyNV, void,
        (void* pTimings),
        (m_Device, pTimings) )
    SIR_VK_PROC( queueNotifyOutOfBandLegacyNV, vkQueueNotifyOutOfBandLegacyNV, void,
        (VkQueue queue, uint32_t queueType),
        (queue, queueType) )
    SIR_VK_PROC( getSleepStatusLegacyNV, vkGetSleepStatusLegacyNV, void,
        (VkBool32* pLowLatencyMode),
        (m_Device, pLowLatencyMode) )
    SIR_VK_PROC( shutdownLatencyDeviceLegacyNV, vkShutdownLatencyDeviceLegacyNV, void,
        (),
        (m_Device) )
#endif

#if defined(VK_EXT_descriptor_buffer)
    SIR_VK_PROC( getDescriptorSetLayoutSizeEXT, vkGetDescriptorSetLayoutSizeEXT, void,
        (VkDescriptorSetLayout layout, VkDeviceSize* pLayoutSizeInBytes),
        (m_Device, layout, pLayoutSizeInBytes) )
    SIR_VK_PROC( getDescriptorSetLayoutBindingOffsetEXT, vkGetDescriptorSetLayoutBindingOffsetEXT, void,
        (VkDescriptorSetLayout layout, uint32_t binding, VkDeviceSize* pOffset),
        (m_Device, layout, binding, pOffset) )
    SIR_VK_PROC( getDescriptorEXT, vkGetDescriptorEXT, void,
        (const VkDescriptorGetInfoEXT* pDescriptorInfo, size_t dataSize, void* pDescriptor),
        (m_Device, pDescriptorInfo, dataSize, pDescriptor) )
    SIR_VK_PROC( cmdBindDescriptorBuffersEXT, vkCmdBindDescriptorBuffersEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t bufferCount, const VkDescriptorBufferBindingInfoEXT* pBindingInfos),
        (commandBuffer, bufferCount, pBindingInfos) )
    SIR_VK_PROC( cmdSetDescriptorBufferOffsetsEXT, vkCmdSetDescriptorBufferOffsetsEXT, void,
        (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const uint32_t* pBufferIndices, const VkDeviceSize* pOffsets),
        (commandBuffer, pipelineBindPoint, layout, firstSet, setCount, pBufferIndices, pOffsets) )
    SIR_VK_PROC( cmdBindDescriptorBufferEmbeddedSamplersEXT, vkCmdBindDescriptorBufferEmbeddedSamplersEXT, void,
        (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set),
        (commandBuffer, pipelineBindPoint, layout, set) )
    SIR_VK_PROC( getBufferOpaqueCaptureDescriptorDataEXT, vkGetBufferOpaqueCaptureDescriptorDataEXT, VkResult,
        (const VkBufferCaptureDescriptorDataInfoEXT* pInfo, void* pData),
        (m_Device, pInfo, pData) )
    SIR_VK_PROC( getImageOpaqueCaptureDescriptorDataEXT, vkGetImageOpaqueCaptureDescriptorDataEXT, VkResult,
        (const VkImageCaptureDescriptorDataInfoEXT* pInfo, void* pData),
        (m_Device, pInfo, pData) )
    SIR_VK_PROC( getImageViewOpaqueCaptureDescriptorDataEXT, vkGetImageViewOpaqueCaptureDescriptorDataEXT, VkResult,
        (const VkImageViewCaptureDescriptorDataInfoEXT* pInfo, void* pData),
        (m_Device, pInfo, pData) )
    SIR_VK_PROC( getSamplerOpaqueCaptureDescriptorDataEXT, vkGetSamplerOpaqueCaptureDescriptorDataEXT, VkResult,
        (const VkSamplerCaptureDescriptorDataInfoEXT* pInfo, void* pData),
        (m_Device, pInfo, pData) )
    SIR_VK_PROC( getAccelerationStructureOpaqueCaptureDescriptorDataEXT, vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT, VkResult,
        (const VkAccelerationStructureCaptureDescriptorDataInfoEXT* pInfo, void* pData),
        (m_Device, pInfo, pData) )
#endif

#if defined(VK_NV_fragment_shading_rate_enums)
    SIR_VK_PROC( cmdSetFragmentShadingRateEnumNV, vkCmdSetFragmentShadingRateEnumNV, void,
        (VkCommandBuffer           commandBuffer, VkFragmentShadingRateNV                     shadingRate, const VkFragmentShadingRateCombinerOpKHR    combinerOps[2]),
        (commandBuffer, shadingRate, combinerOps) )
#endif

#if defined(VK_EXT_device_fault)
    SIR_VK_PROC( getDeviceFaultInfoEXT, vkGetDeviceFaultInfoEXT, VkResult,
        (VkDeviceFaultCountsEXT* pFaultCounts, VkDeviceFaultInfoEXT* pFaultInfo),
        (m_Device, pFaultCounts, pFaultInfo) )
#endif

#if defined(VK_EXT_vertex_input_dynamic_state)
    SIR_VK_PROC( cmdSetVertexInputEXT, vkCmdSetVertexInputEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions),
        (commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions) )
#endif

#if defined(VK_HUAWEI_subpass_shading)
    SIR_VK_PROC( getDeviceSubpassShadingMaxWorkgroupSizeHUAWEI, vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI, VkResult,
        (VkRenderPass renderpass, VkExtent2D* pMaxWorkgroupSize),
        (m_Device, renderpass, pMaxWorkgroupSize) )
    SIR_VK_PROC( cmdSubpassShadingHUAWEI, vkCmdSubpassShadingHUAWEI, void,
        (VkCommandBuffer commandBuffer),
        (commandBuffer) )
#endif

#if defined(VK_HUAWEI_invocation_mask)
    SIR_VK_PROC( cmdBindInvocationMaskHUAWEI, vkCmdBindInvocationMaskHUAWEI, void,
        (VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout),
        (commandBuffer, imageView, imageLayout) )
#endif

#if defined(VK_NV_external_memory_rdma)
    SIR_VK_PROC( getMemoryRemoteAddressNV, vkGetMemoryRemoteAddressNV, VkResult,
        (const VkMemoryGetRemoteAddressInfoNV* pMemoryGetRemoteAddressInfo, VkRemoteAddressNV* pAddress),
        (m_Device, pMemoryGetRemoteAddressInfo, pAddress) )
#endif

#if defined(VK_EXT_pipeline_properties)
    SIR_VK_PROC( getPipelinePropertiesEXT, vkGetPipelinePropertiesEXT, VkResult,
        (const VkPipelineInfoKHR* pPipelineInfo, VkBaseOutStructure* pPipelineProperties),
        (m_Device, pPipelineInfo, pPipelineProperties) )
#endif

#if defined(VK_EXT_extended_dynamic_state2)
    SIR_VK_PROC( cmdSetPatchControlPointsEXT, vkCmdSetPatchControlPointsEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t patchControlPoints),
        (commandBuffer, patchControlPoints) )
    SIR_VK_PROC( cmdSetRasterizerDiscardEnableEXT, vkCmdSetRasterizerDiscardEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable),
        (commandBuffer, rasterizerDiscardEnable) )
    SIR_VK_PROC( cmdSetDepthBiasEnableEXT, vkCmdSetDepthBiasEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable),
        (commandBuffer, depthBiasEnable) )
    SIR_VK_PROC( cmdSetLogicOpEXT, vkCmdSetLogicOpEXT, void,
        (VkCommandBuffer commandBuffer, VkLogicOp logicOp),
        (commandBuffer, logicOp) )
    SIR_VK_PROC( cmdSetPrimitiveRestartEnableEXT, vkCmdSetPrimitiveRestartEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable),
        (commandBuffer, primitiveRestartEnable) )
#endif

#if defined(VK_EXT_color_write_enable)
    SIR_VK_PROC( cmdSetColorWriteEnableEXT, vkCmdSetColorWriteEnableEXT, void,
        (VkCommandBuffer       commandBuffer, uint32_t                                attachmentCount, const VkBool32*   pColorWriteEnables),
        (commandBuffer, attachmentCount, pColorWriteEnables) )
#endif

#if defined(VK_EXT_multi_draw)
    SIR_VK_PROC( cmdDrawMultiEXT, vkCmdDrawMultiEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawInfoEXT* pVertexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride),
        (commandBuffer, drawCount, pVertexInfo, instanceCount, firstInstance, stride) )
    SIR_VK_PROC( cmdDrawMultiIndexedEXT, vkCmdDrawMultiIndexedEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawIndexedInfoEXT* pIndexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride, const int32_t* pVertexOffset),
        (commandBuffer, drawCount, pIndexInfo, instanceCount, firstInstance, stride, pVertexOffset) )
#endif

#if defined(VK_EXT_opacity_micromap)
    SIR_VK_PROC( createMicromapEXT, vkCreateMicromapEXT, VkResult,
        (const VkMicromapCreateInfoEXT*                     pCreateInfo, const VkAllocationCallbacks*       pAllocator, VkMicromapEXT*                                     pMicromap),
        (m_Device, pCreateInfo, pAllocator, pMicromap) )
    SIR_VK_PROC( destroyMicromapEXT, vkDestroyMicromapEXT, void,
        (VkMicromapEXT micromap, const VkAllocationCallbacks*    pAllocator),
        (m_Device, micromap, pAllocator) )
    SIR_VK_PROC( cmdBuildMicromapsEXT, vkCmdBuildMicromapsEXT, void,
        (VkCommandBuffer             commandBuffer, uint32_t                                      infoCount, const VkMicromapBuildInfoEXT* pInfos),
        (commandBuffer, infoCount, pInfos) )
    SIR_VK_PROC( buildMicromapsEXT, vkBuildMicromapsEXT, VkResult,
        (VkDeferredOperationKHR        deferredOperation, uint32_t                                      infoCount, const VkMicromapBuildInfoEXT* pInfos),
        (m_Device, deferredOperation, infoCount, pInfos) )
    SIR_VK_PROC( copyMicromapEXT, vkCopyMicromapEXT, VkResult,
        (VkDeferredOperationKHR deferredOperation, const VkCopyMicromapInfoEXT*           pInfo),
        (m_Device, deferredOperation, pInfo) )
    SIR_VK_PROC( copyMicromapToMemoryEXT, vkCopyMicromapToMemoryEXT, VkResult,
        (VkDeferredOperationKHR deferredOperation, const VkCopyMicromapToMemoryInfoEXT*   pInfo),
        (m_Device, deferredOperation, pInfo) )
    SIR_VK_PROC( copyMemoryToMicromapEXT, vkCopyMemoryToMicromapEXT, VkResult,
        (VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToMicromapInfoEXT*   pInfo),
        (m_Device, deferredOperation, pInfo) )
    SIR_VK_PROC( writeMicromapsPropertiesEXT, vkWriteMicromapsPropertiesEXT, VkResult,
        (uint32_t                                 micromapCount, const VkMicromapEXT* pMicromaps, VkQueryType                              queryType, size_t                                   dataSize, void*                     pData, size_t                                   stride),
        (m_Device, micromapCount, pMicromaps, queryType, dataSize, pData, stride) )
    SIR_VK_PROC( cmdCopyMicromapEXT, vkCmdCopyMicromapEXT, void,
        (VkCommandBuffer commandBuffer, const VkCopyMicromapInfoEXT*      pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdCopyMicromapToMemoryEXT, vkCmdCopyMicromapToMemoryEXT, void,
        (VkCommandBuffer    commandBuffer, const VkCopyMicromapToMemoryInfoEXT* pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdCopyMemoryToMicromapEXT, vkCmdCopyMemoryToMicromapEXT, void,
        (VkCommandBuffer    commandBuffer, const VkCopyMemoryToMicromapInfoEXT* pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdWriteMicromapsPropertiesEXT, vkCmdWriteMicromapsPropertiesEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t                                 micromapCount, const VkMicromapEXT* pMicromaps, VkQueryType        queryType, VkQueryPool                              queryPool, uint32_t                                 firstQuery),
        (commandBuffer, micromapCount, pMicromaps, queryType, queryPool, firstQuery) )
    SIR_VK_PROC( getDeviceMicromapCompatibilityEXT, vkGetDeviceMicromapCompatibilityEXT, void,
        (const VkMicromapVersionInfoEXT*          pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility),
        (m_Device, pVersionInfo, pCompatibility) )
    SIR_VK_PROC( getMicromapBuildSizesEXT, vkGetMicromapBuildSizesEXT, void,
        (VkAccelerationStructureBuildTypeKHR buildType, const VkMicromapBuildInfoEXT*       pBuildInfo, VkMicromapBuildSizesInfoEXT*        pSizeInfo),
        (m_Device, buildType, pBuildInfo, pSizeInfo) )
#endif

#if defined(VK_HUAWEI_cluster_culling_shader)
    SIR_VK_PROC( cmdDrawClusterHUAWEI, vkCmdDrawClusterHUAWEI, void,
        (VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ),
        (commandBuffer, groupCountX, groupCountY, groupCountZ) )
    SIR_VK_PROC( cmdDrawClusterIndirectHUAWEI, vkCmdDrawClusterIndirectHUAWEI, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset),
        (commandBuffer, buffer, offset) )
#endif

#if defined(VK_EXT_pageable_device_local_memory)
    SIR_VK_PROC( setDeviceMemoryPriorityEXT, vkSetDeviceMemoryPriorityEXT, void,
        (VkDeviceMemory memory, float          priority),
        (m_Device, memory, priority) )
#endif

#if defined(VK_ARM_scheduling_controls)
    SIR_VK_PROC( cmdSetDispatchParametersARM, vkCmdSetDispatchParametersARM, void,
        (VkCommandBuffer commandBuffer, const VkDispatchParametersARM*    pDispatchParameters),
        (commandBuffer, pDispatchParameters) )
#endif

#if defined(VK_VALVE_descriptor_set_host_mapping)
    SIR_VK_PROC( getDescriptorSetLayoutHostMappingInfoVALVE, vkGetDescriptorSetLayoutHostMappingInfoVALVE, void,
        (const VkDescriptorSetBindingReferenceVALVE* pBindingReference, VkDescriptorSetLayoutHostMappingInfoVALVE* pHostMapping),
        (m_Device, pBindingReference, pHostMapping) )
    SIR_VK_PROC( getDescriptorSetHostMappingVALVE, vkGetDescriptorSetHostMappingVALVE, void,
        (VkDescriptorSet descriptorSet, void** ppData),
        (m_Device, descriptorSet, ppData) )
#endif

#if defined(VK_NV_copy_memory_indirect)
    SIR_VK_PROC( cmdCopyMemoryIndirectNV, vkCmdCopyMemoryIndirectNV, void,
        (VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride),
        (commandBuffer, copyBufferAddress, copyCount, stride) )
    SIR_VK_PROC( cmdCopyMemoryToImageIndirectNV, vkCmdCopyMemoryToImageIndirectNV, void,
        (VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride, VkImage dstImage, VkImageLayout dstImageLayout, const VkImageSubresourceLayers* pImageSubresources),
        (commandBuffer, copyBufferAddress, copyCount, stride, dstImage, dstImageLayout, pImageSubresources) )
#endif

#if defined(VK_NV_memory_decompression)
    SIR_VK_PROC( cmdDecompressMemoryNV, vkCmdDecompressMemoryNV, void,
        (VkCommandBuffer commandBuffer, uint32_t decompressRegionCount, const VkDecompressMemoryRegionNV* pDecompressMemoryRegions),
        (commandBuffer, decompressRegionCount, pDecompressMemoryRegions) )
    SIR_VK_PROC( cmdDecompressMemoryIndirectCountNV, vkCmdDecompressMemoryIndirectCountNV, void,
        (VkCommandBuffer commandBuffer, VkDeviceAddress indirectCommandsAddress, VkDeviceAddress indirectCommandsCountAddress, uint32_t stride),
        (commandBuffer, indirectCommandsAddress, indirectCommandsCountAddress, stride) )
#endif

#if defined(VK_NV_device_generated_commands_compute)
    SIR_VK_PROC( getPipelineIndirectMemoryRequirementsNV, vkGetPipelineIndirectMemoryRequirementsNV, void,
        (const VkComputePipelineCreateInfo* pCreateInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pCreateInfo, pMemoryRequirements) )
    SIR_VK_PROC( cmdUpdatePipelineIndirectBufferNV, vkCmdUpdatePipelineIndirectBufferNV, void,
        (VkCommandBuffer commandBuffer, VkPipelineBindPoint           pipelineBindPoint, VkPipeline                    pipeline),
        (commandBuffer, pipelineBindPoint, pipeline) )
    SIR_VK_PROC( getPipelineIndirectDeviceAddressNV, vkGetPipelineIndirectDeviceAddressNV, VkDeviceAddress,
        (const VkPipelineIndirectDeviceAddressInfoNV* pInfo),
        (m_Device, pInfo) )
#endif

#if defined(VK_EXT_extended_dynamic_state3)
    SIR_VK_PROC( cmdSetDepthClampEnableEXT, vkCmdSetDepthClampEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthClampEnable),
        (commandBuffer, depthClampEnable) )
    SIR_VK_PROC( cmdSetPolygonModeEXT, vkCmdSetPolygonModeEXT, void,
        (VkCommandBuffer commandBuffer, VkPolygonMode polygonMode),
        (commandBuffer, polygonMode) )
    SIR_VK_PROC( cmdSetRasterizationSamplesEXT, vkCmdSetRasterizationSamplesEXT, void,
        (VkCommandBuffer commandBuffer, VkSampleCountFlagBits  rasterizationSamples),
        (commandBuffer, rasterizationSamples) )
    SIR_VK_PROC( cmdSetSampleMaskEXT, vkCmdSetSampleMaskEXT, void,
        (VkCommandBuffer commandBuffer, VkSampleCountFlagBits  samples, const VkSampleMask*    pSampleMask),
        (commandBuffer, samples, pSampleMask) )
    SIR_VK_PROC( cmdSetAlphaToCoverageEnableEXT, vkCmdSetAlphaToCoverageEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable),
        (commandBuffer, alphaToCoverageEnable) )
    SIR_VK_PROC( cmdSetAlphaToOneEnableEXT, vkCmdSetAlphaToOneEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable),
        (commandBuffer, alphaToOneEnable) )
    SIR_VK_PROC( cmdSetLogicOpEnableEXT, vkCmdSetLogicOpEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 logicOpEnable),
        (commandBuffer, logicOpEnable) )
    SIR_VK_PROC( cmdSetColorBlendEnableEXT, vkCmdSetColorBlendEnableEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables),
        (commandBuffer, firstAttachment, attachmentCount, pColorBlendEnables) )
    SIR_VK_PROC( cmdSetColorBlendEquationEXT, vkCmdSetColorBlendEquationEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations),
        (commandBuffer, firstAttachment, attachmentCount, pColorBlendEquations) )
    SIR_VK_PROC( cmdSetColorWriteMaskEXT, vkCmdSetColorWriteMaskEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks),
        (commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks) )
    SIR_VK_PROC( cmdSetTessellationDomainOriginEXT, vkCmdSetTessellationDomainOriginEXT, void,
        (VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin),
        (commandBuffer, domainOrigin) )
    SIR_VK_PROC( cmdSetRasterizationStreamEXT, vkCmdSetRasterizationStreamEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t rasterizationStream),
        (commandBuffer, rasterizationStream) )
    SIR_VK_PROC( cmdSetConservativeRasterizationModeEXT, vkCmdSetConservativeRasterizationModeEXT, void,
        (VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode),
        (commandBuffer, conservativeRasterizationMode) )
    SIR_VK_PROC( cmdSetExtraPrimitiveOverestimationSizeEXT, vkCmdSetExtraPrimitiveOverestimationSizeEXT, void,
        (VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize),
        (commandBuffer, extraPrimitiveOverestimationSize) )
    SIR_VK_PROC( cmdSetDepthClipEnableEXT, vkCmdSetDepthClipEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 depthClipEnable),
        (commandBuffer, depthClipEnable) )
    SIR_VK_PROC( cmdSetSampleLocationsEnableEXT, vkCmdSetSampleLocationsEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable),
        (commandBuffer, sampleLocationsEnable) )
    SIR_VK_PROC( cmdSetColorBlendAdvancedEXT, vkCmdSetColorBlendAdvancedEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced),
        (commandBuffer, firstAttachment, attachmentCount, pColorBlendAdvanced) )
    SIR_VK_PROC( cmdSetProvokingVertexModeEXT, vkCmdSetProvokingVertexModeEXT, void,
        (VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode),
        (commandBuffer, provokingVertexMode) )
    SIR_VK_PROC( cmdSetLineRasterizationModeEXT, vkCmdSetLineRasterizationModeEXT, void,
        (VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode),
        (commandBuffer, lineRasterizationMode) )
    SIR_VK_PROC( cmdSetLineStippleEnableEXT, vkCmdSetLineStippleEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable),
        (commandBuffer, stippledLineEnable) )
    SIR_VK_PROC( cmdSetDepthClipNegativeOneToOneEXT, vkCmdSetDepthClipNegativeOneToOneEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne),
        (commandBuffer, negativeOneToOne) )
    SIR_VK_PROC( cmdSetViewportWScalingEnableNV, vkCmdSetViewportWScalingEnableNV, void,
        (VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable),
        (commandBuffer, viewportWScalingEnable) )
    SIR_VK_PROC( cmdSetViewportSwizzleNV, vkCmdSetViewportSwizzleNV, void,
        (VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles),
        (commandBuffer, firstViewport, viewportCount, pViewportSwizzles) )
    SIR_VK_PROC( cmdSetCoverageToColorEnableNV, vkCmdSetCoverageToColorEnableNV, void,
        (VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable),
        (commandBuffer, coverageToColorEnable) )
    SIR_VK_PROC( cmdSetCoverageToColorLocationNV, vkCmdSetCoverageToColorLocationNV, void,
        (VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation),
        (commandBuffer, coverageToColorLocation) )
    SIR_VK_PROC( cmdSetCoverageModulationModeNV, vkCmdSetCoverageModulationModeNV, void,
        (VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode),
        (commandBuffer, coverageModulationMode) )
    SIR_VK_PROC( cmdSetCoverageModulationTableEnableNV, vkCmdSetCoverageModulationTableEnableNV, void,
        (VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable),
        (commandBuffer, coverageModulationTableEnable) )
    SIR_VK_PROC( cmdSetCoverageModulationTableNV, vkCmdSetCoverageModulationTableNV, void,
        (VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable),
        (commandBuffer, coverageModulationTableCount, pCoverageModulationTable) )
    SIR_VK_PROC( cmdSetShadingRateImageEnableNV, vkCmdSetShadingRateImageEnableNV, void,
        (VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable),
        (commandBuffer, shadingRateImageEnable) )
    SIR_VK_PROC( cmdSetRepresentativeFragmentTestEnableNV, vkCmdSetRepresentativeFragmentTestEnableNV, void,
        (VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable),
        (commandBuffer, representativeFragmentTestEnable) )
    SIR_VK_PROC( cmdSetCoverageReductionModeNV, vkCmdSetCoverageReductionModeNV, void,
        (VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode),
        (commandBuffer, coverageReductionMode) )
#endif

#if defined(VK_ARM_tensors)
    SIR_VK_PROC( createTensorARM, vkCreateTensorARM, VkResult,
        (const VkTensorCreateInfoARM* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkTensorARM* pTensor),
        (m_Device, pCreateInfo, pAllocator, pTensor) )
    SIR_VK_PROC( destroyTensorARM, vkDestroyTensorARM, void,
        (VkTensorARM tensor, const VkAllocationCallbacks* pAllocator),
        (m_Device, tensor, pAllocator) )
    SIR_VK_PROC( createTensorViewARM, vkCreateTensorViewARM, VkResult,
        (const VkTensorViewCreateInfoARM* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkTensorViewARM* pView),
        (m_Device, pCreateInfo, pAllocator, pView) )
    SIR_VK_PROC( destroyTensorViewARM, vkDestroyTensorViewARM, void,
        (VkTensorViewARM tensorView, const VkAllocationCallbacks* pAllocator),
        (m_Device, tensorView, pAllocator) )
    SIR_VK_PROC( getTensorMemoryRequirementsARM, vkGetTensorMemoryRequirementsARM, void,
        (const VkTensorMemoryRequirementsInfoARM* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( bindTensorMemoryARM, vkBindTensorMemoryARM, VkResult,
        (uint32_t bindInfoCount, const VkBindTensorMemoryInfoARM* pBindInfos),
        (m_Device, bindInfoCount, pBindInfos) )
    SIR_VK_PROC( getDeviceTensorMemoryRequirementsARM, vkGetDeviceTensorMemoryRequirementsARM, void,
        (const VkDeviceTensorMemoryRequirementsARM* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( cmdCopyTensorARM, vkCmdCopyTensorARM, void,
        (VkCommandBuffer commandBuffer, const VkCopyTensorInfoARM* pCopyTensorInfo),
        (commandBuffer, pCopyTensorInfo) )
    SIR_VK_PROC( getTensorOpaqueCaptureDescriptorDataARM, vkGetTensorOpaqueCaptureDescriptorDataARM, VkResult,
        (const VkTensorCaptureDescriptorDataInfoARM* pInfo, void*                                       pData),
        (m_Device, pInfo, pData) )
    SIR_VK_PROC( getTensorViewOpaqueCaptureDescriptorDataARM, vkGetTensorViewOpaqueCaptureDescriptorDataARM, VkResult,
        (const VkTensorViewCaptureDescriptorDataInfoARM* pInfo, void*                                           pData),
        (m_Device, pInfo, pData) )
#endif

#if defined(VK_EXT_shader_module_identifier)
    SIR_VK_PROC( getShaderModuleIdentifierEXT, vkGetShaderModuleIdentifierEXT, void,
        (VkShaderModule shaderModule, VkShaderModuleIdentifierEXT* pIdentifier),
        (m_Device, shaderModule, pIdentifier) )
    SIR_VK_PROC( getShaderModuleCreateInfoIdentifierEXT, vkGetShaderModuleCreateInfoIdentifierEXT, void,
        (const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModuleIdentifierEXT* pIdentifier),
        (m_Device, pCreateInfo, pIdentifier) )
#endif

#if defined(VK_NV_optical_flow)
    SIR_VK_PROC( createOpticalFlowSessionNV, vkCreateOpticalFlowSessionNV, VkResult,
        (const VkOpticalFlowSessionCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkOpticalFlowSessionNV* pSession),
        (m_Device, pCreateInfo, pAllocator, pSession) )
    SIR_VK_PROC( destroyOpticalFlowSessionNV, vkDestroyOpticalFlowSessionNV, void,
        (VkOpticalFlowSessionNV session, const VkAllocationCallbacks* pAllocator),
        (m_Device, session, pAllocator) )
    SIR_VK_PROC( bindOpticalFlowSessionImageNV, vkBindOpticalFlowSessionImageNV, VkResult,
        (VkOpticalFlowSessionNV session, VkOpticalFlowSessionBindingPointNV bindingPoint, VkImageView view, VkImageLayout layout),
        (m_Device, session, bindingPoint, view, layout) )
    SIR_VK_PROC( cmdOpticalFlowExecuteNV, vkCmdOpticalFlowExecuteNV, void,
        (VkCommandBuffer commandBuffer, VkOpticalFlowSessionNV session, const VkOpticalFlowExecuteInfoNV* pExecuteInfo),
        (commandBuffer, session, pExecuteInfo) )
#endif

#if defined(VK_AMD_anti_lag)
    SIR_VK_PROC( antiLagUpdateAMD, vkAntiLagUpdateAMD, void,
        (const VkAntiLagDataAMD* pData),
        (m_Device, pData) )
#endif

#if defined(VK_EXT_shader_object)
    SIR_VK_PROC( createShadersEXT, vkCreateShadersEXT, VkResult,
        (uint32_t createInfoCount, const VkShaderCreateInfoEXT* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkShaderEXT* pShaders),
        (m_Device, createInfoCount, pCreateInfos, pAllocator, pShaders) )
    SIR_VK_PROC( destroyShaderEXT, vkDestroyShaderEXT, void,
        (VkShaderEXT shader, const VkAllocationCallbacks* pAllocator),
        (m_Device, shader, pAllocator) )
    SIR_VK_PROC( getShaderBinaryDataEXT, vkGetShaderBinaryDataEXT, VkResult,
        (VkShaderEXT shader, size_t* pDataSize, void* pData),
        (m_Device, shader, pDataSize, pData) )
    SIR_VK_PROC( cmdBindShadersEXT, vkCmdBindShadersEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits* pStages, const VkShaderEXT* pShaders),
        (commandBuffer, stageCount, pStages, pShaders) )
    SIR_VK_PROC( cmdSetDepthClampRangeEXT, vkCmdSetDepthClampRangeEXT, void,
        (VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT* pDepthClampRange),
        (commandBuffer, depthClampMode, pDepthClampRange) )
#endif

#if defined(VK_QCOM_tile_properties)
    SIR_VK_PROC( getFramebufferTilePropertiesQCOM, vkGetFramebufferTilePropertiesQCOM, VkResult,
        (VkFramebuffer framebuffer, uint32_t* pPropertiesCount, VkTilePropertiesQCOM* pProperties),
        (m_Device, framebuffer, pPropertiesCount, pProperties) )
    SIR_VK_PROC( getDynamicRenderingTilePropertiesQCOM, vkGetDynamicRenderingTilePropertiesQCOM, VkResult,
        (const VkRenderingInfo* pRenderingInfo, VkTilePropertiesQCOM* pProperties),
        (m_Device, pRenderingInfo, pProperties) )
#endif

#if defined(VK_NV_cooperative_vector)
    SIR_VK_PROC( convertCooperativeVectorMatrixNV, vkConvertCooperativeVectorMatrixNV, VkResult,
        (const VkConvertCooperativeVectorMatrixInfoNV* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( cmdConvertCooperativeVectorMatrixNV, vkCmdConvertCooperativeVectorMatrixNV, void,
        (VkCommandBuffer commandBuffer, uint32_t infoCount, const VkConvertCooperativeVectorMatrixInfoNV* pInfos),
        (commandBuffer, infoCount, pInfos) )
#endif

#if defined(VK_NV_low_latency2)
    SIR_VK_PROC( setLatencySleepModeNV, vkSetLatencySleepModeNV, VkResult,
        (VkSwapchainKHR swapchain, const VkLatencySleepModeInfoNV* pSleepModeInfo),
        (m_Device, swapchain, pSleepModeInfo) )
    SIR_VK_PROC( latencySleepNV, vkLatencySleepNV, VkResult,
        (VkSwapchainKHR swapchain, const VkLatencySleepInfoNV* pSleepInfo),
        (m_Device, swapchain, pSleepInfo) )
    SIR_VK_PROC( setLatencyMarkerNV, vkSetLatencyMarkerNV, void,
        (VkSwapchainKHR swapchain, const VkSetLatencyMarkerInfoNV* pLatencyMarkerInfo),
        (m_Device, swapchain, pLatencyMarkerInfo) )
    SIR_VK_PROC( getLatencyTimingsNV, vkGetLatencyTimingsNV, void,
        (VkSwapchainKHR swapchain, VkGetLatencyMarkerInfoNV* pLatencyMarkerInfo),
        (m_Device, swapchain, pLatencyMarkerInfo) )
    SIR_VK_PROC( queueNotifyOutOfBandNV, vkQueueNotifyOutOfBandNV, void,
        (VkQueue queue, const VkOutOfBandQueueTypeInfoNV* pQueueTypeInfo),
        (queue, pQueueTypeInfo) )
#endif

#if defined(VK_ARM_data_graph)
    SIR_VK_PROC( createDataGraphPipelinesARM, vkCreateDataGraphPipelinesARM, VkResult,
        (VkDeferredOperationKHR deferredOperation, VkPipelineCache        pipelineCache, uint32_t               createInfoCount, const VkDataGraphPipelineCreateInfoARM* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline*     pPipelines),
        (m_Device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines) )
    SIR_VK_PROC( createDataGraphPipelineSessionARM, vkCreateDataGraphPipelineSessionARM, VkResult,
        (const VkDataGraphPipelineSessionCreateInfoARM*   pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDataGraphPipelineSessionARM*                   pSession),
        (m_Device, pCreateInfo, pAllocator, pSession) )
    SIR_VK_PROC( getDataGraphPipelineSessionBindPointRequirementsARM, vkGetDataGraphPipelineSessionBindPointRequirementsARM, VkResult,
        (const VkDataGraphPipelineSessionBindPointRequirementsInfoARM* pInfo, uint32_t* pBindPointRequirementCount, VkDataGraphPipelineSessionBindPointRequirementARM* pBindPointRequirements),
        (m_Device, pInfo, pBindPointRequirementCount, pBindPointRequirements) )
    SIR_VK_PROC( getDataGraphPipelineSessionMemoryRequirementsARM, vkGetDataGraphPipelineSessionMemoryRequirementsARM, void,
        (const VkDataGraphPipelineSessionMemoryRequirementsInfoARM* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( bindDataGraphPipelineSessionMemoryARM, vkBindDataGraphPipelineSessionMemoryARM, VkResult,
        (uint32_t bindInfoCount, const VkBindDataGraphPipelineSessionMemoryInfoARM* pBindInfos),
        (m_Device, bindInfoCount, pBindInfos) )
    SIR_VK_PROC( destroyDataGraphPipelineSessionARM, vkDestroyDataGraphPipelineSessionARM, void,
        (VkDataGraphPipelineSessionARM session, const VkAllocationCallbacks* pAllocator),
        (m_Device, session, pAllocator) )
    SIR_VK_PROC( cmdDispatchDataGraphARM, vkCmdDispatchDataGraphARM, void,
        (VkCommandBuffer commandBuffer, VkDataGraphPipelineSessionARM session, const VkDataGraphPipelineDispatchInfoARM* pInfo),
        (commandBuffer, session, pInfo) )
    SIR_VK_PROC( getDataGraphPipelineAvailablePropertiesARM, vkGetDataGraphPipelineAvailablePropertiesARM, VkResult,
        (const VkDataGraphPipelineInfoARM* pPipelineInfo, uint32_t* pPropertiesCount, VkDataGraphPipelinePropertyARM* pProperties),
        (m_Device, pPipelineInfo, pPropertiesCount, pProperties) )
    SIR_VK_PROC( getDataGraphPipelinePropertiesARM, vkGetDataGraphPipelinePropertiesARM, VkResult,
        (const VkDataGraphPipelineInfoARM* pPipelineInfo, uint32_t                          propertiesCount, VkDataGraphPipelinePropertyQueryResultARM* pProperties),
        (m_Device, pPipelineInfo, propertiesCount, pProperties) )
#endif

#if defined(VK_EXT_attachment_feedback_loop_dynamic_state)
    SIR_VK_PROC( cmdSetAttachmentFeedbackLoopEnableEXT, vkCmdSetAttachmentFeedbackLoopEnableEXT, void,
        (VkCommandBuffer commandBuffer, VkImageAspectFlags aspectMask),
        (commandBuffer, aspectMask) )
#endif

#if defined(VK_QCOM_tile_memory_heap)
    SIR_VK_PROC( cmdBindTileMemoryQCOM, vkCmdBindTileMemoryQCOM, void,
        (VkCommandBuffer commandBuffer, const VkTileMemoryBindInfoQCOM* pTileMemoryBindInfo),
        (commandBuffer, pTileMemoryBindInfo) )
#endif

#if defined(VK_EXT_memory_decompression)
    SIR_VK_PROC( cmdDecompressMemoryEXT, vkCmdDecompressMemoryEXT, void,
        (VkCommandBuffer commandBuffer, const VkDecompressMemoryInfoEXT* pDecompressMemoryInfoEXT),
        (commandBuffer, pDecompressMemoryInfoEXT) )
    SIR_VK_PROC( cmdDecompressMemoryIndirectCountEXT, vkCmdDecompressMemoryIndirectCountEXT, void,
        (VkCommandBuffer commandBuffer, VkMemoryDecompressionMethodFlagsEXT decompressionMethod, VkDeviceAddress indirectCommandsAddress, VkDeviceAddress indirectCommandsCountAddress, uint32_t maxDecompressionCount, uint32_t stride),
        (commandBuffer, decompressionMethod, indirectCommandsAddress, indirectCommandsCountAddress, maxDecompressionCount, stride) )
#endif

#if defined(VK_NV_external_compute_queue)
    SIR_VK_PROC( createExternalComputeQueueNV, vkCreateExternalComputeQueueNV, VkResult,
        (const VkExternalComputeQueueCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkExternalComputeQueueNV* pExternalQueue),
        (m_Device, pCreateInfo, pAllocator, pExternalQueue) )
    SIR_VK_PROC( destroyExternalComputeQueueNV, vkDestroyExternalComputeQueueNV, void,
        (VkExternalComputeQueueNV externalQueue, const VkAllocationCallbacks* pAllocator),
        (m_Device, externalQueue, pAllocator) )
    SIR_VK_PROC( getExternalComputeQueueDataNV, vkGetExternalComputeQueueDataNV, void,
        (VkExternalComputeQueueNV externalQueue, VkExternalComputeQueueDataParamsNV* params, void* pData),
        (externalQueue, params, pData) )
#endif

#if defined(VK_NV_cluster_acceleration_structure)
    SIR_VK_PROC( getClusterAccelerationStructureBuildSizesNV, vkGetClusterAccelerationStructureBuildSizesNV, void,
        (const VkClusterAccelerationStructureInputInfoNV* pInfo, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo),
        (m_Device, pInfo, pSizeInfo) )
    SIR_VK_PROC( cmdBuildClusterAccelerationStructureIndirectNV, vkCmdBuildClusterAccelerationStructureIndirectNV, void,
        (VkCommandBuffer                     commandBuffer, const VkClusterAccelerationStructureCommandsInfoNV*  pCommandInfos),
        (commandBuffer, pCommandInfos) )
#endif

#if defined(VK_NV_partitioned_acceleration_structure)
    SIR_VK_PROC( getPartitionedAccelerationStructuresBuildSizesNV, vkGetPartitionedAccelerationStructuresBuildSizesNV, void,
        (const VkPartitionedAccelerationStructureInstancesInputNV* pInfo, VkAccelerationStructureBuildSizesInfoKHR*                  pSizeInfo),
        (m_Device, pInfo, pSizeInfo) )
    SIR_VK_PROC( cmdBuildPartitionedAccelerationStructuresNV, vkCmdBuildPartitionedAccelerationStructuresNV, void,
        (VkCommandBuffer                     commandBuffer, const VkBuildPartitionedAccelerationStructureInfoNV*  pBuildInfo),
        (commandBuffer, pBuildInfo) )
#endif

#if defined(VK_EXT_device_generated_commands)
    SIR_VK_PROC( getGeneratedCommandsMemoryRequirementsEXT, vkGetGeneratedCommandsMemoryRequirementsEXT, void,
        (const VkGeneratedCommandsMemoryRequirementsInfoEXT* pInfo, VkMemoryRequirements2* pMemoryRequirements),
        (m_Device, pInfo, pMemoryRequirements) )
    SIR_VK_PROC( cmdPreprocessGeneratedCommandsEXT, vkCmdPreprocessGeneratedCommandsEXT, void,
        (VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoEXT* pGeneratedCommandsInfo, VkCommandBuffer stateCommandBuffer),
        (commandBuffer, pGeneratedCommandsInfo, stateCommandBuffer) )
    SIR_VK_PROC( cmdExecuteGeneratedCommandsEXT, vkCmdExecuteGeneratedCommandsEXT, void,
        (VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoEXT* pGeneratedCommandsInfo),
        (commandBuffer, isPreprocessed, pGeneratedCommandsInfo) )
    SIR_VK_PROC( createIndirectCommandsLayoutEXT, vkCreateIndirectCommandsLayoutEXT, VkResult,
        (const VkIndirectCommandsLayoutCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkIndirectCommandsLayoutEXT* pIndirectCommandsLayout),
        (m_Device, pCreateInfo, pAllocator, pIndirectCommandsLayout) )
    SIR_VK_PROC( destroyIndirectCommandsLayoutEXT, vkDestroyIndirectCommandsLayoutEXT, void,
        (VkIndirectCommandsLayoutEXT indirectCommandsLayout, const VkAllocationCallbacks* pAllocator),
        (m_Device, indirectCommandsLayout, pAllocator) )
    SIR_VK_PROC( createIndirectExecutionSetEXT, vkCreateIndirectExecutionSetEXT, VkResult,
        (const VkIndirectExecutionSetCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkIndirectExecutionSetEXT* pIndirectExecutionSet),
        (m_Device, pCreateInfo, pAllocator, pIndirectExecutionSet) )
    SIR_VK_PROC( destroyIndirectExecutionSetEXT, vkDestroyIndirectExecutionSetEXT, void,
        (VkIndirectExecutionSetEXT indirectExecutionSet, const VkAllocationCallbacks* pAllocator),
        (m_Device, indirectExecutionSet, pAllocator) )
    SIR_VK_PROC( updateIndirectExecutionSetPipelineEXT, vkUpdateIndirectExecutionSetPipelineEXT, void,
        (VkIndirectExecutionSetEXT indirectExecutionSet, uint32_t executionSetWriteCount, const VkWriteIndirectExecutionSetPipelineEXT* pExecutionSetWrites),
        (m_Device, indirectExecutionSet, executionSetWriteCount, pExecutionSetWrites) )
    SIR_VK_PROC( updateIndirectExecutionSetShaderEXT, vkUpdateIndirectExecutionSetShaderEXT, void,
        (VkIndirectExecutionSetEXT indirectExecutionSet, uint32_t executionSetWriteCount, const VkWriteIndirectExecutionSetShaderEXT* pExecutionSetWrites),
        (m_Device, indirectExecutionSet, executionSetWriteCount, pExecutionSetWrites) )
#endif

#if defined(VK_ARM_shader_instrumentation)
    SIR_VK_PROC( createShaderInstrumentationARM, vkCreateShaderInstrumentationARM, VkResult,
        (const VkShaderInstrumentationCreateInfoARM* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderInstrumentationARM* pInstrumentation),
        (m_Device, pCreateInfo, pAllocator, pInstrumentation) )
    SIR_VK_PROC( destroyShaderInstrumentationARM, vkDestroyShaderInstrumentationARM, void,
        (VkShaderInstrumentationARM instrumentation, const VkAllocationCallbacks*    pAllocator),
        (m_Device, instrumentation, pAllocator) )
    SIR_VK_PROC( cmdBeginShaderInstrumentationARM, vkCmdBeginShaderInstrumentationARM, void,
        (VkCommandBuffer commandBuffer, VkShaderInstrumentationARM instrumentation),
        (commandBuffer, instrumentation) )
    SIR_VK_PROC( cmdEndShaderInstrumentationARM, vkCmdEndShaderInstrumentationARM, void,
        (VkCommandBuffer commandBuffer),
        (commandBuffer) )
    SIR_VK_PROC( getShaderInstrumentationValuesARM, vkGetShaderInstrumentationValuesARM, VkResult,
        (VkShaderInstrumentationARM instrumentation, uint32_t* pMetricBlockCount, void* pMetricValues, VkShaderInstrumentationValuesFlagsARM flags),
        (m_Device, instrumentation, pMetricBlockCount, pMetricValues, flags) )
    SIR_VK_PROC( clearShaderInstrumentationMetricsARM, vkClearShaderInstrumentationMetricsARM, void,
        (VkShaderInstrumentationARM instrumentation),
        (m_Device, instrumentation) )
#endif

#if defined(VK_EXT_fragment_density_map_offset)
    SIR_VK_PROC( cmdEndRendering2EXT, vkCmdEndRendering2EXT, void,
        (VkCommandBuffer                   commandBuffer, const VkRenderingEndInfoKHR*        pRenderingEndInfo),
        (commandBuffer, pRenderingEndInfo) )
#endif

#if defined(VK_EXT_custom_resolve)
    SIR_VK_PROC( cmdBeginCustomResolveEXT, vkCmdBeginCustomResolveEXT, void,
        (VkCommandBuffer commandBuffer, const VkBeginCustomResolveInfoEXT* pBeginCustomResolveInfo),
        (commandBuffer, pBeginCustomResolveInfo) )
#endif

#if defined(VK_NV_compute_occupancy_priority)
    SIR_VK_PROC( cmdSetComputeOccupancyPriorityNV, vkCmdSetComputeOccupancyPriorityNV, void,
        (VkCommandBuffer commandBuffer, const VkComputeOccupancyPriorityParametersNV* pParameters),
        (commandBuffer, pParameters) )
#endif

#if defined(VK_EXT_primitive_restart_index)
    SIR_VK_PROC( cmdSetPrimitiveRestartIndexEXT, vkCmdSetPrimitiveRestartIndexEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t primitiveRestartIndex),
        (commandBuffer, primitiveRestartIndex) )
#endif

#if defined(VK_KHR_acceleration_structure)
    SIR_VK_PROC( createAccelerationStructureKHR, vkCreateAccelerationStructureKHR, VkResult,
        (const VkAccelerationStructureCreateInfoKHR*        pCreateInfo, const VkAllocationCallbacks*       pAllocator, VkAccelerationStructureKHR*                        pAccelerationStructure),
        (m_Device, pCreateInfo, pAllocator, pAccelerationStructure) )
    SIR_VK_PROC( destroyAccelerationStructureKHR, vkDestroyAccelerationStructureKHR, void,
        (VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator),
        (m_Device, accelerationStructure, pAllocator) )
    SIR_VK_PROC( cmdBuildAccelerationStructuresKHR, vkCmdBuildAccelerationStructuresKHR, void,
        (VkCommandBuffer                                    commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos),
        (commandBuffer, infoCount, pInfos, ppBuildRangeInfos) )
    SIR_VK_PROC( cmdBuildAccelerationStructuresIndirectKHR, vkCmdBuildAccelerationStructuresIndirectKHR, void,
        (VkCommandBuffer                  commandBuffer, uint32_t                                           infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress*             pIndirectDeviceAddresses, const uint32_t*                    pIndirectStrides, const uint32_t* const*             ppMaxPrimitiveCounts),
        (commandBuffer, infoCount, pInfos, pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts) )
    SIR_VK_PROC( buildAccelerationStructuresKHR, vkBuildAccelerationStructuresKHR, VkResult,
        (VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos),
        (m_Device, deferredOperation, infoCount, pInfos, ppBuildRangeInfos) )
    SIR_VK_PROC( copyAccelerationStructureKHR, vkCopyAccelerationStructureKHR, VkResult,
        (VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo),
        (m_Device, deferredOperation, pInfo) )
    SIR_VK_PROC( copyAccelerationStructureToMemoryKHR, vkCopyAccelerationStructureToMemoryKHR, VkResult,
        (VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo),
        (m_Device, deferredOperation, pInfo) )
    SIR_VK_PROC( copyMemoryToAccelerationStructureKHR, vkCopyMemoryToAccelerationStructureKHR, VkResult,
        (VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo),
        (m_Device, deferredOperation, pInfo) )
    SIR_VK_PROC( writeAccelerationStructuresPropertiesKHR, vkWriteAccelerationStructuresPropertiesKHR, VkResult,
        (uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType  queryType, size_t       dataSize, void* pData, size_t stride),
        (m_Device, accelerationStructureCount, pAccelerationStructures, queryType, dataSize, pData, stride) )
    SIR_VK_PROC( cmdCopyAccelerationStructureKHR, vkCmdCopyAccelerationStructureKHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdCopyAccelerationStructureToMemoryKHR, vkCmdCopyAccelerationStructureToMemoryKHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( cmdCopyMemoryToAccelerationStructureKHR, vkCmdCopyMemoryToAccelerationStructureKHR, void,
        (VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo),
        (commandBuffer, pInfo) )
    SIR_VK_PROC( getAccelerationStructureDeviceAddressKHR, vkGetAccelerationStructureDeviceAddressKHR, VkDeviceAddress,
        (const VkAccelerationStructureDeviceAddressInfoKHR* pInfo),
        (m_Device, pInfo) )
    SIR_VK_PROC( cmdWriteAccelerationStructuresPropertiesKHR, vkCmdWriteAccelerationStructuresPropertiesKHR, void,
        (VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery),
        (commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery) )
    SIR_VK_PROC( getDeviceAccelerationStructureCompatibilityKHR, vkGetDeviceAccelerationStructureCompatibilityKHR, void,
        (const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility),
        (m_Device, pVersionInfo, pCompatibility) )
    SIR_VK_PROC( getAccelerationStructureBuildSizesKHR, vkGetAccelerationStructureBuildSizesKHR, void,
        (VkAccelerationStructureBuildTypeKHR                 buildType, const VkAccelerationStructureBuildGeometryInfoKHR*  pBuildInfo, const uint32_t*  pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR*           pSizeInfo),
        (m_Device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo) )
#endif

#if defined(VK_KHR_ray_tracing_pipeline)
    SIR_VK_PROC( cmdTraceRaysKHR, vkCmdTraceRaysKHR, void,
        (VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth),
        (commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth) )
    SIR_VK_PROC( createRayTracingPipelinesKHR, vkCreateRayTracingPipelinesKHR, VkResult,
        (VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines),
        (m_Device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines) )
    SIR_VK_PROC( getRayTracingCaptureReplayShaderGroupHandlesKHR, vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, VkResult,
        (VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData),
        (m_Device, pipeline, firstGroup, groupCount, dataSize, pData) )
    SIR_VK_PROC( cmdTraceRaysIndirectKHR, vkCmdTraceRaysIndirectKHR, void,
        (VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress),
        (commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, indirectDeviceAddress) )
    SIR_VK_PROC( getRayTracingShaderGroupStackSizeKHR, vkGetRayTracingShaderGroupStackSizeKHR, VkDeviceSize,
        (VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader),
        (m_Device, pipeline, group, groupShader) )
    SIR_VK_PROC( cmdSetRayTracingPipelineStackSizeKHR, vkCmdSetRayTracingPipelineStackSizeKHR, void,
        (VkCommandBuffer commandBuffer, uint32_t pipelineStackSize),
        (commandBuffer, pipelineStackSize) )
#endif

#if defined(VK_EXT_mesh_shader)
    SIR_VK_PROC( cmdDrawMeshTasksEXT, vkCmdDrawMeshTasksEXT, void,
        (VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ),
        (commandBuffer, groupCountX, groupCountY, groupCountZ) )
    SIR_VK_PROC( cmdDrawMeshTasksIndirectEXT, vkCmdDrawMeshTasksIndirectEXT, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride),
        (commandBuffer, buffer, offset, drawCount, stride) )
    SIR_VK_PROC( cmdDrawMeshTasksIndirectCountEXT, vkCmdDrawMeshTasksIndirectCountEXT, void,
        (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride),
        (commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride) )
#endif

#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
    SIR_VK_PROC( getAndroidHardwareBufferPropertiesANDROID, vkGetAndroidHardwareBufferPropertiesANDROID, VkResult,
        (const struct AHardwareBuffer* buffer, VkAndroidHardwareBufferPropertiesANDROID* pProperties),
        (m_Device, buffer, pProperties) )
    SIR_VK_PROC( getMemoryAndroidHardwareBufferANDROID, vkGetMemoryAndroidHardwareBufferANDROID, VkResult,
        (const VkMemoryGetAndroidHardwareBufferInfoANDROID* pInfo, struct AHardwareBuffer** pBuffer),
        (m_Device, pInfo, pBuffer) )
#endif

#if defined(VK_FUCHSIA_external_memory)
    SIR_VK_PROC( getMemoryZirconHandleFUCHSIA, vkGetMemoryZirconHandleFUCHSIA, VkResult,
        (const VkMemoryGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, zx_handle_t* pZirconHandle),
        (m_Device, pGetZirconHandleInfo, pZirconHandle) )
    SIR_VK_PROC( getMemoryZirconHandlePropertiesFUCHSIA, vkGetMemoryZirconHandlePropertiesFUCHSIA, VkResult,
        (VkExternalMemoryHandleTypeFlagBits handleType, zx_handle_t zirconHandle, VkMemoryZirconHandlePropertiesFUCHSIA* pMemoryZirconHandleProperties),
        (m_Device, handleType, zirconHandle, pMemoryZirconHandleProperties) )
#endif

#if defined(VK_FUCHSIA_external_semaphore)
    SIR_VK_PROC( importSemaphoreZirconHandleFUCHSIA, vkImportSemaphoreZirconHandleFUCHSIA, VkResult,
        (const VkImportSemaphoreZirconHandleInfoFUCHSIA* pImportSemaphoreZirconHandleInfo),
        (m_Device, pImportSemaphoreZirconHandleInfo) )
    SIR_VK_PROC( getSemaphoreZirconHandleFUCHSIA, vkGetSemaphoreZirconHandleFUCHSIA, VkResult,
        (const VkSemaphoreGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, zx_handle_t* pZirconHandle),
        (m_Device, pGetZirconHandleInfo, pZirconHandle) )
#endif

#if defined(VK_FUCHSIA_buffer_collection)
    SIR_VK_PROC( createBufferCollectionFUCHSIA, vkCreateBufferCollectionFUCHSIA, VkResult,
        (const VkBufferCollectionCreateInfoFUCHSIA* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferCollectionFUCHSIA* pCollection),
        (m_Device, pCreateInfo, pAllocator, pCollection) )
    SIR_VK_PROC( setBufferCollectionImageConstraintsFUCHSIA, vkSetBufferCollectionImageConstraintsFUCHSIA, VkResult,
        (VkBufferCollectionFUCHSIA collection, const VkImageConstraintsInfoFUCHSIA* pImageConstraintsInfo),
        (m_Device, collection, pImageConstraintsInfo) )
    SIR_VK_PROC( setBufferCollectionBufferConstraintsFUCHSIA, vkSetBufferCollectionBufferConstraintsFUCHSIA, VkResult,
        (VkBufferCollectionFUCHSIA collection, const VkBufferConstraintsInfoFUCHSIA* pBufferConstraintsInfo),
        (m_Device, collection, pBufferConstraintsInfo) )
    SIR_VK_PROC( destroyBufferCollectionFUCHSIA, vkDestroyBufferCollectionFUCHSIA, void,
        (VkBufferCollectionFUCHSIA collection, const VkAllocationCallbacks* pAllocator),
        (m_Device, collection, pAllocator) )
    SIR_VK_PROC( getBufferCollectionPropertiesFUCHSIA, vkGetBufferCollectionPropertiesFUCHSIA, VkResult,
        (VkBufferCollectionFUCHSIA collection, VkBufferCollectionPropertiesFUCHSIA* pProperties),
        (m_Device, collection, pProperties) )
#endif

#if defined(VK_EXT_metal_objects)
    SIR_VK_PROC( exportMetalObjectsEXT, vkExportMetalObjectsEXT, void,
        (VkExportMetalObjectsInfoEXT* pMetalObjectsInfo),
        (m_Device, pMetalObjectsInfo) )
#endif

#if defined(VK_EXT_external_memory_metal)
    SIR_VK_PROC( getMemoryMetalHandleEXT, vkGetMemoryMetalHandleEXT, VkResult,
        (const VkMemoryGetMetalHandleInfoEXT* pGetMetalHandleInfo, void** pHandle),
        (m_Device, pGetMetalHandleInfo, pHandle) )
    SIR_VK_PROC( getMemoryMetalHandlePropertiesEXT, vkGetMemoryMetalHandlePropertiesEXT, VkResult,
        (VkExternalMemoryHandleTypeFlagBits handleType, const void* pHandle, VkMemoryMetalHandlePropertiesEXT* pMemoryMetalHandleProperties),
        (m_Device, handleType, pHandle, pMemoryMetalHandleProperties) )
#endif

#if defined(VK_KHR_external_memory_win32)
    SIR_VK_PROC( getMemoryWin32HandleKHR, vkGetMemoryWin32HandleKHR, VkResult,
        (const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle),
        (m_Device, pGetWin32HandleInfo, pHandle) )
    SIR_VK_PROC( getMemoryWin32HandlePropertiesKHR, vkGetMemoryWin32HandlePropertiesKHR, VkResult,
        (VkExternalMemoryHandleTypeFlagBits handleType, HANDLE handle, VkMemoryWin32HandlePropertiesKHR* pMemoryWin32HandleProperties),
        (m_Device, handleType, handle, pMemoryWin32HandleProperties) )
#endif

#if defined(VK_KHR_external_semaphore_win32)
    SIR_VK_PROC( importSemaphoreWin32HandleKHR, vkImportSemaphoreWin32HandleKHR, VkResult,
        (const VkImportSemaphoreWin32HandleInfoKHR* pImportSemaphoreWin32HandleInfo),
        (m_Device, pImportSemaphoreWin32HandleInfo) )
    SIR_VK_PROC( getSemaphoreWin32HandleKHR, vkGetSemaphoreWin32HandleKHR, VkResult,
        (const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle),
        (m_Device, pGetWin32HandleInfo, pHandle) )
#endif

#if defined(VK_KHR_external_fence_win32)
    SIR_VK_PROC( importFenceWin32HandleKHR, vkImportFenceWin32HandleKHR, VkResult,
        (const VkImportFenceWin32HandleInfoKHR* pImportFenceWin32HandleInfo),
        (m_Device, pImportFenceWin32HandleInfo) )
    SIR_VK_PROC( getFenceWin32HandleKHR, vkGetFenceWin32HandleKHR, VkResult,
        (const VkFenceGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle),
        (m_Device, pGetWin32HandleInfo, pHandle) )
#endif

#if defined(VK_NV_external_memory_win32)
    SIR_VK_PROC( getMemoryWin32HandleNV, vkGetMemoryWin32HandleNV, VkResult,
        (VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagsNV handleType, HANDLE* pHandle),
        (m_Device, memory, handleType, pHandle) )
#endif

#if defined(VK_EXT_full_screen_exclusive)
    SIR_VK_PROC( acquireFullScreenExclusiveModeEXT, vkAcquireFullScreenExclusiveModeEXT, VkResult,
        (VkSwapchainKHR swapchain),
        (m_Device, swapchain) )
    SIR_VK_PROC( releaseFullScreenExclusiveModeEXT, vkReleaseFullScreenExclusiveModeEXT, VkResult,
        (VkSwapchainKHR swapchain),
        (m_Device, swapchain) )
    SIR_VK_PROC( getDeviceGroupSurfacePresentModes2EXT, vkGetDeviceGroupSurfacePresentModes2EXT, VkResult,
        (const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, VkDeviceGroupPresentModeFlagsKHR* pModes),
        (m_Device, pSurfaceInfo, pModes) )
#endif

#if defined(VK_QNX_external_memory_screen_buffer)
    SIR_VK_PROC( getScreenBufferPropertiesQNX, vkGetScreenBufferPropertiesQNX, VkResult,
        (const struct _screen_buffer* buffer, VkScreenBufferPropertiesQNX* pProperties),
        (m_Device, buffer, pProperties) )
#endif

#if defined(VK_AMDX_shader_enqueue)
    SIR_VK_PROC( createExecutionGraphPipelinesAMDX, vkCreateExecutionGraphPipelinesAMDX, VkResult,
        (VkPipelineCache pipelineCache, uint32_t                                        createInfoCount, const VkExecutionGraphPipelineCreateInfoAMDX* pCreateInfos, const VkAllocationCallbacks*    pAllocator, VkPipeline*               pPipelines),
        (m_Device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines) )
    SIR_VK_PROC( getExecutionGraphPipelineScratchSizeAMDX, vkGetExecutionGraphPipelineScratchSizeAMDX, VkResult,
        (VkPipeline                                      executionGraph, VkExecutionGraphPipelineScratchSizeAMDX*        pSizeInfo),
        (m_Device, executionGraph, pSizeInfo) )
    SIR_VK_PROC( getExecutionGraphPipelineNodeIndexAMDX, vkGetExecutionGraphPipelineNodeIndexAMDX, VkResult,
        (VkPipeline                                      executionGraph, const VkPipelineShaderStageNodeCreateInfoAMDX*  pNodeInfo, uint32_t*                                       pNodeIndex),
        (m_Device, executionGraph, pNodeInfo, pNodeIndex) )
    SIR_VK_PROC( cmdInitializeGraphScratchMemoryAMDX, vkCmdInitializeGraphScratchMemoryAMDX, void,
        (VkCommandBuffer                                 commandBuffer, VkPipeline                                      executionGraph, VkDeviceAddress                                 scratch, VkDeviceSize                                    scratchSize),
        (commandBuffer, executionGraph, scratch, scratchSize) )
    SIR_VK_PROC( cmdDispatchGraphAMDX, vkCmdDispatchGraphAMDX, void,
        (VkCommandBuffer                                 commandBuffer, VkDeviceAddress                                 scratch, VkDeviceSize                                    scratchSize, const VkDispatchGraphCountInfoAMDX*             pCountInfo),
        (commandBuffer, scratch, scratchSize, pCountInfo) )
    SIR_VK_PROC( cmdDispatchGraphIndirectAMDX, vkCmdDispatchGraphIndirectAMDX, void,
        (VkCommandBuffer                                 commandBuffer, VkDeviceAddress                                 scratch, VkDeviceSize                                    scratchSize, const VkDispatchGraphCountInfoAMDX*             pCountInfo),
        (commandBuffer, scratch, scratchSize, pCountInfo) )
    SIR_VK_PROC( cmdDispatchGraphIndirectCountAMDX, vkCmdDispatchGraphIndirectCountAMDX, void,
        (VkCommandBuffer                                 commandBuffer, VkDeviceAddress                                 scratch, VkDeviceSize                                    scratchSize, VkDeviceAddress                                 countInfo),
        (commandBuffer, scratch, scratchSize, countInfo) )
#endif

#if defined(VK_NV_cuda_kernel_launch)
    SIR_VK_PROC( createCudaModuleNV, vkCreateCudaModuleNV, VkResult,
        (const VkCudaModuleCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCudaModuleNV* pModule),
        (m_Device, pCreateInfo, pAllocator, pModule) )
    SIR_VK_PROC( getCudaModuleCacheNV, vkGetCudaModuleCacheNV, VkResult,
        (VkCudaModuleNV module, size_t* pCacheSize, void* pCacheData),
        (m_Device, module, pCacheSize, pCacheData) )
    SIR_VK_PROC( createCudaFunctionNV, vkCreateCudaFunctionNV, VkResult,
        (const VkCudaFunctionCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCudaFunctionNV* pFunction),
        (m_Device, pCreateInfo, pAllocator, pFunction) )
    SIR_VK_PROC( destroyCudaModuleNV, vkDestroyCudaModuleNV, void,
        (VkCudaModuleNV module, const VkAllocationCallbacks* pAllocator),
        (m_Device, module, pAllocator) )
    SIR_VK_PROC( destroyCudaFunctionNV, vkDestroyCudaFunctionNV, void,
        (VkCudaFunctionNV function, const VkAllocationCallbacks* pAllocator),
        (m_Device, function, pAllocator) )
    SIR_VK_PROC( cmdCudaLaunchKernelNV, vkCmdCudaLaunchKernelNV, void,
        (VkCommandBuffer commandBuffer, const VkCudaLaunchInfoNV* pLaunchInfo),
        (commandBuffer, pLaunchInfo) )
#endif

#if defined(VK_OHOS_external_memory)
    SIR_VK_PROC( getNativeBufferPropertiesOHOS, vkGetNativeBufferPropertiesOHOS, VkResult,
        (const struct OH_NativeBuffer* buffer, VkNativeBufferPropertiesOHOS* pProperties),
        (m_Device, buffer, pProperties) )
    SIR_VK_PROC( getMemoryNativeBufferOHOS, vkGetMemoryNativeBufferOHOS, VkResult,
        (const VkMemoryGetNativeBufferInfoOHOS* pInfo, struct OH_NativeBuffer** pBuffer),
        (m_Device, pInfo, pBuffer) )
#endif
