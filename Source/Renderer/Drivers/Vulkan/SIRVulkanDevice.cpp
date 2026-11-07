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
#include <Core/Platform/SIRAtomic.h>

static SIRAtomic<uint64_t> s_nNextVulkanSerial( 1 );

SIRVulkanDevice::SIRVulkanDevice( void ) noexcept
    : m_Library{ nullptr }, m_pfnGetInstanceProcAddr( nullptr ), m_pfnGetDeviceProcAddr( nullptr ), m_pfnDestroyInstance( nullptr ),
      m_pfnEnumeratePhysicalDevices( nullptr ), m_pfnGetPhysicalDeviceProperties( nullptr ),
      m_pfnGetPhysicalDeviceQueueFamilyProperties( nullptr ), m_pfnGetPhysicalDeviceMemoryProperties( nullptr ),
      m_pfnGetPhysicalDeviceFeatures2( nullptr ), m_pfnCreateDevice( nullptr ), m_pfnDestroyDevice( nullptr ),
      m_pfnGetDeviceQueue( nullptr ), m_pfnDeviceWaitIdle( nullptr ), m_pfnCreateCommandPool( nullptr ), m_pfnDestroyCommandPool( nullptr ),
      m_pfnAllocateCommandBuffers( nullptr ), m_pfnResetCommandBuffer( nullptr ), m_pfnBeginCommandBuffer( nullptr ),
      m_pfnEndCommandBuffer( nullptr ), m_pfnCmdPipelineBarrier2( nullptr ), m_pfnCmdCopyBuffer2( nullptr ), m_pfnQueueSubmit2( nullptr ),
      m_pfnCreateSemaphore( nullptr ), m_pfnDestroySemaphore( nullptr ), m_pfnGetSemaphoreCounterValue( nullptr ),
      m_pfnWaitSemaphores( nullptr ), m_Instance( VK_NULL_HANDLE ), m_PhysicalDevice( VK_NULL_HANDLE ), m_Device( VK_NULL_HANDLE ),
      m_GraphicsQueue( VK_NULL_HANDLE ), m_ComputeQueue( VK_NULL_HANDLE ), m_TransferQueue( VK_NULL_HANDLE ), m_Allocator( nullptr ), m_TransferCommandPool( VK_NULL_HANDLE ),
      m_TransferTimeline( VK_NULL_HANDLE ), m_TransferSlots{}, m_nDeviceSerial( 0 ), m_nLastTransferValue( 0 ), m_iNextTransferSlot( 0 ),
      m_bDeviceLost( false ), m_Adapter{}, m_nLiveBuffers( 0 ), m_bReady( false )
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
	m_pfnGetPhysicalDeviceFeatures2 =
	    ( PFN_vkGetPhysicalDeviceFeatures2 )m_pfnGetInstanceProcAddr( m_Instance, "vkGetPhysicalDeviceFeatures2" );
	m_pfnGetDeviceProcAddr = ( PFN_vkGetDeviceProcAddr )m_pfnGetInstanceProcAddr( m_Instance, "vkGetDeviceProcAddr" );

	return m_pfnDestroyInstance && m_pfnEnumeratePhysicalDevices && m_pfnGetPhysicalDeviceProperties &&
	       m_pfnGetPhysicalDeviceQueueFamilyProperties && m_pfnGetPhysicalDeviceMemoryProperties && m_pfnCreateDevice &&
	       m_pfnGetDeviceProcAddr && m_pfnGetPhysicalDeviceFeatures2;
}

uint32_t sirVulkanAdapterPriority( VkPhysicalDeviceType eType ) noexcept
{
	switch ( eType ) {
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return 4;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return 3;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		return 2;
	default:
		return 1;
	}
}

