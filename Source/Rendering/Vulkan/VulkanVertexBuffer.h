#ifndef __VULKAN_BUFFER_H__
#define __VULKAN_BUFFER_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Pch.h"
#include "VulkanUtils.h"
#include "../IVertexBuffer.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanVertexBuffer : public IVertexBuffer
	{
	public:
		VulkanVertexBuffer( const SVulkanInstance& instance, size_t nSize, EStreamType eUsage, const void *pData = NULL );
		virtual ~VulkanVertexBuffer() override;

		virtual void Update( size_t nOffset, size_t nSize, const void *pData ) override;

		virtual void Bind( void ) override;
		virtual void Unbind( void ) override;

		virtual void *Map( EBufferMapAccess eAccess ) override;
		virtual void Unmap( void ) override;

		virtual size_t GetSize( void ) const override;

		const VkBuffer GetNativeHandle( void ) const;
	private:
		const SVulkanInstance& m_Instance;

		VmaAllocation m_Allocation;
		VkBuffer m_hBuffer;
	};

	SIRENGINE_FORCEINLINE const VkBuffer VulkanVertexBuffer::GetNativeHandle( void ) const
	{
		return m_hBuffer;
	}
};

#endif