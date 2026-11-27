/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_RENDER_PIPELINE_H__
#define __SIR_RENDER_PIPELINE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVector.h>
#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>
#include "SIRRenderProgram.h"
#include "SIRRenderShader.h"

class SIRRenderBindingLayout;

class SIRRenderPipeline : public SIRRenderResource
{
public:
	explicit SIRRenderPipeline( const SIRRenderPipeline& ) = delete;
	explicit SIRRenderPipeline( SIRRenderPipeline&& ) = delete;
	virtual ~SIRRenderPipeline() noexcept = default;

	SIRRenderPipeline& operator=( const SIRRenderPipeline& ) = delete;
	SIRRenderPipeline& operator=( SIRRenderPipeline&& ) = delete;

	SIRRenderPipelineType getType( void ) const noexcept
	{
		return m_eType;
	}

	const SIRRenderBindingLayout& getBindingLayout( void ) const noexcept
	{
		return *m_pBindingLayout;
	}

	const SIRRenderProgram& getProgram( void ) const noexcept
	{
		return *m_Program;
	}

protected:
	explicit SIRRenderPipeline( const SIRSharedPtr<SIRRenderDevice>& device,
	                            SIRRenderPipelineType eType,
	                            const SIRSharedPtr<SIRRenderBindingLayout>& layout,
	                            const SIRSharedPtr<SIRRenderProgram>& program ) noexcept
	    : SIRRenderResource( device ),
	      m_eType( eType ),
	      m_pBindingLayout( layout ),
	      m_Program( program )
	{
	}

	SIRRenderPipelineType m_eType{};
	SIRSharedPtr<SIRRenderBindingLayout> m_pBindingLayout;
	SIRSharedPtr<SIRRenderProgram> m_Program;
};

#endif // __SIR_RENDER_PIPELINE_H__
