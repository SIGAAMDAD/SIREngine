#ifndef __RENDER_SHADER_H__
#define __RENDER_SHADER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>

class IRenderShader : public CResourceDef
{
public:
    IRenderShader( void )
        : m_nType( ST_INVALID )
    { }
    virtual ~IRenderShader()
    { }

    static IRenderShader *Create( const RenderShaderInit_t& shaderInit );

    virtual RenderShaderType_t GetType( void ) const = 0;
    virtual const char *GetName( void ) const = 0;
protected:
    virtual bool Load( const char *pszFilePath ) = 0;

    RenderShaderType_t m_nType;
};

#endif