#ifndef __IRENDER_SYSTEM_H__
#define __IRENDER_SYSTEM_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "RenderConfig.h"

namespace SIREngine::Rendering
{
	class RenderSystem
	{
	public:
		static void Init( const SRenderConfig& config );
		static void Update( void );
		static void Shutdown( void );
		static void Restart( void );
	};
};

#endif