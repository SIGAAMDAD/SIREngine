#include "GLContext.h"
#include "GLCommon.h"
#include "GLShaderPipeline.h"

CVar<int> render_UseShaderStorageBufferObjects(
    "r.OpenGL.UseShaderStorageBufferObjects",
    0,
    0,
    "Allow usage of the extension GL_ARB_shader_storage_buffer_object\n"
    "  0: Use a uniform buffer for shader data transmission\n"
    "  1: Use a shader storage buffer for shader data transmission",
    CVG_RENDERER
);

GLContext::GLContext( const ApplicationInfo_t& appInfo )
    : IRenderContext( appInfo )
{
    m_pGLContext = SDL_GL_CreateContext( m_pWindow );
    if ( !m_pGLContext ) {

    }
}

GLContext::~GLContext()
{
    if ( m_pGLContext ) {
        SDL_GL_DeleteContext( m_pGLContext );
    }
}

void GLContext::CheckExtensionsSupport( void )
{
}

void GLContext::SetupShaderPipeline( void )
{
}

void GLContext::SwapBuffers( void )
{
    SDL_GL_SwapWindow( m_pWindow );

    nglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
}

void GLContext::CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline )
{
    GLShaderPipeline *pRenderData = dynamic_cast<GLShaderPipeline *>( pShaderPipeline );
    GLProgram *pShader = pRenderData->GetPipelineData()->pShader;

    CVector<GLShaderBuffer *>& inputBuffers = pRenderData->GetPipelineData()->dataInputBuffers;
    for ( auto& it : inputBuffers ) {
        it->SwapData(  );
    }
}

void *GLContext::Alloc( size_t nBytes, size_t nAligment = 16 )
{
    void *pBuffer;

    return pBuffer;
}

void GLContext::Free( void *pBuffer )
{
}