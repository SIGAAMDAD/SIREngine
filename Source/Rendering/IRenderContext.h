#ifndef __IRENDER_CONTEXT_H__
#define __IRENDER_CONTEXT_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>
#include "RenderConfig.h"

namespace SIREngine::Rendering
{
	class IRenderContext
	{
	public:
		IRenderContext( const SRenderConfig& config );
		virtual ~IRenderContext();
	protected:
		SDL_Window *m_pWindow;
	};
};

#endif