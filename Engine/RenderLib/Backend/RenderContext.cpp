#include "Vulkan/VKCommon.h"
#include "OpenGL/GLCommon.h"
#include "RenderBuffer.h"
#include "RenderFramebuffer.h"
#include "RenderShader.h"
#include "RenderProgram.h"
#include "RenderShaderBuffer.h"
#include "RenderShaderPipeline.h"
#include "RenderContext.h"
#include "RenderTexture.h"
#include "OpenGL/GLContext.h"
#include "Vulkan/VKContext.h"
#include "OpenGL/GLBuffer.h"
#include "OpenGL/GLFramebuffer.h"
#include "OpenGL/GLVertexArray.h"
#include "OpenGL/GLProgram.h"
#include "OpenGL/GLShader.h"
#include "OpenGL/GLShaderBuffer.h"
#include "OpenGL/GLShaderPipeline.h"
#include "OpenGL/GLTexture.h"
#include "Vulkan/VKBuffer.h"
#include "Vulkan/VKFramebuffer.h"
#include "Vulkan/VKProgram.h"
#include "Vulkan/VKShader.h"
#include "Vulkan/VKShaderBuffer.h"
#include "Vulkan/VKShaderPipeline.h"
#include "Vulkan/VKTexture.h"

using namespace SIREngine;
using namespace SIREngine::RenderLib::Backend::OpenGL;
using namespace SIREngine::RenderLib::Backend::Vulkan;
using namespace SIREngine::RenderLib::Backend;
using namespace SIREngine::Application;

namespace SIREngine::RenderLib::Backend {

SIRENGINE_DEFINE_LOG_CATEGORY( RenderBackend, ELogLevel::Info );

IRenderContext *g_pRenderContext;

CVar<int32_t> r_TextureStreamingBudget(
    "r.TextureStreamingBudget",
    256,
    Cvar_Save,
    "Sets the engine's GPU texture buffer streaming limit. Set in MiB",
    CVG_RENDERER
);
CVar<bool32> r_UseHDRTextures(
    "r.UseHDRTextures",
    0,
    Cvar_Save,
    "",
    CVG_RENDERER
);
CVar<uint32_t> r_WindowWidth(
    "r.WindowWidth",
    1920,
    Cvar_Save,
    "Sets the engine's window width.",
    CVG_RENDERER
);
CVar<uint32_t> r_WindowHeight(
    "r.WindowHeight",
    1080,
    Cvar_Save,
    "Sets the engine's window height.",
    CVG_RENDERER
);

static const char *GetContextType( unsigned flags )
{
    if ( flags & SDL_WINDOW_OPENGL ) {
        return "OpenGL";
    } else if ( flags & SDL_WINDOW_VULKAN ) {
        return "Vulkan";
    }
    return "Software";
}

IRenderContext::IRenderContext( const ApplicationInfo_t& appInfo )
    : m_AppInfo( appInfo )
{
#if !defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
    unsigned windowFlags;

    windowFlags = 0;

    switch ( m_AppInfo.eWindowFlags & WF_CONTEXT_BITS ) {
    case WF_OPENGL_CONTEXT:
        windowFlags |= SDL_WINDOW_OPENGL;
        break;
    case WF_VULKAN_CONTEXT:
        windowFlags |= SDL_WINDOW_VULKAN;
        break;
    case WF_D3D11_CONTEXT:
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#else
#endif
        break;
    case WF_SOFTWARE_CONTEXT:
        break;
    };

    switch ( m_AppInfo.eWindowFlags & WF_MODE_BITS ) {
    case WF_MODE_FULLSCREEN:
        windowFlags |= SDL_WINDOW_FULLSCREEN;
        break;
    case WF_MODE_WINDOWED:
        break;
    };

    if ( m_AppInfo.eWindowFlags & WF_MODE_BORDERLESS ) {
        windowFlags |= SDL_WINDOW_BORDERLESS;
    }

    SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Creating SDL2 window instance:" );
    SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " IsFullscreen: %s", windowFlags & SDL_WINDOW_FULLSCREEN ? "TRUE" : "FALSE" );
    SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " IsBorderless: %s", windowFlags & SDL_WINDOW_BORDERLESS ? "TRUE" : "FALSE" );
    SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " Width: %u", m_AppInfo.nWindowWidth );
    SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " Height: %u", m_AppInfo.nWindowHeight );
    SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " ContextType: %s", GetContextType( windowFlags ) );

