#ifndef __GL_PROGRAM_H__
#define __GL_PROGRAM_H__

#pragma once

#include "../RenderCommon.h"
#include "GLShader.h"

class GLProgram : public IRenderProgram
{
public:
    GLProgram( const char *pszShaderName );
    virtual ~GLProgram();

    virtual GLShader *GetVertexShader( void ) override;
    virtual GLShader *GetPixelShader( void ) override;

    virtual const GLShader *GetVertexShader( void ) const override;
    virtual const GLShader *GetPixelShader( void ) const override;

    virtual bool Load( void ) override;

    SIRENGINE_FORCEINLINE GLuint GetProgramID( void ) const
    { return m_hProgramID; }
private:
    GLuint m_hProgramID;
};

#endif