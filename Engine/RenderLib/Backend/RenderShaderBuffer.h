#ifndef __RENDER_SHADERBUFFER_H__
#define __RENDER_SHADERBUFFER_H__

#pragma once

#include "../RenderCommon.h"

class IRenderShaderBuffer
{
public:
    IRenderShaderBuffer( void );
    virtual ~IRenderShaderBuffer();
protected:
    IRenderProgram *m_pShader;
    uint32_t m_nProgramBinding;
};

#endif