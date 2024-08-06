#include "../RenderBuffer.h"
#include "../RenderShaderPipeline.h"
#include "../RenderContext.h"
#include "GLContext.h"
#include "GLCommon.h"
#include "GLBuffer.h"
#include "GLShader.h"
#include "GLProgram.h"
#include "GLTexture.h"
#include "GLVertexArray.h"
#include "GLShaderPipeline.h"

CVector<GLTexture *> GLContext::g_EvictionLRUCache;

CVar<bool32> r_UseShaderStorageBufferObjects(
    "r.OpenGL.UseShaderStorageBufferObjects",
    0,
    Cvar_Default,
    "Allow usage of the extension GL_ARB_shader_storage_buffer_object\n"
    "  0: Use a uniform buffer for shader data transmission\n"
    "  1: Use a shader storage buffer for shader data transmission",
    CVG_RENDERER
);
CVar<bool32> r_UseMappedBufferObjects(
    "r.OpenGL.UseMappedBufferObjects",
    0,
    Cvar_Save,
    "Use glMapBuffer instead of glBufferSubData, GL_ARB_map_buffer_range must be available on this device.",
    CVG_RENDERER
);
CVar<uint32_t> r_GLTetureMinLRUSize(
    "r.OpenGL.TextureMinLRUSize",
    528,
    Cvar_Save | Cvar_Developer,
    "Sets the maximum number of textures that can be in GPU memory\n"
    "before the engine begins purging memory.\n",
    CVG_RENDERER
);

extern CVar<bool32> r_IgnoreShaderCompilerFailure;
extern CVar<bool32> r_IgnoreShaderLinkFailure;
extern CVar<bool32> r_EnableShaderLRU;

/* TODO: make this a little bit more relevant
uint32_t g_nMaxSubDataSize = 2*1024*1024;
CVarRef<uint32_t> r_MaxSubBufferSize(
    "r.OpenGL.MaxSubBufferSize",
    g_nMaxSubDataSize,
    Cvar_Developer,
    "Sets the maximum amount of data the can be sent per glBufferSubData call.",
    CVG_RENDERER
);
*/

bool32 g_bUseBufferDiscard = true;
CVarRef<bool32> r_UseBufferDiscard(
    "r.OpenGL.UseBufferDiscard",
    g_bUseBufferDiscard,
    Cvar_Developer,
    "Toggles buffer orphaning for faster OpenGL synching.",
    CVG_RENDERER
);

extern CVar<bool32> r_UsePixelBufferObjects;

GLContext::GLContext( const ApplicationInfo_t& appInfo )
    : IRenderContext( appInfo )
{
    g_ConsoleManager.RegisterCVar( &r_UseBufferDiscard );
    g_ConsoleManager.RegisterCVar( &r_UseShaderStorageBufferObjects );
    g_ConsoleManager.RegisterCVar( &r_GLTetureMinLRUSize );
    g_ConsoleManager.RegisterCVar( &r_UseMappedBufferObjects );
    g_ConsoleManager.RegisterCVar( &r_UsePixelBufferObjects );
    g_ConsoleManager.RegisterCVar( &r_TextureStreamingBudget );
    g_ConsoleManager.RegisterCVar( &r_UseHDRTextures );
}

GLContext::~GLContext()
{
}

void GLContext::Init( void )
{
    m_pGLContext = SDL_GL_CreateContext( m_pWindow );
    if ( !m_pGLContext ) {
        SIRENGINE_ERROR( "SDL_GL_CreateContext failed: %s", SDL_GetError() );
    }
    SIRENGINE_LOG( "Created SDL2 OpenGL Context" );

    SDL_GL_MakeCurrent( m_pWindow, m_pGLContext );
    
    InitGLProcs();
    SIRENGINE_LOG( "Loaded GL Procs" );

    SIREngine_strncpyz( m_szGLSLVersion, (const char *)nglGetString( GL_SHADING_LANGUAGE_VERSION ),
        sizeof( m_szGLSLVersion ) );
    SIREngine_strncpyz( m_szDriverVersion, (const char *)nglGetString( GL_VERSION ),
        sizeof( m_szDriverVersion ) );
    SIREngine_strncpyz( m_szVendorString, (const char *)nglGetString( GL_VENDOR ),
        sizeof( m_szVendorString ) );
    SIREngine_strncpyz( m_szRendererString, (const char *)nglGetString( GL_RENDERER ),
        sizeof( m_szRendererString ) );
    
    SIRENGINE_LOG( "OpenGL Driver Query:" );
    SIRENGINE_LOG( " Driver Version: %s", m_szDriverVersion );
    SIRENGINE_LOG( " Shader Language Version (GLSL): %s", m_szGLSLVersion );
    SIRENGINE_LOG( " Vendor: %s", m_szVendorString );
    SIRENGINE_LOG( " Renderer: %s", m_szRendererString );

    GetGPUExtensionList();

    CheckExtensionsSupport();
}

