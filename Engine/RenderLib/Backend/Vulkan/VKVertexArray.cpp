#include "VKVertexArray.h"

VKVertexArray::VKVertexArray( void )
{
}

VKVertexArray::~VKVertexArray()
{
}

void VKVertexArray::SetVertexAttribs( const VertexAttribInfo_t *pVertexAttribs, uint64_t nAttribCount )
{
    VkDescriptorSetLayoutBinding layoutBinding;
    memset( &layoutBinding, 0, sizeof( layoutBinding ) );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    memset( &vertexInputInfo, 0, sizeof( vertexInputInfo ) );
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    memset( &layoutInfo, 0, sizeof( layoutInfo ) );
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
}