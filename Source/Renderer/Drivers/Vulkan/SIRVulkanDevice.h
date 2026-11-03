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

#ifndef __SIRENGINE_SIRVULKANDEVICE_H__
#define __SIRENGINE_SIRVULKANDEVICE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRDynamicLibrary.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

typedef enum sirVulkanResult_e : uint8_t
{
	SIR_VULKAN_SUCCESS = 0,
	SIR_VULKAN_INVALID_ARGUMENT,
	SIR_VULKAN_INVALID_STATE,
	SIR_VULKAN_LOADER_UNAVAILABLE,
	SIR_VULKAN_API_UNSUPPORTED,
	SIR_VULKAN_DEVICE_UNAVAILABLE,
	SIR_VULKAN_NATIVE_FAILURE,
	SIR_VULKAN_ALLOCATION_FAILED,
	SIR_VULKAN_BUSY,
	SIR_VULKAN_TIMEOUT
} sirVulkanResult_t;

typedef struct sirVulkanAdapterInfo_s
{
	uint32_t nApiVersion;
	uint32_t nDriverVersion;
	uint32_t nVendorId;
	uint32_t nDeviceId;
	uint32_t nQueueFamilyIndex;
	uint32_t nMemoryTypeCount;
	uint32_t nMemoryHeapCount;
	uint64_t nDeviceLocalHeapBytes;
	char szDeviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
} sirVulkanAdapterInfo_t;

typedef struct sirVulkanMemoryStats_s
{
	uint64_t nBlockBytes;
	uint64_t nAllocationBytes;
	uint64_t nEstimatedUsageBytes;
	uint64_t nEstimatedBudgetBytes;
	uint32_t nLiveBuffers;
} sirVulkanMemoryStats_t;

// Private Vulkan-driver ownership. No Vk or VMA types enter SIRRenderingDevice.
typedef struct sirVulkanCompletion_s
{
	uint64_t nDeviceSerial;
	uint64_t nValue;
} sirVulkanCompletion_t;

constexpr uint32_t SIR_VULKAN_TRANSFER_SLOT_COUNT = 8;

typedef struct sirVulkanTransferSlot_s
{
	VkCommandBuffer hCommandBuffer;
	uint64_t nLastValue;
} sirVulkanTransferSlot_t;

typedef struct sirVulkanBuffer_s
{
	VkBuffer hBuffer;
	VmaAllocation pAllocation;
	VmaAllocator pOwnerAllocator;
	VkDeviceSize nSize;
	VkBufferUsageFlags nUsage;
	uint64_t nDeviceSerial;
	uint64_t nLastGpuUse;
	bool bHostVisible;
} sirVulkanBuffer_t;

