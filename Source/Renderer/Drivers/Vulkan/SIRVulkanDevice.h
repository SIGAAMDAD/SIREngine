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
	uint32_t nComputeQueueFamilyIndex;
	uint32_t nTransferQueueFamilyIndex;
	uint32_t nMaximumTextureDimension;
	uint32_t nMemoryTypeCount;
	uint32_t nMemoryHeapCount;
	uint32_t szMaximumComputeGroupCount[3];
	bool bComputeCapable;
	bool bSoftwareAdapter;
	uint64_t nDeviceLocalHeapBytes;
	char szDeviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
} sirVulkanAdapterInfo_t;

typedef struct sirVulkanMemoryStats_s
{
	uint64_t nBlockBytes;
	uint64_t nAllocationBytes;
	uint64_t nEstimatedUsageBytes;
	uint64_t nEstimatedBudgetBytes;
	uint64_t nSealedBudgetBytes;
	uint64_t nPeakCommittedBytes;
	uint64_t nPeakAllocationBytes;
	uint64_t nUploadMappedBytes;
	uint64_t nReadbackMappedBytes;
	uint32_t nLiveBuffers;
	uint32_t nLiveImages;
	uint32_t nLiveImageViews;
	uint32_t nLiveSamplers;
} sirVulkanMemoryStats_t;

// Higher scores appear first in the shared adapter registry.
uint32_t sirVulkanAdapterPriority( VkPhysicalDeviceType eType ) noexcept;

// Discovery only: current command submission uses the selected graphics family.
typedef struct sirVulkanQueueFamilySelection_s
{
	uint32_t nGraphicsFamilyIndex;
	uint32_t nComputeFamilyIndex;
	uint32_t nTransferFamilyIndex;
	bool bGraphicsSupportsCompute;
} sirVulkanQueueFamilySelection_t;

bool sirVulkanSelectQueueFamilies( const VkQueueFamilyProperties *pFamilies, uint32_t nCount,
                                   sirVulkanQueueFamilySelection_t *pSelection ) noexcept;

// Private Vulkan-driver ownership. No Vk or VMA types enter SIRRenderingDevice.
typedef struct sirVulkanPoolBudget_s
{
	uint64_t nResidentBytes;
	uint64_t nUploadBytes;
	uint64_t nReadbackBytes;
} sirVulkanPoolBudget_t;

// A non-null budget must specify three finite, block-aligned pools.
bool sirVulkanValidatePoolBudget( const sirVulkanPoolBudget_t *pBudget ) noexcept;

typedef struct sirVulkanCompletion_s
{
	uint64_t nDeviceSerial;
	uint64_t nValue;
} sirVulkanCompletion_t;

constexpr uint32_t SIR_VULKAN_TRANSFER_SLOT_COUNT = 8;
constexpr uint32_t SIR_VULKAN_COPY_BATCH_CAPACITY = 8;
constexpr uint32_t SIR_VULKAN_MAX_TRACKED_SUBRESOURCES = 4096;

typedef struct sirVulkanTransferSlot_s
{
	VkCommandBuffer hCommandBuffer;
	uint64_t nLastValue;
} sirVulkanTransferSlot_t;

typedef struct sirVulkanComputePipeline_s
{
	VkPipeline hPipeline;
	VkPipelineLayout hLayout;
	uint64_t nDeviceSerial;
	uint64_t nLastGpuUse;
} sirVulkanComputePipeline_t;

// Validates the Vulkan/SPIR-V binary envelope before handing bytecode to
// the driver. Full semantic validation remains the shader cooker's job.
bool sirVulkanValidateSpirv( const void *pBytes, uint64_t nByteCount, const char *pszEntryPoint ) noexcept;

typedef enum sirVulkanBufferPlacement_e : uint8_t
{
	SIR_VULKAN_BUFFER_GPU_LOCAL = 0,
	SIR_VULKAN_BUFFER_CPU_UPLOAD,
	SIR_VULKAN_BUFFER_CPU_READBACK,
	SIR_VULKAN_BUFFER_GPU_UPLOAD
} sirVulkanBufferPlacement_t;

