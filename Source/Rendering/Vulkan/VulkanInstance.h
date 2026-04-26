#ifndef __VULKAN_INSTANCE_H__
#define __VULKAN_INSTANCE_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "../DisplayConfig.h"
#include "VulkanDevice.h"
#include "VulkanSurface.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanInstance
	{
	public:
		VulkanInstance( SDL_Window *pWindow, const SDisplayConfig& config );
		~VulkanInstance();

		void UpdateFrame( void );
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

		void CreateInstance( SDL_Window *pWindow, const SDisplayConfig& config );

		VkInstance m_hInstance;
		VulkanDevice *m_pDevice;
		VulkanSurface *m_pSurface;
	};
};

#endif