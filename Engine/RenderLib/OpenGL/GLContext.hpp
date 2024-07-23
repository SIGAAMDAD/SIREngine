#ifndef __GL_CONTEXT_HPP__
#define __GL_CONTEXT_HPP__

#pragma once

#include <Engine/RenderLib/RenderContext.hpp>

class GLContext : public IRenderContext
{
public:
    GLContext( void );
    virtual ~GLContext() override;
private:
    void InitGLProcs( void );
};

#endif