#ifndef __SIRENGINE_RENDERAPP_INTERFACE_H__
#define __SIRENGINE_RENDERAPP_INTERFACE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#define RENDERLIB_API_VERSION 1

#include <Engine/Core/EngineApp.h>
#include <Engine/RenderLib/RenderCommon.h>

namespace SIREngine::RenderLib {
    typedef enum {
        AntiAlias_None,

        AntiAlias_2xMSAA,
        AntiAlias_4xMSAA,
        AntiAlias_8xMSAA,
        AntiAlias_16xMSAA,
        AntiAlias_32xMSAA,
        AntiAlias_2xSSAA,
        AntiAlias_4xSSAA,
        AntiAlias_SMAA,
        AntiAlias_FXAA,

        NumAntiAliasTypes
    } AntiAliasingType_t;

    class CRenderer : public IEngineApp
    {
    public:
        CRenderer( void );
        virtual ~CRenderer();

        virtual const char *GetName( void ) const override;
        virtual bool IsActive( void ) const override;
        virtual uint32_t GetState( void ) const override;

        virtual void Init( void ) override;
        virtual void Shutdown( void ) override;
        virtual void Frame( int64_t msec ) override;

        void BeginFrame( void );
        void EndFrame( void );
        void FinalizeTextures( void );

        virtual void SaveGame( void ) override;
        virtual void LoadGame( void ) override;
    private:
        void PostProcess( void );
    };

    extern CRenderer *g_pRenderLib;
};

#endif