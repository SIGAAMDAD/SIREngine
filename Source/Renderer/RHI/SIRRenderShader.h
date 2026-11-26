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

#ifndef __SIR_RENDER_SHADER_H__
#define __SIR_RENDER_SHADER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>

class SIRRenderShader : public SIRRenderResource
{
public:
	explicit SIRRenderShader( const SIRRenderShader& ) = delete;
	explicit SIRRenderShader( SIRRenderShader&& ) = delete;
	virtual ~SIRRenderShader() noexcept = default;

	SIRRenderShader& operator=( const SIRRenderShader& ) = delete;
	SIRRenderShader& operator=( SIRRenderShader&& ) = delete;

	uint32_t getStage( void ) const noexcept
	{
		return m_nStageMask;
	}

	SIRRenderShaderFormat getFormat( void ) const noexcept
	{
		return m_eFormat;
	}

	const char *getEntryPoint( void ) const noexcept
	{
		return m_Description.pszEntryPoint;
	}

	uint64_t getReflection( void ) const noexcept
	{
		return m_Description.nReflectionHash;
	}

	const sirRenderShaderDescription_t& getDescription( void ) const noexcept
	{
		return m_Description;
	}

protected:
	explicit SIRRenderShader( const SIRSharedPtr<SIRRenderDevice>& device, const sirRenderShaderDescription_t& description, SIRRenderShaderFormat eFormat ) noexcept
	    : SIRRenderResource( device ),
	      m_Description( description ),
	      m_nStageMask( description.nStageMask ),
	      m_eFormat( eFormat )
	{
	}

	sirRenderShaderDescription_t m_Description{};
	uint32_t m_nStageMask{};
	SIRRenderShaderFormat m_eFormat{};
};
#endif // __SIR_RENDER_SHADER_H__
