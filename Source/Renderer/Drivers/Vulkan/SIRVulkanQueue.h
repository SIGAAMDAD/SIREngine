#ifndef __SIR_VULKAN_QUEUE_H__
#define __SIR_VULKAN_QUEUE_H__

#include "SIRVulkanShared.h"

class SIRVulkanQueue
{
public:
	explicit SIRVulkanQueue( void ) noexcept;
	~SIRVulkanQueue() noexcept;

private:
	VkQueue m_hQueue;
};

#endif // __SIR_VULKAN_QUEUE_H__
