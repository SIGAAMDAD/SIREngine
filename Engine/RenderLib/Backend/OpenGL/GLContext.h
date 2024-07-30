#ifndef __GL_CONTEXT_H__
#define __GL_CONTEXT_H__

#pragma once

#include "../RenderContext.h"

class GLContext : public IRenderContext
{
public:
    GLContext( const ApplicationInfo_t& appInfo );
    virtual ~GLContext() override;

    virtual void SetupShaderPipeline( void ) override;
    virtual void SwapBuffers( void ) override;
    virtual void CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline ) override;

    virtual void *Alloc( size_t nBytes, size_t nAligment = 16 ) override;
    virtual void Free( void *pBuffer ) override;
private:
    virtual void GetGPUExtensionList( void ) override;

    void InitGLProcs( void );
    void CheckExtensionsSupport( void );

    SDL_GLContext m_pGLContext;
};

#endif