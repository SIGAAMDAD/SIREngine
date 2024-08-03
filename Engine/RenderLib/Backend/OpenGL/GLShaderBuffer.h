#ifndef __GL_SHADERBUFFER_H__
#define __GL_SHADERBUFFER_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderShaderBuffer.h"
#include "GLBuffer.h"

class GLShaderBuffer : public IRenderShaderBuffer
{
public:
    GLShaderBuffer( GLPipelineSet_t *pPipeline, uint32_t nProgramBinding );
    virtual ~GLShaderBuffer() override;

    void SwapData( GLPipelineSet_t *pSet ) {}
private:
    GLBuffer *m_pBuffer;
    GLPipelineSet_t *m_pPipeline;
};

#endif