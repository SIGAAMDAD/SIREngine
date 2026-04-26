#ifndef __VULKAN_VERTEX_LAYOUT_H__
#define __VULKAN_VERTEX_LAYOUT_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>
#include "Pch.h"
#include "../IVertexLayout.h"
#include "VulkanUtils.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanVertexLayout : public IVertexLayout
	{
	public:
		VulkanVertexLayout( const SVulkanInstance& instance );
		virtual ~VulkanVertexLayout() override;

		virtual void AddAttribute( const SVertexAttribute& attrib ) override;
		virtual void AddBinding( const SVertexBinding& binding ) override;

		virtual void LinkAttributeToBinding( uint32_t nAttributeLocation, uint32_t nBindingIndex ) override;

		virtual bool Build( void ) override;

		virtual size_t GetHash( void ) const override;

		const VkPipelineVertexInputStateCreateInfo& GetVertexInfo( void ) const;
	private:
		eastl::vector<VkVertexInputBindingDescription> m_Bindings;
		eastl::vector<VkVertexInputAttributeDescription> m_Attributes;

		VkPipelineVertexInputStateCreateInfo m_VertexInputInfo;
	};

	SIRENGINE_FORCEINLINE const VkPipelineVertexInputStateCreateInfo& VulkanVertexLayout::GetVertexInfo( void ) const
	{
		m_VertexInputInfo;
	}
};

#endif