#include "Pch.h"
#include "GLContext.h"

using namespace SIREngine::Rendering::OpenGL;

GLContext::GLContext( const SRenderConfig& config )
	: IRenderContext( config )
{
	m_hContext = SDL_GL_CreateContext( m_pWindow );
	if ( m_hContext == NULL ) {

	}

	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, SDL_TRUE );
}

GLContext::~GLContext()
{
	if ( m_hContext != NULL ) {
		SDL_GL_DestroyContext( m_hContext );
	}
}