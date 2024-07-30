#include "GLShaderBuffer.h"

GLShaderBuffer::GLShaderBuffer( GLPipelineSet_t *pPipeline, uint32_t nProgramBinding )
    : GLBuffer( BUFFER_TYPE_UNIFORM ), m_pPipeline( pPipeline )
{
    GLProgram *program;

    m_nProgramBinding = nProgramBinding;

    program = pPipeline->pShader;

    nglBindBuffer( m_nBufferTarget, m_hBufferID );
    nglUniformBlockBinding( program->GetProgramID(), nProgramBinding, pPipeline->dataInputBuffers.Size() );
    nglBindBufferRange( m_nBufferTarget, 0, m_hBufferID, 0, m_nBufferSize );
    nglBindBufferBase( m_nBufferTarget, 0, m_hBufferID );
    nglBindBuffer( m_nBufferTarget, 0 );

    pPipeline->dataInputBuffers.Push( this );
}

GLShaderBuffer::~GLShaderBuffer()
{
}

