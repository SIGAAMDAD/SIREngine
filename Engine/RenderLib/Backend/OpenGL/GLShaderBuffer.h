#ifndef __GL_SHADERBUFFER_H__
#define __GL_SHADERBUFFER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderShaderBuffer.h"
#include "GLBuffer.h"

namespace SIREngine::RenderLib::Backend::OpenGL {
    class GLShaderBuffer : public Backend::IRenderShaderBuffer
    {
    public:
        GLShaderBuffer( GLPipelineSet_t *pPipeline, uint32_t nProgramBinding );
        virtual ~GLShaderBuffer() override;

        void SwapData( GLPipelineSet_t *pSet ) {}
    private:
        GLBuffer *m_pBuffer;
        GLPipelineSet_t *m_pPipeline;
    };
};

#endif