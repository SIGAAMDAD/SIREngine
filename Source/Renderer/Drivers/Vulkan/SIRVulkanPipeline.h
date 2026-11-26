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

#ifndef __SIR_VULKAN_PIPELINE_H__
#define __SIR_VULKAN_PIPELINE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderPipeline.h>
#include <Renderer/RHI/SIRRenderShader.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanBindingLayout.h>

class SIRVulkanPipeline final : public SIRRenderPipeline
{
public:
	SIRVulkanPipeline( const SIRSharedPtr<SIRRenderDevice>& device,
	                   const SIRSharedPtr<SIRVulkanBindingLayout>& layout,
	                   const SIRSharedPtr<SIRRenderShader>& shader,
	                   const sirContainerAllocator_t *pAllocator = nullptr ) noexcept;
	SIRVulkanPipeline( const SIRSharedPtr<SIRRenderDevice>& device,
	                   const SIRSharedPtr<SIRVulkanBindingLayout>& layout,
	                   const SIRSharedPtr<SIRRenderShader>& vertexShader,
	                   const SIRSharedPtr<SIRRenderShader>& fragmentShader,
	                   const sirGraphicsPipelineDesc_t& description,
	                   const sirContainerAllocator_t *pAllocator = nullptr ) noexcept;
	~SIRVulkanPipeline() noexcept override;

	VkPipeline getNativePipeline() const noexcept
	{
		return m_Native.hPipeline;
	}

	VkPipelineLayout getNativeLayout() const noexcept
	{
		return m_Native.hLayout;
	}

	const sirVulkanComputePipeline_t& getNativePipelineState() const noexcept
	{
		return m_Native;
	}

	bool isValid() const noexcept
	{
		return m_Native.hPipeline != VK_NULL_HANDLE;
	}

private:
	VkPipeline m_hPipeline;
	sirVulkanComputePipeline_t m_Native{};
};

#endif
