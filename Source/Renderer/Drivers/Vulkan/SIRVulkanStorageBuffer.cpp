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
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_VERTEX ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_INDEX ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_CONSTANT ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_STORAGE ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_TEXEL ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_INDIRECT ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_TRANSFERSOURCE ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_TRANSFERDESTINATION ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_ACCELERATIONSTRUCTURE ) | static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_SHADERBINDINGTABLE ) |
	    static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_DEVICEADDRESS );
	if ( ( usage & ~nKnownUsage ) != 0 ) {
		return 0;
	}

	VkBufferUsageFlags result = 0;
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_VERTEX ) ) {
		result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_INDEX ) ) {
		result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_CONSTANT ) ) {
		result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_STORAGE ) ) {
		result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_TEXEL ) ) {
		result |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_INDIRECT ) ) {
		result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_TRANSFERSOURCE ) ) {
		result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_TRANSFERDESTINATION ) ) {
		result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_ACCELERATIONSTRUCTURE ) ) {
		result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_SHADERBINDINGTABLE ) ) {
		result |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
	}
	if ( usage & static_cast<uint64_t>( SIRRenderBufferUsage::SIR_RENDER_DEVICEADDRESS ) ) {
		result |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	}
	return result;
}

SIRVulkanStorageBuffer::SIRVulkanStorageBuffer( const SIRSharedPtr<SIRRenderDevice>& device, const sirRenderBufferDescription_t& description ) noexcept
    : SIRRenderStorageBuffer( device, description, 0 )
{
	if ( device.get() == nullptr ) {
		return;
	}
	if ( description.nSize == 0 || description.nUsage == 0 ) {
		return;
	}

	SIRVulkanDevice& nativeDevice = static_cast<SIRVulkanDevice&>( *device );
	SIRVulkanBufferPlacement placement = SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_GPU_LOCAL;
	if ( description.bHostVisible || description.eMemory != SIRRenderMemoryType::SIR_RENDER_DEVICELOCAL ) {
		placement = description.eMemory == SIRRenderMemoryType::SIR_RENDER_HOSTCACHED ? SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_CPU_READBACK
		                                                                              : SIRVulkanBufferPlacement::SIR_VULKAN_BUFFER_CPU_UPLOAD;
	}

	const VkBufferUsageFlags usage = sirVulkanBufferUsage( description.nUsage );
	if ( usage == 0 ) {
		return;
	}
	if ( nativeDevice.createBuffer( description.nSize, usage, placement, &m_Native, description.bPersistentlyMapped ) == SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
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

	SIRVulkanDevice& nativeDevice = static_cast<SIRVulkanDevice&>( *getDevice() );
	if ( nativeDevice.waitIdle() == SIRRenderResult::SIR_RENDER_SUCCESS ) {
		static_cast<void>( nativeDevice.unmapBuffer( &m_Native ) );
		static_cast<void>( nativeDevice.destroyIdleBuffer( &m_Native ) );
	}
}

SIRRenderResult SIRVulkanStorageBuffer::map( uint64_t nOffsetBytes, uint64_t nSizeBytes, sirRenderBufferMapping_t *pMapping ) noexcept
{
	if ( pMapping == nullptr || nOffsetBytes > getSize() || nSizeBytes > getSize() - nOffsetBytes || !isHostVisible() || m_Native.hBuffer == VK_NULL_HANDLE ||
	     getDevice().get() == nullptr ) {
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}
	void *pData = nullptr;

	const SIRVulkanResult result = static_cast<SIRVulkanDevice&>( *getDevice() ).mapBuffer( &m_Native, nOffsetBytes, nSizeBytes, &pData );
	if ( result != SIRVulkanResult::SIR_VULKAN_SUCCESS || pData == nullptr ) {
		return result == SIRVulkanResult::SIR_VULKAN_BUSY ? SIRRenderResult::SIR_RENDER_BUSY : SIRRenderResult::SIR_RENDER_BACKENDERROR;
	}

	pMapping->bytes = SIRSpan<uint8_t>( static_cast<uint8_t *>( pData ), nSizeBytes );
	pMapping->nOffset = nOffsetBytes;

	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanStorageBuffer::unmap( void ) noexcept
{
	if ( getDevice().get() == nullptr || m_Native.hBuffer == VK_NULL_HANDLE ) {
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}
	const SIRVulkanResult result = static_cast<SIRVulkanDevice&>( *getDevice() ).unmapBuffer( &m_Native );
	return result == SIRVulkanResult::SIR_VULKAN_SUCCESS ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_BACKENDERROR;
}

SIRRenderResult SIRVulkanStorageBuffer::flush( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept
{
	if ( nOffsetBytes > getSize() || nSizeBytes > getSize() - nOffsetBytes || getDevice().get() == nullptr ) {
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}

	const SIRVulkanResult result = static_cast<SIRVulkanDevice&>( *getDevice() ).flushBuffer( &m_Native, nOffsetBytes, nSizeBytes );

	if ( result == SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		return SIRRenderResult::SIR_RENDER_SUCCESS;
	}
	if ( result == SIRVulkanResult::SIR_VULKAN_BUSY ) {
		return SIRRenderResult::SIR_RENDER_BUSY;
	}
	return SIRRenderResult::SIR_RENDER_BACKENDERROR;
}

SIRRenderResult SIRVulkanStorageBuffer::invalidate( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept
{
	if ( nOffsetBytes > getSize() || nSizeBytes > getSize() - nOffsetBytes || getDevice().get() == nullptr ) {
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}

	const SIRVulkanResult result = static_cast<SIRVulkanDevice&>( *getDevice() ).invalidateBuffer( &m_Native, nOffsetBytes, nSizeBytes );
	return result == SIRVulkanResult::SIR_VULKAN_SUCCESS ? SIRRenderResult::SIR_RENDER_SUCCESS
	       : result == SIRVulkanResult::SIR_VULKAN_BUSY  ? SIRRenderResult::SIR_RENDER_BUSY
	                                                     : SIRRenderResult::SIR_RENDER_BACKENDERROR;
}

uint64_t SIRVulkanStorageBuffer::getDeviceAddress( void ) const noexcept
{
	if ( getDevice().get() == nullptr )
		return 0;
	uint64_t address = 0;
	return static_cast<SIRVulkanDevice&>( *getDevice() ).getBufferDeviceAddress( &m_Native, &address ) == SIRVulkanResult::SIR_VULKAN_SUCCESS ? address : 0;
}
