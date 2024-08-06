#include "VKContext.h"
#include "VKShaderPipeline.h"
#include <EASTL/algorithm.h>
#include <EASTL/array.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include "VKShader.h"

VKContext *g_pVKContext;

PFN_vkCreateSwapchainKHR fn_vkCreateSwapchainKHR = NULL;
PFN_vkCmdPushDescriptorSetKHR fn_vkCmdPushDescriptorSetKHR = NULL;
PFN_vkCreateDescriptorUpdateTemplateKHR fn_vkCreateDescriptorUpdateTemplateKHR = NULL;

static PFN_vkDestroyDebugUtilsMessengerEXT fn_vkDestroyDebugUtilsMessengerEXT = NULL;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void *pUserData
)
{
    if ( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT || messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ) {
        SIRENGINE_WARNING( "[VULKAN DEBUG] %s", pCallbackData->pMessage );
    } else if ( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ) {
        SIRENGINE_LOG( "[VULKAN DEBUG] %s", pCallbackData->pMessage );
    }
    switch ( messageType ) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        break;
    };

    return VK_FALSE;
}

void VkError( const char *pCall, VkResult nResult )
{
    typedef struct VkErrorString {
        const char *pString;
        VkResult nError;
    } VkErrorString;

    #define VK_ERROR( x ) { SIRENGINE_XSTRING( x ), x }
    const VkErrorString szErrorStrings[] = {
        VK_ERROR( VK_SUCCESS ),
        VK_ERROR( VK_NOT_READY ),
        VK_ERROR( VK_TIMEOUT ),
        VK_ERROR( VK_EVENT_SET ),
        VK_ERROR( VK_EVENT_RESET ),
        VK_ERROR( VK_INCOMPLETE ),
        VK_ERROR( VK_ERROR_OUT_OF_HOST_MEMORY ),
        VK_ERROR( VK_ERROR_OUT_OF_DEVICE_MEMORY ),
        VK_ERROR( VK_ERROR_INITIALIZATION_FAILED ),
        VK_ERROR( VK_ERROR_DEVICE_LOST ),
        VK_ERROR( VK_ERROR_MEMORY_MAP_FAILED ),
        VK_ERROR( VK_ERROR_LAYER_NOT_PRESENT ),
        VK_ERROR( VK_ERROR_EXTENSION_NOT_PRESENT ),
        VK_ERROR( VK_ERROR_FEATURE_NOT_PRESENT ),
        VK_ERROR( VK_ERROR_INCOMPATIBLE_DRIVER ),
        VK_ERROR( VK_ERROR_TOO_MANY_OBJECTS ),
        VK_ERROR( VK_ERROR_FORMAT_NOT_SUPPORTED ),
        VK_ERROR( VK_ERROR_FRAGMENTED_POOL ),
        VK_ERROR( VK_ERROR_UNKNOWN ),
        VK_ERROR( VK_ERROR_OUT_OF_POOL_MEMORY ),
        VK_ERROR( VK_ERROR_INVALID_EXTERNAL_HANDLE ),
        VK_ERROR( VK_ERROR_FRAGMENTATION ),
        VK_ERROR( VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS ),
        VK_ERROR( VK_PIPELINE_COMPILE_REQUIRED ),
        VK_ERROR( VK_ERROR_SURFACE_LOST_KHR ),
        VK_ERROR( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR ),
        VK_ERROR( VK_SUBOPTIMAL_KHR),
        VK_ERROR( VK_ERROR_OUT_OF_DATE_KHR ),
        VK_ERROR( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR ),
        VK_ERROR( VK_ERROR_VALIDATION_FAILED_EXT ),
        VK_ERROR( VK_ERROR_INVALID_SHADER_NV ),
        VK_ERROR( VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT ),
        VK_ERROR( VK_ERROR_NOT_PERMITTED_KHR ),
        VK_ERROR( VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT ),
        VK_ERROR( VK_THREAD_IDLE_KHR ),
        VK_ERROR( VK_THREAD_DONE_KHR ),
        VK_ERROR( VK_OPERATION_DEFERRED_KHR ),
        VK_ERROR( VK_OPERATION_NOT_DEFERRED_KHR ),
        VK_ERROR( VK_ERROR_OUT_OF_POOL_MEMORY_KHR ),
        VK_ERROR( VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR ),
        VK_ERROR( VK_ERROR_FRAGMENTATION_EXT ),
        VK_ERROR( VK_ERROR_NOT_PERMITTED_EXT ),
        VK_ERROR( VK_ERROR_INVALID_DEVICE_ADDRESS_EXT ),
        VK_ERROR( VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR ),
        VK_ERROR( VK_PIPELINE_COMPILE_REQUIRED_EXT ),
        VK_ERROR( VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT ),
    };

    const VkErrorString *pError;
    for ( const auto& it : szErrorStrings ) {
        if ( it.nError == nResult ) {
            pError = eastl::addressof( it );
        }
    }
    
    if ( nResult == VK_ERROR_UNKNOWN ) {
        SIRENGINE_WARNING( "%s: VK_ERROR_UNKNOWN", pCall );
    } else {
        SIRENGINE_ERROR( "%s: %s (%u)", pCall, pError->pString, pError->nError );
    }
}

static int RateVKDevice( VkPhysicalDevice hDevice )
{
    int score;

    score = 0;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties( hDevice, &deviceProperties );

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures( hDevice, &deviceFeatures );

    switch ( deviceProperties.deviceType ) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        // dedicated GPUs have the greatest performance
        score += 100000;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        // most likely an intel GPU
        score += 700;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        // software rendering, the slowest
        score += 200;
        break;
    };

    // this engine is more optimized on specific vendors
    switch ( deviceProperties.vendorID ) {
    case VK_VENDOR_ID_MESA:
        score += 1000;
        break;
    };

    score += deviceProperties.limits.maxImageDimension2D;

    // add some bonus points for framebuffer features
    score += deviceProperties.limits.maxFramebufferWidth;
    score += deviceProperties.limits.maxFramebufferHeight;

    score += deviceProperties.limits.maxDescriptorSetSamplers;
    score += deviceProperties.limits.maxDescriptorSetUniformBuffers;

    score += deviceProperties.limits.maxVertexInputAttributeOffset;
    score += deviceProperties.limits.maxVertexInputAttributes;
    score += deviceProperties.limits.maxVertexInputBindings;
    score += deviceProperties.limits.maxVertexInputBindingStride;

    // 16-bit shader input required for colors, TODO: make it so that this is
    // not a required device feature
    if ( !deviceFeatures.shaderInt16 ) {
        score = 0;
    }
    if ( !deviceFeatures.fullDrawIndexUint32 ) {
        score = 0;
    }
    if ( !deviceFeatures.textureCompressionBC ) {
        score += 500;
    }
    if ( !deviceFeatures.textureCompressionETC2 ) {
        score += 500;
    }

    SIRENGINE_LOG( "VkPhysicalDevice Score: %i", score );

    return score;
}

