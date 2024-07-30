#include "VKShaderBuffer.h"

extern CVar<uint32_t> e_MaxFPS;

VKShaderBuffer::VKShaderBuffer( VKPipelineSet_t *pPipeline, uint32_t nProgramBinding )
    : VKBuffer( BUFFER_TYPE_UNIFORM ), m_pPipeline( pPipeline )
{
    uint32_t i;
    VKBuffer *pBuffer;
    VkDescriptorSetLayout *pLayouts;

    switch ( szDefaultUniforms[nProgramBinding].nType ) {
    case UniformType_Buffer:
        m_nType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        break;
    case UniformType_Sampler:
        m_nType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        break;
    };

    m_nProgramBinding = szDefaultUniforms[nProgramBinding].nIndex;

    pLayouts = (VkDescriptorSetLayout *)alloca( sizeof( *pLayouts ) * e_MaxFPS.GetValue() );

    VkDescriptorSetAllocateInfo allocInfo;
    memset( &allocInfo, 0, sizeof( allocInfo ) );
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pPipeline->hDescriptorPool;
    allocInfo.descriptorSetCount = e_MaxFPS.GetValue();
    allocInfo.pSetLayouts = pLayouts;

    m_DescriptorSets.Resize( e_MaxFPS.GetValue() );
    if ( vkAllocateDescriptorSets( g_pVKContext->GetDevice(), &allocInfo, m_DescriptorSets.GetBuffer() ) != VK_SUCCESS ) {

    }

    m_ShaderBuffers.Reserve( e_MaxFPS.GetValue() );

    for ( i = 0; i < e_MaxFPS.GetValue(); i++ ) {
        pBuffer = new VKBuffer( BUFFER_TYPE_UNIFORM );
        m_ShaderBuffers.Push( pBuffer );

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
    }
}

VKShaderBuffer::~VKShaderBuffer()
{
}

void VKShaderBuffer::SwapData( VKPipelineSet_t *pSet )
{
    uint32_t i;

    for ( i = 0; i < e_MaxFPS.GetValue(); i++ ) {
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