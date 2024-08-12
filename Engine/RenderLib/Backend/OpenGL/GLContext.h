#ifndef __GL_CONTEXT_H__
#define __GL_CONTEXT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "GLTexture.h"
#include "../RenderContext.h"

namespace SIREngine::RenderLib::Backend::OpenGL {
    class GLContext : public Backend::IRenderContext
    {
    public:
        GLContext( const Application::ApplicationInfo_t& appInfo );
        virtual ~GLContext() override;

        virtual void Init( void ) override;
        virtual void Shutdown( void ) override;

        virtual void SetupShaderPipeline( void ) override;
        virtual void SwapBuffers( void ) override;
        virtual void BeginFrame( void ) override;
        virtual void CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline ) override;

        virtual void *Alloc( size_t nBytes, size_t nAligment = 16 ) override;
        virtual void Free( void *pBuffer ) override;

        virtual const GPUMemoryUsage_t GetMemoryUsage( void ) override;
        virtual void PrintMemoryInfo( void ) const override;

        virtual Backend::IRenderProgram *AllocateProgram( const RenderProgramInit_t& programInfo ) override;
        virtual Backend::IRenderShader *AllocateShader( const RenderShaderInit_t& shaderInit ) override;
        virtual Backend::IRenderBuffer *AllocateBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize ) override;
        virtual Backend::IRenderTexture *AllocateTexture( const TextureInit_t& textureInfo ) override;
    private:
        virtual void GetGPUExtensionList( void ) override;

        void InitGLProcs( void );
        void CheckExtensionsSupport( void );

        char m_szGLSLVersion[1024];
        char m_szDriverVersion[1024];
        char m_szRendererString[1024];
        char m_szVendorString[1024];

        SDL_GLContext m_pGLContext;

        static CVector<GLTexture *> g_EvictionLRUCache;
    };

    extern CVar<bool32> r_UsePixelBufferObjects;
    extern CVar<bool32> r_UseMappedBufferObjects;
};

#endif