#ifndef __VULKAN_BUFFER_H__
#define __VULKAN_BUFFER_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "../IRenderBuffer.h"
#include "Pch.h"
#include "VulkanUtils.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanBuffer : public IRenderBuffer
	{
	public:
		VulkanBuffer( const SVulkanInstance& instance, size_t nSize, EStreamType eUsage, EBufferType eType, const void *pData = NULL );
		virtual ~VulkanBuffer() override;

		virtual void Update( size_t nOffset, size_t nSize, const void *pData ) override;

		virtual void Bind( void ) override;
		virtual void Unbind( void ) override;

		virtual void *Map( EBufferMapAccess eAccess ) override;
		virtual void Unmap( void ) override;

		virtual size_t GetSize( void ) const override;
	protected:
		const SVulkanInstance& m_Instance;

		VkBuffer m_hBuffer;
		VmaAllocation m_Allocation;
	};
};

#endif