/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include "Renderer/Drivers/Vulkan/SIRVulkanShared.h"
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic pop
#endif
#include <Renderer/Drivers/Vulkan/SIRVulkanCommandList.h>

static std::atomic<uint64_t> s_nNextVulkanSerial( 1 );

static VkBufferUsageFlags sirVulkanBufferUsageForDescription( uint64_t nUsage ) noexcept
{
	VkBufferUsageFlags result = 0;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_VERTEX ) ) != 0 ) result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_INDEX ) ) != 0 ) result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_CONSTANT ) ) != 0 ) result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_STORAGE ) ) != 0 ) result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_TEXEL ) ) != 0 ) {
		result |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
	}
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_INDIRECT ) ) != 0 ) result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_TRANSFERSOURCE ) ) != 0 ) result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_TRANSFERDESTINATION ) ) != 0 ) result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_ACCELERATIONSTRUCTURE ) ) != 0 ) result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_SHADERBINDINGTABLE ) ) != 0 ) result |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_DEVICEADDRESS ) ) != 0 ) result |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	return result;
}

static VkFormat sirVulkanFormatForDescription( SIRRenderFormat format ) noexcept
{
	switch ( format ) {
	case SIRRenderFormat::SIR_RENDER_R8: return VK_FORMAT_R8_UNORM;
	case SIRRenderFormat::SIR_RENDER_RG8: return VK_FORMAT_R8G8_UNORM;
	case SIRRenderFormat::SIR_RENDER_RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_R16F: return VK_FORMAT_R16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_RG16F: return VK_FORMAT_R16G16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_R32F: return VK_FORMAT_R32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_RG32F: return VK_FORMAT_R32G32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_D24S8: return VK_FORMAT_D24_UNORM_S8_UINT;
	case SIRRenderFormat::SIR_RENDER_D32F: return VK_FORMAT_D32_SFLOAT;
	default: return VK_FORMAT_UNDEFINED;
	}
}

static VkImageUsageFlags sirVulkanImageUsageForDescription( uint64_t nUsage ) noexcept
{
	VkImageUsageFlags result = 0;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_SAMPLED ) ) != 0 ) result |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_STORAGE ) ) != 0 ) result |= VK_IMAGE_USAGE_STORAGE_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_COLORATTACHMENT ) ) != 0 ) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_DEPTHSTENCILATTACHMENT ) ) != 0 ) result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TRANSFERSOURCE ) ) != 0 ) result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TRANSFERDESTINATION ) ) != 0 ) result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_INPUTATTACHMENT ) ) != 0 ) result |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	if ( ( nUsage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_PRESENT ) ) != 0 ) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	return result;
}

static VkSampleCountFlagBits sirVulkanSampleCount( uint32_t nSamples ) noexcept
{
	switch ( nSamples ) {
	case 1: return VK_SAMPLE_COUNT_1_BIT;
	case 2: return VK_SAMPLE_COUNT_2_BIT;
	case 4: return VK_SAMPLE_COUNT_4_BIT;
	case 8: return VK_SAMPLE_COUNT_8_BIT;
	case 16: return VK_SAMPLE_COUNT_16_BIT;
	case 32: return VK_SAMPLE_COUNT_32_BIT;
	case 64: return VK_SAMPLE_COUNT_64_BIT;
	default: return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
	}
}

