#include "Pch.h"
#include "VulkanUtils.h"
#include "VulkanDevice.h"
#include "VulkanVertexLayout.h"

using namespace SIREngine::Rendering::Vulkan;

#ifdef NDEBUG
static constexpr const bool s_bEnableValidationLayers = false;
#else
static constexpr const bool s_bEnableValidationLayers = true;
#endif

static constexpr const char *s_szValidationLayers[] = {
	"VK_LAYER_KHRONOS_validation"
};

static constexpr const char *s_szExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
	VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};

static VulkanVertexLayout *s_pVertexLayout;
static VulkanVertexBuffer *s_pVertexBuffer;

char **GetSupportedExtensions( VkPhysicalDevice hDevice, uint32_t *pSupportedExtensionCount )
{
	VkExtensionProperties *pExtensions;
	static char **ppExtensionList, *pMemory;
	char *pNamePtr;
	uint32_t nActiveExtensions;

	vkEnumerateDeviceExtensionProperties( hDevice, NULL, &nActiveExtensions, NULL );
	pExtensions = (VkExtensionProperties *)calloc( sizeof( *pExtensions ) * nActiveExtensions, 1 );
	ppExtensionList = (char **)calloc( sizeof( *ppExtensionList ) * nActiveExtensions, 1 );
	pMemory = (char *)alloca( 128 * nActiveExtensions );
	memset( pMemory, 0, 128 * nActiveExtensions );
	vkEnumerateDeviceExtensionProperties( hDevice, NULL, &nActiveExtensions, pExtensions );

	printf( "Vulkan API Supported Extensions:\n" );
	for ( uint32_t i = 0; i < nActiveExtensions; i++ ) {
		for ( uint32_t j = 0; j < SIREngine_ArrayLength( s_szExtensions ); j++ ) {
			if ( !strcmp( pExtensions[ i ].extensionName, s_szExtensions[ j ] ) ) {
				size_t length = strlen( s_szExtensions[ j ] ) + 1;
				pNamePtr = pMemory + ( 128 * i );

				strcpy_s( pNamePtr, length, s_szExtensions[ j ] );
				ppExtensionList[ *pSupportedExtensionCount ] = pNamePtr;
				printf( "- %s\n", pNamePtr );

				*pSupportedExtensionCount += 1;
			}
		}
	}

	free( pExtensions );

	return ppExtensionList;
}

