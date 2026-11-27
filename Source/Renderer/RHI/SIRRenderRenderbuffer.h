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

#ifndef __SIR_RENDER_RENDERBUFFER_H__
#define __SIR_RENDER_RENDERBUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>

class SIRRenderRenderbuffer : public SIRRenderResource
{
public:
	explicit SIRRenderRenderbuffer( const SIRRenderRenderbuffer& ) = delete;
	explicit SIRRenderRenderbuffer( SIRRenderRenderbuffer&& ) = delete;
	virtual ~SIRRenderRenderbuffer() noexcept = default;
	SIRRenderRenderbuffer& operator=( const SIRRenderRenderbuffer& ) = delete;
	SIRRenderRenderbuffer& operator=( SIRRenderRenderbuffer&& ) = delete;

	uint32_t getWidth( void ) const noexcept
	{
		return m_nWidth;
	}

	uint32_t getHeight( void ) const noexcept
	{
		return m_nHeight;
	}

	uint32_t getSampleCount( void ) const noexcept
	{
		return m_nSampleCount;
	}

	uint64_t getUsage( void ) const noexcept
	{
		return m_nUsage;
	}

	SIRRenderFormat getFormat( void ) const noexcept
	{
		return m_eFormat;
	}

	SIRRenderMemoryClass getMemoryType( void ) const noexcept
	{
		return m_eMemoryType;
	}

protected:
	explicit SIRRenderRenderbuffer( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderRenderbufferDesc& description ) noexcept
	    : SIRRenderResource( device ),
	      m_nWidth( description.nWidth ),
	      m_nHeight( description.nHeight ),
	      m_nSampleCount( description.nSampleCount ),
	      m_nUsage( description.nUsageMask ),
	      m_eFormat( description.eFormat ),
	      m_eMemoryType( description.eMemory )
	{
	}

	uint32_t m_nWidth{};
	uint32_t m_nHeight{};
	uint32_t m_nSampleCount{};
	uint64_t m_nUsage{};
	SIRRenderFormat m_eFormat{};
	SIRRenderMemoryClass m_eMemoryType{};
};
#endif // __SIR_RENDER_RENDERBUFFER_H__
