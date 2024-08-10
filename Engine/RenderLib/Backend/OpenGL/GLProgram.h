#ifndef __GL_PROGRAM_H__
#define __GL_PROGRAM_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderProgram.h"

class GLProgram : public IRenderProgram
{
public:
    GLProgram( const RenderProgramInit_t& programInfo );
    virtual ~GLProgram() override;

    virtual bool Load( void ) override { return false; }

    SIRENGINE_FORCEINLINE GLuint GetProgramID( void ) const
    { return m_hProgramID; }
private:
    GLuint m_hProgramID;
};

#endif