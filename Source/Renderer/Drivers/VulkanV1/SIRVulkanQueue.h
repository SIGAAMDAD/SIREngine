#ifndef __SIR_VULKAN_QUEUE_H__
#define __SIR_VULKAN_QUEUE_H__

#include <Renderer/RHI/SIRRenderQueue.h>

class SIRVulkanDevice;

class SIRVulkanQueue final : public SIRRenderQueue
{
public:
	SIRVulkanQueue( SIRVulkanDevice& device, SIRRenderQueueType eType ) noexcept;
	SIRRenderResult submit( const SIRRenderSubmission& submission ) noexcept override;
	SIRRenderResult waitIdle( void ) noexcept override;
	SIRRenderResult collectCompleted( void ) noexcept override;
};

#endif // __SIR_VULKAN_QUEUE_H__
