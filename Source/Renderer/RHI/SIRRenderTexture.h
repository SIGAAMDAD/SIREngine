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

#ifndef __SIR_RENDER_TEXTURE_H__
#define __SIR_RENDER_TEXTURE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>

class SIRRenderTexture : public SIRRenderResource
{
public:
	explicit SIRRenderTexture( const SIRRenderTexture& ) = delete;
	explicit SIRRenderTexture( SIRRenderTexture&& ) = delete;
	virtual ~SIRRenderTexture() noexcept = default;
	SIRRenderTexture& operator=( const SIRRenderTexture& ) = delete;
	SIRRenderTexture& operator=( SIRRenderTexture&& ) = delete;

	uint32_t getWidth( void ) const noexcept
	{
		return m_Description.nWidth;
	}

	uint32_t getHeight( void ) const noexcept
	{
		return m_Description.nHeight;
	}

	uint32_t getDepth( void ) const noexcept
	{
		return m_Description.nDepth;
	}

	uint32_t getMipLevels( void ) const noexcept
	{
		return m_Description.nMipLevels;
	}

	uint32_t getArrayLayers( void ) const noexcept
	{
		return m_Description.nLayers;
	}

	uint32_t getSampleCount( void ) const noexcept
	{
		return m_Description.nSamples;
	}

	uint32_t getUsage( void ) const noexcept
	{
		return static_cast<uint32_t>( m_Description.nUsage );
	}

	SIRRenderFormat getFormat( void ) const noexcept
	{
		return m_Description.eFormat;
	}

	SIRRenderTextureDimension getDimension( void ) const noexcept
	{
		return m_Description.eDimension;
	}

	SIRRenderMemoryType getMemoryType( void ) const noexcept
	{
		return m_Description.eMemory;
	}

	bool isExternallyOwned( void ) const noexcept
	{
		return m_Description.bExternallyOwned;
	}

	bool isSparse( void ) const noexcept
	{
		return m_Description.bSparse;
	}

	const sirRenderTextureDescription_t& getDescription( void ) const noexcept
	{
		return m_Description;
	}

	virtual SIRRenderResult getSubresourceLayout( const sirRenderSubresourceRange_t& subresources, sirRenderBufferRange_t *pLayout ) const noexcept = 0;

protected:
	explicit SIRRenderTexture( const SIRSharedPtr<SIRRenderDevice>& device, const sirRenderTextureDescription_t& description ) noexcept
	    : SIRRenderResource( device ),
	      m_Description( description )
	{
	}

	sirRenderTextureDescription_t m_Description{};
};
#endif // __SIR_RENDER_TEXTURE_H__
