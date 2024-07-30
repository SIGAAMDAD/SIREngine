#ifndef __VK_SHADERBUFFER_H__
#define __VK_SHADERBUFFER_H__

#pragma once

#include "../RenderShaderBuffer.h"
#include "VKBuffer.h"
#include "VKShaderPipeline.h"

class VKShaderBuffer : public IRenderShaderBuffer, VKBuffer
{
public:
    VKShaderBuffer( VKPipelineSet_t *pPipeline, uint32_t nProgramBinding );
    virtual ~VKShaderBuffer() override;

    SIRENGINE_FORCEINLINE VkBuffer GetVKObject( void )
    { return m_pBuffer; }

    SIRENGINE_FORCEINLINE CVector<VkDescriptorSet>& GetDescriptorSets( void )
    { return m_DescriptorSets; }

    virtual void SwapData( VKPipelineSet_t *pSet ) override;
private:
    CVector<VkDescriptorSet> m_DescriptorSets;
    CVector<VKBuffer *> m_ShaderBuffers;
    VkDescriptorType m_nType;

    VKPipelineSet_t *m_pPipeline;
};

#endif