static SIRRenderResult sirVulkanMemoryRequirementsResult( VkResult eResult ) noexcept
{
	if ( eResult == VK_SUCCESS ) return SIRRenderResult::SIR_RENDER_SUCCESS;
	if ( eResult == VK_ERROR_OUT_OF_DEVICE_MEMORY || eResult == VK_ERROR_OUT_OF_HOST_MEMORY ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	return SIRRenderResult::SIR_RENDER_BACKENDERROR;
}

static void sirVulkanSetMemoryRequirements( const VkMemoryRequirements& native, sirRenderMemoryRequirements_t *pRequirements ) noexcept
{
	pRequirements->nSize = native.size;
	pRequirements->nAlignment = native.alignment;
	pRequirements->nMemoryTypeMask = native.memoryTypeBits;
}

SIRVulkanQueue::SIRVulkanQueue( SIRVulkanDevice& device, SIRRenderQueueType eType ) noexcept
    : SIRRenderQueue( device, eType )
{
}

SIRRenderResult SIRVulkanQueue::submit( const SIRRenderSubmission& submission ) noexcept
{
	if ( submission.commands.getData() == nullptr || submission.commands.getSize() == 0 ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	for ( uint32_t i = 0; i < submission.commands.getSize(); ++i ) {
		auto *pCommandList = dynamic_cast<SIRVulkanCommandList *>( submission.commands[i] );
		if ( pCommandList == nullptr ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
		sirVulkanCompletion_t completion{};
		const SIRRenderResult result = pCommandList->submitNative( &completion );
		if ( result != SIRRenderResult::SIR_RENDER_SUCCESS ) return result;
	}
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}
SIRRenderResult SIRVulkanQueue::waitIdle( void ) noexcept { return static_cast<SIRVulkanDevice&>( getDevice() ).waitIdle(); }
SIRRenderResult SIRVulkanQueue::collectCompleted( void ) noexcept { return SIRRenderResult::SIR_RENDER_SUCCESS; }

SIRVulkanMemoryAllocator::SIRVulkanMemoryAllocator( SIRVulkanDevice& device ) noexcept : SIRRenderMemoryAllocator( device ) {}

SIRRenderResult SIRVulkanMemoryAllocator::getBufferRequirements( const sirRenderBufferDescription_t& description,
                                                                  sirRenderMemoryRequirements_t *pRequirements ) const noexcept
{
	if ( pRequirements == nullptr || description.nSize == 0 || description.nUsage == 0 ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	*pRequirements = {};
	const SIRVulkanDevice& device = static_cast<const SIRVulkanDevice&>( getDevice() );
	if ( !device.m_bReady || device.m_Procs.vkCreateBuffer == nullptr || device.m_Procs.vkGetBufferMemoryRequirements == nullptr ||
	     device.m_Procs.vkDestroyBuffer == nullptr || device.m_Allocator == nullptr ) {
		return SIRRenderResult::SIR_RENDER_BACKENDERROR;
	}
	VkBufferCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = description.nSize;
	info.usage = sirVulkanBufferUsageForDescription( description.nUsage );
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( info.usage == 0 ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	VkBuffer buffer = VK_NULL_HANDLE;
	const VkResult eCreate = device.m_Procs.vkCreateBuffer( device.m_Device, &info, nullptr, &buffer );
	if ( eCreate != VK_SUCCESS ) return sirVulkanMemoryRequirementsResult( eCreate );
	VkMemoryRequirements native{};
	device.m_Procs.vkGetBufferMemoryRequirements( device.m_Device, buffer, &native );
	device.m_Procs.vkDestroyBuffer( device.m_Device, buffer, nullptr );

	VmaAllocationCreateInfo allocation{};
	allocation.usage = VMA_MEMORY_USAGE_AUTO;
	if ( description.bHostVisible || description.eMemory != SIRRenderMemoryType::SIR_RENDER_DEVICELOCAL ) {
		allocation.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		if ( description.bHostCoherent || description.eMemory == SIRRenderMemoryType::SIR_RENDER_HOSTCOHERENT ) allocation.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		if ( description.eMemory == SIRRenderMemoryType::SIR_RENDER_HOSTCACHED ) allocation.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	} else if ( description.eMemory == SIRRenderMemoryType::SIR_RENDER_DEVICELOCAL ) {
		allocation.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	} else {
		allocation.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}
	uint32_t memoryType = UINT32_MAX;
	if ( vmaFindMemoryTypeIndexForBufferInfo( device.m_Allocator, &info, &allocation, &memoryType ) != VK_SUCCESS ||
	     memoryType >= 32 || ( native.memoryTypeBits & ( 1U << memoryType ) ) == 0 ) {
		return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	}
	sirVulkanSetMemoryRequirements( native, pRequirements );
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}
SIRRenderResult SIRVulkanMemoryAllocator::getTextureRequirements( const sirRenderTextureDescription_t& description,
                                                                   sirRenderMemoryRequirements_t *pRequirements ) const noexcept
{
	if ( pRequirements == nullptr || description.nWidth == 0 || description.nHeight == 0 || description.nDepth == 0 || description.nLayers == 0 ||
	     description.nMipLevels == 0 || description.nSamples == 0 || description.eFormat == SIRRenderFormat::SIR_RENDER_UNKNOWN ||
	     description.bExternallyOwned || description.bSparse ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	*pRequirements = {};
	const SIRVulkanDevice& device = static_cast<const SIRVulkanDevice&>( getDevice() );
	if ( !device.m_bReady || device.m_Procs.vkCreateImage == nullptr || device.m_Procs.vkGetImageMemoryRequirements == nullptr ||
	     device.m_Procs.vkDestroyImage == nullptr || device.m_Allocator == nullptr ) return SIRRenderResult::SIR_RENDER_BACKENDERROR;
	const VkFormat format = sirVulkanFormatForDescription( description.eFormat );
	const VkImageUsageFlags usage = sirVulkanImageUsageForDescription( description.nUsage );
	const VkSampleCountFlagBits samples = sirVulkanSampleCount( description.nSamples );
	constexpr uint64_t nUsageMask = static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_SAMPLED ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_STORAGE ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_COLORATTACHMENT ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_DEPTHSTENCILATTACHMENT ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TRANSFERSOURCE ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TRANSFERDESTINATION ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_INPUTATTACHMENT );
	const bool bDepth = ( sirVulkanImageAspectMask( format ) & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0;
	if ( format == VK_FORMAT_UNDEFINED || usage == 0 || samples == VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM || description.nSamples != 1 ||
	     ( description.nUsage & ~nUsageMask ) != 0 ||
	     ( bDepth && ( description.nUsage & ( static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_STORAGE ) |
	                                           static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_COLORATTACHMENT ) ) ) != 0 ) ||
	     ( !bDepth && ( description.nUsage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_DEPTHSTENCILATTACHMENT ) ) != 0 ) ||
	     ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_TWO && description.nDepth != 1 ) ||
	     ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_ONE && ( description.nHeight != 1 || description.nDepth != 1 ) ) ||
	     ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_THREE && description.nLayers != 1 ) ||
	     ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_CUBE && ( description.nDepth != 1 || description.nLayers < 6 || description.nLayers % 6 != 0 ) ) ) {
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}
	VkImageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = description.eDimension == SIRRenderTextureDimension::SIR_RENDER_ONE ? VK_IMAGE_TYPE_1D :
	                 description.eDimension == SIRRenderTextureDimension::SIR_RENDER_THREE ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = { description.nWidth, description.nHeight, description.nDepth };
	info.mipLevels = description.nMipLevels;
	info.arrayLayers = description.nLayers;
	info.samples = samples;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_CUBE ) info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	VkImage image = VK_NULL_HANDLE;
	const VkResult eCreate = device.m_Procs.vkCreateImage( device.m_Device, &info, nullptr, &image );
	if ( eCreate != VK_SUCCESS ) return sirVulkanMemoryRequirementsResult( eCreate );
	VkMemoryRequirements native{};
	device.m_Procs.vkGetImageMemoryRequirements( device.m_Device, image, &native );
	device.m_Procs.vkDestroyImage( device.m_Device, image, nullptr );
	VmaAllocationCreateInfo allocation{};
	allocation.usage = VMA_MEMORY_USAGE_AUTO;
	allocation.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	uint32_t memoryType = UINT32_MAX;
	if ( vmaFindMemoryTypeIndexForImageInfo( device.m_Allocator, &info, &allocation, &memoryType ) != VK_SUCCESS || memoryType >= 32 ||
	     ( native.memoryTypeBits & ( 1U << memoryType ) ) == 0 ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	sirVulkanSetMemoryRequirements( native, pRequirements );
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}
SIRRenderResult SIRVulkanMemoryAllocator::getRenderbufferRequirements( const sirRenderTextureDescription_t& description,
                                                                       sirRenderMemoryRequirements_t *pRequirements ) const noexcept
{
	if ( pRequirements == nullptr || description.nWidth == 0 || description.nHeight == 0 || description.eFormat == SIRRenderFormat::SIR_RENDER_UNKNOWN ) {
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}
	sirRenderTextureDescription_t texture = description;
	texture.nDepth = 1;
	texture.nLayers = 1;
	texture.nMipLevels = 1;
	texture.eDimension = SIRRenderTextureDimension::SIR_RENDER_TWO;
	texture.nUsage &= static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_COLORATTACHMENT ) |
	                 static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_DEPTHSTENCILATTACHMENT );
	if ( texture.nUsage == 0 ) {
		const bool bDepth = description.eFormat == SIRRenderFormat::SIR_RENDER_D24S8 || description.eFormat == SIRRenderFormat::SIR_RENDER_D32F;
		texture.nUsage = static_cast<uint64_t>( bDepth ? SIRRenderTextureUsage::SIR_RENDER_DEPTHSTENCILATTACHMENT
		                                              : SIRRenderTextureUsage::SIR_RENDER_COLORATTACHMENT );
	}
	return getTextureRequirements( texture, pRequirements );
}
SIRRenderResult SIRVulkanMemoryAllocator::getAccelerationStructureRequirements( uint64_t nSizeBytes,
                                                                                 sirRenderMemoryRequirements_t *pRequirements ) const noexcept
{
	if ( pRequirements == nullptr || nSizeBytes == 0 ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirRenderBufferDescription_t description{};
	description.nSize = nSizeBytes;
	description.nUsage = static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_ACCELERATIONSTRUCTURE ) |
	                     static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_DEVICEADDRESS );
	description.eMemory = SIRRenderMemoryType::SIR_RENDER_DEVICELOCAL;
	return getBufferRequirements( description, pRequirements );
}
SIRRenderResult SIRVulkanMemoryAllocator::getMemoryBudget( sirRenderMemoryBudget_t *pBudget ) const noexcept
{
	if ( pBudget == nullptr ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	*pBudget = {};
	const SIRVulkanDevice& device = static_cast<const SIRVulkanDevice&>( getDevice() );
	if ( !device.m_bReady || device.m_Allocator == nullptr ) return SIRRenderResult::SIR_RENDER_BACKENDERROR;
	VmaBudget budgets[VK_MAX_MEMORY_HEAPS]{};
	vmaGetHeapBudgets( device.m_Allocator, budgets );
	for ( uint32_t iHeap = 0; iHeap < device.m_Adapter.nMemoryHeapCount; ++iHeap ) {
		pBudget->nBudget = budgets[iHeap].budget > UINT64_MAX - pBudget->nBudget ? UINT64_MAX : pBudget->nBudget + budgets[iHeap].budget;
		pBudget->nUsage = budgets[iHeap].usage > UINT64_MAX - pBudget->nUsage ? UINT64_MAX : pBudget->nUsage + budgets[iHeap].usage;
	}
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRVulkanDevice::SIRVulkanDevice( void ) noexcept
    : SIRRenderDevice( SIRRenderBackend::SIR_RENDER_VULKAN, {} ),
      m_Library{},
      m_Procs{},
      m_Instance( VK_NULL_HANDLE ),
      m_PhysicalDevice( VK_NULL_HANDLE ),
      m_Device( VK_NULL_HANDLE ),
      m_GraphicsQueue( VK_NULL_HANDLE ),
      m_ComputeQueue( VK_NULL_HANDLE ),
      m_TransferQueue( VK_NULL_HANDLE ),
      m_Allocator( nullptr ),
      m_TransferCommandPool( VK_NULL_HANDLE ),
      m_TransferTimeline( VK_NULL_HANDLE ),
      m_TransferSlots{},
      m_nDeviceSerial( 0 ),
      m_nLastTransferValue( 0 ),
      m_iNextTransferSlot( 0 ),
      m_bDeviceLost( false ),
      m_Adapter{},
      m_nLiveBuffers( 0 ),
      m_bReady( false ),
      m_GraphicsRHIQueue( *this, SIRRenderQueueType::SIR_RENDER_GRAPHICS ),
      m_ComputeRHIQueue( *this, SIRRenderQueueType::SIR_RENDER_COMPUTE ),
      m_TransferRHIQueue( *this, SIRRenderQueueType::SIR_RENDER_TRANSFER ),
      m_RHIAllocator( *this )
{
}

SIRVulkanDevice::~SIRVulkanDevice( void ) noexcept
{
	// The caller must destroy its buffers and wait for GPU users before shutdown.
	shutdownNative();
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

bool sirVulkanSelectQueueFamilies( const VkQueueFamilyProperties *pFamilies, uint32_t nCount, sirVulkanQueueFamilySelection_t *pSelection ) noexcept
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
	pSelection->nTransferFamilyIndex =
	    nDedicatedTransfer != UINT32_MAX ? nDedicatedTransfer : ( nTransferFallback != UINT32_MAX ? nTransferFallback : pSelection->nGraphicsFamilyIndex );
	return true;
}

bool sirVulkanBuildQueueCreateInfos( const sirVulkanQueueFamilySelection_t& selection, const float *pPriority, VkDeviceQueueCreateInfo ( &szInfos )[3], uint32_t *pCount ) noexcept
{
	if ( pCount == nullptr ) {
		return false;
	}
	*pCount = 0;
	if ( pPriority == nullptr || selection.nGraphicsFamilyIndex == UINT32_MAX ) {
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
			if ( szInfos[iExisting].queueFamilyIndex == nFamily ) {
				bAlreadyAdded = true;
				break;
			}
		}
		if ( bAlreadyAdded ) {
			continue;
		}
		VkDeviceQueueCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueFamilyIndex = nFamily;
		info.queueCount = 1;
		info.pQueuePriorities = pPriority;
		szInfos[( *pCount )++] = info;
	}
	return true;
}

bool SIRVulkanDevice::chooseAdapter( uint32_t nAdapterIndex ) noexcept
{
	uint32_t nDeviceCount = 0;
	if ( m_Procs.vkEnumeratePhysicalDevices( m_Instance, &nDeviceCount, nullptr ) != VK_SUCCESS || nDeviceCount == 0 || nDeviceCount > 32 ) {
		return false;
	}

	VkPhysicalDevice szDevices[32];
	memset( szDevices, 0, sizeof( szDevices ) );
	if ( m_Procs.vkEnumeratePhysicalDevices( m_Instance, &nDeviceCount, szDevices ) != VK_SUCCESS ) {
		return false;
	}

	VkPhysicalDevice szEligible[32];
	sirVulkanQueueFamilySelection_t szSelections[32];
	uint32_t szPriorities[32];
	uint32_t nEligibleCount = 0;

	memset( szEligible, 0, sizeof( szEligible ) );
	memset( szSelections, 0, sizeof( szSelections ) );
	memset( szPriorities, 0, sizeof( szPriorities ) );

	for ( uint32_t iDevice = 0; iDevice < nDeviceCount; ++iDevice ) {
		VkPhysicalDeviceProperties properties{};
		m_Procs.vkGetPhysicalDeviceProperties( szDevices[iDevice], &properties );
		if ( properties.apiVersion < VK_API_VERSION_1_3 ) {
			continue;
		}

		VkPhysicalDeviceVulkan13Features features13{};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

		VkPhysicalDeviceVulkan12Features features12{};
		features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features12.pNext = &features13;

		VkPhysicalDeviceFeatures2 features{};
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &features12;

		m_Procs.vkGetPhysicalDeviceFeatures2( szDevices[iDevice], &features );
		if ( features12.timelineSemaphore == VK_FALSE || features13.synchronization2 == VK_FALSE || features13.dynamicRendering == VK_FALSE ) {
			continue;
		}
		uint32_t nFamilyCount = 0;
		m_Procs.vkGetPhysicalDeviceQueueFamilyProperties( szDevices[iDevice], &nFamilyCount, nullptr );
		if ( nFamilyCount == 0 || nFamilyCount > 64 ) {
			continue;
		}

		VkQueueFamilyProperties szFamiliesInfo[64];
		memset( szFamiliesInfo, 0, sizeof( szFamiliesInfo ) );
		m_Procs.vkGetPhysicalDeviceQueueFamilyProperties( szDevices[iDevice], &nFamilyCount, szFamiliesInfo );
		sirVulkanQueueFamilySelection_t selection{};
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

	VkPhysicalDeviceProperties selectedProperties{};
	m_Procs.vkGetPhysicalDeviceProperties( m_PhysicalDevice, &selectedProperties );
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

	memcpy( m_Adapter.szMaximumComputeGroupCount, selectedProperties.limits.maxComputeWorkGroupCount, sizeof( m_Adapter.szMaximumComputeGroupCount ) );
	memcpy( m_Adapter.szDeviceName, selectedProperties.deviceName, sizeof( m_Adapter.szDeviceName ) );

	VkPhysicalDeviceMemoryProperties memory{};
	m_Procs.vkGetPhysicalDeviceMemoryProperties( m_PhysicalDevice, &memory );
	m_Adapter.nMemoryHeapCount = memory.memoryHeapCount;
	m_Adapter.nMemoryTypeCount = memory.memoryTypeCount;
	for ( uint32_t iHeap = 0; iHeap < memory.memoryHeapCount; ++iHeap ) {
		if ( ( memory.memoryHeaps[iHeap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) != 0 ) {
			m_Adapter.nDeviceLocalHeapBytes += memory.memoryHeaps[iHeap].size;
		}
	}

	return true;
}

SIRVulkanResult SIRVulkanDevice::initializeInstance( void ) noexcept
{
	if ( m_bReady || m_Library.isOpen() ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}

#if defined( _WIN32 )
	const char *szLoader = "vulkan-1.dll";
#else
	const char *szLoader = "libvulkan.so.1";
#endif
	if ( m_Library.load( szLoader ) != sirLibraryResult::SIR_LIBRARY_OK ) {
		return SIRVulkanResult::SIR_VULKAN_LOADER_UNAVAILABLE;
	}

	void *pGetInstanceProcAddr = nullptr;
	static_cast<void>( m_Library.findSymbol( "vkGetInstanceProcAddr", &pGetInstanceProcAddr ) );
	m_Procs.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>( pGetInstanceProcAddr );
	if ( m_Procs.vkGetInstanceProcAddr == nullptr ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_LOADER_UNAVAILABLE;
	}
	if ( !SIR_Vulkan_LoadGlobalProcs( m_Procs, m_Procs.vkGetInstanceProcAddr ) ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_LOADER_UNAVAILABLE;
	}

	uint32_t nLoaderVersion = VK_API_VERSION_1_0;
	if ( m_Procs.vkEnumerateInstanceVersion == nullptr || m_Procs.vkEnumerateInstanceVersion( &nLoaderVersion ) != VK_SUCCESS || nLoaderVersion < VK_API_VERSION_1_4 ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	}

	if ( m_Procs.vkCreateInstance == nullptr ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_LOADER_UNAVAILABLE;
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "SIREngine";
	appInfo.applicationVersion = VK_MAKE_API_VERSION( 0, 0, 1, 0 );
	appInfo.pEngineName = "SIREngine";
	appInfo.engineVersion = VK_MAKE_API_VERSION( 0, 0, 1, 0 );
	appInfo.apiVersion = VK_API_VERSION_1_4;

	VkInstanceCreateInfo instanceInfo{};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	if ( m_Procs.vkCreateInstance( &instanceInfo, nullptr, &m_Instance ) != VK_SUCCESS ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	if ( !SIR_Vulkan_LoadInstanceProcs( m_Procs, m_Instance ) ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::probeAdapters( uint32_t nAdapterIndex, uint32_t *pCount, sirVulkanAdapterInfo_t *pAdapter ) noexcept
{
	if ( ( pCount == nullptr && pAdapter == nullptr ) || nAdapterIndex >= 32 ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( m_bReady || m_Library.isOpen() ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pCount != nullptr ) {
		*pCount = 0;
	}
	if ( pAdapter != nullptr ) {
		*pAdapter = {};
	}
	const SIRVulkanResult eInstance = initializeInstance();
	if ( eInstance != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
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
	if ( shutdownNative() != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	return bSelected ? SIRVulkanResult::SIR_VULKAN_SUCCESS : SIRVulkanResult::SIR_VULKAN_DEVICE_UNAVAILABLE;
}

SIRRenderResult SIRVulkanDevice::initialize( void ) noexcept
{
	return initializeNative() == SIRVulkanResult::SIR_VULKAN_SUCCESS ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_BACKENDERROR;
}

SIRVulkanResult SIRVulkanDevice::initializeNative( const sirVulkanPoolBudget_t *pBudget, uint32_t nAdapterIndex ) noexcept
{
	if ( m_bReady || m_Library.isOpen() ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pBudget != nullptr && !sirVulkanValidatePoolBudget( pBudget ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const SIRVulkanResult eInstance = initializeInstance();
	if ( eInstance != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		return eInstance;
	}
	if ( !chooseAdapter( nAdapterIndex ) ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_DEVICE_UNAVAILABLE;
	}

	VkPhysicalDeviceVulkan13Features supported13{};
	supported13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	VkPhysicalDeviceVulkan12Features supported12{};
	supported12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	supported12.pNext = &supported13;
	VkPhysicalDeviceFeatures2 queriedFeatures{};
	queriedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	queriedFeatures.pNext = &supported12;
	m_Procs.vkGetPhysicalDeviceFeatures2( m_PhysicalDevice, &queriedFeatures );
	if ( supported12.timelineSemaphore == VK_FALSE || supported13.synchronization2 == VK_FALSE || supported13.dynamicRendering == VK_FALSE ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	}

	VkPhysicalDeviceVulkan13Features enabled13{};
	enabled13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	enabled13.synchronization2 = VK_TRUE;
	enabled13.dynamicRendering = VK_TRUE;
	VkPhysicalDeviceVulkan12Features enabled12{};
	enabled12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	enabled12.timelineSemaphore = VK_TRUE;
	enabled12.bufferDeviceAddress = supported12.bufferDeviceAddress;
	enabled12.pNext = &enabled13;

	const float fPriority = 1.0f;
	const sirVulkanQueueFamilySelection_t families = {
	    m_Adapter.nQueueFamilyIndex, m_Adapter.nComputeQueueFamilyIndex, m_Adapter.nTransferQueueFamilyIndex, m_Adapter.bComputeCapable };
	VkDeviceQueueCreateInfo szQueueInfos[3];
	memset( szQueueInfos, 0, sizeof( szQueueInfos ) );
	uint32_t nQueueCount = 0;
	if ( !sirVulkanBuildQueueCreateInfos( families, &fPriority, szQueueInfos, &nQueueCount ) ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_DEVICE_UNAVAILABLE;
	}

	VkPhysicalDeviceFeatures coreFeatures{};
	coreFeatures.independentBlend = queriedFeatures.features.independentBlend;
	VkDeviceCreateInfo deviceInfo{};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pEnabledFeatures = &coreFeatures;
	deviceInfo.queueCreateInfoCount = nQueueCount;
	deviceInfo.pQueueCreateInfos = szQueueInfos;
	deviceInfo.pNext = &enabled12;
	if ( m_Procs.vkCreateDevice( m_PhysicalDevice, &deviceInfo, nullptr, &m_Device ) != VK_SUCCESS ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	if ( !SIR_Vulkan_LoadDeviceProcs( m_Procs, m_Device ) || m_Procs.vkDestroyDevice == nullptr || m_Procs.vkGetDeviceQueue == nullptr ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	m_Procs.vkGetDeviceQueue( m_Device, m_Adapter.nQueueFamilyIndex, 0, &m_GraphicsQueue );
	if ( m_Adapter.nComputeQueueFamilyIndex != UINT32_MAX ) {
		m_Procs.vkGetDeviceQueue( m_Device, m_Adapter.nComputeQueueFamilyIndex, 0, &m_ComputeQueue );
	}
	if ( m_Adapter.nTransferQueueFamilyIndex != UINT32_MAX ) {
		m_Procs.vkGetDeviceQueue( m_Device, m_Adapter.nTransferQueueFamilyIndex, 0, &m_TransferQueue );
	}
	if ( m_GraphicsQueue == VK_NULL_HANDLE || ( m_Adapter.nComputeQueueFamilyIndex != UINT32_MAX && m_ComputeQueue == VK_NULL_HANDLE ) ||
	     ( m_Adapter.nTransferQueueFamilyIndex != UINT32_MAX && m_TransferQueue == VK_NULL_HANDLE ) ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	VmaVulkanFunctions functions{};
	functions.vkGetInstanceProcAddr = m_Procs.vkGetInstanceProcAddr;
	functions.vkGetDeviceProcAddr = m_Procs.vkGetDeviceProcAddr;
	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.instance = m_Instance;
	allocatorInfo.physicalDevice = m_PhysicalDevice;
	allocatorInfo.device = m_Device;
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
	if ( supported12.bufferDeviceAddress == VK_TRUE ) allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocatorInfo.pVulkanFunctions = &functions;
	if ( vmaCreateAllocator( &allocatorInfo, &m_Allocator ) != VK_SUCCESS ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED;
	}
	if ( pBudget != nullptr ) {
		const SIRVulkanResult ePools = initializeSealedPools( pBudget );
		if ( ePools != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			shutdownNative();
			return ePools;
		}
	}

	if ( !initializeTransferQueue() || !loadImageDispatch() ) {
		shutdownNative();
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	m_nDeviceSerial = s_nNextVulkanSerial.fetch_add( 1, std::memory_order_relaxed );
	m_bReady = true;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::shutdownNative( void ) noexcept
{
	if ( m_nLiveBuffers != 0 || m_nLiveImages != 0 || m_nLiveImageViews != 0 || m_nLiveSamplers != 0 || m_nLivePipelines != 0 || m_nLiveDescriptorSets != 0 ) {
		return SIRVulkanResult::SIR_VULKAN_BUSY;
	}

	m_bReady = false;
	bool bIdle = true;
	if ( m_Device != VK_NULL_HANDLE && m_Procs.vkDeviceWaitIdle != nullptr ) {
		bIdle = m_Procs.vkDeviceWaitIdle( m_Device ) == VK_SUCCESS;
	}
	destroyTransferQueue();
	destroySealedPools();
	if ( m_Allocator != nullptr ) {
		vmaDestroyAllocator( m_Allocator );
		m_Allocator = nullptr;
	}
	if ( m_Device != VK_NULL_HANDLE && m_Procs.vkDestroyDevice != nullptr ) {
		m_Procs.vkDestroyDevice( m_Device, nullptr );
	}
	m_Device = VK_NULL_HANDLE;
	m_GraphicsQueue = VK_NULL_HANDLE;
	m_ComputeQueue = VK_NULL_HANDLE;
	m_TransferQueue = VK_NULL_HANDLE;
	if ( m_Instance != VK_NULL_HANDLE && m_Procs.vkDestroyInstance != nullptr ) {
		m_Procs.vkDestroyInstance( m_Instance, nullptr );
	}
	m_Instance = VK_NULL_HANDLE;
	m_PhysicalDevice = VK_NULL_HANDLE;
	if ( m_Library.isOpen() ) {
		static_cast<void>( m_Library.unload() );
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
	return bIdle ? SIRVulkanResult::SIR_VULKAN_SUCCESS : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
}

SIRRenderResult SIRVulkanDevice::shutdown( void ) noexcept
{
	const SIRVulkanResult result = shutdownNative();
	return result == SIRVulkanResult::SIR_VULKAN_SUCCESS ? SIRRenderResult::SIR_RENDER_SUCCESS :
	       ( result == SIRVulkanResult::SIR_VULKAN_BUSY ? SIRRenderResult::SIR_RENDER_BUSY : SIRRenderResult::SIR_RENDER_BACKENDERROR );
}

SIRRenderQueue *SIRVulkanDevice::getGraphicsQueue( void ) noexcept { return &m_GraphicsRHIQueue; }
SIRRenderQueue *SIRVulkanDevice::getComputeQueue( void ) noexcept { return &m_ComputeRHIQueue; }
SIRRenderQueue *SIRVulkanDevice::getTransferQueue( void ) noexcept { return &m_TransferRHIQueue; }
SIRRenderMemoryAllocator& SIRVulkanDevice::getMemoryAllocator( void ) noexcept { return m_RHIAllocator; }
SIRRenderResult SIRVulkanDevice::waitIdle( void ) noexcept
{
	if ( m_Device == VK_NULL_HANDLE || m_Procs.vkDeviceWaitIdle == nullptr ) return m_bReady ? SIRRenderResult::SIR_RENDER_BACKENDERROR : SIRRenderResult::SIR_RENDER_SUCCESS;
	return m_Procs.vkDeviceWaitIdle( m_Device ) == VK_SUCCESS ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_DEVICELOST;
}
bool SIRVulkanDevice::isLost( void ) const noexcept { return m_bDeviceLost; }

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
	VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
	memset( budgets, 0, sizeof( budgets ) );
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

SIRVulkanResult SIRVulkanDevice::createBuffer( uint64_t nBytes, VkBufferUsageFlags nUsage, bool bHostVisible, sirVulkanBuffer_t *pBuffer ) noexcept
{
	const SIRVulkanBufferPlacement ePlacement = !bHostVisible ? SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_GPU_LOCAL
	                                            : ( ( nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) != 0 && ( nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 )
	                                                ? SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_CPU_READBACK
	                                                : SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_CPU_UPLOAD;
	return createBuffer( nBytes, nUsage, ePlacement, pBuffer );
}

SIRVulkanResult SIRVulkanDevice::createBuffer( uint64_t nBytes, VkBufferUsageFlags nUsage, SIRVulkanBufferPlacement ePlacement,
                                               sirVulkanBuffer_t *pBuffer, bool bPersistentlyMapped ) noexcept
{
	if ( !m_bReady ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || nBytes == 0 || nUsage == 0 || pBuffer->hBuffer != VK_NULL_HANDLE || pBuffer->pAllocation != nullptr || pBuffer->pOwnerAllocator != nullptr ||
	     ePlacement > SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_GPU_UPLOAD ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = static_cast<VkDeviceSize>( nBytes );
	bufferInfo.usage = nUsage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
	const bool bHostVisible = sirVulkanPlacementHostVisible( ePlacement );
	if ( m_bSealedPools ) {
		allocationInfo.pool = m_szSealedPools[sirVulkanPlacementPool( ePlacement )];
		allocationInfo.flags |= VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT;
	}
	if ( bHostVisible ) {
		if ( bPersistentlyMapped ) allocationInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
		if ( ePlacement == SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_CPU_READBACK ) {
			allocationInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
			allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		} else {
			allocationInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		}
		if ( ePlacement == SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_GPU_UPLOAD ) {
			allocationInfo.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}
		allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	} else {
		allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}
	if ( m_bSealedPools && !isSealedBufferMemoryType( bufferInfo, sirVulkanPlacementPool( ePlacement ) ) ) {
		return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	}
	VmaAllocationInfo nativeAllocation{};
	if ( vmaCreateBuffer( m_Allocator, &bufferInfo, &allocationInfo, &pBuffer->hBuffer, &pBuffer->pAllocation, &nativeAllocation ) != VK_SUCCESS ) {
		pBuffer->hBuffer = VK_NULL_HANDLE;
		pBuffer->pAllocation = nullptr;
		return SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED;
	}

	if ( bHostVisible && bPersistentlyMapped && nativeAllocation.pMappedData == nullptr ) {
		vmaDestroyBuffer( m_Allocator, pBuffer->hBuffer, pBuffer->pAllocation );
		*pBuffer = {};
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	pBuffer->pMappedData = nativeAllocation.pMappedData;
	pBuffer->nAllocationBytes = nativeAllocation.size;
	pBuffer->ePlacement = ePlacement;
	pBuffer->pOwnerAllocator = m_Allocator;
	pBuffer->nSize = static_cast<VkDeviceSize>( nBytes );
	pBuffer->nUsage = nUsage;
	pBuffer->nDeviceSerial = m_nDeviceSerial;
	pBuffer->nLastGpuUse = 0;
	pBuffer->bHostVisible = bHostVisible;
	pBuffer->bHostCoherent = false;
	pBuffer->bPersistentlyMapped = bPersistentlyMapped;
	pBuffer->bMapped = bPersistentlyMapped;
	VkMemoryPropertyFlags memoryProperties = 0;
	vmaGetAllocationMemoryProperties( m_Allocator, pBuffer->pAllocation, &memoryProperties );
	pBuffer->nMemoryProperties = memoryProperties;
	pBuffer->bHostCoherent = ( memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) != 0;
	if ( bHostVisible ) {
		if ( ePlacement == SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_CPU_READBACK ) {
			m_nReadbackMappedBytes += pBuffer->nAllocationBytes;
		} else {
			m_nUploadMappedBytes += pBuffer->nAllocationBytes;
		}
	}
	++m_nLiveBuffers;
	updateMemoryHighWater();
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::writeBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, const void *pData, uint64_t nBytes ) noexcept
{
	if ( !m_bReady ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator || !pBuffer->bHostVisible || pData == nullptr || nBytes == 0 ||
	     nOffset > pBuffer->nSize || nBytes > pBuffer->nSize - nOffset || pBuffer->nDeviceSerial != m_nDeviceSerial ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !isBufferIdle( pBuffer ) ) {
		return SIRVulkanResult::SIR_VULKAN_BUSY;
	}
	void *pMapped = nullptr;
	const SIRVulkanResult eMap = mapBuffer( pBuffer, nOffset, nBytes, &pMapped );
	if ( eMap != SIRVulkanResult::SIR_VULKAN_SUCCESS ) return eMap;
	memcpy( pMapped, pData, static_cast<size_t>( nBytes ) );
	const SIRVulkanResult eFlush = flushBuffer( pBuffer, nOffset, nBytes );
	const SIRVulkanResult eUnmap = unmapBuffer( pBuffer );
	return eFlush != SIRVulkanResult::SIR_VULKAN_SUCCESS ? eFlush : eUnmap;
}

SIRVulkanResult SIRVulkanDevice::mapBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, uint64_t nBytes, void **ppData ) noexcept
{
	if ( ppData != nullptr ) *ppData = nullptr;
	if ( !m_bReady || pBuffer == nullptr || ppData == nullptr || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator ||
	     !pBuffer->bHostVisible || pBuffer->nDeviceSerial != m_nDeviceSerial || nOffset > pBuffer->nSize || nBytes > pBuffer->nSize - nOffset ) {
		return !m_bReady ? SIRVulkanResult::SIR_VULKAN_INVALID_STATE : SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !isBufferIdle( pBuffer ) ) return SIRVulkanResult::SIR_VULKAN_BUSY;
	if ( pBuffer->pMappedData == nullptr ) {
		const VkResult eMap = vmaMapMemory( m_Allocator, pBuffer->pAllocation, &pBuffer->pMappedData );
		if ( eMap != VK_SUCCESS || pBuffer->pMappedData == nullptr ) {
			pBuffer->pMappedData = nullptr;
			return eMap == VK_ERROR_OUT_OF_HOST_MEMORY ? SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
		}
		pBuffer->bMapped = true;
	} else if ( !pBuffer->bPersistentlyMapped && pBuffer->bMapped ) {
		return SIRVulkanResult::SIR_VULKAN_BUSY;
	}
	*ppData = static_cast<uint8_t *>( pBuffer->pMappedData ) + nOffset;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::unmapBuffer( sirVulkanBuffer_t *pBuffer ) noexcept
{
	if ( !m_bReady || pBuffer == nullptr || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator ||
	     !pBuffer->bHostVisible || pBuffer->nDeviceSerial != m_nDeviceSerial ) {
		return !m_bReady ? SIRVulkanResult::SIR_VULKAN_INVALID_STATE : SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( pBuffer->bPersistentlyMapped ) return SIRVulkanResult::SIR_VULKAN_SUCCESS;
	if ( pBuffer->bMapped ) {
		vmaUnmapMemory( m_Allocator, pBuffer->pAllocation );
		pBuffer->pMappedData = nullptr;
		pBuffer->bMapped = false;
	}
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::flushBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	if ( !m_bReady || pBuffer == nullptr || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator ||
	     !pBuffer->bHostVisible || pBuffer->nDeviceSerial != m_nDeviceSerial || nOffset > pBuffer->nSize || nBytes > pBuffer->nSize - nOffset ) {
		return !m_bReady ? SIRVulkanResult::SIR_VULKAN_INVALID_STATE : SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( pBuffer->bHostCoherent ) return SIRVulkanResult::SIR_VULKAN_SUCCESS;
	if ( !isBufferIdle( pBuffer ) ) return SIRVulkanResult::SIR_VULKAN_BUSY;
	const VkResult eFlush = vmaFlushAllocation( m_Allocator, pBuffer->pAllocation, nOffset, nBytes == 0 ? VK_WHOLE_SIZE : nBytes );
	return eFlush == VK_SUCCESS ? SIRVulkanResult::SIR_VULKAN_SUCCESS : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
}

SIRVulkanResult SIRVulkanDevice::invalidateBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	if ( !m_bReady || pBuffer == nullptr || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator ||
	     !pBuffer->bHostVisible || pBuffer->nDeviceSerial != m_nDeviceSerial || nOffset > pBuffer->nSize || nBytes > pBuffer->nSize - nOffset ) {
		return !m_bReady ? SIRVulkanResult::SIR_VULKAN_INVALID_STATE : SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( pBuffer->bHostCoherent ) return SIRVulkanResult::SIR_VULKAN_SUCCESS;
	if ( !isBufferIdle( pBuffer ) ) return SIRVulkanResult::SIR_VULKAN_BUSY;
	const VkResult eInvalidate = vmaInvalidateAllocation( m_Allocator, pBuffer->pAllocation, nOffset, nBytes == 0 ? VK_WHOLE_SIZE : nBytes );
	return eInvalidate == VK_SUCCESS ? SIRVulkanResult::SIR_VULKAN_SUCCESS : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
}

SIRVulkanResult SIRVulkanDevice::getBufferDeviceAddress( const sirVulkanBuffer_t *pBuffer, uint64_t *pAddress ) const noexcept
{
	if ( pAddress != nullptr ) *pAddress = 0;
	if ( !m_bReady || pBuffer == nullptr || pAddress == nullptr || pBuffer->hBuffer == VK_NULL_HANDLE || pBuffer->pOwnerAllocator != m_Allocator ||
	     pBuffer->nDeviceSerial != m_nDeviceSerial || ( pBuffer->nUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ) == 0 ||
	     m_Procs.vkGetBufferDeviceAddress == nullptr ) {
		return !m_bReady ? SIRVulkanResult::SIR_VULKAN_INVALID_STATE : SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	VkBufferDeviceAddressInfo info{};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	info.buffer = pBuffer->hBuffer;
	*pAddress = m_Procs.vkGetBufferDeviceAddress( m_Device, &info );
	return *pAddress != 0 ? SIRVulkanResult::SIR_VULKAN_SUCCESS : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
}

SIRVulkanResult SIRVulkanDevice::getImageSubresourceLayout( const sirVulkanImage_t *pImage, const VkImageSubresource& subresource,
                                                            VkSubresourceLayout *pLayout ) const noexcept
{
	if ( pLayout != nullptr ) *pLayout = {};
	if ( !m_bReady || m_bDeviceLost || pImage == nullptr || pLayout == nullptr || pImage->hImage == VK_NULL_HANDLE || pImage->pOwnerAllocator != m_Allocator ||
	     pImage->nDeviceSerial != m_nDeviceSerial || pImage->eTiling != VK_IMAGE_TILING_LINEAR || m_Procs.vkGetImageSubresourceLayout == nullptr || subresource.aspectMask == 0 ||
	     ( subresource.aspectMask & ~sirVulkanImageAspectMask( pImage->eFormat ) ) != 0 || subresource.mipLevel >= pImage->nMips ||
	     subresource.arrayLayer >= pImage->nLayers ) {
		return !m_bReady ? SIRVulkanResult::SIR_VULKAN_INVALID_STATE : SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	m_Procs.vkGetImageSubresourceLayout( m_Device, pImage->hImage, &subresource, pLayout );
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::destroyIdleBuffer( sirVulkanBuffer_t *pBuffer ) noexcept
{
	if ( !m_bReady ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pBuffer->hBuffer == VK_NULL_HANDLE || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator || m_nLiveBuffers == 0 ||
	     pBuffer->nDeviceSerial != m_nDeviceSerial ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !isBufferIdle( pBuffer ) ) {
		return SIRVulkanResult::SIR_VULKAN_BUSY;
	}
	if ( pBuffer->bHostVisible ) {
		if ( pBuffer->ePlacement == SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_CPU_READBACK ) {
			m_nReadbackMappedBytes -= pBuffer->nAllocationBytes;
		} else {
			m_nUploadMappedBytes -= pBuffer->nAllocationBytes;
		}
	}
	if ( pBuffer->bMapped && !pBuffer->bPersistentlyMapped ) {
		vmaUnmapMemory( m_Allocator, pBuffer->pAllocation );
	}
	vmaDestroyBuffer( m_Allocator, pBuffer->hBuffer, pBuffer->pAllocation );
	memset( pBuffer, 0, sizeof( *pBuffer ) );
	--m_nLiveBuffers;

	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}
