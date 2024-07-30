#ifndef __VK_SHADERPIPELINE_H__
#define __VK_SHADERPIPELINE_H__

#pragma once

#include "../RenderCommon.h"
#include "VKVertexArray.h"
#include "VKCommon.h"
#include "VKProgram.h"
#include "VKShaderBuffer.h"

typedef struct {
    VkPipeline hPipeline;
    VkPipelineLayout hLayout;
    VkDescriptorPool hDescriptorPool;
    VkDescriptorSetLayout hDescriptorSetLayout;

    CVector<VKShaderBuffer *> descriptorSets;

    VkVertexInputAttributeDescription *pAttributes;
    VkVertexInputBindingDescription *pBindings;

    uint32_t nAttribCount;
    uint32_t nBindingCount;
} VKPipelineSet_t;

class VKShaderPipeline : public IRenderShaderPipeline
{
public:
    VKShaderPipeline( void );
    virtual ~VKShaderPipeline() override;

    virtual uint64_t AddVertexAttribSet( const VertexInputDescription_t& vertexInput ) override;
    virtual void SetShaderInputState( uint64_t nCacheID ) override;

    void ClearPipelineCache( void );
private:
    void AllocateUniformBufferLayout( VKPipelineSet_t *pSet );

    void LoadVulkanPipelineCache( void );
    void SaveVulkanPipelineCache( void );
    uint64_t CheckVulkanCacheForSet( const VertexInputDescription_t& vertexInput );

    CVector<VKPipelineSet_t *> m_PipelineCache;

    VkPipelineCache m_hVulkanDataCache;
    VkSampler m_Samplers[4];
};

#endif