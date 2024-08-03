#ifndef __SDL_CONTEXT_H__
#define __SDL_CONTEXT_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "SDLCommon.h"
#include "../RenderContext.h"

class SDLContext : public IRenderContext
{
public:
    SDLContext( const ApplicationInfo_t& appInfo );
    virtual ~SDLContext() override;

    virtual void SetupShaderPipeline( void ) override;
    virtual void SwapBuffers( void ) override;
    virtual void BeginFrame( void );
    virtual void CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline ) override;

    virtual void *Alloc( size_t nBytes, size_t nAligment = 16 ) override;
    virtual void Free( void *pBuffer ) override;
private:
    SDL_Renderer *m_pRenderer;
    SDL_Surface *m_pSurface;
};

#endif