bool sirVulkanPlacementHostVisible( sirVulkanBufferPlacement_t ePlacement ) noexcept;
uint32_t sirVulkanPlacementPool( sirVulkanBufferPlacement_t ePlacement ) noexcept;
bool sirVulkanMemoryTypeCompatible( uint32_t nPoolMemoryType, uint32_t nResourceMemoryTypes ) noexcept;
void sirVulkanAdvanceMemoryPeak( uint64_t nCommittedBytes, uint64_t nAllocatedBytes, uint64_t *pPeakCommitted,
                                 uint64_t *pPeakAllocated ) noexcept;

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
	void *pMappedData;
	uint64_t nAllocationBytes;
	sirVulkanBufferPlacement_t ePlacement;
} sirVulkanBuffer_t;

typedef struct sirVulkanBufferCopy_s
{
	sirVulkanBuffer_t *pSource;
	sirVulkanBuffer_t *pDestination;
	uint64_t nSourceOffset;
	uint64_t nDestinationOffset;
	uint64_t nBytes;
} sirVulkanBufferCopy_t;

// Pure validation, independent of the Vulkan loader and physical device.
sirVulkanResult_t sirVulkanValidateBufferCopies( const sirVulkanBufferCopy_t *pCopies, uint32_t nCount ) noexcept;

typedef struct sirVulkanImage_s
{
	VkImage hImage;
	VmaAllocation pAllocation;
	VmaAllocator pOwnerAllocator;
	uint64_t nDeviceSerial;
	VkFormat eFormat;
	VkImageType eType;
	uint64_t nLastGpuUse;
	VkImageUsageFlags nUsage;
	uint32_t nMips;
	uint32_t nLayers;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nDepth;
	uint64_t szUploadedSubresources[SIR_VULKAN_MAX_TRACKED_SUBRESOURCES / 64];
	// Four bits per mip/layer: persistent color layouts including transfer source/destination.
	uint8_t szSubresourceLayouts[SIR_VULKAN_MAX_TRACKED_SUBRESOURCES / 2];
	bool bCube;
} sirVulkanImage_t;

typedef struct sirVulkanImageView_s
{
	VkImageView hView;
	VkImage hImage;
	uint64_t nDeviceSerial;
} sirVulkanImageView_t;

typedef struct sirVulkanSampler_s
{
	VkSampler hSampler;
	uint64_t nDeviceSerial;
} sirVulkanSampler_t;

typedef struct sirVulkanFormatBlock_s
{
	uint32_t nBlockWidth;
	uint32_t nBlockHeight;
	uint32_t nBytesPerBlock;
} sirVulkanFormatBlock_t;

// The transfer validator has no Vulkan device dependency and is exercised
// by CPU-only tests on hosts without Vulkan 1.4 hardware.
bool sirVulkanGetFormatBlock( VkFormat eFormat, sirVulkanFormatBlock_t *pBlock ) noexcept;
bool sirVulkanGetSubresourceLayout( const sirVulkanImage_t *pImage, uint32_t nMip, uint32_t nLayer, VkImageLayout *pLayout ) noexcept;
bool sirVulkanSetSubresourceLayout( sirVulkanImage_t *pImage, uint32_t nMip, uint32_t nLayer, VkImageLayout eLayout ) noexcept;
VkImageAspectFlags sirVulkanImageAspectMask( VkFormat eFormat ) noexcept;

typedef struct sirVulkanBufferBarrier_s
{
	VkPipelineStageFlags2 nSourceStages;
	VkPipelineStageFlags2 nDestinationStages;
	VkAccessFlags2 nSourceAccess;
	VkAccessFlags2 nDestinationAccess;
	bool bUav;
} sirVulkanBufferBarrier_t;

