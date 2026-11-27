#include "SIRGLFramebuffer.h"
#include "Renderer/Drivers/OpenGL/SIRGLUtilities.h"
#include "Renderer/RHI/SIRRenderTypes.h"
#include "SIRGLDevice.h"

SIRGLFramebuffer::SIRGLFramebuffer( const SIRSharedPtr<SIRRenderDevice>& device ) noexcept
    : SIRRenderResource( device )
{
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();

	if ( glDevice.hasExtension( "GL_ARB_direct_state_access" ) ) {
		glDevice.createFramebuffers( 1, &m_hFramebufferID );
	} else if ( glDevice.hasExtension( "GL_EXT_framebuffer_object" ) ) {
		glDevice.genFramebuffersEXT( 1, &m_hFramebufferID );
	}
}

SIRGLFramebuffer::~SIRGLFramebuffer() noexcept
{
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();

	if ( glDevice.hasExtension( "GL_EXT_framebuffer_object" ) ) {
		glDevice.deleteFramebuffersEXT( 1, &m_hFramebufferID );
	}
}

void SIRGLFramebuffer::clear( void ) noexcept
{
	if ( !isValid() ) {
		return;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();

	if ( glDevice.hasExtension( "GL_ARB_direct_state_access" ) ) {
		const GLfloat szColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

		for ( uint32_t i = 0; i < m_ColorAttachments.getSize(); i++ ) {
			glDevice.clearNamedFramebufferfv( m_hFramebufferID, GL_COLOR, static_cast<GLint>( i ), szColor );
		}
	}
}

void SIRGLFramebuffer::blit(
    SIRGLFramebuffer& targetFbo, const SIRRenderViewport& srcExtents, const SIRRenderViewport& dstExtents, SIRRenderFilter eFilter, SIRRenderTextureAspect eMask ) noexcept
{
	if ( !targetFbo.isValid() ) {
		return;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();

	if ( glDevice.hasExtension( "GL_ARB_direct_state_access" ) ) {
		glDevice.blitNamedFramebuffer( m_hFramebufferID,
		                               targetFbo.m_hFramebufferID,
		                               static_cast<GLint>( srcExtents.fX ),
		                               static_cast<GLint>( srcExtents.fY ),
		                               static_cast<GLint>( srcExtents.fX + srcExtents.fWidth ),
		                               static_cast<GLint>( srcExtents.fY + srcExtents.fHeight ),

		                               static_cast<GLint>( dstExtents.fX ),
		                               static_cast<GLint>( dstExtents.fY ),
		                               static_cast<GLint>( dstExtents.fX + dstExtents.fWidth ),
		                               static_cast<GLint>( dstExtents.fY + dstExtents.fHeight ),

		                               SIRGLUtilities::toGLMask( eMask ),
		                               SIRGLUtilities::toGLFilter( eFilter ) );
	} else if ( glDevice.hasExtension( "GL_EXT_framebuffer_object" ) ) {
		glDevice.bindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, m_hFramebufferID );
		glDevice.bindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, targetFbo.m_hFramebufferID );
		glDevice.blitFramebufferEXT( static_cast<GLint>( srcExtents.fX ),
		                             static_cast<GLint>( srcExtents.fY ),
		                             static_cast<GLint>( srcExtents.fX + srcExtents.fWidth ),
		                             static_cast<GLint>( srcExtents.fY + srcExtents.fHeight ),

		                             static_cast<GLint>( dstExtents.fX ),
		                             static_cast<GLint>( dstExtents.fY ),
		                             static_cast<GLint>( dstExtents.fX + dstExtents.fWidth ),
		                             static_cast<GLint>( dstExtents.fY + dstExtents.fHeight ),

		                             SIRGLUtilities::toGLMask( eMask ),
		                             SIRGLUtilities::toGLFilter( eFilter ) );
	}
}
