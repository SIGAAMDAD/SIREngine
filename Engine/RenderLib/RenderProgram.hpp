#ifndef __RENDER_PROGRAM_HPP__
#define __RENDER_PROGRAM_HPP__

#pragma once

#include <Engine/RenderLib/RenderCommon.hpp>
#include <Engine/RenderLib/RenderShader.hpp>
#include <EASTL/vector.h>

class IRenderProgram
{
public:
    IRenderProgram( const char *pszShaderName );
    virtual ~IRenderProgram();

    virtual IRenderShader *GetVertexShader( void );
    virtual IRenderShader *GetPixelShader( void );
    virtual IRenderShader *GetGemetryShader( void );

    virtual const IRenderShader *GetVertexShader( void ) const;
    virtual const IRenderShader *GetPixelShader( void ) const;
    virtual const IRenderShader *GetGemetryShader( void ) const;

    virtual bool Load( void ) = 0;
protected:
    IRenderShader *m_pVertexShader;
    IRenderShader *m_pFragmentShader;
    IRenderShader *m_pGeometryShader;
};

#endif