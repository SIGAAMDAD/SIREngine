#include "IRenderContext.h"

using namespace SIREngine::Rendering;

static Uint32 GetWindowFlags( const SRenderConfig& config )
{
	Uint32 flags = 0;

	if ( config.eApi == ERenderAPI::Vulkan ) {
		flags |= SDL_WINDOW_VULKAN;
	}
	else if ( config.eApi == ERenderAPI::OpenGL ) {
		flags |= SDL_WINDOW_OPENGL;
	}
}

IRenderContext::IRenderContext( const SRenderConfig& config )
{
	if ( !SDL_InitSubSystem( SDL_INIT_VIDEO ) ) {
	}

	m_pWindow = SDL_CreateWindow( config.DisplayConfig.pszWindowName, config.DisplayConfig.nWidth, config.DisplayConfig.nHeight, GetWindowFlags( config ) );
}

IRenderContext::~IRenderContext()
{
	if ( m_pWindow != NULL ) {
		SDL_DestroyWindow( m_pWindow );
	}
	SDL_QuitSubSystem( SDL_INIT_VIDEO );
}