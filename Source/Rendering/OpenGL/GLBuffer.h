#ifndef __GL_BUFFER_H__
#define __GL_BUFFER_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Pch.h"
#include "../IRenderBuffer.h"

namespace SIREngine::Rendering::OpenGL
{	
	class GLBuffer : public IRenderBuffer
	{
	public:
		GLBuffer( EStreamType eType, size_t nSize, const void *pData );
		virtual ~GLBuffer() override;

		void SetData( const void *pData, size_t nSize );
	private:
		size_t m_nSize;
		GLuint m_hId;

		const EStreamType m_eStreamType;
		void *m_pStreamData;
	};
};

#endif