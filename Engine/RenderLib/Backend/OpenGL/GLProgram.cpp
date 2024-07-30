#include "GLProgram.h"

GLProgram::GLProgram( const char *pszShaderName )
{
    m_hProgramID = nglCreateProgram();
}

GLProgram::~GLProgram()
{
    nglDeleteProgram( m_hProgramID );
}