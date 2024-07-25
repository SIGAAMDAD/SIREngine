#include "VKBuffer.hpp"

VKBuffer::VKBuffer( gpuBufferType_t nType )
    : IRenderBuffer( nType )
{
    Init();
}

VKBuffer::~VKBuffer()
{
}

void VKBuffer::Init( void )
{
    {
        VkBufferCreateInfo bufferInfo;
        memset( &bufferInfo, 0, sizeof( bufferInfo ) );
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

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
        };

        VmaAllocationCreateInfo allocInfo;
        memset( &allocInfo, 0, sizeof( allocInfo ) );
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if ( vmaCreateBuffer( g_pVKContext->GetAllocator(), &bufferInfo, &allocInfo, &m_pBuffer, &m_Allocation, NULL ) != VK_SUCCESS ) {

        }
        
        if ( vmaMapMemory( g_pVKContext->GetAllocator(), m_Allocation, &m_pMappedGPUBuffer ) != VK_SUCCESS ) {

        }
    }
}

void VKBuffer::Shutdown( void )
{
    if ( m_pBuffer != NULL ) {
        vmaDestroyBuffer( g_pVKContext->GetAllocator(), m_pBuffer, m_Allocation );
        vmaUnmapMemory( g_pVKContext->GetAllocator(), m_Allocation );
    }
}
