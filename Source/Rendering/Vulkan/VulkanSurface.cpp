#include "Pch.h"
#include "VulkanSurface.h"

using namespace SIREngine::Rendering::Vulkan;

VulkanSurface::VulkanSurface( SDL_Window *pWindow, VkInstance hInstance )
	: m_hInstance( hInstance )
{
	SDL_Vulkan_CreateSurface( pWindow, hInstance, NULL, &m_hSurface );
}

VulkanSurface::~VulkanSurface()
{
	if ( m_hSurface != VK_NULL_HANDLE ) {
		SDL_Vulkan_DestroySurface( m_hInstance, m_hSurface, NULL );
	}
}