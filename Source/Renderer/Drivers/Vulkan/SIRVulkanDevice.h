#ifndef __SIR_VULKAN_DEVICE_H__
#define __SIR_VULKAN_DEVICE_H__

#include <Renderer/RHI/SIRRenderDevice.h>
#include "SIRVulkanShared.h"

class SIRVulkanDevice
{
public:

	#include "SIRVulkanDeviceProcMethods.inl"

private:
	VkInstance m_hInstance;
	VkPhysicalDevice m_hPhysicalDevicel
};

#endif // __SIR_VULKAN_DEVICE_H__
