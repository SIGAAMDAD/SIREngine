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

#ifndef __SIR_RENDER_PROGRAM_H__
#define __SIR_RENDER_PROGRAM_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRResourcePtr.h>
#include <Core/Containers/SIRArray.h>
#include <Core/Containers/SIRString.h>
#include "SIRRenderTypes.h"
#include "SIRRenderResource.h"
#include "SIRRenderShader.h"

class SIRRenderProgram : public SIRRenderResource
{
public:
	SIRRenderProgram( const SIRRenderProgram& ) = delete;
	SIRRenderProgram( SIRRenderProgram&& ) = delete;
	SIRRenderProgram& operator=( const SIRRenderProgram& ) = delete;
	SIRRenderProgram& operator=( SIRRenderProgram&& ) = delete;
	virtual ~SIRRenderProgram() noexcept = default;

	uint32_t getStageMask( void ) const noexcept
	{
		return m_nStageMask;
	}

	const SIRArray<SIRResourcePtr<SIRRenderShader>, static_cast<size_t>( SIRRenderShaderStage::SIR_RENDER_STAGE_COUNT )>& getShaders( void ) const noexcept
	{
		return m_Shaders;
	}

protected:
	explicit SIRRenderProgram( const SIRSharedPtr<SIRRenderDevice>& device, const SIRString& shaderFile ) noexcept
	    : SIRRenderResource( device )
	{
	}

	uint32_t m_nStageMask{};
	SIRArray<SIRResourcePtr<SIRRenderShader>, static_cast<size_t>( SIRRenderShaderStage::SIR_RENDER_STAGE_COUNT )> m_Shaders;
};

#endif // __SIR_RENDER_PROGRAM_H__
