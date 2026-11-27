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

#include <Core/Strings/SIRString.h>
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

	SIRRenderShaderStageMask getStage( void ) const noexcept
	{
		return m_eStage;
	}

	SIRRenderShaderFormat getFormat( void ) const noexcept
	{
		return m_eFormat;
	}

	const char *getEntryPoint( void ) const noexcept
	{
		return m_EntryPoint.c_str();
	}

	uint64_t getReflection( void ) const noexcept
	{
		return m_nReflectionHash;
	}

	uint64_t getContentHash( void ) const noexcept
	{
		return m_nContentHash;
	}

protected:
	explicit SIRRenderShader( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderShaderCode& description ) noexcept
	    : SIRRenderResource( device ),
	      m_eStage( description.eStage ),
	      m_eFormat( description.eFormat ),
	      m_EntryPoint( description.pszEntryPoint == nullptr ? "" : description.pszEntryPoint ),
	      m_nContentHash( description.nContentHash ),
	      m_nReflectionHash( description.nReflectionHash )
	{
	}

	SIRRenderShaderStageMask m_eStage{};
	SIRRenderShaderFormat m_eFormat{};
	SIRString m_EntryPoint;
	uint64_t m_nContentHash{};
	uint64_t m_nReflectionHash{};
};
#endif // __SIR_RENDER_SHADER_H__
