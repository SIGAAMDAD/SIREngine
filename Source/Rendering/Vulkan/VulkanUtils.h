#ifndef __VULKAN_UTILS_H__
#define __VULKAN_UTILS_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Pch.h"
#include <EASTL/optional.h>

namespace SIREngine::Rendering::Vulkan
{
	struct SQueueFamilyIndices
	{
		eastl::optional<uint32_t> nGraphicsFamily;
		eastl::optional<uint32_t> nPresentSupport;

		bool IsCompleted()
		{
			return nGraphicsFamily.has_value() && nPresentSupport.has_value();
		}
	};

	struct SVulkanInstance
	{
		VkDevice hDevice;
		VkPhysicalDevice hPhysicalDevice;
		VkSurfaceKHR hSurface;
		VmaAllocator Allocator;

		SQueueFamilyIndices QueueFamilyIndices;
	};

	static constexpr const int MAX_FRAMES_IN_FLIGHT = 2;
};

#endif