// First headless slice: one owner thread creates and retires the native device.
// Buffers must be retired before shutdown; queue submissions arrive in a later slice.
class SIRVulkanDevice
{
private:
	sirDynamicLibrary_t m_Library;
	PFN_vkGetInstanceProcAddr m_pfnGetInstanceProcAddr;
	PFN_vkGetDeviceProcAddr m_pfnGetDeviceProcAddr;
	PFN_vkDestroyInstance m_pfnDestroyInstance;
	PFN_vkEnumeratePhysicalDevices m_pfnEnumeratePhysicalDevices;
	PFN_vkGetPhysicalDeviceProperties m_pfnGetPhysicalDeviceProperties;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties m_pfnGetPhysicalDeviceQueueFamilyProperties;
	PFN_vkGetPhysicalDeviceMemoryProperties m_pfnGetPhysicalDeviceMemoryProperties;
	PFN_vkGetPhysicalDeviceFeatures2 m_pfnGetPhysicalDeviceFeatures2;
	PFN_vkCreateDevice m_pfnCreateDevice;
	PFN_vkDestroyDevice m_pfnDestroyDevice;
	PFN_vkGetDeviceQueue m_pfnGetDeviceQueue;
	PFN_vkDeviceWaitIdle m_pfnDeviceWaitIdle;
	PFN_vkCreateCommandPool m_pfnCreateCommandPool;
	PFN_vkDestroyCommandPool m_pfnDestroyCommandPool;
	PFN_vkAllocateCommandBuffers m_pfnAllocateCommandBuffers;
	PFN_vkResetCommandBuffer m_pfnResetCommandBuffer;
	PFN_vkBeginCommandBuffer m_pfnBeginCommandBuffer;
	PFN_vkEndCommandBuffer m_pfnEndCommandBuffer;
	PFN_vkCmdPipelineBarrier2 m_pfnCmdPipelineBarrier2;
	PFN_vkCmdCopyBuffer2 m_pfnCmdCopyBuffer2;
	PFN_vkQueueSubmit2 m_pfnQueueSubmit2;
	PFN_vkCreateSemaphore m_pfnCreateSemaphore;
	PFN_vkDestroySemaphore m_pfnDestroySemaphore;
	PFN_vkGetSemaphoreCounterValue m_pfnGetSemaphoreCounterValue;
	PFN_vkWaitSemaphores m_pfnWaitSemaphores;
	VkInstance m_Instance;
	VkPhysicalDevice m_PhysicalDevice;
	VkDevice m_Device;
	VkQueue m_GraphicsQueue;
	VmaAllocator m_Allocator;
	VkCommandPool m_TransferCommandPool;
	VkSemaphore m_TransferTimeline;
	sirVulkanTransferSlot_t m_TransferSlots[SIR_VULKAN_TRANSFER_SLOT_COUNT];
	uint64_t m_nDeviceSerial;
	uint64_t m_nLastTransferValue;
	uint32_t m_iNextTransferSlot;
	bool m_bDeviceLost;
	sirVulkanAdapterInfo_t m_Adapter;
	uint32_t m_nLiveBuffers;
	bool m_bReady;

	bool loadInstanceDispatch( void ) noexcept;
	bool chooseAdapter( void ) noexcept;
	bool initializeTransferQueue( void ) noexcept;
	void destroyTransferQueue( void ) noexcept;
	bool isBufferIdle( const sirVulkanBuffer_t *pBuffer ) const noexcept;

public:
	SIRVulkanDevice( void ) noexcept;
	~SIRVulkanDevice( void ) noexcept;

	SIRVulkanDevice( const SIRVulkanDevice & ) = delete;
	SIRVulkanDevice &operator=( const SIRVulkanDevice & ) = delete;

	sirVulkanResult_t initialize( void ) noexcept;
	bool shutdown( void ) noexcept;
	bool isReady( void ) const noexcept;
	bool queryAdapter( sirVulkanAdapterInfo_t *pAdapter ) const noexcept;
	bool queryMemoryStats( sirVulkanMemoryStats_t *pStats ) const noexcept;

	sirVulkanResult_t createBuffer( uint64_t nBytes, VkBufferUsageFlags nUsage, bool bHostVisible, sirVulkanBuffer_t *pBuffer ) noexcept;
	sirVulkanResult_t writeBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, const void *pData, uint64_t nBytes ) noexcept;
	sirVulkanResult_t destroyIdleBuffer( sirVulkanBuffer_t *pBuffer ) noexcept;
	sirVulkanResult_t readBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, void *pData, uint64_t nBytes ) noexcept;
	sirVulkanResult_t submitBufferCopy( sirVulkanBuffer_t *pSource, uint64_t nSourceOffset, sirVulkanBuffer_t *pDestination,
	                                    uint64_t nDestinationOffset, uint64_t nBytes, sirVulkanCompletion_t *pCompletion ) noexcept;
	sirVulkanResult_t queryCompletion( const sirVulkanCompletion_t *pCompletion, bool *pComplete ) const noexcept;
	sirVulkanResult_t waitCompletion( const sirVulkanCompletion_t *pCompletion, uint64_t nTimeoutNs ) const noexcept;
};

#endif // __SIRENGINE_SIRVULKANDEVICE_H__