typedef struct sirVulkanImageBarrier_s
{
	VkImageLayout eBefore;
	VkImageLayout eAfter;
	uint32_t nMip;
	uint32_t nLayer;
	uint32_t nMipCount;
	uint32_t nLayerCount;
	bool bUav;
} sirVulkanImageBarrier_t;

// Identical, failure-atomic range/layout/usage admission for standalone and recorded barriers.
bool sirVulkanValidateNativeImageBarrier( const sirVulkanImage_t *pImage, const sirVulkanImageBarrier_t& barrier ) noexcept;
// CPU-only translation used by native image barriers and copy transitions.
bool sirVulkanImageAccessScope( VkImageLayout eLayout, VkPipelineStageFlags2 *pStages, VkAccessFlags2 *pAccess ) noexcept;

typedef struct sirVulkanTextureTransfer_s
{
	uint64_t nBufferOffset;
	uint32_t nBufferRowLength;
	uint32_t nBufferImageHeight;
	uint32_t nMip;
	uint32_t nLayer;
	VkOffset3D offset;
	VkExtent3D extent;
	bool bBufferToImage;
} sirVulkanTextureTransfer_t;

typedef enum sirVulkanRecordedKind_e : uint8_t
{
	SIR_VULKAN_RECORDED_BUFFER_COPY = 0,
	SIR_VULKAN_RECORDED_TEXTURE_COPY,
	SIR_VULKAN_RECORDED_IMAGE_BARRIER,
	SIR_VULKAN_RECORDED_BUFFER_BARRIER,
	SIR_VULKAN_RECORDED_COMPUTE_DISPATCH
} sirVulkanRecordedKind_t;

typedef struct sirVulkanRecordedCommand_s
{
	sirVulkanRecordedKind_t eKind;
	sirVulkanBufferCopy_t bufferCopy;
	sirVulkanBufferBarrier_t bufferBarrier;
	sirVulkanBuffer_t *pBuffer;
	sirVulkanImage_t *pImage;
	sirVulkanTextureTransfer_t textureCopy;
	sirVulkanImageBarrier_t imageBarrier;
	sirVulkanComputePipeline_t *pPipeline;
	uint32_t nGroupCountX;
	uint32_t nGroupCountY;
	uint32_t nGroupCountZ;
} sirVulkanRecordedCommand_t;

// Pure ordered preflight: never mutates caller-owned VkImage state.
sirVulkanResult_t sirVulkanValidateRecordedCommands( const sirVulkanRecordedCommand_t *pCommands, uint32_t nCount ) noexcept;
// Whether this command needs a transfer-stage memory dependency on an earlier buffer access.
bool sirVulkanRecordedNeedsBufferBarrier( const sirVulkanRecordedCommand_t *pCommands, uint32_t nIndex ) noexcept;

// Exact byte span from the first to last copied byte, including row/slice strides.
// Independently validates the transfer's block-aligned buffer footprint.
bool sirVulkanTextureTransferFootprint( const sirVulkanImage_t *pImage, const sirVulkanTextureTransfer_t& transfer,
                                        uint64_t *pBytes ) noexcept;
sirVulkanResult_t sirVulkanValidateTextureTransfer( const sirVulkanBuffer_t *pBuffer, const sirVulkanImage_t *pImage,
                                                    const sirVulkanTextureTransfer_t& transfer ) noexcept;
