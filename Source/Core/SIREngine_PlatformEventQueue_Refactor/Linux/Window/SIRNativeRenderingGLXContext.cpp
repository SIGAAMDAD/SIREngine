#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "SIRNativeRenderingGLXContext.h"
#include "Core/Platform/Shared/Threading/SIRScopedLock.h"
#include "Core/Platform/Shared/Threading/SIRSync.h"
#include "Core/Platform/Shared/Window/SIRNativeRenderingGLContext.h"

static SIRMutex s_ContextLock;
static GLXContext s_hCurrentContext = nullptr;

SIRNativeRenderingGLXContext::SIRNativeRenderingGLXContext( Display *pDisplay, Window hWindow, const SIRGLContext& context ) noexcept
    : SIRNativeRenderingGLContext( context )
{
	const int szAttributes[] = { GLX_X_RENDERABLE,
	                             True,
	                             GLX_DRAWABLE_TYPE,
	                             GLX_WINDOW_BIT,
	                             GLX_RENDER_TYPE,
	                             GLX_RGBA_BIT,

	                             GLX_X_VISUAL_TYPE,
	                             GLX_TRUE_COLOR,

	                             GLX_RED_SIZE,
	                             context.nRedSize,
	                             GLX_GREEN_SIZE,
	                             context.nGreenSize,
	                             GLX_BLUE_SIZE,
	                             context.nBlueSize,
	                             GLX_ALPHA_SIZE,
	                             context.nAlphaSize,

	                             GLX_DEPTH_SIZE,
	                             context.nDepthSize,
	                             GLX_STENCIL_SIZE,
	                             context.nStencilSize,

	                             GLX_DOUBLEBUFFER,
	                             context.bDoubleBuffer,

	                             None };
	int nCount = 0;

	const int nScreen = DefaultScreen( hWindow );
	GLXFBConfig *pConfigurations = glXChooseFBConfig( pDisplay, nScreen, szAttributes, &nCount );
	if ( pConfigurations == nullptr || nCount == 0 ) {
		return;
	}

	XVisualInfo *pVisual = glXGetVisualFromFBConfig( pDisplay, pConfigurations[0] );
	if ( pVisual == nullptr ) {
		return;
	}

	m_hGLXWindow = glXCreateWindow( pDisplay, pConfigurations[0], hWindow, szAttributes );
}

bool SIRNativeRenderingGLXContext::makeCurrent( void ) noexcept
{
	Bool bSuccess = glXMakeContextCurrent( m_pDisplay, m_hGLXWindow, m_hGLXWindow, m_hContext );
	if ( !bSuccess ) {
		return false;
	}

	const SIRScopedLock<SIRMutex> lock( s_ContextLock );
	s_hCurrentContext = m_hContext;

	return bSuccess;
}

bool SIRNativeRenderingGLXContext::releaseCurrent( void ) noexcept
{
	const SIRScopedLock<SIRMutex> lock( s_ContextLock );
	if ( s_hCurrentContext == nullptr ) {
		return false;
	}

	glXMakeContextCurrent( m_pDisplay, m_hGLXWindow, m_hGLXWindow, nullptr );
	s_hCurrentContext = nullptr;

	return true;
}

bool SIRNativeRenderingGLXContext::swapBuffers( void ) noexcept
{
	glXSwapBuffers( m_pDisplay, m_hGLXWindow );
	return true;
}

bool SIRNativeRenderingGLXContext::isCurrent( void ) const noexcept
{
	const SIRScopedLock<SIRMutex> lock( s_ContextLock );
	return m_hContext == s_hCurrentContext;
}
