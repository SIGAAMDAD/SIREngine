#ifndef __SIR_VULKAN_FRAMEBUFFER_H__
#define __SIR_VULKAN_FRAMEBUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRVulkanShared.h"
#include <Renderer/RHI/SIRRenderFramebuffer.h>

class SIRVulkanFramebuffer final : public SIRRenderFramebuffer
{
public:
	explicit SIRVulkanFramebuffer( const SIRSharedPtr<SIRRenderDevice>& device,
	                               const sirRenderFramebufferDescription_t& description ) noexcept;
	~SIRVulkanFramebuffer() noexcept override;

private:
	VkFramebuffer m_hFramebuffer;
};

#endif // __SIR_VULKAN_FRAMEBUFFER_H__
