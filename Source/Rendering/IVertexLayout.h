#ifndef __SIRENGINE_IVERTEX_LAYOUT_H__
#define __SIRENGINE_IVERTEX_LAYOUT_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>
#include "Types.h"

namespace SIREngine::Rendering
{
	class IVertexLayout
	{
	public:
		virtual ~IVertexLayout() = default;

		virtual void AddAttribute( const SVertexAttribute& attrib ) = 0;
		virtual void AddBinding( const SVertexBinding& binding ) = 0;

		virtual void LinkAttributeToBinding( uint32_t nAttributeLocation, uint32_t nBindingIndex ) = 0;

		virtual void Build( void ) = 0;

		virtual size_t GetHash( void ) const = 0;
	};
};

#endif