#ifndef __RENDER_SHADER_H__
#define __RENDER_SHADER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>

namespace SIREngine::RenderLib::Backend {
    class IRenderShader
    {
    public:
        IRenderShader( void )
            : m_nType( ST_INVALID )
        { }
        virtual ~IRenderShader()
        { }

        static IRenderShader *Create( const RenderShaderInit_t& shaderInit );

        virtual RenderShaderType_t GetType( void ) const = 0;
        inline virtual const char *GetName( void ) const
        { return m_szName; }
    protected:
        virtual bool Load( const char *pszFilePath ) = 0;

        char m_szName[ MAX_RESOURCE_PATH ];
        RenderShaderType_t m_nType;
    };
};

#endif