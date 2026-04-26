#ifndef __VULKAN_COMMAND_BUFFER_H__
#define __VULKAN_COMMAND_BUFFER_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Pch.h"
#include "VulkanUtils.h"
#include "VulkanSwapChain.h"
#include "VulkanVertexBuffer.h"
#include <EASTL/bonus/ring_buffer.h>

namespace SIREngine::Rendering::Vulkan
{
	class VulkanCommandBuffer
	{
	public:
		VulkanCommandBuffer( const SVulkanInstance& instance, VulkanSwapChain *pSwapChain );
		~VulkanCommandBuffer();

		void Reset( void );
		void SubmitQueue( VkQueue hQueue );
		void PresentFrame( VkQueue hPresentQueue );

		void BindVertexBuffer( const VulkanVertexBuffer *pBuffer );

		void BeginRecord( void );
		void EndRecord( void );

		void BeginRenderPass( VkRenderPass hRenderPass, const VkViewport& viewport, const VkRect2D& scissor );
		void EndRenderPass( void );
		
		void BindPipeline( VkPipeline pipeline );

		void Draw( uint32_t nVertexCount, uint32_t nInstanceCount, uint32_t nFirstVertex, uint32_t nFirstInstance );
	private:
		const SVulkanInstance& m_Instance;

		VulkanSwapChain *m_pSwapChain;

		VkCommandPool m_hCommandPool;
		VkCommandBuffer m_hCommandBuffers[ MAX_FRAMES_IN_FLIGHT ];

		VkSemaphore *m_parrhImageAvailableSemaphores;
		VkSemaphore *m_parrhRenderFinishedSemaphores;
		VkFence m_hInFlightFences[ MAX_FRAMES_IN_FLIGHT ];

		uint64_t m_nPresentId;
		uint32_t m_nImageIndex;
		uint32_t m_nCurrentFrame;
		uint32_t m_nPreviousFrame;
	};
};

#endif