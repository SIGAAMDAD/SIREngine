#ifndef __IVERTEX_BUFFER_H__
#define __IVERTEX_BUFFER_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>

namespace SIREngine::Rendering
{
	enum class EMapAccess
	{
		/// <summary>
		/// The buffer's memory is shared between multiple resources.
		/// </summary>
		Shared = SIRENGINE_BIT( 0 ),

		/// <summary>
		/// Won't be unmapped until the buffer is released or resized.
		/// </summary>
		Persistent = SIRENGINE_BIT( 1 ),

		/// <summary>
		/// The buffer can be resized.
		/// </summary>
		Dynamic = SIRENGINE_BIT( 2 ),

		/// <summary>
		/// The buffer is read-only.
		/// </summary>
		ReadOnly = SIRENGINE_BIT( 3 ),

		/// <summary>
		/// The buffer is write-only.
		/// </summary>
		WriteOnly = SIRENGINE_BIT( 4 ),

		Count
	};

	class IVertexBuffer
	{
	public:
		IVertexBuffer( void ) = default;
		virtual ~IVertexBuffer() = default;

		virtual void *Map( EMapAccess access ) = 0;
		virtual void Unmap( void ) = 0;
		
		virtual size_t GetSize( void ) const = 0;
	protected:
	};
};

#endif