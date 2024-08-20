#ifndef __GL_SHADER_H__
#define __GL_SHADER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderShader.h"

namespace SIREngine::RenderLib::Backend::OpenGL {
    class GLShader : public Backend::IRenderShader
    {
    public:
        GLShader( const RenderShaderInit_t& shaderInfo );
        virtual ~GLShader() override;

        virtual RenderShaderType_t GetType( void ) const override { return m_nType; }
    private:
        virtual bool Load( const char *pszFilePath ) override { return false; }

        GLuint m_hShaderID;
    };
};

#endif