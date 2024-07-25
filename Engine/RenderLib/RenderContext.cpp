#include "RenderContext.hpp"

IRenderContext::IRenderContext( const ApplicationInfo_t& appInfo )
    : m_AppInfo( appInfo )
{
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
    case WF_MODE_BORDERLESS_FULLSCREEN:
        windowFlags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
        break;
    case WF_MODE_BORDERLESS_WINDOWED:
        windowFlags |= SDL_WINDOW_BORDERLESS;
        break;
    };

    m_pWindow = SDL_CreateWindow(
        m_AppInfo.pszWindowName,
        m_AppInfo.nWindowPosX, m_AppInfo.nWindowPosY,
        m_AppInfo.nWindowWidth, m_AppInfo.nWindowHeight,
        windowFlags );
    if ( !m_pWindow ) {

    }
}

IRenderContext::~IRenderContext() {
    if ( m_pWindow != NULL ) {
        SDL_DestroyWindow( m_pWindow );
    }
}