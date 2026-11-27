#include "SIRGLUtilities.h"
#include "Renderer/RHI/SIRRenderTypes.h"
#include <GL/gl.h>
#include <GL/glext.h>

GLenum SIRGLUtilities::toGLFilter( SIRRenderFilter eFilter, SIRRenderMipFilter eMipFilter ) noexcept
{
	if ( eFilter == SIRRenderFilter::Linear ) {
		if ( eMipFilter == SIRRenderMipFilter::Linear ) {
			return GL_LINEAR_MIPMAP_LINEAR;
		}
		if ( eMipFilter == SIRRenderMipFilter::Nearest ) {
			return GL_LINEAR_MIPMAP_NEAREST;
		}
		return GL_LINEAR;
	}
	if ( eFilter == SIRRenderFilter::Nearest ) {
		if ( eMipFilter == SIRRenderMipFilter::Linear ) {
			return GL_NEAREST_MIPMAP_LINEAR;
		}
		if ( eMipFilter == SIRRenderMipFilter::Nearest ) {
			return GL_NEAREST_MIPMAP_NEAREST;
		}
		return GL_NEAREST;
	}
}
