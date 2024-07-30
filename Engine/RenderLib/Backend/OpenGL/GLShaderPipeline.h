#ifndef __GL_SHADERPIPELINE_H__
#define __GL_SHADERPIPELINE_H__

#pragma once

#include "../RenderCommon.h"
#include "GLCommon.h"
#include "GLProgram.h"
#include "GLVertexArray.h"
#include "GLShaderBuffer.h"

typedef struct {
    GLProgram *pShader;
    uint32_t nEnabledAttribs;

    CVector<GLShaderBuffer *> dataInputBuffers;
} GLPipelineSet_t;

class GLShaderPipeline : public IRenderShaderPipeline
{
public:
    GLShaderPipeline( void );
    virtual ~GLShaderPipeline() override;

    virtual uint64_t AddVertexAttribSet( const VertexInputDescription_t& vertexInput ) override;
    virtual void SetShaderInputState( uint64_t nCacheID ) override;
    virtual void ResetPipelineState( void ) override;

    SIRENGINE_FORCEINLINE GLPipelineSet_t *GetPipelineData( void )
    { return &m_PipelineCache[ m_nUsedPipeline ]; }
private:
    void UpdateVertexPointers( const GLPipelineSet_t *pSet );

    CVector<GLPipelineSet_t> m_PipelineCache;

    GLProgram *m_pDefaultShader;
    GLVertexArray *m_pPipelineArray;

    uint32_t m_nEnabledAttribs;
};

#endif