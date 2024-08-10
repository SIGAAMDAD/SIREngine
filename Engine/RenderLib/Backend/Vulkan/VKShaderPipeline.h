#ifndef __VK_SHADERPIPELINE_H__
#define __VK_SHADERPIPELINE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"
#include "../RenderShaderPipeline.h"
#include "VKBuffer.h"

class VKShaderPipeline : public IRenderShaderPipeline
{
public:
    VKShaderPipeline( void );
    virtual ~VKShaderPipeline() override;

    virtual uint64_t AddVertexAttribSet( const VertexInputDescription_t& vertexInput ) override;
    virtual void SetShaderInputState( uint64_t nCacheID ) override;
    virtual void ResetPipelineState( void ) override;

    SIRENGINE_FORCEINLINE VKPipelineSet_t *GetPipelineData( void )
    { return m_PipelineCache[ m_nUsedPipeline ]; }

    void ClearPipelineCache( void );
private:
    void AllocateUniformBufferLayout( VKPipelineSet_t *pSet, const VertexInputDescription_t& vertexInput );

    void LoadVulkanPipelineCache( void );
    void SaveVulkanPipelineCache( void );

    uint64_t CheckVulkanCacheForSet( const VertexInputDescription_t& vertexInput );

    CVector<VKPipelineSet_t *> m_PipelineCache;

    VKBuffer *m_pVertexBuffer;
    VKBuffer *m_pIndexBuffer;

    VkPipelineCache m_hVulkanDataCache;
    VkSampler m_Samplers[4];
};

#endif