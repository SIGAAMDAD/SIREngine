#include "GLShaderBuffer.h"
#include "GLProgram.h"
#include "../RenderShaderPipeline.h"

GLShaderBuffer::GLShaderBuffer( GLPipelineSet_t *pPipeline, uint32_t nProgramBinding )
    : m_pPipeline( pPipeline )
{
    GLProgram *program;

    m_pBuffer = new GLBuffer( BUFFER_TYPE_UNIFORM, BufferUsage_Stream, szDefaultUniforms[ nProgramBinding ].nSize );
    m_nProgramBinding = nProgramBinding;

    program = pPipeline->pShader;

    m_pBuffer->Bind();
    nglUniformBlockBinding( program->GetProgramID(), nProgramBinding, pPipeline->dataInputBuffers.size() );
    nglBindBufferRange( m_pBuffer->GetGLTarget(), 0, m_pBuffer->GetGLObject(), 0, m_pBuffer->GetSize() );
    nglBindBufferBase( m_pBuffer->GetGLTarget(), 0, m_pBuffer->GetSize() );
    m_pBuffer->Unbind();

    pPipeline->dataInputBuffers.emplace_back( this );
}

GLShaderBuffer::~GLShaderBuffer()
{
    delete m_pBuffer;
}