bool sirVulkanSelectQueueFamilies( const VkQueueFamilyProperties *pFamilies, uint32_t nCount,
                                   sirVulkanQueueFamilySelection_t *pSelection ) noexcept
{
	if ( pSelection == nullptr ) {
		return false;
	}
	*pSelection = { UINT32_MAX, UINT32_MAX, UINT32_MAX, false };
	if ( pFamilies == nullptr || nCount == 0 || nCount > 64 ) {
		return false;
	}
	uint32_t nDedicatedCompute = UINT32_MAX;
	uint32_t nDedicatedTransfer = UINT32_MAX;
	uint32_t nTransferFallback = UINT32_MAX;
	for ( uint32_t iFamily = 0; iFamily < nCount; ++iFamily ) {
		const VkQueueFamilyProperties& family = pFamilies[iFamily];
		if ( family.queueCount == 0 ) {
			continue;
		}
		const VkQueueFlags nFlags = family.queueFlags;
		const bool bGraphics = ( nFlags & VK_QUEUE_GRAPHICS_BIT ) != 0;
		const bool bCompute = ( nFlags & VK_QUEUE_COMPUTE_BIT ) != 0;
		const bool bTransfer = ( nFlags & VK_QUEUE_TRANSFER_BIT ) != 0;
		if ( bGraphics && ( pSelection->nGraphicsFamilyIndex == UINT32_MAX || ( bCompute && !pSelection->bGraphicsSupportsCompute ) ) ) {
			pSelection->nGraphicsFamilyIndex = iFamily;
			pSelection->bGraphicsSupportsCompute = bCompute;
		}
		if ( bCompute && pSelection->nComputeFamilyIndex == UINT32_MAX ) {
			pSelection->nComputeFamilyIndex = iFamily;
		}
		if ( bCompute && !bGraphics && nDedicatedCompute == UINT32_MAX ) {
			nDedicatedCompute = iFamily;
		}
		if ( bTransfer && !bGraphics && !bCompute && nDedicatedTransfer == UINT32_MAX ) {
			nDedicatedTransfer = iFamily;
		}
		if ( ( bTransfer || bCompute || bGraphics ) && !bGraphics && nTransferFallback == UINT32_MAX ) {
			nTransferFallback = iFamily;
		}
	}
	if ( pSelection->nGraphicsFamilyIndex == UINT32_MAX ) {
		*pSelection = { UINT32_MAX, UINT32_MAX, UINT32_MAX, false };
		return false;
	}
	if ( nDedicatedCompute != UINT32_MAX ) {
		pSelection->nComputeFamilyIndex = nDedicatedCompute;
	}
	if ( pSelection->nComputeFamilyIndex == UINT32_MAX && pSelection->bGraphicsSupportsCompute ) {
		pSelection->nComputeFamilyIndex = pSelection->nGraphicsFamilyIndex;
	}
	pSelection->nTransferFamilyIndex = nDedicatedTransfer != UINT32_MAX
	                                       ? nDedicatedTransfer
	                                       : ( nTransferFallback != UINT32_MAX ? nTransferFallback : pSelection->nGraphicsFamilyIndex );
	return true;
}

bool sirVulkanBuildQueueCreateInfos( const sirVulkanQueueFamilySelection_t& selection, const float *pPriority,
                                     VkDeviceQueueCreateInfo *pInfos, uint32_t *pCount ) noexcept
{
	if ( pCount == nullptr ) {
		return false;
	}
	*pCount = 0;
	if ( pPriority == nullptr || pInfos == nullptr || selection.nGraphicsFamilyIndex == UINT32_MAX ) {
		return false;
	}
	const uint32_t szFamilies[3] = { selection.nGraphicsFamilyIndex, selection.nComputeFamilyIndex, selection.nTransferFamilyIndex };
	for ( uint32_t iFamily = 0; iFamily < 3; ++iFamily ) {
		const uint32_t nFamily = szFamilies[iFamily];
		if ( nFamily == UINT32_MAX ) {
			continue;
		}
		bool bAlreadyAdded = false;
		for ( uint32_t iExisting = 0; iExisting < *pCount; ++iExisting ) {
			if ( pInfos[iExisting].queueFamilyIndex == nFamily ) {
				bAlreadyAdded = true;
				break;
			}
		}
		if ( bAlreadyAdded ) {
			continue;
		}
		VkDeviceQueueCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueFamilyIndex = nFamily;
		info.queueCount = 1;
		info.pQueuePriorities = pPriority;
		pInfos[( *pCount )++] = info;
	}
	return true;
}

