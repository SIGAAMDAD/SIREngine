#include "GLProgram.h"

GLProgram::GLProgram( const RenderProgramInit_t& programInfo )
{
    m_hProgramID = nglCreateProgram();
}

GLProgram::~GLProgram()
{
    nglDeleteProgram( m_hProgramID );
}