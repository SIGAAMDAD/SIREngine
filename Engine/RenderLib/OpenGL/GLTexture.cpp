#include "GLTexture.hpp"

GLTexture::GLTexture( void )
{
    nglGenTextures( 1, &m_nTextureID );
}

GLTexture::~GLTexture()
{
    nglDeleteBuffers( 1, &m_nTextureID );
}
