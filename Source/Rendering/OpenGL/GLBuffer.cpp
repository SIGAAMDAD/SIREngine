#include "GLBuffer.h"
#include "GLProcs.h"

using namespace SIREngine::Rendering::OpenGL;

GLBuffer::GLBuffer( EStreamType eType, size_t nSize, const void *pData )
	: m_nSize( nSize ), m_eStreamType( eType )
{
	nglCreateBuffers( 1, &m_hId );

	GLbitfield accessMode = 0;
	switch ( eType )
	{
		case EStreamType::FlushAndForget:
			accessMode = GL_MAP_WRITE_BIT;
			break;
		case EStreamType::Persistent:
			accessMode = GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT;
			break;
	}

	nglNamedBufferStorage( m_hId, nSize, pData, accessMode );
}

GLBuffer::~GLBuffer()
{
	nglDeleteBuffers( 1, &m_hId );
}

void GLBuffer::SetData( const void *pData, size_t nSize )
{
}