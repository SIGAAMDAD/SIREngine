#ifndef __VULKAN_DEVICE_H__
#define __VULKAN_DEVICE_H__

#include "Pch.h"
#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "../DisplayConfig.h"
#include "VulkanSwapChain.h"
#include "VulkanSurface.h"
#include "VulkanPipelineBuilder.h"
#include "VulkanUtils.h"
#include "VulkanCommandBuffer.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanDevice
	{
	public:
		VulkanDevice( SDL_Window *pWindow, VkInstance hInstance, const SDisplayConfig& config );
		~VulkanDevice();

		const SVulkanInstance& GetInstance( void ) const;
		void DrawFrame( void );
	private:
		struct SVulkanPhysicalDeviceInfo
		{
			VkPhysicalDeviceFeatures deviceFeatures;
			VkPhysicalDeviceProperties deviceProperties;
			VkPhysicalDevice device;
			uint32_t queueFamilyIndex;
		};
		
		uint32_t EvaluateDevice( VkPhysicalDevice hDevice, SVulkanPhysicalDeviceInfo *pInfo );
		void FindPhysicalDevice( VkInstance hInstance, SVulkanPhysicalDeviceInfo *pInfo );

		bool CheckValidationLayerSupport( void );
		
		VkQueue m_hGraphicsQueue;
		VkQueue m_hPresentQueue;
		SVulkanInstance m_Instance;

		VkRenderPass m_hRenderPass;
		VkPipeline m_hPipeline;

		VulkanSwapChain *m_pSwapChain;

		VulkanSurface *m_pSurface;
		VulkanPipelineBuilder *m_pPipelineBuilder;

		VulkanCommandBuffer *m_pCommandBuffer;

		VmaAllocator m_Allocator;
	};

	SIRENGINE_FORCEINLINE const SVulkanInstance& VulkanDevice::GetInstance( void ) const
	{
		return m_Instance;
	}
};

#endif