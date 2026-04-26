#include "Pch.h"
#include "VulkanContext.h"

using namespace SIREngine::Rendering::Vulkan;

VulkanContext::VulkanContext( const RenderConfig& config )
	: IRenderContext( config )
{
	if ( !SDL_Vulkan_LoadLibrary( NULL ) ) {
	}

	m_pInstance = new VulkanInstance( config.m_DisplayConfig );
}

VulkanContext::~VulkanContext()
{
	if ( m_pInstance != NULL ) {
		delete m_pInstance;
	}
	SDL_Vulkan_UnloadLibrary();
}