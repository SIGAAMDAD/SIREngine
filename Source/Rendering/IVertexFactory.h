#ifndef __SIRENGINE_IVERTEX_FACTORY_H__
#define __SIRENGINE_IVERTEX_FACTORY_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>

namespace SIREngine::Rendering
{
	class IVertexFactory
	{
	public:
		IVertexFactory();
		virtual ~IVertexFactory() = 0;
	protected:
	};
};

#endif