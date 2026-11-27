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

#include "SIRVulkanStorageBuffer.h"
#include "Renderer/Drivers/Vulkan/SIRVulkanDevice.h"
#include "Renderer/RHI/SIRRenderTypes.h"

static VkBufferUsageFlags sirVulkanBufferUsage( uint64_t usage ) noexcept
{
	constexpr uint64_t nKnownUsage =
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_VERTEX ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_INDEX ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_CONSTANT ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_STORAGE ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_TEXEL ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_INDIRECT ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_TRANSFER_SOURCE ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_TRANSFER_DESTINATION ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_ACCELERATION_STRUCTURE ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_SHADER_BINDING_TABLE ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_DEVICE_ADDRESS );
	if ( ( usage & ~nKnownUsage ) != 0 ) {
		return 0;
	}

	VkBufferUsageFlags result = 0;
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_VERTEX ) ) {
		result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_INDEX ) ) {
		result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_CONSTANT ) ) {
		result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_STORAGE ) ) {
		result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_TEXEL ) ) {
		result |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_INDIRECT ) ) {
		result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_TRANSFER_SOURCE ) ) {
		result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_TRANSFER_DESTINATION ) ) {
		result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_ACCELERATION_STRUCTURE ) ) {
		result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_SHADER_BINDING_TABLE ) ) {
		result |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_BUFFER_DEVICE_ADDRESS ) ) {
		result |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	}
	return result;
}

SIRVulkanStorageBuffer::SIRVulkanStorageBuffer( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderBufferDesc& description ) noexcept
    : SIRRenderStorageBuffer( device, description, 0 )
{
	if ( device.get() == nullptr || device->getBackend() != SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN ) {
		return;
	}
	if ( description.nSizeBytes == 0 || description.nUsageMask == 0 ) {
		return;
	}

	SIRVulkanDevice& nativeDevice = getDevice<SIRVulkanDevice>();
	SIRVulkanBufferPlacement placement = SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_GPU_LOCAL;
	if ( ( description.nMemoryPropertyMask & static_cast<uint32_t>( SIRRenderMemoryProperty::SIR_RENDER_MEMORY_PROPERTY_HOST_VISIBLE ) ) != 0 ||
	     description.eMemory != SIRRenderMemoryClass::SIR_RENDER_MEMORY_GPU_LOCAL ) {
		placement = description.eMemory == SIRRenderMemoryClass::SIR_RENDER_MEMORY_CPU_READBACK ? SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_CPU_READBACK
		                                                                              : SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_CPU_UPLOAD;
	}

	const VkBufferUsageFlags usage = sirVulkanBufferUsage( description.nUsageMask );
	if ( usage == 0 ) {
		return;
	}
	const bool bPersistent = ( description.nMemoryPropertyMask & static_cast<uint32_t>( SIRRenderMemoryProperty::SIR_RENDER_MEMORY_PROPERTY_PERSISTENT_MAPPING ) ) != 0;
	if ( nativeDevice.createBuffer( description.nSizeBytes, usage, placement, &m_Native, bPersistent ) == SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		m_nCommittedBytes = m_Native.nAllocationBytes;
		m_bHostVisible = m_Native.bHostVisible;
		m_bHostCoherent = m_Native.bHostCoherent;
		m_bPersistentlyMapped = m_Native.bPersistentlyMapped;
	}
}

SIRVulkanStorageBuffer::~SIRVulkanStorageBuffer() noexcept
{
	if ( m_Native.hBuffer == VK_NULL_HANDLE || getDevice().get() == nullptr ) {
		return;
	}

	SIRVulkanDevice& nativeDevice = getDevice<SIRVulkanDevice>();
	if ( m_Native.nLastGpuUse != 0 ) {
		const sirVulkanCompletion_t completion{ m_Native.nDeviceSerial, m_Native.nLastGpuUse };
		if ( nativeDevice.waitCompletion( &completion, UINT64_MAX ) != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			return;
		}
	}
	static_cast<void>( nativeDevice.unmapBuffer( &m_Native ) );
	static_cast<void>( nativeDevice.destroyIdleBuffer( &m_Native ) );
}

SIRRenderResult SIRVulkanStorageBuffer::map( uint64_t nOffsetBytes, uint64_t nSizeBytes, SIRRenderBufferMapping *pMapping ) noexcept
{
	if ( pMapping == nullptr || nOffsetBytes > getSize() || nSizeBytes > getSize() - nOffsetBytes || !isHostVisible() || m_Native.hBuffer == VK_NULL_HANDLE ||
	     getDevice().get() == nullptr ) {
		return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	}
	void *pData = nullptr;

	const SIRVulkanResult result = static_cast<SIRVulkanDevice&>( *getDevice() ).mapBuffer( &m_Native, nOffsetBytes, nSizeBytes, &pData );
	if ( result != SIRVulkanResult::SIR_VULKAN_SUCCESS || pData == nullptr ) {
		return result == SIRVulkanResult::SIR_VULKAN_BUSY ? SIRRenderResult::SIR_RENDER_BUSY : SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
	}

	pMapping->bytes = SIRSpan<uint8_t>( static_cast<uint8_t *>( pData ), nSizeBytes );
	pMapping->nOffsetBytes = nOffsetBytes;

	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanStorageBuffer::unmap( void ) noexcept
{
	if ( getDevice().get() == nullptr || m_Native.hBuffer == VK_NULL_HANDLE ) {
		return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	}
	const SIRVulkanResult result = static_cast<SIRVulkanDevice&>( *getDevice() ).unmapBuffer( &m_Native );
	return result == SIRVulkanResult::SIR_VULKAN_SUCCESS ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
}

SIRRenderResult SIRVulkanStorageBuffer::flush( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept
{
	if ( nOffsetBytes > getSize() || nSizeBytes > getSize() - nOffsetBytes || getDevice().get() == nullptr ) {
		return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	}

	const SIRVulkanResult result = static_cast<SIRVulkanDevice&>( *getDevice() ).flushBuffer( &m_Native, nOffsetBytes, nSizeBytes );

	if ( result == SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		return SIRRenderResult::SIR_RENDER_SUCCESS;
	}
	if ( result == SIRVulkanResult::SIR_VULKAN_BUSY ) {
		return SIRRenderResult::SIR_RENDER_BUSY;
	}
	return SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
}

SIRRenderResult SIRVulkanStorageBuffer::invalidate( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept
{
	if ( nOffsetBytes > getSize() || nSizeBytes > getSize() - nOffsetBytes || getDevice().get() == nullptr ) {
		return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	}

	const SIRVulkanResult result = static_cast<SIRVulkanDevice&>( *getDevice() ).invalidateBuffer( &m_Native, nOffsetBytes, nSizeBytes );
	return result == SIRVulkanResult::SIR_VULKAN_SUCCESS ? SIRRenderResult::SIR_RENDER_SUCCESS
	       : result == SIRVulkanResult::SIR_VULKAN_BUSY  ? SIRRenderResult::SIR_RENDER_BUSY
	                                                     : SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
}

uint64_t SIRVulkanStorageBuffer::getDeviceAddress( void ) const noexcept
{
	if ( getDevice().get() == nullptr )
		return 0;
	uint64_t address = 0;
	return static_cast<SIRVulkanDevice&>( *getDevice() ).getBufferDeviceAddress( &m_Native, &address ) == SIRVulkanResult::SIR_VULKAN_SUCCESS ? address : 0;
}