bool FindQueueFamilyIndex( VkPhysicalDevice hDevice, VkSurfaceKHR hSurface, SQueueFamilyIndices *pQueueFamilyIndices )
{
	uint32_t nQueueFamilyCount = 0;
	VkQueueFamilyProperties *pProperties;

	vkGetPhysicalDeviceQueueFamilyProperties( hDevice, &nQueueFamilyCount, NULL );
	pProperties = (VkQueueFamilyProperties *)alloca( sizeof( *pProperties ) * nQueueFamilyCount );
	vkGetPhysicalDeviceQueueFamilyProperties( hDevice, &nQueueFamilyCount, pProperties );

	for ( uint32_t i = 0; i < nQueueFamilyCount; i++ ) {
		if ( pProperties[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
			pQueueFamilyIndices->nGraphicsFamily = i;
		}

		VkBool32 bPresentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR( hDevice, i, hSurface, &bPresentSupport );
		
		if ( bPresentSupport ) {
			pQueueFamilyIndices->nPresentSupport = i;
		}
	}

	for ( uint32_t i = 0; i < nQueueFamilyCount; i++ ) {
		VkBool32 bPresentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR( hDevice, i, hSurface, &bPresentSupport );

		if ( ( pProperties[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && bPresentSupport ) {
			pQueueFamilyIndices->nGraphicsFamily = i;
			pQueueFamilyIndices->nPresentSupport = i;
			break;
		}
	}

	return pQueueFamilyIndices->IsCompleted();
}

VkRenderPass CreateRenderPass( VkDevice hDevice, VulkanSwapChain *pSwapChain )
{
	VkAttachmentDescription colorAttachment;
	memset( &colorAttachment, 0, sizeof( colorAttachment ) );
	colorAttachment.format = pSwapChain->GetImageFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef;
	memset( &colorAttachmentRef, 0, sizeof( colorAttachmentRef ) );
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass;
	memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency;
	memset( &dependency, 0, sizeof( dependency ) );
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo createInfo;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = &colorAttachment;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &dependency;

	VkRenderPass hRenderPass;
	vkCreateRenderPass( hDevice, &createInfo, NULL, &hRenderPass );

	return hRenderPass;
}

VulkanDevice::VulkanDevice( SDL_Window *pWindow, VkInstance hInstance, const SDisplayConfig& config )
{
	m_pSurface = new VulkanSurface( pWindow, hInstance );
	m_Instance.hSurface = m_pSurface->GetSurface();
	
	{
		SVulkanPhysicalDeviceInfo physicalDeviceInfo;
		memset( &physicalDeviceInfo, 0, sizeof( physicalDeviceInfo ) );
		FindPhysicalDevice( hInstance, &physicalDeviceInfo );
		m_Instance.hPhysicalDevice = physicalDeviceInfo.device;
		printf( "Selected physical device of %s\n", physicalDeviceInfo.deviceProperties.deviceName );

		SQueueFamilyIndices indices;
		FindQueueFamilyIndex( physicalDeviceInfo.device, m_Instance.hSurface, &indices );

		float defaultPriority = 1.0f;

		VkDeviceQueueCreateInfo arrQueueCreateInfos[ 2 ];
		uint32_t nQueueCreateInfos = 1;

		VkDeviceQueueCreateInfo graphicsQueueCreateInfo;
		memset( &graphicsQueueCreateInfo, 0, sizeof( graphicsQueueCreateInfo ) );
		graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		graphicsQueueCreateInfo.queueFamilyIndex = indices.nGraphicsFamily.value();
		graphicsQueueCreateInfo.queueCount = 1;
		graphicsQueueCreateInfo.pQueuePriorities = &defaultPriority;
		arrQueueCreateInfos[ 0 ] = graphicsQueueCreateInfo;

		if ( indices.nGraphicsFamily.value() != indices.nPresentSupport.value() ) {
			static VkDeviceQueueCreateInfo presentQueueCreateInfo;
			memset( &presentQueueCreateInfo, 0, sizeof( presentQueueCreateInfo ) );
			presentQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			presentQueueCreateInfo.queueFamilyIndex = indices.nPresentSupport.value();
			presentQueueCreateInfo.queueCount = 1;
			presentQueueCreateInfo.pQueuePriorities = &defaultPriority;
			arrQueueCreateInfos[ 1 ] = presentQueueCreateInfo;

			nQueueCreateInfos++;
		}

		uint32_t nSupportedExtensions = 0;
		char **ppSupportedExtensions = GetSupportedExtensions( physicalDeviceInfo.device, &nSupportedExtensions );

		VkDeviceCreateInfo createInfo;
		memset( &createInfo, 0, sizeof( createInfo ) );
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		
		createInfo.pQueueCreateInfos = arrQueueCreateInfos;
		createInfo.queueCreateInfoCount = nQueueCreateInfos;

		createInfo.ppEnabledExtensionNames = s_szExtensions;
		createInfo.enabledExtensionCount = SIREngine_ArrayLength( s_szExtensions );
		
		createInfo.pEnabledFeatures = &physicalDeviceInfo.deviceFeatures;

		if ( s_bEnableValidationLayers ) {
			createInfo.enabledLayerCount = SIREngine_ArrayLength( s_szValidationLayers );
			createInfo.ppEnabledLayerNames = s_szValidationLayers;
		} else {
			createInfo.enabledLayerCount = 0;
		}
	
		VkResult result = vkCreateDevice( m_Instance.hPhysicalDevice, &createInfo, NULL, &m_Instance.hDevice );
		if ( result != VK_SUCCESS ) {
			printf( "VkDevice creation failed due to error %i\n", result );
			return;
		}

		free( ppSupportedExtensions );

		vkGetDeviceQueue( m_Instance.hDevice, m_Instance.QueueFamilyIndices.nGraphicsFamily.value(), 0, &m_hGraphicsQueue );
		vkGetDeviceQueue( m_Instance.hDevice, m_Instance.QueueFamilyIndices.nPresentSupport.value(), 0, &m_hPresentQueue );
	}

	VmaVulkanFunctions vulkanFunctions;
	memset( &vulkanFunctions, 0, sizeof( vulkanFunctions ) );
	vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorInfo;
	memset( &allocatorInfo, 0, sizeof( allocatorInfo ) );
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
	allocatorInfo.physicalDevice = m_Instance.hPhysicalDevice;
	allocatorInfo.device = m_Instance.hDevice;
	allocatorInfo.instance = hInstance;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;

	vmaCreateAllocator( &allocatorInfo, &m_Allocator );
	m_Instance.Allocator = m_Allocator;
	
	m_pSwapChain = new VulkanSwapChain( pWindow, m_Instance );

	VulkanProgram *pProgram = new VulkanProgram( m_Instance.hDevice, "Triangle" );

	{
		m_hRenderPass = CreateRenderPass( m_Instance.hDevice, m_pSwapChain );
		m_pSwapChain->CreateImages( m_hRenderPass );
	}
	
	s_pVertexLayout = new VulkanVertexLayout( m_Instance );

	m_pPipelineBuilder = new VulkanPipelineBuilder( m_Instance, m_hRenderPass, m_pSwapChain );
	
	s_pVertexLayout->AddBinding( { 0, sizeof( SVertex ) } );
	s_pVertexLayout->AddAttribute( { 0, EVertexAttributeType::Float2, 0, false } );
	s_pVertexLayout->AddAttribute( { 1, EVertexAttributeType::Float3, 0, false } );
	s_pVertexLayout->LinkAttributeToBinding( 0, 0 );
	s_pVertexLayout->LinkAttributeToBinding( 1, 0 );
	s_pVertexLayout->Build();

	m_pPipelineBuilder->StartPipeline();
	m_pPipelineBuilder->BindVertexLayout( s_pVertexLayout );
	m_pPipelineBuilder->BindProgram( pProgram );
	m_pPipelineBuilder->SetViewportState( { 640, 480 }, { 0, 0 } );
	m_hPipeline = (VkPipeline)m_pPipelineBuilder->FinishPipeline();

	const SVertex arrVertices[] = {
		{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
	    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
	};

	s_pVertexBuffer = new VulkanVertexBuffer( m_Instance, sizeof( arrVertices ), EStreamType::FlushAndForget, arrVertices );
	m_pCommandBuffer = new VulkanCommandBuffer( m_Instance, m_pSwapChain );
}

VulkanDevice::~VulkanDevice()
{
	if ( m_pPipelineBuilder != NULL ) {
		delete m_pPipelineBuilder;
	}
	if ( m_pSwapChain != NULL ) {
		delete m_pSwapChain;
	}
	if ( m_pSurface != NULL ) {
		delete m_pSurface;
	}
	if ( m_pCommandBuffer != NULL ) {
		delete m_pCommandBuffer;
	}
	if ( m_hRenderPass != VK_NULL_HANDLE ) {
		vkDestroyRenderPass( m_Instance.hDevice, m_hRenderPass, NULL );
	}
	vmaDestroyAllocator( m_Allocator );
	if ( m_Instance.hDevice != VK_NULL_HANDLE ) {
		vkDestroyDevice( m_Instance.hDevice, NULL );
	}
}

void VulkanDevice::DrawFrame( void )
{
	VkViewport viewport;
	memset( &viewport, 0, sizeof( viewport ) );
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = 640;
	viewport.height = 480;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	memset( &scissor, 0, sizeof( scissor ) );
	scissor.extent.width = 640;
	scissor.extent.height = 480;
	scissor.offset = { 0, 0 };

	m_pCommandBuffer->Reset();
	m_pCommandBuffer->BeginRecord();

	{
		m_pCommandBuffer->BeginRenderPass( m_hRenderPass, viewport, scissor );
	
		m_pCommandBuffer->BindPipeline( m_hPipeline );
		
		m_pCommandBuffer->BindVertexBuffer( s_pVertexBuffer );
		m_pCommandBuffer->Draw( 3, 1, 0, 0 );
	
		m_pCommandBuffer->EndRenderPass();
	}

	m_pCommandBuffer->EndRecord();

	m_pCommandBuffer->SubmitQueue( m_hGraphicsQueue );

	m_pCommandBuffer->PresentFrame( m_hPresentQueue );
}

bool VulkanDevice::CheckValidationLayerSupport( void )
{
	VkLayerProperties *pLayerProperties = NULL;
	uint32_t nLayerCount = 0;

	vkEnumerateInstanceLayerProperties( &nLayerCount, NULL );
	pLayerProperties = (VkLayerProperties *)alloca( sizeof( *pLayerProperties ) * nLayerCount );
	vkEnumerateInstanceLayerProperties( &nLayerCount, pLayerProperties );

	int nLayers = SIREngine_ArrayLength( s_szValidationLayers );
	for ( int i = 0; i < nLayers; i++ ) {
		bool bLayerFound = false;

		for ( uint32_t l = 0; l < nLayerCount; l++ ) {
			if ( !strcmp( s_szValidationLayers[ i ], pLayerProperties[ l ].layerName ) ) {
				bLayerFound = true;
				break;
			}
		}
		if ( !bLayerFound ) {
			return false;
		}
	}

	return true;
}

uint32_t VulkanDevice::EvaluateDevice( VkPhysicalDevice hDevice, SVulkanPhysicalDeviceInfo *pInfo )
{
	vkGetPhysicalDeviceProperties( hDevice, &pInfo->deviceProperties );
	vkGetPhysicalDeviceFeatures( hDevice, &pInfo->deviceFeatures );

	uint32_t nScore = 0;
	if ( pInfo->deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
		nScore += 1000;
	}
	else if ( pInfo->deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
		nScore += 500;

	}
	nScore += pInfo->deviceProperties.limits.maxImageDimension2D;
	
	SQueueFamilyIndices indices;
	FindQueueFamilyIndex( hDevice, m_Instance.hSurface, &indices );
	pInfo->queueFamilyIndex = indices.nGraphicsFamily.value();

	return nScore;
}

void VulkanDevice::FindPhysicalDevice( VkInstance hInstance, SVulkanPhysicalDeviceInfo *pInfo )
{
	{ // get the device list
		VkPhysicalDevice *pDeviceList = VK_NULL_HANDLE;
		uint32_t *pScores;
		uint32_t nDeviceCount = 0;

		vkEnumeratePhysicalDevices( hInstance, &nDeviceCount, NULL );
		pDeviceList = (VkPhysicalDevice *)alloca( sizeof( *pDeviceList ) * nDeviceCount );
		pScores = (uint32_t *)alloca( sizeof( *pScores ) * nDeviceCount );
		vkEnumeratePhysicalDevices( hInstance, &nDeviceCount, pDeviceList );

		for ( uint32_t i = 0; i < nDeviceCount; i++ ) {
			pScores[ i ] = EvaluateDevice( pDeviceList[ i ], pInfo );
		}

		uint32_t highestScore = 0;
		for ( uint32_t i = 0; i < nDeviceCount; i++ ) {
			if ( highestScore < pScores[ i ] ) {
				highestScore = pScores[ i ];
				pInfo->device = pDeviceList[ i ];
			}
		}
	}
}