#include "GLProgram.h"

using namespace SIREngine;
using namespace SIREngine::RenderLib::Backend::OpenGL;

CVar<bool32> r_EnableShaderLRU(
    "r.OpenGL.EnableProgramLRU",
    0,
    Cvar_Save,
    "OpenGL program LRU cache.\n"
    "For use only when the driver can only support a fixed amount of active GL programs.\n"
    "  0: disabled\n"
    "  1: when the LRU cache limits are reached, the oldest GL program(s) will be deleted to make room. Expect hitching if requested shader is not in LRU cache.\n",
    CVG_RENDERER
);
CVar<bool32> r_ShaderLRUMaxProgramCount(
    "r.OpenGL.ProgramLRUMaxCount",
    1000,
    Cvar_Save,
    "Limits the maximum number of active OpenGL shaders at one time, Set to 0 to disable the LRU cache.\n",
    CVG_RENDERER
);

GLProgram::GLProgram( const RenderProgramInit_t& programInfo )
{
    m_hProgramID = nglCreateProgram();
}

GLProgram::~GLProgram()
{
    nglDeleteProgram( m_hProgramID );
}