#if defined(SIRENGINE_BUILD_EDITOR)
    windowFlags |= SDL_WINDOW_SHOWN;
#endif

    m_pWindow = SDL_CreateWindow(
        m_AppInfo.pszWindowName,
        m_AppInfo.nWindowPosX, m_AppInfo.nWindowPosY,
        m_AppInfo.nWindowWidth, m_AppInfo.nWindowHeight,
        windowFlags );
    if ( !m_pWindow ) {
        SIRENGINE_ERROR( "SDL_CreateWindow failed: %s", SDL_GetError() );
    }
#else
#endif
}

IRenderContext::~IRenderContext() {
#if !defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
    if ( m_pWindow != NULL ) {
        SDL_DestroyWindow( m_pWindow );
    }
#endif
}

extern CVar<bool32> e_Fullscreen;
extern CVar<uint32_t> e_RenderAPI;

IRenderContext *IRenderContext::CreateRenderContext( void )
{
    ApplicationInfo_t *pAppInfo;

    const CVector<CString>& cmdLine = Application::Get()->GetCommandLine();
    pAppInfo = eastl::addressof( Application::Get()->GetAppInfo() );

    r_TextureStreamingBudget.Register();
    r_UseHDRTextures.Register();
    r_WindowWidth.Register();
    r_WindowHeight.Register();

    pAppInfo->nWindowWidth = r_WindowWidth.GetValue();
    pAppInfo->nWindowHeight = r_WindowHeight.GetValue();
    pAppInfo->nWindowPosX = 0;
    pAppInfo->nWindowPosY = 0;

    const CString renderAPI = Application::Get()->GetCommandParmValue( "-renderer=" );

    if ( Application::Get()->CheckCommandParm( "-nofullscreen" ) || e_Fullscreen.GetValue() ) {
        pAppInfo->eWindowFlags &= ~WF_MODE_FULLSCREEN;
        pAppInfo->eWindowFlags |= WF_MODE_WINDOWED;
    }

    if ( e_RenderAPI.GetValue() == RAPI_OPENGL || renderAPI == "opengl" ) {
        pAppInfo->eWindowFlags |= WF_OPENGL_CONTEXT;
        g_pRenderContext = new OpenGL::GLContext( *pAppInfo );
    }
    else if ( e_RenderAPI.GetValue() == RAPI_VULKAN || renderAPI == "vulkan" ) {
        pAppInfo->eWindowFlags |= WF_VULKAN_CONTEXT;
        g_pRenderContext = new Vulkan::VKContext( *pAppInfo );
        g_pVKContext = dynamic_cast<VKContext *>( g_pRenderContext );
    }
    else if ( e_RenderAPI.GetValue() == RAPI_D3D11 || renderAPI == "directx11" ) {
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#else
#endif
    }
    else {
        pAppInfo->eWindowFlags |= WF_OPENGL_CONTEXT;
        g_pRenderContext = new OpenGL::GLContext( *pAppInfo );
    }
    g_pRenderContext->Init();
    g_pRenderContext->SetupShaderPipeline();

    return g_pRenderContext;
}

IRenderBuffer *IRenderBuffer::Create( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize )
{
    return g_pRenderContext->AllocateBuffer( nType, nUsage, nSize );
}

IRenderShader *IRenderShader::Create( const RenderShaderInit_t& shaderInit )
{
    return g_pRenderContext->AllocateShader( shaderInit );
}

IRenderProgram *IRenderProgram::Create( const RenderProgramInit_t& programInfo )
{
    return g_pRenderContext->AllocateProgram( programInfo );
}

IRenderTexture *IRenderTexture::Create( const TextureInit_t& textureInfo )
{
    return g_pRenderContext->AllocateTexture( textureInfo );
}

};