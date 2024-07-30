#ifndef __VK_PROGRAM_H__
#define __VK_PROGRAM_H__

#pragma once

#include "../RenderProgram.h"
#include "VKCommon.h"

class VKProgram : public IRenderProgram
{
public:
    VKProgram( const RenderProgramInit_t& programInfo );
    virtual ~VKProgram();

    virtual IRenderShader *GetVertexShader( void ) override;
    virtual IRenderShader *GetPixelShader( void ) override;

    virtual const IRenderShader *GetVertexShader( void ) const override;
    virtual const IRenderShader *GetPixelShader( void ) const override;
};

#endif