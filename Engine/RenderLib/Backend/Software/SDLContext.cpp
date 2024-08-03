#include "SDLContext.h"

SDLContext::SDLContext( const ApplicationInfo_t& appInfo )
    : IRenderContext( appInfo )
{
    m_pSurface = SDL_CreateRGBSurface( 0, appInfo.nWindowWidth, appInfo.nWindowHeight, 32, 0, 0, 0, 0 );
    if ( !m_pSurface ) {

    }

    m_pRenderer = SDL_CreateSoftwareRenderer( m_pSurface );
    if ( !m_pRenderer ) {

    }
}

SDLContext::~SDLContext()
{
    if ( m_pRenderer ) {
        SDL_DestroyRenderer( m_pRenderer );
    }
    if ( m_pSurface ) {
        SDL_DestroyWindowSurface( m_pWindow );
    }
}

void SDLContext::SwapBuffers( void )
{
    SDL_RenderPresent( m_pRenderer );
    SDL_UpdateWindowSurface( m_pWindow );
}