bool SIRVulkanDevice::chooseAdapter( uint32_t nAdapterIndex ) noexcept
{
	uint32_t nDeviceCount = 0;
	if ( m_pfnEnumeratePhysicalDevices( m_Instance, &nDeviceCount, nullptr ) != VK_SUCCESS || nDeviceCount == 0 || nDeviceCount > 32 ) {
		return false;
	}
	VkPhysicalDevice szDevices[32] = {};
	if ( m_pfnEnumeratePhysicalDevices( m_Instance, &nDeviceCount, szDevices ) != VK_SUCCESS ) {
		return false;
	}

	VkPhysicalDevice szEligible[32] = {};
	sirVulkanQueueFamilySelection_t szSelections[32] = {};
	uint32_t szPriorities[32] = {};
	uint32_t nEligibleCount = 0;
	for ( uint32_t iDevice = 0; iDevice < nDeviceCount; ++iDevice ) {
		VkPhysicalDeviceProperties properties = {};
		m_pfnGetPhysicalDeviceProperties( szDevices[iDevice], &properties );
		if ( properties.apiVersion < VK_API_VERSION_1_4 ) {
			continue;
		}
		VkPhysicalDeviceVulkan13Features features13 = {};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		VkPhysicalDeviceVulkan12Features features12 = {};
		features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features12.pNext = &features13;
		VkPhysicalDeviceFeatures2 features = {};
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &features12;
		m_pfnGetPhysicalDeviceFeatures2( szDevices[iDevice], &features );
		if ( features12.timelineSemaphore == VK_FALSE || features13.synchronization2 == VK_FALSE ) {
			continue;
		}
		uint32_t nFamilyCount = 0;
		m_pfnGetPhysicalDeviceQueueFamilyProperties( szDevices[iDevice], &nFamilyCount, nullptr );
		if ( nFamilyCount == 0 || nFamilyCount > 64 ) {
			continue;
		}
		VkQueueFamilyProperties szFamiliesInfo[64] = {};
		m_pfnGetPhysicalDeviceQueueFamilyProperties( szDevices[iDevice], &nFamilyCount, szFamiliesInfo );
		sirVulkanQueueFamilySelection_t selection = {};
		if ( !sirVulkanSelectQueueFamilies( szFamiliesInfo, nFamilyCount, &selection ) ) {
			continue;
		}

		const uint32_t nPriority = sirVulkanAdapterPriority( properties.deviceType );
		uint32_t iInsert = nEligibleCount;
		while ( iInsert > 0 && szPriorities[iInsert - 1] < nPriority ) {
			szEligible[iInsert] = szEligible[iInsert - 1];
			szSelections[iInsert] = szSelections[iInsert - 1];

			szPriorities[iInsert] = szPriorities[iInsert - 1];
			--iInsert;
		}
		szEligible[iInsert] = szDevices[iDevice];
		szSelections[iInsert] = selection;

		szPriorities[iInsert] = nPriority;
		++nEligibleCount;
	}
	m_nEligibleAdapterCount = nEligibleCount;
	if ( nAdapterIndex >= nEligibleCount ) {
		return false;
	}

	m_PhysicalDevice = szEligible[nAdapterIndex];
	VkPhysicalDeviceProperties selectedProperties = {};
	m_pfnGetPhysicalDeviceProperties( m_PhysicalDevice, &selectedProperties );
	m_Adapter.nApiVersion = selectedProperties.apiVersion;
	m_Adapter.nDriverVersion = selectedProperties.driverVersion;
	m_Adapter.nVendorId = selectedProperties.vendorID;
	m_Adapter.nDeviceId = selectedProperties.deviceID;
	m_Adapter.nQueueFamilyIndex = szSelections[nAdapterIndex].nGraphicsFamilyIndex;
	m_Adapter.nComputeQueueFamilyIndex = szSelections[nAdapterIndex].nComputeFamilyIndex;
	m_Adapter.nTransferQueueFamilyIndex = szSelections[nAdapterIndex].nTransferFamilyIndex;
	m_Adapter.nMaximumTextureDimension = selectedProperties.limits.maxImageDimension2D;
	m_Adapter.bComputeCapable = szSelections[nAdapterIndex].bGraphicsSupportsCompute;
	m_Adapter.bSoftwareAdapter = selectedProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
	memcpy( m_Adapter.szMaximumComputeGroupCount, selectedProperties.limits.maxComputeWorkGroupCount,
	        sizeof( m_Adapter.szMaximumComputeGroupCount ) );
	memcpy( m_Adapter.szDeviceName, selectedProperties.deviceName, sizeof( m_Adapter.szDeviceName ) );

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

sirVulkanResult_t SIRVulkanDevice::initializeInstance( void ) noexcept
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
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::probeAdapters( uint32_t nAdapterIndex, uint32_t *pCount, sirVulkanAdapterInfo_t *pAdapter ) noexcept
{
	if ( ( pCount == nullptr && pAdapter == nullptr ) || nAdapterIndex >= 32 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( m_bReady || m_Library.pNativeHandle != nullptr ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pCount != nullptr ) {
		*pCount = 0;
	}
	if ( pAdapter != nullptr ) {
		*pAdapter = {};
	}
	const sirVulkanResult_t eInstance = initializeInstance();
	if ( eInstance != SIR_VULKAN_SUCCESS ) {
		return eInstance;
	}
	const bool bSelected = chooseAdapter( pAdapter != nullptr ? nAdapterIndex : 0 );
	const uint32_t nEligible = m_nEligibleAdapterCount;
	if ( pCount != nullptr ) {
		*pCount = nEligible;
	}
	if ( pAdapter != nullptr && bSelected ) {
		*pAdapter = m_Adapter;
	}
	if ( !shutdown() ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	return bSelected ? SIR_VULKAN_SUCCESS : SIR_VULKAN_DEVICE_UNAVAILABLE;
}

sirVulkanResult_t SIRVulkanDevice::initialize( const sirVulkanPoolBudget_t *pBudget, uint32_t nAdapterIndex ) noexcept
{
	if ( m_bReady || m_Library.pNativeHandle != nullptr ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pBudget != nullptr && !sirVulkanValidatePoolBudget( pBudget ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const sirVulkanResult_t eInstance = initializeInstance();
	if ( eInstance != SIR_VULKAN_SUCCESS ) {
		return eInstance;
	}
	if ( !chooseAdapter( nAdapterIndex ) ) {
		shutdown();
		return SIR_VULKAN_DEVICE_UNAVAILABLE;
	}

	VkPhysicalDeviceVulkan13Features supported13 = {};
	supported13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	VkPhysicalDeviceVulkan12Features supported12 = {};
	supported12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	supported12.pNext = &supported13;
	VkPhysicalDeviceFeatures2 queriedFeatures = {};
	queriedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	queriedFeatures.pNext = &supported12;
	m_pfnGetPhysicalDeviceFeatures2( m_PhysicalDevice, &queriedFeatures );
	if ( supported12.timelineSemaphore == VK_FALSE || supported13.synchronization2 == VK_FALSE ) {
		shutdown();
		return SIR_VULKAN_API_UNSUPPORTED;
	}

	VkPhysicalDeviceVulkan13Features enabled13 = {};
	enabled13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	enabled13.synchronization2 = VK_TRUE;
	VkPhysicalDeviceVulkan12Features enabled12 = {};
	enabled12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	enabled12.timelineSemaphore = VK_TRUE;
	enabled12.pNext = &enabled13;

	const float fPriority = 1.0f;
	const sirVulkanQueueFamilySelection_t families = { m_Adapter.nQueueFamilyIndex, m_Adapter.nComputeQueueFamilyIndex,
	                                                   m_Adapter.nTransferQueueFamilyIndex, m_Adapter.bComputeCapable };
	VkDeviceQueueCreateInfo szQueueInfos[3] = {};
	uint32_t nQueueCount = 0;
	if ( !sirVulkanBuildQueueCreateInfos( families, &fPriority, szQueueInfos, &nQueueCount ) ) {
		shutdown();
		return SIR_VULKAN_DEVICE_UNAVAILABLE;
	}

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = nQueueCount;
	deviceInfo.pQueueCreateInfos = szQueueInfos;
	deviceInfo.pNext = &enabled12;
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
	if ( m_Adapter.nComputeQueueFamilyIndex != UINT32_MAX ) {
		m_pfnGetDeviceQueue( m_Device, m_Adapter.nComputeQueueFamilyIndex, 0, &m_ComputeQueue );
	}
	if ( m_Adapter.nTransferQueueFamilyIndex != UINT32_MAX ) {
		m_pfnGetDeviceQueue( m_Device, m_Adapter.nTransferQueueFamilyIndex, 0, &m_TransferQueue );
	}
	if ( m_GraphicsQueue == VK_NULL_HANDLE ||
	     ( m_Adapter.nComputeQueueFamilyIndex != UINT32_MAX && m_ComputeQueue == VK_NULL_HANDLE ) ||
	     ( m_Adapter.nTransferQueueFamilyIndex != UINT32_MAX && m_TransferQueue == VK_NULL_HANDLE ) ) {
		shutdown();
		return SIR_VULKAN_NATIVE_FAILURE;
	}

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
	if ( pBudget != nullptr ) {
		const sirVulkanResult_t ePools = initializeSealedPools( pBudget );
		if ( ePools != SIR_VULKAN_SUCCESS ) {
			shutdown();
			return ePools;
		}
	}

	if ( !initializeTransferQueue() || !loadImageDispatch() ) {
		shutdown();
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	m_nDeviceSerial = s_nNextVulkanSerial.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
	m_bReady = true;
	return SIR_VULKAN_SUCCESS;
}

bool SIRVulkanDevice::shutdown( void ) noexcept
{
	if ( m_nLiveBuffers != 0 || m_nLiveImages != 0 || m_nLiveImageViews != 0 || m_nLiveSamplers != 0 || m_nLivePipelines != 0 ) {
		return false;
	}

	m_bReady = false;
	bool bIdle = true;
	if ( m_Device != VK_NULL_HANDLE && m_pfnDeviceWaitIdle != nullptr ) {
		bIdle = m_pfnDeviceWaitIdle( m_Device ) == VK_SUCCESS;
	}
	destroyTransferQueue();
	destroySealedPools();
	if ( m_Allocator != nullptr ) {
		vmaDestroyAllocator( m_Allocator );
		m_Allocator = nullptr;
	}
	if ( m_Device != VK_NULL_HANDLE && m_pfnDestroyDevice != nullptr ) {
		m_pfnDestroyDevice( m_Device, nullptr );
	}
	m_Device = VK_NULL_HANDLE;
	m_GraphicsQueue = VK_NULL_HANDLE;
	m_ComputeQueue = VK_NULL_HANDLE;
	m_TransferQueue = VK_NULL_HANDLE;
	if ( m_Instance != VK_NULL_HANDLE && m_pfnDestroyInstance != nullptr ) {
		m_pfnDestroyInstance( m_Instance, nullptr );
	}
	m_Instance = VK_NULL_HANDLE;
	m_PhysicalDevice = VK_NULL_HANDLE;
	if ( m_Library.pNativeHandle != nullptr ) {
		SIRDynamicLibrary::unload( &m_Library );
	}
	memset( &m_Adapter, 0, sizeof( m_Adapter ) );
	m_nDeviceSerial = 0;
	m_nLastTransferValue = 0;
	m_nEligibleAdapterCount = 0;
	m_nPeakCommittedBytes = 0;
	m_nPeakAllocationBytes = 0;
	m_nUploadMappedBytes = 0;
	m_nReadbackMappedBytes = 0;
	m_bDeviceLost = false;
	return bIdle;
}

bool SIRVulkanDevice::isReady( void ) const noexcept
{
	return m_bReady;
}

uint32_t SIRVulkanDevice::getAdapterCount( void ) const noexcept
{
	return m_bReady ? m_nEligibleAdapterCount : 0;
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
	pStats->nSealedBudgetBytes = m_nSealedBudgetBytes;
	pStats->nPeakCommittedBytes = m_nPeakCommittedBytes;
	pStats->nPeakAllocationBytes = m_nPeakAllocationBytes;
	pStats->nUploadMappedBytes = m_nUploadMappedBytes;
	pStats->nReadbackMappedBytes = m_nReadbackMappedBytes;
	pStats->nLiveBuffers = m_nLiveBuffers;
	pStats->nLiveImages = m_nLiveImages;
	pStats->nLiveImageViews = m_nLiveImageViews;
	pStats->nLiveSamplers = m_nLiveSamplers;
	return true;
}

sirVulkanResult_t SIRVulkanDevice::createBuffer( uint64_t nBytes, VkBufferUsageFlags nUsage, bool bHostVisible,
                                                 sirVulkanBuffer_t *pBuffer ) noexcept
{
	const sirVulkanBufferPlacement_t ePlacement =
	    !bHostVisible ? SIR_VULKAN_BUFFER_GPU_LOCAL
		: ( ( nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) != 0 && ( nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 )
	        ? SIR_VULKAN_BUFFER_CPU_READBACK
	        : SIR_VULKAN_BUFFER_CPU_UPLOAD;
	return createBuffer( nBytes, nUsage, ePlacement, pBuffer );
}

sirVulkanResult_t SIRVulkanDevice::createBuffer( uint64_t nBytes, VkBufferUsageFlags nUsage, sirVulkanBufferPlacement_t ePlacement,
                                                 sirVulkanBuffer_t *pBuffer ) noexcept
{
	if ( !m_bReady ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || nBytes == 0 || nUsage == 0 || pBuffer->hBuffer != VK_NULL_HANDLE || pBuffer->pAllocation != nullptr ||
	     pBuffer->pOwnerAllocator != nullptr || ePlacement > SIR_VULKAN_BUFFER_GPU_UPLOAD ) {
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
	const bool bHostVisible = sirVulkanPlacementHostVisible( ePlacement );
	if ( m_bSealedPools ) {
		allocationInfo.pool = m_szSealedPools[sirVulkanPlacementPool( ePlacement )];
		allocationInfo.flags |= VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT;
	}
	if ( bHostVisible ) {
		allocationInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
		if ( ePlacement == SIR_VULKAN_BUFFER_CPU_READBACK ) {
			allocationInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
			allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		} else {
			allocationInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		}
		if ( ePlacement == SIR_VULKAN_BUFFER_GPU_UPLOAD ) {
			allocationInfo.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}
		allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	} else {
		allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}
	if ( m_bSealedPools && !isSealedBufferMemoryType( bufferInfo, sirVulkanPlacementPool( ePlacement ) ) ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}
	VmaAllocationInfo nativeAllocation = {};
	if ( vmaCreateBuffer( m_Allocator, &bufferInfo, &allocationInfo, &pBuffer->hBuffer, &pBuffer->pAllocation, &nativeAllocation ) !=
	     VK_SUCCESS ) {
		pBuffer->hBuffer = VK_NULL_HANDLE;
		pBuffer->pAllocation = nullptr;
		return SIR_VULKAN_ALLOCATION_FAILED;
	}

	if ( bHostVisible && nativeAllocation.pMappedData == nullptr ) {
		vmaDestroyBuffer( m_Allocator, pBuffer->hBuffer, pBuffer->pAllocation );
		*pBuffer = {};
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	pBuffer->pMappedData = nativeAllocation.pMappedData;
	pBuffer->nAllocationBytes = nativeAllocation.size;
	pBuffer->ePlacement = ePlacement;
	pBuffer->pOwnerAllocator = m_Allocator;
	pBuffer->nSize = ( VkDeviceSize )nBytes;
	pBuffer->nUsage = nUsage;
	pBuffer->nDeviceSerial = m_nDeviceSerial;
	pBuffer->nLastGpuUse = 0;
	pBuffer->bHostVisible = bHostVisible;
	if ( bHostVisible ) {
		if ( ePlacement == SIR_VULKAN_BUFFER_CPU_READBACK ) {
			m_nReadbackMappedBytes += pBuffer->nAllocationBytes;
		} else {
			m_nUploadMappedBytes += pBuffer->nAllocationBytes;
		}
	}
	++m_nLiveBuffers;
	updateMemoryHighWater();
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::writeBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, const void *pData, uint64_t nBytes ) noexcept
{
	if ( !m_bReady ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator || !pBuffer->bHostVisible ||
	     pData == nullptr || nBytes == 0 || nOffset > pBuffer->nSize || nBytes > pBuffer->nSize - nOffset ||
	     pBuffer->nDeviceSerial != m_nDeviceSerial ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !isBufferIdle( pBuffer ) ) {
		return SIR_VULKAN_BUSY;
	}
	if ( pBuffer->pMappedData == nullptr ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	memcpy( ( uint8_t * )pBuffer->pMappedData + nOffset, pData, ( size_t )nBytes );
	const VkResult eFlush = vmaFlushAllocation( m_Allocator, pBuffer->pAllocation, nOffset, nBytes );
	return eFlush == VK_SUCCESS ? SIR_VULKAN_SUCCESS : SIR_VULKAN_NATIVE_FAILURE;
}

sirVulkanResult_t SIRVulkanDevice::destroyIdleBuffer( sirVulkanBuffer_t *pBuffer ) noexcept
{
	if ( !m_bReady ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pBuffer->hBuffer == VK_NULL_HANDLE || pBuffer->pAllocation == nullptr ||
	     pBuffer->pOwnerAllocator != m_Allocator || m_nLiveBuffers == 0 || pBuffer->nDeviceSerial != m_nDeviceSerial ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !isBufferIdle( pBuffer ) ) {
		return SIR_VULKAN_BUSY;
	}
	if ( pBuffer->bHostVisible ) {
		if ( pBuffer->ePlacement == SIR_VULKAN_BUFFER_CPU_READBACK ) {
			m_nReadbackMappedBytes -= pBuffer->nAllocationBytes;
		} else {
			m_nUploadMappedBytes -= pBuffer->nAllocationBytes;
		}
	}
	vmaDestroyBuffer( m_Allocator, pBuffer->hBuffer, pBuffer->pAllocation );
	memset( pBuffer, 0, sizeof( *pBuffer ) );
	--m_nLiveBuffers;
	return SIR_VULKAN_SUCCESS;
}
