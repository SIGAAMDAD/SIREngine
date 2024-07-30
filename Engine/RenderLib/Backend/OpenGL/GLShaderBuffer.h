#ifndef __GL_SHADERBUFFER_H__
#define __GL_SHADERBUFFER_H__

#pragma once

#include "../RenderShaderBuffer.h"
#include "GLCommon.h"
#include "GLProgram.h"
#include "GLBuffer.h"
#include "GLShaderPipeline.h"

class GLShaderBuffer : public IRenderShaderBuffer, GLBuffer
{
public:
    GLShaderBuffer( GLPipelineSet_t *pPipeline, uint32_t nProgramBinding );
    virtual ~GLShaderBuffer() override;

    void SwapData( GLPipelineSet_t *pSet ) override;
private:
    GLPipelineSet_t *m_pPipeline;
};

#endif