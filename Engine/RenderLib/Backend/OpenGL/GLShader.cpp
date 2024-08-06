#include "GLShader.h"

CVar<bool32> r_IgnoreShaderLinkFailure(
    "r.OpenGL.IgnoreShaderLinkFailure",
    0,
    Cvar_Default,
    "Ignore OpenGL program link failures.\n"
    "  0: Program link failure generates a fatal error when encountered. (default)\n"
    "  1: Ignore link failures. this may allow a program to continue but could lead to undefined rendering behaviour.",
    CVG_RENDERER
);
CVar<bool32> r_IgnoreShaderCompilerFailure(
    "r.OpenGL.IgnoreShaderCompilerFailure",
    0,
    Cvar_Default,
    "Ignore OpenGL shader compile failures.\n"
    "  0: Shader compile failure return an error when encountered. (default)\n"
    "  1: Ignore Shader compile failures.",
    CVG_RENDERER
);

GLShader::GLShader( const RenderShaderInit_t& shaderInfo )
{
    GLenum shaderType;

    switch ( shaderInfo.nType ) {
    case ST_VERTEX:
        shaderType = GL_VERTEX_SHADER;
        break;
    case ST_FRAGMENT:
        shaderType = GL_FRAGMENT_SHADER;
        break;
    case ST_GEOMETRY:
        shaderType = GL_GEOMETRY_SHADER;
    };

    m_hShaderID = nglCreateShader( shaderType );
}

GLShader::~GLShader()
{
    nglDeleteShader( m_hShaderID );
}

