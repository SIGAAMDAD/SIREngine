#ifndef __VULKAN_PIPELINE_CACHE_H__
#define __VULKAN_PIPELINE_CACHE_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Pch.h"
#include "VulkanSwapChain.h"
#include "VulkanProgram.h"
#include "VulkanUtils.h"
#include "VulkanVertexLayout.h"
#include "../IRenderPipelineBuilder.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanPipelineBuilder : public IRenderPipelineBuilder
	{
	public:
		VulkanPipelineBuilder( const SVulkanInstance& instance, VkRenderPass hRenderPass, VulkanSwapChain *pSwapChain );
		virtual ~VulkanPipelineBuilder() override;
		
		virtual void StartPipeline( void ) override;
		virtual void BindProgram( IRenderProgram *pProgram ) override;
		virtual void BindVertexLayout( IVertexLayout *pLayout ) override;
		virtual void BindShaderBuffer( uint32_t nBinding, const IShaderBuffer *pBuffer ) override;
		virtual void SetViewportState( const glm::vec2& size, const glm::vec2& position ) override;
		virtual void *FinishPipeline( void ) override;
	private:
		typedef struct pipelineInfo_t
		{
			glm::vec2 ViewportSize;
			glm::vec2 ViewportPosition;
			VkPipelineVertexInputStateCreateInfo VertexInputInfo;
			eastl::fixed_vector<VkPipelineShaderStageCreateInfo, ShaderType_Count> arrShaderStages;
		} SPipelineInfo;

		const SVulkanInstance& m_Instance;

		const VkRenderPass m_hRenderPass;
		const VulkanSwapChain *m_pSwapChain;
		VkPipelineCache m_hPipelineCache;

		SPipelineInfo *m_pPipelineInfo;
		eastl::unordered_map<VkPipeline, SPipelineInfo *> m_DataCache;
	};
};


#endif