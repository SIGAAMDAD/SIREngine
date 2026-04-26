#ifndef __IRENDER_BUFFER_H__
#define __IRENDER_BUFFER_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "RenderTypes.h"

namespace SIREngine::Rendering
{
	enum class EStreamType : uint32_t
	{
		/// @brief We give the GPU the buffer once and never change it ever again.
		FlushAndForget,

		/// @brief The buffer is persistently mapped for maximum performance.
		Persistent,

		Count
	};

	enum class EBufferType : uint32_t
	{
		/// @brief 
		Vertex,

		/// @brief 
		Index,

		/// @brief 
		ShaderStorage,

		Count
	};

	class IRenderBuffer
	{
	public:
		virtual ~IRenderBuffer() = default;

		virtual void Update( size_t nOffset, size_t nSize, const void *pData ) = 0;

		virtual void Bind( void ) = 0;
		virtual void Unbind( void ) = 0;

		virtual void *Map( EBufferMapAccess eAccess ) = 0;
		virtual void Unmap( void ) = 0;

		virtual size_t GetSize( void ) const = 0;
	protected:
		void *m_pData;
		size_t m_nSize;

		EStreamType m_eUsage;
		
		EBufferType m_eType;
	};
};

#endif