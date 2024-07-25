#ifndef __RENDER_SHADER_HPP__
#define __RENDER_SHADER_HPP__

#pragma once

#include <Engine/RenderLib/RenderCommon.hpp>

typedef enum {
    ST_VERTEX,
    ST_FRAGMENT,
    ST_GEOMETRY,
    ST_TESSELATION,

    ST_PIXEL = ST_FRAGMENT
} renderShaderType_t;

class IRenderShader : public CResourceDef
{
public:
    IRenderShader( const char *pszShaderName );
    virtual ~IRenderShader();

    virtual renderShaderType_t GetType( void ) const = 0;
    virtual const char *GetName( void ) const = 0;
protected:
    virtual bool Load( const char *pszFilePath ) = 0;

    renderShaderType_t m_nType;
};

#endif