#ifndef __RENDER_SHADERBUFFER_H__
#define __RENDER_SHADERBUFFER_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>

class IRenderShaderBuffer
{
public:
    IRenderShaderBuffer( void )
        : m_pShader( NULL ), m_nProgramBinding( 0 )
    { }
    virtual ~IRenderShaderBuffer()
    { }
protected:
    IRenderProgram *m_pShader;
    uint32_t m_nProgramBinding;
};

#endif