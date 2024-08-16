#include "GLTexture.h"
#include "GLContext.h"

using namespace SIREngine;
using namespace SIREngine::RenderLib::Backend;
using namespace SIREngine::RenderLib::Backend::OpenGL;

extern uint64_t SIREngine::Application::g_nFrameNumber;

CVar<bool32> SIREngine::RenderLib::Backend::OpenGL::r_UsePixelBufferObjects(
	"r.OpenGL.UsePixelBufferObjects",
	0,
	Cvar_Save,
	"Enables usage of OpenGL pixel buffers for texture streaming\n"
	"instead of traditional texture usage (fire-and-forget)\n",
	CVG_RENDERER
);

static GLenum GetImageGPUFormat( uint32_t nChannels )
{
	GLenum format;

	if ( nChannels == 3 ) {
		if ( r_UseHDRTextures.GetValue() ) {
			format = GL_RGB16F;
		} else {
			format = GL_RGB8;
		}
	} else {
		if ( r_UseHDRTextures.GetValue() ) {
			format = GL_RGBA16F;
		} else {
			format = GL_RGBA8;
		}
	}
	return format;
}

GLTexture::GLTexture( const TextureInit_t& textureInfo )
{
	nglGenTextures( 1, &m_nTextureID );
	if ( r_UsePixelBufferObjects.GetValue() ) {
		nglCreateBuffers( SIREngine_StaticArrayLength( m_hBufferID ), m_hBufferID );
	}

	Upload( textureInfo );
}

GLTexture::~GLTexture()
{
	if ( r_UsePixelBufferObjects.GetValue() ) {
		nglDeleteBuffers( SIREngine_StaticArrayLength( m_hBufferID ), m_hBufferID );
	}
	nglDeleteTextures( 1, &m_nTextureID );
}


// source: http://www.songho.ca/opengl/gl_pbo.html
void GLTexture::StreamBuffer( void )
{
	void *pMappedBuffer;
	GLint nextIndex;

	m_nBufferIndex = ( m_nBufferIndex + 1 ) % 2;
	nextIndex = ( m_nBufferIndex + 1 ) % 2;

	nglBindTexture( GL_TEXTURE_2D, m_nTextureID );

	nglBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_hBufferID[ m_nBufferIndex ] );
	// copy pixels from PBO to texture object
	// use offset instead of pointer
	nglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, m_nWidth, m_nHeight, m_nGLTextureFormat, GL_UNSIGNED_BYTE, NULL );
	nglBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );

	nglBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_hBufferID[ nextIndex ] );
	
	// orphan the buffer to avoid a sync issue
	if ( r_UseMappedBufferObjects.GetValue() ) {
		nglInvalidateBufferData( m_hBufferID[ nextIndex ] );
		pMappedBuffer = nglMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY | GL_MAP_PERSISTENT_BIT
			| GL_MAP_COHERENT_BIT );
		if ( pMappedBuffer ) {
			memcpy( pMappedBuffer, m_ImageData.GetBuffer(), m_ImageData.GetSize() );    
			nglUnmapBuffer( GL_PIXEL_UNPACK_BUFFER );
		}
	} else {
		nglBufferData( GL_PIXEL_UNPACK_BUFFER, m_ImageData.GetSize(), NULL, GL_STREAM_DRAW );
		nglBufferSubData( GL_PIXEL_UNPACK_BUFFER, 0, m_ImageData.GetSize(), m_ImageData.GetBuffer() );
	}
	nglBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );

	m_nFrameLastRendered = Application::g_nFrameNumber;
}

void GLTexture::Upload( const TextureInit_t& textureInfo )
{
	GL_CALL( nglBindTexture( GL_TEXTURE_2D, m_nTextureID ) );
	if ( textureInfo.bIsGPUOnly ) {
	} else {
		m_ImageData.Load( textureInfo.filePath );
		
		if ( r_UsePixelBufferObjects.GetValue() ) {
			for ( auto& it : m_hBufferID ) {
				nglBindBuffer( GL_PIXEL_UNPACK_BUFFER, it );
				if ( r_UseMappedBufferObjects.GetValue() ) {
					nglBufferStorage( GL_PIXEL_UNPACK_BUFFER, m_ImageData.GetSize(), m_ImageData.GetBuffer(),
						GL_WRITE_ONLY | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT );
				} else {
					nglBufferData( GL_PIXEL_UNPACK_BUFFER, m_ImageData.GetSize(), NULL, GL_STREAM_DRAW );
				}
				nglBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
			}
		}
		GL_CALL( nglTexImage2D( GL_TEXTURE_2D, 0, GetImageGPUFormat( m_ImageData.GetChannels() ),
			m_ImageData.GetWidth(), m_ImageData.GetHeight(), 0, GL_RGBA,
			GL_UNSIGNED_BYTE, m_ImageData.GetBuffer() ) );
		SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Created OpenGL GPU Texture: (size) %ux%u, 0x%lx",
			m_ImageData.GetWidth(), m_ImageData.GetHeight(),
			(uintptr_t)m_ImageData.GetBuffer() );
		
		GL_CALL( nglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
		GL_CALL( nglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
		GL_CALL( nglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT ) );
		GL_CALL( nglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT ) );
		GL_CALL( nglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 ) );
	}
	GL_CALL( nglBindTexture( GL_TEXTURE_2D, 0 ) );
}

void GLTexture::EvictGLResource( void )
{
	if ( m_bCanCreateAsEvicted && m_bInGPUMemory ) {
		if ( CanBeEvicted() ) {
			nglDeleteTextures( 1, &m_nTextureID );

			m_bInGPUMemory = false;
			nglGenTextures( 1, &m_nTextureID );
		}
	}
}

bool GLTexture::CanBeEvicted( void ) const
{
	return m_bCanCreateAsEvicted;
}