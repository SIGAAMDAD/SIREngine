#ifndef __IRENDER_PIPELINE_BUILDER_H__
#define __IRENDER_PIPELINE_BUILDER_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "IVertexLayout.h"
#include "IRenderProgram.h"
#include "IShaderBuffer.h"

namespace SIREngine::Rendering
{
	class IRenderPipelineBuilder
	{
	public:
		virtual ~IRenderPipelineBuilder() = default;
		
		virtual void StartPipeline( void ) = 0;
		virtual void BindProgram( IRenderProgram *pProgram ) = 0;
		virtual void BindVertexLayout( IVertexLayout *pLayout ) = 0;
		virtual void BindShaderBuffer( uint32_t nBinding, const IShaderBuffer *pBuffer ) = 0;
		virtual void SetViewportState( const glm::vec2& screen, const glm::vec2& position ) = 0;
		virtual void *FinishPipeline( void ) = 0;
	};
};

#endif