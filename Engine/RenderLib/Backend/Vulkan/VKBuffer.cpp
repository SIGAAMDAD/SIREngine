#include "VKBuffer.h"
#include "VKContext.h"

static const char *szBufferTypes[] = {
    "VERTEX",
    "INDEX",
    "UNIFORM",
    "TEXTURE",
};

uint32_t GetMemoryType( uint32_t nTypeFilter, VkMemoryPropertyFlags props )
{
    uint32_t i;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties( g_pVKContext->GetPhysicalDevice(), &memProperties );

    for ( i = 0; i < memProperties.memoryTypeCount; i++ ) {
        if ( ( nTypeFilter & SIRENGINE_BIT( i ) ) && ( memProperties.memoryTypes[i].propertyFlags & props ) == props ) {
            return i;
        }
    }

    SIRENGINE_ERROR( "VKContext::GetMemoryType: couldn't find a suitable Vulkan Buffer Memory type!" );
    return 0;
}

VKBuffer::VKBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize )
    : IRenderBuffer( nType, nUsage )
{
    m_pMappedGPUBuffer = NULL;
    m_BufferName = SIRENGINE_TEMP_VSTRING( "VulkanBuffer_%s", szBufferTypes[ nType ] );
    m_nType = nType;
    Init( NULL, nSize );
}

VKBuffer::~VKBuffer()
{
    Shutdown();
}

void VKBuffer::Init( const void *pBuffer, uint64_t nSize )
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

    VK_CALL( vmaCreateBuffer( g_pVKContext->GetAllocator(), &bufferInfo, &allocInfo, &m_pBuffer, &m_Allocation, NULL ) );

    vmaSetAllocationName( g_pVKContext->GetAllocator(), m_Allocation, m_BufferName.c_str() );
}

void VKBuffer::Shutdown( void )
{
    if ( m_pBuffer != NULL ) {
        if ( m_pMappedGPUBuffer ) { 
            UnmapGPUBuffer();
        }
        vmaDestroyBuffer( g_pVKContext->GetAllocator(), m_pBuffer, m_Allocation );
    }
}