static void DumpPhysicalDeviceFeatures( VkPhysicalDevice hDevice )
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties( hDevice, &deviceProperties );

    SIRENGINE_LOG( "Vulkan Driver Query:" );
    SIRENGINE_LOG( "  Device Name: %s", deviceProperties.deviceName );
    SIRENGINE_LOG( "  Device ID: %u", deviceProperties.deviceID );
    switch ( deviceProperties.deviceType ) {
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        SIRENGINE_LOG( "  Device Type: CPU/Software Rendering" );
        break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        SIRENGINE_LOG( "  Device Type: Dedicated Graphics Card" );
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        SIRENGINE_LOG( "  Device Type: Integrated Graphics Card" );
        break;
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        SIRENGINE_LOG( "  Device Type: Unknown" );
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        SIRENGINE_LOG( "  Device Type: Virtualized GPU" );
        break;
    };
    SIRENGINE_LOG( "  Driver Version: %u", deviceProperties.driverVersion );
    SIRENGINE_LOG( "  Vulkan API Version: %u", deviceProperties.apiVersion );
    switch ( deviceProperties.vendorID ) {
    case VK_VENDOR_ID_CODEPLAY:
        SIRENGINE_LOG( "  Vendor: Codeplay Software" );
        break;
    case VK_VENDOR_ID_KAZAN:
        SIRENGINE_LOG( "  Vendor: Kazan Graphics" );
        break;
    case VK_VENDOR_ID_MESA:
        SIRENGINE_LOG( "  Vendor: Mesa (NVidia)" );
        break;
    case VK_VENDOR_ID_POCL:
        SIRENGINE_LOG( "  Vendor: PoCL Think Silicon" );
        break;
    case VK_VENDOR_ID_VIV:
        SIRENGINE_LOG( "  Vendor: VIV" );
        break;
    case VK_VENDOR_ID_VSI:
        SIRENGINE_LOG( "  Vendor: VSI Media" );
        break;
    };
    {
        char str[ VK_UUID_SIZE + 1 ];
        memcpy( str, deviceProperties.pipelineCacheUUID, sizeof( deviceProperties.pipelineCacheUUID ) );
        str[ VK_UUID_SIZE ] = '\0';

        SIRENGINE_LOG( "  Pipeline Cache UUID: %s", str );    
    }
    SIRENGINE_LOG( "(LIMITS)" );
    SIRENGINE_LOG( " - maxColorAttachments: %u", deviceProperties.limits.maxColorAttachments );
    SIRENGINE_LOG( " - maxDescriptorSetStorageBuffers: %u",
        deviceProperties.limits.maxDescriptorSetStorageBuffers );
    SIRENGINE_LOG( " - maxDescriptorSetInputAttachments: %u",
        deviceProperties.limits.maxDescriptorSetInputAttachments );
    SIRENGINE_LOG( " - maxDescriptorSetSampledImages: %u",
        deviceProperties.limits.maxDescriptorSetSampledImages );
    SIRENGINE_LOG( " - maxDescriptorSetSamplers: %u",
        deviceProperties.limits.maxDescriptorSetSamplers );
    SIRENGINE_LOG( " - maxDescriptorSetStorageBuffersDynamic: %u",
        deviceProperties.limits.maxDescriptorSetStorageBuffersDynamic );
    SIRENGINE_LOG( " - maxDescriptorSetStorageImages: %u",
        deviceProperties.limits.maxDescriptorSetStorageImages );
    SIRENGINE_LOG( " - maxDescriptorSetUniformBuffers: %u",
        deviceProperties.limits.maxDescriptorSetUniformBuffers );
    SIRENGINE_LOG( " - maxDescriptorSetUniformBuffersDynamic: %u",
        deviceProperties.limits.maxDescriptorSetUniformBuffersDynamic );
    SIRENGINE_LOG( " - maxFramebufferWidth: %u",
        deviceProperties.limits.maxFramebufferWidth );
    SIRENGINE_LOG( " - maxFramebufferHeight: %u",
        deviceProperties.limits.maxFramebufferHeight );
    SIRENGINE_LOG( " - maxImageDimension2D: %u",
        deviceProperties.limits.maxImageDimension2D );
    SIRENGINE_LOG( " - maxUniformBufferRange: %u",
        deviceProperties.limits.maxUniformBufferRange );
    SIRENGINE_LOG( " - maxVertexInputAttributeOffset: %u",
        deviceProperties.limits.maxVertexInputAttributeOffset );
    SIRENGINE_LOG( " - maxVertexInputAttributes: %u",
        deviceProperties.limits.maxVertexInputAttributes );
    SIRENGINE_LOG( " - maxVertexInputBindings: %u",
        deviceProperties.limits.maxVertexInputBindings );
    SIRENGINE_LOG( " - maxVertexInputBindingStride: %u",
        deviceProperties.limits.maxVertexInputBindingStride );
    SIRENGINE_LOG( " - maxVertexOutputComponents: %u",
        deviceProperties.limits.maxVertexOutputComponents );
}

