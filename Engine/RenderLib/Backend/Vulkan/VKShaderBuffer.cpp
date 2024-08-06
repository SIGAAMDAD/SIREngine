#include "VKShaderBuffer.h"

VKShaderBuffer::VKShaderBuffer(
    VKPipelineSet_t *pPipeline, uint32_t nProgramBinding,
    VkDescriptorSetLayoutBinding *pLayoutBindings
)
    : m_pPipeline( pPipeline )
{
    uint32_t i;
    VKBuffer *pBuffer;
    VkDescriptorSetLayout *pLayouts;

    switch ( szDefaultUniforms[nProgramBinding].nType ) {
    case UniformType_StaticBuffer:
        m_nType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        break;
    case UniformType_DynamicBuffer:
        m_nType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        break;
    case UniformType_Sampler:
        m_nType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        break;
    };

    m_nProgramBinding = szDefaultUniforms[nProgramBinding].nIndex;

    m_ShaderBuffers.reserve( VK_MAX_FRAMES_IN_FLIGHT );

    for ( i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ ) {
        pBuffer = dynamic_cast<VKBuffer *>( IRenderBuffer::Create( BUFFER_TYPE_UNIFORM, BufferUsage_Stream, 2*1024 ) );
        m_ShaderBuffers.emplace_back( pBuffer );

    /*
        VkWriteDescriptorSet descriptorWrite;
        memset( &descriptorWrite, 0, sizeof( descriptorWrite ) );
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_DescriptorSets[i];
        descriptorWrite.dstBinding = szDefaultUniforms[i].nIndex;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = m_nType;
        descriptorWrite.descriptorCount = 1;

        switch ( szDefaultUniforms[i].nType ) {
        case UniformType_Buffer: {
            static VkDescriptorBufferInfo bufferInfo;
            memset( &bufferInfo, 0, sizeof( bufferInfo ) );
            bufferInfo.buffer = pBuffer->GetVKObject();
            bufferInfo.offset = 0;
            bufferInfo.range = szDefaultUniforms[i].nSize;

            descriptorWrite.pBufferInfo = &bufferInfo;
            break; }
        case UniformType_Sampler: {
            static VkDescriptorImageInfo imageInfo;
            memset( &imageInfo, 0, sizeof( imageInfo ) );
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = VK_NULL_HANDLE;
            imageInfo.sampler = VK_NULL_HANDLE;

            descriptorWrite.pImageInfo = &imageInfo;
            break; }
        };

        vkUpdateDescriptorSets( g_pVKContext->GetDevice(), 1, &descriptorWrite, 0, NULL );
        */
    }
}

VKShaderBuffer::~VKShaderBuffer()
{
}

void VKShaderBuffer::SwapData( VKPipelineSet_t *pSet )
{
    uint32_t i;

    for ( i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ ) {
        VkWriteDescriptorSet descriptorWrite;
        memset( &descriptorWrite, 0, sizeof( descriptorWrite ) );
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_DescriptorSets[i];
        descriptorWrite.dstBinding = szDefaultUniforms[i].nIndex;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = m_nType;
        descriptorWrite.descriptorCount = 1;

        switch ( szDefaultUniforms[i].nType ) {
        case UniformType_DynamicBuffer:
        case UniformType_StaticBuffer: {
            static VkDescriptorBufferInfo bufferInfo;
            memset( &bufferInfo, 0, sizeof( bufferInfo ) );
            bufferInfo.buffer = m_ShaderBuffers[i]->GetVKObject();
            bufferInfo.offset = 0;
            bufferInfo.range = szDefaultUniforms[i].nSize;

            descriptorWrite.pBufferInfo = &bufferInfo;
            break; }
        case UniformType_Sampler: {
            static VkDescriptorImageInfo imageInfo;
            memset( &imageInfo, 0, sizeof( imageInfo ) );
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = VK_NULL_HANDLE;
            imageInfo.sampler = VK_NULL_HANDLE;

            descriptorWrite.pImageInfo = &imageInfo;
            break; }
        };

        vkUpdateDescriptorSets( g_pVKContext->GetDevice(), 1, &descriptorWrite, 0, NULL );
    }
}