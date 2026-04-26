#include "VulkanBuffer.h"

using namespace SIREngine::Rendering::Vulkan;

VulkanBuffer::VulkanBuffer( const SVulkanInstance& instance, size_t nSize, EStreamType eUsage, EBufferType eType, const void *pData )
	: m_Instance( instance )
{
	VkBufferCreateInfo createInfo;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	createInfo.size = nSize;

	VmaAllocationCreateInfo allocInfo;
	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	VmaAllocationCreateFlagBits flags;
	switch ( eUsage ) {
		case EStreamType::FlushAndForget:
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			break;
		case EStreamType::Persistent:
			allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			break;
	}

	vmaCreateBuffer( m_Instance.Allocator, &createInfo, &allocInfo, &m_hBuffer, &m_Allocation, NULL );
}

VulkanBuffer::~VulkanBuffer()
{
	if ( m_Allocation != NULL ) {
		vmaDestroyBuffer( m_Instance.Allocator, m_hBuffer, m_Allocation );
	}
}

void VulkanBuffer::Update( size_t nOffset, size_t nSize, const void *pData )
{
	if ( m_eUsage != EStreamType::Persistent ) {
		return;
	}
}

void VulkanBuffer::Bind( void )
{
}

void VulkanBuffer::Unbind( void )
{
}

void *VulkanBuffer::Map( EBufferMapAccess eAccess )
{
	VkResult result = vmaMapMemory( m_Instance.Allocator, m_Allocation, &m_pData );
	if ( result != VK_SUCCESS ) {
		printf( "Error in vmaMapMemory: %i\n", result );
	}
	return m_pData;
}

void VulkanBuffer::Unmap( void )
{
	if ( m_pData == NULL ) {
		return;
	}
	vmaUnmapMemory( m_Instance.Allocator, m_Allocation );
	m_pData = NULL;
}

size_t VulkanBuffer::GetSize( void ) const
{
	return m_nSize;
}