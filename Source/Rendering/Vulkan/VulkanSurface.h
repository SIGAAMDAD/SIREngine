#ifndef __VULKAN_SURFACE_H__
#define __VULKAN_SURFACE_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Pch.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanSurface
	{
	public:
		VulkanSurface( SDL_Window *pWindow, VkInstance hInstance );
		~VulkanSurface();

		VkSurfaceKHR GetSurface( void ) const;
	private:
		const VkInstance m_hInstance;

		VkSurfaceKHR m_hSurface;
	};

	SIRENGINE_FORCEINLINE VkSurfaceKHR VulkanSurface::GetSurface( void ) const
	{
		return m_hSurface;
	}
};

#endif