// Conservatively track initialization of an entire mip/layer, not merely a written region.
bool sirVulkanTransferCoversSubresource( const sirVulkanImage_t *pImage, const sirVulkanTextureTransfer_t& transfer ) noexcept;
VkImageLayout sirVulkanUploadFinalLayout( const sirVulkanImage_t *pImage, const sirVulkanTextureTransfer_t& transfer ) noexcept;

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
	PFN_vkCmdBindPipeline m_pfnCmdBindPipeline = nullptr;
	PFN_vkCmdDispatch m_pfnCmdDispatch = nullptr;
	PFN_vkCmdCopyBufferToImage2 m_pfnCmdCopyBufferToImage2 = nullptr;
	PFN_vkCmdCopyImageToBuffer2 m_pfnCmdCopyImageToBuffer2 = nullptr;
	PFN_vkQueueSubmit2 m_pfnQueueSubmit2;
	PFN_vkCreateSemaphore m_pfnCreateSemaphore;
	PFN_vkDestroySemaphore m_pfnDestroySemaphore;
	PFN_vkGetSemaphoreCounterValue m_pfnGetSemaphoreCounterValue;
	PFN_vkWaitSemaphores m_pfnWaitSemaphores;
	PFN_vkCreateImageView m_pfnCreateImageView = nullptr;
	PFN_vkDestroyImageView m_pfnDestroyImageView = nullptr;
	PFN_vkCreateSampler m_pfnCreateSampler = nullptr;
	PFN_vkDestroySampler m_pfnDestroySampler = nullptr;
	VkInstance m_Instance;
	VkPhysicalDevice m_PhysicalDevice;
	VkDevice m_Device;
	VkQueue m_GraphicsQueue;
	VmaAllocator m_Allocator;
	VmaPool m_szSealedPools[3] = {};
	uint32_t m_szSealedMemoryTypes[3] = {};
	bool m_bSealedPools = false;
	uint64_t m_nSealedBudgetBytes = 0;
	uint64_t m_nPeakCommittedBytes = 0;
	uint64_t m_nPeakAllocationBytes = 0;
	uint64_t m_nUploadMappedBytes = 0;
	uint64_t m_nReadbackMappedBytes = 0;
	VkCommandPool m_TransferCommandPool;
	VkSemaphore m_TransferTimeline;
	sirVulkanTransferSlot_t m_TransferSlots[SIR_VULKAN_TRANSFER_SLOT_COUNT];
	uint64_t m_nDeviceSerial;
	uint64_t m_nLastTransferValue;
	uint32_t m_iNextTransferSlot;
	bool m_bDeviceLost;
	sirVulkanAdapterInfo_t m_Adapter;
	uint32_t m_nEligibleAdapterCount = 0;
	uint32_t m_nLiveBuffers;
	uint32_t m_nLiveImages = 0;
	uint32_t m_nLiveImageViews = 0;
	uint32_t m_nLiveSamplers = 0;
	uint32_t m_nLivePipelines = 0;
	bool m_bReady;

	sirVulkanResult_t initializeInstance( void ) noexcept;
	bool loadInstanceDispatch( void ) noexcept;
	bool chooseAdapter( uint32_t nAdapterIndex ) noexcept;
	bool initializeTransferQueue( void ) noexcept;
	bool loadImageDispatch( void ) noexcept;
	void destroyTransferQueue( void ) noexcept;
	bool isBufferIdle( const sirVulkanBuffer_t *pBuffer ) const noexcept;
	sirVulkanResult_t chooseResidentMemoryType( uint32_t *pMemoryType ) noexcept;
	sirVulkanResult_t initializeSealedPools( const sirVulkanPoolBudget_t *pBudget ) noexcept;
	void destroySealedPools( void ) noexcept;
	void updateMemoryHighWater( void ) noexcept;
	bool isSealedMemoryType( const VkImageCreateInfo& info ) const noexcept;
	bool isSealedBufferMemoryType( const VkBufferCreateInfo& info, uint32_t iPool ) const noexcept;

