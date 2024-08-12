#include "GLShaderPipeline.h"
#include "GLBuffer.h"

using namespace SIREngine::RenderLib::Backend::OpenGL;
using namespace SIREngine::RenderLib;

static const GLenum attribTypeGL[ NUMATTRIBTYPES ] = {
    GL_INT,
    GL_FLOAT,

    GL_INT64_ARB,
    GL_DOUBLE,

    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,

    GL_SHORT,
    GL_UNSIGNED_SHORT,
};

static const GLint attribTypeCountGL[ NUMATTRIBTYPES ] = {
    1,
    1,

    1,
    1,

    2,
    3,
    4,

    4,
    4,
};

GLShaderPipeline::GLShaderPipeline( void )
{
    uint64_t i;

    RenderProgramInit_t programInfo;
    programInfo.pName = "DefaultShader";
    m_pDefaultShader = dynamic_cast<GLProgram *>( IRenderProgram::Create( programInfo ) );

    RenderPipelineInputSet_t input;
    input.pVertexAttribs = szDefaultVertexAttribs;
    input.nAttribCount = SIREngine_StaticArrayLength( szDefaultVertexAttribs );
    input.pShader = m_pDefaultShader;
    m_pPipelineArray = new GLVertexArray( input );

    //
    // initialize GL state vertex pointers
    // during runtime, we'll turn specific attributes
    // on and off, but we'll never change the actual
    // layout of the shader input
    //
    m_pPipelineArray->GetVertexBuffer()->Bind();
    for ( i = 0; i < SIREngine_StaticArrayLength( szDefaultVertexAttribs ); i++ ) {
        nglEnableVertexArrayAttrib( m_pPipelineArray->GetGLObject(), szDefaultVertexAttribs[i].nShaderBinding );
        nglVertexAttribPointer(
            szDefaultVertexAttribs[i].nShaderBinding,
            attribTypeCountGL[ szDefaultVertexAttribs[i].nType ],
            attribTypeGL[ szDefaultVertexAttribs[i].nType ],
            szDefaultVertexAttribs[i].bNormalized,
            szDefaultVertexAttribs[i].nStride,
            (const GLvoid *)szDefaultVertexAttribs[i].nOffset
        );
    }
    m_pPipelineArray->GetVertexBuffer()->Unbind();
}

GLShaderPipeline::~GLShaderPipeline()
{
    delete m_pDefaultShader;
    delete m_pPipelineArray;
}

uint64_t GLShaderPipeline::AddVertexAttribSet( const VertexInputDescription_t& vertexInput )
{
    uint64_t nCacheID;
    GLPipelineSet_t *pSet;

    m_PipelineCache.emplace_back();
    pSet = &m_PipelineCache.back();

    pSet->pShader = dynamic_cast<GLProgram *>( vertexInput.pShader );
    pSet->nEnabledAttribs = vertexInput.nEnabledVertexAttributes;

    nCacheID = m_PipelineCache.size();

    return nCacheID;
}

void GLShaderPipeline::SetShaderInputState( uint64_t nCacheID )
{
    uint64_t i;
    const GLPipelineSet_t *pSet;

    if ( nCacheID == INVALID_PIPELINE_CACHE_ID ) {
        // reset to default
        ResetPipelineState();
        return;
    }

    m_nUsedPipeline = nCacheID;
    pSet = &m_PipelineCache[ nCacheID ];

    UpdateVertexPointers( pSet );
}

void GLShaderPipeline::ResetPipelineState( void )
{
    uint64_t i;

    for ( i = 0; i < NumAttribs; i++ ) {
        nglEnableVertexArrayAttrib( m_pPipelineArray->GetGLObject(), szDefaultVertexAttribs[i].nShaderBinding );
    }
}

void GLShaderPipeline::UpdateVertexPointers( const GLPipelineSet_t *pSet )
{
    uint64_t i;
    uint32_t attribBit;

    if ( m_nEnabledAttribs == pSet->nEnabledAttribs ) {
        return; // already set
    }

    for ( i = 0; i < NumAttribs; i++ ) {
        const bool32 bEnabled = pSet->nEnabledAttribs & SIRENGINE_BIT( i );

        if ( bEnabled ) {
            if ( !( m_nEnabledAttribs & pSet->nEnabledAttribs ) ) {
                nglEnableVertexArrayAttrib( m_pPipelineArray->GetGLObject(),
                    szDefaultVertexAttribs[i].nShaderBinding );
            }
        } else {
            if ( ( m_nEnabledAttribs & pSet->nEnabledAttribs ) ) {
                nglDisableVertexArrayAttrib( m_pPipelineArray->GetGLObject(),
                    szDefaultVertexAttribs[i].nShaderBinding );
            }
        }
    }
}