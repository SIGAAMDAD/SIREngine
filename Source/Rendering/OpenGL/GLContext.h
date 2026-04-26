#ifndef __GL_CONTEXT_H__
#define __GL_CONTEXT_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "../RenderConfig.h"
#include "../IRenderContext.h"

namespace SIREngine::Rendering::OpenGL
{
	class GLContext : public IRenderContext
	{
	public:
		GLContext( const SRenderConfig& config );
		virtual ~GLContext() override;
	private:
		SDL_GLContext m_hContext;
	};
};

#endif