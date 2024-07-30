#include "VKBuffer.h"

VKBuffer::VKBuffer( GPUBufferType_t nType )
{
    m_nType = nType;
    Init();
}

VKBuffer::VKBuffer( GPUBufferType_t nType, uint64_t nSize )
{
    m_nType = nType;
    Init( nSize );
}

VKBuffer::~VKBuffer()
{
    Shutdown();
}

void VKBuffer::Init( void )
{
    {
        VkBufferCreateInfo bufferInfo;
        memset( &bufferInfo, 0, sizeof( bufferInfo ) );
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.size = 1*1024*1024;

        m_nBufferSize = bufferInfo.size;

        switch ( m_nType ) {
        case BUFFER_TYPE_VERTEX:
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        case BUFFER_TYPE_INDEX:
            bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case BUFFER_TYPE_UNIFORM:
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
        case BUFFER_TYPE_TEXTURE:
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
        };

        VmaAllocationCreateInfo allocInfo;
        memset( &allocInfo, 0, sizeof( allocInfo ) );
        if ( m_nType == BUFFER_TYPE_TEXTURE ) {
            // create a specialized staging buffer for image upload
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        } else {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        }

        if ( vmaCreateBuffer( g_pVKContext->GetAllocator(), &bufferInfo, &allocInfo, &m_pBuffer, &m_Allocation, NULL ) != VK_SUCCESS ) {

        }
    }
}

void VKBuffer::Init( uint64_t nSize )
{
    {
        VkBufferCreateInfo bufferInfo;
        memset( &bufferInfo, 0, sizeof( bufferInfo ) );
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.size = nSize;

        m_nBufferSize = bufferInfo.size;

        switch ( m_nType ) {
        case BUFFER_TYPE_VERTEX:
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        case BUFFER_TYPE_INDEX:
            bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case BUFFER_TYPE_UNIFORM:
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
        case BUFFER_TYPE_TEXTURE:
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
        };

        VmaAllocationCreateInfo allocInfo;
        memset( &allocInfo, 0, sizeof( allocInfo ) );
        if ( m_nType == BUFFER_TYPE_TEXTURE ) {
            // create a specialized staging buffer for image upload
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        } else {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        }

        if ( vmaCreateBuffer( g_pVKContext->GetAllocator(), &bufferInfo, &allocInfo, &m_pBuffer, &m_Allocation, NULL ) != VK_SUCCESS ) {

        }
    }
}

void VKBuffer::Shutdown( void )
{
    if ( m_pBuffer != NULL ) {
        UnmapGPUBuffer();
        vmaDestroyBuffer( g_pVKContext->GetAllocator(), m_pBuffer, m_Allocation );
    }
}