public:
	SIRVulkanDevice( void ) noexcept;
	~SIRVulkanDevice( void ) noexcept;

	SIRVulkanDevice( const SIRVulkanDevice& ) = delete;
	SIRVulkanDevice& operator=( const SIRVulkanDevice& ) = delete;

	sirVulkanResult_t initialize( const sirVulkanPoolBudget_t *pBudget = nullptr, uint32_t nAdapterIndex = 0 ) noexcept;
	sirVulkanResult_t probeAdapters( uint32_t nAdapterIndex, uint32_t *pCount, sirVulkanAdapterInfo_t *pAdapter ) noexcept;
	bool shutdown( void ) noexcept;
	bool isReady( void ) const noexcept;
	uint32_t getAdapterCount( void ) const noexcept;
	bool queryAdapter( sirVulkanAdapterInfo_t *pAdapter ) const noexcept;
	bool queryMemoryStats( sirVulkanMemoryStats_t *pStats ) const noexcept;

	sirVulkanResult_t createComputePipeline( const void *pCode, uint64_t nCodeBytes, const char *pszEntryPoint,
	                                         sirVulkanComputePipeline_t *pPipeline ) noexcept;
	sirVulkanResult_t destroyComputePipeline( sirVulkanComputePipeline_t *pPipeline ) noexcept;
	sirVulkanResult_t createBuffer( uint64_t nBytes, VkBufferUsageFlags nUsage, bool bHostVisible, sirVulkanBuffer_t *pBuffer ) noexcept;
	sirVulkanResult_t createBuffer( uint64_t nBytes, VkBufferUsageFlags nUsage, sirVulkanBufferPlacement_t ePlacement,
	                                sirVulkanBuffer_t *pBuffer ) noexcept;
	sirVulkanResult_t writeBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, const void *pData, uint64_t nBytes ) noexcept;
	sirVulkanResult_t createImage( const VkImageCreateInfo& info, sirVulkanImage_t *pImage ) noexcept;
	sirVulkanResult_t destroyImage( sirVulkanImage_t *pImage ) noexcept;
	sirVulkanResult_t createImageView( const sirVulkanImage_t *pImage, const VkImageViewCreateInfo& info,
	                                   sirVulkanImageView_t *pView ) noexcept;
	sirVulkanResult_t destroyImageView( sirVulkanImageView_t *pView ) noexcept;
	sirVulkanResult_t createSampler( const VkSamplerCreateInfo& info, sirVulkanSampler_t *pSampler ) noexcept;
	sirVulkanResult_t destroySampler( sirVulkanSampler_t *pSampler ) noexcept;
	sirVulkanResult_t destroyIdleBuffer( sirVulkanBuffer_t *pBuffer ) noexcept;
	sirVulkanResult_t readBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, void *pData, uint64_t nBytes ) noexcept;
	sirVulkanResult_t submitBufferCopy( sirVulkanBuffer_t *pSource, uint64_t nSourceOffset, sirVulkanBuffer_t *pDestination,
	                                    uint64_t nDestinationOffset, uint64_t nBytes, sirVulkanCompletion_t *pCompletion ) noexcept;
	sirVulkanResult_t submitRecordedCommands( const sirVulkanRecordedCommand_t *pCommands, uint32_t nCount,
	                                          sirVulkanCompletion_t *pCompletion ) noexcept;
	sirVulkanResult_t submitBufferCopies( const sirVulkanBufferCopy_t *pCopies, uint32_t nCount,
	                                      sirVulkanCompletion_t *pCompletion ) noexcept;
	sirVulkanResult_t submitImageBarrier( sirVulkanImage_t *pImage, const sirVulkanImageBarrier_t& barrier,
	                                      sirVulkanCompletion_t *pCompletion ) noexcept;
	sirVulkanResult_t submitBufferImageCopy( sirVulkanBuffer_t *pBuffer, sirVulkanImage_t *pImage,
	                                         const sirVulkanTextureTransfer_t& transfer, sirVulkanCompletion_t *pCompletion ) noexcept;
	sirVulkanResult_t queryCompletion( const sirVulkanCompletion_t *pCompletion, bool *pComplete ) const noexcept;
	sirVulkanResult_t waitCompletion( const sirVulkanCompletion_t *pCompletion, uint64_t nTimeoutNs ) const noexcept;
};

#endif // __SIRENGINE_SIRVULKANDEVICE_H__
