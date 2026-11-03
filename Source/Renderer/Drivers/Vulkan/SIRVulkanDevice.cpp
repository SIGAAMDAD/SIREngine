/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

#include <string.h>

SIRVulkanDevice::SIRVulkanDevice( void ) noexcept
    : m_Library{ nullptr }, m_pfnGetInstanceProcAddr( nullptr ), m_pfnGetDeviceProcAddr( nullptr ), m_pfnDestroyInstance( nullptr ),
      m_pfnEnumeratePhysicalDevices( nullptr ), m_pfnGetPhysicalDeviceProperties( nullptr ),
      m_pfnGetPhysicalDeviceQueueFamilyProperties( nullptr ), m_pfnGetPhysicalDeviceMemoryProperties( nullptr ),
      m_pfnCreateDevice( nullptr ), m_pfnDestroyDevice( nullptr ), m_pfnGetDeviceQueue( nullptr ), m_Instance( VK_NULL_HANDLE ),
      m_PhysicalDevice( VK_NULL_HANDLE ), m_Device( VK_NULL_HANDLE ), m_GraphicsQueue( VK_NULL_HANDLE ), m_Allocator( nullptr ),
      m_Adapter{}, m_nLiveBuffers( 0 ), m_bReady( false )
{
}

SIRVulkanDevice::~SIRVulkanDevice( void ) noexcept
{
	// The caller must destroy its buffers and wait for GPU users before shutdown.
	shutdown();
}

bool SIRVulkanDevice::loadInstanceDispatch( void ) noexcept
{
	m_pfnDestroyInstance = ( PFN_vkDestroyInstance )m_pfnGetInstanceProcAddr( m_Instance, "vkDestroyInstance" );
	m_pfnEnumeratePhysicalDevices = ( PFN_vkEnumeratePhysicalDevices )m_pfnGetInstanceProcAddr( m_Instance, "vkEnumeratePhysicalDevices" );
	m_pfnGetPhysicalDeviceProperties =
	    ( PFN_vkGetPhysicalDeviceProperties )m_pfnGetInstanceProcAddr( m_Instance, "vkGetPhysicalDeviceProperties" );
	m_pfnGetPhysicalDeviceQueueFamilyProperties =
	    ( PFN_vkGetPhysicalDeviceQueueFamilyProperties )m_pfnGetInstanceProcAddr( m_Instance, "vkGetPhysicalDeviceQueueFamilyProperties" );
	m_pfnGetPhysicalDeviceMemoryProperties =
	    ( PFN_vkGetPhysicalDeviceMemoryProperties )m_pfnGetInstanceProcAddr( m_Instance, "vkGetPhysicalDeviceMemoryProperties" );
	m_pfnCreateDevice = ( PFN_vkCreateDevice )m_pfnGetInstanceProcAddr( m_Instance, "vkCreateDevice" );
	m_pfnGetDeviceProcAddr = ( PFN_vkGetDeviceProcAddr )m_pfnGetInstanceProcAddr( m_Instance, "vkGetDeviceProcAddr" );

	return m_pfnDestroyInstance && m_pfnEnumeratePhysicalDevices && m_pfnGetPhysicalDeviceProperties &&
	       m_pfnGetPhysicalDeviceQueueFamilyProperties && m_pfnGetPhysicalDeviceMemoryProperties && m_pfnCreateDevice &&
	       m_pfnGetDeviceProcAddr;
}

