#include "VKContext.hpp"

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
        score += 1000;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        // most likely an intel GPU
        score += 700;
        break;
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

    // add some bonues points for extra functionality with vertex array attributes
    score += deviceProperties.limits.maxVertexInputAttributeOffset;
    score += deviceProperties.limits.maxVertexInputAttributes;
    score += deviceProperties.limits.maxVertexInputBindings;
    score += deviceProperties.limits.maxVertexInputBindingStride;

    score += deviceProperties.limits.maxStorageBufferRange;

    // 16-bit shader input required for colors, TODO: make it so that this is
    // not a required device feature
    if ( !deviceFeatures.shaderInt16 ) {
        score = 0;
    }
    if ( !deviceFeatures.fullDrawIndexUint32 ) {
        score = 0;
    }

    return score;
}

VKContext::VKContext( const ApplicationInfo_t& appInfo )
    : IRenderContext( appInfo )
{
    InitWindowInstance();
}

VKContext::~VKContext()
{
    if ( m_hSurface != NULL ) {
        vkDestroySurfaceKHR( m_hInstance, m_hSurface, NULL );
    }
    if ( m_hInstance != NULL ) {
        vkDestroyInstance( m_hInstance, NULL );
    }
}

void VKContext::InitWindowInstance( void )
{
    uint32_t extensionCount;
    VkExtensionProperties *extensionList;

    VkApplicationInfo appInfo;
    memset( &appInfo, 0, sizeof( appInfo ) );
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_AppInfo.pszAppName; // user defined window name
    appInfo.applicationVersion = m_AppInfo.nAppVersion;
    appInfo.pEngineName = SIRENGINE_VERSION_STRING;
    appInfo.engineVersion = SIRENGINE_VERSION;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo;
    memset( &createInfo, 0, sizeof( createInfo ) );
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if ( !SDL_Vulkan_GetInstanceExtensions( m_pWindow, &m_nGPUExtensionCount, m_pGPUExtensionList ) ) {

    }

    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    createInfo.enabledExtensionCount = m_nGPUExtensionCount;
    createInfo.ppEnabledExtensionNames = m_pGPUExtensionList;

    {
        extensionCount = 0;
        extensionList = NULL;
        vkEnumerateInstanceExtensionProperties( NULL, &extensionCount, NULL );

        extensionList = (VkExtensionProperties *)alloca( sizeof( *extensionList ) * extensionCount );
        vkEnumerateInstanceExtensionProperties( NULL, &extensionCount, extensionList );
    }

    if ( vkCreateInstance( &createInfo, NULL, &m_hInstance ) == VK_SUCCESS ) {

    }

    if ( !SDL_Vulkan_CreateSurface( m_pWindow, m_hInstance, &m_hSurface ) ) {

    }
}

void VKContext::InitPhysicalVKDevice( void )
{
    uint32_t deviceCount, i;
    VkPhysicalDevice *deviceList;
    int *deviceScoreList, prevScore;

    deviceCount = 0;
    m_hPhysicalDevice = VK_NULL_HANDLE;

    vkEnumeratePhysicalDevices( m_hInstance, &deviceCount, NULL );

    if ( deviceCount == 0 ) {

    }

    prevScore = 0;
    deviceScoreList = (int *)alloca( sizeof( *deviceScoreList ) * deviceCount );
    deviceList = (VkPhysicalDevice *)alloca( sizeof( *deviceList ) * deviceCount );
    vkEnumeratePhysicalDevices( m_hInstance, &deviceCount, deviceList );
    for ( i = 0; i < deviceCount; i++ ) {
        deviceScoreList[i] = RateVKDevice( deviceList[i] );
        if ( deviceScoreList[i] > prevScore ) {
            m_hPhysicalDevice = deviceList[i];
        }
        prevScore = deviceScoreList[i];
    }

    if ( m_hPhysicalDevice == VK_NULL_HANDLE ) {

    }
}
