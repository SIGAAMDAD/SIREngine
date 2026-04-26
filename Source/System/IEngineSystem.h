#ifndef __SIRENGINE_IENGINE_SYSTEM_H__
#define __SIRENGINE_IENGINE_SYSTEM_H__

#include <Core/Compiler.h>	

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

namespace SIREngine::System
{
	class IEngineSystem
	{
	public:
		virtual ~IEngineSystem() = default;

		virtual void Init( void ) = 0;
		virtual void Shutdown( void ) = 0;
		virtual void Update( void ) = 0;
	};
};

#endif