bool SIRVulkanDevice::chooseAdapter( void ) noexcept
{
	uint32_t nDeviceCount = 0;
	if ( m_pfnEnumeratePhysicalDevices( m_Instance, &nDeviceCount, nullptr ) != VK_SUCCESS || nDeviceCount == 0 || nDeviceCount > 32 ) {
		return false;
	}

	VkPhysicalDevice hDevices[32] = {};
	if ( m_pfnEnumeratePhysicalDevices( m_Instance, &nDeviceCount, hDevices ) != VK_SUCCESS ) {
		return false;
	}

	uint32_t nBestScore = 0;
	for ( uint32_t iDevice = 0; iDevice < nDeviceCount; ++iDevice ) {
		VkPhysicalDeviceProperties properties = {};
		m_pfnGetPhysicalDeviceProperties( hDevices[iDevice], &properties );
		if ( properties.apiVersion < VK_API_VERSION_1_4 ) {
			continue;
		}

		uint32_t nFamilyCount = 0;
		m_pfnGetPhysicalDeviceQueueFamilyProperties( hDevices[iDevice], &nFamilyCount, nullptr );
		if ( nFamilyCount == 0 || nFamilyCount > 64 ) {
			continue;
		}
		VkQueueFamilyProperties families[64] = {};
		m_pfnGetPhysicalDeviceQueueFamilyProperties( hDevices[iDevice], &nFamilyCount, families );
		for ( uint32_t iFamily = 0; iFamily < nFamilyCount; ++iFamily ) {
			if ( families[iFamily].queueCount == 0 || ( families[iFamily].queueFlags & VK_QUEUE_GRAPHICS_BIT ) == 0 ) {
				continue;
			}
			const uint32_t nScore = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? 4
			                        : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 3
			                        : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU    ? 2
			                                                                                          : 1;
			if ( nScore <= nBestScore ) {
				break;
			}
			nBestScore = nScore;
			m_PhysicalDevice = hDevices[iDevice];
			m_Adapter.nApiVersion = properties.apiVersion;
			m_Adapter.nDriverVersion = properties.driverVersion;
			m_Adapter.nVendorId = properties.vendorID;
			m_Adapter.nDeviceId = properties.deviceID;
			m_Adapter.nQueueFamilyIndex = iFamily;
			memcpy( m_Adapter.szDeviceName, properties.deviceName, sizeof( m_Adapter.szDeviceName ) );
			break;
		}
	}

	if ( m_PhysicalDevice == VK_NULL_HANDLE ) {
		return false;
	}

	VkPhysicalDeviceMemoryProperties memory = {};
	m_pfnGetPhysicalDeviceMemoryProperties( m_PhysicalDevice, &memory );
	m_Adapter.nMemoryHeapCount = memory.memoryHeapCount;
	m_Adapter.nMemoryTypeCount = memory.memoryTypeCount;
	for ( uint32_t iHeap = 0; iHeap < memory.memoryHeapCount; ++iHeap ) {
		if ( ( memory.memoryHeaps[iHeap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) != 0 ) {
			m_Adapter.nDeviceLocalHeapBytes += memory.memoryHeaps[iHeap].size;
		}
	}
	return true;
}

sirVulkanResult_t SIRVulkanDevice::initialize( void ) noexcept
{
	if ( m_bReady || m_Library.pNativeHandle != nullptr ) {
		return SIR_VULKAN_INVALID_STATE;
	}

#if defined( _WIN32 )
	const char *szLoader = "vulkan-1.dll";
#else
	const char *szLoader = "libvulkan.so.1";
#endif
	if ( !SIRDynamicLibrary::load( szLoader, &m_Library ) ) {
		return SIR_VULKAN_LOADER_UNAVAILABLE;
	}

	m_pfnGetInstanceProcAddr = ( PFN_vkGetInstanceProcAddr )SIRDynamicLibrary::findSymbol( &m_Library, "vkGetInstanceProcAddr" );
	if ( m_pfnGetInstanceProcAddr == nullptr ) {
		shutdown();
		return SIR_VULKAN_LOADER_UNAVAILABLE;
	}

	PFN_vkEnumerateInstanceVersion pfnEnumerateInstanceVersion =
	    ( PFN_vkEnumerateInstanceVersion )m_pfnGetInstanceProcAddr( VK_NULL_HANDLE, "vkEnumerateInstanceVersion" );
	uint32_t nLoaderVersion = VK_API_VERSION_1_0;
	if ( pfnEnumerateInstanceVersion == nullptr || pfnEnumerateInstanceVersion( &nLoaderVersion ) != VK_SUCCESS ||
	     nLoaderVersion < VK_API_VERSION_1_4 ) {
		shutdown();
		return SIR_VULKAN_API_UNSUPPORTED;
	}

	PFN_vkCreateInstance pfnCreateInstance = ( PFN_vkCreateInstance )m_pfnGetInstanceProcAddr( VK_NULL_HANDLE, "vkCreateInstance" );
	if ( pfnCreateInstance == nullptr ) {
		shutdown();
		return SIR_VULKAN_LOADER_UNAVAILABLE;
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "SIREngine";
	appInfo.applicationVersion = VK_MAKE_API_VERSION( 0, 0, 1, 0 );
	appInfo.pEngineName = "SIREngine";
	appInfo.engineVersion = VK_MAKE_API_VERSION( 0, 0, 1, 0 );
	appInfo.apiVersion = VK_API_VERSION_1_4;

	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	if ( pfnCreateInstance( &instanceInfo, nullptr, &m_Instance ) != VK_SUCCESS ) {
		shutdown();
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	if ( !loadInstanceDispatch() ) {
		shutdown();
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	if ( !chooseAdapter() ) {
		shutdown();
		return SIR_VULKAN_DEVICE_UNAVAILABLE;
	}

	const float fPriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = m_Adapter.nQueueFamilyIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &fPriority;

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;
	if ( m_pfnCreateDevice( m_PhysicalDevice, &deviceInfo, nullptr, &m_Device ) != VK_SUCCESS ) {
		shutdown();
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	m_pfnDestroyDevice = ( PFN_vkDestroyDevice )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyDevice" );
	m_pfnGetDeviceQueue = ( PFN_vkGetDeviceQueue )m_pfnGetDeviceProcAddr( m_Device, "vkGetDeviceQueue" );
	if ( m_pfnDestroyDevice == nullptr || m_pfnGetDeviceQueue == nullptr ) {
		shutdown();
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	m_pfnGetDeviceQueue( m_Device, m_Adapter.nQueueFamilyIndex, 0, &m_GraphicsQueue );

	VmaVulkanFunctions functions = {};
	functions.vkGetInstanceProcAddr = m_pfnGetInstanceProcAddr;
	functions.vkGetDeviceProcAddr = m_pfnGetDeviceProcAddr;
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.instance = m_Instance;
	allocatorInfo.physicalDevice = m_PhysicalDevice;
	allocatorInfo.device = m_Device;
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
	allocatorInfo.pVulkanFunctions = &functions;
	if ( vmaCreateAllocator( &allocatorInfo, &m_Allocator ) != VK_SUCCESS ) {
		shutdown();
		return SIR_VULKAN_ALLOCATION_FAILED;
	}

	m_bReady = true;
	return SIR_VULKAN_SUCCESS;
}

bool SIRVulkanDevice::shutdown( void ) noexcept
{
	if ( m_nLiveBuffers != 0 ) {
		return false;
	}

	m_bReady = false;
	if ( m_Allocator != nullptr ) {
		vmaDestroyAllocator( m_Allocator );
		m_Allocator = nullptr;
	}
	if ( m_Device != VK_NULL_HANDLE && m_pfnDestroyDevice != nullptr ) {
		m_pfnDestroyDevice( m_Device, nullptr );
	}
	m_Device = VK_NULL_HANDLE;
	m_GraphicsQueue = VK_NULL_HANDLE;
	if ( m_Instance != VK_NULL_HANDLE && m_pfnDestroyInstance != nullptr ) {
		m_pfnDestroyInstance( m_Instance, nullptr );
	}
	m_Instance = VK_NULL_HANDLE;
	m_PhysicalDevice = VK_NULL_HANDLE;
	if ( m_Library.pNativeHandle != nullptr ) {
		SIRDynamicLibrary::unload( &m_Library );
	}
	memset( &m_Adapter, 0, sizeof( m_Adapter ) );
	return true;
}

bool SIRVulkanDevice::isReady( void ) const noexcept
{
	return m_bReady;
}

bool SIRVulkanDevice::queryAdapter( sirVulkanAdapterInfo_t *pAdapter ) const noexcept
{
	if ( !m_bReady || pAdapter == nullptr ) {
		return false;
	}
	*pAdapter = m_Adapter;
	return true;
}

bool SIRVulkanDevice::queryMemoryStats( sirVulkanMemoryStats_t *pStats ) const noexcept
{
	if ( !m_bReady || pStats == nullptr ) {
		return false;
	}
	VmaBudget budgets[VK_MAX_MEMORY_HEAPS] = {};
	vmaGetHeapBudgets( m_Allocator, budgets );
	memset( pStats, 0, sizeof( *pStats ) );

	for ( uint32_t iHeap = 0; iHeap < m_Adapter.nMemoryHeapCount; ++iHeap ) {
		pStats->nBlockBytes += budgets[iHeap].statistics.blockBytes;
		pStats->nAllocationBytes += budgets[iHeap].statistics.allocationBytes;
		pStats->nEstimatedUsageBytes += budgets[iHeap].usage;
		pStats->nEstimatedBudgetBytes += budgets[iHeap].budget;
	}
	pStats->nLiveBuffers = m_nLiveBuffers;
	return true;
}

sirVulkanResult_t SIRVulkanDevice::createBuffer( uint64_t nBytes, VkBufferUsageFlags nUsage, bool bHostVisible,
                                                 sirVulkanBuffer_t *pBuffer ) noexcept
{
	if ( !m_bReady ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || nBytes == 0 || nUsage == 0 || pBuffer->hBuffer != VK_NULL_HANDLE || pBuffer->pAllocation != nullptr ||
	     pBuffer->pOwnerAllocator != nullptr ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = ( VkDeviceSize )nBytes;
	bufferInfo.usage = nUsage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo allocationInfo = {};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
	if ( bHostVisible ) {
		allocationInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	}
	if ( vmaCreateBuffer( m_Allocator, &bufferInfo, &allocationInfo, &pBuffer->hBuffer, &pBuffer->pAllocation, nullptr ) != VK_SUCCESS ) {
		pBuffer->hBuffer = VK_NULL_HANDLE;
		pBuffer->pAllocation = nullptr;
		return SIR_VULKAN_ALLOCATION_FAILED;
	}

	pBuffer->pOwnerAllocator = m_Allocator;
	pBuffer->nSize = ( VkDeviceSize )nBytes;
	pBuffer->bHostVisible = bHostVisible;
	++m_nLiveBuffers;
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::writeBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, const void *pData, uint64_t nBytes ) noexcept
{
	if ( !m_bReady ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator || !pBuffer->bHostVisible ||
	     pData == nullptr || nBytes == 0 || nOffset > pBuffer->nSize || nBytes > pBuffer->nSize - nOffset ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	void *pMapped = nullptr;
	if ( vmaMapMemory( m_Allocator, pBuffer->pAllocation, &pMapped ) != VK_SUCCESS || pMapped == nullptr ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	memcpy( ( uint8_t * )pMapped + nOffset, pData, ( size_t )nBytes );
	const VkResult eFlush = vmaFlushAllocation( m_Allocator, pBuffer->pAllocation, nOffset, nBytes );
	vmaUnmapMemory( m_Allocator, pBuffer->pAllocation );
	return eFlush == VK_SUCCESS ? SIR_VULKAN_SUCCESS : SIR_VULKAN_NATIVE_FAILURE;
}

sirVulkanResult_t SIRVulkanDevice::destroyIdleBuffer( sirVulkanBuffer_t *pBuffer ) noexcept
{
	if ( !m_bReady ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pBuffer->hBuffer == VK_NULL_HANDLE || pBuffer->pAllocation == nullptr ||
	     pBuffer->pOwnerAllocator != m_Allocator || m_nLiveBuffers == 0 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	vmaDestroyBuffer( m_Allocator, pBuffer->hBuffer, pBuffer->pAllocation );
	memset( pBuffer, 0, sizeof( *pBuffer ) );
	--m_nLiveBuffers;
	return SIR_VULKAN_SUCCESS;
}
