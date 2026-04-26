#include "Pch.h"
#include "VulkanInstance.h"

using namespace SIREngine::Rendering::Vulkan;

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData ) {
	printf( "[VULKAN API]: %s\n", pCallbackData->pMessage );
	return VK_FALSE;
}

VulkanInstance::VulkanInstance( SDL_Window *pWindow, const SDisplayConfig& config )
{
	CreateInstance( pWindow, config );
}

VulkanInstance::~VulkanInstance()
{
	if ( m_hInstance != VK_NULL_HANDLE ) {
		vkDestroyInstance( m_hInstance, NULL );
	}
}

void VulkanInstance::UpdateFrame( void )
{
	m_pDevice->DrawFrame();
}

void VulkanInstance::CreateInstance( SDL_Window *pWindow, const SDisplayConfig& config )
{
	VkApplicationInfo appInfo;
	memset( &appInfo, 0, sizeof( appInfo ) );
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "";
	appInfo.applicationVersion = VK_MAKE_VERSION( 2, 0, 1 );
	appInfo.pEngineName = "SIREngine";
	appInfo.engineVersion = VK_MAKE_VERSION( 2, 0, 0 );
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

#ifndef NDEBUG
	const char *szEnabledExtensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
	createInfo.ppEnabledExtensionNames = szEnabledExtensions;
	createInfo.enabledExtensionCount = 1;

	VkDebugUtilsMessengerCreateInfoEXT debugInfo;
	memset( &debugInfo, 0, sizeof( debugInfo ) );
	debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	debugInfo.pfnUserCallback = DebugCallback;
	
	createInfo.pNext = &debugInfo;
#endif

	VkResult result = vkCreateInstance( &createInfo, NULL, &m_hInstance );
	if ( m_hInstance == NULL ) {
		return;
	}

	m_pDevice = new VulkanDevice( pWindow, m_hInstance, config );
}