QueueFamilyIndices_t VKContext::FindQueueFamilies( void )
{
    QueueFamilyIndices_t indices;
    uint32_t nQueueFamilyCount, i;
    VkQueueFamilyProperties *pQueueProperties;

    memset( &indices, 0, sizeof( indices ) );

    nQueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( m_hPhysicalDevice, &nQueueFamilyCount, NULL );

    pQueueProperties = (VkQueueFamilyProperties *)alloca( sizeof( *pQueueProperties ) * nQueueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( m_hPhysicalDevice, &nQueueFamilyCount, pQueueProperties );

    for ( i = 0; i < nQueueFamilyCount; i++ ) {
        if ( pQueueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
            indices.bHasGraphicsFamily = true;
            indices.nGraphicsFamily = i;

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR( m_hPhysicalDevice, i, m_hSurface, &presentSupport );
            if ( presentSupport ) {
                indices.bHasPresentFamily = true;
                indices.nPresentFamily = i;
            }

            if ( indices.IsComplete() ) {
                break;
            }

            i++;
        }
    }

    return indices;
}

SwapChainSupportInfo_t& VKContext::QuerySwapChainSupport( void )
{
    static SwapChainSupportInfo_t swapChainSupport;
    uint32_t nFormatCount, nPresentModeCount;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( m_hPhysicalDevice, m_hSurface, &swapChainSupport.capabilities );

    vkGetPhysicalDeviceSurfaceFormatsKHR( m_hPhysicalDevice, m_hSurface, &nFormatCount, NULL );
    if ( nFormatCount != 0 ) {
        static VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR *)alloca( sizeof( *formats ) * nFormatCount );
        memset( formats, 0, sizeof( *formats ) * nFormatCount );
        swapChainSupport.pFormats = formats;
        swapChainSupport.nFormats = nFormatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR( m_hPhysicalDevice, m_hSurface, &nFormatCount, formats );
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR( m_hPhysicalDevice, m_hSurface, &nPresentModeCount, NULL );
    if ( nPresentModeCount != 0 ) {
        static VkPresentModeKHR *presentModes = (VkPresentModeKHR *)alloca(
            sizeof( *presentModes ) * nPresentModeCount );
        memset( presentModes, 0, sizeof( *presentModes ) * nPresentModeCount );
        
        swapChainSupport.pPresentModes = presentModes;
        swapChainSupport.nPresentModes = nPresentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR( m_hPhysicalDevice, m_hSurface, &nPresentModeCount, presentModes );
    }

    return swapChainSupport;
}

VkSurfaceFormatKHR VKContext::ChooseSwapSurfaceFormat( const VkSurfaceFormatKHR *pAvailableFormats,
    uint32_t nAvailableFormats )
{
    uint32_t i;
    
    for ( i = 0; i < nAvailableFormats; i++ ) {
        if ( pAvailableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB
            && pAvailableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
        {
            return pAvailableFormats[i];
        }
    }

    return pAvailableFormats[0];
}

VkPresentModeKHR VKContext::ChooseSwapPresentMode( const VkPresentModeKHR *pAvailablePresentModes,
    uint32_t nAvailablePresentModes )
{
    uint32_t i;

    for ( i = 0; i < nAvailablePresentModes; i++ ) {
        if ( pAvailablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR ) {
            return pAvailablePresentModes[i];
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VKContext::ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities, int nWidth, int nHeight )
{
    if ( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = { (uint32_t)nWidth, (uint32_t)nHeight };

        actualExtent.width = eastl::clamp( actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width );
        actualExtent.height = eastl::clamp( actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height );
        
        return actualExtent;
    }
}

static void *Vulkan_Allocate( void *pUserData, size_t nSize, size_t nAlignment, VkSystemAllocationScope scope )
{
    void *pBuffer;

#if 0
    pBuffer = g_pMemAlloc->Alloc( nSize );
#else
    pBuffer = malloc( nSize );
#endif

    return pBuffer;
}

static void Vulkan_Free( void *pUserData, void *pMemory )
{
#if 0
    g_pMemAlloc->Free( pMemory );
#else
    ::free( pMemory );
#endif
}

static void *Vulkan_Reallocate( void *pUserData, void *pOriginal, size_t nSize, size_t nAlignment, VkSystemAllocationScope scope )
{
    void *pBuffer;

#if 0
    pBuffer = g_pMemAlloc->Realloc( pOriginal, nSize );
//    if ( pOriginal ) {
//        size_t nAllocSize = g_pMemAlloc->GetAllocSize( pOriginal );
//		memcpy( pBuffer, pOriginal, nAllocSize <= nSize ? nSize : nAllocSize );
//        g_pMemAlloc->Free( pOriginal );
//	}
#else
    pBuffer = realloc( pOriginal, nSize );
#endif

    return pBuffer;
}

VKContext::VKContext( const ApplicationInfo_t& appInfo )
    : IRenderContext( appInfo )
{
}

VKContext::~VKContext()
{
}

void VKContext::Init( void )
{
    SIRENGINE_LOG( "Initializing VKContext" );

    memset( &m_AllocationCallbacks, 0, sizeof( m_AllocationCallbacks ) );
    m_AllocationCallbacks.pfnAllocation = Vulkan_Allocate;
    m_AllocationCallbacks.pfnFree = Vulkan_Free;
    m_AllocationCallbacks.pfnReallocation = Vulkan_Reallocate;

    m_nCurrentFrameIndex = 0;

    InitWindowInstance();
    InitPhysicalVKDevice();
    InitLogicalDevice();
    InitRenderPass();
    InitSwapChain();
}

void VKContext::Shutdown( void )
{
    PrintMemoryInfo();

    m_pGenericShader->~VKProgram();
    Free( m_pGenericShader );

    m_hShaderPipeline->~IRenderShaderPipeline();
    Free( m_hShaderPipeline );

    ShutdownSwapChain();

    vmaDestroyAllocator( m_hAllocator );

    vkDestroyFence( m_hDevice, m_hInFlightFences[0], GetAllocationCallbacks() );
    vkDestroyFence( m_hDevice, m_hInFlightFences[1], GetAllocationCallbacks() );

    vkDestroySemaphore( m_hDevice, m_hRenderFinished[0], GetAllocationCallbacks() );
    vkDestroySemaphore( m_hDevice, m_hRenderFinished[1], GetAllocationCallbacks() );

    vkDestroySemaphore( m_hDevice, m_hSwapChainImageAvailable[0], GetAllocationCallbacks() );
    vkDestroySemaphore( m_hDevice, m_hSwapChainImageAvailable[1], GetAllocationCallbacks() );

    vkDestroyRenderPass( m_hDevice, m_hRenderPass, GetAllocationCallbacks() );

    vkDestroyCommandPool( m_hDevice, m_hCommandPool, GetAllocationCallbacks() );

    fn_vkDestroyDebugUtilsMessengerEXT( m_hInstance, m_hDebugHandler, GetAllocationCallbacks() );
    vkDestroySurfaceKHR( m_hInstance, m_hSurface, NULL );
    vkDestroyDevice( m_hDevice, GetAllocationCallbacks() );
    vkDestroyInstance( m_hInstance, GetAllocationCallbacks() );
}

void VKContext::RecreateSwapChain( void )
{
    int width, height;
    SDL_Event event;

    SDL_GetWindowSize( m_pWindow, &width, &height );

    while ( width == 0 || height == 0 ) {
        SDL_GetWindowSize( m_pWindow, &width, &height );
        SDL_WaitEvent( &event );
    }

    VK_CALL( vkDeviceWaitIdle( m_hDevice ) );

    ShutdownSwapChain();
    InitSwapChain();    
}

/*
void VKContext::InitDescriptorCache( void )
{
    VkDescriptorSetLayoutBinding *pBindings;
    VkDescriptorPoolSize *pPoolSizes;
    VkWriteDescriptorSet *pWriteSets;
    uint32_t i, j;

    // FIXME: find some other way of doing THIS
    const VkDescriptorType szDescriptorTypes[ NumUniforms ] = {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    };

    {
        VkImageViewCreateInfo createInfo;
        memset( &createInfo, 0, sizeof( createInfo ) );
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        createInfo.image = VK_NULL_HANDLE;
        createInfo.subresourceRange.layerCount = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        VK_CALL( vkCreateImageView( m_hDevice, &createInfo, NULL, &m_UniformDiffuseMap ) );
    }

    {
        VkImageViewCreateInfo createInfo;
        memset( &createInfo, 0, sizeof( createInfo ) );
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        createInfo.image = VK_NULL_HANDLE;
        createInfo.subresourceRange.layerCount = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        VK_CALL( vkCreateImageView( m_hDevice, &createInfo, NULL, &m_UniformNormalMap ) );
    }

    {
        VkImageViewCreateInfo createInfo;
        memset( &createInfo, 0, sizeof( createInfo ) );
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        createInfo.image = VK_NULL_HANDLE;
        createInfo.subresourceRange.layerCount = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        VK_CALL( vkCreateImageView( m_hDevice, &createInfo, NULL, &m_UniformSpecularMap ) );
    }

    pPoolSizes = (VkDescriptorPoolSize *)alloca( sizeof( *pPoolSizes ) * NumUniformTypes );
    memset( pPoolSizes, 0, sizeof( *pPoolSizes ) * NumUniformTypes );

    {
        // image samplers
        pPoolSizes[0].descriptorCount = 1;
        pPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        // uniform buffers
        pPoolSizes[1].descriptorCount = 1;
        pPoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    VkDescriptorPoolCreateInfo poolInfo;
    memset( &poolInfo, 0, sizeof( poolInfo ) );
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = NumUniforms;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = pPoolSizes;

    VK_CALL( vkCreateDescriptorPool( m_hDevice, &poolInfo, NULL, &m_DescriptorCache.hPool ) );

    pBindings = (VkDescriptorSetLayoutBinding *)alloca( sizeof( *pBindings ) * NumUniforms );

    m_DescriptorCache.pLayouts = (VkDescriptorSetLayout *)Alloc( sizeof( *m_DescriptorCache.pLayouts ) * VK_MAX_FRAMES_IN_FLIGHT );
    m_DescriptorCache.pSets = (VkDescriptorSet *)Alloc( sizeof( *m_DescriptorCache.pSets ) * VK_MAX_FRAMES_IN_FLIGHT );

    { // fragment shader descriptor set
        memset( pBindings, 0, sizeof( *pBindings ) * NumUniforms );

        // DiffuseMap
        pBindings[0].binding = 0;
        pBindings[0].descriptorCount = 1;
        pBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pBindings[0].pImmutableSamplers = VK_NULL_HANDLE;
        pBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo setLayoutInfo;
        memset( &setLayoutInfo, 0, sizeof( setLayoutInfo ) );
        setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayoutInfo.bindingCount = 1;
        setLayoutInfo.pBindings = pBindings;

        VK_CALL( vkCreateDescriptorSetLayout( m_hDevice, &setLayoutInfo, NULL, &m_DescriptorCache.pLayouts[ 0 ] ) );
    }

    { // vertex shader descriptor set
        memset( pBindings, 0, sizeof( *pBindings ) * NumUniforms );

        // VertexInput
        pBindings[0].binding = 1;
        pBindings[0].descriptorCount = 1;
        pBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pBindings[0].pImmutableSamplers = VK_NULL_HANDLE;
        pBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo setLayoutInfo;
        memset( &setLayoutInfo, 0, sizeof( setLayoutInfo ) );
        setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayoutInfo.bindingCount = 1;
        setLayoutInfo.pBindings = pBindings;

        VK_CALL( vkCreateDescriptorSetLayout( m_hDevice, &setLayoutInfo, NULL, &m_DescriptorCache.pLayouts[ 1 ] ) );
    }

    VkDescriptorSetAllocateInfo allocInfo;
    memset( &allocInfo, 0, sizeof( allocInfo ) );
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorCache.hPool;
    allocInfo.descriptorSetCount = VK_MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = m_DescriptorCache.pLayouts;

    VK_CALL( vkAllocateDescriptorSets( m_hDevice, &allocInfo, m_DescriptorCache.pSets ) );

    pWriteSets = (VkWriteDescriptorSet *)alloca( sizeof( *pWriteSets ) * VK_MAX_FRAMES_IN_FLIGHT );
    memset( pWriteSets, 0, sizeof( *pWriteSets ) * VK_MAX_FRAMES_IN_FLIGHT );

    for ( i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ ) {
        pWriteSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pWriteSets[i].descriptorCount = 1;
        pWriteSets[i].descriptorType = pBindings[i].descriptorType;
        pWriteSets[i].dstBinding = pBindings[i].binding;
        pWriteSets[i].dstSet = m_DescriptorCache.pSets[i];
        if ( szDefaultUniforms[i].nType == UniformType_Buffer ) {
            static VkDescriptorBufferInfo bufferInfo;
            memset( &bufferInfo, 0, sizeof( bufferInfo ) );
            bufferInfo.range = szDefaultUniforms[i].nSize;
            bufferInfo.offset = 0;
            bufferInfo.buffer = VK_NULL_HANDLE;
            pWriteSets[i].pBufferInfo = &bufferInfo;
        }
        else if ( szDefaultUniforms[i].nType == UniformType_Sampler ) {
            static VkDescriptorImageInfo imageInfo;
            memset( &imageInfo, 0, sizeof( imageInfo ) );
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            switch ( szDefaultUniforms[i].nIndex ) {
            case Uniform_DiffuseMap:
                imageInfo.imageView = m_UniformDiffuseMap;
                break;
            case Uniform_NormalMap:
                imageInfo.imageView = m_UniformNormalMap;
                break;
            case Uniform_SpecularMap:
                imageInfo.imageView = m_UniformSpecularMap;
                break;
            };
            imageInfo.sampler = VK_NULL_HANDLE;
            pWriteSets[i].pImageInfo = &imageInfo;
        }
    }
    vkUpdateDescriptorSets( m_hDevice, VK_MAX_FRAMES_IN_FLIGHT, pWriteSets, 0, NULL );

    m_DescriptorCache.nSetCount = VK_MAX_FRAMES_IN_FLIGHT;
}
*/

void VKContext::InitSwapChain( void )
{
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkExtent2D extent;
    uint32_t nImageCount, i;
    QueueFamilyIndices_t queueIndices;

    const SwapChainSupportInfo_t& swapChainSupport = QuerySwapChainSupport();
    surfaceFormat = ChooseSwapSurfaceFormat( swapChainSupport.pFormats, swapChainSupport.nFormats );
    presentMode = ChooseSwapPresentMode( swapChainSupport.pPresentModes, swapChainSupport.nPresentModes );
    extent = ChooseSwapExtent( swapChainSupport.capabilities,
        g_pApplication->GetAppInfo().nWindowWidth, g_pApplication->GetAppInfo().nWindowHeight );

    nImageCount = swapChainSupport.capabilities.minImageCount + 1;
    if ( swapChainSupport.capabilities.maxImageCount > 0 && nImageCount > swapChainSupport.capabilities.maxImageCount ) {
        nImageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo;
    memset( &createInfo, 0, sizeof( createInfo ) );
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = GetSurface();
    createInfo.minImageCount = nImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    queueIndices = FindQueueFamilies();
    const bool32 queueFamilyIndices[] = {
        queueIndices.bHasGraphicsFamily, queueIndices.bHasPresentFamily
    };

    if ( queueIndices.nGraphicsFamily != queueIndices.nPresentFamily ) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CALL( fn_vkCreateSwapchainKHR( m_hDevice, &createInfo, GetAllocationCallbacks(), &m_hSwapChain ) );

    VK_CALL( vkGetSwapchainImagesKHR( m_hDevice, m_hSwapChain, &nImageCount, NULL ) );
    m_SwapChainImages.resize( nImageCount );
    VK_CALL( vkGetSwapchainImagesKHR( m_hDevice, m_hSwapChain, &nImageCount, m_SwapChainImages.data() ) );
    SIRENGINE_LOG( "Allocated VKSwapchainKHR Object %u.", i );

    m_nSwapChainFormat = surfaceFormat.format;
    m_nSwapChainFormat = VK_FORMAT_B8G8R8A8_SRGB;
    m_nSwapChainExtent = extent;

    m_SwapChainImageViews.resize( m_SwapChainImages.size() );

    for ( i = 0; i < m_SwapChainImages.size(); i++ ) {
        VkImageViewCreateInfo imageViewInfo;
        memset( &imageViewInfo, 0, sizeof( imageViewInfo ) );
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = m_SwapChainImages[i];
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format = m_nSwapChainFormat;
        imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;

        VK_CALL( vkCreateImageView( m_hDevice, &imageViewInfo, GetAllocationCallbacks(), &m_SwapChainImageViews[i] ) );

        SIRENGINE_LOG( "Allocated SwapChain VkImageView Object %u.", i );
    }

    m_SwapChainFramebuffers.resize( m_SwapChainImageViews.size() );

    for ( i = 0; i < m_SwapChainFramebuffers.size(); i++ ) {
        VkImageView szAttachments[] = {
            m_SwapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo;
        memset( &framebufferInfo, 0, sizeof( framebufferInfo ) );
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_hRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = szAttachments;
        framebufferInfo.width = g_pApplication->GetAppInfo().nWindowWidth;
        framebufferInfo.height = g_pApplication->GetAppInfo().nWindowHeight;
        framebufferInfo.layers = 1;

        VK_CALL( vkCreateFramebuffer( m_hDevice, &framebufferInfo, GetAllocationCallbacks(), &m_SwapChainFramebuffers[i] ) );
        SIRENGINE_LOG( "Allocated SwapChain VkFramebuffer Object %u.", i );
    }
}

void VKContext::ShutdownSwapChain( void )
{
    for ( auto it : m_SwapChainImageViews ) {
        if ( it ) {
            vkDestroyImageView( m_hDevice, it, GetAllocationCallbacks() );
        }
    }
    m_SwapChainImageViews.clear();

    for ( size_t i = 0; i < m_SwapChainImageAllocations.size(); i++ ) {
        if ( m_SwapChainImages[i] ) {
            vmaDestroyImage( m_hAllocator, m_SwapChainImages[i], m_SwapChainImageAllocations[i] );
        }
    }
    m_SwapChainImages.clear();
    m_SwapChainImageAllocations.clear();

    for ( auto it : m_SwapChainFramebuffers ) {
        if ( it ) {
            vkDestroyFramebuffer( m_hDevice, it, GetAllocationCallbacks() );
        }
    }

    if ( m_hSwapChain ) {
        vkDestroySwapchainKHR( m_hDevice, m_hSwapChain, GetAllocationCallbacks() );
    }
}

void VKContext::InitWindowInstance( void )
{
    uint32_t nExtensionCount, i;
    const char **pExtensionList;
    const char *szLayers[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    CVector<const char *> usedExtensions;
    uint32_t nUsedExtensions;
    const char *szRequiredExtensions[] = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    if ( !SDL_Vulkan_GetInstanceExtensions( m_pWindow, &nExtensionCount, NULL ) ) {
        SIRENGINE_ERROR( "SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError() );
    }
    pExtensionList = (const char **)alloca( sizeof( *pExtensionList ) * nExtensionCount );
    if ( !SDL_Vulkan_GetInstanceExtensions( m_pWindow, &nExtensionCount, pExtensionList ) ) {
        SIRENGINE_ERROR( "SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError() );
    }

    nUsedExtensions = ( nExtensionCount + SIREngine_StaticArrayLength( szRequiredExtensions ) );

    usedExtensions.reserve( nUsedExtensions );
    for ( i = 0; i < nExtensionCount; i++ ) {
        usedExtensions.emplace_back( pExtensionList[i] );
    }
    for ( i = 0; i < SIREngine_StaticArrayLength( szRequiredExtensions ); i++ ) {
        usedExtensions.emplace_back( szRequiredExtensions[i] );
    }

    SIRENGINE_LOG( "Found %u Extensions.", nExtensionCount );
    SIRENGINE_LOG( "Using Vulkan Instance Extensions:" );
    for ( i = 0; i < nUsedExtensions; i++ ) {
        SIRENGINE_LOG( "- \"%s\"", usedExtensions[i] );
    }

    VkApplicationInfo appInfo;
    memset( &appInfo, 0, sizeof( appInfo ) );
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_AppInfo.pszAppName; // user defined window name
    appInfo.applicationVersion = m_AppInfo.nAppVersion;
    appInfo.pEngineName = SIRENGINE_VERSION_STRING;
    appInfo.engineVersion = SIRENGINE_VERSION;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    SIRENGINE_LOG( "Fetching VulkanInstance extensions..." );

    VkInstanceCreateInfo createInfo;
    memset( &createInfo, 0, sizeof( createInfo ) );
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

//    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    createInfo.enabledExtensionCount = usedExtensions.size();
    createInfo.ppEnabledExtensionNames = usedExtensions.data();

    createInfo.enabledLayerCount = SIREngine_StaticArrayLength( szLayers );
    createInfo.ppEnabledLayerNames = szLayers;

    VK_CALL( vkCreateInstance( &createInfo, GetAllocationCallbacks(), &m_hInstance ) );
    SIRENGINE_LOG( "VKContext Instance created." );

    if ( !SDL_Vulkan_CreateSurface( m_pWindow, m_hInstance, &m_hSurface ) ) {
        SIRENGINE_ERROR( "SDL_Vulkan_CreateSurface failed: %s", SDL_GetError() );
    }
    SIRENGINE_LOG( "VKContext VkSurfaceKHR allocated." );
}

void VKContext::InitCommandPool( void )
{
    const QueueFamilyIndices_t indices = FindQueueFamilies();

    VkCommandPoolCreateInfo poolInfo;
    memset( &poolInfo, 0, sizeof( poolInfo ) );
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.nGraphicsFamily;

    VK_CALL( vkCreateCommandPool( m_hDevice, &poolInfo, GetAllocationCallbacks(), &m_hCommandPool ) );
    SIRENGINE_LOG( "Allocated VkCommandPool Object." );

    VkCommandBufferAllocateInfo allocInfo;
    memset( &allocInfo, 0, sizeof( allocInfo ) );
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_hCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = SIREngine_StaticArrayLength( m_hCommandBuffers );

    VK_CALL( vkAllocateCommandBuffers( m_hDevice, &allocInfo, m_hCommandBuffers ) );
    SIRENGINE_LOG( "Allocated VkCommandBuffer Objects." );

    VkSemaphoreCreateInfo semaphoreInfo;
    memset( &semaphoreInfo, 0, sizeof( semaphoreInfo ) );
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo;
    memset( &fenceInfo, 0, sizeof( fenceInfo ) );
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CALL( vkCreateFence( m_hDevice, &fenceInfo, GetAllocationCallbacks(), &m_hInFlightFences[0] ) );
    VK_CALL( vkCreateFence( m_hDevice, &fenceInfo, GetAllocationCallbacks(), &m_hInFlightFences[1] ) );

    VK_CALL( vkCreateSemaphore( m_hDevice, &semaphoreInfo, GetAllocationCallbacks(), &m_hSwapChainImageAvailable[0] ) );
    VK_CALL( vkCreateSemaphore( m_hDevice, &semaphoreInfo, GetAllocationCallbacks(), &m_hSwapChainImageAvailable[1] ) );

    VK_CALL( vkCreateSemaphore( m_hDevice, &semaphoreInfo, GetAllocationCallbacks(), &m_hRenderFinished[0] ) );
    VK_CALL( vkCreateSemaphore( m_hDevice, &semaphoreInfo, GetAllocationCallbacks(), &m_hRenderFinished[1] ) );

    SIRENGINE_LOG( "Allocated Vulkan Synchronization Objects." );
}

void VKContext::InitLogicalDevice( void )
{
    VkDeviceQueueCreateInfo szQueueCreateInfos[2];
    uint32_t nQueueCreateInfoCount, i;
    float nQueuePriority;
    const std::initializer_list<const CString> szRequiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
        VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
        VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,
        VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
    };
    const char *szLayers[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    const VkValidationFeatureEnableEXT szEnabledValidationFeatures[] = {
        VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
    };
    uint32_t nUsedExtensions;
    VkExtensionProperties *pExtensionList;
    CVector<const char *> usedExtensions;

    const QueueFamilyIndices_t indices = FindQueueFamilies();
    const uint32_t szUniqueQueueFamilies[] = { indices.nGraphicsFamily, indices.nPresentFamily };

    nQueuePriority = 1.0f;
    memset( szQueueCreateInfos, 0, sizeof( szQueueCreateInfos ) );
    for ( i = 0; i < SIREngine_StaticArrayLength( szUniqueQueueFamilies ); i++ ) {
        szQueueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        szQueueCreateInfos[i].queueFamilyIndex = szUniqueQueueFamilies[i];
        szQueueCreateInfos[i].queueCount = 1;
        szQueueCreateInfos[i].pQueuePriorities = &nQueuePriority;
    }

    vkEnumerateDeviceExtensionProperties( m_hPhysicalDevice, NULL, &nUsedExtensions, NULL );
    pExtensionList = (VkExtensionProperties *)alloca( sizeof( *pExtensionList ) * nUsedExtensions );
    memset( pExtensionList, 0, sizeof( *pExtensionList ) * nUsedExtensions );
    vkEnumerateDeviceExtensionProperties( m_hPhysicalDevice, NULL, &nUsedExtensions, pExtensionList );
    
    usedExtensions.reserve( szRequiredExtensions.size() );
    for ( i = 0; i < nUsedExtensions; i++ ) {
        if ( eastl::find( szRequiredExtensions.begin(), szRequiredExtensions.end(),
            pExtensionList[i].extensionName ) != szRequiredExtensions.end() )
        {
            m_GPUExtensionList.emplace_back( pExtensionList[i].extensionName );
            usedExtensions.emplace_back( pExtensionList[i].extensionName );
            SIRENGINE_LOG( "Using Vulkan Extension \"%s\"", pExtensionList[i].extensionName );
        }
    }

    VkValidationFeaturesEXT enabledFeatures;
    memset( &enabledFeatures, 0, sizeof( enabledFeatures ) );
    enabledFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    enabledFeatures.enabledValidationFeatureCount = SIREngine_StaticArrayLength( szEnabledValidationFeatures );
    enabledFeatures.pEnabledValidationFeatures = szEnabledValidationFeatures;

    VkPhysicalDevice16BitStorageFeaturesKHR deviceFeatures16Bit;
    memset( &deviceFeatures16Bit, 0, sizeof( deviceFeatures16Bit ) );
    deviceFeatures16Bit.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
    deviceFeatures16Bit.storageInputOutput16 = VK_FALSE;
    
    VkPhysicalDeviceFeatures2KHR deviceFeatures;
    memset( &deviceFeatures, 0, sizeof( deviceFeatures ) );
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    deviceFeatures.features.samplerAnisotropy = VK_TRUE;
    deviceFeatures.features.fullDrawIndexUint32 = VK_TRUE;
    deviceFeatures.features.shaderInt16 = VK_TRUE;
    deviceFeatures.pNext = &deviceFeatures16Bit;

    VkDeviceCreateInfo createInfo;
    memset( &createInfo, 0, sizeof( createInfo ) );
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = 1; // segfaults in glcore if set to 2
    createInfo.pQueueCreateInfos = szQueueCreateInfos;

    createInfo.pNext = &deviceFeatures;

    createInfo.enabledExtensionCount = usedExtensions.size();
    createInfo.ppEnabledExtensionNames = usedExtensions.data();

    createInfo.enabledLayerCount = SIREngine_StaticArrayLength( szLayers );
    createInfo.ppEnabledLayerNames = szLayers;

    assert( m_hPhysicalDevice );
    VK_CALL( vkCreateDevice( m_hPhysicalDevice, &createInfo, GetAllocationCallbacks(), &m_hDevice ) );

    vkGetDeviceQueue( m_hDevice, indices.nGraphicsFamily, 0, &m_hGraphicsQueue );
    vkGetDeviceQueue( m_hDevice, indices.nPresentFamily, 0, &m_hPresentQueue );

    GetGPUExtensionList();

    VmaAllocatorCreateInfo allocatorInfo;
    memset( &allocatorInfo, 0, sizeof( allocatorInfo ) );
    allocatorInfo.device = m_hDevice;
    allocatorInfo.instance = m_hInstance;
    allocatorInfo.physicalDevice = m_hPhysicalDevice;
    allocatorInfo.preferredLargeHeapBlockSize = 128ull * 1024 * 1024;
    {
        static VmaVulkanFunctions funcProcs;
        funcProcs.vkAllocateMemory =
            (PFN_vkAllocateMemory)vkGetDeviceProcAddr( m_hDevice, "vkAllocateMemory" );
        funcProcs.vkBindBufferMemory2KHR =
            (PFN_vkBindBufferMemory2KHR)vkGetDeviceProcAddr( m_hDevice, "vkBindBufferMemory2KHR" );
        funcProcs.vkBindImageMemory2KHR =
            (PFN_vkBindImageMemory2KHR)vkGetDeviceProcAddr( m_hDevice, "vkBindImageMemory2KHR" );
        funcProcs.vkGetBufferMemoryRequirements2KHR =
            (PFN_vkGetBufferMemoryRequirements2KHR)vkGetDeviceProcAddr( m_hDevice, "vkGetBufferMemoryRequirements2KHR" );
        funcProcs.vkGetImageMemoryRequirements2KHR =
            (PFN_vkGetImageMemoryRequirements2KHR)vkGetDeviceProcAddr( m_hDevice, "vkGetImageMemoryRequirements2KHR" );
        
        allocatorInfo.pVulkanFunctions = &funcProcs;
    }
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
    
    VK_CALL( vmaCreateAllocator( &allocatorInfo, &m_hAllocator ) );
    assert( m_hAllocator );
}

void VKContext::InitPhysicalVKDevice( void )
{
    uint32_t deviceCount, i;
    VkPhysicalDevice *deviceList;
    int *deviceScoreList, prevScore;
    int bestScore;

    deviceCount = 0;
    m_hPhysicalDevice = VK_NULL_HANDLE;

    VK_CALL( vkEnumeratePhysicalDevices( m_hInstance, &deviceCount, NULL ) );

    if ( deviceCount == 0 ) {
        SIRENGINE_WARNING( "vkEnumeratePhysicalDevices got 0" );
        return;
    }

    prevScore = 0;
    deviceScoreList = (int *)alloca( sizeof( *deviceScoreList ) * deviceCount );
    deviceList = (VkPhysicalDevice *)alloca( sizeof( *deviceList ) * deviceCount );
    VK_CALL( vkEnumeratePhysicalDevices( m_hInstance, &deviceCount, deviceList ) );

    memset( deviceScoreList, 0, sizeof( *deviceScoreList ) * deviceCount );

    for ( i = 0; i < deviceCount; i++ ) {
        deviceScoreList[i] = RateVKDevice( deviceList[i] );
        DumpPhysicalDeviceFeatures( deviceList[i] );
        if ( deviceScoreList[i] > prevScore ) {
            m_hPhysicalDevice = deviceList[i];
            bestScore = i;
        }
        prevScore = deviceScoreList[i];
    }

    if ( m_hPhysicalDevice == VK_NULL_HANDLE ) {
        SIRENGINE_WARNING( "Couldn't find a suitable VkPhysicalDevice." );
        return;
    }
    
    DumpPhysicalDeviceFeatures( m_hPhysicalDevice );
}

void VKContext::InitRenderPass( void )
{
    VkAttachmentDescription colorAttachment;
    memset( &colorAttachment, 0, sizeof( colorAttachment ) );
    colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;
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

    VkSubpassDescription subPass;
    memset( &subPass, 0, sizeof( subPass ) );
    subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPass.colorAttachmentCount = 1;
    subPass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo;
    memset( &renderPassInfo, 0, sizeof( renderPassInfo ) );
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subPass;

    VK_CALL( vkCreateRenderPass( m_hDevice, &renderPassInfo, GetAllocationCallbacks(), &m_hRenderPass ) );
    SIRENGINE_LOG( "Allocated VkRenderPass." );
}

void VKContext::CheckExtensionsSupported( void )
{
    uint32_t nExtensionCount;
    bool32 bValid;
    const char *szExtensionsList[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VK_CALL( vkEnumerateDeviceExtensionProperties( m_hPhysicalDevice, NULL, &nExtensionCount, NULL ) );
    
    m_Extensions.resize( nExtensionCount );
    VK_CALL( vkEnumerateDeviceExtensionProperties( m_hPhysicalDevice, NULL, &nExtensionCount, m_Extensions.data() ) );

    bValid = true;
    for ( const auto& it : szExtensionsList ) {
        for ( const auto& ext : m_Extensions ) {
            bool32 bFound = false;
            if ( SIREngine_stricmp( it, ext.extensionName ) == 0 ) {
                bFound = true;
                break;
            }
            if ( !bFound ) {
                bValid = false;
                break;
            }
        }
        if ( !bValid ) {
            break;
        }
    }

    if ( !bValid ) {

    }
}

void VKContext::CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline )
{
    VkCommandBufferBeginInfo beginInfo;
    memset( &beginInfo, 0, sizeof( beginInfo ) );
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // optional
    beginInfo.pInheritanceInfo = NULL;

    VK_CALL( vkBeginCommandBuffer( m_hCommandBuffers[ m_nCurrentFrameIndex ], &beginInfo ) );

    const VkClearValue clearColor = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    VKPipelineSet_t *pSetData = dynamic_cast<VKShaderPipeline *>( pShaderPipeline )->GetPipelineData();

    VkRenderPassBeginInfo passInfo;
    memset( &passInfo, 0, sizeof( passInfo ) );
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = m_hRenderPass;
    passInfo.framebuffer = m_SwapChainFramebuffers[ 0 ];
    passInfo.renderArea.offset = { 0, 0 };
    passInfo.renderArea.extent = m_nSwapChainExtent;
    vkCmdBeginRenderPass( m_hCommandBuffers[ m_nCurrentFrameIndex ], &passInfo, VK_SUBPASS_CONTENTS_INLINE );

    vkCmdBindPipeline( m_hCommandBuffers[ m_nCurrentFrameIndex ], VK_PIPELINE_BIND_POINT_GRAPHICS, m_hPipeline );

    VkViewport viewport;
    memset( &viewport, 0, sizeof( viewport ) );
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_nSwapChainExtent.width;
    viewport.height = (float)m_nSwapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport( m_hCommandBuffers[ m_nCurrentFrameIndex ], 0, 1, &viewport );

    VkRect2D scissor;
    memset( &scissor, 0, sizeof( scissor ) );
    scissor.offset = { 0, 0 };
    scissor.extent = m_nSwapChainExtent;
    vkCmdSetScissor( m_hCommandBuffers[ m_nCurrentFrameIndex ], 0, 1, &scissor );

    //
    // bind relevant data
    //

    vkCmdBindDescriptorSets( m_hCommandBuffers[ m_nCurrentFrameIndex ], VK_PIPELINE_BIND_POINT_GRAPHICS, pSetData->hLayout, 0,
        VK_MAX_FRAMES_IN_FLIGHT, pSetData->pDescriptorSets, 0, NULL );
    
    VkBuffer szVertexBuffers[] = {
        VK_NULL_HANDLE
    };
    VkDeviceSize szVertexBufferOffsets[] = {
        0
    };

    vkCmdBindVertexBuffers( m_hCommandBuffers[ m_nCurrentFrameIndex ], 0, SIREngine_StaticArrayLength( szVertexBuffers ), szVertexBuffers,
        szVertexBufferOffsets );
    vkCmdBindIndexBuffer( m_hCommandBuffers[ m_nCurrentFrameIndex ], VK_NULL_HANDLE, 0, VK_INDEX_TYPE_UINT32 );

    vkCmdDrawIndexed( m_hCommandBuffers[ m_nCurrentFrameIndex ], pShaderPipeline->GetIndexCount(), 1, 0, 0, 0 );

    vkCmdEndRenderPass( m_hCommandBuffers[ m_nCurrentFrameIndex ] );

    VK_CALL( vkEndCommandBuffer( m_hCommandBuffers[ m_nCurrentFrameIndex ] ) );
}

void VKContext::LockPipeline( void )
{
}

void VKContext::BeginFrame( void )
{
    VkResult nResult;

    VK_CALL( vkWaitForFences( m_hDevice, 1, &m_hInFlightFences[ m_nCurrentFrameIndex ], VK_TRUE, SIRENGINE_UINT64_MAX ) );

    nResult = vkAcquireNextImageKHR( m_hDevice, m_hSwapChain, SIRENGINE_UINT64_MAX, m_hSwapChainImageAvailable[ m_nCurrentFrameIndex ],
        VK_NULL_HANDLE, &m_nImageFrameIndex );
    if ( nResult == VK_ERROR_OUT_OF_DATE_KHR ) {
        RecreateSwapChain();
        return;
    } else if ( nResult != VK_SUCCESS && nResult != VK_SUBOPTIMAL_KHR ) {
        SIRENGINE_ERROR( "Error acquiring Vulkan swapchain image" );
    }
    
    VK_CALL( vkResetFences( m_hDevice, 1, &m_hInFlightFences[ m_nCurrentFrameIndex ] ) );

    VK_CALL( vkResetCommandBuffer( m_hCommandBuffers[ m_nCurrentFrameIndex ], 0 ) );
}

void VKContext::SwapBuffers( void )
{
    VkResult nResult;

    VkSubmitInfo submitInfo;
    memset( &submitInfo, 0, sizeof( submitInfo ) );
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore szWaitSemaphores[] = {
        m_hSwapChainImageAvailable[ m_nCurrentFrameIndex ]
    };
    VkPipelineStageFlags szWaitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo.waitSemaphoreCount = SIREngine_StaticArrayLength( szWaitSemaphores );
    submitInfo.pWaitSemaphores = szWaitSemaphores;
    submitInfo.pWaitDstStageMask = szWaitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_hCommandBuffers[ m_nCurrentFrameIndex ];

    VkSemaphore szSignalSemaphores[] = {
        m_hRenderFinished[ m_nCurrentFrameIndex ]
    };
    submitInfo.signalSemaphoreCount = SIREngine_StaticArrayLength( szSignalSemaphores );
    submitInfo.pSignalSemaphores = szSignalSemaphores;

    VK_CALL( vkQueueSubmit( m_hGraphicsQueue, 1, &submitInfo, m_hInFlightFences[ m_nCurrentFrameIndex ] ) );

    VkPresentInfoKHR presentInfo;
    memset( &presentInfo, 0, sizeof( presentInfo ) );
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = SIREngine_StaticArrayLength( szWaitSemaphores );
    presentInfo.pWaitSemaphores = szWaitSemaphores;

    VkSwapchainKHR szSwapChains[] = {
        m_hSwapChain
    };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = szSwapChains;

    presentInfo.pImageIndices = &m_nImageFrameIndex;

    nResult = vkQueuePresentKHR( m_hPresentQueue, &presentInfo );
    if ( nResult == VK_ERROR_OUT_OF_DATE_KHR || nResult == VK_SUBOPTIMAL_KHR /* || frameBufferResized */ ) {
        /* frameBufferResized = false; */
        RecreateSwapChain();
    } else if ( nResult != VK_SUCCESS ) {
        SIRENGINE_ERROR( "Error presenting Vulkan swapchain image" );
    }

    m_nCurrentFrameIndex = ( m_nCurrentFrameIndex + 1 ) % VK_MAX_FRAMES_IN_FLIGHT;
}

void *VKContext::Alloc( size_t nBytes, size_t nAlignment )
{
    void *p, *pPlusPointerSize, *pAligned;
    void **pStoredPtr;
    const size_t adjustedAlignment = ( nAlignment > sizeof( void * ) ) ? nAlignment : sizeof( void * );

	p = new char[ nBytes + adjustedAlignment + sizeof( void * ) ];
    pPlusPointerSize = (void *)( (uintptr_t)p + sizeof( void * ) );
	pAligned = (void *)( ( (uintptr_t)pPlusPointerSize + adjustedAlignment - 1 ) & ~( adjustedAlignment - 1 ) );
				
    pStoredPtr = (void **)pAligned - 1;
    assert( pStoredPtr >= p );
    *( pStoredPtr ) = p;

    assert( ( (size_t)pAligned & ~( nAlignment - 1 ) ) == (size_t)pAligned );

    return pAligned;
}

void VKContext::Free( void *pBuffer )
{
    if ( pBuffer != NULL ) {
        uintptr_t nOffset = *( ( (char *)pBuffer ) - 1 );
        delete[] ( (char *)pBuffer - nOffset );
	} else {
        SIRENGINE_WARNING( "VKContext::Free: NULL pointer" );
    }
}

VkFormat VKContext::GetSupportedFormat( const VkFormat *pFormats, uint64_t nFormatCount,
    VkImageTiling nTiling, VkFormatFeatureFlags nFeatures ) const
{
    uint64_t i;

    for ( i = 0; i < nFormatCount; i++ ) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties( m_hPhysicalDevice, pFormats[i], &props );

        if ( nTiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & nFeatures ) == nFeatures ) {
            return pFormats[i];
        } else if ( nTiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & nFeatures ) == nFeatures ) {
            return pFormats[i];
        }
    }
    
    SIRENGINE_WARNING( "Couldn't fetch a supported VkFormat" );
    
    return VK_FORMAT_UNDEFINED;
}

void VKContext::GetGPUExtensionList( void )
{
    SIRENGINE_LOG( "Fetching vkCreateSwapchainKHR..." );
    fn_vkCreateSwapchainKHR = vkCreateSwapchainKHR;
    if ( !fn_vkCreateSwapchainKHR ) {
        fn_vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)vkGetDeviceProcAddr(
            m_hDevice,
            "vkCreateSwapchainKHR"
        );
        if ( !fn_vkCreateSwapchainKHR ) {
            SIRENGINE_ERROR( "couldn't get vkCreateSwapchainKHR!" );
        }
    }

    SIRENGINE_LOG( "Fetching vkCmdPushDescriptorSetKHR..." );
    fn_vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(
        m_hDevice,
        "vkCmdPushDescriptorSetKHR"
    );
    if ( !fn_vkCmdPushDescriptorSetKHR  ) {
        SIRENGINE_ERROR( "couldn't get vkCmdPushDescriptorSetKHR!" );
    }

    SIRENGINE_LOG( "Fetching vkCreateDescriptorUpdateTemplateKHR..." );
    fn_vkCreateDescriptorUpdateTemplateKHR = (PFN_vkCreateDescriptorUpdateTemplateKHR)vkGetDeviceProcAddr(
        m_hDevice,
        "vkCreateDescriptorUpdateTemplateKHR"
    );
    if ( !fn_vkCreateDescriptorUpdateTemplateKHR  ) {
        SIRENGINE_ERROR( "couldn't get vkCreateDescriptorUpdateTemplateKHR!" );
    }

    {
        SIRENGINE_LOG( "Checking for VK_EXT_DEBUG_UTILS..." );

        VkDebugUtilsMessengerCreateInfoEXT debugInfo;
        memset( &debugInfo, 0, sizeof( debugInfo ) );
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = debugCallback;

        auto pFunction = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( m_hInstance, "vkCreateDebugUtilsMessengerEXT" );
        if ( pFunction ) {
            VK_CALL( pFunction( m_hInstance, &debugInfo, NULL, &m_hDebugHandler ) );
        } else {
            SIRENGINE_WARNING( "VK_DEBUG_UTILS_MESSENGER_KHR Extension not found." );
            return;
        }

        fn_vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_hInstance,
            "vkDestroyDebugUtilsMessengerEXT"
        );
        if ( !fn_vkDestroyDebugUtilsMessengerEXT ) {
            SIRENGINE_ERROR( "Failed to get Vulkan Instance Function proc \"vkDestroyDebugUtilsMessengerEXT\"" );
        }

        SIRENGINE_LOG( "Initialized VK_EXT_DEBUG_UTILS." );
    }
}

void VKContext::SetupShaderPipeline( void )
{
    m_hShaderPipeline = new VKShaderPipeline();

    RenderProgramInit_t programInfo;
    programInfo.nShaderPasses = 1;
    programInfo.pName = "Generic";
    m_pGenericShader = dynamic_cast<VKProgram *>( IRenderProgram::Create( programInfo ) );

    VertexInputDescription_t shaderInput;
    shaderInput.nEnabledVertexAttributes = AttribBit_Position | AttribBit_TexCoords | AttribBit_Color;
    shaderInput.nRenderPassCount = 1;
    shaderInput.pRenderPasses = NULL;
    shaderInput.nUniformCount = SIREngine_StaticArrayLength( szDefaultUniforms );
    shaderInput.pUniformList = szDefaultUniforms;
    shaderInput.pShader = m_pGenericShader;

    m_hShaderPipeline->AddVertexAttribSet( shaderInput );
}

const GPUMemoryUsage_t VKContext::GetMemoryUsage( void )
{
    GPUMemoryUsage_t memUsage;
    uint32_t i;
    
    memset( &memUsage, 0, sizeof( memUsage ) );

    VmaBudget memoryBudgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets( m_hAllocator, memoryBudgets );


    for ( i = 0; i < VK_MAX_MEMORY_HEAPS; i++ ) {
    }
}

static const char *GetVulkanMemoryFlagsString( VkMemoryPropertyFlags flags )
{
    static char szBuffer[1024];

    szBuffer[0] = '\0';
    if ( flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
        strncat( szBuffer, "(Device Local) ", sizeof( szBuffer ) - 1 );
    }
    if ( flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT ) {
        strncat( szBuffer, "(Host Cached) ", sizeof( szBuffer ) - 1 );
    }
    if ( flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) {
        strncat( szBuffer, "(Host Coherent) ", sizeof( szBuffer ) - 1 );
    }
    if ( flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) {
        strncat( szBuffer, "(Host Visible) ", sizeof( szBuffer ) - 1 );
    }
    
    return szBuffer;
}

void VKContext::PrintMemoryInfo( void ) const
{
    uint32_t i;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties( m_hPhysicalDevice, &memProperties );

    SIRENGINE_LOG( "Vulkan Memory Usage:" );
    for ( i = 0; i < memProperties.memoryTypeCount; i++ ) {
        const VkMemoryHeap *pHeapData = &memProperties.memoryHeaps[ memProperties.memoryTypes[i].heapIndex ];

        SIRENGINE_LOG( " Memory Heap %u:", i );
        SIRENGINE_LOG( " - Flags: %s", GetVulkanMemoryFlagsString( memProperties.memoryTypes[i].propertyFlags ) );
        SIRENGINE_LOG( " - Size: %s", SIREngine_GetMemoryString( pHeapData->size ) );
    }

    VmaTotalStatistics memDetails;
    vmaCalculateStatistics( m_hAllocator, &memDetails );

    SIRENGINE_LOG( "Vma (Vulkan Memory Allocator) Memory Usage:" );
    for ( i = 0; i < VK_MAX_MEMORY_HEAPS; i++ ) {
        if ( memDetails.memoryHeap[i].statistics.allocationCount == 0 ) {
            continue;
        }

        SIRENGINE_LOG( " Memory Heap %u:", i );
        SIRENGINE_LOG( " - Largest Allocation: %s",
            SIREngine_GetMemoryString( memDetails.memoryHeap[i].allocationSizeMax ) );
        SIRENGINE_LOG( " - Smallest Allocation: %s",
            SIREngine_GetMemoryString( memDetails.memoryHeap[i].allocationSizeMin ) );
        SIRENGINE_LOG( " - Unused Fragment Count: %u", memDetails.memoryHeap[i].unusedRangeCount );
        SIRENGINE_LOG( " - Unused Largest Fragment: %s",
            SIREngine_GetMemoryString( memDetails.memoryHeap[i].unusedRangeSizeMax ) );
        SIRENGINE_LOG( " - Unused Smallest Fragment: %s",
            SIREngine_GetMemoryString( memDetails.memoryHeap[i].unusedRangeSizeMin ) );
        SIRENGINE_LOG( " - Total Allocated CPU Memory: %s",
            SIREngine_GetMemoryString( memDetails.memoryHeap[i].statistics.allocationBytes ) );
        SIRENGINE_LOG( " - Alllocation Count: %u", memDetails.memoryHeap[i].statistics.allocationCount );
        SIRENGINE_LOG( " - Driver Allocated Used CPU Memory: %s",
            SIREngine_GetMemoryString( memDetails.memoryHeap[i].statistics.blockBytes ) );
        SIRENGINE_LOG( " - Driver Allocated Unused CPU Memory: %s",
            SIREngine_GetMemoryString( memDetails.memoryHeap[i].statistics.blockBytes - memDetails.memoryHeap[i].statistics.allocationBytes ) );
    }
}

IRenderProgram *VKContext::AllocateProgram( const RenderProgramInit_t& programInfo )
{
    return new VKProgram( programInfo );
}

IRenderShader *VKContext::AllocateShader( const RenderShaderInit_t& shaderInit )
{
    return new VKShader( shaderInit );
}

IRenderBuffer *VKContext::AllocateBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize )
{
    return new VKBuffer( nType, nUsage, nSize );
}

IRenderTexture *VKContext::AllocateTexture( const TextureInit_t& textureInfo )
{
    return new VKTexture( textureInfo );
}
