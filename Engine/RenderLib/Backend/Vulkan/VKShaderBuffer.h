#ifndef __VK_SHADERBUFFER_H__
#define __VK_SHADERBUFFER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"
#include "../RenderShaderBuffer.h"
#include "VKBuffer.h"

namespace SIREngine::RenderLib::Backend::Vulkan {
    class VKShaderBuffer : public Backend::IRenderShaderBuffer
    {
    public:
        VKShaderBuffer( VKPipelineSet_t *pPipeline, uint32_t nProgramBinding,
            VkDescriptorSetLayoutBinding *pLayoutBindings );
        virtual ~VKShaderBuffer() override;

        SIRENGINE_FORCEINLINE VkBuffer GetVKObject( void )
        { return m_pBuffer->GetVKObject(); }

        SIRENGINE_FORCEINLINE CVector<VkDescriptorSet>& GetDescriptorSets( void )
        { return m_DescriptorSets; }

        void SwapData( VKPipelineSet_t *pSet );
    private:
        VKBuffer *m_pBuffer;
        CVector<VkDescriptorSet> m_DescriptorSets;
        CVector<VKBuffer *> m_ShaderBuffers;
        VkDescriptorType m_nType;

        VKPipelineSet_t *m_pPipeline;
    };
};

#endif