void GLContext::Shutdown( void )
{
    if ( m_pGLContext ) {
        SDL_GL_DeleteContext( m_pGLContext );
    }
}

void GLContext::CheckExtensionsSupport( void )
{
    auto GPUHasExtension = [&]( const char *pExtensionName ) -> bool {
        return eastl::find( m_GPUExtensionList.cbegin(), m_GPUExtensionList.cend(),
            pExtensionName ) != m_GPUExtensionList.cend();
    };

    if ( GPUHasExtension( "GL_ARB_pixel_buffer_object" ) ) {
        r_UsePixelBufferObjects.SetValue( true );
        SIRENGINE_LOG( "Using GL_ARB_pixel_buffer_object." );
    } else {
        SIRENGINE_WARNING( "GL_ARB_pixel_buffer_object not found, texture streaming objects not available." );
    }
}

void GLContext::SetupShaderPipeline( void )
{
}

void GLContext::SwapBuffers( void )
{
    SDL_GL_SwapWindow( m_pWindow );

    nglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    for ( uint32_t nEvictionCount = 0; g_EvictionLRUCache.size() > r_GLTetureMinLRUSize.GetValue(); nEvictionCount++ ) {
        GLTexture *pTexture = g_EvictionLRUCache.back();
        pTexture->EvictGLResource();
        g_EvictionLRUCache.pop_back();
    }
}

void GLContext::CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline )
{
    GLShaderPipeline *pRenderData = dynamic_cast<GLShaderPipeline *>( pShaderPipeline );
    GLProgram *pShader = pRenderData->GetPipelineData()->pShader;

    CVector<GLShaderBuffer *>& inputBuffers = pRenderData->GetPipelineData()->dataInputBuffers;
    for ( auto& it : inputBuffers ) {
//        it->SwapData(  );
    }

    pRenderData->GetVertexArray()->Bind();

    nglDrawElements( GL_TRIANGLES, pRenderData->GetIndexCount(), GL_UNSIGNED_INT, NULL );

    pRenderData->GetVertexArray()->Unbind();
}

void GLContext::GetGPUExtensionList( void )
{
    char szExtensions[80000];
    GLint nExtensionCount, i;
    const char *pSrc;
    char *pDest;
    char szExtensionName[128];

    SIREngine_strncpyz( szExtensions, (const char *)nglGetString( GL_EXTENSIONS ), sizeof( szExtensions ) );
    nglGetIntegerv( GL_NUM_EXTENSIONS, &nExtensionCount );

    m_GPUExtensionList.reserve( nExtensionCount );
    pSrc = szExtensions;
    for ( i = 0; i < nExtensionCount; i++ ) {
        pDest = szExtensionName;
        while ( *pSrc != ' ' ) {
            *pDest++ = *pSrc++;
        }
        pSrc++;
        *pDest++ = '\0';
        m_GPUExtensionList.emplace_back( szExtensionName );

        SIRENGINE_LOG( "Found OpenGL Extension \"%s\"", szExtensionName );
    }
}

void *GLContext::Alloc( size_t nBytes, size_t nAligment )
{
    void *pBuffer;
    return malloc( nBytes );
}

void GLContext::Free( void *pBuffer )
{
    free( pBuffer );
}

const GPUMemoryUsage_t GLContext::GetMemoryUsage( void )
{
}

void GLContext::PrintMemoryInfo( void ) const
{
}


IRenderProgram *GLContext::AllocateProgram( const RenderProgramInit_t& programInfo )
{
    return new ( m_pResourceAllocator->Allocate( sizeof( GLProgram ) ) ) GLProgram( programInfo );
}

IRenderShader *GLContext::AllocateShader( const RenderShaderInit_t& shaderInit )
{
    return new ( m_pResourceAllocator->Allocate( sizeof( GLShader ) ) ) GLShader( shaderInit );
}

IRenderBuffer *GLContext::AllocateBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize )
{
    return new ( m_pResourceAllocator->Allocate( sizeof( GLBuffer ) ) ) GLBuffer( nType, nUsage, nSize );
}

IRenderTexture *GLContext::AllocateTexture( const TextureInit_t& textureInfo )
{
    return new ( m_pResourceAllocator->Allocate( sizeof( GLTexture ) ) ) GLTexture( textureInfo );
}
