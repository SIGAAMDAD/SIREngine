#ifndef __ISHADER_BUFFER_H__
#define __ISHADER_BUFFER_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "IRenderBuffer.h"
#include "IRenderProgram.h"

namespace SIREngine::Rendering
{
	class IShaderBuffer : public IRenderBuffer
	{
	public:
		IShaderBuffer( IRenderProgram *pProgram );
		virtual ~IShaderBuffer() = default;
	protected:
		const IRenderProgram *m_pProgram;
	};

	SIRENGINE_FORCEINLINE IShaderBuffer::IShaderBuffer( IRenderProgram *pProgram )
		: m_pProgram( pProgram )
	{
	}
};

#endif